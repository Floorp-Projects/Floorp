/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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

static void LogError(const char* what) {
  __android_log_print(ANDROID_LOG_ERROR, "Gecko", "%s: %s (%d)", what,
                      strerror(errno), errno);
}

SharedMemoryBasic::SharedMemoryBasic()
    : mShmFd(-1), mMemory(nullptr), mOpenRights(RightsReadWrite) {}

SharedMemoryBasic::~SharedMemoryBasic() {
  Unmap();
  CloseHandle();
}

bool SharedMemoryBasic::SetHandle(const Handle& aHandle, OpenRights aRights) {
  MOZ_ASSERT(-1 == mShmFd, "Already Create()d");
  mShmFd = aHandle.fd;
  mOpenRights = aRights;
  return true;
}

bool SharedMemoryBasic::Create(size_t aNbytes) {
  MOZ_ASSERT(-1 == mShmFd, "Already Create()d");

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

bool SharedMemoryBasic::Map(size_t nBytes, void* fixed_address) {
  MOZ_ASSERT(nullptr == mMemory, "Already Map()d");

  int prot = PROT_READ;
  if (mOpenRights == RightsReadWrite) {
    prot |= PROT_WRITE;
  }

  // Don't use MAP_FIXED when a fixed_address was specified, since that can
  // replace pages that are alread mapped at that address.
  mMemory = mmap(fixed_address, nBytes, prot, MAP_SHARED, mShmFd, 0);

  if (MAP_FAILED == mMemory) {
    if (!fixed_address) {
      LogError("ShmemAndroid::Map()");
    }
    mMemory = nullptr;
    return false;
  }

  if (fixed_address && mMemory != fixed_address) {
    if (munmap(mMemory, nBytes)) {
      LogError("ShmemAndroid::Map():unmap");
      mMemory = nullptr;
      return false;
    }
  }

  Mapped(nBytes);
  return true;
}

void* SharedMemoryBasic::FindFreeAddressSpace(size_t size) {
  void* memory =
      mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  munmap(memory, size);
  return memory != (void*)-1 ? memory : NULL;
}

bool SharedMemoryBasic::ShareToProcess(base::ProcessId /*unused*/,
                                       Handle* aNewHandle) {
  MOZ_ASSERT(mShmFd >= 0, "Should have been Create()d by now");

  int shmfdDup = dup(mShmFd);
  if (-1 == shmfdDup) {
    LogError("ShmemAndroid::ShareToProcess()");
    return false;
  }

  aNewHandle->fd = shmfdDup;
  aNewHandle->auto_close = true;
  return true;
}

void SharedMemoryBasic::Unmap() {
  if (!mMemory) {
    return;
  }

  if (munmap(mMemory, Size())) {
    LogError("ShmemAndroid::Unmap()");
  }
  mMemory = nullptr;
}

void SharedMemoryBasic::CloseHandle() {
  if (mShmFd != -1) {
    close(mShmFd);
    mShmFd = -1;
    mOpenRights = RightsReadWrite;
  }
}

}  // namespace ipc
}  // namespace mozilla
