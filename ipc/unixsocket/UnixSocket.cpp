/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UnixSocket.h"
#include "nsTArray.h"
#include "nsXULAppAPI.h"
#include <fcntl.h>

static const size_t MAX_READ_SIZE = 1 << 16;

namespace mozilla {
namespace ipc {

class UnixSocketImpl : public UnixSocketWatcher
{
public:
  UnixSocketImpl(MessageLoop* mIOLoop,
                 UnixSocketConsumer* aConsumer, UnixSocketConnector* aConnector,
                 const nsACString& aAddress)
    : UnixSocketWatcher(mIOLoop)
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

  void QueueWriteData(UnixSocketRawData* aData)
  {
    mOutgoingQ.AppendElement(aData);
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

private:
  // Set up flags on whatever our current file descriptor is.
  static bool SetSocketFlags(int aFd);

  void FireSocketError();

  /**
   * Raw data queue. Must be pushed/popped from IO thread only.
   */
  typedef nsTArray<UnixSocketRawData* > UnixSocketRawDataQueue;
  UnixSocketRawDataQueue mOutgoingQ;

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

template<class T>
class DeleteInstanceRunnable : public nsRunnable
{
public:
  DeleteInstanceRunnable(T* aInstance)
  : mInstance(aInstance)
  { }

  NS_IMETHOD Run()
  {
    delete mInstance;

    return NS_OK;
  }

private:
  T* mInstance;
};

class UnixSocketImplRunnable : public nsRunnable
{
public:
  UnixSocketImpl* GetImpl() const
  {
    return mImpl;
  }
protected:
  UnixSocketImplRunnable(UnixSocketImpl* aImpl)
  : mImpl(aImpl)
  {
    MOZ_ASSERT(aImpl);
  }
  virtual ~UnixSocketImplRunnable()
  { }
private:
  UnixSocketImpl* mImpl;
};

class OnSocketEventRunnable : public UnixSocketImplRunnable
{
public:
  enum SocketEvent {
    CONNECT_SUCCESS,
    CONNECT_ERROR,
    DISCONNECT
  };

  OnSocketEventRunnable(UnixSocketImpl* aImpl, SocketEvent e)
  : UnixSocketImplRunnable(aImpl)
  , mEvent(e)
  {
    MOZ_ASSERT(!NS_IsMainThread());
  }

  NS_IMETHOD Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread());

    UnixSocketImpl* impl = GetImpl();

    if (impl->IsShutdownOnMainThread()) {
      NS_WARNING("CloseSocket has already been called!");
      // Since we've already explicitly closed and the close happened before
      // this, this isn't really an error. Since we've warned, return OK.
      return NS_OK;
    }
    if (mEvent == CONNECT_SUCCESS) {
      impl->mConsumer->NotifySuccess();
    } else if (mEvent == CONNECT_ERROR) {
      impl->mConsumer->NotifyError();
    } else if (mEvent == DISCONNECT) {
      impl->mConsumer->NotifyDisconnect();
    }
    return NS_OK;
  }
private:
  SocketEvent mEvent;
};

class SocketReceiveRunnable : public UnixSocketImplRunnable
{
public:
  SocketReceiveRunnable(UnixSocketImpl* aImpl, UnixSocketRawData* aData)
  : UnixSocketImplRunnable(aImpl)
  , mRawData(aData)
  {
    MOZ_ASSERT(aData);
  }

  NS_IMETHOD Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread());

    UnixSocketImpl* impl = GetImpl();

    if (impl->IsShutdownOnMainThread()) {
      NS_WARNING("mConsumer is null, aborting receive!");
      // Since we've already explicitly closed and the close happened before
      // this, this isn't really an error. Since we've warned, return OK.
      return NS_OK;
    }

    MOZ_ASSERT(impl->mConsumer);
    impl->mConsumer->ReceiveSocketData(mRawData);
    return NS_OK;
  }
private:
  nsAutoPtr<UnixSocketRawData> mRawData;
};

class RequestClosingSocketRunnable : public UnixSocketImplRunnable
{
public:
  RequestClosingSocketRunnable(UnixSocketImpl* aImpl)
  : UnixSocketImplRunnable(aImpl)
  { }

  NS_IMETHOD Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread());

    UnixSocketImpl* impl = GetImpl();
    if (impl->IsShutdownOnMainThread()) {
      NS_WARNING("CloseSocket has already been called!");
      // Since we've already explicitly closed and the close happened before
      // this, this isn't really an error. Since we've warned, return OK.
      return NS_OK;
    }

    // Start from here, same handling flow as calling CloseSocket() from
    // upper layer
    impl->mConsumer->CloseSocket();
    return NS_OK;
  }
};

class UnixSocketImplTask : public CancelableTask
{
public:
  UnixSocketImpl* GetImpl() const
  {
    return mImpl;
  }
  void Cancel() MOZ_OVERRIDE
  {
    mImpl = nullptr;
  }
  bool IsCanceled() const
  {
    return !mImpl;
  }
protected:
  UnixSocketImplTask(UnixSocketImpl* aImpl)
  : mImpl(aImpl)
  {
    MOZ_ASSERT(mImpl);
  }
private:
  UnixSocketImpl* mImpl;
};

class SocketSendTask : public UnixSocketImplTask
{
public:
  SocketSendTask(UnixSocketImpl* aImpl,
                 UnixSocketConsumer* aConsumer,
                 UnixSocketRawData* aData)
  : UnixSocketImplTask(aImpl)
  , mConsumer(aConsumer)
  , mData(aData)
  {
    MOZ_ASSERT(aConsumer);
    MOZ_ASSERT(aData);
  }
  void Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(!IsCanceled());

    UnixSocketImpl* impl = GetImpl();
    MOZ_ASSERT(!impl->IsShutdownOnIOThread());

    impl->QueueWriteData(mData);
  }
private:
  nsRefPtr<UnixSocketConsumer> mConsumer;
  UnixSocketRawData* mData;
};

class SocketListenTask : public UnixSocketImplTask
{
public:
  SocketListenTask(UnixSocketImpl* aImpl)
  : UnixSocketImplTask(aImpl)
  { }

  void Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(!NS_IsMainThread());
    if (!IsCanceled()) {
      GetImpl()->Listen();
    }
  }
};

class SocketConnectTask : public UnixSocketImplTask
{
public:
  SocketConnectTask(UnixSocketImpl* aImpl)
  : UnixSocketImplTask(aImpl)
  { }

  void Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(!IsCanceled());
    GetImpl()->Connect();
  }
};

class SocketDelayedConnectTask : public UnixSocketImplTask
{
public:
  SocketDelayedConnectTask(UnixSocketImpl* aImpl)
  : UnixSocketImplTask(aImpl)
  { }

  void Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread());
    if (IsCanceled()) {
      return;
    }
    UnixSocketImpl* impl = GetImpl();
    if (impl->IsShutdownOnMainThread()) {
      return;
    }
    impl->ClearDelayedConnectTask();
    XRE_GetIOMessageLoop()->PostTask(FROM_HERE, new SocketConnectTask(impl));
  }
};

class ShutdownSocketTask : public UnixSocketImplTask
{
public:
  ShutdownSocketTask(UnixSocketImpl* aImpl)
  : UnixSocketImplTask(aImpl)
  { }

  void Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(!IsCanceled());

    UnixSocketImpl* impl = GetImpl();

    // At this point, there should be no new events on the IO thread after this
    // one with the possible exception of a SocketListenTask that
    // ShutdownOnIOThread will cancel for us. We are now fully shut down, so we
    // can send a message to the main thread that will delete impl safely knowing
    // that no more tasks reference it.
    impl->ShutdownOnIOThread();

    nsRefPtr<nsIRunnable> r(new DeleteInstanceRunnable<UnixSocketImpl>(impl));
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
  nsRefPtr<OnSocketEventRunnable> r =
    new OnSocketEventRunnable(this, OnSocketEventRunnable::CONNECT_ERROR);
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

  nsRefPtr<OnSocketEventRunnable> r =
    new OnSocketEventRunnable(this, OnSocketEventRunnable::CONNECT_SUCCESS);
  NS_DispatchToMainThread(r);

  AddWatchers(READ_WATCHER, true);
  if (!mOutgoingQ.IsEmpty()) {
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

  nsRefPtr<OnSocketEventRunnable> r =
    new OnSocketEventRunnable(this, OnSocketEventRunnable::CONNECT_SUCCESS);
  NS_DispatchToMainThread(r);

  AddWatchers(READ_WATCHER, true);
  if (!mOutgoingQ.IsEmpty()) {
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

  // Read all of the incoming data.
  while (true) {
    nsAutoPtr<UnixSocketRawData> incoming(new UnixSocketRawData(MAX_READ_SIZE));

    ssize_t ret = read(GetFd(), incoming->mData, incoming->mSize);
    if (ret <= 0) {
      if (ret == -1) {
        if (errno == EINTR) {
          continue; // retry system call when interrupted
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          return; // no data available: return and re-poll
        }

#ifdef DEBUG
        NS_WARNING("Cannot read from network");
#endif
        // else fall through to error handling on other errno's
      }

      // We're done with our descriptors. Ensure that spurious events don't
      // cause us to end up back here.
      RemoveWatchers(READ_WATCHER|WRITE_WATCHER);
      nsRefPtr<RequestClosingSocketRunnable> r =
        new RequestClosingSocketRunnable(this);
      NS_DispatchToMainThread(r);
      return;
    }

    incoming->mSize = ret;
    nsRefPtr<SocketReceiveRunnable> r =
      new SocketReceiveRunnable(this, incoming.forget());
    NS_DispatchToMainThread(r);

    // If ret is less than MAX_READ_SIZE, there's no
    // more data in the socket for us to read now.
    if (ret < ssize_t(MAX_READ_SIZE)) {
      return;
    }
  }
}

void
UnixSocketImpl::OnSocketCanSendWithoutBlocking()
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());
  MOZ_ASSERT(GetConnectionStatus() == SOCKET_IS_CONNECTED); // see bug 990984

  // Try to write the bytes of mCurrentRilRawData.  If all were written, continue.
  //
  // Otherwise, save the byte position of the next byte to write
  // within mCurrentWriteOffset, and request another write when the
  // system won't block.
  //
  while (true) {
    UnixSocketRawData* data;
    if (mOutgoingQ.IsEmpty()) {
      return;
    }
    data = mOutgoingQ.ElementAt(0);
    const uint8_t *toWrite;
    toWrite = data->mData;

    while (data->mCurrentWriteOffset < data->mSize) {
      ssize_t write_amount = data->mSize - data->mCurrentWriteOffset;
      ssize_t written;
      written = write (GetFd(), toWrite + data->mCurrentWriteOffset,
                         write_amount);
      if (written > 0) {
        data->mCurrentWriteOffset += written;
      }
      if (written != write_amount) {
        break;
      }
    }

    if (data->mCurrentWriteOffset != data->mSize) {
      AddWatchers(WRITE_WATCHER, false);
      return;
    }
    mOutgoingQ.RemoveElementAt(0);
    delete data;
  }
}

UnixSocketConsumer::UnixSocketConsumer() : mImpl(nullptr)
                                         , mConnectionStatus(SOCKET_DISCONNECTED)
                                         , mConnectTimestamp(0)
                                         , mConnectDelayMs(0)
{
}

UnixSocketConsumer::~UnixSocketConsumer()
{
  MOZ_ASSERT(mConnectionStatus == SOCKET_DISCONNECTED);
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
  XRE_GetIOMessageLoop()->PostTask(FROM_HERE,
                                   new SocketSendTask(mImpl, this, aData));
  return true;
}

bool
UnixSocketConsumer::SendSocketData(const nsACString& aStr)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!mImpl) {
    return false;
  }
  if (aStr.Length() > MAX_READ_SIZE) {
    return false;
  }

  MOZ_ASSERT(!mImpl->IsShutdownOnMainThread());
  UnixSocketRawData* d = new UnixSocketRawData(aStr.BeginReading(),
                                               aStr.Length());
  XRE_GetIOMessageLoop()->PostTask(FROM_HERE,
                                   new SocketSendTask(mImpl, this, d));
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
  if (!mImpl || mConnectionStatus != SOCKET_CONNECTED) {
    NS_WARNING("No socket currently open!");
    return;
  }
  mImpl->GetSocketAddr(aAddrStr);
}

void
UnixSocketConsumer::NotifySuccess()
{
  MOZ_ASSERT(NS_IsMainThread());
  mConnectionStatus = SOCKET_CONNECTED;
  mConnectTimestamp = PR_IntervalNow();
  OnConnectSuccess();
}

void
UnixSocketConsumer::NotifyError()
{
  MOZ_ASSERT(NS_IsMainThread());
  mConnectionStatus = SOCKET_DISCONNECTED;
  mConnectDelayMs = CalculateConnectDelayMs();
  OnConnectError();
}

void
UnixSocketConsumer::NotifyDisconnect()
{
  MOZ_ASSERT(NS_IsMainThread());
  mConnectionStatus = SOCKET_DISCONNECTED;
  mConnectDelayMs = CalculateConnectDelayMs();
  OnDisconnect();
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
  mConnectionStatus = SOCKET_CONNECTING;
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
  mConnectionStatus = SOCKET_LISTENING;
  XRE_GetIOMessageLoop()->PostTask(FROM_HERE,
                                   new SocketListenTask(mImpl));
  return true;
}

uint32_t
UnixSocketConsumer::CalculateConnectDelayMs() const
{
  MOZ_ASSERT(NS_IsMainThread());

  uint32_t connectDelayMs = mConnectDelayMs;

  if ((PR_IntervalNow()-mConnectTimestamp) > connectDelayMs) {
    // reset delay if connection has been opened for a while, or...
    connectDelayMs = 0;
  } else if (!connectDelayMs) {
    // ...start with a delay of ~1 sec, or...
    connectDelayMs = 1<<10;
  } else if (connectDelayMs < (1<<16)) {
    // ...otherwise increase delay by a factor of 2
    connectDelayMs <<= 1;
  }
  return connectDelayMs;
}

} // namespace ipc
} // namespace mozilla
