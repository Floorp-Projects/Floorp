/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothDaemonConnection.h"
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include "mozilla/ipc/DataSocket.h"
#include "mozilla/ipc/UnixSocketWatcher.h"
#include "nsTArray.h"
#include "nsXULAppAPI.h"

#ifdef CHROMIUM_LOG
#undef CHROMIUM_LOG
#endif

#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define CHROMIUM_LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "I/O", args);
#else
#include <stdio.h>
#define IODEBUG true
#define CHROMIUM_LOG(args...) if (IODEBUG) printf(args);
#endif

namespace mozilla {
namespace ipc {

// The connection to the Bluetooth daemon is established
// using an abstract socket name. The \0 prefix will be added
// by the |Connect| method.
static const char sBluetoothdSocketName[] = "bluez_hal_socket";

//
// BluetoothDaemonPDU
//

BluetoothDaemonPDU::BluetoothDaemonPDU(uint8_t aService, uint8_t aOpcode,
                                       uint16_t aPayloadSize)
  : UnixSocketIOBuffer(HEADER_SIZE + aPayloadSize)
  , mConsumer(nullptr)
  , mUserData(nullptr)
{
  uint8_t* data = Append(HEADER_SIZE);
  MOZ_ASSERT(data);

  // Setup PDU header
  data[OFF_SERVICE] = aService;
  data[OFF_OPCODE] = aOpcode;
  memcpy(data + OFF_LENGTH, &aPayloadSize, sizeof(aPayloadSize));
}

BluetoothDaemonPDU::BluetoothDaemonPDU(size_t aPayloadSize)
  : UnixSocketIOBuffer(HEADER_SIZE + aPayloadSize)
  , mConsumer(nullptr)
  , mUserData(nullptr)
{ }

void
BluetoothDaemonPDU::GetHeader(uint8_t& aService, uint8_t& aOpcode,
                              uint16_t& aPayloadSize)
{
  memcpy(&aService, GetData(OFF_SERVICE), sizeof(aService));
  memcpy(&aOpcode, GetData(OFF_OPCODE), sizeof(aOpcode));
  memcpy(&aPayloadSize, GetData(OFF_LENGTH), sizeof(aPayloadSize));
}

ssize_t
BluetoothDaemonPDU::Send(int aFd)
{
  struct iovec iv;
  memset(&iv, 0, sizeof(iv));
  iv.iov_base = GetData(GetLeadingSpace());
  iv.iov_len = GetSize();

  struct msghdr msg;
  memset(&msg, 0, sizeof(msg));
  msg.msg_iov = &iv;
  msg.msg_iovlen = 1;
  msg.msg_control = nullptr;
  msg.msg_controllen = 0;

  ssize_t res = TEMP_FAILURE_RETRY(sendmsg(aFd, &msg, 0));
  if (res < 0) {
    MOZ_ASSERT(errno != EBADF); /* internal error */
    OnError("sendmsg", errno);
    return -1;
  }

  Consume(res);

  if (mConsumer) {
    // We successfully sent a PDU, now store the
    // result runnable in the consumer.
    mConsumer->StoreUserData(*this);
  }

  return res;
}

#define CMSGHDR_CONTAINS_FD(_cmsghdr) \
    ( ((_cmsghdr)->cmsg_level == SOL_SOCKET) && \
      ((_cmsghdr)->cmsg_type == SCM_RIGHTS) )

ssize_t
BluetoothDaemonPDU::Receive(int aFd)
{
  struct iovec iv;
  memset(&iv, 0, sizeof(iv));
  iv.iov_base = GetData(0);
  iv.iov_len = GetAvailableSpace();

  uint8_t cmsgbuf[CMSG_SPACE(sizeof(int))];

  struct msghdr msg;
  memset(&msg, 0, sizeof(msg));
  msg.msg_iov = &iv;
  msg.msg_iovlen = 1;
  msg.msg_control = cmsgbuf;
  msg.msg_controllen = sizeof(cmsgbuf);

  ssize_t res = TEMP_FAILURE_RETRY(recvmsg(aFd, &msg, MSG_NOSIGNAL));
  if (res < 0) {
    MOZ_ASSERT(errno != EBADF); /* internal error */
    OnError("recvmsg", errno);
    return -1;
  }
  if (msg.msg_flags & (MSG_CTRUNC | MSG_OOB | MSG_ERRQUEUE)) {
    return -1;
  }

  SetRange(0, res);

  struct cmsghdr *chdr = CMSG_FIRSTHDR(&msg);

  for (; chdr; chdr = CMSG_NXTHDR(&msg, chdr)) {
    if (NS_WARN_IF(!CMSGHDR_CONTAINS_FD(chdr))) {
      continue;
    }
    // Retrieve sent file descriptor. If multiple file descriptors
    // have been sent, we close all but the final one.
    mReceivedFd = *(static_cast<int*>(CMSG_DATA(chdr)));
  }

  return res;
}

int
BluetoothDaemonPDU::AcquireFd()
{
  return mReceivedFd.forget();
}

nsresult
BluetoothDaemonPDU::UpdateHeader()
{
  size_t len = GetPayloadSize();
  if (len >= MAX_PAYLOAD_LENGTH) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  uint16_t len16 = static_cast<uint16_t>(len);

  memcpy(GetData(OFF_LENGTH), &len16, sizeof(len16));

  return NS_OK;
}

size_t
BluetoothDaemonPDU::GetPayloadSize() const
{
  MOZ_ASSERT(GetSize() >= HEADER_SIZE);

  return GetSize() - HEADER_SIZE;
}

void
BluetoothDaemonPDU::OnError(const char* aFunction, int aErrno)
{
  CHROMIUM_LOG("%s failed with error %d (%s)",
               aFunction, aErrno, strerror(aErrno));
}

//
// BluetoothDaemonPDUConsumer
//

BluetoothDaemonPDUConsumer::BluetoothDaemonPDUConsumer()
{ }

BluetoothDaemonPDUConsumer::~BluetoothDaemonPDUConsumer()
{ }

//
// BluetoothDaemonConnectionIO
//

class BluetoothDaemonConnectionIO final
  : public UnixSocketWatcher
  , public ConnectionOrientedSocketIO
{
public:
  BluetoothDaemonConnectionIO(MessageLoop* aIOLoop, int aFd,
                              ConnectionStatus aConnectionStatus,
                              BluetoothDaemonConnection* aConnection,
                              BluetoothDaemonPDUConsumer* aConsumer);

  // Task callback methods
  //

  void Connect(const char* aSocketName);

  void Send(UnixSocketIOBuffer* aBuffer);

  void OnSocketCanReceiveWithoutBlocking() override;
  void OnSocketCanSendWithoutBlocking() override;

  void OnConnected() override;
  void OnError(const char* aFunction, int aErrno) override;

  // Methods for |ConnectionOrientedSocketIO|
  //

  nsresult Accept(int aFd,
                  const union sockaddr_any* aAddr,
                  socklen_t aAddrLen) override;

  // Methods for |DataSocketIO|
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
  BluetoothDaemonConnection* mConnection;
  BluetoothDaemonPDUConsumer* mConsumer;
  nsAutoPtr<BluetoothDaemonPDU> mPDU;
  bool mShuttingDownOnIOThread;
};

BluetoothDaemonConnectionIO::BluetoothDaemonConnectionIO(
  MessageLoop* aIOLoop, int aFd,
  ConnectionStatus aConnectionStatus,
  BluetoothDaemonConnection* aConnection,
  BluetoothDaemonPDUConsumer* aConsumer)
: UnixSocketWatcher(aIOLoop, aFd, aConnectionStatus)
, mConnection(aConnection)
, mConsumer(aConsumer)
, mShuttingDownOnIOThread(false)
{
  MOZ_ASSERT(mConnection);
  MOZ_ASSERT(mConsumer);
}

void
BluetoothDaemonConnectionIO::Connect(const char* aSocketName)
{
  static const size_t sNameOffset = 1;

  MOZ_ASSERT(aSocketName);

  // Create socket address

  struct sockaddr_un addr;
  size_t namesiz = strlen(aSocketName) + 1;

  if((sNameOffset + namesiz) > sizeof(addr.sun_path)) {
    CHROMIUM_LOG("Address too long for socket struct!");
    return;
  }
  memset(addr.sun_path, '\0', sNameOffset); // abstract socket
  memcpy(addr.sun_path + sNameOffset, aSocketName, namesiz);
  addr.sun_family = AF_UNIX;

  socklen_t socklen = offsetof(struct sockaddr_un, sun_path) + 1 + namesiz;

  // Create socket

  int fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
  if (fd < 0) {
    OnError("socket", errno);
    return;
  }
  if (TEMP_FAILURE_RETRY(fcntl(fd, F_SETFL, O_NONBLOCK)) < 0) {
    OnError("fcntl", errno);
    ScopedClose cleanupFd(fd);
    return;
  }

  SetFd(fd);

  // Connect socket to address; calls OnConnected()
  // on success, or OnError() otherwise
  nsresult rv = UnixSocketWatcher::Connect(
    reinterpret_cast<struct sockaddr*>(&addr), socklen);
  NS_WARN_IF(NS_FAILED(rv));
}

void
BluetoothDaemonConnectionIO::Send(UnixSocketIOBuffer* aBuffer)
{
  MOZ_ASSERT(aBuffer);

  EnqueueData(aBuffer);
  AddWatchers(WRITE_WATCHER, false);
}

void
BluetoothDaemonConnectionIO::OnSocketCanReceiveWithoutBlocking()
{
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
BluetoothDaemonConnectionIO::OnSocketCanSendWithoutBlocking()
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());
  MOZ_ASSERT(GetConnectionStatus() == SOCKET_IS_CONNECTED);
  MOZ_ASSERT(!IsShutdownOnIOThread());

  if (NS_WARN_IF(NS_FAILED(SendPendingData(GetFd())))) {
    RemoveWatchers(WRITE_WATCHER);
  }
}

void
BluetoothDaemonConnectionIO::OnConnected()
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
BluetoothDaemonConnectionIO::OnError(const char* aFunction, int aErrno)
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());

  UnixFdWatcher::OnError(aFunction, aErrno);

  // Clean up watchers, status, fd
  Close();

  // Tell the main thread we've errored
  NS_DispatchToMainThread(
    new SocketIOEventRunnable(this, SocketIOEventRunnable::CONNECT_ERROR));
}

// |ConnectionOrientedSocketIO|

nsresult
BluetoothDaemonConnectionIO::Accept(int aFd,
                                    const union sockaddr_any* aAddr,
                                    socklen_t aAddrLen)
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());
  MOZ_ASSERT(GetConnectionStatus() == SOCKET_IS_CONNECTING);

  // File-descriptor setup

  if (TEMP_FAILURE_RETRY(fcntl(aFd, F_SETFL, O_NONBLOCK)) < 0) {
    OnError("fcntl", errno);
    ScopedClose cleanupFd(aFd);
    return NS_ERROR_FAILURE;
  }

  SetSocket(aFd, SOCKET_IS_CONNECTED);

  // Signal success
  OnConnected();

  return NS_OK;
}

// |DataSocketIO|

nsresult
BluetoothDaemonConnectionIO::QueryReceiveBuffer(UnixSocketIOBuffer** aBuffer)
{
  MOZ_ASSERT(aBuffer);

  if (!mPDU) {
    /* There's only one PDU for receiving. We reuse it every time. */
    mPDU = new BluetoothDaemonPDU(BluetoothDaemonPDU::MAX_PAYLOAD_LENGTH);
  }
  *aBuffer = mPDU.get();

  return NS_OK;
}

void
BluetoothDaemonConnectionIO::ConsumeBuffer()
{
  MOZ_ASSERT(mConsumer);

  mConsumer->Handle(*mPDU);
}

void
BluetoothDaemonConnectionIO::DiscardBuffer()
{
  // Nothing to do.
}

// |SocketIOBase|

SocketBase*
BluetoothDaemonConnectionIO::GetSocketBase()
{
  return mConnection;
}

bool
BluetoothDaemonConnectionIO::IsShutdownOnMainThread() const
{
  MOZ_ASSERT(NS_IsMainThread());

  return mConnection == nullptr;
}

bool
BluetoothDaemonConnectionIO::IsShutdownOnIOThread() const
{
  return mShuttingDownOnIOThread;
}

void
BluetoothDaemonConnectionIO::ShutdownOnMainThread()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!IsShutdownOnMainThread());

  mConnection = nullptr;
}

void
BluetoothDaemonConnectionIO::ShutdownOnIOThread()
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!mShuttingDownOnIOThread);

  Close(); // will also remove fd from I/O loop
  mShuttingDownOnIOThread = true;
}

//
// I/O helper tasks
//

class BluetoothDaemonConnectTask final
  : public SocketIOTask<BluetoothDaemonConnectionIO>
{
public:
  BluetoothDaemonConnectTask(BluetoothDaemonConnectionIO* aIO)
  : SocketIOTask<BluetoothDaemonConnectionIO>(aIO)
  { }

  void Run() override
  {
    if (IsCanceled()) {
      return;
    }
    GetIO()->Connect(sBluetoothdSocketName);
  }
};

//
// BluetoothDaemonConnection
//

BluetoothDaemonConnection::BluetoothDaemonConnection()
  : mIO(nullptr)
{ }

BluetoothDaemonConnection::~BluetoothDaemonConnection()
{ }

nsresult
BluetoothDaemonConnection::ConnectSocket(BluetoothDaemonPDUConsumer* aConsumer)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mIO) {
    CHROMIUM_LOG("Bluetooth daemon already connecting/connected!");
    return NS_ERROR_FAILURE;
  }

  SetConnectionStatus(SOCKET_CONNECTING);

  MessageLoop* ioLoop = XRE_GetIOMessageLoop();
  mIO = new BluetoothDaemonConnectionIO(
    ioLoop, -1, UnixSocketWatcher::SOCKET_IS_CONNECTING, this, aConsumer);
  ioLoop->PostTask(FROM_HERE, new BluetoothDaemonConnectTask(mIO));

  return NS_OK;
}

ConnectionOrientedSocketIO*
BluetoothDaemonConnection::PrepareAccept(BluetoothDaemonPDUConsumer* aConsumer)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mIO);
  MOZ_ASSERT(aConsumer);

  SetConnectionStatus(SOCKET_CONNECTING);

  mIO = new BluetoothDaemonConnectionIO(
    XRE_GetIOMessageLoop(), -1, UnixSocketWatcher::SOCKET_IS_CONNECTING,
    this, aConsumer);

  return mIO;
}

// |ConnectionOrientedSocket|

ConnectionOrientedSocketIO*
BluetoothDaemonConnection::GetIO()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mIO); // Call |PrepareAccept| before listening for connections

  return mIO;
}

// |DataSocket|

void
BluetoothDaemonConnection::SendSocketData(UnixSocketIOBuffer* aBuffer)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mIO);

  XRE_GetIOMessageLoop()->PostTask(
    FROM_HERE,
    new SocketIOSendTask<BluetoothDaemonConnectionIO,
                         UnixSocketIOBuffer>(mIO, aBuffer));
}

// |SocketBase|

void
BluetoothDaemonConnection::CloseSocket()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mIO) {
    CHROMIUM_LOG("Bluetooth daemon already disconnected!");
    return;
  }

  XRE_GetIOMessageLoop()->PostTask(FROM_HERE, new SocketIOShutdownTask(mIO));

  mIO = nullptr;

  NotifyDisconnect();
}

}
}
