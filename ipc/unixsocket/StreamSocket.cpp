/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StreamSocket.h"
#include <fcntl.h>
#include "mozilla/RefPtr.h"
#include "nsXULAppAPI.h"
#include "UnixSocketConnector.h"

static const size_t MAX_READ_SIZE = 1 << 16;

namespace mozilla {
namespace ipc {

//
// StreamSocketIO
//

class StreamSocketIO final
  : public UnixSocketWatcher
  , public ConnectionOrientedSocketIO
{
public:
  class ConnectTask;
  class DelayedConnectTask;
  class ReceiveRunnable;

  StreamSocketIO(MessageLoop* mIOLoop,
                 StreamSocket* aStreamSocket,
                 UnixSocketConnector* aConnector);
  StreamSocketIO(MessageLoop* mIOLoop, int aFd,
                 ConnectionStatus aConnectionStatus,
                 StreamSocket* aStreamSocket,
                 UnixSocketConnector* aConnector);
  ~StreamSocketIO();

  StreamSocket* GetStreamSocket();
  DataSocket* GetDataSocket();

  // Delayed-task handling
  //

  void SetDelayedConnectTask(CancelableTask* aTask);
  void ClearDelayedConnectTask();
  void CancelDelayedConnectTask();

  // Task callback methods
  //

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
  void OnSocketCanReceiveWithoutBlocking() override;
  void OnSocketCanSendWithoutBlocking() override;

  // Methods for |ConnectionOrientedSocketIO|
  //

  nsresult Accept(int aFd,
                  const union sockaddr_any* aAddr,
                  socklen_t aAddrLen) override;

  // Methods for |DataSocket|
  //

  nsresult QueryReceiveBuffer(UnixSocketIOBuffer** aBuffer) override;
  void ConsumeBuffer() override;
  void DiscardBuffer() override;

  // Methods for |SocketIOBase|
  //

  SocketBase* GetSocketBase() override;

  bool IsShutdownOnMainThread() const override;
  bool IsShutdownOnIOThread() const override;

  void ShutdownOnMainThread() override;
  void ShutdownOnIOThread() override;

private:
  void FireSocketError();

  /**
   * Consumer pointer. Non-thread safe RefPtr, so should only be manipulated
   * directly from main thread. All non-main-thread accesses should happen with
   * mIO as container.
   */
  RefPtr<StreamSocket> mStreamSocket;

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
   * Task member for delayed connect task. Should only be access on main thread.
   */
  CancelableTask* mDelayedConnectTask;

  /**
   * I/O buffer for received data
   */
  nsAutoPtr<UnixSocketRawData> mBuffer;
};

StreamSocketIO::StreamSocketIO(MessageLoop* mIOLoop,
                               StreamSocket* aStreamSocket,
                               UnixSocketConnector* aConnector)
  : UnixSocketWatcher(mIOLoop)
  , mStreamSocket(aStreamSocket)
  , mConnector(aConnector)
  , mShuttingDownOnIOThread(false)
  , mAddressLength(0)
  , mDelayedConnectTask(nullptr)
{
  MOZ_ASSERT(mStreamSocket);
  MOZ_ASSERT(mConnector);
}

StreamSocketIO::StreamSocketIO(MessageLoop* mIOLoop, int aFd,
                               ConnectionStatus aConnectionStatus,
                               StreamSocket* aStreamSocket,
                               UnixSocketConnector* aConnector)
  : UnixSocketWatcher(mIOLoop, aFd, aConnectionStatus)
  , mStreamSocket(aStreamSocket)
  , mConnector(aConnector)
  , mShuttingDownOnIOThread(false)
  , mAddressLength(0)
  , mDelayedConnectTask(nullptr)
{
  MOZ_ASSERT(mStreamSocket);
  MOZ_ASSERT(mConnector);
}

StreamSocketIO::~StreamSocketIO()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(IsShutdownOnMainThread());
}

StreamSocket*
StreamSocketIO::GetStreamSocket()
{
  return mStreamSocket.get();
}

DataSocket*
StreamSocketIO::GetDataSocket()
{
  return mStreamSocket.get();
}

void
StreamSocketIO::SetDelayedConnectTask(CancelableTask* aTask)
{
  MOZ_ASSERT(NS_IsMainThread());

  mDelayedConnectTask = aTask;
}

void
StreamSocketIO::ClearDelayedConnectTask()
{
  MOZ_ASSERT(NS_IsMainThread());

  mDelayedConnectTask = nullptr;
}

void
StreamSocketIO::CancelDelayedConnectTask()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mDelayedConnectTask) {
    return;
  }

  mDelayedConnectTask->Cancel();
  ClearDelayedConnectTask();
}

void
StreamSocketIO::Connect()
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());
  MOZ_ASSERT(mConnector);

  MOZ_ASSERT(!IsOpen());

  struct sockaddr* address = reinterpret_cast<struct sockaddr*>(&mAddress);
  mAddressLength = sizeof(mAddress);

  int fd;
  nsresult rv = mConnector->CreateStreamSocket(address, &mAddressLength, fd);
  if (NS_FAILED(rv)) {
    FireSocketError();
    return;
  }
  SetFd(fd);

  // calls OnConnected() on success, or OnError() otherwise
  rv = UnixSocketWatcher::Connect(address, mAddressLength);
  NS_WARN_IF(NS_FAILED(rv));
}

void
StreamSocketIO::Send(UnixSocketIOBuffer* aData)
{
  EnqueueData(aData);
  AddWatchers(WRITE_WATCHER, false);
}

void
StreamSocketIO::OnConnected()
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());
  MOZ_ASSERT(GetConnectionStatus() == SOCKET_IS_CONNECTED);

  NS_DispatchToMainThread(
    new SocketIOEventRunnable(this, SocketIOEventRunnable::CONNECT_SUCCESS));

  AddWatchers(READ_WATCHER, true);
  if (HasPendingData()) {
    AddWatchers(WRITE_WATCHER, false);
  }
}

void
StreamSocketIO::OnListening()
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());

  NS_NOTREACHED("Invalid call to |StreamSocketIO::OnListening|");
}

void
StreamSocketIO::OnError(const char* aFunction, int aErrno)
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());

  UnixFdWatcher::OnError(aFunction, aErrno);
  FireSocketError();
}

void
StreamSocketIO::OnSocketCanReceiveWithoutBlocking()
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
StreamSocketIO::OnSocketCanSendWithoutBlocking()
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
StreamSocketIO::FireSocketError()
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());

  // Clean up watchers, statuses, fds
  Close();

  // Tell the main thread we've errored
  NS_DispatchToMainThread(
    new SocketIOEventRunnable(this, SocketIOEventRunnable::CONNECT_ERROR));
}

// |ConnectionOrientedSocketIO|

nsresult
StreamSocketIO::Accept(int aFd,
                       const union sockaddr_any* aAddr, socklen_t aAddrLen)
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());
  MOZ_ASSERT(GetConnectionStatus() == SOCKET_IS_CONNECTING);

  SetSocket(aFd, SOCKET_IS_CONNECTED);

  // Address setup
  mAddressLength = aAddrLen;
  memcpy(&mAddress, aAddr, mAddressLength);

  // Signal success
  NS_DispatchToMainThread(
    new SocketIOEventRunnable(this, SocketIOEventRunnable::CONNECT_SUCCESS));

  AddWatchers(READ_WATCHER, true);
  if (HasPendingData()) {
    AddWatchers(WRITE_WATCHER, false);
  }

  return NS_OK;
}

// |DataSocketIO|

nsresult
StreamSocketIO::QueryReceiveBuffer(UnixSocketIOBuffer** aBuffer)
{
  MOZ_ASSERT(aBuffer);

  if (!mBuffer) {
    mBuffer = new UnixSocketRawData(MAX_READ_SIZE);
  }
  *aBuffer = mBuffer.get();

  return NS_OK;
}

/**
 * |ReceiveRunnable| transfers data received on the I/O thread
 * to an instance of |StreamSocket| on the main thread.
 */
class StreamSocketIO::ReceiveRunnable final
  : public SocketIORunnable<StreamSocketIO>
{
public:
  ReceiveRunnable(StreamSocketIO* aIO, UnixSocketBuffer* aBuffer)
    : SocketIORunnable<StreamSocketIO>(aIO)
    , mBuffer(aBuffer)
  { }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    StreamSocketIO* io = SocketIORunnable<StreamSocketIO>::GetIO();

    if (NS_WARN_IF(io->IsShutdownOnMainThread())) {
      // Since we've already explicitly closed and the close
      // happened before this, this isn't really an error.
      return NS_OK;
    }

    StreamSocket* streamSocket = io->GetStreamSocket();
    MOZ_ASSERT(streamSocket);

    streamSocket->ReceiveSocketData(mBuffer);

    return NS_OK;
  }

private:
  nsAutoPtr<UnixSocketBuffer> mBuffer;
};

void
StreamSocketIO::ConsumeBuffer()
{
  NS_DispatchToMainThread(new ReceiveRunnable(this, mBuffer.forget()));
}

void
StreamSocketIO::DiscardBuffer()
{
  // Nothing to do.
}

// |SocketIOBase|

SocketBase*
StreamSocketIO::GetSocketBase()
{
  return GetDataSocket();
}

bool
StreamSocketIO::IsShutdownOnMainThread() const
{
  MOZ_ASSERT(NS_IsMainThread());

  return mStreamSocket == nullptr;
}

bool
StreamSocketIO::IsShutdownOnIOThread() const
{
  return mShuttingDownOnIOThread;
}

void
StreamSocketIO::ShutdownOnMainThread()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!IsShutdownOnMainThread());

  mStreamSocket = nullptr;
}

void
StreamSocketIO::ShutdownOnIOThread()
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!mShuttingDownOnIOThread);

  Close(); // will also remove fd from I/O loop
  mShuttingDownOnIOThread = true;
}

//
// Socket tasks
//

class StreamSocketIO::ConnectTask final
  : public SocketIOTask<StreamSocketIO>
{
public:
  ConnectTask(StreamSocketIO* aIO)
  : SocketIOTask<StreamSocketIO>(aIO)
  { }

  void Run() override
  {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(!IsCanceled());

    GetIO()->Connect();
  }
};

class StreamSocketIO::DelayedConnectTask final
  : public SocketIOTask<StreamSocketIO>
{
public:
  DelayedConnectTask(StreamSocketIO* aIO)
  : SocketIOTask<StreamSocketIO>(aIO)
  { }

  void Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (IsCanceled()) {
      return;
    }

    StreamSocketIO* io = GetIO();
    if (io->IsShutdownOnMainThread()) {
      return;
    }

    io->ClearDelayedConnectTask();
    XRE_GetIOMessageLoop()->PostTask(FROM_HERE, new ConnectTask(io));
  }
};

//
// StreamSocket
//

StreamSocket::StreamSocket()
: mIO(nullptr)
{ }

StreamSocket::~StreamSocket()
{
  MOZ_ASSERT(!mIO);
}

nsresult
StreamSocket::Connect(UnixSocketConnector* aConnector,
                      int aDelayMs)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mIO);

  MessageLoop* ioLoop = XRE_GetIOMessageLoop();
  mIO = new StreamSocketIO(ioLoop, this, aConnector);
  SetConnectionStatus(SOCKET_CONNECTING);

  if (aDelayMs > 0) {
    StreamSocketIO::DelayedConnectTask* connectTask =
      new StreamSocketIO::DelayedConnectTask(mIO);
    mIO->SetDelayedConnectTask(connectTask);
    MessageLoop::current()->PostDelayedTask(FROM_HERE, connectTask, aDelayMs);
  } else {
    ioLoop->PostTask(FROM_HERE, new StreamSocketIO::ConnectTask(mIO));
  }
  return NS_OK;
}

ConnectionOrientedSocketIO*
StreamSocket::PrepareAccept(UnixSocketConnector* aConnector)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mIO);
  MOZ_ASSERT(aConnector);

  nsAutoPtr<UnixSocketConnector> connector(aConnector);

  SetConnectionStatus(SOCKET_CONNECTING);

  mIO = new StreamSocketIO(XRE_GetIOMessageLoop(),
                           -1, UnixSocketWatcher::SOCKET_IS_CONNECTING,
                           this, connector.forget());
  return mIO;
}

// |DataSocket|

void
StreamSocket::SendSocketData(UnixSocketIOBuffer* aBuffer)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mIO);

  MOZ_ASSERT(!mIO->IsShutdownOnMainThread());
  XRE_GetIOMessageLoop()->PostTask(
    FROM_HERE,
    new SocketIOSendTask<StreamSocketIO, UnixSocketIOBuffer>(mIO, aBuffer));
}

// |SocketBase|

void
StreamSocket::Close()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mIO);

  mIO->CancelDelayedConnectTask();

  // From this point on, we consider |mIO| as being deleted. We sever
  // the relationship here so any future calls to |Connect| will create
  // a new I/O object.
  mIO->ShutdownOnMainThread();

  XRE_GetIOMessageLoop()->PostTask(FROM_HERE, new SocketIOShutdownTask(mIO));

  mIO = nullptr;

  NotifyDisconnect();
}


} // namespace ipc
} // namespace mozilla
