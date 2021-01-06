/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <sys/mman.h>  // mprotect
#include <unistd.h>    // sysconf

#include "mozilla/ipc/SharedMemory.h"

#if defined(XP_MACOSX) && defined(__x86_64__)
#  include "prenv.h"
#endif

namespace mozilla {
namespace ipc {

#if defined(XP_MACOSX) && defined(__x86_64__)
std::atomic<size_t> sPageSizeOverride = 0;
#endif

void SharedMemory::SystemProtect(char* aAddr, size_t aSize, int aRights) {
  if (!SystemProtectFallible(aAddr, aSize, aRights)) {
    MOZ_CRASH("can't mprotect()");
  }
}

bool SharedMemory::SystemProtectFallible(char* aAddr, size_t aSize,
                                         int aRights) {
  int flags = 0;
  if (aRights & RightsRead) flags |= PROT_READ;
  if (aRights & RightsWrite) flags |= PROT_WRITE;
  if (RightsNone == aRights) flags = PROT_NONE;

  return 0 == mprotect(aAddr, aSize, flags);
}

size_t SharedMemory::SystemPageSize() {
#if defined(XP_MACOSX) && defined(__x86_64__)
  if (sPageSizeOverride == 0) {
    if (PR_GetEnv("MOZ_SHMEM_PAGESIZE_16K")) {
      sPageSizeOverride = 16 * 1024;
    } else {
      sPageSizeOverride = sysconf(_SC_PAGESIZE);
    }
  }
  return sPageSizeOverride;
#else
  return sysconf(_SC_PAGESIZE);
#endif
}

}  // namespace ipc
}  // namespace mozilla
