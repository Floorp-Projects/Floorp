/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include "base/message_loop.h"
#include "BluetoothInterface.h"
#include "nsAutoPtr.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

BEGIN_BLUETOOTH_NAMESPACE

template<class T>
struct interface_traits
{ };

//
// Result handling
//

template <typename Obj, typename Res>
class BluetoothInterfaceRunnable0 : public nsRunnable
{
public:
  BluetoothInterfaceRunnable0(Obj* aObj, Res (Obj::*aMethod)())
  : mObj(aObj)
  , mMethod(aMethod)
  {
    MOZ_ASSERT(mObj);
    MOZ_ASSERT(mMethod);
  }

  NS_METHOD
  Run() MOZ_OVERRIDE
  {
    ((*mObj).*mMethod)();
    return NS_OK;
  }

private:
  nsRefPtr<Obj> mObj;
  void (Obj::*mMethod)();
};

template <typename Obj, typename Res, typename Arg1>
class BluetoothInterfaceRunnable1 : public nsRunnable
{
public:
  BluetoothInterfaceRunnable1(Obj* aObj, Res (Obj::*aMethod)(Arg1),
                              const Arg1& aArg1)
  : mObj(aObj)
  , mMethod(aMethod)
  , mArg1(aArg1)
  {
    MOZ_ASSERT(mObj);
    MOZ_ASSERT(mMethod);
  }

  NS_METHOD
  Run() MOZ_OVERRIDE
  {
    ((*mObj).*mMethod)(mArg1);
    return NS_OK;
  }

private:
  nsRefPtr<Obj> mObj;
  void (Obj::*mMethod)(Arg1);
  Arg1 mArg1;
};

template <typename Obj, typename Res,
          typename Arg1, typename Arg2, typename Arg3>
class BluetoothInterfaceRunnable3 : public nsRunnable
{
public:
  BluetoothInterfaceRunnable3(Obj* aObj,
                              Res (Obj::*aMethod)(Arg1, Arg2, Arg3),
                              const Arg1& aArg1, const Arg2& aArg2,
                              const Arg3& aArg3)
  : mObj(aObj)
  , mMethod(aMethod)
  , mArg1(aArg1)
  , mArg2(aArg2)
  , mArg3(aArg3)
  {
    MOZ_ASSERT(mObj);
    MOZ_ASSERT(mMethod);
  }

  NS_METHOD
  Run() MOZ_OVERRIDE
  {
    ((*mObj).*mMethod)(mArg1, mArg2, mArg3);
    return NS_OK;
  }

private:
  nsRefPtr<Obj> mObj;
  void (Obj::*mMethod)(Arg1, Arg2, Arg3);
  Arg1 mArg1;
  Arg2 mArg2;
  Arg3 mArg3;
};

//
// Socket Interface
//

template<>
struct interface_traits<BluetoothSocketInterface>
{
  typedef const btsock_interface_t const_interface_type;

  static const char* profile_id()
  {
    return BT_PROFILE_SOCKETS_ID;
  }
};

typedef
  BluetoothInterfaceRunnable1<BluetoothSocketResultHandler, void, int>
  BluetoothSocketIntResultRunnable;

typedef
  BluetoothInterfaceRunnable3<BluetoothSocketResultHandler,
                              void, int, const nsAString_internal&, int>
  BluetoothSocketIntStringIntResultRunnable;

typedef
  BluetoothInterfaceRunnable1<BluetoothSocketResultHandler, void, bt_status_t>
  BluetoothSocketErrorRunnable;

static nsresult
DispatchBluetoothSocketResult(BluetoothSocketResultHandler* aRes,
                              void (BluetoothSocketResultHandler::*aMethod)(int),
                              int aArg, bt_status_t aStatus)
{
  MOZ_ASSERT(aRes);

  nsRunnable* runnable;

  if (aStatus == BT_STATUS_SUCCESS) {
    runnable = new BluetoothSocketIntResultRunnable(aRes, aMethod, aArg);
  } else {
    runnable = new BluetoothSocketErrorRunnable(aRes,
      &BluetoothSocketResultHandler::OnError, aStatus);
  }
  nsresult rv = NS_DispatchToMainThread(runnable);
  if (NS_FAILED(rv)) {
    BT_WARNING("NS_DispatchToMainThread failed: %X", rv);
  }
  return rv;
}

static nsresult
DispatchBluetoothSocketResult(
  BluetoothSocketResultHandler* aRes,
  void (BluetoothSocketResultHandler::*aMethod)(int, const nsAString&, int),
  int aArg1, const nsAString& aArg2, int aArg3, bt_status_t aStatus)
{
  MOZ_ASSERT(aRes);

  nsRunnable* runnable;

  if (aStatus == BT_STATUS_SUCCESS) {
    runnable = new BluetoothSocketIntStringIntResultRunnable(aRes, aMethod,
                                                             aArg1, aArg2,
                                                             aArg3);
  } else {
    runnable = new BluetoothSocketErrorRunnable(aRes,
      &BluetoothSocketResultHandler::OnError, aStatus);
  }
  nsresult rv = NS_DispatchToMainThread(runnable);
  if (NS_FAILED(rv)) {
    BT_WARNING("NS_DispatchToMainThread failed: %X", rv);
  }
  return rv;
}

void
BluetoothSocketInterface::Listen(btsock_type_t aType,
                                 const char* aServiceName,
                                 const uint8_t* aServiceUuid,
                                 int aChannel, int aFlags,
                                 BluetoothSocketResultHandler* aRes)
{
  int fd;

  bt_status_t status = mInterface->listen(aType, aServiceName, aServiceUuid,
                                          aChannel, &fd, aFlags);
  if (aRes) {
    DispatchBluetoothSocketResult(aRes, &BluetoothSocketResultHandler::Listen,
                                  fd, status);
  }
}

#define CMSGHDR_CONTAINS_FD(_cmsghdr) \
    ( ((_cmsghdr)->cmsg_level == SOL_SOCKET) && \
      ((_cmsghdr)->cmsg_type == SCM_RIGHTS) )

/* |SocketMessageWatcher| receives Bluedroid's socket setup
 * messages on the I/O thread. You need to inherit from this
 * class to make use of it.
 *
 * Bluedroid sends two socket info messages (20 bytes) at
 * the beginning of a connection to both peers.
 *
 *   - 1st message: [channel:4]
 *   - 2nd message: [size:2][bd address:6][channel:4][connection status:4]
 *
 * On the server side, the second message will contain a
 * socket file descriptor for the connection. The client
 * uses the original file descriptor.
 */
class SocketMessageWatcher : public MessageLoopForIO::Watcher
{
public:
  static const unsigned char MSG1_SIZE = 4;
  static const unsigned char MSG2_SIZE = 16;

  static const unsigned char OFF_CHANNEL1 = 0;
  static const unsigned char OFF_SIZE = 4;
  static const unsigned char OFF_BDADDRESS = 6;
  static const unsigned char OFF_CHANNEL2 = 12;
  static const unsigned char OFF_STATUS = 16;

  SocketMessageWatcher(int aFd)
  : mFd(aFd)
  , mClientFd(-1)
  , mLen(0)
  { }

  virtual ~SocketMessageWatcher()
  { }

  virtual void Proceed(bt_status_t aStatus) = 0;

  void OnFileCanReadWithoutBlocking(int aFd) MOZ_OVERRIDE
  {
    bt_status_t status;

    switch (mLen) {
      case 0:
        status = RecvMsg1();
        break;
      case MSG1_SIZE:
        status = RecvMsg2();
        break;
      default:
        /* message-size error */
        status = BT_STATUS_FAIL;
        break;
    }

    if (IsComplete() || status != BT_STATUS_SUCCESS) {
      mWatcher.StopWatchingFileDescriptor();
      Proceed(status);
    }
  }

  void OnFileCanWriteWithoutBlocking(int aFd) MOZ_OVERRIDE
  { }

  void Watch()
  {
    MessageLoopForIO::current()->WatchFileDescriptor(
      mFd,
      true,
      MessageLoopForIO::WATCH_READ,
      &mWatcher,
      this);
  }

  bool IsComplete() const
  {
    return mLen == (MSG1_SIZE + MSG2_SIZE);
  }

  int GetFd() const
  {
    return mFd;
  }

  int32_t GetChannel1() const
  {
    return ReadInt32(OFF_CHANNEL1);
  }

  int32_t GetSize() const
  {
    return ReadInt16(OFF_SIZE);
  }

  nsString GetBdAddress() const
  {
    nsString bdAddress;
    ReadBdAddress(OFF_BDADDRESS, bdAddress);
    return bdAddress;
  }

  int32_t GetChannel2() const
  {
    return ReadInt32(OFF_CHANNEL2);
  }

  int32_t GetConnectionStatus() const
  {
    return ReadInt32(OFF_STATUS);
  }

  int GetClientFd() const
  {
    return mClientFd;
  }

private:
  bt_status_t RecvMsg1()
  {
    struct iovec iv;
    memset(&iv, 0, sizeof(iv));
    iv.iov_base = mBuf;
    iv.iov_len = MSG1_SIZE;

    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = &iv;
    msg.msg_iovlen = 1;

    ssize_t res = TEMP_FAILURE_RETRY(recvmsg(mFd, &msg, MSG_NOSIGNAL));
    if (res < 0) {
      return BT_STATUS_FAIL;
    }

    mLen += res;

    return BT_STATUS_SUCCESS;
  }

  bt_status_t RecvMsg2()
  {
    struct iovec iv;
    memset(&iv, 0, sizeof(iv));
    iv.iov_base = mBuf + MSG1_SIZE;
    iv.iov_len = MSG2_SIZE;

    struct msghdr msg;
    struct cmsghdr cmsgbuf[2 * sizeof(cmsghdr) + 0x100];
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = &iv;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsgbuf;
    msg.msg_controllen = sizeof(cmsgbuf);

    ssize_t res = TEMP_FAILURE_RETRY(recvmsg(mFd, &msg, MSG_NOSIGNAL));
    if (res < 0) {
      return BT_STATUS_FAIL;
    }

    mLen += res;

    if (msg.msg_flags & (MSG_CTRUNC | MSG_OOB | MSG_ERRQUEUE)) {
      return BT_STATUS_FAIL;
    }

    struct cmsghdr *cmsgptr = CMSG_FIRSTHDR(&msg);

    // Extract client fd from message header
    for (; cmsgptr; cmsgptr = CMSG_NXTHDR(&msg, cmsgptr)) {
      if (CMSGHDR_CONTAINS_FD(cmsgptr)) {
        // if multiple file descriptors have been sent, we close
        // all but the final one.
        if (mClientFd != -1) {
          TEMP_FAILURE_RETRY(close(mClientFd));
        }
        // retrieve sent client fd
        mClientFd = *(static_cast<int*>(CMSG_DATA(cmsgptr)));
      }
    }

    return BT_STATUS_SUCCESS;
  }

  int16_t ReadInt16(unsigned long aOffset) const
  {
    /* little-endian buffer */
    return (static_cast<int16_t>(mBuf[aOffset + 1]) << 8) |
            static_cast<int16_t>(mBuf[aOffset]);
  }

  int32_t ReadInt32(unsigned long aOffset) const
  {
    /* little-endian buffer */
    return (static_cast<int32_t>(mBuf[aOffset + 3]) << 24) |
           (static_cast<int32_t>(mBuf[aOffset + 2]) << 16) |
           (static_cast<int32_t>(mBuf[aOffset + 1]) << 8) |
            static_cast<int32_t>(mBuf[aOffset]);
  }

  void ReadBdAddress(unsigned long aOffset, nsAString& aBdAddress) const
  {
    char str[18];
    sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x",
            mBuf[aOffset + 0], mBuf[aOffset + 1], mBuf[aOffset + 2],
            mBuf[aOffset + 3], mBuf[aOffset + 4], mBuf[aOffset + 5]);
    aBdAddress.AssignLiteral(str);
  }

  MessageLoopForIO::FileDescriptorWatcher mWatcher;
  int mFd;
  int mClientFd;
  unsigned char mLen;
  uint8_t mBuf[MSG1_SIZE + MSG2_SIZE];
};

/* |SocketMessageWatcherTask| starts a SocketMessageWatcher
 * on the I/O task
 */
class SocketMessageWatcherTask MOZ_FINAL : public Task
{
public:
  SocketMessageWatcherTask(SocketMessageWatcher* aWatcher)
  : mWatcher(aWatcher)
  {
    MOZ_ASSERT(mWatcher);
  }

  void Run() MOZ_OVERRIDE
  {
    mWatcher->Watch();
  }

private:
  SocketMessageWatcher* mWatcher;
};

/* |DeleteTask| deletes a class instance on the I/O thread
 */
template <typename T>
class DeleteTask MOZ_FINAL : public Task
{
public:
  DeleteTask(T* aPtr)
  : mPtr(aPtr)
  { }

  void Run() MOZ_OVERRIDE
  {
    mPtr = nullptr;
  }

private:
  nsAutoPtr<T> mPtr;
};

/* |ConnectWatcher| specializes SocketMessageWatcher for
 * connect operations by reading the socket messages from
 * Bluedroid and forwarding the connected socket to the
 * resource handler.
 */
class ConnectWatcher MOZ_FINAL : public SocketMessageWatcher
{
public:
  ConnectWatcher(int aFd, BluetoothSocketResultHandler* aRes)
  : SocketMessageWatcher(aFd)
  , mRes(aRes)
  { }

  void Proceed(bt_status_t aStatus) MOZ_OVERRIDE
  {
    if (mRes) {
      DispatchBluetoothSocketResult(mRes,
                                   &BluetoothSocketResultHandler::Connect,
                                    GetFd(), GetBdAddress(),
                                    GetConnectionStatus(), aStatus);
    }
    MessageLoopForIO::current()->PostTask(
      FROM_HERE, new DeleteTask<ConnectWatcher>(this));
  }

private:
  nsRefPtr<BluetoothSocketResultHandler> mRes;
};

void
BluetoothSocketInterface::Connect(const bt_bdaddr_t* aBdAddr,
                                  btsock_type_t aType, const uint8_t* aUuid,
                                  int aChannel, int aFlags,
                                  BluetoothSocketResultHandler* aRes)
{
  int fd;

  bt_status_t status = mInterface->connect(aBdAddr, aType, aUuid, aChannel,
                                           &fd, aFlags);
  if (status == BT_STATUS_SUCCESS) {
    /* receive Bluedroid's socket-setup messages */
    Task* t = new SocketMessageWatcherTask(new ConnectWatcher(fd, aRes));
    XRE_GetIOMessageLoop()->PostTask(FROM_HERE, t);
  } else if (aRes) {
      DispatchBluetoothSocketResult(aRes,
                                    &BluetoothSocketResultHandler::Connect,
                                    -1, EmptyString(), 0, status);
  }
}

/* Specializes SocketMessageWatcher for Accept operations by
 * reading the socket messages from Bluedroid and forwarding
 * the received client socket to the resource handler. The
 * first message is received immediately. When there's a new
 * connection, Bluedroid sends the 2nd message with the socket
 * info and socket file descriptor.
 */
class AcceptWatcher MOZ_FINAL : public SocketMessageWatcher
{
public:
  AcceptWatcher(int aFd, BluetoothSocketResultHandler* aRes)
  : SocketMessageWatcher(aFd)
  , mRes(aRes)
  {
    /* not supplying a result handler leaks received file descriptor */
    MOZ_ASSERT(mRes);
  }

  void Proceed(bt_status_t aStatus) MOZ_OVERRIDE
  {
    if (mRes) {
      DispatchBluetoothSocketResult(mRes,
                                    &BluetoothSocketResultHandler::Accept,
                                    GetClientFd(), GetBdAddress(),
                                    GetConnectionStatus(),
                                    aStatus);
    }
    MessageLoopForIO::current()->PostTask(
      FROM_HERE, new DeleteTask<AcceptWatcher>(this));
  }

private:
  nsRefPtr<BluetoothSocketResultHandler> mRes;
};

void
BluetoothSocketInterface::Accept(int aFd, BluetoothSocketResultHandler* aRes)
{
  /* receive Bluedroid's socket-setup messages and client fd */
  Task* t = new SocketMessageWatcherTask(new AcceptWatcher(aFd, aRes));
  XRE_GetIOMessageLoop()->PostTask(FROM_HERE, t);
}

BluetoothSocketInterface::BluetoothSocketInterface(
  const btsock_interface_t* aInterface)
: mInterface(aInterface)
{
  MOZ_ASSERT(mInterface);
}

BluetoothSocketInterface::~BluetoothSocketInterface()
{ }

//
// Handsfree Interface
//

template<>
struct interface_traits<BluetoothHandsfreeInterface>
{
  typedef const bthf_interface_t const_interface_type;

  static const char* profile_id()
  {
    return BT_PROFILE_HANDSFREE_ID;
  }
};

typedef
  BluetoothInterfaceRunnable0<BluetoothHandsfreeResultHandler, void>
  BluetoothHandsfreeResultRunnable;

typedef
  BluetoothInterfaceRunnable1<BluetoothHandsfreeResultHandler, void, bt_status_t>
  BluetoothHandsfreeErrorRunnable;

static nsresult
DispatchBluetoothHandsfreeResult(
  BluetoothHandsfreeResultHandler* aRes,
  void (BluetoothHandsfreeResultHandler::*aMethod)(),
  bt_status_t aStatus)
{
  MOZ_ASSERT(aRes);

  nsRunnable* runnable;

  if (aStatus == BT_STATUS_SUCCESS) {
    runnable = new BluetoothHandsfreeResultRunnable(aRes, aMethod);
  } else {
    runnable = new BluetoothHandsfreeErrorRunnable(aRes,
      &BluetoothHandsfreeResultHandler::OnError, aStatus);
  }
  nsresult rv = NS_DispatchToMainThread(runnable);
  if (NS_FAILED(rv)) {
    BT_WARNING("NS_DispatchToMainThread failed: %X", rv);
  }
  return rv;
}

BluetoothHandsfreeInterface::BluetoothHandsfreeInterface(
  const bthf_interface_t* aInterface)
: mInterface(aInterface)
{
  MOZ_ASSERT(mInterface);
}

BluetoothHandsfreeInterface::~BluetoothHandsfreeInterface()
{ }

void
BluetoothHandsfreeInterface::Init(bthf_callbacks_t* aCallbacks,
                                  BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status = mInterface->init(aCallbacks);

  if (aRes) {
    DispatchBluetoothHandsfreeResult(aRes,
                                     &BluetoothHandsfreeResultHandler::Init,
                                     status);
  }
}

void
BluetoothHandsfreeInterface::Cleanup(BluetoothHandsfreeResultHandler* aRes)
{
  mInterface->cleanup();

  if (aRes) {
    DispatchBluetoothHandsfreeResult(aRes,
                                     &BluetoothHandsfreeResultHandler::Cleanup,
                                     BT_STATUS_SUCCESS);
  }
}

/* Connect / Disconnect */

void
BluetoothHandsfreeInterface::Connect(bt_bdaddr_t* aBdAddr,
                                     BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status = mInterface->connect(aBdAddr);

  if (aRes) {
    DispatchBluetoothHandsfreeResult(
      aRes, &BluetoothHandsfreeResultHandler::Connect, status);
  }
}

void
BluetoothHandsfreeInterface::Disconnect(bt_bdaddr_t* aBdAddr,
                                        BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status = mInterface->disconnect(aBdAddr);

  if (aRes) {
    DispatchBluetoothHandsfreeResult(
      aRes, &BluetoothHandsfreeResultHandler::Disconnect, status);
  }
}

void
BluetoothHandsfreeInterface::ConnectAudio(
  bt_bdaddr_t* aBdAddr, BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status = mInterface->connect_audio(aBdAddr);

  if (aRes) {
    DispatchBluetoothHandsfreeResult(
      aRes, &BluetoothHandsfreeResultHandler::ConnectAudio, status);
  }
}

void
BluetoothHandsfreeInterface::DisconnectAudio(
  bt_bdaddr_t* aBdAddr, BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status = mInterface->disconnect_audio(aBdAddr);

  if (aRes) {
    DispatchBluetoothHandsfreeResult(
      aRes, &BluetoothHandsfreeResultHandler::DisconnectAudio, status);
  }
}

/* Voice Recognition */

void
BluetoothHandsfreeInterface::StartVoiceRecognition(
  BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status = mInterface->start_voice_recognition();

  if (aRes) {
    DispatchBluetoothHandsfreeResult(
      aRes, &BluetoothHandsfreeResultHandler::StartVoiceRecognition, status);
  }
}

void
BluetoothHandsfreeInterface::StopVoiceRecognition(
  BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status = mInterface->stop_voice_recognition();

  if (aRes) {
    DispatchBluetoothHandsfreeResult(
      aRes, &BluetoothHandsfreeResultHandler::StopVoiceRecognition, status);
  }
}

/* Volume */

void
BluetoothHandsfreeInterface::VolumeControl(
  bthf_volume_type_t aType, int aVolume, BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status = mInterface->volume_control(aType, aVolume);

  if (aRes) {
    DispatchBluetoothHandsfreeResult(
      aRes, &BluetoothHandsfreeResultHandler::VolumeControl, status);
  }
}

/* Device status */

void
BluetoothHandsfreeInterface::DeviceStatusNotification(
  bthf_network_state_t aNtkState, bthf_service_type_t aSvcType, int aSignal,
  int aBattChg, BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status = mInterface->device_status_notification(
    aNtkState, aSvcType, aSignal, aBattChg);

  if (aRes) {
    DispatchBluetoothHandsfreeResult(
      aRes, &BluetoothHandsfreeResultHandler::DeviceStatusNotification,
      status);
  }
}

/* Responses */

void
BluetoothHandsfreeInterface::CopsResponse(
  const char* aCops, BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status = mInterface->cops_response(aCops);

  if (aRes) {
    DispatchBluetoothHandsfreeResult(
      aRes, &BluetoothHandsfreeResultHandler::CopsResponse, status);
  }
}

void
BluetoothHandsfreeInterface::CindResponse(
  int aSvc, int aNumActive, int aNumHeld, bthf_call_state_t aCallSetupState,
  int aSignal, int aRoam, int aBattChg, BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status = mInterface->cind_response(aSvc, aNumActive, aNumHeld,
                                                 aCallSetupState, aSignal,
                                                 aRoam, aBattChg);
  if (aRes) {
    DispatchBluetoothHandsfreeResult(
      aRes, &BluetoothHandsfreeResultHandler::CindResponse, status);
  }
}

void
BluetoothHandsfreeInterface::FormattedAtResponse(
  const char* aRsp, BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status = mInterface->formatted_at_response(aRsp);

  if (aRes) {
    DispatchBluetoothHandsfreeResult(
      aRes, &BluetoothHandsfreeResultHandler::FormattedAtResponse, status);
  }
}

void
BluetoothHandsfreeInterface::AtResponse(bthf_at_response_t aResponseCode,
                                        int aErrorCode,
                                        BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status = mInterface->at_response(aResponseCode, aErrorCode);

  if (aRes) {
    DispatchBluetoothHandsfreeResult(
      aRes, &BluetoothHandsfreeResultHandler::AtResponse, status);
  }
}

void
BluetoothHandsfreeInterface::ClccResponse(
  int aIndex, bthf_call_direction_t aDir, bthf_call_state_t aState,
  bthf_call_mode_t aMode, bthf_call_mpty_type_t aMpty, const char* aNumber,
  bthf_call_addrtype_t aType, BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status = mInterface->clcc_response(aIndex, aDir, aState, aMode,
                                                 aMpty, aNumber, aType);
  if (aRes) {
    DispatchBluetoothHandsfreeResult(
      aRes, &BluetoothHandsfreeResultHandler::ClccResponse, status);
  }
}

/* Phone State */

void
BluetoothHandsfreeInterface::PhoneStateChange(int aNumActive, int aNumHeld,
  bthf_call_state_t aCallSetupState, const char* aNumber,
  bthf_call_addrtype_t aType, BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status = mInterface->phone_state_change(aNumActive, aNumHeld,
                                                      aCallSetupState,
                                                      aNumber, aType);
  if (aRes) {
    DispatchBluetoothHandsfreeResult(
      aRes, &BluetoothHandsfreeResultHandler::PhoneStateChange, status);
  }
}

//
// Bluetooth Advanced Audio Interface
//

template<>
struct interface_traits<BluetoothA2dpInterface>
{
  typedef const btav_interface_t const_interface_type;

  static const char* profile_id()
  {
    return BT_PROFILE_ADVANCED_AUDIO_ID;
  }
};

typedef
  BluetoothInterfaceRunnable0<BluetoothA2dpResultHandler, void>
  BluetoothA2dpResultRunnable;

typedef
  BluetoothInterfaceRunnable1<BluetoothA2dpResultHandler, void, bt_status_t>
  BluetoothA2dpErrorRunnable;

static nsresult
DispatchBluetoothA2dpResult(
  BluetoothA2dpResultHandler* aRes,
  void (BluetoothA2dpResultHandler::*aMethod)(),
  bt_status_t aStatus)
{
  MOZ_ASSERT(aRes);

  nsRunnable* runnable;

  if (aStatus == BT_STATUS_SUCCESS) {
    runnable = new BluetoothA2dpResultRunnable(aRes, aMethod);
  } else {
    runnable = new BluetoothA2dpErrorRunnable(aRes,
      &BluetoothA2dpResultHandler::OnError, aStatus);
  }
  nsresult rv = NS_DispatchToMainThread(runnable);
  if (NS_FAILED(rv)) {
    BT_WARNING("NS_DispatchToMainThread failed: %X", rv);
  }
  return rv;
}

BluetoothA2dpInterface::BluetoothA2dpInterface(
  const btav_interface_t* aInterface)
: mInterface(aInterface)
{
  MOZ_ASSERT(mInterface);
}

BluetoothA2dpInterface::~BluetoothA2dpInterface()
{ }

bt_status_t
BluetoothA2dpInterface::Init(btav_callbacks_t* aCallbacks)
{
  return mInterface->init(aCallbacks);
}

void
BluetoothA2dpInterface::Cleanup()
{
  mInterface->cleanup();
}

bt_status_t
BluetoothA2dpInterface::Connect(bt_bdaddr_t *aBdAddr)
{
  return mInterface->connect(aBdAddr);
}

bt_status_t
BluetoothA2dpInterface::Disconnect(bt_bdaddr_t *aBdAddr)
{
  return mInterface->disconnect(aBdAddr);
}

//
// Bluetooth AVRCP Interface
//

#if ANDROID_VERSION >= 18
template<>
struct interface_traits<BluetoothAvrcpInterface>
{
  typedef const btrc_interface_t const_interface_type;

  static const char* profile_id()
  {
    return BT_PROFILE_AV_RC_ID;
  }
};

typedef
  BluetoothInterfaceRunnable0<BluetoothAvrcpResultHandler, void>
  BluetoothAvrcpResultRunnable;

typedef
  BluetoothInterfaceRunnable1<BluetoothAvrcpResultHandler, void, bt_status_t>
  BluetoothAvrcpErrorRunnable;

static nsresult
DispatchBluetoothAvrcpResult(
  BluetoothAvrcpResultHandler* aRes,
  void (BluetoothAvrcpResultHandler::*aMethod)(),
  bt_status_t aStatus)
{
  MOZ_ASSERT(aRes);

  nsRunnable* runnable;

  if (aStatus == BT_STATUS_SUCCESS) {
    runnable = new BluetoothAvrcpResultRunnable(aRes, aMethod);
  } else {
    runnable = new BluetoothAvrcpErrorRunnable(aRes,
      &BluetoothAvrcpResultHandler::OnError, aStatus);
  }
  nsresult rv = NS_DispatchToMainThread(runnable);
  if (NS_FAILED(rv)) {
    BT_WARNING("NS_DispatchToMainThread failed: %X", rv);
  }
  return rv;
}

BluetoothAvrcpInterface::BluetoothAvrcpInterface(
  const btrc_interface_t* aInterface)
: mInterface(aInterface)
{
  MOZ_ASSERT(mInterface);
}

BluetoothAvrcpInterface::~BluetoothAvrcpInterface()
{ }

bt_status_t
BluetoothAvrcpInterface::Init(btrc_callbacks_t* aCallbacks)
{
  return mInterface->init(aCallbacks);
}

void
BluetoothAvrcpInterface::Cleanup()
{
  mInterface->cleanup();
}

bt_status_t
BluetoothAvrcpInterface::GetPlayStatusRsp(btrc_play_status_t aPlayStatus,
                                          uint32_t aSongLen, uint32_t aSongPos)
{
  return mInterface->get_play_status_rsp(aPlayStatus, aSongLen, aSongPos);
}

bt_status_t
BluetoothAvrcpInterface::ListPlayerAppAttrRsp(int aNumAttr,
                                              btrc_player_attr_t* aPAttrs)
{
  return mInterface->list_player_app_attr_rsp(aNumAttr, aPAttrs);
}

bt_status_t
BluetoothAvrcpInterface::ListPlayerAppValueRsp(int aNumVal, uint8_t* aPVals)
{
  return mInterface->list_player_app_value_rsp(aNumVal, aPVals);
}

bt_status_t
BluetoothAvrcpInterface::GetPlayerAppValueRsp(btrc_player_settings_t* aPVals)
{
  return mInterface->get_player_app_value_rsp(aPVals);
}

bt_status_t
BluetoothAvrcpInterface::GetPlayerAppAttrTextRsp(int aNumAttr,
  btrc_player_setting_text_t* aPAttrs)
{
  return mInterface->get_player_app_attr_text_rsp(aNumAttr, aPAttrs);
}

bt_status_t
BluetoothAvrcpInterface::GetPlayerAppValueTextRsp(int aNumVal,
  btrc_player_setting_text_t* aPVals)
{
  return mInterface->get_player_app_value_text_rsp(aNumVal, aPVals);
}

bt_status_t
BluetoothAvrcpInterface::GetElementAttrRsp(uint8_t aNumAttr,
                                           btrc_element_attr_val_t* aPAttrs)
{
  return mInterface->get_element_attr_rsp(aNumAttr, aPAttrs);
}

bt_status_t
BluetoothAvrcpInterface::SetPlayerAppValueRsp(btrc_status_t aRspStatus)
{
  return mInterface->set_player_app_value_rsp(aRspStatus);
}

bt_status_t
BluetoothAvrcpInterface::RegisterNotificationRsp(btrc_event_id_t aEventId,
  btrc_notification_type_t aType, btrc_register_notification_t* aPParam)
{
  return mInterface->register_notification_rsp(aEventId, aType, aPParam);
}

bt_status_t
BluetoothAvrcpInterface::SetVolume(uint8_t aVolume)
{
#if ANDROID_VERSION >= 19
  return mInterface->set_volume(aVolume);
#else
  return BT_STATUS_UNSUPPORTED;
#endif
}
#endif

//
// Bluetooth Core Interface
//

typedef
  BluetoothInterfaceRunnable0<BluetoothResultHandler, void>
  BluetoothResultRunnable;

typedef
  BluetoothInterfaceRunnable1<BluetoothResultHandler, void, int>
  BluetoothErrorRunnable;

static nsresult
DispatchBluetoothResult(BluetoothResultHandler* aRes,
                        void (BluetoothResultHandler::*aMethod)(),
                        int aStatus)
{
  MOZ_ASSERT(aRes);

  nsRunnable* runnable;

  if (aStatus == BT_STATUS_SUCCESS) {
    runnable = new BluetoothResultRunnable(aRes, aMethod);
  } else {
    runnable = new
      BluetoothErrorRunnable(aRes, &BluetoothResultHandler::OnError, aStatus);
  }
  nsresult rv = NS_DispatchToMainThread(runnable);
  if (NS_FAILED(rv)) {
    BT_WARNING("NS_DispatchToMainThread failed: %X", rv);
  }
  return rv;
}

/* returns the container structure of a variable; _t is the container's
 * type, _v the name of the variable, and _m is _v's field within _t
 */
#define container(_t, _v, _m) \
  ( (_t*)( ((const unsigned char*)(_v)) - offsetof(_t, _m) ) )

BluetoothInterface*
BluetoothInterface::GetInstance()
{
  static BluetoothInterface* sBluetoothInterface;

  if (sBluetoothInterface) {
    return sBluetoothInterface;
  }

  /* get driver module */

  const hw_module_t* module;
  int err = hw_get_module(BT_HARDWARE_MODULE_ID, &module);
  if (err) {
    BT_WARNING("hw_get_module failed: %s", strerror(err));
    return nullptr;
  }

  /* get device */

  hw_device_t* device;
  err = module->methods->open(module, BT_HARDWARE_MODULE_ID, &device);
  if (err) {
    BT_WARNING("open failed: %s", strerror(err));
    return nullptr;
  }

  const bluetooth_device_t* bt_device =
    container(bluetooth_device_t, device, common);

  /* get interface */

  const bt_interface_t* bt_interface = bt_device->get_bluetooth_interface();
  if (!bt_interface) {
    BT_WARNING("get_bluetooth_interface failed");
    goto err_get_bluetooth_interface;
  }

  if (bt_interface->size != sizeof(*bt_interface)) {
    BT_WARNING("interface of incorrect size");
    goto err_bt_interface_size;
  }

  sBluetoothInterface = new BluetoothInterface(bt_interface);

  return sBluetoothInterface;

err_bt_interface_size:
err_get_bluetooth_interface:
  err = device->close(device);
  if (err) {
    BT_WARNING("close failed: %s", strerror(err));
  }
  return nullptr;
}

BluetoothInterface::BluetoothInterface(const bt_interface_t* aInterface)
: mInterface(aInterface)
{
  MOZ_ASSERT(mInterface);
}

BluetoothInterface::~BluetoothInterface()
{ }

void
BluetoothInterface::Init(bt_callbacks_t* aCallbacks,
                         BluetoothResultHandler* aRes)
{
  int status = mInterface->init(aCallbacks);

  if (aRes) {
    DispatchBluetoothResult(aRes, &BluetoothResultHandler::Init, status);
  }
}

void
BluetoothInterface::Cleanup(BluetoothResultHandler* aRes)
{
  mInterface->cleanup();

  if (aRes) {
    DispatchBluetoothResult(aRes, &BluetoothResultHandler::Cleanup,
                            BT_STATUS_SUCCESS);
  }
}

void
BluetoothInterface::Enable(BluetoothResultHandler* aRes)
{
  int status = mInterface->enable();

  if (aRes) {
    DispatchBluetoothResult(aRes, &BluetoothResultHandler::Enable, status);
  }
}

void
BluetoothInterface::Disable(BluetoothResultHandler* aRes)
{
  int status = mInterface->disable();

  if (aRes) {
    DispatchBluetoothResult(aRes, &BluetoothResultHandler::Disable, status);
  }
}

/* Adapter Properties */

void
BluetoothInterface::GetAdapterProperties(BluetoothResultHandler* aRes)
{
  int status = mInterface->get_adapter_properties();

  if (aRes) {
    DispatchBluetoothResult(aRes,
                            &BluetoothResultHandler::GetAdapterProperties,
                            status);
  }
}

void
BluetoothInterface::GetAdapterProperty(bt_property_type_t aType,
                                       BluetoothResultHandler* aRes)
{
  int status = mInterface->get_adapter_property(aType);

  if (aRes) {
    DispatchBluetoothResult(aRes,
                            &BluetoothResultHandler::GetAdapterProperty,
                            status);
  }
}

void
BluetoothInterface::SetAdapterProperty(const bt_property_t* aProperty,
                                       BluetoothResultHandler* aRes)
{
  int status = mInterface->set_adapter_property(aProperty);

  if (aRes) {
    DispatchBluetoothResult(aRes,
                            &BluetoothResultHandler::SetAdapterProperty,
                            status);
  }
}

/* Remote Device Properties */

void
BluetoothInterface::GetRemoteDeviceProperties(bt_bdaddr_t *aRemoteAddr,
                                              BluetoothResultHandler* aRes)
{
  int status = mInterface->get_remote_device_properties(aRemoteAddr);

  if (aRes) {
    DispatchBluetoothResult(aRes,
                            &BluetoothResultHandler::GetRemoteDeviceProperties,
                            status);
  }
}

void
BluetoothInterface::GetRemoteDeviceProperty(bt_bdaddr_t* aRemoteAddr,
                                            bt_property_type_t aType,
                                            BluetoothResultHandler* aRes)
{
  int status = mInterface->get_remote_device_property(aRemoteAddr, aType);

  if (aRes) {
    DispatchBluetoothResult(aRes,
                            &BluetoothResultHandler::GetRemoteDeviceProperty,
                            status);
  }
}

void
BluetoothInterface::SetRemoteDeviceProperty(bt_bdaddr_t* aRemoteAddr,
                                            const bt_property_t* aProperty,
                                            BluetoothResultHandler* aRes)
{
  int status = mInterface->set_remote_device_property(aRemoteAddr, aProperty);

  if (aRes) {
    DispatchBluetoothResult(aRes,
                            &BluetoothResultHandler::SetRemoteDeviceProperty,
                            status);
  }
}

/* Remote Services */

void
BluetoothInterface::GetRemoteServiceRecord(bt_bdaddr_t* aRemoteAddr,
                                           bt_uuid_t* aUuid,
                                           BluetoothResultHandler* aRes)
{
  int status = mInterface->get_remote_service_record(aRemoteAddr, aUuid);

  if (aRes) {
    DispatchBluetoothResult(aRes,
                            &BluetoothResultHandler::GetRemoteServiceRecord,
                            status);
  }
}

void
BluetoothInterface::GetRemoteServices(bt_bdaddr_t* aRemoteAddr,
                                      BluetoothResultHandler* aRes)
{
  int status = mInterface->get_remote_services(aRemoteAddr);

  if (aRes) {
    DispatchBluetoothResult(aRes,
                            &BluetoothResultHandler::GetRemoteServices,
                            status);
  }
}

/* Discovery */

void
BluetoothInterface::StartDiscovery(BluetoothResultHandler* aRes)
{
  int status = mInterface->start_discovery();

  if (aRes) {
    DispatchBluetoothResult(aRes,
                            &BluetoothResultHandler::StartDiscovery,
                            status);
  }
}

void
BluetoothInterface::CancelDiscovery(BluetoothResultHandler* aRes)
{
  int status = mInterface->cancel_discovery();

  if (aRes) {
    DispatchBluetoothResult(aRes,
                            &BluetoothResultHandler::CancelDiscovery,
                            status);
  }
}

/* Bonds */

void
BluetoothInterface::CreateBond(const bt_bdaddr_t* aBdAddr,
                               BluetoothResultHandler* aRes)
{
  int status = mInterface->create_bond(aBdAddr);

  if (aRes) {
    DispatchBluetoothResult(aRes,
                            &BluetoothResultHandler::CreateBond,
                            status);
  }
}

void
BluetoothInterface::RemoveBond(const bt_bdaddr_t* aBdAddr,
                               BluetoothResultHandler* aRes)
{
  int status = mInterface->remove_bond(aBdAddr);

  if (aRes) {
    DispatchBluetoothResult(aRes,
                            &BluetoothResultHandler::RemoveBond,
                            status);
  }
}

void
BluetoothInterface::CancelBond(const bt_bdaddr_t* aBdAddr,
                               BluetoothResultHandler* aRes)
{
  int status = mInterface->cancel_bond(aBdAddr);

  if (aRes) {
    DispatchBluetoothResult(aRes,
                            &BluetoothResultHandler::CancelBond,
                            status);
  }
}

/* Authentication */

void
BluetoothInterface::PinReply(const bt_bdaddr_t* aBdAddr, uint8_t aAccept,
                             uint8_t aPinLen, bt_pin_code_t* aPinCode,
                             BluetoothResultHandler* aRes)
{
  int status = mInterface->pin_reply(aBdAddr, aAccept, aPinLen, aPinCode);

  if (aRes) {
    DispatchBluetoothResult(aRes,
                            &BluetoothResultHandler::PinReply,
                            status);
  }
}

void
BluetoothInterface::SspReply(const bt_bdaddr_t* aBdAddr,
                             bt_ssp_variant_t aVariant,
                             uint8_t aAccept, uint32_t aPasskey,
                             BluetoothResultHandler* aRes)
{
  int status = mInterface->ssp_reply(aBdAddr, aVariant, aAccept, aPasskey);

  if (aRes) {
    DispatchBluetoothResult(aRes,
                            &BluetoothResultHandler::SspReply,
                            status);
  }
}

/* DUT Mode */

void
BluetoothInterface::DutModeConfigure(uint8_t aEnable,
                                     BluetoothResultHandler* aRes)
{
  int status = mInterface->dut_mode_configure(aEnable);

  if (aRes) {
    DispatchBluetoothResult(aRes,
                            &BluetoothResultHandler::DutModeConfigure,
                            status);
  }
}

void
BluetoothInterface::DutModeSend(uint16_t aOpcode, uint8_t* aBuf, uint8_t aLen,
                                BluetoothResultHandler* aRes)
{
  int status = mInterface->dut_mode_send(aOpcode, aBuf, aLen);

  if (aRes) {
    DispatchBluetoothResult(aRes,
                            &BluetoothResultHandler::DutModeSend,
                            status);
  }
}

/* LE Mode */

void
BluetoothInterface::LeTestMode(uint16_t aOpcode, uint8_t* aBuf, uint8_t aLen,
                               BluetoothResultHandler* aRes)
{
#if ANDROID_VERSION >= 18
  int status = mInterface->le_test_mode(aOpcode, aBuf, aLen);
#else
  int status = BT_STATUS_UNSUPPORTED;
#endif

  if (aRes) {
    DispatchBluetoothResult(aRes,
                            &BluetoothResultHandler::LeTestMode,
                            status);
  }
}

/* Profile Interfaces */

template <class T>
T*
BluetoothInterface::GetProfileInterface()
{
  static T* sBluetoothProfileInterface;

  if (sBluetoothProfileInterface) {
    return sBluetoothProfileInterface;
  }

  typename interface_traits<T>::const_interface_type* interface =
    reinterpret_cast<typename interface_traits<T>::const_interface_type*>(
      mInterface->get_profile_interface(interface_traits<T>::profile_id()));

  if (!interface) {
    BT_WARNING("Bluetooth profile '%s' is not supported",
               interface_traits<T>::profile_id());
    return nullptr;
  }

  if (interface->size != sizeof(*interface)) {
    BT_WARNING("interface of incorrect size");
    return nullptr;
  }

  sBluetoothProfileInterface = new T(interface);

  return sBluetoothProfileInterface;
}

BluetoothSocketInterface*
BluetoothInterface::GetBluetoothSocketInterface()
{
  return GetProfileInterface<BluetoothSocketInterface>();
}

BluetoothHandsfreeInterface*
BluetoothInterface::GetBluetoothHandsfreeInterface()
{
  return GetProfileInterface<BluetoothHandsfreeInterface>();
}

BluetoothA2dpInterface*
BluetoothInterface::GetBluetoothA2dpInterface()
{
  return GetProfileInterface<BluetoothA2dpInterface>();
}

BluetoothAvrcpInterface*
BluetoothInterface::GetBluetoothAvrcpInterface()
{
#if ANDROID_VERSION >= 18
  return GetProfileInterface<BluetoothAvrcpInterface>();
#else
  return nullptr;
#endif
}

END_BLUETOOTH_NAMESPACE
