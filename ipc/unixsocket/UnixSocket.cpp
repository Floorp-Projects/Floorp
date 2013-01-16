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
    , mFd(-1)
    , mConnector(aConnector)
    , mCurrentTaskIsCanceled(false)
    , mAddress(aAddress)
  {
  }

  ~UnixSocketImpl()
  {
    StopTask();
    mReadWatcher.StopWatchingFileDescriptor();
    mWriteWatcher.StopWatchingFileDescriptor();
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

  void CancelTask()
  {
    if (!mTask) {
      return;
    }
    mTask->Cancel();
    mTask = nullptr;
    mCurrentTaskIsCanceled = true;
  }
  
  void UnsetTask()
  {
    mTask = nullptr;
  }

  void EnqueueTask(int aDelayMs, CancelableTask* aTask)
  {
    MessageLoopForIO* ioLoop = MessageLoopForIO::current();
    if (!ioLoop) {
      NS_WARNING("No IOLoop to attach to, cancelling self!");
      return;
    }
    if (mTask) {
      return;
    }
    if (mCurrentTaskIsCanceled) {
      return;
    }
    mTask = aTask;
    if (aDelayMs) {
      ioLoop->PostDelayedTask(FROM_HERE, mTask, aDelayMs);
    } else {
      ioLoop->PostTask(FROM_HERE, mTask);
    }
  }
  
  void SetUpIO()
  {
    MOZ_ASSERT(!mIOLoop);
    mIOLoop = MessageLoopForIO::current();
    mIOLoop->WatchFileDescriptor(mFd,
                                 true,
                                 MessageLoopForIO::WATCH_READ,
                                 &mReadWatcher,
                                 this);
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
   * Stop whatever connect/accept task is running
   */
  void StopTask()
  {
    if (mTask) {
      mTask->Cancel();
      mTask = nullptr;
    }
    mCurrentTaskIsCanceled = true;
  }

  /** 
   * Set up nonblocking flags on whatever our current file descriptor is.
   *
   * @return true if successful, false otherwise
   */
  bool SetNonblockFlags();

  void GetSocketAddr(nsAString& aAddrStr)
  {
    if (!mConnector)
    {
      NS_WARNING("No connector to get socket address from!");
      aAddrStr = nsString();
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
   * Incoming packet. Only to be accessed on IO Thread.
   */
  nsAutoPtr<UnixSocketRawData> mIncoming;

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
  bool mCurrentTaskIsCanceled;

  /**
   * Pointer to the task we're currently running. DO NOT DELETE MANUALLY. This
   * will be taken care of by the IO loop. Just set to nullptr.
   */
  CancelableTask* mTask;

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
  sockaddr mAddr;

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
  }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());
    if (!mImpl->mConsumer) {
      NS_WARNING("CloseSocket has already been called! (mConsumer is null)");
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
    if(!mImpl->mConsumer) {
      NS_WARNING("mConsumer is null, aborting receive!");
      // Since we've already explicitly closed and the close happened before
      // this, this isn't really an error. Since we've warned, return OK.
      return NS_OK;
    }
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
    mImpl->QueueWriteData(mData);
  }

private:
  nsRefPtr<UnixSocketConsumer> mConsumer;
  UnixSocketImpl* mImpl;
  UnixSocketRawData* mData;
};

class SocketCloseTask : public nsRunnable
{
public:
  SocketCloseTask(UnixSocketImpl* aImpl)
    : mImpl(aImpl)
  {
    MOZ_ASSERT(aImpl);
  }

  NS_IMETHOD
  Run()
  {
    mImpl->mConsumer->CloseSocket();
    return NS_OK;
  }

private:
  UnixSocketImpl* mImpl;
};

class StartImplReadingTask : public Task
{
public:
  StartImplReadingTask(UnixSocketImpl* aImpl)
    : mImpl(aImpl)
  {
  }

  void
  Run()
  {
    mImpl->SetUpIO();
  }
private:
  UnixSocketImpl* mImpl;
};

class SocketAcceptTask : public CancelableTask {
  virtual void Run();

  bool mCanceled;
  UnixSocketImpl* mImpl;
public:
  virtual void Cancel() { mCanceled = true; }
  SocketAcceptTask(UnixSocketImpl* aImpl) : mCanceled(false), mImpl(aImpl) { }
};

void SocketAcceptTask::Run() {
  mImpl->UnsetTask();
  if (mCanceled) {
    return;
  }
  mImpl->Accept();
}

class SocketConnectTask : public CancelableTask {
  virtual void Run();

  bool mCanceled;
  UnixSocketImpl* mImpl;
public:
  SocketConnectTask(UnixSocketImpl* aImpl) : mCanceled(false), mImpl(aImpl) { }
  virtual void Cancel() { mCanceled = true; }  
};

void SocketConnectTask::Run() {
  mImpl->UnsetTask();
  if (mCanceled) {
    return;
  }
  mImpl->Connect();
}

void
UnixSocketImpl::Accept()
{

  if (!mConnector) {
    NS_WARNING("No connector object available!");
    return;
  }

  // This will set things we don't particularly care about, but it will hand
  // back the correct structure size which is what we do care about.
  mConnector->CreateAddr(true, mAddrSize, &mAddr, nullptr);

  if(mFd.get() < 0)
  {
    mFd = mConnector->Create();
    if (mFd.get() < 0) {
      return;
    }

    if (!SetNonblockFlags()) {
      return;
    }

    if (bind(mFd.get(), &mAddr, mAddrSize)) {
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

  int client_fd;
  client_fd = accept(mFd.get(), &mAddr, &mAddrSize);
  if (client_fd < 0) {
    EnqueueTask(SOCKET_RETRY_TIME_MS, new SocketAcceptTask(this));
    return;
  }

  if (!mConnector->SetUp(client_fd)) {
    NS_WARNING("Could not set up socket!");
    return;
  }
  mFd.reset(client_fd);

  nsRefPtr<OnSocketEventTask> t =
    new OnSocketEventTask(this, OnSocketEventTask::CONNECT_SUCCESS);
  NS_DispatchToMainThread(t);

  // Due to the fact that we've dispatched our OnConnectSuccess message before
  // starting reading, we're guaranteed that any subsequent read tasks will
  // happen after the object has been notified of a successful connect.
  XRE_GetIOMessageLoop()->PostTask(FROM_HERE,
                                   new StartImplReadingTask(this));
}

void
UnixSocketImpl::Connect()
{
  if(mFd.get() < 0)
  {
    mFd = mConnector->Create();
    if (mFd.get() < 0) {
      return;
    }
  }

  int ret;

  mConnector->CreateAddr(false, mAddrSize, &mAddr, mAddress.get());

  ret = connect(mFd.get(), &mAddr, mAddrSize);

  if (ret) {
#if DEBUG
    LOG("Socket connect errno=%d\n", errno);
#endif
    mFd.reset(-1);
    nsRefPtr<OnSocketEventTask> t =
      new OnSocketEventTask(this, OnSocketEventTask::CONNECT_ERROR);
    NS_DispatchToMainThread(t);
    return;
  }

  if (!mConnector->SetUp(mFd)) {
    NS_WARNING("Could not set up socket!");
    return;
  }

  nsRefPtr<OnSocketEventTask> t =
    new OnSocketEventTask(this, OnSocketEventTask::CONNECT_SUCCESS);
  NS_DispatchToMainThread(t);

  // Due to the fact that we've dispatched our OnConnectSuccess message before
  // starting reading, we're guaranteed that any subsequent read tasks will
  // happen after the object has been notified of a successful connect.
  XRE_GetIOMessageLoop()->PostTask(FROM_HERE,
                                   new StartImplReadingTask(this));
}

bool
UnixSocketImpl::SetNonblockFlags()
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

  // Select non-blocking IO.
  if (-1 == fcntl(mFd, F_SETFL, O_NONBLOCK)) {
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
  nsCString str(aStr);
  UnixSocketRawData* d = new UnixSocketRawData(aStr.Length());
  memcpy(d->mData, str.get(), aStr.Length());
  XRE_GetIOMessageLoop()->PostTask(FROM_HERE,
                                   new SocketSendTask(this, mImpl, d));
  return true;
}

void
UnixSocketConsumer::CloseSocket()
{
  // Needed due to refcount change
  MOZ_ASSERT(NS_IsMainThread());
  if (!mImpl) {
    return;
  }
  UnixSocketImpl* impl = mImpl;
  // To make sure the owner doesn't die on the IOThread, remove pointer here
  mImpl = nullptr;
  // Line it up to be destructed on the IO Thread
  impl->mConsumer.forget();
  impl->StopTask();

  // The receiver task should have been stopped at this point, but
  // SocketReceiverTask runnables might still be pending the main
  // thread. We enqueue the DeleteInstanceRunnable _after_ any pending
  // SocketReceiverTask. Otherwise we might free 'impl' before those
  // runnables have been executed.
  nsRefPtr<nsIRunnable> t(new DeleteInstanceRunnable<UnixSocketImpl>(impl));
  NS_ENSURE_TRUE_VOID(t);
  nsresult rv = NS_DispatchToMainThread(t);
  NS_ENSURE_SUCCESS_VOID(rv);
  t.forget();

  NotifyDisconnect();
}

void
UnixSocketImpl::OnFileCanReadWithoutBlocking(int aFd)
{
  // Keep reading data until either
  //
  //   - mIncoming is completely read
  //     If so, sConsumer->MessageReceived(mIncoming.forget())
  //
  //   - mIncoming isn't completely read, but there's no more
  //     data available on the socket
  //     If so, break;
  while (true) {
    if (!mIncoming) {
      uint8_t data[MAX_READ_SIZE];
      ssize_t ret = read(aFd, data, MAX_READ_SIZE);
      if (ret <= 0) {
        if (ret == -1) {
          if (errno == EINTR) {
            continue; // retry system call when interrupted
          }
          else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return; // no data available: return and re-poll
          }
          // else fall through to error handling on other errno's
        }
#ifdef DEBUG
        NS_WARNING("Cannot read from network");
#endif
        // At this point, assume that we can't actually access
        // the socket anymore
        mReadWatcher.StopWatchingFileDescriptor();
        mWriteWatcher.StopWatchingFileDescriptor();
        nsRefPtr<SocketCloseTask> t = new SocketCloseTask(this);
        NS_DispatchToMainThread(t);
        return;
      }
      mIncoming = new UnixSocketRawData(ret);
      memcpy(mIncoming->mData, data, ret);
      nsRefPtr<SocketReceiveTask> t =
        new SocketReceiveTask(this, mIncoming.forget());
      NS_DispatchToMainThread(t);
      if (ret < ssize_t(MAX_READ_SIZE)) {
        return;
      }
    }
  }
}

void
UnixSocketImpl::OnFileCanWriteWithoutBlocking(int aFd)
{
  // Try to write the bytes of mCurrentRilRawData.  If all were written, continue.
  //
  // Otherwise, save the byte position of the next byte to write
  // within mCurrentRilRawData, and request another write when the
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
}

void
UnixSocketConsumer::GetSocketAddr(nsAString& aAddrStr)
{
  if (!mImpl || mConnectionStatus != SOCKET_CONNECTED) {
    NS_WARNING("No socket currently open!");
    aAddrStr = nsString();
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
                                  const char* aAddress)
{
  MOZ_ASSERT(aConnector);
  MOZ_ASSERT(NS_IsMainThread());
  if (mImpl) {
    NS_WARNING("Socket already connecting/connected!");
    return false;
  }
  nsCString addr;
  addr.Assign(aAddress);
  mImpl = new UnixSocketImpl(this, aConnector, addr);
  XRE_GetIOMessageLoop()->PostTask(FROM_HERE,
                                   new SocketConnectTask(mImpl));
  mConnectionStatus = SOCKET_CONNECTING;
  return true;
}

bool
UnixSocketConsumer::ListenSocket(UnixSocketConnector* aConnector)
{
  MOZ_ASSERT(aConnector);
  MOZ_ASSERT(NS_IsMainThread());
  if (mImpl) {
    NS_WARNING("Socket already connecting/connected!");
    return false;
  }
  nsCString addr;
  mImpl = new UnixSocketImpl(this, aConnector, addr);
  XRE_GetIOMessageLoop()->PostTask(FROM_HERE,
                                   new SocketAcceptTask(mImpl));
  mConnectionStatus = SOCKET_LISTENING;
  return true;
}

void
UnixSocketConsumer::CancelSocketTask()
{
  mConnectionStatus = SOCKET_DISCONNECTED;
  if(!mImpl) {
    NS_WARNING("No socket implementation to cancel task on!");
    return;
  }
  mImpl->CancelTask();
}

} // namespace ipc
} // namespace mozilla
