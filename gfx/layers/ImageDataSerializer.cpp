/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageDataSerializer.h"
#include "gfx2DGlue.h"            // for SurfaceFormatToImageFormat
#include "mozilla/gfx/Point.h"    // for IntSize
#include "mozilla/Assertions.h"   // for MOZ_ASSERT, etc
#include "mozilla/gfx/2D.h"       // for DataSourceSurface, Factory
#include "mozilla/gfx/Logging.h"  // for gfxDebug
#include "mozilla/gfx/Tools.h"    // for GetAlignedStride, etc
#include "mozilla/gfx/Types.h"
#include "mozilla/mozalloc.h"  // for operator delete, etc
#include "YCbCrUtils.h"        // for YCbCr conversions

namespace mozilla {
namespace layers {
namespace ImageDataSerializer {

using namespace gfx;

int32_t ComputeRGBStride(SurfaceFormat aFormat, int32_t aWidth) {
#ifdef XP_MACOSX
  // Some drivers require an alignment of 32 bytes for efficient texture upload.
  return GetAlignedStride<32>(aWidth, BytesPerPixel(aFormat));
#else
  return GetAlignedStride<4>(aWidth, BytesPerPixel(aFormat));
#endif
}

int32_t GetRGBStride(const RGBDescriptor& aDescriptor) {
  return ComputeRGBStride(aDescriptor.format(), aDescriptor.size().width);
}

uint32_t ComputeRGBBufferSize(IntSize aSize, SurfaceFormat aFormat) {
  MOZ_ASSERT(aSize.height >= 0 && aSize.width >= 0);

  // This takes care of checking whether there could be overflow
  // with enough margin for the metadata.
  if (!gfx::Factory::AllowedSurfaceSize(aSize)) {
    return 0;
  }

  // Note we're passing height instad of the bpp parameter, but the end
  // result is the same - and the bpp was already taken care of in the
  // ComputeRGBStride function.
  int32_t bufsize = GetAlignedStride<16>(ComputeRGBStride(aFormat, aSize.width),
                                         aSize.height);

  if (bufsize < 0) {
    // This should not be possible thanks to Factory::AllowedSurfaceSize
    return 0;
  }

  return bufsize;
}

// Minimum required shmem size in bytes
uint32_t ComputeYCbCrBufferSize(const gfx::IntSize& aYSize, int32_t aYStride,
                                const gfx::IntSize& aCbCrSize,
                                int32_t aCbCrStride) {
  MOZ_ASSERT(aYSize.height >= 0 && aYSize.width >= 0);

  if (aYSize.height < 0 || aYSize.width < 0 || aCbCrSize.height < 0 ||
      aCbCrSize.width < 0 ||
      !gfx::Factory::AllowedSurfaceSize(IntSize(aYStride, aYSize.height)) ||
      !gfx::Factory::AllowedSurfaceSize(
          IntSize(aCbCrStride, aCbCrSize.height))) {
    return 0;
  }

  // Overflow checks are performed in AllowedSurfaceSize
  return GetAlignedStride<4>(aYSize.height, aYStride) +
         2 * GetAlignedStride<4>(aCbCrSize.height, aCbCrStride);
}

uint32_t ComputeYCbCrBufferSize(const gfx::IntSize& aYSize, int32_t aYStride,
                                const gfx::IntSize& aCbCrSize,
                                int32_t aCbCrStride, uint32_t aYOffset,
                                uint32_t aCbOffset, uint32_t aCrOffset) {
  MOZ_ASSERT(aYSize.height >= 0 && aYSize.width >= 0);

  if (aYSize.height < 0 || aYSize.width < 0 || aCbCrSize.height < 0 ||
      aCbCrSize.width < 0 ||
      !gfx::Factory::AllowedSurfaceSize(IntSize(aYStride, aYSize.height)) ||
      !gfx::Factory::AllowedSurfaceSize(
          IntSize(aCbCrStride, aCbCrSize.height))) {
    return 0;
  }

  uint32_t yLength = GetAlignedStride<4>(aYStride, aYSize.height);
  uint32_t cbCrLength = GetAlignedStride<4>(aCbCrStride, aCbCrSize.height);
  if (yLength == 0 || cbCrLength == 0) {
    return 0;
  }

  CheckedInt<uint32_t> yEnd = aYOffset;
  yEnd += yLength;
  CheckedInt<uint32_t> cbEnd = aCbOffset;
  cbEnd += cbCrLength;
  CheckedInt<uint32_t> crEnd = aCrOffset;
  crEnd += cbCrLength;

  if (!yEnd.isValid() || !cbEnd.isValid() || !crEnd.isValid() ||
      yEnd.value() > aCbOffset || cbEnd.value() > aCrOffset) {
    return 0;
  }

  return crEnd.value();
}

uint32_t ComputeYCbCrBufferSize(uint32_t aBufferSize) {
  return GetAlignedStride<4>(aBufferSize, 1);
}

void ComputeYCbCrOffsets(int32_t yStride, int32_t yHeight, int32_t cbCrStride,
                         int32_t cbCrHeight, uint32_t& outYOffset,
                         uint32_t& outCbOffset, uint32_t& outCrOffset) {
  outYOffset = 0;
  outCbOffset = outYOffset + GetAlignedStride<4>(yStride, yHeight);
  outCrOffset = outCbOffset + GetAlignedStride<4>(cbCrStride, cbCrHeight);
}

gfx::SurfaceFormat FormatFromBufferDescriptor(
    const BufferDescriptor& aDescriptor) {
  switch (aDescriptor.type()) {
    case BufferDescriptor::TRGBDescriptor:
      return aDescriptor.get_RGBDescriptor().format();
    case BufferDescriptor::TYCbCrDescriptor:
      return gfx::SurfaceFormat::YUV;
    default:
      MOZ_CRASH("GFX: FormatFromBufferDescriptor");
  }
}

gfx::IntSize SizeFromBufferDescriptor(const BufferDescriptor& aDescriptor) {
  switch (aDescriptor.type()) {
    case BufferDescriptor::TRGBDescriptor:
      return aDescriptor.get_RGBDescriptor().size();
    case BufferDescriptor::TYCbCrDescriptor:
      return aDescriptor.get_YCbCrDescriptor().ySize();
    default:
      MOZ_CRASH("GFX: SizeFromBufferDescriptor");
  }
}

Maybe<gfx::IntSize> CbCrSizeFromBufferDescriptor(
    const BufferDescriptor& aDescriptor) {
  switch (aDescriptor.type()) {
    case BufferDescriptor::TRGBDescriptor:
      return Nothing();
    case BufferDescriptor::TYCbCrDescriptor:
      return Some(aDescriptor.get_YCbCrDescriptor().cbCrSize());
    default:
      MOZ_CRASH("GFX:  CbCrSizeFromBufferDescriptor");
  }
}

Maybe<gfx::YUVColorSpace> YUVColorSpaceFromBufferDescriptor(
    const BufferDescriptor& aDescriptor) {
  switch (aDescriptor.type()) {
    case BufferDescriptor::TRGBDescriptor:
      return Nothing();
    case BufferDescriptor::TYCbCrDescriptor:
      return Some(aDescriptor.get_YCbCrDescriptor().yUVColorSpace());
    default:
      MOZ_CRASH("GFX:  YUVColorSpaceFromBufferDescriptor");
  }
}

Maybe<gfx::ColorDepth> ColorDepthFromBufferDescriptor(
    const BufferDescriptor& aDescriptor) {
  switch (aDescriptor.type()) {
    case BufferDescriptor::TRGBDescriptor:
      return Nothing();
    case BufferDescriptor::TYCbCrDescriptor:
      return Some(aDescriptor.get_YCbCrDescriptor().colorDepth());
    default:
      MOZ_CRASH("GFX:  ColorDepthFromBufferDescriptor");
  }
}

Maybe<StereoMode> StereoModeFromBufferDescriptor(
    const BufferDescriptor& aDescriptor) {
  switch (aDescriptor.type()) {
    case BufferDescriptor::TRGBDescriptor:
      return Nothing();
    case BufferDescriptor::TYCbCrDescriptor:
      return Some(aDescriptor.get_YCbCrDescriptor().stereoMode());
    default:
      MOZ_CRASH("GFX:  StereoModeFromBufferDescriptor");
  }
}

uint8_t* GetYChannel(uint8_t* aBuffer, const YCbCrDescriptor& aDescriptor) {
  return aBuffer + aDescriptor.yOffset();
}

uint8_t* GetCbChannel(uint8_t* aBuffer, const YCbCrDescriptor& aDescriptor) {
  return aBuffer + aDescriptor.cbOffset();
}

uint8_t* GetCrChannel(uint8_t* aBuffer, const YCbCrDescriptor& aDescriptor) {
  return aBuffer + aDescriptor.crOffset();
}

already_AddRefed<DataSourceSurface> DataSourceSurfaceFromYCbCrDescriptor(
    uint8_t* aBuffer, const YCbCrDescriptor& aDescriptor,
    gfx::DataSourceSurface* aSurface) {
  gfx::IntSize ySize = aDescriptor.ySize();

  RefPtr<DataSourceSurface> result;
  if (aSurface) {
    MOZ_ASSERT(aSurface->GetSize() == ySize);
    MOZ_ASSERT(aSurface->GetFormat() == gfx::SurfaceFormat::B8G8R8X8);
    if (aSurface->GetSize() == ySize &&
        aSurface->GetFormat() == gfx::SurfaceFormat::B8G8R8X8) {
      result = aSurface;
    }
  }

  if (!result) {
    result =
        Factory::CreateDataSourceSurface(ySize, gfx::SurfaceFormat::B8G8R8X8);
  }
  if (NS_WARN_IF(!result)) {
    return nullptr;
  }

  DataSourceSurface::MappedSurface map;
  if (NS_WARN_IF(!result->Map(DataSourceSurface::MapType::WRITE, &map))) {
    return nullptr;
  }

  layers::PlanarYCbCrData ycbcrData;
  ycbcrData.mYChannel = GetYChannel(aBuffer, aDescriptor);
  ycbcrData.mYStride = aDescriptor.yStride();
  ycbcrData.mYSize = ySize;
  ycbcrData.mCbChannel = GetCbChannel(aBuffer, aDescriptor);
  ycbcrData.mCrChannel = GetCrChannel(aBuffer, aDescriptor);
  ycbcrData.mCbCrStride = aDescriptor.cbCrStride();
  ycbcrData.mCbCrSize = aDescriptor.cbCrSize();
  ycbcrData.mPicSize = ySize;
  ycbcrData.mYUVColorSpace = aDescriptor.yUVColorSpace();
  ycbcrData.mColorDepth = aDescriptor.colorDepth();

  gfx::ConvertYCbCrToRGB(ycbcrData, gfx::SurfaceFormat::B8G8R8X8, ySize,
                         map.mData, map.mStride);

  result->Unmap();
  return result.forget();
}

void ConvertAndScaleFromYCbCrDescriptor(uint8_t* aBuffer,
                                        const YCbCrDescriptor& aDescriptor,
                                        const gfx::SurfaceFormat& aDestFormat,
                                        const gfx::IntSize& aDestSize,
                                        unsigned char* aDestBuffer,
                                        int32_t aStride) {
  MOZ_ASSERT(aBuffer);

  layers::PlanarYCbCrData ycbcrData;
  ycbcrData.mYChannel = GetYChannel(aBuffer, aDescriptor);
  ycbcrData.mYStride = aDescriptor.yStride();
  ;
  ycbcrData.mYSize = aDescriptor.ySize();
  ycbcrData.mCbChannel = GetCbChannel(aBuffer, aDescriptor);
  ycbcrData.mCrChannel = GetCrChannel(aBuffer, aDescriptor);
  ycbcrData.mCbCrStride = aDescriptor.cbCrStride();
  ycbcrData.mCbCrSize = aDescriptor.cbCrSize();
  ycbcrData.mPicSize = aDescriptor.ySize();
  ycbcrData.mYUVColorSpace = aDescriptor.yUVColorSpace();
  ycbcrData.mColorDepth = aDescriptor.colorDepth();

  gfx::ConvertYCbCrToRGB(ycbcrData, aDestFormat, aDestSize, aDestBuffer,
                         aStride);
}

}  // namespace ImageDataSerializer
}  // namespace layers
}  // namespace mozilla
