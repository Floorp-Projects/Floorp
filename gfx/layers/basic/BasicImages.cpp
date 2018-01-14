/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdint.h>                     // for uint8_t, uint32_t
#include "BasicLayers.h"                // for BasicLayerManager
#include "ImageContainer.h"             // for PlanarYCbCrImage, etc
#include "ImageTypes.h"                 // for ImageFormat, etc
#include "cairo.h"                      // for cairo_user_data_key_t
#include "gfxASurface.h"                // for gfxASurface, etc
#include "gfxPlatform.h"                // for gfxPlatform, gfxImageFormat
#include "gfxUtils.h"                   // for gfxUtils
#include "mozilla/CheckedInt.h"
#include "mozilla/mozalloc.h"           // for operator delete[], etc
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "nsAutoRef.h"                  // for nsCountedRef
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsDebug.h"                    // for NS_ERROR, NS_ASSERTION
#include "nsISupportsImpl.h"            // for Image::Release, etc
#include "nsThreadUtils.h"              // for NS_IsMainThread
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/gfx/DataSurfaceHelpers.h"
#include "gfx2DGlue.h"
#include "YCbCrUtils.h"                 // for YCbCr conversions

namespace mozilla {
namespace layers {

class BasicPlanarYCbCrImage : public RecyclingPlanarYCbCrImage
{
public:
  BasicPlanarYCbCrImage(const gfx::IntSize& aScaleHint, gfxImageFormat aOffscreenFormat, BufferRecycleBin *aRecycleBin)
    : RecyclingPlanarYCbCrImage(aRecycleBin)
    , mScaleHint(aScaleHint)
    , mStride(0)
    , mDelayedConversion(false)
  {
    SetOffscreenFormat(aOffscreenFormat);
  }

  ~BasicPlanarYCbCrImage()
  {
    if (mDecodedBuffer) {
      // Right now this only happens if the Image was never drawn, otherwise
      // this will have been tossed away at surface destruction.
      mRecycleBin->RecycleBuffer(Move(mDecodedBuffer), mSize.height * mStride);
    }
  }

  virtual bool CopyData(const Data& aData) override;
  virtual void SetDelayedConversion(bool aDelayed) override { mDelayedConversion = aDelayed; }

  already_AddRefed<gfx::SourceSurface> GetAsSourceSurface() override;

  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  virtual size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    size_t size = RecyclingPlanarYCbCrImage::SizeOfExcludingThis(aMallocSizeOf);
    size += aMallocSizeOf(mDecodedBuffer.get());
    return size;
  }

private:
  UniquePtr<uint8_t[]> mDecodedBuffer;
  gfx::IntSize mScaleHint;
  int mStride;
  bool mDelayedConversion;
};

class BasicImageFactory : public ImageFactory
{
public:
  BasicImageFactory() {}

  virtual RefPtr<PlanarYCbCrImage>
  CreatePlanarYCbCrImage(const gfx::IntSize& aScaleHint, BufferRecycleBin* aRecycleBin) override
  {
    return new BasicPlanarYCbCrImage(aScaleHint, gfxPlatform::GetPlatform()->GetOffscreenFormat(), aRecycleBin);
  }
};

bool
BasicPlanarYCbCrImage::CopyData(const Data& aData)
{
  RecyclingPlanarYCbCrImage::CopyData(aData);

  if (mDelayedConversion) {
    return false;
  }

  // Do some sanity checks to prevent integer overflow
  if (aData.mYSize.width > PlanarYCbCrImage::MAX_DIMENSION ||
      aData.mYSize.height > PlanarYCbCrImage::MAX_DIMENSION) {
    NS_ERROR("Illegal image source width or height");
    return false;
  }

  gfx::SurfaceFormat format = gfx::ImageFormatToSurfaceFormat(GetOffscreenFormat());

  gfx::IntSize size(mScaleHint);
  gfx::GetYCbCrToRGBDestFormatAndSize(aData, format, size);
  if (size.width > PlanarYCbCrImage::MAX_DIMENSION ||
      size.height > PlanarYCbCrImage::MAX_DIMENSION) {
    NS_ERROR("Illegal image dest width or height");
    return false;
  }

  mStride = gfx::StrideForFormatAndWidth(format, size.width);
  mozilla::CheckedInt32 requiredBytes =
    mozilla::CheckedInt32(size.height) * mozilla::CheckedInt32(mStride);
  if (!requiredBytes.isValid()) {
    // invalid size
    return false;
  }
  mDecodedBuffer = AllocateBuffer(requiredBytes.value());
  if (!mDecodedBuffer) {
    // out of memory
    return false;
  }

  gfx::ConvertYCbCrToRGB(aData, format, size, mDecodedBuffer.get(), mStride);
  SetOffscreenFormat(gfx::SurfaceFormatToImageFormat(format));
  mSize = size;

  return true;
}

already_AddRefed<gfx::SourceSurface>
BasicPlanarYCbCrImage::GetAsSourceSurface()
{
  NS_ASSERTION(NS_IsMainThread(), "Must be main thread");

  if (mSourceSurface) {
    RefPtr<gfx::SourceSurface> surface(mSourceSurface);
    return surface.forget();
  }

  if (!mDecodedBuffer) {
    return PlanarYCbCrImage::GetAsSourceSurface();
  }

  gfxImageFormat format = GetOffscreenFormat();

  RefPtr<gfx::SourceSurface> surface;
  {
    // Create a DrawTarget so that we can own the data inside mDecodeBuffer.
    // We create the target out of mDecodedBuffer, and get a snapshot from it.
    // The draw target is destroyed on scope exit and the surface owns the data.
    RefPtr<gfx::DrawTarget> drawTarget
      = gfxPlatform::CreateDrawTargetForData(mDecodedBuffer.get(),
                                             mSize,
                                             mStride,
                                             gfx::ImageFormatToSurfaceFormat(format));
    if (!drawTarget) {
      return nullptr;
    }

    surface = drawTarget->Snapshot();
  }

  mRecycleBin->RecycleBuffer(Move(mDecodedBuffer), mSize.height * mStride);

  mSourceSurface = surface;
  return surface.forget();
}


ImageFactory*
BasicLayerManager::GetImageFactory()
{
  if (!mFactory) {
    mFactory = new BasicImageFactory();
  }

  return mFactory.get();
}

} // namespace layers
} // namespace mozilla
