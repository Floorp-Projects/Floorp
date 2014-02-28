/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <fcntl.h>
#include "UnixFileWatcher.h"

namespace mozilla {
namespace ipc {

UnixFileWatcher::~UnixFileWatcher()
{
}

nsresult
UnixFileWatcher::Open(const char* aFilename, int aFlags, mode_t aMode)
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());
  MOZ_ASSERT(aFlags & O_NONBLOCK);

  int fd = TEMP_FAILURE_RETRY(open(aFilename, aFlags, aMode));
  if (fd < 0) {
    OnError("open", errno);
    return NS_ERROR_FAILURE;
  }
  SetFd(fd);
  OnOpened();

  return NS_OK;
}

UnixFileWatcher::UnixFileWatcher(MessageLoop* aIOLoop)
: UnixFdWatcher(aIOLoop)
{
}

UnixFileWatcher::UnixFileWatcher(MessageLoop* aIOLoop, int aFd)
: UnixFdWatcher(aIOLoop, aFd)
{
}

}
}
