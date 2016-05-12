/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ISurfaceAllocator.h"

namespace mozilla {
namespace layers {

NS_IMPL_ISUPPORTS(GfxMemoryImageReporter, nsIMemoryReporter)

mozilla::Atomic<ptrdiff_t> GfxMemoryImageReporter::sAmount(0);

mozilla::ipc::SharedMemory::SharedMemoryType OptimalShmemType()
{
  return ipc::SharedMemory::SharedMemoryType::TYPE_BASIC;
}

// XXX - We should actually figure out the minimum shmem allocation size on
// a certain platform and use that.
const uint32_t sShmemPageSize = 4096;

#ifdef DEBUG
const uint32_t sSupportedBlockSize = 4;
#endif

FixedSizeSmallShmemSectionAllocator::FixedSizeSmallShmemSectionAllocator(ClientIPCAllocator* aShmProvider)
: mShmProvider(aShmProvider)
{
  MOZ_ASSERT(mShmProvider && mShmProvider->AsShmemAllocator());
}

FixedSizeSmallShmemSectionAllocator::~FixedSizeSmallShmemSectionAllocator()
{
  ShrinkShmemSectionHeap();
}

bool
FixedSizeSmallShmemSectionAllocator::AllocShmemSection(uint32_t aSize, ShmemSection* aShmemSection)
{
  // For now we only support sizes of 4. If we want to support different sizes
  // some more complicated bookkeeping should be added.
  MOZ_ASSERT(aSize == sSupportedBlockSize);
  MOZ_ASSERT(aShmemSection);

  if (!IPCOpen()) {
    gfxCriticalError() << "Attempt to allocate a ShmemSection after shutdown.";
    return false;
  }

  uint32_t allocationSize = (aSize + sizeof(ShmemSectionHeapAllocation));

  for (size_t i = 0; i < mUsedShmems.size(); i++) {
    ShmemSectionHeapHeader* header = mUsedShmems[i].get<ShmemSectionHeapHeader>();
    if ((header->mAllocatedBlocks + 1) * allocationSize + sizeof(ShmemSectionHeapHeader) < sShmemPageSize) {
      aShmemSection->shmem() = mUsedShmems[i];
      MOZ_ASSERT(mUsedShmems[i].IsWritable());
      break;
    }
  }

  if (!aShmemSection->shmem().IsWritable()) {
    ipc::Shmem tmp;
    if (!GetShmAllocator()->AllocUnsafeShmem(sShmemPageSize, OptimalShmemType(), &tmp)) {
      return false;
    }

    ShmemSectionHeapHeader* header = tmp.get<ShmemSectionHeapHeader>();
    header->mTotalBlocks = 0;
    header->mAllocatedBlocks = 0;

    mUsedShmems.push_back(tmp);
    aShmemSection->shmem() = tmp;
  }

  MOZ_ASSERT(aShmemSection->shmem().IsWritable());

  ShmemSectionHeapHeader* header = aShmemSection->shmem().get<ShmemSectionHeapHeader>();
  uint8_t* heap = aShmemSection->shmem().get<uint8_t>() + sizeof(ShmemSectionHeapHeader);

  ShmemSectionHeapAllocation* allocHeader = nullptr;

  if (header->mTotalBlocks > header->mAllocatedBlocks) {
    // Search for the first available block.
    for (size_t i = 0; i < header->mTotalBlocks; i++) {
      allocHeader = reinterpret_cast<ShmemSectionHeapAllocation*>(heap);

      if (allocHeader->mStatus == STATUS_FREED) {
        break;
      }
      heap += allocationSize;
    }
    MOZ_ASSERT(allocHeader && allocHeader->mStatus == STATUS_FREED);
    MOZ_ASSERT(allocHeader->mSize == sSupportedBlockSize);
  } else {
    heap += header->mTotalBlocks * allocationSize;

    header->mTotalBlocks++;
    allocHeader = reinterpret_cast<ShmemSectionHeapAllocation*>(heap);
    allocHeader->mSize = aSize;
  }

  MOZ_ASSERT(allocHeader);
  header->mAllocatedBlocks++;
  allocHeader->mStatus = STATUS_ALLOCATED;

  aShmemSection->size() = aSize;
  aShmemSection->offset() = (heap + sizeof(ShmemSectionHeapAllocation)) - aShmemSection->shmem().get<uint8_t>();
  ShrinkShmemSectionHeap();
  return true;
}

void
FixedSizeSmallShmemSectionAllocator::FreeShmemSection(mozilla::layers::ShmemSection& aShmemSection)
{
  MOZ_ASSERT(aShmemSection.size() == sSupportedBlockSize);
  MOZ_ASSERT(aShmemSection.offset() < sShmemPageSize - sSupportedBlockSize);

  if (!aShmemSection.shmem().IsWritable()) {
    return;
  }

  ShmemSectionHeapAllocation* allocHeader =
    reinterpret_cast<ShmemSectionHeapAllocation*>(aShmemSection.shmem().get<char>() +
                                                  aShmemSection.offset() -
                                                  sizeof(ShmemSectionHeapAllocation));

  MOZ_ASSERT(allocHeader->mSize == aShmemSection.size());

  DebugOnly<bool> success = allocHeader->mStatus.compareExchange(STATUS_ALLOCATED, STATUS_FREED);
  // If this fails something really weird is going on.
  MOZ_ASSERT(success);

  ShmemSectionHeapHeader* header = aShmemSection.shmem().get<ShmemSectionHeapHeader>();
  header->mAllocatedBlocks--;
}

void
FixedSizeSmallShmemSectionAllocator::DeallocShmemSection(mozilla::layers::ShmemSection& aShmemSection)
{
  if (!IPCOpen()) {
    gfxCriticalNote << "Attempt to dealloc a ShmemSections after shutdown.";
    return;
  }

  FreeShmemSection(aShmemSection);
  ShrinkShmemSectionHeap();
}


void
FixedSizeSmallShmemSectionAllocator::ShrinkShmemSectionHeap()
{
  if (!IPCOpen()) {
    mUsedShmems.clear();
    return;
  }

  // The loop will terminate as we either increase i, or decrease size
  // every time through.
  size_t i = 0;
  while (i < mUsedShmems.size()) {
    ShmemSectionHeapHeader* header = mUsedShmems[i].get<ShmemSectionHeapHeader>();
    if (header->mAllocatedBlocks == 0) {
      GetShmAllocator()->DeallocShmem(mUsedShmems[i]);
      // We don't particularly care about order, move the last one in the array
      // to this position.
      if (i < mUsedShmems.size() - 1) {
        mUsedShmems[i] = mUsedShmems[mUsedShmems.size() - 1];
      }
      mUsedShmems.pop_back();
    } else {
      i++;
    }
  }
}

} // namespace layers
} // namespace mozilla
