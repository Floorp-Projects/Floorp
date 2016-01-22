/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MacIOSurfaceHelpers.h"
#include "mozilla/gfx/MacIOSurface.h"
#include "YCbCrUtils.h"

namespace mozilla {

using namespace gfx;

namespace layers {

already_AddRefed<SourceSurface>
CreateSourceSurfaceFromMacIOSurface(MacIOSurface* aSurface)
{
  RefPtr<DataSourceSurface> dataSurface;
  aSurface->Lock();
  size_t bytesPerRow = aSurface->GetBytesPerRow();
  size_t ioWidth = aSurface->GetDevicePixelWidth();
  size_t ioHeight = aSurface->GetDevicePixelHeight();

  SurfaceFormat format = aSurface->GetFormat() == SurfaceFormat::NV12 ? SurfaceFormat::B8G8R8X8 : SurfaceFormat::B8G8R8A8;

  dataSurface = Factory::CreateDataSourceSurface(IntSize(ioWidth, ioHeight), format);
  if (NS_WARN_IF(!dataSurface)) {
    return nullptr;
  }

  DataSourceSurface::MappedSurface mappedSurface;
  if (!dataSurface->Map(DataSourceSurface::WRITE, &mappedSurface))
    return nullptr;

  if (aSurface->GetFormat() == SurfaceFormat::NV12) {
    if (aSurface->GetDevicePixelWidth() > PlanarYCbCrImage::MAX_DIMENSION ||
        aSurface->GetDevicePixelHeight() > PlanarYCbCrImage::MAX_DIMENSION) {
      return nullptr;
    }

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
    data.mYSize = IntSize(aSurface->GetDevicePixelWidth(0), aSurface->GetDevicePixelHeight(0));
    data.mCbChannel = cbPlane.get();
    data.mCrChannel = crPlane.get();
    data.mCbCrStride = cbCrWidth;
    data.mCbCrSize = IntSize(cbCrWidth, cbCrHeight);
    data.mPicSize = data.mYSize;

    ConvertYCbCrToRGB(data, SurfaceFormat::B8G8R8X8, IntSize(ioWidth, ioHeight), mappedSurface.mData, mappedSurface.mStride);
  } else {
    unsigned char* ioData = (unsigned char*)aSurface->GetBaseAddress();

    for (size_t i = 0; i < ioHeight; ++i) {
      memcpy(mappedSurface.mData + i * mappedSurface.mStride,
             ioData + i * bytesPerRow,
             ioWidth * 4);
    }
  }

  dataSurface->Unmap();
  aSurface->Unlock();

  return dataSurface.forget();
}

} // namespace gfx
} // namespace mozilla
