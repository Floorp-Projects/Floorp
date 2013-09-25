/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ISurfaceAllocator.h"
#include <sys/types.h>                  // for int32_t
#include "gfxASurface.h"                // for gfxASurface, etc
#include "gfxPlatform.h"                // for gfxPlatform, gfxImageFormat
#include "gfxSharedImageSurface.h"      // for gfxSharedImageSurface
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
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

SharedMemory::SharedMemoryType OptimalShmemType()
{
  return SharedMemory::TYPE_BASIC;
}

bool
IsSurfaceDescriptorValid(const SurfaceDescriptor& aSurface)
{
  return aSurface.type() != SurfaceDescriptor::T__None &&
         aSurface.type() != SurfaceDescriptor::Tnull_t;
}

bool
ISurfaceAllocator::AllocSharedImageSurface(const gfxIntSize& aSize,
                               gfxContentType aContent,
                               gfxSharedImageSurface** aBuffer)
{
  SharedMemory::SharedMemoryType shmemType = OptimalShmemType();
  gfxImageFormat format = gfxPlatform::GetPlatform()->OptimalFormatForContent(aContent);

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
                                          gfxContentType aContent,
                                          SurfaceDescriptor* aBuffer)
{
  return AllocSurfaceDescriptorWithCaps(aSize, aContent, DEFAULT_BUFFER_CAPS, aBuffer);
}

bool
ISurfaceAllocator::AllocSurfaceDescriptorWithCaps(const gfxIntSize& aSize,
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
#ifdef XP_MACOSX
    // Workaround a bug in Quartz where drawing an a8 surface to another a8
    // surface with OPERATOR_SOURCE still requires the destination to be clear.
    if (format == gfxImageFormatA8) {
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

#if !defined(MOZ_HAVE_PLATFORM_SPECIFIC_LAYER_BUFFERS)
bool
ISurfaceAllocator::PlatformAllocSurfaceDescriptor(const gfxIntSize&,
                                                  gfxContentType,
                                                  uint32_t,
                                                  SurfaceDescriptor*)
{
  return false;
}
#endif

} // namespace
} // namespace
