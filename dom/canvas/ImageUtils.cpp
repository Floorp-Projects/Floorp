/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageUtils.h"
#include "ImageBitmapUtils.h"
#include "ImageContainer.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/dom/ImageBitmapBinding.h"
#include "mozilla/ErrorResult.h"

using namespace mozilla::layers;
using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

static ImageBitmapFormat
GetImageBitmapFormatFromSurfaceFromat(SurfaceFormat aSurfaceFormat)
{
  switch (aSurfaceFormat) {
  case SurfaceFormat::B8G8R8A8:
  case SurfaceFormat::B8G8R8X8:
    return ImageBitmapFormat::BGRA32;
  case SurfaceFormat::R8G8B8A8:
  case SurfaceFormat::R8G8B8X8:
    return ImageBitmapFormat::RGBA32;
  case SurfaceFormat::R8G8B8:
    return ImageBitmapFormat::RGB24;
  case SurfaceFormat::B8G8R8:
    return ImageBitmapFormat::BGR24;
  case SurfaceFormat::HSV:
    return ImageBitmapFormat::HSV;
  case SurfaceFormat::Lab:
    return ImageBitmapFormat::Lab;
  case SurfaceFormat::Depth:
    return ImageBitmapFormat::DEPTH;
  case SurfaceFormat::A8:
    return ImageBitmapFormat::GRAY8;
  case SurfaceFormat::R5G6B5_UINT16:
  case SurfaceFormat::YUV:
  case SurfaceFormat::NV12:
  case SurfaceFormat::UNKNOWN:
  default:
    return ImageBitmapFormat::EndGuard_;
  }
}

static ImageBitmapFormat
GetImageBitmapFormatFromPlanarYCbCrData(layers::PlanarYCbCrData const *aData)
{
  MOZ_ASSERT(aData);

  if (aData->mYSkip == 0 && aData->mCbSkip == 0 && aData->mCrSkip == 0) { // Possibly three planes.
    if (aData->mCbChannel >= aData->mYChannel + aData->mYSize.height * aData->mYStride &&
        aData->mCrChannel >= aData->mCbChannel + aData->mCbCrSize.height * aData->mCbCrStride) { // Three planes.
      if (aData->mYSize.height == aData->mCbCrSize.height) {
        if (aData->mYSize.width == aData->mCbCrSize.width) {
          return ImageBitmapFormat::YUV444P;
        } else if (((aData->mYSize.width + 1) / 2) == aData->mCbCrSize.width) {
          return ImageBitmapFormat::YUV422P;
        }
      } else if (((aData->mYSize.height + 1) / 2) == aData->mCbCrSize.height) {
        if (((aData->mYSize.width + 1) / 2) == aData->mCbCrSize.width) {
          return ImageBitmapFormat::YUV420P;
        }
      }
    }
  } else if (aData->mYSkip == 0 && aData->mCbSkip == 1 && aData->mCrSkip == 1) { // Possibly two planes.
    if (aData->mCbChannel >= aData->mYChannel + aData->mYSize.height * aData->mYStride &&
        aData->mCbChannel == aData->mCrChannel - 1) { // Two planes.
      if (((aData->mYSize.height + 1) / 2) == aData->mCbCrSize.height &&
          ((aData->mYSize.width + 1) / 2) == aData->mCbCrSize.width) {
        return ImageBitmapFormat::YUV420SP_NV12;  // Y-Cb-Cr
      }
    } else if (aData->mCrChannel >= aData->mYChannel + aData->mYSize.height * aData->mYStride &&
               aData->mCrChannel == aData->mCbChannel - 1) { // Two planes.
      if (((aData->mYSize.height + 1) / 2) == aData->mCbCrSize.height &&
          ((aData->mYSize.width + 1) / 2) == aData->mCbCrSize.width) {
        return ImageBitmapFormat::YUV420SP_NV21;  // Y-Cr-Cb
      }
    }
  }

  return ImageBitmapFormat::EndGuard_;
}

// ImageUtils::Impl implements the _generic_ algorithm which always readback
// data in RGBA format into CPU memory.
// Since layers::CairoImage is just a warpper to a DataSourceSurface, the
// implementation of CairoSurfaceImpl is nothing different to the generic
// version.
class ImageUtils::Impl
{
public:
  explicit Impl(layers::Image* aImage)
  : mImage(aImage)
  , mSurface(nullptr)
  {
  }

  virtual ~Impl() = default;

  virtual ImageBitmapFormat
  GetFormat() const
  {
    return GetImageBitmapFormatFromSurfaceFromat(Surface()->GetFormat());
  }

  virtual uint32_t
  GetBufferLength() const
  {
    const uint32_t stride = Surface()->Stride();
    const IntSize size = Surface()->GetSize();
    return (uint32_t)(size.height * stride);
  }

  virtual UniquePtr<ImagePixelLayout>
  MapDataInto(uint8_t* aBuffer,
              uint32_t aOffset,
              uint32_t aBufferLength,
              ImageBitmapFormat aFormat,
              ErrorResult& aRv) const
  {
    DataSourceSurface::ScopedMap map(Surface(), DataSourceSurface::READ);
    if (!map.IsMapped()) {
      aRv.Throw(NS_ERROR_ILLEGAL_VALUE);
      return nullptr;
    }

    // Copy or convert data.
    UniquePtr<ImagePixelLayout> srcLayout =
      CreateDefaultPixelLayout(GetFormat(), Surface()->GetSize().width,
                               Surface()->GetSize().height, map.GetStride());

    // Prepare destination buffer.
    uint8_t* dstBuffer = aBuffer + aOffset;
    UniquePtr<ImagePixelLayout> dstLayout =
      CopyAndConvertImageData(GetFormat(), map.GetData(), srcLayout.get(),
                              aFormat, dstBuffer);

    if (!dstLayout) {
      aRv.Throw(NS_ERROR_NOT_AVAILABLE);
      return nullptr;
    }

    return dstLayout;
  }

protected:
  Impl() {}

  DataSourceSurface* Surface() const
  {
    if (!mSurface) {
      RefPtr<SourceSurface> surface = mImage->GetAsSourceSurface();
      MOZ_ASSERT(surface);

      mSurface = surface->GetDataSurface();
      MOZ_ASSERT(mSurface);
    }

    return mSurface.get();
  }

  RefPtr<layers::Image> mImage;
  mutable RefPtr<DataSourceSurface> mSurface;
};

// YUVImpl is optimized for the layers::PlanarYCbCrImage and layers::NVImage.
// This implementation does not readback data in RGBA format but keep it in YUV
// format family.
class YUVImpl final : public ImageUtils::Impl
{
public:
  explicit YUVImpl(layers::Image* aImage)
  : Impl(aImage)
  {
    MOZ_ASSERT(aImage->GetFormat() == ImageFormat::PLANAR_YCBCR ||
               aImage->GetFormat() == ImageFormat::NV_IMAGE);
  }

  ImageBitmapFormat GetFormat() const override
  {
    return GetImageBitmapFormatFromPlanarYCbCrData(GetPlanarYCbCrData());
  }

  uint32_t GetBufferLength() const override
  {
    if (mImage->GetFormat() == ImageFormat::PLANAR_YCBCR) {
      return mImage->AsPlanarYCbCrImage()->GetDataSize();
    } else {
      return mImage->AsNVImage()->GetBufferSize();
    }
  }

  UniquePtr<ImagePixelLayout>
  MapDataInto(uint8_t* aBuffer,
              uint32_t aOffset,
              uint32_t aBufferLength,
              ImageBitmapFormat aFormat,
              ErrorResult& aRv) const override
  {
    // Prepare source buffer and pixel layout.
    const PlanarYCbCrData* data = GetPlanarYCbCrData();

    UniquePtr<ImagePixelLayout> srcLayout =
      CreatePixelLayoutFromPlanarYCbCrData(data);

    // Do conversion.
    UniquePtr<ImagePixelLayout> dstLayout =
      CopyAndConvertImageData(GetFormat(), data->mYChannel, srcLayout.get(),
                              aFormat, aBuffer+aOffset);

    if (!dstLayout) {
      aRv.Throw(NS_ERROR_NOT_AVAILABLE);
      return nullptr;
    }

    return dstLayout;
  }

private:
  const PlanarYCbCrData* GetPlanarYCbCrData() const
  {
    if (mImage->GetFormat() == ImageFormat::PLANAR_YCBCR) {
      return mImage->AsPlanarYCbCrImage()->GetData();
    } else {
      return mImage->AsNVImage()->GetData();
    }
  }
};

// TODO: optimize for other platforms.
// For GONK: implement GrallocImageImpl, GrallocPlanarYCbCrImpl and GonkCameraImpl.
// For Windows: implement D3D9RGB32TextureImpl and D3D11ShareHandleTextureImpl.
// Others: SharedBGRImpl, MACIOSrufaceImpl, GLImageImpl, SurfaceTextureImpl
//         EGLImageImpl and OverlayImegImpl.

ImageUtils::ImageUtils(layers::Image* aImage)
: mImpl(nullptr)
{
  MOZ_ASSERT(aImage, "Create ImageUtils with nullptr.");
  switch(aImage->GetFormat()) {
  case mozilla::ImageFormat::PLANAR_YCBCR:
  case mozilla::ImageFormat::NV_IMAGE:
    mImpl = new YUVImpl(aImage);
    break;
  case mozilla::ImageFormat::CAIRO_SURFACE:
  default:
    mImpl = new Impl(aImage);
  }
}

ImageUtils::~ImageUtils()
{
  if (mImpl) { delete mImpl; mImpl = nullptr; }
}

ImageBitmapFormat
ImageUtils::GetFormat() const
{
  MOZ_ASSERT(mImpl);
  return mImpl->GetFormat();
}

uint32_t
ImageUtils::GetBufferLength() const
{
  MOZ_ASSERT(mImpl);
  return mImpl->GetBufferLength();
}

UniquePtr<ImagePixelLayout>
ImageUtils::MapDataInto(uint8_t* aBuffer,
                        uint32_t aOffset,
                        uint32_t aBufferLength,
                        ImageBitmapFormat aFormat,
                        ErrorResult& aRv) const
{
  MOZ_ASSERT(mImpl);
  MOZ_ASSERT(aBuffer, "Map data into a null buffer.");
  return mImpl->MapDataInto(aBuffer, aOffset, aBufferLength, aFormat, aRv);
}

} // namespace dom
} // namespace mozilla
