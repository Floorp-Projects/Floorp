/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "libyuv.h"
#include "MacIOSurfaceHelpers.h"
#include "mozilla/gfx/MacIOSurface.h"
#include "mozilla/ScopeExit.h"
#include "YCbCrUtils.h"

namespace mozilla {

using namespace gfx;

namespace layers {

#define ALIGNED_32(x) ((x + 31) & ~31)
#define ALIGNEDPTR_32(x) \
  reinterpret_cast<uint8_t*>((reinterpret_cast<uintptr_t>(x) + 31) & ~31)

static nsresult CopyFromLockedMacIOSurface(MacIOSurface* aSurface,
                                           uint8_t* aData, int32_t aStride,
                                           const IntSize& aSize,
                                           SurfaceFormat aFormat) {
  size_t bytesPerRow = aSurface->GetBytesPerRow();
  SurfaceFormat ioFormat = aSurface->GetFormat();

  if ((ioFormat == SurfaceFormat::NV12 || ioFormat == SurfaceFormat::YUV422) &&
      (aSize.width > PlanarYCbCrImage::MAX_DIMENSION ||
       aSize.height > PlanarYCbCrImage::MAX_DIMENSION)) {
    return NS_ERROR_FAILURE;
  }

  if (ioFormat == SurfaceFormat::NV12) {
    /* Extract and separate the CbCr planes */
    size_t cbCrStride = aSurface->GetBytesPerRow(1);
    size_t cbCrWidth = aSurface->GetDevicePixelWidth(1);
    size_t cbCrHeight = aSurface->GetDevicePixelHeight(1);

    auto cbPlane = MakeUnique<uint8_t[]>(cbCrWidth * cbCrHeight);
    auto crPlane = MakeUnique<uint8_t[]>(cbCrWidth * cbCrHeight);

    uint8_t* src = (uint8_t*)aSurface->GetBaseAddressOfPlane(1);
    uint8_t* cbDest = cbPlane.get();
    uint8_t* crDest = crPlane.get();

    for (size_t i = 0; i < cbCrHeight; i++) {
      uint8_t* rowSrc = src + cbCrStride * i;
      for (size_t j = 0; j < cbCrWidth; j++) {
        *cbDest = *rowSrc;
        cbDest++;
        rowSrc++;
        *crDest = *rowSrc;
        crDest++;
        rowSrc++;
      }
    }

    /* Convert to RGB */
    PlanarYCbCrData data;
    data.mYChannel = (uint8_t*)aSurface->GetBaseAddressOfPlane(0);
    data.mYStride = aSurface->GetBytesPerRow(0);
    data.mCbChannel = cbPlane.get();
    data.mCrChannel = crPlane.get();
    data.mCbCrStride = cbCrWidth;
    data.mPictureRect = IntRect(IntPoint(0, 0), aSize);
    data.mYUVColorSpace = aSurface->GetYUVColorSpace();
    data.mColorPrimaries = aSurface->mColorPrimaries;
    data.mColorRange = aSurface->IsFullRange() ? gfx::ColorRange::FULL
                                               : gfx::ColorRange::LIMITED;
    data.mChromaSubsampling = ChromaSubsampling::HALF_WIDTH_AND_HEIGHT;

    ConvertYCbCrToRGB(data, SurfaceFormat::B8G8R8X8, aSize, aData, aStride);
  } else if (ioFormat == SurfaceFormat::YUV422) {
    if (aSize.width == ALIGNED_32(aSize.width)) {
      // Optimization when width is aligned to 32.
      libyuv::ConvertToARGB((uint8_t*)aSurface->GetBaseAddress(),
                            0 /* not used */, aData, aStride, 0, 0, aSize.width,
                            aSize.height, aSize.width, aSize.height,
                            libyuv::kRotate0, libyuv::FOURCC_YUYV);
    } else {
      /* Convert to YV16 */
      size_t cbCrWidth = (aSize.width + 1) >> 1;
      size_t cbCrHeight = aSize.height;
      // Ensure our stride is a multiple of 32 to allow for memory aligned rows.
      size_t cbCrStride = ALIGNED_32(cbCrWidth);
      size_t strideDelta = cbCrStride - cbCrWidth;
      MOZ_ASSERT(strideDelta <= 31);

      auto yPlane = MakeUnique<uint8_t[]>(cbCrStride * 2 * aSize.height + 31);
      auto cbPlane = MakeUnique<uint8_t[]>(cbCrStride * cbCrHeight + 31);
      auto crPlane = MakeUnique<uint8_t[]>(cbCrStride * cbCrHeight + 31);

      uint8_t* src = (uint8_t*)aSurface->GetBaseAddress();
      uint8_t* yDest = ALIGNEDPTR_32(yPlane.get());
      uint8_t* cbDest = ALIGNEDPTR_32(cbPlane.get());
      uint8_t* crDest = ALIGNEDPTR_32(crPlane.get());

      for (int32_t i = 0; i < aSize.height; i++) {
        uint8_t* rowSrc = src + bytesPerRow * i;
        for (size_t j = 0; j < cbCrWidth; j++) {
          *yDest = *rowSrc;
          yDest++;
          rowSrc++;
          *cbDest = *rowSrc;
          cbDest++;
          rowSrc++;
          *yDest = *rowSrc;
          yDest++;
          rowSrc++;
          *crDest = *rowSrc;
          crDest++;
          rowSrc++;
        }
        if (strideDelta) {
          cbDest += strideDelta;
          crDest += strideDelta;
          yDest += strideDelta << 1;
        }
      }

      /* Convert to RGB */
      PlanarYCbCrData data;
      data.mYChannel = ALIGNEDPTR_32(yPlane.get());
      data.mYStride = cbCrStride * 2;
      data.mCbChannel = ALIGNEDPTR_32(cbPlane.get());
      data.mCrChannel = ALIGNEDPTR_32(crPlane.get());
      data.mCbCrStride = cbCrStride;
      data.mPictureRect = IntRect(IntPoint(0, 0), aSize);
      data.mYUVColorSpace = aSurface->GetYUVColorSpace();
      data.mColorPrimaries = aSurface->mColorPrimaries;
      data.mColorRange = aSurface->IsFullRange() ? gfx::ColorRange::FULL
                                                 : gfx::ColorRange::LIMITED;
      data.mChromaSubsampling = ChromaSubsampling::HALF_WIDTH;

      ConvertYCbCrToRGB(data, SurfaceFormat::B8G8R8X8, aSize, aData, aStride);
    }
  } else {
    unsigned char* ioData = (unsigned char*)aSurface->GetBaseAddress();

    for (int32_t i = 0; i < aSize.height; ++i) {
      memcpy(aData + i * aStride, ioData + i * bytesPerRow, aSize.width * 4);
    }
  }

  return NS_OK;
}

already_AddRefed<SourceSurface> CreateSourceSurfaceFromMacIOSurface(
    MacIOSurface* aSurface) {
  aSurface->Lock();
  auto scopeExit = MakeScopeExit([&]() { aSurface->Unlock(); });

  size_t ioWidth = aSurface->GetDevicePixelWidth();
  size_t ioHeight = aSurface->GetDevicePixelHeight();
  IntSize size((int32_t)ioWidth, (int32_t)ioHeight);
  SurfaceFormat ioFormat = aSurface->GetFormat();

  SurfaceFormat format =
      (ioFormat == SurfaceFormat::NV12 || ioFormat == SurfaceFormat::YUV422)
          ? SurfaceFormat::B8G8R8X8
          : SurfaceFormat::B8G8R8A8;

  RefPtr<DataSourceSurface> dataSurface =
      Factory::CreateDataSourceSurface(size, format);
  if (NS_WARN_IF(!dataSurface)) {
    return nullptr;
  }

  DataSourceSurface::ScopedMap map(dataSurface, DataSourceSurface::WRITE);
  if (NS_WARN_IF(!map.IsMapped())) {
    return nullptr;
  }

  nsresult rv = CopyFromLockedMacIOSurface(aSurface, map.GetData(),
                                           map.GetStride(), size, format);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  return dataSurface.forget();
}

nsresult CreateSurfaceDescriptorBufferFromMacIOSurface(
    MacIOSurface* aSurface, SurfaceDescriptorBuffer& aSdBuffer,
    Image::BuildSdbFlags aFlags,
    const std::function<MemoryOrShmem(uint32_t)>& aAllocate) {
  aSurface->Lock();
  auto scopeExit = MakeScopeExit([&]() { aSurface->Unlock(); });

  size_t ioWidth = aSurface->GetDevicePixelWidth();
  size_t ioHeight = aSurface->GetDevicePixelHeight();
  IntSize size((int32_t)ioWidth, (int32_t)ioHeight);
  SurfaceFormat ioFormat = aSurface->GetFormat();

  SurfaceFormat format =
      (ioFormat == SurfaceFormat::NV12 || ioFormat == SurfaceFormat::YUV422)
          ? SurfaceFormat::B8G8R8X8
          : SurfaceFormat::B8G8R8A8;

  uint8_t* buffer = nullptr;
  int32_t stride = 0;
  nsresult rv = Image::AllocateSurfaceDescriptorBufferRgb(
      size, format, buffer, aSdBuffer, stride, aAllocate);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return CopyFromLockedMacIOSurface(aSurface, buffer, stride, size, format);
}

}  // namespace layers
}  // namespace mozilla
