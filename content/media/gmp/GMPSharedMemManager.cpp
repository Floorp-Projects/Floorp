/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPSharedMemManager.h"
#include "GMPMessageUtils.h"
#include "mozilla/ipc/SharedMemory.h"

namespace mozilla {
namespace gmp {

// Really one set of pools on each side of the plugin API.

// YUV buffers go from Encoder parent to child; pool there, and then return
// with Decoded() frames to the Decoder parent and goes into the parent pool.
// Compressed (encoded) data goes from the Decoder parent to the child;
// pool there, and then return with Encoded() frames and goes into the parent
// pool.
static nsTArray<ipc::Shmem> sGmpFreelist[GMPSharedMemManager::kGMPNumTypes];
static uint32_t sGmpAllocated[GMPSharedMemManager::kGMPNumTypes]; // 0's

bool
GMPSharedMemManager::MgrAllocShmem(GMPMemoryClasses aClass, size_t aSize,
                                   ipc::Shmem::SharedMemory::SharedMemoryType aType,
                                   ipc::Shmem* aMem)
{
  CheckThread();

  // first look to see if we have a free buffer large enough
  for (uint32_t i = 0; i < sGmpFreelist[aClass].Length(); i++) {
    MOZ_ASSERT(sGmpFreelist[aClass][i].IsWritable());
    if (aSize <= sGmpFreelist[aClass][i].Size<uint8_t>()) {
      *aMem = sGmpFreelist[aClass][i];
      sGmpFreelist[aClass].RemoveElementAt(i);
      return true;
    }
  }

  // Didn't find a buffer free with enough space; allocate one
  size_t pagesize = ipc::SharedMemory::SystemPageSize();
  aSize = (aSize + (pagesize-1)) & ~(pagesize-1); // round up to page size
  bool retval = Alloc(aSize, aType, aMem);
  if (retval) {
    sGmpAllocated[aClass]++;
  }
  return retval;
}

bool
GMPSharedMemManager::MgrDeallocShmem(GMPMemoryClasses aClass, ipc::Shmem& aMem)
{
  CheckThread();

  size_t size = aMem.Size<uint8_t>();
  size_t total = 0;
  // XXX This works; there are better pool algorithms.  We need to avoid
  // "falling off a cliff" with too low a number
  if (sGmpFreelist[aClass].Length() > 10) {
    Dealloc(sGmpFreelist[aClass][0]);
    sGmpFreelist[aClass].RemoveElementAt(0);
    // The allocation numbers will be fubar on the Child!
    sGmpAllocated[aClass]--;
  }
  for (uint32_t i = 0; i < sGmpFreelist[aClass].Length(); i++) {
    MOZ_ASSERT(sGmpFreelist[aClass][i].IsWritable());
    total += sGmpFreelist[aClass][i].Size<uint8_t>();
    if (size < sGmpFreelist[aClass][i].Size<uint8_t>()) {
      sGmpFreelist[aClass].InsertElementAt(i, aMem);
      return true;
    }
  }
  sGmpFreelist[aClass].AppendElement(aMem);

  return true;
}

uint32_t
GMPSharedMemManager::NumInUse(GMPMemoryClasses aClass)
{
  return sGmpAllocated[aClass] - sGmpFreelist[aClass].Length();
}

}
}
