#include "client.h"
#include "mainwindow.h"

typedef QPair<quint8,QVariant> CustomPackage;
Q_DECLARE_METATYPE(CustomPackage)
static int id1 = qRegisterMetaType<CustomPackage>();
static int id2 = qRegisterMetaTypeStreamOperators<CustomPackage>();

Client::Client(QObject *parent) :
    QObject(parent), connection(0), isMaster(false), status(CSunconnected)
{
    connect(&server, SIGNAL(newConnection(Connection*)), this, SLOT(incomingConnection(Connection*)));
}

Client::~Client()
{
    removeConnection();
}

int Client::getServerPort()
{
    return server.serverPort();
}

Connection *Client::setupConnection(Connection *connection)
{
    if(!connection)
        connection = new Connection(this);
    connect(connection, SIGNAL(readyRead()), connection, SLOT(receiveData()));
    connect(connection, SIGNAL(receivedData(Connection::PackageType,QVariant)), this, SLOT(handleData(Connection::PackageType,QVariant)));
    connect(connection, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(removeConnection()));
    connect(connection, SIGNAL(disconnected()), this, SLOT(removeConnection()));
    connect(connection, SIGNAL(logMessage(QString,Qt::GlobalColor)), this, SIGNAL(logMessage(QString,Qt::GlobalColor)));

    return connection;
}

void Client::incomingConnection(Connection *incomingConnection)
{
    if(!connection) {
        connection = setupConnection(incomingConnection);
        emit logMessage(QString("Waiting for Handshake from %1:%2").arg(connection->peerAddress().toString()).arg(connection->peerPort()), Qt::yellow);
        status = CSwait4Handshake;
    } else {
        emit logMessage(QString("dismiss incoming connection from %1:%2").
                        arg(incomingConnection->peerAddress().toString()).
                        arg(incomingConnection->peerPort()));
        incomingConnection->deleteLater();
    }
}

void Client::handleData(Connection::PackageType type, QVariant data)
{
    switch(status) {
    case CSunconnected:
        emit logMessage("Received unexpected incoming data", Qt::red);
        break;
    case CSinitiatedConnection: // answer of initiated handshake
    {
        QString text = QString("client confirmed role to be %1.").arg(((data.toBool()) ? "master" : "slave"));
        logMessage(text, Qt::yellow);

        status = CSready;

        emit establishedConnection(isMaster);
    }
        break;
    case CSwait4Handshake: // reveiving now the awaited handshake
    {
        isMaster = !data.toBool();
        QString text = QString("receiving role to be %1.").arg(((isMaster) ? "master" : "slave"));
        logMessage(text, Qt::yellow);

        connection->sendData(Connection::PTroleDefinition, isMaster);
        status = CSready;

        emit establishedConnection(isMaster);
    }
        break;
    default:
    {
        if(type == Connection::PTcustomData) {
            CustomPackage package(data.value<CustomPackage>());
            emit receiveData(package.first, package.second);
        }
    }
    }
}

void Client::initiateConnection(QString host, int port, bool isMaster)
{
    if(!connection) {
        this->isMaster = isMaster;
        connection = setupConnection();
        connection->connectToHost(host, port);
        status = CSinitiatedConnection;
        connect(connection, SIGNAL(connected()), this, SLOT(startHandShake()));
    }
}


void Client::startHandShake()
{
    connection->sendData(Connection::PTroleDefinition, isMaster);
}

void Client::removeConnection()
{
    if(connection) {
        emit logMessage(QString("Removing Connection... (%1)").arg(connection->errorString()), Qt::yellow);
        connection->deleteLater();
    }
    connection = 0;
    status = CSunconnected;
    emit closedConnection();
}

void Client::sendData(quint8 type, QVariant data)
{
    if(connection && status == CSready) {
        CustomPackage package(type, data);
        connection->sendData(Connection::PTcustomData, QVariant::fromValue(package));
    } else {
        emit logMessage("Not ready for sending data.");
    }
}
