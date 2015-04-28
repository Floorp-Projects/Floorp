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
  , protected DataSocketIO
  , public ConnectionOrientedSocketIO
{
public:
  class ConnectTask;
  class DelayedConnectTask;
  class ReceiveRunnable;

  StreamSocketIO(MessageLoop* mIOLoop,
                 StreamSocket* aStreamSocket,
                 UnixSocketConnector* aConnector,
                 const nsACString& aAddress);
  StreamSocketIO(MessageLoop* mIOLoop, int aFd,
                 ConnectionStatus aConnectionStatus,
                 StreamSocket* aStreamSocket,
                 UnixSocketConnector* aConnector,
                 const nsACString& aAddress);
  ~StreamSocketIO();

  void GetSocketAddr(nsAString& aAddrStr) const;

  StreamSocket* GetStreamSocket();
  DataSocket* GetDataSocket();
  SocketBase* GetSocketBase();

  // StreamSocketIOBase
  //

  nsresult Accept(int aFd,
                  const union sockaddr_any* aAddr, socklen_t aAddrLen);

  // Shutdown state
  //

  bool IsShutdownOnMainThread() const;
  void ShutdownOnMainThread();

  bool IsShutdownOnIOThread() const;
  void ShutdownOnIOThread();

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

  void OnAccepted(int aFd, const sockaddr_any* aAddr,
                  socklen_t aAddrLen) override;
  void OnConnected() override;
  void OnError(const char* aFunction, int aErrno) override;
  void OnListening() override;
  void OnSocketCanReceiveWithoutBlocking() override;
  void OnSocketCanSendWithoutBlocking() override;

  // Methods for |DataSocket|
  //

  nsresult QueryReceiveBuffer(UnixSocketIOBuffer** aBuffer);
  void ConsumeBuffer();
  void DiscardBuffer();

private:
  void FireSocketError();

  // Set up flags on file descriptor.
  static bool SetSocketFlags(int aFd);

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
   * Address we are connecting to, assuming we are creating a client connection.
   */
  nsCString mAddress;

  /**
   * Size of the socket address struct
   */
  socklen_t mAddrSize;

  /**
   * Address struct of the socket currently in use
   */
  sockaddr_any mAddr;

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
                               UnixSocketConnector* aConnector,
                               const nsACString& aAddress)
  : UnixSocketWatcher(mIOLoop)
  , mStreamSocket(aStreamSocket)
  , mConnector(aConnector)
  , mShuttingDownOnIOThread(false)
  , mAddress(aAddress)
  , mDelayedConnectTask(nullptr)
{
  MOZ_ASSERT(mStreamSocket);
  MOZ_ASSERT(mConnector);
}

StreamSocketIO::StreamSocketIO(MessageLoop* mIOLoop, int aFd,
                               ConnectionStatus aConnectionStatus,
                               StreamSocket* aStreamSocket,
                               UnixSocketConnector* aConnector,
                               const nsACString& aAddress)
  : UnixSocketWatcher(mIOLoop, aFd, aConnectionStatus)
  , mStreamSocket(aStreamSocket)
  , mConnector(aConnector)
  , mShuttingDownOnIOThread(false)
  , mAddress(aAddress)
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

void
StreamSocketIO::GetSocketAddr(nsAString& aAddrStr) const
{
  if (!mConnector) {
    NS_WARNING("No connector to get socket address from!");
    aAddrStr.Truncate();
    return;
  }
  mConnector->GetSocketAddr(mAddr, aAddrStr);
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

void
StreamSocketIO::ShutdownOnMainThread()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!IsShutdownOnMainThread());

  mStreamSocket = nullptr;
}

bool
StreamSocketIO::IsShutdownOnIOThread() const
{
  return mShuttingDownOnIOThread;
}

void
StreamSocketIO::ShutdownOnIOThread()
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!mShuttingDownOnIOThread);

  Close(); // will also remove fd from I/O loop
  mShuttingDownOnIOThread = true;
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

nsresult
StreamSocketIO::Accept(int aFd,
                       const union sockaddr_any* aAddr, socklen_t aAddrLen)
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());
  MOZ_ASSERT(GetConnectionStatus() == SOCKET_IS_CONNECTING);

  // File-descriptor setup

  if (!mConnector->SetUp(aFd)) {
    NS_WARNING("Could not set up socket!");
    return NS_ERROR_FAILURE;
  }

  if (!SetSocketFlags(aFd)) {
    return NS_ERROR_FAILURE;
  }
  SetSocket(aFd, SOCKET_IS_CONNECTED);

  AddWatchers(READ_WATCHER, true);
  if (HasPendingData()) {
    AddWatchers(WRITE_WATCHER, false);
  }

  // Address setup

  memcpy(&mAddr, aAddr, aAddrLen);
  mAddrSize = aAddrLen;

  // Signal success

  nsRefPtr<nsRunnable> r =
    new SocketIOEventRunnable<StreamSocketIO>(
      this, SocketIOEventRunnable<StreamSocketIO>::CONNECT_SUCCESS);
  NS_DispatchToMainThread(r);

  return NS_OK;
}

void
StreamSocketIO::Connect()
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());
  MOZ_ASSERT(mConnector);

  if (!IsOpen()) {
    int fd = mConnector->Create();
    if (fd < 0) {
      NS_WARNING("Cannot create socket fd!");
      FireSocketError();
      return;
    }
    if (!SetSocketFlags(fd)) {
      NS_WARNING("Cannot set socket flags!");
      FireSocketError();
      return;
    }
    SetFd(fd);
  }

  if (!mConnector->CreateAddr(false, mAddrSize, mAddr, mAddress.get())) {
    NS_WARNING("Cannot create socket address!");
    FireSocketError();
    return;
  }

  // calls OnConnected() on success, or OnError() otherwise
  nsresult rv = UnixSocketWatcher::Connect(
    reinterpret_cast<struct sockaddr*>(&mAddr), mAddrSize);
  NS_WARN_IF(NS_FAILED(rv));
}

void
StreamSocketIO::Send(UnixSocketIOBuffer* aData)
{
  EnqueueData(aData);
  AddWatchers(WRITE_WATCHER, false);
}

void
StreamSocketIO::OnAccepted(int aFd,
                           const sockaddr_any* aAddr,
                           socklen_t aAddrLen)
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());
  MOZ_ASSERT(GetConnectionStatus() == SOCKET_IS_LISTENING);
  MOZ_ASSERT(aAddr);
  MOZ_ASSERT(aAddrLen <= static_cast<socklen_t>(sizeof(mAddr)));

  memcpy (&mAddr, aAddr, aAddrLen);
  mAddrSize = aAddrLen;

  if (!mConnector->SetUp(aFd)) {
    NS_WARNING("Could not set up socket!");
    return;
  }

  RemoveWatchers(READ_WATCHER|WRITE_WATCHER);
  Close();
  if (!SetSocketFlags(aFd)) {
    return;
  }
  SetSocket(aFd, SOCKET_IS_CONNECTED);

  nsRefPtr<nsRunnable> r =
    new SocketIOEventRunnable<StreamSocketIO>(
      this, SocketIOEventRunnable<StreamSocketIO>::CONNECT_SUCCESS);
  NS_DispatchToMainThread(r);

  AddWatchers(READ_WATCHER, true);
  if (HasPendingData()) {
    AddWatchers(WRITE_WATCHER, false);
  }
}

void
StreamSocketIO::OnConnected()
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());
  MOZ_ASSERT(GetConnectionStatus() == SOCKET_IS_CONNECTED);

  if (!SetSocketFlags(GetFd())) {
    NS_WARNING("Cannot set socket flags!");
    FireSocketError();
    return;
  }

  if (!mConnector->SetUp(GetFd())) {
    NS_WARNING("Could not set up socket!");
    FireSocketError();
    return;
  }

  nsRefPtr<nsRunnable> r =
    new SocketIOEventRunnable<StreamSocketIO>(
      this, SocketIOEventRunnable<StreamSocketIO>::CONNECT_SUCCESS);
  NS_DispatchToMainThread(r);

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

  ssize_t res = ReceiveData(GetFd(), this);
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

  nsresult rv = SendPendingData(GetFd(), this);
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
  nsRefPtr<nsRunnable> r =
    new SocketIOEventRunnable<StreamSocketIO>(
      this, SocketIOEventRunnable<StreamSocketIO>::CONNECT_ERROR);

  NS_DispatchToMainThread(r);
}

bool
StreamSocketIO::SetSocketFlags(int aFd)
{
  static const int reuseaddr = 1;

  // Set socket addr to be reused even if kernel is still waiting to close
  int res = setsockopt(aFd, SOL_SOCKET, SO_REUSEADDR,
                       &reuseaddr, sizeof(reuseaddr));
  if (res < 0) {
    return false;
  }

  // Set close-on-exec bit.
  int flags = TEMP_FAILURE_RETRY(fcntl(aFd, F_GETFD));
  if (-1 == flags) {
    return false;
  }
  flags |= FD_CLOEXEC;
  if (-1 == TEMP_FAILURE_RETRY(fcntl(aFd, F_SETFD, flags))) {
    return false;
  }

  // Set non-blocking status flag.
  flags = TEMP_FAILURE_RETRY(fcntl(aFd, F_GETFL));
  if (-1 == flags) {
    return false;
  }
  flags |= O_NONBLOCK;
  if (-1 == TEMP_FAILURE_RETRY(fcntl(aFd, F_SETFL, flags))) {
    return false;
  }

  return true;
}

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

bool
StreamSocket::SendSocketData(const nsACString& aStr)
{
  if (aStr.Length() > MAX_READ_SIZE) {
    return false;
  }

  SendSocketData(new UnixSocketRawData(aStr.BeginReading(), aStr.Length()));

  return true;
}

void
StreamSocket::Close()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mIO) {
    return;
  }

  mIO->CancelDelayedConnectTask();

  // From this point on, we consider mIO as being deleted.
  // We sever the relationship here so any future calls to listen or connect
  // will create a new implementation.
  mIO->ShutdownOnMainThread();

  XRE_GetIOMessageLoop()->PostTask(
    FROM_HERE, new SocketIOShutdownTask<StreamSocketIO>(mIO));

  mIO = nullptr;

  NotifyDisconnect();
}

void
StreamSocket::GetSocketAddr(nsAString& aAddrStr)
{
  aAddrStr.Truncate();
  if (!mIO || GetConnectionStatus() != SOCKET_CONNECTED) {
    NS_WARNING("No socket currently open!");
    return;
  }
  mIO->GetSocketAddr(aAddrStr);
}

bool
StreamSocket::Connect(UnixSocketConnector* aConnector,
                      const char* aAddress,
                      int aDelayMs)
{
  MOZ_ASSERT(aConnector);
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<UnixSocketConnector> connector(aConnector);

  if (mIO) {
    NS_WARNING("Socket already connecting/connected!");
    return false;
  }

  nsCString addr(aAddress);
  MessageLoop* ioLoop = XRE_GetIOMessageLoop();
  mIO = new StreamSocketIO(ioLoop, this, connector.forget(), addr);
  SetConnectionStatus(SOCKET_CONNECTING);
  if (aDelayMs > 0) {
    StreamSocketIO::DelayedConnectTask* connectTask =
      new StreamSocketIO::DelayedConnectTask(mIO);
    mIO->SetDelayedConnectTask(connectTask);
    MessageLoop::current()->PostDelayedTask(FROM_HERE, connectTask, aDelayMs);
  } else {
    ioLoop->PostTask(FROM_HERE, new StreamSocketIO::ConnectTask(mIO));
  }

  return true;
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
                           this, connector.forget(), EmptyCString());
  return mIO;
}

} // namespace ipc
} // namespace mozilla
