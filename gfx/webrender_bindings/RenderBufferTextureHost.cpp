/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
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

wr::WrExternalImage
RenderBufferTextureHost::Lock(uint8_t aChannelIndex, gl::GLContext* aGL)
{
  if (!mLocked) {
    if (!GetBuffer()) {
      // We hit some problems to get the shmem.
      return InvalidToWrExternalImage();
    }
    if (mFormat != gfx::SurfaceFormat::YUV) {
      mSurface = gfx::Factory::CreateWrappingDataSourceSurface(GetBuffer(),
                                                               layers::ImageDataSerializer::GetRGBStride(mDescriptor.get_RGBDescriptor()),
                                                               mSize,
                                                               mFormat);
      if (NS_WARN_IF(!mSurface)) {
        return InvalidToWrExternalImage();
      }
      if (NS_WARN_IF(!mSurface->Map(gfx::DataSourceSurface::MapType::READ_WRITE, &mMap))) {
        mSurface = nullptr;
        return InvalidToWrExternalImage();
      }
    } else {
      const layers::YCbCrDescriptor& desc = mDescriptor.get_YCbCrDescriptor();

      mYSurface = gfx::Factory::CreateWrappingDataSourceSurface(layers::ImageDataSerializer::GetYChannel(GetBuffer(), desc),
                                                                desc.yStride(),
                                                                desc.ySize(),
                                                                gfx::SurfaceFormat::A8);
      mCbSurface = gfx::Factory::CreateWrappingDataSourceSurface(layers::ImageDataSerializer::GetCbChannel(GetBuffer(), desc),
                                                                 desc.cbCrStride(),
                                                                 desc.cbCrSize(),
                                                                 gfx::SurfaceFormat::A8);
      mCrSurface = gfx::Factory::CreateWrappingDataSourceSurface(layers::ImageDataSerializer::GetCrChannel(GetBuffer(), desc),
                                                                 desc.cbCrStride(),
                                                                 desc.cbCrSize(),
                                                                 gfx::SurfaceFormat::A8);
      if (NS_WARN_IF(!mYSurface || !mCbSurface || !mCrSurface)) {
        mYSurface = mCbSurface = mCrSurface = nullptr;
        return InvalidToWrExternalImage();
      }
      if (NS_WARN_IF(!mYSurface->Map(gfx::DataSourceSurface::MapType::READ_WRITE, &mYMap) ||
                     !mCbSurface->Map(gfx::DataSourceSurface::MapType::READ_WRITE, &mCbMap) ||
                     !mCrSurface->Map(gfx::DataSourceSurface::MapType::READ_WRITE, &mCrMap))) {
        mYSurface = mCbSurface = mCrSurface = nullptr;
        return InvalidToWrExternalImage();
      }
    }
    mLocked = true;
  }

  RenderBufferData data = GetBufferDataForRender(aChannelIndex);
  return RawDataToWrExternalImage(data.mData, data.mBufferSize);
}

void
RenderBufferTextureHost::Unlock()
{
  if (mLocked) {
    if (mSurface) {
      mSurface->Unmap();
      mSurface = nullptr;
    } else if (mYSurface) {
      mYSurface->Unmap();
      mCbSurface->Unmap();
      mCrSurface->Unmap();
      mYSurface = mCbSurface = mCrSurface = nullptr;
    }
    mLocked = false;
  }
}

RenderBufferTextureHost::RenderBufferData
RenderBufferTextureHost::GetBufferDataForRender(uint8_t aChannelIndex)
{
  MOZ_ASSERT(mFormat != gfx::SurfaceFormat::YUV || aChannelIndex < 3);
  MOZ_ASSERT(mFormat == gfx::SurfaceFormat::YUV || aChannelIndex < 1);
  MOZ_ASSERT(mLocked);

  if (mFormat != gfx::SurfaceFormat::YUV) {
    MOZ_ASSERT(mSurface);

    return RenderBufferData(mMap.mData, mMap.mStride * mSurface->GetSize().height);
  } else {
    MOZ_ASSERT(mYSurface && mCbSurface && mCrSurface);

    switch (aChannelIndex) {
      case 0:
        return RenderBufferData(mYMap.mData, mYMap.mStride * mYSurface->GetSize().height);
        break;
      case 1:
        return RenderBufferData(mCbMap.mData, mCbMap.mStride * mCbSurface->GetSize().height);
        break;
      case 2:
        return RenderBufferData(mCrMap.mData, mCrMap.mStride * mCrSurface->GetSize().height);
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("unexpected to be called");
        return RenderBufferData(nullptr, 0);
    }
  }
}

} // namespace wr
} // namespace mozilla
