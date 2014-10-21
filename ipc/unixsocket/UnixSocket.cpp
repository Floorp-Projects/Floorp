/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UnixSocket.h"
#include "nsTArray.h"
#include "nsXULAppAPI.h"
#include <fcntl.h>
#include "mozilla/unused.h"

static const size_t MAX_READ_SIZE = 1 << 16;

namespace mozilla {
namespace ipc {

//
// UnixSocketConsumerIO
//

class UnixSocketConsumerIO MOZ_FINAL : public UnixSocketWatcher
                                     , protected SocketIOBase
{
public:
  UnixSocketConsumerIO(MessageLoop* mIOLoop,
                       UnixSocketConsumer* aConsumer,
                       UnixSocketConnector* aConnector,
                       const nsACString& aAddress);
  ~UnixSocketConsumerIO();

  void                GetSocketAddr(nsAString& aAddrStr) const;
  SocketConsumerBase* GetConsumer();
  SocketBase*         GetSocketBase();

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
   * Run bind/listen to prepare for further runs of accept()
   */
  void Listen();

  /**
   * Connect to a socket
   */
  void Connect();

  void Send(UnixSocketRawData* aData);

  // I/O callback methods
  //

  void OnAccepted(int aFd, const sockaddr_any* aAddr,
                  socklen_t aAddrLen) MOZ_OVERRIDE;
  void OnConnected() MOZ_OVERRIDE;
  void OnError(const char* aFunction, int aErrno) MOZ_OVERRIDE;
  void OnListening() MOZ_OVERRIDE;
  void OnSocketCanReceiveWithoutBlocking() MOZ_OVERRIDE;
  void OnSocketCanSendWithoutBlocking() MOZ_OVERRIDE;

private:
  void FireSocketError();

  // Set up flags on file descriptor.
  static bool SetSocketFlags(int aFd);

  /**
   * Consumer pointer. Non-thread safe RefPtr, so should only be manipulated
   * directly from main thread. All non-main-thread accesses should happen with
   * mIO as container.
   */
  RefPtr<UnixSocketConsumer> mConsumer;

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
};

UnixSocketConsumerIO::UnixSocketConsumerIO(MessageLoop* mIOLoop,
                                           UnixSocketConsumer* aConsumer,
                                           UnixSocketConnector* aConnector,
                                           const nsACString& aAddress)
  : UnixSocketWatcher(mIOLoop)
  , SocketIOBase(MAX_READ_SIZE)
  , mConsumer(aConsumer)
  , mConnector(aConnector)
  , mShuttingDownOnIOThread(false)
  , mAddress(aAddress)
  , mDelayedConnectTask(nullptr)
{
  MOZ_ASSERT(mConsumer);
  MOZ_ASSERT(mConnector);
}

UnixSocketConsumerIO::~UnixSocketConsumerIO()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(IsShutdownOnMainThread());
}

void
UnixSocketConsumerIO::GetSocketAddr(nsAString& aAddrStr) const
{
  if (!mConnector) {
    NS_WARNING("No connector to get socket address from!");
    aAddrStr.Truncate();
    return;
  }
  mConnector->GetSocketAddr(mAddr, aAddrStr);
}

SocketConsumerBase*
UnixSocketConsumerIO::GetConsumer()
{
  return mConsumer.get();
}

SocketBase*
UnixSocketConsumerIO::GetSocketBase()
{
  return GetConsumer();
}

bool
UnixSocketConsumerIO::IsShutdownOnMainThread() const
{
  MOZ_ASSERT(NS_IsMainThread());

  return mConsumer == nullptr;
}

void
UnixSocketConsumerIO::ShutdownOnMainThread()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!IsShutdownOnMainThread());

  mConsumer = nullptr;
}

bool
UnixSocketConsumerIO::IsShutdownOnIOThread() const
{
  return mShuttingDownOnIOThread;
}

void
UnixSocketConsumerIO::ShutdownOnIOThread()
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!mShuttingDownOnIOThread);

  Close(); // will also remove fd from I/O loop
  mShuttingDownOnIOThread = true;
}

void
UnixSocketConsumerIO::SetDelayedConnectTask(CancelableTask* aTask)
{
  MOZ_ASSERT(NS_IsMainThread());

  mDelayedConnectTask = aTask;
}

void
UnixSocketConsumerIO::ClearDelayedConnectTask()
{
  MOZ_ASSERT(NS_IsMainThread());

  mDelayedConnectTask = nullptr;
}

void
UnixSocketConsumerIO::CancelDelayedConnectTask()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mDelayedConnectTask) {
    return;
  }

  mDelayedConnectTask->Cancel();
  ClearDelayedConnectTask();
}

void
UnixSocketConsumerIO::Listen()
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());
  MOZ_ASSERT(mConnector);

  // This will set things we don't particularly care about, but it will hand
  // back the correct structure size which is what we do care about.
  if (!mConnector->CreateAddr(true, mAddrSize, mAddr, nullptr)) {
    NS_WARNING("Cannot create socket address!");
    FireSocketError();
    return;
  }

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

    // calls OnListening on success, or OnError otherwise
    nsresult rv = UnixSocketWatcher::Listen(
      reinterpret_cast<struct sockaddr*>(&mAddr), mAddrSize);
    NS_WARN_IF(NS_FAILED(rv));
  }
}

void
UnixSocketConsumerIO::Connect()
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
UnixSocketConsumerIO::Send(UnixSocketRawData* aData)
{
  EnqueueData(aData);
  AddWatchers(WRITE_WATCHER, false);
}

void
UnixSocketConsumerIO::OnAccepted(int aFd,
                                 const sockaddr_any* aAddr,
                                 socklen_t aAddrLen)
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());
  MOZ_ASSERT(GetConnectionStatus() == SOCKET_IS_LISTENING);
  MOZ_ASSERT(aAddr);
  MOZ_ASSERT(aAddrLen <= sizeof(mAddr));

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
    new SocketIOEventRunnable<UnixSocketConsumerIO>(
      this, SocketIOEventRunnable<UnixSocketConsumerIO>::CONNECT_SUCCESS);
  NS_DispatchToMainThread(r);

  AddWatchers(READ_WATCHER, true);
  if (HasPendingData()) {
    AddWatchers(WRITE_WATCHER, false);
  }
}

void
UnixSocketConsumerIO::OnConnected()
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
    new SocketIOEventRunnable<UnixSocketConsumerIO>(
      this, SocketIOEventRunnable<UnixSocketConsumerIO>::CONNECT_SUCCESS);
  NS_DispatchToMainThread(r);

  AddWatchers(READ_WATCHER, true);
  if (HasPendingData()) {
    AddWatchers(WRITE_WATCHER, false);
  }
}

void
UnixSocketConsumerIO::OnListening()
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());
  MOZ_ASSERT(GetConnectionStatus() == SOCKET_IS_LISTENING);

  if (!mConnector->SetUpListenSocket(GetFd())) {
    NS_WARNING("Could not set up listen socket!");
    FireSocketError();
    return;
  }

  AddWatchers(READ_WATCHER, true);
}

void
UnixSocketConsumerIO::OnError(const char* aFunction, int aErrno)
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());

  UnixFdWatcher::OnError(aFunction, aErrno);
  FireSocketError();
}

void
UnixSocketConsumerIO::OnSocketCanReceiveWithoutBlocking()
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
UnixSocketConsumerIO::OnSocketCanSendWithoutBlocking()
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
UnixSocketConsumerIO::FireSocketError()
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());

  // Clean up watchers, statuses, fds
  Close();

  // Tell the main thread we've errored
  nsRefPtr<nsRunnable> r =
    new SocketIOEventRunnable<UnixSocketConsumerIO>(
      this, SocketIOEventRunnable<UnixSocketConsumerIO>::CONNECT_ERROR);

  NS_DispatchToMainThread(r);
}

bool
UnixSocketConsumerIO::SetSocketFlags(int aFd)
{
  // Set socket addr to be reused even if kernel is still waiting to close
  int n = 1;
  if (setsockopt(aFd, SOL_SOCKET, SO_REUSEADDR, &n, sizeof(n)) < 0) {
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

//
// Socket tasks
//

class ListenTask MOZ_FINAL : public SocketIOTask<UnixSocketConsumerIO>
{
public:
  ListenTask(UnixSocketConsumerIO* aIO)
  : SocketIOTask<UnixSocketConsumerIO>(aIO)
  { }

  void Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(!NS_IsMainThread());

    if (!IsCanceled()) {
      GetIO()->Listen();
    }
  }
};

class ConnectTask MOZ_FINAL : public SocketIOTask<UnixSocketConsumerIO>
{
public:
  ConnectTask(UnixSocketConsumerIO* aIO)
  : SocketIOTask<UnixSocketConsumerIO>(aIO)
  { }

  void Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(!IsCanceled());

    GetIO()->Connect();
  }
};

class DelayedConnectTask MOZ_FINAL : public SocketIOTask<UnixSocketConsumerIO>
{
public:
  DelayedConnectTask(UnixSocketConsumerIO* aIO)
  : SocketIOTask<UnixSocketConsumerIO>(aIO)
  { }

  void Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (IsCanceled()) {
      return;
    }

    UnixSocketConsumerIO* io = GetIO();
    if (io->IsShutdownOnMainThread()) {
      return;
    }

    io->ClearDelayedConnectTask();
    XRE_GetIOMessageLoop()->PostTask(FROM_HERE, new ConnectTask(io));
  }
};

//
// UnixSocketConsumer
//

UnixSocketConsumer::UnixSocketConsumer()
: mIO(nullptr)
{ }

UnixSocketConsumer::~UnixSocketConsumer()
{
  MOZ_ASSERT(!mIO);
}

bool
UnixSocketConsumer::SendSocketData(UnixSocketRawData* aData)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!mIO) {
    return false;
  }

  MOZ_ASSERT(!mIO->IsShutdownOnMainThread());
  XRE_GetIOMessageLoop()->PostTask(
    FROM_HERE,
    new SocketIOSendTask<UnixSocketConsumerIO, UnixSocketRawData>(mIO, aData));

  return true;
}

bool
UnixSocketConsumer::SendSocketData(const nsACString& aStr)
{
  if (aStr.Length() > MAX_READ_SIZE) {
    return false;
  }

  nsAutoPtr<UnixSocketRawData> data(
    new UnixSocketRawData(aStr.BeginReading(), aStr.Length()));

  if (!SendSocketData(data)) {
    return false;
  }

  unused << data.forget();

  return true;
}

void
UnixSocketConsumer::CloseSocket()
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
    FROM_HERE, new SocketIOShutdownTask<UnixSocketConsumerIO>(mIO));

  mIO = nullptr;

  NotifyDisconnect();
}

void
UnixSocketConsumer::GetSocketAddr(nsAString& aAddrStr)
{
  aAddrStr.Truncate();
  if (!mIO || GetConnectionStatus() != SOCKET_CONNECTED) {
    NS_WARNING("No socket currently open!");
    return;
  }
  mIO->GetSocketAddr(aAddrStr);
}

bool
UnixSocketConsumer::ConnectSocket(UnixSocketConnector* aConnector,
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
  mIO = new UnixSocketConsumerIO(ioLoop, this, connector.forget(), addr);
  SetConnectionStatus(SOCKET_CONNECTING);
  if (aDelayMs > 0) {
    DelayedConnectTask* connectTask = new DelayedConnectTask(mIO);
    mIO->SetDelayedConnectTask(connectTask);
    MessageLoop::current()->PostDelayedTask(FROM_HERE, connectTask, aDelayMs);
  } else {
    ioLoop->PostTask(FROM_HERE, new ConnectTask(mIO));
  }
  return true;
}

bool
UnixSocketConsumer::ListenSocket(UnixSocketConnector* aConnector)
{
  MOZ_ASSERT(aConnector);
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<UnixSocketConnector> connector(aConnector);

  if (mIO) {
    NS_WARNING("Socket already connecting/connected!");
    return false;
  }

  mIO = new UnixSocketConsumerIO(
    XRE_GetIOMessageLoop(), this, connector.forget(), EmptyCString());
  SetConnectionStatus(SOCKET_LISTENING);
  XRE_GetIOMessageLoop()->PostTask(FROM_HERE, new ListenTask(mIO));
  return true;
}

} // namespace ipc
} // namespace mozilla
