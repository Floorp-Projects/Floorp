/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderBufferTextureHost.h"

#include "mozilla/gfx/Logging.h"
#include "mozilla/layers/ImageDataSerializer.h"

namespace mozilla {
namespace wr {

RenderBufferTextureHost::RenderBufferTextureHost(uint8_t* aBuffer,
                                                 const layers::BufferDescriptor& aDescriptor)
  : mBuffer(aBuffer)
  , mDescriptor(aDescriptor)
  , mLocked(false)
{
  MOZ_COUNT_CTOR_INHERITED(RenderBufferTextureHost, RenderTextureHost);

  switch (mDescriptor.type()) {
    case layers::BufferDescriptor::TYCbCrDescriptor: {
      const layers::YCbCrDescriptor& ycbcr = mDescriptor.get_YCbCrDescriptor();
      mSize = ycbcr.ySize();
      mFormat = gfx::SurfaceFormat::YUV;
      break;
    }
    case layers::BufferDescriptor::TRGBDescriptor: {
      const layers::RGBDescriptor& rgb = mDescriptor.get_RGBDescriptor();
      mSize = rgb.size();
      mFormat = rgb.format();
      break;
    }
    default:
      gfxCriticalError() << "Bad buffer host descriptor " << (int)mDescriptor.type();
      MOZ_CRASH("GFX: Bad descriptor");
  }
}

RenderBufferTextureHost::~RenderBufferTextureHost()
{
  MOZ_COUNT_DTOR_INHERITED(RenderBufferTextureHost, RenderTextureHost);
}

already_AddRefed<gfx::DataSourceSurface>
RenderBufferTextureHost::GetAsSurface()
{
  RefPtr<gfx::DataSourceSurface> result;
  if (mFormat == gfx::SurfaceFormat::YUV) {
    result = layers::ImageDataSerializer::DataSourceSurfaceFromYCbCrDescriptor(
      GetBuffer(), mDescriptor.get_YCbCrDescriptor());
    if (NS_WARN_IF(!result)) {
      return nullptr;
    }
  } else {
    result =
      gfx::Factory::CreateWrappingDataSourceSurface(GetBuffer(),
          layers::ImageDataSerializer::GetRGBStride(mDescriptor.get_RGBDescriptor()),
        mSize, mFormat);
  }
  return result.forget();
}

bool
RenderBufferTextureHost::Lock()
{
  MOZ_ASSERT(!mLocked);

  // XXX temporal workaround for YUV handling
  if (!mSurface) {
    mSurface = GetAsSurface();
    if (!mSurface) {
      return false;
    }
  }

  if (NS_WARN_IF(!mSurface->Map(gfx::DataSourceSurface::MapType::READ_WRITE, &mMap))) {
    mSurface = nullptr;
    return false;
  }

  mLocked = true;
  return true;
}

void
RenderBufferTextureHost::Unlock()
{
  MOZ_ASSERT(mLocked);
  mLocked = false;
  if (mSurface) {
    mSurface->Unmap();
  }
  mSurface = nullptr;
}

RenderBufferTextureHost::RenderBufferData
RenderBufferTextureHost::GetBufferDataForRender(uint8_t aChannelIndex)
{
  // TODO: handle multiple channel bufferTextureHost(e.g. yuv textureHost)
  MOZ_ASSERT(aChannelIndex < 1);

  MOZ_ASSERT(mLocked);
  MOZ_ASSERT(mSurface);
  return RenderBufferData(mMap.mData, mMap.mStride * mSurface->GetSize().height);
}

} // namespace wr
} // namespace mozilla
