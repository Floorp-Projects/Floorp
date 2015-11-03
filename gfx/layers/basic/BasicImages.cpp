/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
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
#include "mozilla/mozalloc.h"           // for operator delete[], etc
#include "nsAutoPtr.h"                  // for nsRefPtr, nsAutoArrayPtr
#include "nsAutoRef.h"                  // for nsCountedRef
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsDebug.h"                    // for NS_ERROR, NS_ASSERTION
#include "nsISupportsImpl.h"            // for Image::Release, etc
#include "nsThreadUtils.h"              // for NS_IsMainThread
#include "mozilla/gfx/Point.h"          // for IntSize
#include "gfx2DGlue.h"
#include "YCbCrUtils.h"                 // for YCbCr conversions
#ifdef XP_MACOSX
#include "gfxQuartzImageSurface.h"
#endif

namespace mozilla {
namespace layers {

class BasicPlanarYCbCrImage : public PlanarYCbCrImage
{
public:
  BasicPlanarYCbCrImage(const gfx::IntSize& aScaleHint, gfxImageFormat aOffscreenFormat, BufferRecycleBin *aRecycleBin)
    : PlanarYCbCrImage(aRecycleBin)
    , mScaleHint(aScaleHint)
    , mDelayedConversion(false)
  {
    SetOffscreenFormat(aOffscreenFormat);
  }

  ~BasicPlanarYCbCrImage()
  {
    if (mDecodedBuffer) {
      // Right now this only happens if the Image was never drawn, otherwise
      // this will have been tossed away at surface destruction.
      mRecycleBin->RecycleBuffer(mDecodedBuffer.forget(), mSize.height * mStride);
    }
  }

  virtual bool SetData(const Data& aData) override;
  virtual void SetDelayedConversion(bool aDelayed) override { mDelayedConversion = aDelayed; }

  already_AddRefed<gfx::SourceSurface> GetAsSourceSurface() override;

  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  virtual size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    size_t size = PlanarYCbCrImage::SizeOfExcludingThis(aMallocSizeOf);
    size += mDecodedBuffer.SizeOfExcludingThis(aMallocSizeOf);
    return size;
  }

private:
  nsAutoArrayPtr<uint8_t> mDecodedBuffer;
  gfx::IntSize mScaleHint;
  int mStride;
  bool mDelayedConversion;
};

class BasicImageFactory : public ImageFactory
{
public:
  BasicImageFactory() {}

  virtual already_AddRefed<Image> CreateImage(ImageFormat aFormat,
                                              const gfx::IntSize &aScaleHint,
                                              BufferRecycleBin *aRecycleBin)
  {
    RefPtr<Image> image;
    if (aFormat == ImageFormat::PLANAR_YCBCR) {
      image = new BasicPlanarYCbCrImage(aScaleHint, gfxPlatform::GetPlatform()->GetOffscreenFormat(), aRecycleBin);
      return image.forget();
    }

    return ImageFactory::CreateImage(aFormat, aScaleHint, aRecycleBin);
  }
};

bool
BasicPlanarYCbCrImage::SetData(const Data& aData)
{
  PlanarYCbCrImage::SetData(aData);

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

  gfxImageFormat iFormat = gfx::SurfaceFormatToImageFormat(format);
  mStride = gfxASurface::FormatStrideForWidth(iFormat, size.width);
  mDecodedBuffer = AllocateBuffer(size.height * mStride);
  if (!mDecodedBuffer) {
    // out of memory
    return false;
  }

  gfx::ConvertYCbCrToRGB(aData, format, size, mDecodedBuffer, mStride);
  SetOffscreenFormat(iFormat);
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
      = gfxPlatform::GetPlatform()->CreateDrawTargetForData(mDecodedBuffer,
                                                            mSize,
                                                            mStride,
                                                            gfx::ImageFormatToSurfaceFormat(format));
    if (!drawTarget) {
      return nullptr;
    }

    surface = drawTarget->Snapshot();
  }

  mRecycleBin->RecycleBuffer(mDecodedBuffer.forget(), mSize.height * mStride);

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
