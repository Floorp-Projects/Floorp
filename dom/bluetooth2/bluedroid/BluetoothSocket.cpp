/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothSocket.h"

#include <fcntl.h>
#include <sys/socket.h>

#include "base/message_loop.h"
#include "BluetoothInterface.h"
#include "BluetoothSocketObserver.h"
#include "BluetoothUtils.h"
#include "mozilla/FileUtils.h"
#include "mozilla/RefPtr.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

using namespace mozilla::ipc;
USING_BLUETOOTH_NAMESPACE

static const size_t MAX_READ_SIZE = 1 << 16;
static const uint8_t UUID_OBEX_OBJECT_PUSH[] = {
  0x00, 0x00, 0x11, 0x05, 0x00, 0x00, 0x10, 0x00,
  0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB
};
static BluetoothSocketInterface* sBluetoothSocketInterface;

// helper functions
static bool
EnsureBluetoothSocketHalLoad()
{
  if (sBluetoothSocketInterface) {
    return true;
  }

  BluetoothInterface* btInf = BluetoothInterface::GetInstance();
  NS_ENSURE_TRUE(btInf, false);

  sBluetoothSocketInterface = btInf->GetBluetoothSocketInterface();
  NS_ENSURE_TRUE(sBluetoothSocketInterface, false);

  return true;
}

class mozilla::dom::bluetooth::DroidSocketImpl : public ipc::UnixFdWatcher
{
public:
  /* The connection status in DroidSocketImpl indicates the current
   * phase of the socket connection. The initial settign should always
   * be DISCONNECTED, when no connection is present.
   *
   * To establish a connection on the server, DroidSocketImpl moves
   * to LISTENING. It now waits for incoming connection attempts by
   * installing a read watcher on the I/O thread. When its socket file
   * descriptor becomes readable, DroidSocketImpl accepts the connection
   * and finally moves DroidSocketImpl to CONNECTED. DroidSocketImpl now
   * uses read and write watchers during data transfers. Any socket setup
   * is handled internally by the accept method.
   *
   * On the client side, DroidSocketImpl moves to CONNECTING and installs
   * a write watcher on the I/O thread to wait until the connection is
   * ready. The socket setup is handled internally by the connect method.
   * Installing the write handler makes the code compatible with POSIX
   * semantics for non-blocking connects and gives a clear signal when the
   * conncetion is ready. DroidSocketImpl then moves to CONNECTED and uses
   * read and write watchers during data transfers.
   */
  enum ConnectionStatus {
    SOCKET_IS_DISCONNECTED = 0,
    SOCKET_IS_LISTENING,
    SOCKET_IS_CONNECTING,
    SOCKET_IS_CONNECTED
  };

  DroidSocketImpl(MessageLoop* aIOLoop, BluetoothSocket* aConsumer, int aFd)
    : ipc::UnixFdWatcher(aIOLoop, aFd)
    , mConsumer(aConsumer)
    , mShuttingDownOnIOThread(false)
    , mChannel(0)
    , mAuth(false)
    , mEncrypt(false)
    , mConnectionStatus(SOCKET_IS_DISCONNECTED)
  { }

  DroidSocketImpl(MessageLoop* aIOLoop, BluetoothSocket* aConsumer,
                  int aChannel, bool aAuth, bool aEncrypt)
    : ipc::UnixFdWatcher(aIOLoop)
    , mConsumer(aConsumer)
    , mShuttingDownOnIOThread(false)
    , mChannel(aChannel)
    , mAuth(aAuth)
    , mEncrypt(aEncrypt)
    , mConnectionStatus(SOCKET_IS_DISCONNECTED)
  { }

  DroidSocketImpl(MessageLoop* aIOLoop, BluetoothSocket* aConsumer,
                  const nsAString& aDeviceAddress,
                  int aChannel, bool aAuth, bool aEncrypt)
    : ipc::UnixFdWatcher(aIOLoop)
    , mConsumer(aConsumer)
    , mShuttingDownOnIOThread(false)
    , mDeviceAddress(aDeviceAddress)
    , mChannel(aChannel)
    , mAuth(aAuth)
    , mEncrypt(aEncrypt)
    , mConnectionStatus(SOCKET_IS_DISCONNECTED)
  {
    MOZ_ASSERT(!mDeviceAddress.IsEmpty());
  }

  ~DroidSocketImpl()
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  void QueueWriteData(UnixSocketRawData* aData)
  {
    mOutgoingQ.AppendElement(aData);
    OnFileCanWriteWithoutBlocking(GetFd());
  }

  bool IsShutdownOnMainThread()
  {
    MOZ_ASSERT(NS_IsMainThread());
    return mConsumer == nullptr;
  }

  bool IsShutdownOnIOThread()
  {
    return mShuttingDownOnIOThread;
  }

  void ShutdownOnMainThread()
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(!IsShutdownOnMainThread());
    mConsumer = nullptr;
  }

  void ShutdownOnIOThread()
  {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(!mShuttingDownOnIOThread);

    Close(); // will also remove fd from I/O loop
    mShuttingDownOnIOThread = true;
  }

  void Connect(int aFd);
  void Listen(int aFd);
  void Accept(int aFd);

  void ConnectClientFd()
  {
    // Stop current read watch
    RemoveWatchers(READ_WATCHER);

    mConnectionStatus = SOCKET_IS_CONNECTED;

    // Restart read & write watch on client fd
    AddWatchers(READ_WATCHER, true);
    AddWatchers(WRITE_WATCHER, false);
  }

  /**
   * Consumer pointer. Non-thread safe RefPtr, so should only be manipulated
   * directly from main thread. All non-main-thread accesses should happen with
   * mImpl as container.
   */
  RefPtr<BluetoothSocket> mConsumer;

private:
  /**
   * libevent triggered functions that reads data from socket when available and
   * guarenteed non-blocking. Only to be called on IO thread.
   *
   * @param aFd [in] File descriptor to read from
   */
  virtual void OnFileCanReadWithoutBlocking(int aFd);

  /**
   * libevent or developer triggered functions that writes data to socket when
   * available and guarenteed non-blocking. Only to be called on IO thread.
   *
   * @param aFd [in] File descriptor to read from
   */
  virtual void OnFileCanWriteWithoutBlocking(int aFd);

  void OnSocketCanReceiveWithoutBlocking(int aFd);
  void OnSocketCanAcceptWithoutBlocking(int aFd);
  void OnSocketCanSendWithoutBlocking(int aFd);
  void OnSocketCanConnectWithoutBlocking(int aFd);

  /**
   * Raw data queue. Must be pushed/popped from IO thread only.
   */
  typedef nsTArray<UnixSocketRawData* > UnixSocketRawDataQueue;
  UnixSocketRawDataQueue mOutgoingQ;

  /**
   * If true, do not requeue whatever task we're running
   */
  bool mShuttingDownOnIOThread;

  nsString mDeviceAddress;
  int mChannel;
  bool mAuth;
  bool mEncrypt;
  ConnectionStatus mConnectionStatus;
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

class DroidSocketImplRunnable : public nsRunnable
{
public:
  DroidSocketImpl* GetImpl() const
  {
    return mImpl;
  }

protected:
  DroidSocketImplRunnable(DroidSocketImpl* aImpl)
  : mImpl(aImpl)
  {
    MOZ_ASSERT(aImpl);
  }

  virtual ~DroidSocketImplRunnable()
  { }

private:
  DroidSocketImpl* mImpl;
};

class OnSocketEventRunnable : public DroidSocketImplRunnable
{
public:
  enum SocketEvent {
    CONNECT_SUCCESS,
    CONNECT_ERROR,
    DISCONNECT
  };

  OnSocketEventRunnable(DroidSocketImpl* aImpl, SocketEvent e)
  : DroidSocketImplRunnable(aImpl)
  , mEvent(e)
  {
    MOZ_ASSERT(!NS_IsMainThread());
  }

  NS_IMETHOD Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread());

    DroidSocketImpl* impl = GetImpl();

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

class RequestClosingSocketTask : public nsRunnable
{
public:
  RequestClosingSocketTask(DroidSocketImpl* aImpl) : mImpl(aImpl)
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
    mImpl->mConsumer->CloseDroidSocket();
    return NS_OK;
  }
private:
  DroidSocketImpl* mImpl;
};

class ShutdownSocketTask : public Task {
  virtual void Run()
  {
    MOZ_ASSERT(!NS_IsMainThread());

    // At this point, there should be no new events on the IO thread after this
    // one with the possible exception of a SocketAcceptTask that
    // ShutdownOnIOThread will cancel for us. We are now fully shut down, so we
    // can send a message to the main thread that will delete mImpl safely knowing
    // that no more tasks reference it.
    mImpl->ShutdownOnIOThread();

    nsRefPtr<nsIRunnable> t(new DeleteInstanceRunnable<
                                  mozilla::dom::bluetooth::DroidSocketImpl>(mImpl));
    nsresult rv = NS_DispatchToMainThread(t);
    NS_ENSURE_SUCCESS_VOID(rv);
  }

  DroidSocketImpl* mImpl;

public:
  ShutdownSocketTask(DroidSocketImpl* aImpl) : mImpl(aImpl) { }
};

class SocketReceiveTask : public nsRunnable
{
public:
  SocketReceiveTask(DroidSocketImpl* aImpl, UnixSocketRawData* aData) :
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
  DroidSocketImpl* mImpl;
  nsAutoPtr<UnixSocketRawData> mRawData;
};

class SocketSendTask : public Task
{
public:
  SocketSendTask(BluetoothSocket* aConsumer, DroidSocketImpl* aImpl,
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
  nsRefPtr<BluetoothSocket> mConsumer;
  DroidSocketImpl* mImpl;
  UnixSocketRawData* mData;
};

class DroidSocketImplTask : public CancelableTask
{
public:
  DroidSocketImpl* GetDroidSocketImpl() const
  {
    return mDroidSocketImpl;
  }
  void Cancel() MOZ_OVERRIDE
  {
    mDroidSocketImpl = nullptr;
  }
  bool IsCanceled() const
  {
    return !mDroidSocketImpl;
  }
protected:
  DroidSocketImplTask(DroidSocketImpl* aDroidSocketImpl)
  : mDroidSocketImpl(aDroidSocketImpl)
  {
    MOZ_ASSERT(mDroidSocketImpl);
  }
private:
  DroidSocketImpl* mDroidSocketImpl;
};

class SocketConnectTask : public DroidSocketImplTask
{
public:
  SocketConnectTask(DroidSocketImpl* aDroidSocketImpl, int aFd)
  : DroidSocketImplTask(aDroidSocketImpl)
  , mFd(aFd)
  { }

  void Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(!IsCanceled());
    GetDroidSocketImpl()->Connect(mFd);
  }

private:
  int mFd;
};

class SocketListenTask : public DroidSocketImplTask
{
public:
  SocketListenTask(DroidSocketImpl* aDroidSocketImpl, int aFd)
  : DroidSocketImplTask(aDroidSocketImpl)
  , mFd(aFd)
  { }

  void Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(!NS_IsMainThread());
    if (!IsCanceled()) {
      GetDroidSocketImpl()->Listen(mFd);
    }
  }

private:
  int mFd;
};

class SocketConnectClientFdTask : public Task
{
  virtual void Run()
  {
    MOZ_ASSERT(!NS_IsMainThread());
    mImpl->ConnectClientFd();
  }

  DroidSocketImpl* mImpl;
public:
  SocketConnectClientFdTask(DroidSocketImpl* aImpl) : mImpl(aImpl) { }
};

void
DroidSocketImpl::Connect(int aFd)
{
  MOZ_ASSERT(aFd >= 0);

  int flags = TEMP_FAILURE_RETRY(fcntl(aFd, F_GETFL));
  NS_ENSURE_TRUE_VOID(flags >= 0);

  if (!(flags & O_NONBLOCK)) {
    int res = TEMP_FAILURE_RETRY(fcntl(aFd, F_SETFL, flags | O_NONBLOCK));
    NS_ENSURE_TRUE_VOID(!res);
  }

  SetFd(aFd);
  mConnectionStatus = SOCKET_IS_CONNECTING;

  AddWatchers(WRITE_WATCHER, false);
}

void
DroidSocketImpl::Listen(int aFd)
{
  MOZ_ASSERT(aFd >= 0);

  int flags = TEMP_FAILURE_RETRY(fcntl(aFd, F_GETFL));
  NS_ENSURE_TRUE_VOID(flags >= 0);

  if (!(flags & O_NONBLOCK)) {
    int res = TEMP_FAILURE_RETRY(fcntl(aFd, F_SETFL, flags | O_NONBLOCK));
    NS_ENSURE_TRUE_VOID(!res);
  }

  SetFd(aFd);
  mConnectionStatus = SOCKET_IS_LISTENING;

  AddWatchers(READ_WATCHER, true);
}

void
DroidSocketImpl::Accept(int aFd)
{
  Close();

  int flags = TEMP_FAILURE_RETRY(fcntl(aFd, F_GETFL));
  NS_ENSURE_TRUE_VOID(flags >= 0);

  if (!(flags & O_NONBLOCK)) {
    int res = TEMP_FAILURE_RETRY(fcntl(aFd, F_SETFL, flags | O_NONBLOCK));
    NS_ENSURE_TRUE_VOID(!res);
  }

  SetFd(aFd);
  mConnectionStatus = SOCKET_IS_CONNECTED;

  nsRefPtr<OnSocketEventRunnable> r =
    new OnSocketEventRunnable(this, OnSocketEventRunnable::CONNECT_SUCCESS);
  NS_DispatchToMainThread(r);

  AddWatchers(READ_WATCHER, true);
  if (!mOutgoingQ.IsEmpty()) {
    AddWatchers(WRITE_WATCHER, false);
  }
}

void
DroidSocketImpl::OnFileCanReadWithoutBlocking(int aFd)
{
  if (mConnectionStatus == SOCKET_IS_CONNECTED) {
    OnSocketCanReceiveWithoutBlocking(aFd);
  } else if (mConnectionStatus == SOCKET_IS_LISTENING) {
    OnSocketCanAcceptWithoutBlocking(aFd);
  } else {
    NS_NOTREACHED("invalid connection state for reading");
  }
}

void
DroidSocketImpl::OnSocketCanReceiveWithoutBlocking(int aFd)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!mShuttingDownOnIOThread);

  // Read all of the incoming data.
  while (true) {
    nsAutoPtr<UnixSocketRawData> incoming(new UnixSocketRawData(MAX_READ_SIZE));

    ssize_t ret = read(aFd, incoming->mData, incoming->mSize);

    if (ret <= 0) {
      if (ret == -1) {
        if (errno == EINTR) {
          continue; // retry system call when interrupted
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          return; // no data available: return and re-poll
        }

        BT_WARNING("Cannot read from network");
        // else fall through to error handling on other errno's
      }

      // We're done with our descriptors. Ensure that spurious events don't
      // cause us to end up back here.
      RemoveWatchers(READ_WATCHER | WRITE_WATCHER);
      nsRefPtr<RequestClosingSocketTask> t = new RequestClosingSocketTask(this);
      NS_DispatchToMainThread(t);
      return;
    }

    incoming->mSize = ret;
    nsRefPtr<SocketReceiveTask> t =
      new SocketReceiveTask(this, incoming.forget());
    NS_DispatchToMainThread(t);

    // If ret is less than MAX_READ_SIZE, there's no
    // more data in the socket for us to read now.
    if (ret < ssize_t(MAX_READ_SIZE)) {
      return;
    }
  }

  MOZ_CRASH("We returned early");
}

class AcceptTask MOZ_FINAL : public DroidSocketImplTask
{
public:
  AcceptTask(DroidSocketImpl* aDroidSocketImpl, int aFd)
  : DroidSocketImplTask(aDroidSocketImpl)
  , mFd(aFd)
  { }

  void Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(!IsCanceled());

    GetDroidSocketImpl()->Accept(mFd);
  }

private:
  int mFd;
};

class AcceptResultHandler MOZ_FINAL : public BluetoothSocketResultHandler
{
public:
  AcceptResultHandler(DroidSocketImpl* aImpl)
  : mImpl(aImpl)
  {
    MOZ_ASSERT(mImpl);
  }

  void Accept(int aFd, const nsAString& aBdAddress,
              int aConnectionStatus) MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (mImpl->IsShutdownOnMainThread()) {
      BT_LOGD("mConsumer is null, aborting receive!");
      return;
    }

    mImpl->mConsumer->SetAddress(aBdAddress);
    XRE_GetIOMessageLoop()->PostTask(FROM_HERE, new AcceptTask(mImpl, aFd));
  }

  void OnError(bt_status_t aStatus) MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread());
    BT_LOGR("BluetoothSocketInterface::Accept failed: %d", (int)aStatus);
  }

private:
  DroidSocketImpl* mImpl;
};

class AcceptRunnable MOZ_FINAL : public nsRunnable
{
public:
  AcceptRunnable(int aFd, DroidSocketImpl* aImpl)
  : mFd(aFd)
  , mImpl(aImpl)
  {
    MOZ_ASSERT(mImpl);
  }

  NS_IMETHOD Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(sBluetoothSocketInterface);

    sBluetoothSocketInterface->Accept(mFd, new AcceptResultHandler(mImpl));

    return NS_OK;
  }

private:
  int mFd;
  DroidSocketImpl* mImpl;
};

void
DroidSocketImpl::OnSocketCanAcceptWithoutBlocking(int aFd)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!mShuttingDownOnIOThread);

  /* When a listening socket is ready for receiving data,
   * we can call |Accept| on it.
   */

  RemoveWatchers(READ_WATCHER);
  nsRefPtr<AcceptRunnable> t = new AcceptRunnable(aFd, this);
  NS_DispatchToMainThread(t);
}

void
DroidSocketImpl::OnFileCanWriteWithoutBlocking(int aFd)
{
  if (mConnectionStatus == SOCKET_IS_CONNECTED) {
    OnSocketCanSendWithoutBlocking(aFd);
  } else if (mConnectionStatus == SOCKET_IS_CONNECTING) {
    OnSocketCanConnectWithoutBlocking(aFd);
  } else {
    NS_NOTREACHED("invalid connection state for writing");
  }
}

void
DroidSocketImpl::OnSocketCanSendWithoutBlocking(int aFd)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!mShuttingDownOnIOThread);
  MOZ_ASSERT(aFd >= 0);

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
      AddWatchers(WRITE_WATCHER, false);
    }
    mOutgoingQ.RemoveElementAt(0);
    delete data;
  }
}

void
DroidSocketImpl::OnSocketCanConnectWithoutBlocking(int aFd)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!mShuttingDownOnIOThread);

  /* We follow Posix behaviour here: Connect operations are
   * complete once we can write to the connecting socket.
   */

  mConnectionStatus = SOCKET_IS_CONNECTED;

  nsRefPtr<OnSocketEventRunnable> r =
    new OnSocketEventRunnable(this, OnSocketEventRunnable::CONNECT_SUCCESS);
  NS_DispatchToMainThread(r);

  AddWatchers(READ_WATCHER, true);
  if (!mOutgoingQ.IsEmpty()) {
    AddWatchers(WRITE_WATCHER, false);
  }
}

BluetoothSocket::BluetoothSocket(BluetoothSocketObserver* aObserver,
                                 BluetoothSocketType aType,
                                 bool aAuth,
                                 bool aEncrypt)
  : mObserver(aObserver)
  , mImpl(nullptr)
  , mAuth(aAuth)
  , mEncrypt(aEncrypt)
{
  MOZ_ASSERT(aObserver);

  EnsureBluetoothSocketHalLoad();
  mDeviceAddress.AssignLiteral(BLUETOOTH_ADDRESS_NONE);
}

void
BluetoothSocket::CloseDroidSocket()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!mImpl) {
    return;
  }

  // From this point on, we consider mImpl as being deleted.
  // We sever the relationship here so any future calls to listen or connect
  // will create a new implementation.
  mImpl->ShutdownOnMainThread();
  XRE_GetIOMessageLoop()->PostTask(FROM_HERE,
                                   new ShutdownSocketTask(mImpl));
  mImpl = nullptr;

  NotifyDisconnect();
}

class ConnectSocketResultHandler MOZ_FINAL : public BluetoothSocketResultHandler
{
public:
  ConnectSocketResultHandler(DroidSocketImpl* aImpl)
  : mImpl(aImpl)
  {
    MOZ_ASSERT(mImpl);
  }

  void Connect(int aFd, const nsAString& aBdAddress,
               int aConnectionStatus) MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (!mImpl->IsShutdownOnMainThread()) {
      mImpl->mConsumer->SetAddress(aBdAddress);
    }
    XRE_GetIOMessageLoop()->PostTask(FROM_HERE,
                                     new SocketConnectTask(mImpl, aFd));
  }

  void OnError(bt_status_t aStatus) MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread());
    BT_WARNING("Connect failed: %d", (int)aStatus);
  }

private:
  DroidSocketImpl* mImpl;
};

bool
BluetoothSocket::Connect(const nsAString& aDeviceAddress, int aChannel)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_FALSE(mImpl, false);

  mImpl = new DroidSocketImpl(XRE_GetIOMessageLoop(), this, aDeviceAddress,
                              aChannel, mAuth, mEncrypt);

  bt_bdaddr_t remoteBdAddress;
  StringToBdAddressType(aDeviceAddress, &remoteBdAddress);

  // TODO: uuid as argument
  sBluetoothSocketInterface->Connect(&remoteBdAddress,
                                     BTSOCK_RFCOMM,
                                     UUID_OBEX_OBJECT_PUSH,
                                     aChannel,
                                     (BTSOCK_FLAG_ENCRYPT * mEncrypt) |
                                     (BTSOCK_FLAG_AUTH * mAuth),
                                     new ConnectSocketResultHandler(mImpl));
  return true;
}

class ListenResultHandler MOZ_FINAL : public BluetoothSocketResultHandler
{
public:
  ListenResultHandler(DroidSocketImpl* aImpl)
  : mImpl(aImpl)
  {
    MOZ_ASSERT(mImpl);
  }

  void Listen(int aFd) MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread());

    XRE_GetIOMessageLoop()->PostTask(FROM_HERE,
                                     new SocketListenTask(mImpl, aFd));
  }

  void OnError(bt_status_t aStatus) MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread());

    BT_WARNING("Listen failed: %d", (int)aStatus);
  }

private:
  DroidSocketImpl* mImpl;
};

bool
BluetoothSocket::Listen(int aChannel)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_FALSE(mImpl, false);

  mImpl = new DroidSocketImpl(XRE_GetIOMessageLoop(), this, aChannel, mAuth,
                              mEncrypt);

  sBluetoothSocketInterface->Listen(BTSOCK_RFCOMM,
                                    "OBEX Object Push",
                                    UUID_OBEX_OBJECT_PUSH,
                                    aChannel,
                                    (BTSOCK_FLAG_ENCRYPT * mEncrypt) |
                                    (BTSOCK_FLAG_AUTH * mAuth),
                                    new ListenResultHandler(mImpl));
  return true;
}

bool
BluetoothSocket::SendDroidSocketData(UnixSocketRawData* aData)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_TRUE(mImpl, false);

  MOZ_ASSERT(!mImpl->IsShutdownOnMainThread());
  XRE_GetIOMessageLoop()->PostTask(FROM_HERE,
                                   new SocketSendTask(this, mImpl, aData));
  return true;
}

void
BluetoothSocket::ReceiveSocketData(nsAutoPtr<UnixSocketRawData>& aMessage)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mObserver);
  mObserver->ReceiveSocketData(this, aMessage);
}

void
BluetoothSocket::OnConnectSuccess()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mObserver);
  mObserver->OnSocketConnectSuccess(this);
}

void
BluetoothSocket::OnConnectError()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mObserver);
  mObserver->OnSocketConnectError(this);
}

void
BluetoothSocket::OnDisconnect()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mObserver);
  mObserver->OnSocketDisconnect(this);
}
