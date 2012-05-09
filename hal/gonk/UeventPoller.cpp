/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <linux/types.h>
#include <linux/netlink.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "base/message_loop.h"
#include "mozilla/FileUtils.h"
#include "nsAutoPtr.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

#include "UeventPoller.h"

namespace mozilla {
namespace hal_impl {

static void ShutdownUevent();

class NetlinkPoller : public MessageLoopForIO::Watcher
{
public:
  NetlinkPoller() : mSocket(-1),
                    mIOLoop(MessageLoopForIO::current())
  {
  }

  virtual ~NetlinkPoller() {}

  bool OpenSocket();

  virtual void OnFileCanReadWithoutBlocking(int fd);

  // no writing to the netlink socket
  virtual void OnFileCanWriteWithoutBlocking(int fd)
  {
    MOZ_NOT_REACHED("Must not write to netlink socket");
  }

  MessageLoopForIO *GetIOLoop () const { return mIOLoop; }
  void RegisterObserver(IUeventObserver *aObserver)
  {
    mUeventObserverList.AddObserver(aObserver);
  }

  void UnregisterObserver(IUeventObserver *aObserver)
  {
    mUeventObserverList.RemoveObserver(aObserver);
    if (mUeventObserverList.Length() == 0)
      ShutdownUevent();  // this will destroy self
  }

private:
  ScopedClose mSocket;
  MessageLoopForIO* mIOLoop;
  MessageLoopForIO::FileDescriptorWatcher mReadWatcher;

  NetlinkEvent mNetlinkEvent;
  const static int kBuffsize = 64 * 1024;
  uint8_t mBuffer [kBuffsize];

  typedef ObserverList<NetlinkEvent> UeventObserverList;
  UeventObserverList mUeventObserverList;
};

bool
NetlinkPoller::OpenSocket()
{
  mSocket.rwget() = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
  if (mSocket.get() < 0)
    return false;

  int sz = kBuffsize;

  if (setsockopt(mSocket.get(), SOL_SOCKET, SO_RCVBUFFORCE, &sz, sizeof(sz)) < 0)
    return false;

  // add FD_CLOEXEC flag
  int flags = fcntl(mSocket.get(), F_GETFD);
  if (flags == -1) {
      return false;
  }
  flags |= FD_CLOEXEC;
  if (fcntl(mSocket.get(), F_SETFD, flags) == -1)
    return false;

  // set non-blocking
  if (fcntl(mSocket.get(), F_SETFL, O_NONBLOCK) == -1)
    return false;

  struct sockaddr_nl saddr;
  bzero(&saddr, sizeof(saddr));
  saddr.nl_family = AF_NETLINK;
  saddr.nl_groups = 1;
  saddr.nl_pid = getpid();

  if (bind(mSocket.get(), (struct sockaddr *)&saddr, sizeof(saddr)) == -1)
    return false;

  if (!mIOLoop->WatchFileDescriptor(mSocket.get(),
                                    true,
                                    MessageLoopForIO::WATCH_READ,
                                    &mReadWatcher,
                                    this)) {
      return false;
  }

  return true;
}

static nsAutoPtr<NetlinkPoller> sPoller;

class UeventInitTask : public Task
{
  virtual void Run()
  {
    if (!sPoller) {
      return;
    }
    if (sPoller->OpenSocket()) {
      return;
    }
    sPoller->GetIOLoop()->PostDelayedTask(FROM_HERE, new UeventInitTask(), 1000);
  }
};

void
NetlinkPoller::OnFileCanReadWithoutBlocking(int fd)
{
  MOZ_ASSERT(fd == mSocket.get());
  while (true) {
    int ret = read(fd, mBuffer, kBuffsize);
    if (ret == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return;
      }
      if (errno == EINTR) {
        continue;
      }
    }
    if (ret <= 0) {
      // fatal error on netlink socket which should not happen
      _exit(1);
    }
    mNetlinkEvent.decode(reinterpret_cast<char*>(mBuffer), ret);
    mUeventObserverList.Broadcast(mNetlinkEvent);
  }
}

static void
InitializeUevent()
{
  MOZ_ASSERT(!sPoller);
  sPoller = new NetlinkPoller();
  sPoller->GetIOLoop()->PostTask(FROM_HERE, new UeventInitTask());

}

static void
ShutdownUevent()
{
  sPoller = NULL;
}

void
RegisterUeventListener(IUeventObserver *aObserver)
{
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());

  if (!sPoller)
    InitializeUevent();
  sPoller->RegisterObserver(aObserver);
}

void
UnregisterUeventListener(IUeventObserver *aObserver)
{
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());

  sPoller->UnregisterObserver(aObserver);
}

} // hal_impl
} // mozilla

