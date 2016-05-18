/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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

#include "HalLog.h"
#include "nsDebug.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/FileUtils.h"
#include "mozilla/Monitor.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

#include "UeventPoller.h"

using namespace mozilla;

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
    MOZ_CRASH("Must not write to netlink socket");
  }

  MessageLoopForIO *GetIOLoop () const { return mIOLoop; }
  void RegisterObserver(IUeventObserver *aObserver)
  {
    mUeventObserverList.AddObserver(aObserver);
  }

  void UnregisterObserver(IUeventObserver *aObserver)
  {
    mUeventObserverList.RemoveObserver(aObserver);
    if (mUeventObserverList.Length() == 0) {
      ShutdownUevent();  // this will destroy self
    }
  }

private:
  ScopedClose mSocket;
  MessageLoopForIO* mIOLoop;
  MessageLoopForIO::FileDescriptorWatcher mReadWatcher;

  const static int kBuffsize = 64 * 1024;
  uint8_t mBuffer [kBuffsize];

  typedef ObserverList<NetlinkEvent> UeventObserverList;
  UeventObserverList mUeventObserverList;
};

bool
NetlinkPoller::OpenSocket()
{
  mSocket.rwget() = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
  if (mSocket.get() < 0) {
    return false;
  }

  int sz = kBuffsize;

  if (setsockopt(mSocket.get(), SOL_SOCKET, SO_RCVBUFFORCE, &sz,
                 sizeof(sz)) < 0) {
    return false;
  }

  // add FD_CLOEXEC flag
  int flags = fcntl(mSocket.get(), F_GETFD);
  if (flags == -1) {
      return false;
  }
  flags |= FD_CLOEXEC;
  if (fcntl(mSocket.get(), F_SETFD, flags) == -1) {
    return false;
  }

  // set non-blocking
  if (fcntl(mSocket.get(), F_SETFL, O_NONBLOCK) == -1) {
    return false;
  }

  struct sockaddr_nl saddr;
  bzero(&saddr, sizeof(saddr));
  saddr.nl_family = AF_NETLINK;
  saddr.nl_groups = 1;
  saddr.nl_pid = gettid();

  do {
    if (bind(mSocket.get(), (struct sockaddr *)&saddr, sizeof(saddr)) == 0) {
      break;
    }

    if (errno != EADDRINUSE) {
      return false;
    }

    if (saddr.nl_pid == 0) {
      return false;
    }

    // Once there was any other place in the same process assigning saddr.nl_pid by
    // gettid(), we can detect it and print warning message.
    HAL_LOG("The netlink socket address saddr.nl_pid=%u is in use. "
            "Let the kernel re-assign.\n", saddr.nl_pid);
    saddr.nl_pid = 0;
  } while (true);

  if (!mIOLoop->WatchFileDescriptor(mSocket.get(),
                                    true,
                                    MessageLoopForIO::WATCH_READ,
                                    &mReadWatcher,
                                    this)) {
      return false;
  }

  return true;
}

static StaticAutoPtr<NetlinkPoller> sPoller;

class UeventInitTask : public Runnable
{
  NS_IMETHOD Run() override
  {
    if (!sPoller) {
      return NS_OK;
    }
    if (sPoller->OpenSocket()) {
      return NS_OK;
    }
    sPoller->GetIOLoop()->PostDelayedTask(MakeAndAddRef<UeventInitTask>(),
                                          1000);
    return NS_OK;
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
    NetlinkEvent netlinkEvent;
    netlinkEvent.decode(reinterpret_cast<char*>(mBuffer), ret);
    mUeventObserverList.Broadcast(netlinkEvent);
  }
}

static bool sShutdown = false;

class ShutdownNetlinkPoller;
static StaticAutoPtr<ShutdownNetlinkPoller> sShutdownPoller;
static Monitor* sMonitor = nullptr;

class ShutdownNetlinkPoller {
public:
  ~ShutdownNetlinkPoller()
  {
    // This is called from KillClearOnShutdown() on the main thread.
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(XRE_GetIOMessageLoop());

    {
      MonitorAutoLock lock(*sMonitor);

      XRE_GetIOMessageLoop()->PostTask(
          NewRunnableFunction(ShutdownUeventIOThread));

      while (!sShutdown) {
        lock.Wait();
      }
    }

    sShutdown = true;
    delete sMonitor;
  }

  static void MaybeInit()
  {
    MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());
    if (sShutdown || sMonitor) {
      // Don't init twice or init after shutdown.
      return;
    }

    sMonitor = new Monitor("ShutdownNetlinkPoller.monitor");
    {
      ShutdownNetlinkPoller* shutdownPoller = new ShutdownNetlinkPoller();

      nsCOMPtr<nsIRunnable> runnable = NS_NewRunnableFunction([=] () -> void
      {
        sShutdownPoller = shutdownPoller;
        ClearOnShutdown(&sShutdownPoller); // Must run on the main thread.
      });
      MOZ_ASSERT(runnable);
      MOZ_ALWAYS_SUCCEEDS(
        NS_DispatchToMainThread(runnable, NS_DISPATCH_NORMAL));
    }
  }
private:
  ShutdownNetlinkPoller() = default;
  static void ShutdownUeventIOThread()
  {
    MonitorAutoLock l(*sMonitor);
    ShutdownUevent(); // Must run on the IO thread.
    sShutdown = true;
    l.NotifyAll();
  }
};

static void
InitializeUevent()
{
  MOZ_ASSERT(!sPoller);
  sPoller = new NetlinkPoller();
  sPoller->GetIOLoop()->PostTask(MakeAndAddRef<UeventInitTask>());

  ShutdownNetlinkPoller::MaybeInit();
}

static void
ShutdownUevent()
{
  sPoller = nullptr;
}

void
RegisterUeventListener(IUeventObserver *aObserver)
{
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());

  if (sShutdown) {
    return;
  }

  if (!sPoller) {
    InitializeUevent();
  }
  sPoller->RegisterObserver(aObserver);
}

void
UnregisterUeventListener(IUeventObserver *aObserver)
{
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());

  if (sShutdown) {
    return;
  }

  sPoller->UnregisterObserver(aObserver);
}

} // hal_impl
} // mozilla

