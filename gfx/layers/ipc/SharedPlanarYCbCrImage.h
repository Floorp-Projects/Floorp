/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdint.h>                     // for uint8_t, uint32_t
#include "ImageContainer.h"             // for PlanarYCbCrImage, etc
#include "mozilla/Attributes.h"         // for MOZ_OVERRIDE
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/ipc/Shmem.h"          // for Shmem
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsDebug.h"                    // for NS_WARNING
#include "nsTraceRefcnt.h"              // for MOZ_COUNT_CTOR

class gfxASurface;

#ifndef MOZILLA_LAYERS_DeprecatedSharedPlanarYCbCrImage_H
#define MOZILLA_LAYERS_DeprecatedSharedPlanarYCbCrImage_H

namespace mozilla {
namespace layers {

class BufferTextureClient;
class ImageClient;
class ISurfaceAllocator;
class SurfaceDescriptor;
class TextureClient;

// XXX - This class will be removed along with DeprecatedImageClient
class DeprecatedSharedPlanarYCbCrImage : public PlanarYCbCrImage
{
public:
  DeprecatedSharedPlanarYCbCrImage(ISurfaceAllocator* aAllocator);

  ~DeprecatedSharedPlanarYCbCrImage();

  virtual DeprecatedSharedPlanarYCbCrImage* AsDeprecatedSharedPlanarYCbCrImage() MOZ_OVERRIDE
  {
    return this;
  }

  virtual already_AddRefed<gfxASurface> GetAsSurface() MOZ_OVERRIDE
  {
    if (!mAllocated) {
      NS_WARNING("Can't get as surface");
      return nullptr;
    }
    return PlanarYCbCrImage::GetAsSurface();
  }

  virtual void SetData(const PlanarYCbCrData& aData) MOZ_OVERRIDE;
  virtual void SetDataNoCopy(const Data &aData) MOZ_OVERRIDE;

  virtual bool Allocate(PlanarYCbCrData& aData);
  virtual uint8_t* AllocateBuffer(uint32_t aSize) MOZ_OVERRIDE;
  // needs to be overriden because the parent class sets mBuffer which we
  // do not want to happen.
  uint8_t* AllocateAndGetNewBuffer(uint32_t aSize) MOZ_OVERRIDE;

  virtual bool IsValid() MOZ_OVERRIDE {
    return mAllocated;
  }

  /**
   * Setup the Surface descriptor to contain this image's shmem, while keeping
   * ownership of the shmem.
   * if the operation succeeds, return true and AddRef this DeprecatedSharedPlanarYCbCrImage.
   */
  bool ToSurfaceDescriptor(SurfaceDescriptor& aResult);

  /**
   * Setup the Surface descriptor to contain this image's shmem, and loose
   * ownership of the shmem.
   * if the operation succeeds, return true (and does _not_ AddRef this
   * DeprecatedSharedPlanarYCbCrImage).
   */
  bool DropToSurfaceDescriptor(SurfaceDescriptor& aResult);

  /**
   * Returns a DeprecatedSharedPlanarYCbCrImage* iff the descriptor was initialized with
   * ToSurfaceDescriptor.
   */
  static DeprecatedSharedPlanarYCbCrImage* FromSurfaceDescriptor(const SurfaceDescriptor& aDesc);

private:
  ipc::Shmem mShmem;
  RefPtr<ISurfaceAllocator> mSurfaceAllocator;
  bool mAllocated;
};


class SharedPlanarYCbCrImage : public PlanarYCbCrImage
                             , public ISharedImage
{
public:
  SharedPlanarYCbCrImage(ImageClient* aCompositable);
  ~SharedPlanarYCbCrImage();

  virtual ISharedImage* AsSharedImage() MOZ_OVERRIDE { return this; }
  virtual TextureClient* GetTextureClient() MOZ_OVERRIDE;
  virtual uint8_t* GetBuffer() MOZ_OVERRIDE;

  virtual already_AddRefed<gfxASurface> GetAsSurface() MOZ_OVERRIDE;
  virtual void SetData(const PlanarYCbCrData& aData) MOZ_OVERRIDE;
  virtual void SetDataNoCopy(const Data &aData) MOZ_OVERRIDE;

  virtual bool Allocate(PlanarYCbCrData& aData);
  virtual uint8_t* AllocateBuffer(uint32_t aSize) MOZ_OVERRIDE;
  // needs to be overriden because the parent class sets mBuffer which we
  // do not want to happen.
  virtual uint8_t* AllocateAndGetNewBuffer(uint32_t aSize) MOZ_OVERRIDE;

  virtual bool IsValid() MOZ_OVERRIDE;

private:
  RefPtr<BufferTextureClient> mTextureClient;
  RefPtr<ImageClient> mCompositable;
};

} // namespace
} // namespace

#endif
