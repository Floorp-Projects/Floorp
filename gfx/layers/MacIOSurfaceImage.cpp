/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MacIOSurfaceImage.h"
#include "mozilla/layers/CompositableClient.h"
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/layers/MacIOSurfaceTextureClientOGL.h"
#include "mozilla/UniquePtr.h"
#include "YCbCrUtils.h"

using namespace mozilla;
using namespace mozilla::layers;
using namespace mozilla::gfx;

TextureClient*
MacIOSurfaceImage::GetTextureClient(CompositableClient* aClient)
{
  if (!mTextureClient) {
    mTextureClient = TextureClient::CreateWithData(
      MacIOSurfaceTextureData::Create(mSurface),
      TextureFlags::DEFAULT,
      aClient->GetForwarder()
    );
  }
  return mTextureClient;
}

already_AddRefed<SourceSurface>
MacIOSurfaceImage::GetAsSourceSurface()
{
  RefPtr<DataSourceSurface> dataSurface;
  mSurface->Lock();
  size_t bytesPerRow = mSurface->GetBytesPerRow();
  size_t ioWidth = mSurface->GetDevicePixelWidth();
  size_t ioHeight = mSurface->GetDevicePixelHeight();

  SurfaceFormat format = mSurface->GetFormat() == SurfaceFormat::NV12 ? SurfaceFormat::B8G8R8X8 : SurfaceFormat::B8G8R8A8;

  dataSurface = Factory::CreateDataSourceSurface(IntSize(ioWidth, ioHeight), format);
  if (NS_WARN_IF(!dataSurface)) {
    return nullptr;
  }

  DataSourceSurface::MappedSurface mappedSurface;
  if (!dataSurface->Map(DataSourceSurface::WRITE, &mappedSurface))
    return nullptr;

  if (mSurface->GetFormat() == SurfaceFormat::NV12) {
    if (mSurface->GetDevicePixelWidth() > PlanarYCbCrImage::MAX_DIMENSION ||
        mSurface->GetDevicePixelHeight() > PlanarYCbCrImage::MAX_DIMENSION) {
      return nullptr;
    }

    /* Extract and separate the CbCr planes */
    size_t cbCrStride = mSurface->GetBytesPerRow(1);
    size_t cbCrWidth = mSurface->GetDevicePixelWidth(1);
    size_t cbCrHeight = mSurface->GetDevicePixelHeight(1);

    auto cbPlane = MakeUnique<uint8_t[]>(cbCrWidth * cbCrHeight);
    auto crPlane = MakeUnique<uint8_t[]>(cbCrWidth * cbCrHeight);

    uint8_t* src = (uint8_t*)mSurface->GetBaseAddressOfPlane(1);
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
    data.mYChannel = (uint8_t*)mSurface->GetBaseAddressOfPlane(0);
    data.mYStride = mSurface->GetBytesPerRow(0);
    data.mYSize = IntSize(mSurface->GetDevicePixelWidth(0), mSurface->GetDevicePixelHeight(0));
    data.mCbChannel = cbPlane.get();
    data.mCrChannel = crPlane.get();
    data.mCbCrStride = cbCrWidth;
    data.mCbCrSize = IntSize(cbCrWidth, cbCrHeight);
    data.mPicSize = data.mYSize;

    ConvertYCbCrToRGB(data, SurfaceFormat::B8G8R8X8, IntSize(ioWidth, ioHeight), mappedSurface.mData, mappedSurface.mStride);
  } else {
    unsigned char* ioData = (unsigned char*)mSurface->GetBaseAddress();

    for (size_t i = 0; i < ioHeight; ++i) {
      memcpy(mappedSurface.mData + i * mappedSurface.mStride,
             ioData + i * bytesPerRow,
             ioWidth * 4);
    }
  }

  dataSurface->Unmap();
  mSurface->Unlock();

  return dataSurface.forget();
}
