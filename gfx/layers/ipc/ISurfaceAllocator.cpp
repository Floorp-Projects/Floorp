/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ISurfaceAllocator.h"
#include "mozilla/ipc/SharedMemory.h"
#include "gfxSharedImageSurface.h"
#include "gfxPlatform.h"
#include "gfxASurface.h"
#include "mozilla/layers/LayersSurfaces.h"
#include "mozilla/layers/SharedPlanarYCbCrImage.h"
#include "mozilla/layers/SharedRGBImage.h"
#include "nsXULAppAPI.h"

#ifdef DEBUG
#include "prenv.h"
#endif

using namespace mozilla::ipc;

namespace mozilla {
namespace layers {

SharedMemory::SharedMemoryType OptimalShmemType()
{
#if defined(MOZ_PLATFORM_MAEMO) && defined(MOZ_HAVE_SHAREDMEMORYSYSV)
  // Use SysV memory because maemo5 on the N900 only allots 64MB to
  // /dev/shm, even though it has 1GB(!!) of system memory.  Sys V shm
  // is allocated from a different pool.  We don't want an arbitrary
  // cap that's much much lower than available memory on the memory we
  // use for layers.
  return SharedMemory::TYPE_SYSV;
#else
  return SharedMemory::TYPE_BASIC;
#endif
}

bool
IsSurfaceDescriptorValid(const SurfaceDescriptor& aSurface)
{
  return aSurface.type() != SurfaceDescriptor::T__None &&
         aSurface.type() != SurfaceDescriptor::Tnull_t;
}

bool
ISurfaceAllocator::AllocSharedImageSurface(const gfxIntSize& aSize,
                               gfxASurface::gfxContentType aContent,
                               gfxSharedImageSurface** aBuffer)
{
  SharedMemory::SharedMemoryType shmemType = OptimalShmemType();
  gfxASurface::gfxImageFormat format = gfxPlatform::GetPlatform()->OptimalFormatForContent(aContent);

  nsRefPtr<gfxSharedImageSurface> back =
    gfxSharedImageSurface::CreateUnsafe(this, aSize, format, shmemType);
  if (!back)
    return false;

  *aBuffer = nullptr;
  back.swap(*aBuffer);
  return true;
}

bool
ISurfaceAllocator::AllocSurfaceDescriptor(const gfxIntSize& aSize,
                                          gfxASurface::gfxContentType aContent,
                                          SurfaceDescriptor* aBuffer)
{
  return AllocSurfaceDescriptorWithCaps(aSize, aContent, DEFAULT_BUFFER_CAPS, aBuffer);
}

bool
ISurfaceAllocator::AllocSurfaceDescriptorWithCaps(const gfxIntSize& aSize,
                                                  gfxASurface::gfxContentType aContent,
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
    uint8_t *data = new uint8_t[stride * aSize.height];
#ifdef XP_MACOSX
    // Workaround a bug in Quartz where drawing an a8 surface to another a8
    // surface with OPERATOR_SOURCE still requires the destination to be clear.
    if (format == gfxASurface::ImageFormatA8) {
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


void
ISurfaceAllocator::DestroySharedSurface(SurfaceDescriptor* aSurface)
{
  MOZ_ASSERT(aSurface);
  if (!aSurface) {
    return;
  }
  if (!IsOnCompositorSide() && ReleaseOwnedSurfaceDescriptor(*aSurface)) {
    *aSurface = SurfaceDescriptor();
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
    case SurfaceDescriptor::TSurfaceDescriptorD3D10:
      break;
    case SurfaceDescriptor::TMemoryImage:
      delete [] (unsigned char *)aSurface->get_MemoryImage().data();
      break;
    case SurfaceDescriptor::Tnull_t:
    case SurfaceDescriptor::T__None:
      break;
    default:
      NS_RUNTIMEABORT("surface type not implemented!");
  }
  *aSurface = SurfaceDescriptor();
}

bool IsSurfaceDescriptorOwned(const SurfaceDescriptor& aDescriptor)
{
  switch (aDescriptor.type()) {
    case SurfaceDescriptor::TYCbCrImage: {
      const YCbCrImage& ycbcr = aDescriptor.get_YCbCrImage();
      return ycbcr.owner() != 0;
    }
    case SurfaceDescriptor::TRGBImage: {
      const RGBImage& rgb = aDescriptor.get_RGBImage();
      return rgb.owner() != 0;
    }
    default:
      return false;
  }
  return false;
}
bool ReleaseOwnedSurfaceDescriptor(const SurfaceDescriptor& aDescriptor)
{
  SharedPlanarYCbCrImage* sharedYCbCr = SharedPlanarYCbCrImage::FromSurfaceDescriptor(aDescriptor);
  if (sharedYCbCr) {
    sharedYCbCr->Release();
    return true;
  }
  SharedRGBImage* sharedRGB = SharedRGBImage::FromSurfaceDescriptor(aDescriptor);
  if (sharedRGB) {
    sharedRGB->Release();
    return true;
  }
  return false;
}

#if !defined(MOZ_HAVE_PLATFORM_SPECIFIC_LAYER_BUFFERS)
bool
ISurfaceAllocator::PlatformAllocSurfaceDescriptor(const gfxIntSize&,
                                                  gfxASurface::gfxContentType,
                                                  uint32_t,
                                                  SurfaceDescriptor*)
{
  return false;
}
#endif

} // namespace
} // namespace
