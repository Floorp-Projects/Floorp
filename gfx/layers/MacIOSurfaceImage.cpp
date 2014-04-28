/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MacIOSurfaceImage.h"
#include "mozilla/layers/MacIOSurfaceTextureClientOGL.h"

using namespace mozilla;
using namespace mozilla::layers;

TextureClient*
MacIOSurfaceImage::GetTextureClient(CompositableClient* aClient)
{
  if (!mTextureClient) {
    RefPtr<MacIOSurfaceTextureClientOGL> buffer =
      new MacIOSurfaceTextureClientOGL(TextureFlags::DEFAULT);
    buffer->InitWith(mSurface);
    mTextureClient = buffer;
  }
  return mTextureClient;
}

TemporaryRef<gfx::SourceSurface>
MacIOSurfaceImage::GetAsSourceSurface()
{
  mSurface->Lock();
  size_t bytesPerRow = mSurface->GetBytesPerRow();
  size_t ioWidth = mSurface->GetDevicePixelWidth();
  size_t ioHeight = mSurface->GetDevicePixelHeight();

  unsigned char* ioData = (unsigned char*)mSurface->GetBaseAddress();

  RefPtr<gfx::DataSourceSurface> dataSurface
    = gfx::Factory::CreateDataSourceSurface(gfx::IntSize(ioWidth, ioHeight), gfx::SurfaceFormat::B8G8R8A8);
  if (!dataSurface)
    return nullptr;

  gfx::DataSourceSurface::MappedSurface mappedSurface;
  if (!dataSurface->Map(gfx::DataSourceSurface::WRITE, &mappedSurface))
    return nullptr;

  for (size_t i = 0; i < ioHeight; ++i) {
    memcpy(mappedSurface.mData + i * mappedSurface.mStride,
           ioData + i * bytesPerRow,
           ioWidth * 4);
  }

  dataSurface->Unmap();
  mSurface->Unlock();

  return dataSurface;
}
