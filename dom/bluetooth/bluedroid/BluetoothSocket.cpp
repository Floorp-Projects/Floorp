/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothSocket.h"

#include <hardware/bluetooth.h>
#include <hardware/bt_sock.h>
#include <sys/socket.h>

#include "base/message_loop.h"
#include "BluetoothSocketObserver.h"
#include "BluetoothUtils.h"
#include "mozilla/FileUtils.h"
#include "mozilla/RefPtr.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

#define FIRST_SOCKET_INFO_MSG_LENGTH 4
#define TOTAL_SOCKET_INFO_LENGTH 20

using namespace mozilla::ipc;
USING_BLUETOOTH_NAMESPACE

static const size_t MAX_READ_SIZE = 1 << 16;
static const uint8_t UUID_OBEX_OBJECT_PUSH[] = {
  0x00, 0x00, 0x11, 0x05, 0x00, 0x00, 0x10, 0x00,
  0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB
};
static const btsock_interface_t* sBluetoothSocketInterface = nullptr;

// helper functions
static bool
EnsureBluetoothSocketHalLoad()
{
  if (sBluetoothSocketInterface) {
    return true;
  }

  const bt_interface_t* btInf = GetBluetoothInterface();
  NS_ENSURE_TRUE(btInf, false);

  sBluetoothSocketInterface =
    (btsock_interface_t *) btInf->get_profile_interface(BT_PROFILE_SOCKETS_ID);
  NS_ENSURE_TRUE(sBluetoothSocketInterface, false);

  return true;
}

static int16_t
ReadInt16(const uint8_t* aData, size_t* aOffset)
{
  int16_t value = (aData[*aOffset + 1] << 8) | aData[*aOffset];

  *aOffset += 2;
  return value;
}

static int32_t
ReadInt32(const uint8_t* aData, size_t* aOffset)
{
  int32_t value = (aData[*aOffset + 3] << 24) |
                  (aData[*aOffset + 2] << 16) |
                  (aData[*aOffset + 1] << 8) |
                  aData[*aOffset];
  *aOffset += 4;
  return value;
}

static void
ReadBdAddress(const uint8_t* aData, size_t* aOffset, nsAString& aDeviceAddress)
{
  char bdstr[18];
  sprintf(bdstr, "%02x:%02x:%02x:%02x:%02x:%02x",
          aData[*aOffset], aData[*aOffset + 1], aData[*aOffset + 2],
          aData[*aOffset + 3], aData[*aOffset + 4], aData[*aOffset + 5]);

  aDeviceAddress.AssignLiteral(bdstr);
  *aOffset += 6;
}

class mozilla::dom::bluetooth::DroidSocketImpl
    : public MessageLoopForIO::Watcher
{
public:
  DroidSocketImpl(BluetoothSocket* aConsumer, int aFd)
    : mConsumer(aConsumer)
    , mReadMsgForClientFd(false)
    , mIOLoop(nullptr)
    , mFd(aFd)
    , mShuttingDownOnIOThread(false)
  {
  }

  ~DroidSocketImpl()
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  void QueueWriteData(UnixSocketRawData* aData)
  {
    mOutgoingQ.AppendElement(aData);
    OnFileCanWriteWithoutBlocking(mFd);
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

    mReadWatcher.StopWatchingFileDescriptor();
    mWriteWatcher.StopWatchingFileDescriptor();

    mShuttingDownOnIOThread = true;
  }

  void SetUpIO(bool aWrite)
  {
    MOZ_ASSERT(!mIOLoop);
    MOZ_ASSERT(mFd >= 0);
    mIOLoop = MessageLoopForIO::current();

    // Set up a read watch
    mIOLoop->WatchFileDescriptor(mFd,
                                 true,
                                 MessageLoopForIO::WATCH_READ,
                                 &mReadWatcher,
                                 this);

    if (aWrite) {
      // Set up a write watch
      mIOLoop->WatchFileDescriptor(mFd.get(),
                                   false,
                                   MessageLoopForIO::WATCH_WRITE,
                                   &mWriteWatcher,
                                   this);
    }
  }

  void ConnectClientFd()
  {
    // Stop current read watch
    mReadWatcher.StopWatchingFileDescriptor();
    mIOLoop = nullptr;

    // Restart read & write watch on client fd
    SetUpIO(true);
  }

  /**
   * Consumer pointer. Non-thread safe RefPtr, so should only be manipulated
   * directly from main thread. All non-main-thread accesses should happen with
   * mImpl as container.
   */
  RefPtr<BluetoothSocket> mConsumer;

  /**
   * If true, read message header to get client fd.
   */
  bool mReadMsgForClientFd;

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

  /**
   * Read message to get data and client fd wrapped in message header
   *
   * @param aFd     [in]  File descriptor to read message from
   * @param aBuffer [out] Data buffer read
   * @param aLength [out] Number of bytes read
   */
  ssize_t ReadMsg(int aFd, void *aBuffer, size_t aLength);

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
  mozilla::ScopedClose mFd;

  /**
   * If true, do not requeue whatever task we're running
   */
  bool mShuttingDownOnIOThread;
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

class SocketSetUpIOTask : public Task
{
  virtual void Run()
  {
    MOZ_ASSERT(!NS_IsMainThread());
    mImpl->SetUpIO(mWrite);
  }

  DroidSocketImpl* mImpl;
  bool mWrite;
public:
  SocketSetUpIOTask(DroidSocketImpl* aImpl, bool aWrite)
  : mImpl(aImpl), mWrite(aWrite) { }
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

ssize_t
DroidSocketImpl::ReadMsg(int aFd, void *aBuffer, size_t aLength)
{
  ssize_t ret;
  struct msghdr msg;
  struct iovec iv;
  struct cmsghdr cmsgbuf[2 * sizeof(cmsghdr) + 0x100];

  memset(&msg, 0, sizeof(msg));
  memset(&iv, 0, sizeof(iv));

  iv.iov_base = (unsigned char *)aBuffer;
  iv.iov_len = aLength;

  msg.msg_iov = &iv;
  msg.msg_iovlen = 1;
  msg.msg_control = cmsgbuf;
  msg.msg_controllen = sizeof(cmsgbuf);

  ret = recvmsg(mFd.get(), &msg, MSG_NOSIGNAL);
  if (ret < 0 && errno == EPIPE) {
    // Treat this as an end of stream
    return 0;
  }

  NS_ENSURE_FALSE(ret < 0, -1);
  NS_ENSURE_FALSE(msg.msg_flags & (MSG_CTRUNC | MSG_OOB | MSG_ERRQUEUE), -1);

  // Extract client fd from message header
  for (struct cmsghdr *cmsgptr = CMSG_FIRSTHDR(&msg);
       cmsgptr != nullptr; cmsgptr = CMSG_NXTHDR(&msg, cmsgptr)) {
    if (cmsgptr->cmsg_level != SOL_SOCKET) {
      continue;
    }
    if (cmsgptr->cmsg_type == SCM_RIGHTS) {
      int *pDescriptors = (int *)CMSG_DATA(cmsgptr);
      // Overwrite fd with client fd
      mFd.reset(pDescriptors[0]);
      break;
    }
  }

  return ret;
}

void
DroidSocketImpl::OnFileCanReadWithoutBlocking(int aFd)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!mShuttingDownOnIOThread);

  // Read all of the incoming data.
  while (true) {
    nsAutoPtr<UnixSocketRawData> incoming(new UnixSocketRawData(MAX_READ_SIZE));

    ssize_t ret;
    if (!mReadMsgForClientFd) {
      ret = read(aFd, incoming->mData, incoming->mSize);
    } else {
      ret = ReadMsg(aFd, incoming->mData, incoming->mSize);
    }

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
      mReadWatcher.StopWatchingFileDescriptor();
      mWriteWatcher.StopWatchingFileDescriptor();
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

void
DroidSocketImpl::OnFileCanWriteWithoutBlocking(int aFd)
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

BluetoothSocket::BluetoothSocket(BluetoothSocketObserver* aObserver,
                                 BluetoothSocketType aType,
                                 bool aAuth,
                                 bool aEncrypt)
  : mObserver(aObserver)
  , mImpl(nullptr)
  , mAuth(aAuth)
  , mEncrypt(aEncrypt)
  , mReceivedSocketInfoLength(0)
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

  OnDisconnect();
}

bool
BluetoothSocket::CreateDroidSocket(int aFd)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_FALSE(mImpl, false);

  mImpl = new DroidSocketImpl(this, aFd);
  XRE_GetIOMessageLoop()->PostTask(FROM_HERE,
                                   new SocketSetUpIOTask(mImpl, !mIsServer));

  return true;
}

bool
BluetoothSocket::Connect(const nsAString& aDeviceAddress, int aChannel)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!aDeviceAddress.IsEmpty());
  NS_ENSURE_TRUE(sBluetoothSocketInterface, false);

  bt_bdaddr_t remoteBdAddress;
  StringToBdAddressType(aDeviceAddress, &remoteBdAddress);

  // TODO: uuid as argument
  int fd;
  NS_ENSURE_TRUE(BT_STATUS_SUCCESS ==
    sBluetoothSocketInterface->connect(&remoteBdAddress,
                                       BTSOCK_RFCOMM,
                                       UUID_OBEX_OBJECT_PUSH,
                                       aChannel,
                                       &fd,
                                       (mAuth << 1) | mEncrypt),
    false);
  NS_ENSURE_TRUE(fd >= 0, false);

  mIsServer = false;
  return CreateDroidSocket(fd);
}

bool
BluetoothSocket::Listen(int aChannel)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_TRUE(sBluetoothSocketInterface, false);

  // TODO: uuid and service name as arguments
  nsAutoCString serviceName("OBEX Object Push");
  int fd;
  NS_ENSURE_TRUE(BT_STATUS_SUCCESS ==
    sBluetoothSocketInterface->listen(BTSOCK_RFCOMM,
                                      serviceName.get(),
                                      UUID_OBEX_OBJECT_PUSH,
                                      aChannel,
                                      &fd,
                                      (mAuth << 1) | mEncrypt),
    false);
  NS_ENSURE_TRUE(fd >= 0, false);

  mIsServer = true;
  return CreateDroidSocket(fd);
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

bool
BluetoothSocket::ReceiveSocketInfo(nsAutoPtr<UnixSocketRawData>& aMessage)
{
  MOZ_ASSERT(NS_IsMainThread());

  /**
   * 2 socket info messages (20 bytes) to receive at the beginning:
   * - 1st message: [channel:4]
   * - 2nd message: [size:2][bd address:6][channel:4][connection status:4]
   */
  if (mReceivedSocketInfoLength >= TOTAL_SOCKET_INFO_LENGTH) {
    // We've got both socket info messages
    return false;
  }
  mReceivedSocketInfoLength += aMessage->mSize;

  size_t offset = 0;
  if (mReceivedSocketInfoLength == FIRST_SOCKET_INFO_MSG_LENGTH) {
    // 1st message: [channel:4]
    int32_t channel = ReadInt32(aMessage->mData, &offset);
    BT_LOGR("channel %d", channel);

    // If this is server socket, read header of next message for client fd
    mImpl->mReadMsgForClientFd = mIsServer;
  } else if (mReceivedSocketInfoLength == TOTAL_SOCKET_INFO_LENGTH) {
    // 2nd message: [size:2][bd address:6][channel:4][connection status:4]
    int16_t size = ReadInt16(aMessage->mData, &offset);
    ReadBdAddress(aMessage->mData, &offset, mDeviceAddress);
    int32_t channel = ReadInt32(aMessage->mData, &offset);
    int32_t connectionStatus = ReadInt32(aMessage->mData, &offset);

    BT_LOGR("size %d channel %d remote addr %s status %d",
      size, channel, NS_ConvertUTF16toUTF8(mDeviceAddress).get(), connectionStatus);

    if (connectionStatus != 0) {
      OnConnectError();
      return true;
    }

    if (mIsServer) {
      mImpl->mReadMsgForClientFd = false;
      // Connect client fd on IO thread
      XRE_GetIOMessageLoop()->PostTask(FROM_HERE,
                                       new SocketConnectClientFdTask(mImpl));
    }
    OnConnectSuccess();
  }

  return true;
}

void
BluetoothSocket::ReceiveSocketData(nsAutoPtr<UnixSocketRawData>& aMessage)
{
  if (ReceiveSocketInfo(aMessage)) {
    return;
  }

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

