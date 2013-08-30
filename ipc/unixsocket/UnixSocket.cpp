/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UnixSocket.h"

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/socket.h>

#include "base/eintr_wrapper.h"
#include "base/message_loop.h"

#include "mozilla/Monitor.h"
#include "mozilla/Util.h"
#include "mozilla/FileUtils.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "nsTArray.h"
#include "nsXULAppAPI.h"

static const size_t MAX_READ_SIZE = 1 << 16;

#undef LOG
#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "GonkDBus", args);
#else
#define BTDEBUG true
#define LOG(args...) if (BTDEBUG) printf(args);
#endif

static const int SOCKET_RETRY_TIME_MS = 1000;

namespace mozilla {
namespace ipc {

class UnixSocketImpl : public MessageLoopForIO::Watcher
{
public:
  UnixSocketImpl(UnixSocketConsumer* aConsumer, UnixSocketConnector* aConnector,
                 const nsACString& aAddress)
    : mConsumer(aConsumer)
    , mIOLoop(nullptr)
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
    OnFileCanWriteWithoutBlocking(mFd);
  }

  bool isFdValid()
  {
    return mFd > 0;
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

    mReadWatcher.StopWatchingFileDescriptor();
    mWriteWatcher.StopWatchingFileDescriptor();

    mShuttingDownOnIOThread = true;
  }

  void SetUpIO()
  {
    MOZ_ASSERT(!mIOLoop);
    MOZ_ASSERT(mFd >= 0);
    mIOLoop = MessageLoopForIO::current();
    mIOLoop->WatchFileDescriptor(mFd,
                                 true,
                                 MessageLoopForIO::WATCH_READ,
                                 &mReadWatcher,
                                 this);
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

  /** 
   * Accept an incoming connection
   */
  void Accept();

  /**
   * Set up flags on whatever our current file descriptor is.
   *
   * @return true if successful, false otherwise
   */
  bool SetSocketFlags();

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

private:
  /**
   * libevent triggered functions that reads data from socket when available and
   * guarenteed non-blocking. Only to be called on IO thread.
   *
   * @param aFd File descriptor to read from
   */
  virtual void OnFileCanReadWithoutBlocking(int aFd);

  /**
   * libevent or developer triggered functions that writes data to socket when
   * available and guarenteed non-blocking. Only to be called on IO thread.
   *
   * @param aFd File descriptor to read from
   */
  virtual void OnFileCanWriteWithoutBlocking(int aFd);

  /**
   * IO Loop pointer. Must be initalized and called from IO thread only.
   */
  MessageLoopForIO* mIOLoop;

  /**
   * Raw data queue. Must be pushed/popped from IO thread only.
   */
  typedef nsTArray<UnixSocketRawData* > UnixSocketRawDataQueue;
  UnixSocketRawDataQueue mOutgoingQ;

  /**
   * Read watcher for libevent. Only to be accessed on IO Thread.
   */
  MessageLoopForIO::FileDescriptorWatcher mReadWatcher;

  /**
   * Write watcher for libevent. Only to be accessed on IO Thread.
   */
  MessageLoopForIO::FileDescriptorWatcher mWriteWatcher;

  /**
   * File descriptor to read from/write to. Connection happens on user provided
   * thread. Read/write/close happens on IO thread.
   */
  ScopedClose mFd;

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

class OnSocketEventTask : public nsRunnable
{
public:
  enum SocketEvent {
    CONNECT_SUCCESS,
    CONNECT_ERROR,
    DISCONNECT
  };

  OnSocketEventTask(UnixSocketImpl* aImpl, SocketEvent e) :
    mImpl(aImpl),
    mEvent(e)
  {
    MOZ_ASSERT(aImpl);
    MOZ_ASSERT(!NS_IsMainThread());
  }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());
    if (mImpl->IsShutdownOnMainThread()) {
      NS_WARNING("CloseSocket has already been called!");
      // Since we've already explicitly closed and the close happened before
      // this, this isn't really an error. Since we've warned, return OK.
      return NS_OK;
    }
    if (mEvent == CONNECT_SUCCESS) {
      mImpl->mConsumer->NotifySuccess();
    } else if (mEvent == CONNECT_ERROR) {
      mImpl->mConsumer->NotifyError();
    } else if (mEvent == DISCONNECT) {
      mImpl->mConsumer->NotifyDisconnect();
    }
    return NS_OK;
  }
private:
  UnixSocketImpl* mImpl;
  SocketEvent mEvent;
};

class SocketReceiveTask : public nsRunnable
{
public:
  SocketReceiveTask(UnixSocketImpl* aImpl, UnixSocketRawData* aData) :
    mImpl(aImpl),
    mRawData(aData)
  {
    MOZ_ASSERT(aImpl);
    MOZ_ASSERT(aData);
  }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());
    if (mImpl->IsShutdownOnMainThread()) {
      NS_WARNING("mConsumer is null, aborting receive!");
      // Since we've already explicitly closed and the close happened before
      // this, this isn't really an error. Since we've warned, return OK.
      return NS_OK;
    }

    MOZ_ASSERT(mImpl->mConsumer);
    mImpl->mConsumer->ReceiveSocketData(mRawData);
    return NS_OK;
  }
private:
  UnixSocketImpl* mImpl;
  nsAutoPtr<UnixSocketRawData> mRawData;
};

class SocketSendTask : public Task
{
public:
  SocketSendTask(UnixSocketConsumer* aConsumer, UnixSocketImpl* aImpl,
                 UnixSocketRawData* aData)
    : mConsumer(aConsumer),
      mImpl(aImpl),
      mData(aData)
  {
    MOZ_ASSERT(aConsumer);
    MOZ_ASSERT(aImpl);
    MOZ_ASSERT(aData);
  }

  void
  Run()
  {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(!mImpl->IsShutdownOnIOThread());

    mImpl->QueueWriteData(mData);
  }

private:
  nsRefPtr<UnixSocketConsumer> mConsumer;
  UnixSocketImpl* mImpl;
  UnixSocketRawData* mData;
};

class RequestClosingSocketTask : public nsRunnable
{
public:
  RequestClosingSocketTask(UnixSocketImpl* aImpl) : mImpl(aImpl)
  {
    MOZ_ASSERT(aImpl);
  }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (mImpl->IsShutdownOnMainThread()) {
      NS_WARNING("CloseSocket has already been called!");
      // Since we've already explicitly closed and the close happened before
      // this, this isn't really an error. Since we've warned, return OK.
      return NS_OK;
    }

    // Start from here, same handling flow as calling CloseSocket() from
    // upper layer
    mImpl->mConsumer->CloseSocket();
    return NS_OK;
  }
private:
  UnixSocketImpl* mImpl;
};

class SocketAcceptTask : public CancelableTask {
  virtual void Run();

  UnixSocketImpl* mImpl;
public:
  SocketAcceptTask(UnixSocketImpl* aImpl) : mImpl(aImpl) { }

  virtual void Cancel()
  {
    MOZ_ASSERT(!NS_IsMainThread());
    mImpl = nullptr;
  }
};

void SocketAcceptTask::Run()
{
  MOZ_ASSERT(!NS_IsMainThread());

  if (mImpl) {
    mImpl->Accept();
  }
}

class SocketConnectTask : public Task {
  virtual void Run();

  UnixSocketImpl* mImpl;
public:
  SocketConnectTask(UnixSocketImpl* aImpl) : mImpl(aImpl) { }
};

void SocketConnectTask::Run()
{
  MOZ_ASSERT(!NS_IsMainThread());
  mImpl->Connect();
}

class SocketDelayedConnectTask : public CancelableTask {
  virtual void Run();

  UnixSocketImpl* mImpl;
public:
  SocketDelayedConnectTask(UnixSocketImpl* aImpl) : mImpl(aImpl) { }

  virtual void Cancel()
  {
    MOZ_ASSERT(NS_IsMainThread());
    mImpl = nullptr;
  }
};

void SocketDelayedConnectTask::Run()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!mImpl || mImpl->IsShutdownOnMainThread()) {
    return;
  }
  mImpl->ClearDelayedConnectTask();
  XRE_GetIOMessageLoop()->PostTask(FROM_HERE, new SocketConnectTask(mImpl));
}

class ShutdownSocketTask : public Task {
  virtual void Run();

  UnixSocketImpl* mImpl;

public:
  ShutdownSocketTask(UnixSocketImpl* aImpl) : mImpl(aImpl) { }
};

void ShutdownSocketTask::Run()
{
  MOZ_ASSERT(!NS_IsMainThread());

  // At this point, there should be no new events on the IO thread after this
  // one with the possible exception of a SocketAcceptTask that
  // ShutdownOnIOThread will cancel for us. We are now fully shut down, so we
  // can send a message to the main thread that will delete mImpl safely knowing
  // that no more tasks reference it.
  mImpl->ShutdownOnIOThread();

  nsRefPtr<nsIRunnable> t(new DeleteInstanceRunnable<UnixSocketImpl>(mImpl));
  nsresult rv = NS_DispatchToMainThread(t);
  NS_ENSURE_SUCCESS_VOID(rv);
}

void  
UnixSocketImpl::Accept()
{
  MOZ_ASSERT(!NS_IsMainThread());

  if (!mConnector) {
    NS_WARNING("No connector object available!");
    return;
  }

  // This will set things we don't particularly care about, but it will hand
  // back the correct structure size which is what we do care about.
  if (!mConnector->CreateAddr(true, mAddrSize, mAddr, nullptr)) {
    NS_WARNING("Cannot create socket address!");
    return;
  }

  if (mFd.get() < 0) {
    mFd = mConnector->Create();
    if (mFd.get() < 0) {
      return;
    }

    if (!SetSocketFlags()) {
      return;
    }

    if (bind(mFd.get(), (struct sockaddr*)&mAddr, mAddrSize)) {
#ifdef DEBUG
      LOG("...bind(%d) gave errno %d", mFd.get(), errno);
#endif
      return;
    }

    if (listen(mFd.get(), 1)) {
#ifdef DEBUG
      LOG("...listen(%d) gave errno %d", mFd.get(), errno);
#endif
      return;
    }

  }

  SetUpIO();
}

void
UnixSocketImpl::Connect()
{
  MOZ_ASSERT(!NS_IsMainThread());

  if (!mConnector) {
    NS_WARNING("No connector object available!");
    return;
  }

  if (mFd.get() < 0) {
    mFd = mConnector->Create();
    if (mFd.get() < 0) {
      return;
    }
  }

  int ret;

  if (!mConnector->CreateAddr(false, mAddrSize, mAddr, mAddress.get())) {
    NS_WARNING("Cannot create socket address!");
    return;
  }

  // Select non-blocking IO.
  if (-1 == fcntl(mFd.get(), F_SETFL, O_NONBLOCK)) {
    nsRefPtr<OnSocketEventTask> t =
      new OnSocketEventTask(this, OnSocketEventTask::CONNECT_ERROR);
    NS_DispatchToMainThread(t);
    return;
  }

  ret = connect(mFd.get(), (struct sockaddr*)&mAddr, mAddrSize);

  if (ret) {
    if (errno == EINPROGRESS) {
      // Select blocking IO again, since we've now at least queue'd the connect
      // as nonblock.
      int current_opts = fcntl(mFd.get(), F_GETFL, 0);
      if (-1 == current_opts) {
        NS_WARNING("Cannot get socket opts!");
        mFd.reset(-1);
        nsRefPtr<OnSocketEventTask> t =
          new OnSocketEventTask(this, OnSocketEventTask::CONNECT_ERROR);
        NS_DispatchToMainThread(t);
        return;
      }
      if (-1 == fcntl(mFd.get(), F_SETFL, current_opts & ~O_NONBLOCK)) {
        NS_WARNING("Cannot set socket opts to blocking!");
        mFd.reset(-1);
        nsRefPtr<OnSocketEventTask> t =
          new OnSocketEventTask(this, OnSocketEventTask::CONNECT_ERROR);
        NS_DispatchToMainThread(t);
        return;
      }

      // Set up a write watch to make sure we receive the connect signal
      MessageLoopForIO::current()->WatchFileDescriptor(
        mFd.get(),
        false,
        MessageLoopForIO::WATCH_WRITE,
        &mWriteWatcher,
        this);

#ifdef DEBUG
      LOG("UnixSocket Connection delayed!");
#endif
      return;
    }
#if DEBUG
    LOG("Socket connect errno=%d\n", errno);
#endif
    mFd.reset(-1);
    nsRefPtr<OnSocketEventTask> t =
      new OnSocketEventTask(this, OnSocketEventTask::CONNECT_ERROR);
    NS_DispatchToMainThread(t);
    return;
  }

  if (!SetSocketFlags()) {
    return;
  }

  if (!mConnector->SetUp(mFd)) {
    NS_WARNING("Could not set up socket!");
    return;
  }

  nsRefPtr<OnSocketEventTask> t =
    new OnSocketEventTask(this, OnSocketEventTask::CONNECT_SUCCESS);
  NS_DispatchToMainThread(t);

  SetUpIO();
}

bool
UnixSocketImpl::SetSocketFlags()
{
  // Set socket addr to be reused even if kernel is still waiting to close
  int n = 1;
  setsockopt(mFd, SOL_SOCKET, SO_REUSEADDR, &n, sizeof(n));

  // Set close-on-exec bit.
  int flags = fcntl(mFd, F_GETFD);
  if (-1 == flags) {
    return false;
  }

  flags |= FD_CLOEXEC;
  if (-1 == fcntl(mFd, F_SETFD, flags)) {
    return false;
  }

  return true;
}

UnixSocketConsumer::UnixSocketConsumer() : mImpl(nullptr)
                                         , mConnectionStatus(SOCKET_DISCONNECTED)
{
}

UnixSocketConsumer::~UnixSocketConsumer()
{
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
                                   new SocketSendTask(this, mImpl, aData));
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
  UnixSocketRawData* d = new UnixSocketRawData(aStr.Length());
  memcpy(d->mData, aStr.BeginReading(), aStr.Length());
  XRE_GetIOMessageLoop()->PostTask(FROM_HERE,
                                   new SocketSendTask(this, mImpl, d));
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
UnixSocketImpl::OnFileCanReadWithoutBlocking(int aFd)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!mShuttingDownOnIOThread);

  SocketConnectionStatus status = mConsumer->GetConnectionStatus();
  if (status == SOCKET_CONNECTED) {
    // Read all of the incoming data.
    while (true) {
      uint8_t data[MAX_READ_SIZE];
      ssize_t ret = read(aFd, data, MAX_READ_SIZE);
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
        mReadWatcher.StopWatchingFileDescriptor();
        mWriteWatcher.StopWatchingFileDescriptor();
        nsRefPtr<RequestClosingSocketTask> t = new RequestClosingSocketTask(this);
        NS_DispatchToMainThread(t);
        return;
      }

      UnixSocketRawData* incoming = new UnixSocketRawData(ret);
      memcpy(incoming->mData, data, ret);
      nsRefPtr<SocketReceiveTask> t = new SocketReceiveTask(this, incoming);
      NS_DispatchToMainThread(t);

      // If ret is less than MAX_READ_SIZE, there's no more data in the socket
      // for us to read now.
      if (ret < ssize_t(MAX_READ_SIZE)) {
        return;
      }
    }

    MOZ_CRASH("We returned early");
  }

  if (status == SOCKET_LISTENING) {
    int client_fd = accept(mFd.get(), (struct sockaddr*)&mAddr, &mAddrSize);

    if (client_fd < 0) {
      return;
    }

    if (!mConnector->SetUp(client_fd)) {
      NS_WARNING("Could not set up socket!");
      return;
    }

    mReadWatcher.StopWatchingFileDescriptor();
    mWriteWatcher.StopWatchingFileDescriptor();

    mFd.reset(client_fd);
    if (!SetSocketFlags()) {
      return;
    }

    mIOLoop = nullptr;

    nsRefPtr<OnSocketEventTask> t =
      new OnSocketEventTask(this, OnSocketEventTask::CONNECT_SUCCESS);
    NS_DispatchToMainThread(t);

    SetUpIO();
  }
}

void
UnixSocketImpl::OnFileCanWriteWithoutBlocking(int aFd)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!mShuttingDownOnIOThread);

  MOZ_ASSERT(aFd >= 0);
  SocketConnectionStatus status = mConsumer->GetConnectionStatus();
  if (status == SOCKET_CONNECTED) {
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
        written = write (aFd, toWrite + data->mCurrentWriteOffset,
                         write_amount);
        if (written > 0) {
          data->mCurrentWriteOffset += written;
        }
        if (written != write_amount) {
          break;
        }
      }

      if (data->mCurrentWriteOffset != data->mSize) {
        MessageLoopForIO::current()->WatchFileDescriptor(
          aFd,
          false,
          MessageLoopForIO::WATCH_WRITE,
          &mWriteWatcher,
          this);
        return;
      }
      mOutgoingQ.RemoveElementAt(0);
      delete data;
    }
  } else if (status == SOCKET_CONNECTING) {
    int error, ret;
    socklen_t len = sizeof(error);
    ret = getsockopt(mFd.get(), SOL_SOCKET, SO_ERROR, &error, &len);

    if (ret || error) {
      NS_WARNING("getsockopt failure on async socket connect!");
      mFd.reset(-1);
      nsRefPtr<OnSocketEventTask> t =
        new OnSocketEventTask(this, OnSocketEventTask::CONNECT_ERROR);
      NS_DispatchToMainThread(t);
      return;
    }

    if (!SetSocketFlags()) {
      mFd.reset(-1);
      nsRefPtr<OnSocketEventTask> t =
        new OnSocketEventTask(this, OnSocketEventTask::CONNECT_ERROR);
      NS_DispatchToMainThread(t);
      return;
    }

    if (!mConnector->SetUp(mFd)) {
      NS_WARNING("Could not set up socket!");
      mFd.reset(-1);
      nsRefPtr<OnSocketEventTask> t =
        new OnSocketEventTask(this, OnSocketEventTask::CONNECT_ERROR);
      NS_DispatchToMainThread(t);
      return;
    }

    nsRefPtr<OnSocketEventTask> t =
      new OnSocketEventTask(this, OnSocketEventTask::CONNECT_SUCCESS);
    NS_DispatchToMainThread(t);

    SetUpIO();
  }
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
  OnConnectSuccess();
}

void
UnixSocketConsumer::NotifyError()
{
  MOZ_ASSERT(NS_IsMainThread());
  mConnectionStatus = SOCKET_DISCONNECTED;
  OnConnectError();
}

void
UnixSocketConsumer::NotifyDisconnect()
{
  MOZ_ASSERT(NS_IsMainThread());
  mConnectionStatus = SOCKET_DISCONNECTED;
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
  mImpl = new UnixSocketImpl(this, connector.forget(), addr);
  MessageLoop* ioLoop = XRE_GetIOMessageLoop();
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

  mImpl = new UnixSocketImpl(this, connector.forget(), EmptyCString());
  mConnectionStatus = SOCKET_LISTENING;
  XRE_GetIOMessageLoop()->PostTask(FROM_HERE,
                                   new SocketAcceptTask(mImpl));
  return true;
}

} // namespace ipc
} // namespace mozilla
