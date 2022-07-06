/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPSharedMemManager.h"
#include "GMPMessageUtils.h"
#include "mozilla/ipc/SharedMemory.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/ClearOnShutdown.h"

namespace mozilla::gmp {

// Really one set of pools on each side of the plugin API.

// YUV buffers go from Encoder parent to child; pool there, and then return
// with Decoded() frames to the Decoder parent and goes into the parent pool.
// Compressed (encoded) data goes from the Decoder parent to the child;
// pool there, and then return with Encoded() frames and goes into the parent
// pool.
bool GMPSharedMemManager::MgrAllocShmem(GMPSharedMem::GMPMemoryClasses aClass,
                                        size_t aSize, ipc::Shmem* aMem) {
  mData->CheckThread();

  // first look to see if we have a free buffer large enough
  for (uint32_t i = 0; i < GetGmpFreelist(aClass).Length(); i++) {
    MOZ_ASSERT(GetGmpFreelist(aClass)[i].IsWritable());
    if (aSize <= GetGmpFreelist(aClass)[i].Size<uint8_t>()) {
      *aMem = GetGmpFreelist(aClass)[i];
      GetGmpFreelist(aClass).RemoveElementAt(i);
      return true;
    }
  }

  // Didn't find a buffer free with enough space; allocate one
  size_t pagesize = ipc::SharedMemory::SystemPageSize();
  aSize = (aSize + (pagesize - 1)) & ~(pagesize - 1);  // round up to page size
  bool retval = Alloc(aSize, aMem);
  if (retval) {
    // The allocator (or NeedsShmem call) should never return less than we ask
    // for...
    MOZ_ASSERT(aMem->Size<uint8_t>() >= aSize);
    mData->mGmpAllocated[aClass]++;
  }
  return retval;
}

bool GMPSharedMemManager::MgrDeallocShmem(GMPSharedMem::GMPMemoryClasses aClass,
                                          ipc::Shmem& aMem) {
  mData->CheckThread();

  size_t size = aMem.Size<uint8_t>();

  // XXX Bug NNNNNNN Until we put better guards on ipc::shmem, verify we
  // weren't fed an shmem we already had.
  for (uint32_t i = 0; i < GetGmpFreelist(aClass).Length(); i++) {
    if (NS_WARN_IF(aMem == GetGmpFreelist(aClass)[i])) {
      // Safest to crash in this case; should never happen in normal
      // operation.
      MOZ_CRASH("Deallocating Shmem we already have in our cache!");
      // return true;
    }
  }

  // XXX This works; there are better pool algorithms.  We need to avoid
  // "falling off a cliff" with too low a number
  if (GetGmpFreelist(aClass).Length() > 10) {
    Dealloc(std::move(GetGmpFreelist(aClass)[0]));
    GetGmpFreelist(aClass).RemoveElementAt(0);
    // The allocation numbers will be fubar on the Child!
    mData->mGmpAllocated[aClass]--;
  }
  for (uint32_t i = 0; i < GetGmpFreelist(aClass).Length(); i++) {
    MOZ_ASSERT(GetGmpFreelist(aClass)[i].IsWritable());
    if (size < GetGmpFreelist(aClass)[i].Size<uint8_t>()) {
      GetGmpFreelist(aClass).InsertElementAt(i, aMem);
      return true;
    }
  }
  GetGmpFreelist(aClass).AppendElement(aMem);

  return true;
}

uint32_t GMPSharedMemManager::NumInUse(GMPSharedMem::GMPMemoryClasses aClass) {
  return mData->mGmpAllocated[aClass] - GetGmpFreelist(aClass).Length();
}

}  // namespace mozilla::gmp
