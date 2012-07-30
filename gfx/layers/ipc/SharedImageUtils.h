/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_SHAREDIMAGEUTILS_H
#define MOZILLA_LAYERS_SHAREDIMAGEUTILS_H

#include "gfxSharedImageSurface.h"
#include "gfxPlatform.h"
#include "ShadowLayers.h"
 
namespace mozilla {
namespace layers {

template<typename Deallocator>
void DeallocSharedImageData(Deallocator* protocol, const SharedImage& aImage)
{
  if (aImage.type() == SharedImage::TYUVImage) {
    protocol->DeallocShmem(aImage.get_YUVImage().Ydata());
    protocol->DeallocShmem(aImage.get_YUVImage().Udata());
    protocol->DeallocShmem(aImage.get_YUVImage().Vdata());
  } else if (aImage.type() == SharedImage::TSurfaceDescriptor) {
    protocol->DeallocShmem(aImage.get_SurfaceDescriptor().get_Shmem());
  }
}

template<typename Allocator>
bool AllocateSharedBuffer(Allocator* protocol,
                          const gfxIntSize& aSize,
                          gfxASurface::gfxContentType aContent,
                          gfxSharedImageSurface** aBuffer)
{
  ipc::SharedMemory::SharedMemoryType shmemType = OptimalShmemType();
  gfxASurface::gfxImageFormat format = gfxPlatform::GetPlatform()->OptimalFormatForContent(aContent);

  nsRefPtr<gfxSharedImageSurface> back =
    gfxSharedImageSurface::CreateUnsafe(protocol, aSize, format, shmemType);
  if (!back)
    return false;

  *aBuffer = nullptr;
  back.swap(*aBuffer);
  return true;
}


} // namespace
} // namespace

#endif

