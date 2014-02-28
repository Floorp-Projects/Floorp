/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <fcntl.h>
#include "UnixFdWatcher.h"

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

UnixFdWatcher::~UnixFdWatcher()
{
  NS_WARN_IF(IsOpen()); /* mFd should have been closed already */
}

void
UnixFdWatcher::Close()
{
  MOZ_ASSERT(MessageLoopForIO::current() == mIOLoop);

  if (NS_WARN_IF(!IsOpen())) {
    /* mFd should have been open */
    return;
  }
  OnClose();
  RemoveWatchers(READ_WATCHER|WRITE_WATCHER);
  mFd.dispose();
}

void
UnixFdWatcher::AddWatchers(unsigned long aWatchers, bool aPersistent)
{
  MOZ_ASSERT(MessageLoopForIO::current() == mIOLoop);
  MOZ_ASSERT(IsOpen());

  // Before we add a watcher, we need to remove it! Removing is always
  // safe, but adding the same watcher twice can lead to endless loops
  // inside libevent.
  RemoveWatchers(aWatchers);

  if (aWatchers & READ_WATCHER) {
    MessageLoopForIO::current()->WatchFileDescriptor(
      mFd,
      aPersistent,
      MessageLoopForIO::WATCH_READ,
      &mReadWatcher,
      this);
  }
  if (aWatchers & WRITE_WATCHER) {
    MessageLoopForIO::current()->WatchFileDescriptor(
      mFd,
      aPersistent,
      MessageLoopForIO::WATCH_WRITE,
      &mWriteWatcher,
      this);
  }
}

void
UnixFdWatcher::RemoveWatchers(unsigned long aWatchers)
{
  MOZ_ASSERT(MessageLoopForIO::current() == mIOLoop);
  MOZ_ASSERT(IsOpen());

  if (aWatchers & READ_WATCHER) {
    mReadWatcher.StopWatchingFileDescriptor();
  }
  if (aWatchers & WRITE_WATCHER) {
    mWriteWatcher.StopWatchingFileDescriptor();
  }
}

void
UnixFdWatcher::OnError(const char* aFunction, int aErrno)
{
  MOZ_ASSERT(MessageLoopForIO::current() == mIOLoop);

  CHROMIUM_LOG("%s failed with error %d (%s)",
               aFunction, aErrno, strerror(aErrno));
}

UnixFdWatcher::UnixFdWatcher(MessageLoop* aIOLoop)
: mIOLoop(aIOLoop)
{
  MOZ_ASSERT(mIOLoop);
}

UnixFdWatcher::UnixFdWatcher(MessageLoop* aIOLoop, int aFd)
: mIOLoop(aIOLoop)
, mFd(aFd)
{
  MOZ_ASSERT(mIOLoop);
}

void
UnixFdWatcher::SetFd(int aFd)
{
  MOZ_ASSERT(MessageLoopForIO::current() == mIOLoop);
  MOZ_ASSERT(!IsOpen());
  MOZ_ASSERT(FdIsNonBlocking(aFd));

  mFd = aFd;
}

bool
UnixFdWatcher::FdIsNonBlocking(int aFd)
{
  int flags = TEMP_FAILURE_RETRY(fcntl(aFd, F_GETFL));
  return (flags > 0) && (flags & O_NONBLOCK);
}

}
}
