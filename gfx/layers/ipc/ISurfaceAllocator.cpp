/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ISurfaceAllocator.h"
#include <sys/types.h>                  // for int32_t
#include "gfx2DGlue.h"                  // for IntSize
#include "gfxASurface.h"                // for gfxASurface, etc
#include "gfxPlatform.h"                // for gfxPlatform, gfxImageFormat
#include "gfxSharedImageSurface.h"      // for gfxSharedImageSurface
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/Atomics.h"            // for PrimitiveIntrinsics
#include "mozilla/ipc/SharedMemory.h"   // for SharedMemory, etc
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor, etc
#include "ShadowLayerUtils.h"
#include "mozilla/mozalloc.h"           // for operator delete[], etc
#include "nsAutoPtr.h"                  // for nsRefPtr, getter_AddRefs, etc
#include "nsDebug.h"                    // for NS_RUNTIMEABORT
#include "nsXULAppAPI.h"                // for XRE_GetProcessType, etc
#ifdef DEBUG
#include "prenv.h"
#endif

using namespace mozilla::ipc;

namespace mozilla {
namespace layers {

NS_IMPL_ISUPPORTS1(GfxMemoryImageReporter, nsIMemoryReporter)

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

bool
ISurfaceAllocator::AllocSharedImageSurface(const gfx::IntSize& aSize,
                               gfxContentType aContent,
                               gfxSharedImageSurface** aBuffer)
{
  mozilla::ipc::SharedMemory::SharedMemoryType shmemType = OptimalShmemType();
  gfxImageFormat format = gfxPlatform::GetPlatform()->OptimalFormatForContent(aContent);

  nsRefPtr<gfxSharedImageSurface> back =
    gfxSharedImageSurface::CreateUnsafe(this,
                                        gfx::ThebesIntSize(aSize),
                                        format,
                                        shmemType);
  if (!back)
    return false;

  *aBuffer = nullptr;
  back.swap(*aBuffer);
  return true;
}

bool
ISurfaceAllocator::AllocSurfaceDescriptor(const gfx::IntSize& aSize,
                                          gfxContentType aContent,
                                          SurfaceDescriptor* aBuffer)
{
  return AllocSurfaceDescriptorWithCaps(aSize, aContent, DEFAULT_BUFFER_CAPS, aBuffer);
}

bool
ISurfaceAllocator::AllocSurfaceDescriptorWithCaps(const gfx::IntSize& aSize,
                                                  gfxContentType aContent,
                                                  uint32_t aCaps,
                                                  SurfaceDescriptor* aBuffer)
{
  bool tryPlatformSurface = true;
#ifdef DEBUG
  tryPlatformSurface = !PR_GetEnv("MOZ_LAYERS_FORCE_SHMEM_SURFACES");
#endif
  if (tryPlatformSurface &&
      PlatformAllocSurfaceDescriptor(aSize, aContent, aCaps, aBuffer)) {
    return true;
  }

  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    gfxImageFormat format =
      gfxPlatform::GetPlatform()->OptimalFormatForContent(aContent);
    int32_t stride = gfxASurface::FormatStrideForWidth(format, aSize.width);
    uint8_t *data = new (std::nothrow) uint8_t[stride * aSize.height];
    if (!data) {
      return false;
    }
    GfxMemoryImageReporter::DidAlloc(data);
#ifdef XP_MACOSX
    // Workaround a bug in Quartz where drawing an a8 surface to another a8
    // surface with OPERATOR_SOURCE still requires the destination to be clear.
    if (format == gfxImageFormat::A8) {
      memset(data, 0, stride * aSize.height);
    }
#endif
    *aBuffer = MemoryImage((uintptr_t)data, aSize, stride, format);
    return true;
  }

  nsRefPtr<gfxSharedImageSurface> buffer;
  if (!AllocSharedImageSurface(aSize, aContent,
                               getter_AddRefs(buffer))) {
    return false;
  }

  *aBuffer = buffer->GetShmem();
  return true;
}

/* static */ bool
ISurfaceAllocator::IsShmem(SurfaceDescriptor* aSurface)
{
  return aSurface && (aSurface->type() == SurfaceDescriptor::TShmem ||
                      aSurface->type() == SurfaceDescriptor::TYCbCrImage ||
                      aSurface->type() == SurfaceDescriptor::TRGBImage);
}

void
ISurfaceAllocator::DestroySharedSurface(SurfaceDescriptor* aSurface)
{
  MOZ_ASSERT(aSurface);
  if (!aSurface) {
    return;
  }
  if (!IPCOpen()) {
    return;
  }
  if (PlatformDestroySharedSurface(aSurface)) {
    return;
  }
  switch (aSurface->type()) {
    case SurfaceDescriptor::TShmem:
      DeallocShmem(aSurface->get_Shmem());
      break;
    case SurfaceDescriptor::TYCbCrImage:
      DeallocShmem(aSurface->get_YCbCrImage().data());
      break;
    case SurfaceDescriptor::TRGBImage:
      DeallocShmem(aSurface->get_RGBImage().data());
      break;
    case SurfaceDescriptor::TSurfaceDescriptorD3D9:
    case SurfaceDescriptor::TSurfaceDescriptorDIB:
    case SurfaceDescriptor::TSurfaceDescriptorD3D10:
      break;
    case SurfaceDescriptor::TMemoryImage:
      GfxMemoryImageReporter::WillFree((uint8_t*)aSurface->get_MemoryImage().data());
      delete [] (uint8_t*)aSurface->get_MemoryImage().data();
      break;
    case SurfaceDescriptor::Tnull_t:
    case SurfaceDescriptor::T__None:
      break;
    default:
      NS_RUNTIMEABORT("surface type not implemented!");
  }
  *aSurface = SurfaceDescriptor();
}

#if !defined(MOZ_HAVE_PLATFORM_SPECIFIC_LAYER_BUFFERS)
bool
ISurfaceAllocator::PlatformAllocSurfaceDescriptor(const gfx::IntSize&,
                                                  gfxContentType,
                                                  uint32_t,
                                                  SurfaceDescriptor*)
{
  return false;
}
#endif

// XXX - We should actually figure out the minimum shmem allocation size on
// a certain platform and use that.
const uint32_t sShmemPageSize = 4096;
const uint32_t sSupportedBlockSize = 4;

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
  for (size_t i = 0; i < mUsedShmems.size(); i++) {
    ShmemSectionHeapHeader* header = mUsedShmems[i].get<ShmemSectionHeapHeader>();
    if (header->mAllocatedBlocks == 0) {
      DeallocShmem(mUsedShmems[i]);

      // We don't particularly care about order, move the last one in the array
      // to this position.
      mUsedShmems[i] = mUsedShmems[mUsedShmems.size() - 1];
      mUsedShmems.pop_back();
      i--;
      break;
    }
  }
}

} // namespace
} // namespace
