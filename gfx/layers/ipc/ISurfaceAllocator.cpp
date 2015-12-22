/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ISurfaceAllocator.h"
#include <sys/types.h>                  // for int32_t
#include "gfx2DGlue.h"                  // for IntSize
#include "gfxPlatform.h"                // for gfxPlatform, gfxImageFormat
#include "gfxSharedImageSurface.h"      // for gfxSharedImageSurface
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/Atomics.h"            // for PrimitiveIntrinsics
#include "mozilla/ipc/SharedMemory.h"   // for SharedMemory, etc
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor, etc
#include "mozilla/layers/SharedBufferManagerChild.h"
#include "ShadowLayerUtils.h"
#include "mozilla/mozalloc.h"           // for operator delete[], etc
#include "nsAutoPtr.h"                  // for nsRefPtr, getter_AddRefs, etc
#include "nsDebug.h"                    // for NS_RUNTIMEABORT
#include "nsXULAppAPI.h"                // for XRE_GetProcessType, etc
#include "mozilla/ipc/Shmem.h"
#include "mozilla/layers/ImageDataSerializer.h"
#ifdef DEBUG
#include "prenv.h"
#endif

using namespace mozilla::ipc;

namespace mozilla {
namespace layers {

NS_IMPL_ISUPPORTS(GfxMemoryImageReporter, nsIMemoryReporter)

mozilla::Atomic<size_t> GfxMemoryImageReporter::sAmount(0);

mozilla::ipc::SharedMemory::SharedMemoryType OptimalShmemType()
{
  return mozilla::ipc::SharedMemory::TYPE_BASIC;
}

bool
IsSurfaceDescriptorValid(const SurfaceDescriptor& aSurface)
{
  return aSurface.type() != SurfaceDescriptor::T__None &&
         aSurface.type() != SurfaceDescriptor::Tnull_t;
}

ISurfaceAllocator::~ISurfaceAllocator()
{
  // Check if we're not leaking..
  MOZ_ASSERT(mUsedShmems.empty());
}

void
ISurfaceAllocator::Finalize()
{
  ShrinkShmemSectionHeap();
}

static inline uint8_t*
GetAddressFromDescriptor(const SurfaceDescriptor& aDescriptor)
{
  MOZ_ASSERT(IsSurfaceDescriptorValid(aDescriptor));
  MOZ_RELEASE_ASSERT(aDescriptor.type() == SurfaceDescriptor::TSurfaceDescriptorBuffer);

  auto memOrShmem = aDescriptor.get_SurfaceDescriptorBuffer().data();
  if (memOrShmem.type() == MemoryOrShmem::TShmem) {
    return memOrShmem.get_Shmem().get<uint8_t>();
  } else {
    return reinterpret_cast<uint8_t*>(memOrShmem.get_uintptr_t());
  }
}

already_AddRefed<gfx::DrawTarget>
GetDrawTargetForDescriptor(const SurfaceDescriptor& aDescriptor, gfx::BackendType aBackend)
{
  uint8_t* data = GetAddressFromDescriptor(aDescriptor);
  auto rgb = aDescriptor.get_SurfaceDescriptorBuffer().desc().get_RGBDescriptor();
  uint32_t stride = ImageDataSerializer::GetRGBStride(rgb);
  return gfx::Factory::CreateDrawTargetForData(gfx::BackendType::CAIRO,
                                               data, rgb.size(),
                                               stride, rgb.format());
}

already_AddRefed<gfx::DataSourceSurface>
GetSurfaceForDescriptor(const SurfaceDescriptor& aDescriptor)
{
  uint8_t* data = GetAddressFromDescriptor(aDescriptor);
  auto rgb = aDescriptor.get_SurfaceDescriptorBuffer().desc().get_RGBDescriptor();
  uint32_t stride = ImageDataSerializer::GetRGBStride(rgb);
  return gfx::Factory::CreateWrappingDataSourceSurface(data, stride, rgb.size(),
                                                       rgb.format());
}

bool
ISurfaceAllocator::AllocSurfaceDescriptor(const gfx::IntSize& aSize,
                                          gfxContentType aContent,
                                          SurfaceDescriptor* aBuffer)
{
  if (!IPCOpen()) {
    return false;
  }
  return AllocSurfaceDescriptorWithCaps(aSize, aContent, DEFAULT_BUFFER_CAPS, aBuffer);
}

bool
ISurfaceAllocator::AllocSurfaceDescriptorWithCaps(const gfx::IntSize& aSize,
                                                  gfxContentType aContent,
                                                  uint32_t aCaps,
                                                  SurfaceDescriptor* aBuffer)
{
  if (!IPCOpen()) {
    return false;
  }
  gfx::SurfaceFormat format =
    gfxPlatform::GetPlatform()->Optimal2DFormatForContent(aContent);
  size_t size = ImageDataSerializer::ComputeRGBBufferSize(aSize, format);
  if (!size) {
    return false;
  }

  MemoryOrShmem bufferDesc;
  if (IsSameProcess()) {
    uint8_t* data = new (std::nothrow) uint8_t[size];
    if (!data) {
      return false;
    }
    GfxMemoryImageReporter::DidAlloc(data);
#ifdef XP_MACOSX
    // Workaround a bug in Quartz where drawing an a8 surface to another a8
    // surface with OP_SOURCE still requires the destination to be clear.
    if (format == gfx::SurfaceFormat::A8) {
      memset(data, 0, size);
    }
#endif
    bufferDesc = reinterpret_cast<uintptr_t>(data);
  } else {

    mozilla::ipc::SharedMemory::SharedMemoryType shmemType = OptimalShmemType();
    mozilla::ipc::Shmem shmem;
    if (!AllocUnsafeShmem(size, shmemType, &shmem)) {
      return false;
    }

    bufferDesc = shmem;
  }

  *aBuffer = SurfaceDescriptorBuffer(RGBDescriptor(aSize, format), bufferDesc);

  return true;
}

/* static */ bool
ISurfaceAllocator::IsShmem(SurfaceDescriptor* aSurface)
{
  return aSurface && (aSurface->type() == SurfaceDescriptor::TSurfaceDescriptorBuffer)
      && (aSurface->get_SurfaceDescriptorBuffer().data().type() == MemoryOrShmem::TShmem);
}

void
ISurfaceAllocator::DestroySharedSurface(SurfaceDescriptor* aSurface)
{
  MOZ_ASSERT(IPCOpen());
  if (!IPCOpen()) {
    return;
  }

  MOZ_ASSERT(aSurface);
  if (!aSurface) {
    return;
  }
  if (!IPCOpen()) {
    return;
  }
  SurfaceDescriptorBuffer& desc = aSurface->get_SurfaceDescriptorBuffer();
  switch (desc.data().type()) {
    case MemoryOrShmem::TShmem: {
      DeallocShmem(desc.data().get_Shmem());
      break;
    }
    case MemoryOrShmem::Tuintptr_t: {
      uint8_t* ptr = (uint8_t*)desc.data().get_uintptr_t();
      GfxMemoryImageReporter::WillFree(ptr);
      delete [] ptr;
      break;
    }
    default:
      NS_RUNTIMEABORT("surface type not implemented!");
  }
  *aSurface = SurfaceDescriptor();
}

// XXX - We should actually figure out the minimum shmem allocation size on
// a certain platform and use that.
const uint32_t sShmemPageSize = 4096;

#ifdef DEBUG
const uint32_t sSupportedBlockSize = 4;
#endif

enum AllocationStatus
{
  STATUS_ALLOCATED,
  STATUS_FREED
};

struct ShmemSectionHeapHeader
{
  Atomic<uint32_t> mTotalBlocks;
  Atomic<uint32_t> mAllocatedBlocks;
};

struct ShmemSectionHeapAllocation
{
  Atomic<uint32_t> mStatus;
  uint32_t mSize;
};

bool
ISurfaceAllocator::AllocShmemSection(size_t aSize, mozilla::layers::ShmemSection* aShmemSection)
{
  MOZ_ASSERT(IPCOpen());
  if (!IPCOpen()) {
    return false;
  }
  // For now we only support sizes of 4. If we want to support different sizes
  // some more complicated bookkeeping should be added.
  MOZ_ASSERT(aSize == sSupportedBlockSize);
  MOZ_ASSERT(aShmemSection);

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
    if (!AllocUnsafeShmem(sShmemPageSize, ipc::SharedMemory::TYPE_BASIC, &tmp)) {
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
ISurfaceAllocator::FreeShmemSection(mozilla::layers::ShmemSection& aShmemSection)
{
  MOZ_ASSERT(IPCOpen());
  if (!IPCOpen()) {
    return;
  }

  MOZ_ASSERT(aShmemSection.size() == sSupportedBlockSize);
  MOZ_ASSERT(aShmemSection.offset() < sShmemPageSize - sSupportedBlockSize);

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

  ShrinkShmemSectionHeap();
}


void
ISurfaceAllocator::ShrinkShmemSectionHeap()
{
  if (!IPCOpen()) {
    return;
  }

  // The loop will terminate as we either increase i, or decrease size
  // every time through.
  size_t i = 0;
  while (i < mUsedShmems.size()) {
    ShmemSectionHeapHeader* header = mUsedShmems[i].get<ShmemSectionHeapHeader>();
    if (header->mAllocatedBlocks == 0) {
      DeallocShmem(mUsedShmems[i]);

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

bool
ISurfaceAllocator::AllocGrallocBuffer(const gfx::IntSize& aSize,
                                      uint32_t aFormat,
                                      uint32_t aUsage,
                                      MaybeMagicGrallocBufferHandle* aHandle)
{
  MOZ_ASSERT(IPCOpen());
  if (!IPCOpen()) {
    return false;
  }

  return SharedBufferManagerChild::GetSingleton()->AllocGrallocBuffer(aSize, aFormat, aUsage, aHandle);
}

void
ISurfaceAllocator::DeallocGrallocBuffer(MaybeMagicGrallocBufferHandle* aHandle)
{
  MOZ_ASSERT(IPCOpen());
  if (!IPCOpen()) {
    return;
  }

  SharedBufferManagerChild::GetSingleton()->DeallocGrallocBuffer(*aHandle);
}

void
ISurfaceAllocator::DropGrallocBuffer(MaybeMagicGrallocBufferHandle* aHandle)
{
  MOZ_ASSERT(IPCOpen());
  if (!IPCOpen()) {
    return;
  }

  SharedBufferManagerChild::GetSingleton()->DropGrallocBuffer(*aHandle);
}

} // namespace layers
} // namespace mozilla
