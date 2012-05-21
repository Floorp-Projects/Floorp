/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ReentrantMonitor.h"

#include "ImageLayers.h"
#include "BasicLayers.h"
#include "gfxImageSurface.h"

#ifdef XP_MACOSX
#include "gfxQuartzImageSurface.h"
#endif

#include "cairo.h"

#include "gfxUtils.h"

#include "gfxPlatform.h"

using mozilla::ReentrantMonitor;

namespace mozilla {
namespace layers {

class BasicPlanarYCbCrImage : public PlanarYCbCrImage
{
public:
  BasicPlanarYCbCrImage(const gfxIntSize& aScaleHint, gfxImageFormat aOffscreenFormat, BufferRecycleBin *aRecycleBin)
    : PlanarYCbCrImage(aRecycleBin)
    , mScaleHint(aScaleHint)
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

  virtual void SetData(const Data& aData);
  already_AddRefed<gfxASurface> GetAsSurface();

private:
  gfxIntSize mScaleHint;
  int mStride;
  nsAutoArrayPtr<PRUint8> mDecodedBuffer;
};

class BasicImageFactory : public ImageFactory
{
public:
  BasicImageFactory() {}

  virtual already_AddRefed<Image> CreateImage(const Image::Format* aFormats,
                                              PRUint32 aNumFormats,
                                              const gfxIntSize &aScaleHint,
                                              BufferRecycleBin *aRecycleBin)
  {
    if (!aNumFormats) {
      return nsnull;
    }

    nsRefPtr<Image> image;
    if (aFormats[0] == Image::PLANAR_YCBCR) {
      image = new BasicPlanarYCbCrImage(aScaleHint, gfxPlatform::GetPlatform()->GetOffscreenFormat(), aRecycleBin);
      return image.forget();
    }

    return ImageFactory::CreateImage(aFormats, aNumFormats, aScaleHint, aRecycleBin);
  }
};

void
BasicPlanarYCbCrImage::SetData(const Data& aData)
{
  PlanarYCbCrImage::SetData(aData);

  // Do some sanity checks to prevent integer overflow
  if (aData.mYSize.width > PlanarYCbCrImage::MAX_DIMENSION ||
      aData.mYSize.height > PlanarYCbCrImage::MAX_DIMENSION) {
    NS_ERROR("Illegal image source width or height");
    return;
  }

  gfxASurface::gfxImageFormat format = GetOffscreenFormat();

  gfxIntSize size(mScaleHint);
  gfxUtils::GetYCbCrToRGBDestFormatAndSize(aData, format, size);
  if (size.width > PlanarYCbCrImage::MAX_DIMENSION ||
      size.height > PlanarYCbCrImage::MAX_DIMENSION) {
    NS_ERROR("Illegal image dest width or height");
    return;
  }

  mStride = gfxASurface::FormatStrideForWidth(format, size.width);
  mDecodedBuffer = AllocateBuffer(size.height * mStride);
  if (!mDecodedBuffer) {
    // out of memory
    return;
  }

  gfxUtils::ConvertYCbCrToRGB(aData, format, size, mDecodedBuffer, mStride);
  SetOffscreenFormat(format);
  mSize = size;
}

static cairo_user_data_key_t imageSurfaceDataKey;

static void
DestroyBuffer(void* aBuffer)
{
  delete[] static_cast<PRUint8*>(aBuffer);
}

already_AddRefed<gfxASurface>
BasicPlanarYCbCrImage::GetAsSurface()
{
  NS_ASSERTION(NS_IsMainThread(), "Must be main thread");

  if (mSurface) {
    nsRefPtr<gfxASurface> result = mSurface.get();
    return result.forget();
  }

  if (!mDecodedBuffer) {
    return PlanarYCbCrImage::GetAsSurface();
  }

  gfxASurface::gfxImageFormat format = GetOffscreenFormat();

  nsRefPtr<gfxImageSurface> imgSurface =
      new gfxImageSurface(mDecodedBuffer, mSize, mStride, format);
  if (!imgSurface || imgSurface->CairoStatus() != 0) {
    return nsnull;
  }

  // Pass ownership of the buffer to the surface
  imgSurface->SetData(&imageSurfaceDataKey, mDecodedBuffer.forget(), DestroyBuffer);

  nsRefPtr<gfxASurface> result = imgSurface.get();
#if defined(XP_MACOSX)
  nsRefPtr<gfxQuartzImageSurface> quartzSurface =
    new gfxQuartzImageSurface(imgSurface);
  if (quartzSurface) {
    result = quartzSurface.forget();
  }
#endif

  mSurface = result;

  return result.forget();
}

ImageFactory*
BasicLayerManager::GetImageFactory()
{
  if (!mFactory) {
    mFactory = new BasicImageFactory();
  }

  return mFactory.get();
}

}
}
