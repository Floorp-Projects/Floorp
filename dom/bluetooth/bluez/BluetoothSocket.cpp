/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothSocket.h"
#include <fcntl.h>
#include "BluetoothSocketObserver.h"
#include "BluetoothUnixSocketConnector.h"
#include "mozilla/RefPtr.h"
#include "nsISupportsImpl.h" // for MOZ_COUNT_CTOR, MOZ_COUNT_DTOR
#include "nsXULAppAPI.h"

using namespace mozilla::ipc;

BEGIN_BLUETOOTH_NAMESPACE

static const size_t MAX_READ_SIZE = 1 << 16;

//
// BluetoothSocketIO
//

class BluetoothSocket::BluetoothSocketIO final
  : public UnixSocketWatcher
  , public DataSocketIO
{
public:
  BluetoothSocketIO(MessageLoop* aConsumerLoop,
                    MessageLoop* aIOLoop,
                    BluetoothSocket* aConsumer,
                    UnixSocketConnector* aConnector);
  ~BluetoothSocketIO();

  void GetSocketAddr(nsAString& aAddrStr) const;

  BluetoothSocket* GetBluetoothSocket();
  DataSocket* GetDataSocket();

  // Delayed-task handling
  //

  void SetDelayedConnectTask(CancelableTask* aTask);
  void ClearDelayedConnectTask();
  void CancelDelayedConnectTask();

  // Task callback methods
  //

  /**
   * Run bind/listen to prepare for further runs of accept()
   */
  void Listen();

  /**
   * Connect to a socket
   */
  void Connect();

  void Send(UnixSocketIOBuffer* aBuffer);

  // I/O callback methods
  //

  void OnConnected() override;
  void OnError(const char* aFunction, int aErrno) override;
  void OnListening() override;
  void OnSocketCanAcceptWithoutBlocking() override;
  void OnSocketCanReceiveWithoutBlocking() override;
  void OnSocketCanSendWithoutBlocking() override;

  // Methods for |DataSocket|
  //

  nsresult QueryReceiveBuffer(UnixSocketIOBuffer** aBuffer);
  void ConsumeBuffer();
  void DiscardBuffer();

  // Methods for |SocketIOBase|
  //

  SocketBase* GetSocketBase() override;

  bool IsShutdownOnConsumerThread() const override;
  bool IsShutdownOnIOThread() const override;

  void ShutdownOnConsumerThread() override;
  void ShutdownOnIOThread() override;

private:
  class ReceiveTask;

  void FireSocketError();

  /**
   * Consumer pointer. Non-thread-safe pointer, so should only be manipulated
   * directly from consumer thread. All non-consumer-thread accesses should
   * happen with mIO as container.
   */
  BluetoothSocket* mConsumer;

  /**
   * Connector object used to create the connection we are currently using.
   */
  nsAutoPtr<UnixSocketConnector> mConnector;

  /**
   * If true, do not requeue whatever task we're running
   */
  bool mShuttingDownOnIOThread;

  /**
   * Number of valid bytes in |mAddress|
   */
  socklen_t mAddressLength;

  /**
   * Address structure of the socket currently in use
   */
  struct sockaddr_storage mAddress;

  /**
   * Task member for delayed connect task. Should only be access on consumer
   * thread.
   */
  CancelableTask* mDelayedConnectTask;

  /**
   * I/O buffer for received data
   */
  nsAutoPtr<UnixSocketRawData> mBuffer;
};

BluetoothSocket::BluetoothSocketIO::BluetoothSocketIO(
  MessageLoop* aConsumerLoop,
  MessageLoop* aIOLoop,
  BluetoothSocket* aConsumer,
  UnixSocketConnector* aConnector)
  : UnixSocketWatcher(aIOLoop)
  , DataSocketIO(aConsumerLoop)
  , mConsumer(aConsumer)
  , mConnector(aConnector)
  , mShuttingDownOnIOThread(false)
  , mAddressLength(0)
  , mDelayedConnectTask(nullptr)
{
  MOZ_ASSERT(mConsumer);
  MOZ_ASSERT(mConnector);

  MOZ_COUNT_CTOR_INHERITED(BluetoothSocketIO, DataSocketIO);
}

BluetoothSocket::BluetoothSocketIO::~BluetoothSocketIO()
{
  MOZ_ASSERT(IsConsumerThread());
  MOZ_ASSERT(IsShutdownOnConsumerThread());

  MOZ_COUNT_DTOR_INHERITED(BluetoothSocketIO, DataSocketIO);
}

void
BluetoothSocket::BluetoothSocketIO::GetSocketAddr(nsAString& aAddrStr) const
{
  if (!mConnector) {
    NS_WARNING("No connector to get socket address from!");
    aAddrStr.Truncate();
    return;
  }

  nsCString addressString;
  nsresult rv = mConnector->ConvertAddressToString(
    *reinterpret_cast<const struct sockaddr*>(&mAddress), mAddressLength,
    addressString);
  if (NS_FAILED(rv)) {
    aAddrStr.Truncate();
    return;
  }

  aAddrStr.Assign(NS_ConvertUTF8toUTF16(addressString));
}

BluetoothSocket*
BluetoothSocket::BluetoothSocketIO::GetBluetoothSocket()
{
  return mConsumer;
}

DataSocket*
BluetoothSocket::BluetoothSocketIO::GetDataSocket()
{
  return GetBluetoothSocket();
}

void
BluetoothSocket::BluetoothSocketIO::SetDelayedConnectTask(CancelableTask* aTask)
{
  MOZ_ASSERT(IsConsumerThread());

  mDelayedConnectTask = aTask;
}

void
BluetoothSocket::BluetoothSocketIO::ClearDelayedConnectTask()
{
  MOZ_ASSERT(IsConsumerThread());

  mDelayedConnectTask = nullptr;
}

void
BluetoothSocket::BluetoothSocketIO::CancelDelayedConnectTask()
{
  MOZ_ASSERT(IsConsumerThread());

  if (!mDelayedConnectTask) {
    return;
  }

  mDelayedConnectTask->Cancel();
  ClearDelayedConnectTask();
}

void
BluetoothSocket::BluetoothSocketIO::Listen()
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());
  MOZ_ASSERT(mConnector);

  if (!IsOpen()) {
    mAddressLength = sizeof(mAddress);

    int fd;
    nsresult rv = mConnector->CreateListenSocket(
      reinterpret_cast<struct sockaddr*>(&mAddress), &mAddressLength, fd);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      FireSocketError();
      return;
    }
    SetFd(fd);

    // calls OnListening on success, or OnError otherwise
    rv = UnixSocketWatcher::Listen(
      reinterpret_cast<struct sockaddr*>(&mAddress), mAddressLength);
    NS_WARN_IF(NS_FAILED(rv));
  }
}

void
BluetoothSocket::BluetoothSocketIO::Connect()
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());
  MOZ_ASSERT(mConnector);

  if (!IsOpen()) {
    mAddressLength = sizeof(mAddress);

    int fd;
    nsresult rv = mConnector->CreateStreamSocket(
      reinterpret_cast<struct sockaddr*>(&mAddress), &mAddressLength, fd);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      FireSocketError();
      return;
    }
    SetFd(fd);
  }

  // calls OnConnected() on success, or OnError() otherwise
  nsresult rv = UnixSocketWatcher::Connect(
    reinterpret_cast<struct sockaddr*>(&mAddress), mAddressLength);
  NS_WARN_IF(NS_FAILED(rv));
}

void
BluetoothSocket::BluetoothSocketIO::Send(UnixSocketIOBuffer* aBuffer)
{
  EnqueueData(aBuffer);
  AddWatchers(WRITE_WATCHER, false);
}

void
BluetoothSocket::BluetoothSocketIO::OnConnected()
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());
  MOZ_ASSERT(GetConnectionStatus() == SOCKET_IS_CONNECTED);

  GetConsumerThread()->PostTask(
    FROM_HERE, new SocketEventTask(this, SocketEventTask::CONNECT_SUCCESS));

  AddWatchers(READ_WATCHER, true);
  if (HasPendingData()) {
    AddWatchers(WRITE_WATCHER, false);
  }
}

void
BluetoothSocket::BluetoothSocketIO::OnListening()
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());
  MOZ_ASSERT(GetConnectionStatus() == SOCKET_IS_LISTENING);

  AddWatchers(READ_WATCHER, true);
}

void
BluetoothSocket::BluetoothSocketIO::OnError(const char* aFunction, int aErrno)
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());

  UnixFdWatcher::OnError(aFunction, aErrno);
  FireSocketError();
}

void
BluetoothSocket::BluetoothSocketIO::OnSocketCanAcceptWithoutBlocking()
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());
  MOZ_ASSERT(GetConnectionStatus() == SOCKET_IS_LISTENING);

  RemoveWatchers(READ_WATCHER|WRITE_WATCHER);

  mAddressLength = sizeof(mAddress);

  int fd;
  nsresult rv = mConnector->AcceptStreamSocket(
    GetFd(),
    reinterpret_cast<struct sockaddr*>(&mAddress), &mAddressLength, fd);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    FireSocketError();
    return;
  }

  Close();
  SetSocket(fd, SOCKET_IS_CONNECTED);

  GetConsumerThread()->PostTask(
    FROM_HERE, new SocketEventTask(this, SocketEventTask::CONNECT_SUCCESS));

  AddWatchers(READ_WATCHER, true);
  if (HasPendingData()) {
    AddWatchers(WRITE_WATCHER, false);
  }
}

void
BluetoothSocket::BluetoothSocketIO::OnSocketCanReceiveWithoutBlocking()
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());
  MOZ_ASSERT(GetConnectionStatus() == SOCKET_IS_CONNECTED); // see bug 990984

  ssize_t res = ReceiveData(GetFd());
  if (res < 0) {
    /* I/O error */
    RemoveWatchers(READ_WATCHER|WRITE_WATCHER);
  } else if (!res) {
    /* EOF or peer shutdown */
    RemoveWatchers(READ_WATCHER);
  }
}

void
BluetoothSocket::BluetoothSocketIO::OnSocketCanSendWithoutBlocking()
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());
  MOZ_ASSERT(GetConnectionStatus() == SOCKET_IS_CONNECTED); // see bug 990984

  nsresult rv = SendPendingData(GetFd());
  if (NS_FAILED(rv)) {
    return;
  }

  if (HasPendingData()) {
    AddWatchers(WRITE_WATCHER, false);
  }
}

void
BluetoothSocket::BluetoothSocketIO::FireSocketError()
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());

  // Clean up watchers, statuses, fds
  Close();

  // Tell the consumer thread we've errored
  GetConsumerThread()->PostTask(
    FROM_HERE, new SocketEventTask(this, SocketEventTask::CONNECT_ERROR));
}

// |DataSocketIO|

nsresult
BluetoothSocket::BluetoothSocketIO::QueryReceiveBuffer(
  UnixSocketIOBuffer** aBuffer)
{
  MOZ_ASSERT(aBuffer);

  if (!mBuffer) {
    mBuffer = new UnixSocketRawData(MAX_READ_SIZE);
  }
  *aBuffer = mBuffer.get();

  return NS_OK;
}

/**
 * |ReceiveTask| transfers data received on the I/O thread
 * to an instance of |BluetoothSocket| on the consumer thread.
 */
class BluetoothSocket::BluetoothSocketIO::ReceiveTask final
  : public SocketTask<BluetoothSocketIO>
{
public:
  ReceiveTask(BluetoothSocketIO* aIO, UnixSocketBuffer* aBuffer)
    : SocketTask<BluetoothSocketIO>(aIO)
    , mBuffer(aBuffer)
  { }

  void Run() override
  {
    BluetoothSocketIO* io = SocketTask<BluetoothSocketIO>::GetIO();

    MOZ_ASSERT(io->IsConsumerThread());

    if (NS_WARN_IF(io->IsShutdownOnConsumerThread())) {
      // Since we've already explicitly closed and the close
      // happened before this, this isn't really an error.
      return;
    }

    BluetoothSocket* bluetoothSocket = io->GetBluetoothSocket();
    MOZ_ASSERT(bluetoothSocket);

    bluetoothSocket->ReceiveSocketData(mBuffer);
  }

private:
  nsAutoPtr<UnixSocketBuffer> mBuffer;
};

void
BluetoothSocket::BluetoothSocketIO::ConsumeBuffer()
{
  GetConsumerThread()->PostTask(FROM_HERE,
                                new ReceiveTask(this, mBuffer.forget()));
}

void
BluetoothSocket::BluetoothSocketIO::DiscardBuffer()
{
  // Nothing to do.
}

// |SocketIOBase|

SocketBase*
BluetoothSocket::BluetoothSocketIO::GetSocketBase()
{
  return GetDataSocket();
}

bool
BluetoothSocket::BluetoothSocketIO::IsShutdownOnConsumerThread() const
{
  MOZ_ASSERT(IsConsumerThread());

  return mConsumer == nullptr;
}

void
BluetoothSocket::BluetoothSocketIO::ShutdownOnConsumerThread()
{
  MOZ_ASSERT(IsConsumerThread());
  MOZ_ASSERT(!IsShutdownOnConsumerThread());

  mConsumer = nullptr;
}

bool
BluetoothSocket::BluetoothSocketIO::IsShutdownOnIOThread() const
{
  return mShuttingDownOnIOThread;
}

void
BluetoothSocket::BluetoothSocketIO::ShutdownOnIOThread()
{
  MOZ_ASSERT(!IsConsumerThread());
  MOZ_ASSERT(!mShuttingDownOnIOThread);

  Close(); // will also remove fd from I/O loop
  mShuttingDownOnIOThread = true;
}


//
// Socket tasks
//

class BluetoothSocket::ListenTask final
  : public SocketIOTask<BluetoothSocketIO>
{
public:
  ListenTask(BluetoothSocketIO* aIO)
    : SocketIOTask<BluetoothSocketIO>(aIO)
  { }

  void Run() override
  {
    MOZ_ASSERT(!GetIO()->IsConsumerThread());

    if (!IsCanceled()) {
      GetIO()->Listen();
    }
  }
};

class BluetoothSocket::ConnectTask final
  : public SocketIOTask<BluetoothSocketIO>
{
public:
  ConnectTask(BluetoothSocketIO* aIO)
    : SocketIOTask<BluetoothSocketIO>(aIO)
  { }

  void Run() override
  {
    MOZ_ASSERT(!GetIO()->IsConsumerThread());
    MOZ_ASSERT(!IsCanceled());

    GetIO()->Connect();
  }
};

class BluetoothSocket::DelayedConnectTask final
  : public SocketIOTask<BluetoothSocketIO>
{
public:
  DelayedConnectTask(BluetoothSocketIO* aIO)
    : SocketIOTask<BluetoothSocketIO>(aIO)
  { }

  void Run() override
  {
    MOZ_ASSERT(GetIO()->IsConsumerThread());

    if (IsCanceled()) {
      return;
    }

    BluetoothSocketIO* io = GetIO();
    if (io->IsShutdownOnConsumerThread()) {
      return;
    }

    io->ClearDelayedConnectTask();
    io->GetIOLoop()->PostTask(FROM_HERE, new ConnectTask(io));
  }
};

//
// BluetoothSocket
//

BluetoothSocket::BluetoothSocket(BluetoothSocketObserver* aObserver)
  : mObserver(aObserver)
  , mIO(nullptr)
{
  MOZ_ASSERT(aObserver);

  MOZ_COUNT_CTOR_INHERITED(BluetoothSocket, DataSocket);
}

BluetoothSocket::~BluetoothSocket()
{
  MOZ_ASSERT(!mIO);

  MOZ_COUNT_DTOR_INHERITED(BluetoothSocket, DataSocket);
}

nsresult
BluetoothSocket::Connect(const nsAString& aDeviceAddress,
                         const BluetoothUuid& aServiceUuid,
                         BluetoothSocketType aType,
                         int aChannel,
                         bool aAuth, bool aEncrypt)
{
  MOZ_ASSERT(!aDeviceAddress.IsEmpty());

  nsAutoPtr<BluetoothUnixSocketConnector> connector(
    new BluetoothUnixSocketConnector(NS_ConvertUTF16toUTF8(aDeviceAddress),
                                     aType, aChannel, aAuth, aEncrypt));

  nsresult rv = Connect(connector);
  if (NS_FAILED(rv)) {
    nsAutoString addr;
    GetAddress(addr);
    BT_LOGD("%s failed. Current connected device address: %s",
           __FUNCTION__, NS_ConvertUTF16toUTF8(addr).get());
    return rv;
  }
  connector.forget();

  return NS_OK;
}

nsresult
BluetoothSocket::Listen(const nsAString& aServiceName,
                        const BluetoothUuid& aServiceUuid,
                        BluetoothSocketType aType,
                        int aChannel,
                        bool aAuth, bool aEncrypt)
{
  nsAutoPtr<BluetoothUnixSocketConnector> connector(
    new BluetoothUnixSocketConnector(NS_LITERAL_CSTRING(BLUETOOTH_ADDRESS_NONE),
                                     aType, aChannel, aAuth, aEncrypt));

  nsresult rv = Listen(connector);
  if (NS_FAILED(rv)) {
    nsAutoString addr;
    GetAddress(addr);
    BT_LOGD("%s failed. Current connected device address: %s",
           __FUNCTION__, NS_ConvertUTF16toUTF8(addr).get());
    return rv;
  }
  connector.forget();

  return NS_OK;
}

void
BluetoothSocket::ReceiveSocketData(nsAutoPtr<UnixSocketBuffer>& aBuffer)
{
  MOZ_ASSERT(mObserver);

  mObserver->ReceiveSocketData(this, aBuffer);
}

bool
BluetoothSocket::SendSocketData(const nsACString& aStr)
{
  if (aStr.Length() > MAX_READ_SIZE) {
    return false;
  }

  SendSocketData(new UnixSocketRawData(aStr.BeginReading(), aStr.Length()));

  return true;
}

nsresult
BluetoothSocket::Connect(BluetoothUnixSocketConnector* aConnector,
                         int aDelayMs,
                         MessageLoop* aConsumerLoop, MessageLoop* aIOLoop)
{
  MOZ_ASSERT(aConnector);
  MOZ_ASSERT(aConsumerLoop);
  MOZ_ASSERT(aIOLoop);
  MOZ_ASSERT(!mIO);

  mIO = new BluetoothSocketIO(aConsumerLoop, aIOLoop, this, aConnector);
  SetConnectionStatus(SOCKET_CONNECTING);

  if (aDelayMs > 0) {
    DelayedConnectTask* connectTask = new DelayedConnectTask(mIO);
    mIO->SetDelayedConnectTask(connectTask);
    MessageLoop::current()->PostDelayedTask(FROM_HERE, connectTask, aDelayMs);
  } else {
    aIOLoop->PostTask(FROM_HERE, new ConnectTask(mIO));
  }

  return NS_OK;
}

nsresult
BluetoothSocket::Connect(BluetoothUnixSocketConnector* aConnector,
                         int aDelayMs)
{
  return Connect(aConnector, aDelayMs, MessageLoop::current(),
                 XRE_GetIOMessageLoop());
}

nsresult
BluetoothSocket::Listen(BluetoothUnixSocketConnector* aConnector,
                        MessageLoop* aConsumerLoop, MessageLoop* aIOLoop)
{
  MOZ_ASSERT(aConnector);
  MOZ_ASSERT(aConsumerLoop);
  MOZ_ASSERT(aIOLoop);
  MOZ_ASSERT(!mIO);

  mIO = new BluetoothSocketIO(aConsumerLoop, aIOLoop, this, aConnector);
  SetConnectionStatus(SOCKET_LISTENING);

  aIOLoop->PostTask(FROM_HERE, new ListenTask(mIO));

  return NS_OK;
}

nsresult
BluetoothSocket::Listen(BluetoothUnixSocketConnector* aConnector)
{
  return Listen(aConnector, MessageLoop::current(), XRE_GetIOMessageLoop());
}

void
BluetoothSocket::GetAddress(nsAString& aAddrStr)
{
  aAddrStr.Truncate();
  if (!mIO || GetConnectionStatus() != SOCKET_CONNECTED) {
    NS_WARNING("No socket currently open!");
    return;
  }
  mIO->GetSocketAddr(aAddrStr);
}

// |DataSocket|

void
BluetoothSocket::SendSocketData(UnixSocketIOBuffer* aBuffer)
{
  MOZ_ASSERT(mIO);
  MOZ_ASSERT(mIO->IsConsumerThread());
  MOZ_ASSERT(!mIO->IsShutdownOnConsumerThread());

  mIO->GetIOLoop()->PostTask(
    FROM_HERE,
    new SocketIOSendTask<BluetoothSocketIO, UnixSocketIOBuffer>(mIO, aBuffer));
}

// |SocketBase|

void
BluetoothSocket::Close()
{
  if (!mIO) {
    return;
  }

  MOZ_ASSERT(mIO->IsConsumerThread());

  mIO->CancelDelayedConnectTask();

  // From this point on, we consider mIO as being deleted.
  // We sever the relationship here so any future calls to listen or connect
  // will create a new implementation.
  mIO->ShutdownOnConsumerThread();
  mIO->GetIOLoop()->PostTask(FROM_HERE, new SocketIOShutdownTask(mIO));
  mIO = nullptr;

  NotifyDisconnect();
}

void
BluetoothSocket::OnConnectSuccess()
{
  MOZ_ASSERT(mObserver);
  mObserver->OnSocketConnectSuccess(this);
}

void
BluetoothSocket::OnConnectError()
{
  MOZ_ASSERT(mObserver);
  mObserver->OnSocketConnectError(this);
}

void
BluetoothSocket::OnDisconnect()
{
  MOZ_ASSERT(mObserver);
  mObserver->OnSocketDisconnect(this);
}

END_BLUETOOTH_NAMESPACE
