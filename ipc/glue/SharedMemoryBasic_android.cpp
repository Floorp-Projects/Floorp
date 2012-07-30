/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <android/log.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/process_util.h"

#include "SharedMemoryBasic.h"

//
// Temporarily go directly to the kernel interface until we can
// interact better with libcutils.
//
#include <linux/ashmem.h>

namespace mozilla {
namespace ipc {

static void
LogError(const char* what)
{
  __android_log_print(ANDROID_LOG_ERROR, "Gecko",
                      "%s: %s (%d)", what, strerror(errno), errno);
}

SharedMemoryBasic::SharedMemoryBasic()
  : mShmFd(-1)
  , mMemory(nullptr)
{ }

SharedMemoryBasic::SharedMemoryBasic(const Handle& aHandle)
  : mShmFd(aHandle.fd)
  , mMemory(nullptr)
{ }

SharedMemoryBasic::~SharedMemoryBasic()
{
  Unmap();
  Destroy();
}

bool
SharedMemoryBasic::Create(size_t aNbytes)
{
  NS_ABORT_IF_FALSE(-1 == mShmFd, "Already Create()d");

  // Carve a new instance off of /dev/ashmem
  int shmfd = open("/" ASHMEM_NAME_DEF, O_RDWR, 0600);
  if (-1 == shmfd) {
    LogError("ShmemAndroid::Create():open");
    return false;
  }

  if (ioctl(shmfd, ASHMEM_SET_SIZE, aNbytes)) {
    LogError("ShmemAndroid::Unmap():ioctl(SET_SIZE)");
    close(shmfd);
    return false;
  }

  mShmFd = shmfd;
  Created(aNbytes);
  return true;
}

bool
SharedMemoryBasic::Map(size_t nBytes)
{
  NS_ABORT_IF_FALSE(nullptr == mMemory, "Already Map()d");

  mMemory = mmap(nullptr, nBytes,
                 PROT_READ | PROT_WRITE,
                 MAP_SHARED,
                 mShmFd,
                 0);
  if (MAP_FAILED == mMemory) {
    LogError("ShmemAndroid::Map()");
    mMemory = nullptr;
    return false;
  }

  Mapped(nBytes);
  return true;
}

bool
SharedMemoryBasic::ShareToProcess(base::ProcessHandle/*unused*/,
                                  Handle* aNewHandle)
{
  NS_ABORT_IF_FALSE(mShmFd >= 0, "Should have been Create()d by now");

  int shmfdDup = dup(mShmFd);
  if (-1 == shmfdDup) {
    LogError("ShmemAndroid::ShareToProcess()");
    return false;
  }

  aNewHandle->fd = shmfdDup;
  aNewHandle->auto_close = true;
  return true;
}

void
SharedMemoryBasic::Unmap()
{
  if (!mMemory) {
    return;
  }

  if (munmap(mMemory, Size())) {
    LogError("ShmemAndroid::Unmap()");
  }
  mMemory = nullptr;
}

void
SharedMemoryBasic::Destroy()
{
  if (mShmFd > 0) {
    close(mShmFd);
  }
}

} // namespace ipc
} // namespace mozilla
