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
// UnixSocketImpl
//

class UnixSocketImpl : public UnixSocketWatcher
                     , protected SocketIOBase
{
public:
  UnixSocketImpl(MessageLoop* mIOLoop,
                 UnixSocketConsumer* aConsumer, UnixSocketConnector* aConnector,
                 const nsACString& aAddress)
    : UnixSocketWatcher(mIOLoop)
    , SocketIOBase(MAX_READ_SIZE)
    , mConsumer(aConsumer)
    , mConnector(aConnector)
    , mShuttingDownOnIOThread(false)
    , mAddress(aAddress)
    , mDelayedConnectTask(nullptr)
  {
  }

  ~UnixSocketImpl()
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(IsShutdownOnMainThread());
  }

  void Send(UnixSocketRawData* aData)
  {
    EnqueueData(aData);
    AddWatchers(WRITE_WATCHER, false);
  }

  bool IsShutdownOnMainThread()
  {
    MOZ_ASSERT(NS_IsMainThread());
    return mConsumer == nullptr;
  }

  void ShutdownOnMainThread()
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(!IsShutdownOnMainThread());
    mConsumer = nullptr;
  }

  bool IsShutdownOnIOThread()
  {
    return mShuttingDownOnIOThread;
  }

  void ShutdownOnIOThread()
  {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(!mShuttingDownOnIOThread);

    Close(); // will also remove fd from I/O loop
    mShuttingDownOnIOThread = true;
  }

  void SetDelayedConnectTask(CancelableTask* aTask)
  {
    MOZ_ASSERT(NS_IsMainThread());
    mDelayedConnectTask = aTask;
  }

  void ClearDelayedConnectTask()
  {
    MOZ_ASSERT(NS_IsMainThread());
    mDelayedConnectTask = nullptr;
  }

  void CancelDelayedConnectTask()
  {
    MOZ_ASSERT(NS_IsMainThread());
    if (!mDelayedConnectTask) {
      return;
    }
    mDelayedConnectTask->Cancel();
    ClearDelayedConnectTask();
  }

  /**
   * Connect to a socket
   */
  void Connect();

  /**
   * Run bind/listen to prepare for further runs of accept()
   */
  void Listen();

  void GetSocketAddr(nsAString& aAddrStr)
  {
    if (!mConnector) {
      NS_WARNING("No connector to get socket address from!");
      aAddrStr.Truncate();
      return;
    }
    mConnector->GetSocketAddr(mAddr, aAddrStr);
  }

  /**
   * Consumer pointer. Non-thread safe RefPtr, so should only be manipulated
   * directly from main thread. All non-main-thread accesses should happen with
   * mImpl as container.
   */
  RefPtr<UnixSocketConsumer> mConsumer;

  void OnAccepted(int aFd, const sockaddr_any* aAddr,
                  socklen_t aAddrLen) MOZ_OVERRIDE;
  void OnConnected() MOZ_OVERRIDE;
  void OnError(const char* aFunction, int aErrno) MOZ_OVERRIDE;
  void OnListening() MOZ_OVERRIDE;
  void OnSocketCanReceiveWithoutBlocking() MOZ_OVERRIDE;
  void OnSocketCanSendWithoutBlocking() MOZ_OVERRIDE;

  SocketConsumerBase* GetConsumer()
  {
    return mConsumer.get();
  }

private:
  // Set up flags on whatever our current file descriptor is.
  static bool SetSocketFlags(int aFd);

  void FireSocketError();

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

class SocketListenTask : public SocketIOTask<UnixSocketImpl>
{
public:
  SocketListenTask(UnixSocketImpl* aImpl)
  : SocketIOTask<UnixSocketImpl>(aImpl)
  { }

  void Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(!NS_IsMainThread());
    if (!IsCanceled()) {
      GetIO()->Listen();
    }
  }
};

class SocketConnectTask : public SocketIOTask<UnixSocketImpl>
{
public:
  SocketConnectTask(UnixSocketImpl* aImpl)
  : SocketIOTask<UnixSocketImpl>(aImpl)
  { }

  void Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(!IsCanceled());
    GetIO()->Connect();
  }
};

class SocketDelayedConnectTask : public SocketIOTask<UnixSocketImpl>
{
public:
  SocketDelayedConnectTask(UnixSocketImpl* aImpl)
  : SocketIOTask<UnixSocketImpl>(aImpl)
  { }

  void Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread());
    if (IsCanceled()) {
      return;
    }
    UnixSocketImpl* impl = GetIO();
    if (impl->IsShutdownOnMainThread()) {
      return;
    }
    impl->ClearDelayedConnectTask();
    XRE_GetIOMessageLoop()->PostTask(FROM_HERE, new SocketConnectTask(impl));
  }
};

class ShutdownSocketTask : public SocketIOTask<UnixSocketImpl>
{
public:
  ShutdownSocketTask(UnixSocketImpl* aImpl)
  : SocketIOTask<UnixSocketImpl>(aImpl)
  { }

  void Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(!IsCanceled());

    UnixSocketImpl* impl = GetIO();

    // At this point, there should be no new events on the IO thread after this
    // one with the possible exception of a SocketListenTask that
    // ShutdownOnIOThread will cancel for us. We are now fully shut down, so we
    // can send a message to the main thread that will delete impl safely knowing
    // that no more tasks reference it.
    impl->ShutdownOnIOThread();

    nsRefPtr<nsIRunnable> r =
      new SocketIODeleteInstanceRunnable<UnixSocketImpl>(impl);
    nsresult rv = NS_DispatchToMainThread(r);
    NS_ENSURE_SUCCESS_VOID(rv);
  }
};

void
UnixSocketImpl::FireSocketError()
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());

  // Clean up watchers, statuses, fds
  Close();

  // Tell the main thread we've errored
  nsRefPtr<nsRunnable> r =
    new SocketIOEventRunnable<UnixSocketImpl>(
      this, SocketIOEventRunnable<UnixSocketImpl>::CONNECT_ERROR);

  NS_DispatchToMainThread(r);
}

void
UnixSocketImpl::Listen()
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
UnixSocketImpl::Connect()
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

bool
UnixSocketImpl::SetSocketFlags(int aFd)
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

void
UnixSocketImpl::OnAccepted(int aFd,
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
    new SocketIOEventRunnable<UnixSocketImpl>(
      this, SocketIOEventRunnable<UnixSocketImpl>::CONNECT_SUCCESS);
  NS_DispatchToMainThread(r);

  AddWatchers(READ_WATCHER, true);
  if (HasPendingData()) {
    AddWatchers(WRITE_WATCHER, false);
  }
}

void
UnixSocketImpl::OnConnected()
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
    new SocketIOEventRunnable<UnixSocketImpl>(
      this, SocketIOEventRunnable<UnixSocketImpl>::CONNECT_SUCCESS);
  NS_DispatchToMainThread(r);

  AddWatchers(READ_WATCHER, true);
  if (HasPendingData()) {
    AddWatchers(WRITE_WATCHER, false);
  }
}

void
UnixSocketImpl::OnListening()
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
UnixSocketImpl::OnError(const char* aFunction, int aErrno)
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());

  UnixFdWatcher::OnError(aFunction, aErrno);
  FireSocketError();
}

void
UnixSocketImpl::OnSocketCanReceiveWithoutBlocking()
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());
  MOZ_ASSERT(GetConnectionStatus() == SOCKET_IS_CONNECTED); // see bug 990984

  nsresult rv = ReceiveData(GetFd(), this);
  if (NS_FAILED(rv)) {
    RemoveWatchers(READ_WATCHER|WRITE_WATCHER);
    return;
  }
}

void
UnixSocketImpl::OnSocketCanSendWithoutBlocking()
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

//
// UnixSocketConsumer
//

UnixSocketConsumer::UnixSocketConsumer()
: mImpl(nullptr)
{ }

UnixSocketConsumer::~UnixSocketConsumer()
{
  MOZ_ASSERT(!mImpl);
}

bool
UnixSocketConsumer::SendSocketData(UnixSocketRawData* aData)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!mImpl) {
    return false;
  }

  MOZ_ASSERT(!mImpl->IsShutdownOnMainThread());
  XRE_GetIOMessageLoop()->PostTask(
    FROM_HERE, new SocketIOSendTask<UnixSocketImpl>(mImpl, aData));

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
  if (!mImpl) {
    return;
  }

  mImpl->CancelDelayedConnectTask();

  // From this point on, we consider mImpl as being deleted.
  // We sever the relationship here so any future calls to listen or connect
  // will create a new implementation.
  mImpl->ShutdownOnMainThread();

  XRE_GetIOMessageLoop()->PostTask(FROM_HERE,
                                   new ShutdownSocketTask(mImpl));

  mImpl = nullptr;

  NotifyDisconnect();
}

void
UnixSocketConsumer::GetSocketAddr(nsAString& aAddrStr)
{
  aAddrStr.Truncate();
  if (!mImpl || GetConnectionStatus() != SOCKET_CONNECTED) {
    NS_WARNING("No socket currently open!");
    return;
  }
  mImpl->GetSocketAddr(aAddrStr);
}

bool
UnixSocketConsumer::ConnectSocket(UnixSocketConnector* aConnector,
                                  const char* aAddress,
                                  int aDelayMs)
{
  MOZ_ASSERT(aConnector);
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<UnixSocketConnector> connector(aConnector);

  if (mImpl) {
    NS_WARNING("Socket already connecting/connected!");
    return false;
  }

  nsCString addr(aAddress);
  MessageLoop* ioLoop = XRE_GetIOMessageLoop();
  mImpl = new UnixSocketImpl(ioLoop, this, connector.forget(), addr);
  SetConnectionStatus(SOCKET_CONNECTING);
  if (aDelayMs > 0) {
    SocketDelayedConnectTask* connectTask = new SocketDelayedConnectTask(mImpl);
    mImpl->SetDelayedConnectTask(connectTask);
    MessageLoop::current()->PostDelayedTask(FROM_HERE, connectTask, aDelayMs);
  } else {
    ioLoop->PostTask(FROM_HERE, new SocketConnectTask(mImpl));
  }
  return true;
}

bool
UnixSocketConsumer::ListenSocket(UnixSocketConnector* aConnector)
{
  MOZ_ASSERT(aConnector);
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<UnixSocketConnector> connector(aConnector);

  if (mImpl) {
    NS_WARNING("Socket already connecting/connected!");
    return false;
  }

  mImpl = new UnixSocketImpl(XRE_GetIOMessageLoop(), this, connector.forget(),
                             EmptyCString());
  SetConnectionStatus(SOCKET_LISTENING);
  XRE_GetIOMessageLoop()->PostTask(FROM_HERE,
                                   new SocketListenTask(mImpl));
  return true;
}

} // namespace ipc
} // namespace mozilla
