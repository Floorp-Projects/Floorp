/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <math.h>

#include "nsString.h"
#include "nsIMemoryReporter.h"
#include "mozilla/ipc/SharedMemory.h"
#include "mozilla/Atomics.h"

namespace mozilla {
namespace ipc {

static Atomic<size_t> gShmemAllocated;
static Atomic<size_t> gShmemMapped;
static size_t GetShmemAllocated() { return gShmemAllocated; }
static size_t GetShmemMapped() { return gShmemMapped; }

NS_THREADSAFE_MEMORY_REPORTER_IMPLEMENT(ShmemAllocated,
  "shmem-allocated",
  KIND_OTHER,
  UNITS_BYTES,
  GetShmemAllocated,
  "Memory shared with other processes that is accessible (but not "
  "necessarily mapped).")

NS_THREADSAFE_MEMORY_REPORTER_IMPLEMENT(ShmemMapped,
  "shmem-mapped",
  KIND_OTHER,
  UNITS_BYTES,
  GetShmemMapped,
  "Memory shared with other processes that is mapped into the address space.")

SharedMemory::SharedMemory()
  : mAllocSize(0)
  , mMappedSize(0)
{
  static Atomic<uint32_t> registered;
  if (registered.compareExchange(0, 1)) {
    NS_RegisterMemoryReporter(new NS_MEMORY_REPORTER_NAME(ShmemAllocated));
    NS_RegisterMemoryReporter(new NS_MEMORY_REPORTER_NAME(ShmemMapped));
  }
}

/*static*/ size_t
SharedMemory::PageAlignedSize(size_t aSize)
{
  size_t pageSize = SystemPageSize();
  size_t nPagesNeeded = size_t(ceil(double(aSize) / double(pageSize)));
  return pageSize * nPagesNeeded;
}

void
SharedMemory::Created(size_t aNBytes)
{
  mAllocSize = aNBytes;
  gShmemAllocated += mAllocSize;
}

void
SharedMemory::Mapped(size_t aNBytes)
{
  mMappedSize = aNBytes;
  gShmemMapped += mMappedSize;
}

void
SharedMemory::Unmapped()
{
  NS_ABORT_IF_FALSE(gShmemMapped >= mMappedSize,
                    "Can't unmap more than mapped");
  gShmemMapped -= mMappedSize;
  mMappedSize = 0;
}

/*static*/ void
SharedMemory::Destroyed()
{
  NS_ABORT_IF_FALSE(gShmemAllocated >= mAllocSize,
                    "Can't destroy more than allocated");
  gShmemAllocated -= mAllocSize;
  mAllocSize = 0;
}

} // namespace ipc
} // namespace mozilla
