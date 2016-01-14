/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Netd.h"
#include <android/log.h>
#include <cutils/sockets.h>
#include <fcntl.h>
#include <sys/socket.h>

#include "android/log.h"

#include "nsWhitespaceTokenizer.h"
#include "nsXULAppAPI.h"
#include "nsAutoPtr.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Snprintf.h"
#include "SystemProperty.h"

#define NETD_LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Gonk", args)
#define ICS_SYS_USB_RNDIS_MAC "/sys/class/android_usb/android0/f_rndis/ethaddr"
#define INVALID_SOCKET -1
#define MAX_RECONNECT_TIMES 10

using mozilla::system::Property;

namespace {

RefPtr<mozilla::ipc::NetdClient> gNetdClient;
RefPtr<mozilla::ipc::NetdConsumer> gNetdConsumer;

class StopNetdConsumer : public nsRunnable {
public:
  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    gNetdConsumer = nullptr;
    return NS_OK;
  }
};

bool
InitRndisAddress()
{
  char mac[20];
  char serialno[] = "1234567890ABCDEF";
  static const int kEthernetAddressLength = 6;
  char address[kEthernetAddressLength];
  int i = 0;
  int ret = 0;
  int length = 0;
  mozilla::ScopedClose fd;

  fd.rwget() = open(ICS_SYS_USB_RNDIS_MAC, O_WRONLY);
  if (fd.rwget() == -1) {
    NETD_LOG("Unable to open file %s.", ICS_SYS_USB_RNDIS_MAC);
    return false;
  }

  Property::Get("ro.serialno", serialno, "1234567890ABCDEF");

  memset(address, 0, sizeof(address));
  // First byte is 0x02 to signify a locally administered address.
  address[0] = 0x02;
  length = strlen(serialno);
  for (i = 0; i < length; i++) {
    address[i % (kEthernetAddressLength - 1) + 1] ^= serialno[i];
  }

  snprintf_literal(mac, "%02x:%02x:%02x:%02x:%02x:%02x",
                   address[0], address[1], address[2],
                   address[3], address[4], address[5]);
  length = strlen(mac);
  ret = write(fd.get(), mac, length);
  if (ret != length) {
    NETD_LOG("Fail to write file %s.", ICS_SYS_USB_RNDIS_MAC);
    return false;
  }
  return true;
}

} // namespace

namespace mozilla {
namespace ipc {

NetdClient::NetdClient()
  : LineWatcher('\0', MAX_COMMAND_SIZE)
  , mIOLoop(MessageLoopForIO::current())
  , mSocket(INVALID_SOCKET)
  , mCurrentWriteOffset(0)
  , mReConnectTimes(0)
{
  MOZ_COUNT_CTOR(NetdClient);
}

NetdClient::~NetdClient()
{
  MOZ_COUNT_DTOR(NetdClient);
}

bool
NetdClient::OpenSocket()
{
  mSocket.rwget() = socket_local_client("netd",
                                        ANDROID_SOCKET_NAMESPACE_RESERVED,
                                        SOCK_STREAM);
  if (mSocket.rwget() < 0) {
    NETD_LOG("Error connecting to : netd (%s) - will retry", strerror(errno));
    return false;
  }
  // Add FD_CLOEXEC flag.
  int flags = fcntl(mSocket.get(), F_GETFD);
  if (flags == -1) {
    NETD_LOG("Error doing fcntl with F_GETFD command(%s)", strerror(errno));
    return false;
  }
  flags |= FD_CLOEXEC;
  if (fcntl(mSocket.get(), F_SETFD, flags) == -1) {
    NETD_LOG("Error doing fcntl with F_SETFD command(%s)", strerror(errno));
    return false;
  }
  // Set non-blocking.
  if (fcntl(mSocket.get(), F_SETFL, O_NONBLOCK) == -1) {
    NETD_LOG("Error set non-blocking socket(%s)", strerror(errno));
    return false;
  }
  if (!MessageLoopForIO::current()->
      WatchFileDescriptor(mSocket.get(),
                          true,
                          MessageLoopForIO::WATCH_READ,
                          &mReadWatcher,
                          this)) {
    NETD_LOG("Error set socket read watcher(%s)", strerror(errno));
    return false;
  }

  if (!mOutgoingQ.empty()) {
    MessageLoopForIO::current()->
      WatchFileDescriptor(mSocket.get(),
                          false,
                          MessageLoopForIO::WATCH_WRITE,
                          &mWriteWatcher,
                          this);
  }

  NETD_LOG("Connected to netd");
  return true;
}

void NetdClient::OnLineRead(int aFd, nsDependentCSubstring& aMessage)
{
  // Set errno to 0 first. For preventing to use the stale version of errno.
  errno = 0;
  // We found a line terminator. Each line is formatted as an
  // integer response code followed by the rest of the line.
  // Fish out the response code.
  int responseCode = strtol(aMessage.Data(), nullptr, 10);
  if (!errno) {
    NetdCommand* response = new NetdCommand();
    // Passing all the response message, including the line terminator.
    response->mSize = aMessage.Length();
    memcpy(response->mData, aMessage.Data(), aMessage.Length());
    gNetdConsumer->MessageReceived(response);
  }

  if (!responseCode) {
    NETD_LOG("Can't parse netd's response");
  }
}

void
NetdClient::OnFileCanWriteWithoutBlocking(int aFd)
{
  MOZ_ASSERT(aFd == mSocket.get());
  WriteNetdCommand();
}

void
NetdClient::OnError()
{
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());

  mReadWatcher.StopWatchingFileDescriptor();
  mWriteWatcher.StopWatchingFileDescriptor();

  mSocket.dispose();
  mCurrentWriteOffset = 0;
  mCurrentNetdCommand = nullptr;
  while (!mOutgoingQ.empty()) {
    delete mOutgoingQ.front();
    mOutgoingQ.pop();
  }
  Start();
}

// static
void
NetdClient::Start()
{
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());

  if (!gNetdClient) {
    NETD_LOG("Netd Client is not initialized");
    return;
  }

  if (!gNetdClient->OpenSocket()) {
    // Socket open failed, try again in a second.
    NETD_LOG("Fail to connect to Netd");
    if (++gNetdClient->mReConnectTimes > MAX_RECONNECT_TIMES) {
      NETD_LOG("Fail to connect to Netd after retry %d times", MAX_RECONNECT_TIMES);
      return;
    }

    MessageLoopForIO::current()->
      PostDelayedTask(FROM_HERE,
                      NewRunnableFunction(NetdClient::Start),
                      1000);
    return;
  }
  gNetdClient->mReConnectTimes = 0;
}

// static
void
NetdClient::SendNetdCommandIOThread(NetdCommand* aMessage)
{
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());
  MOZ_ASSERT(aMessage);

  if (!gNetdClient) {
    NETD_LOG("Netd Client is not initialized");
    return;
  }

  gNetdClient->mOutgoingQ.push(aMessage);

  if (gNetdClient->mSocket.get() == INVALID_SOCKET) {
    NETD_LOG("Netd connection is not established, push the message to queue");
    return;
  }

  gNetdClient->WriteNetdCommand();
}

void
NetdClient::WriteNetdCommand()
{
  if (!mCurrentNetdCommand) {
    mCurrentWriteOffset = 0;
    mCurrentNetdCommand = mOutgoingQ.front();
    mOutgoingQ.pop();
  }

  while (mCurrentWriteOffset < mCurrentNetdCommand->mSize) {
    ssize_t write_amount = mCurrentNetdCommand->mSize - mCurrentWriteOffset;
    ssize_t written = write(mSocket.get(),
                            mCurrentNetdCommand->mData + mCurrentWriteOffset,
                            write_amount);
    if (written < 0) {
      NETD_LOG("Cannot write to network, error %d\n", (int) written);
      OnError();
      return;
    }

    if (written > 0) {
      mCurrentWriteOffset += written;
    }

    if (written != write_amount) {
      NETD_LOG("WriteNetdCommand fail !!! Write is not completed");
      break;
    }
  }

  if (mCurrentWriteOffset != mCurrentNetdCommand->mSize) {
    MessageLoopForIO::current()->
      WatchFileDescriptor(mSocket.get(),
                          false,
                          MessageLoopForIO::WATCH_WRITE,
                          &mWriteWatcher,
                          this);
    return;
  }

  mCurrentNetdCommand = nullptr;
}

static void
InitNetdIOThread()
{
  bool result;
  char propValue[Property::VALUE_MAX_LENGTH];

  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());
  MOZ_ASSERT(!gNetdClient);

  Property::Get("ro.build.version.sdk", propValue, "0");
  // Assign rndis address for usb tethering in ICS.
  if (atoi(propValue) >= 15) {
    result = InitRndisAddress();
    // We don't return here because InitRnsisAddress() function is related to
    // usb tethering only. Others service such as wifi tethering still need
    // to use ipc to communicate with netd.
    if (!result) {
      NETD_LOG("fail to give rndis interface an address");
    }
  }
  gNetdClient = new NetdClient();
  gNetdClient->Start();
}

static void
ShutdownNetdIOThread()
{
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());
  nsCOMPtr<nsIRunnable> shutdownEvent = new StopNetdConsumer();

  gNetdClient = nullptr;

  NS_DispatchToMainThread(shutdownEvent);
}

void
StartNetd(NetdConsumer* aNetdConsumer)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aNetdConsumer);
  MOZ_ASSERT(gNetdConsumer == nullptr);

  gNetdConsumer = aNetdConsumer;
  XRE_GetIOMessageLoop()->PostTask(
    FROM_HERE,
    NewRunnableFunction(InitNetdIOThread));
}

void
StopNetd()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsIThread* currentThread = NS_GetCurrentThread();
  NS_ASSERTION(currentThread, "This should never be null!");

  XRE_GetIOMessageLoop()->PostTask(
    FROM_HERE,
    NewRunnableFunction(ShutdownNetdIOThread));

  while (gNetdConsumer) {
    if (!NS_ProcessNextEvent(currentThread)) {
      NS_WARNING("Something bad happened!");
      break;
    }
  }
}

/**************************************************************************
*
*   This function runs in net worker Thread context. The net worker thread
*   is created in dom/system/gonk/NetworkManager.js
*
**************************************************************************/
void
SendNetdCommand(NetdCommand* aMessage)
{
  MOZ_ASSERT(aMessage);

  XRE_GetIOMessageLoop()->PostTask(
    FROM_HERE,
    NewRunnableFunction(NetdClient::SendNetdCommandIOThread, aMessage));
}

} // namespace ipc
} // namespace mozilla
