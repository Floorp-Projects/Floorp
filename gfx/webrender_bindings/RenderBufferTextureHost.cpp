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

RenderBufferTextureHost::RenderBufferTextureHost(
    uint8_t* aBuffer, const layers::BufferDescriptor& aDescriptor)
    : mBuffer(aBuffer),
      mDescriptor(aDescriptor),
      mMap(),
      mYMap(),
      mCbMap(),
      mCrMap(),
      mLocked(false) {
  MOZ_COUNT_CTOR_INHERITED(RenderBufferTextureHost, RenderTextureHost);

  switch (mDescriptor.type()) {
    case layers::BufferDescriptor::TYCbCrDescriptor: {
      const layers::YCbCrDescriptor& ycbcr = mDescriptor.get_YCbCrDescriptor();
      mSize = ycbcr.display().Size();
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
      gfxCriticalError() << "Bad buffer host descriptor "
                         << (int)mDescriptor.type();
      MOZ_CRASH("GFX: Bad descriptor");
  }
}

RenderBufferTextureHost::~RenderBufferTextureHost() {
  MOZ_COUNT_DTOR_INHERITED(RenderBufferTextureHost, RenderTextureHost);
}

wr::WrExternalImage RenderBufferTextureHost::Lock(
    uint8_t aChannelIndex, gl::GLContext* aGL, wr::ImageRendering aRendering) {
  if (!mLocked) {
    if (!GetBuffer()) {
      // We hit some problems to get the shmem.
      gfxCriticalNote << "GetBuffer Failed";
      return InvalidToWrExternalImage();
    }
    if (mFormat != gfx::SurfaceFormat::YUV) {
      mSurface = gfx::Factory::CreateWrappingDataSourceSurface(
          GetBuffer(),
          layers::ImageDataSerializer::GetRGBStride(
              mDescriptor.get_RGBDescriptor()),
          mSize, mFormat);
      if (NS_WARN_IF(!mSurface)) {
        gfxCriticalNote << "DataSourceSurface is null";
        return InvalidToWrExternalImage();
      }
      if (NS_WARN_IF(
              !mSurface->Map(gfx::DataSourceSurface::MapType::READ, &mMap))) {
        mSurface = nullptr;
        gfxCriticalNote << "Failed to map Surface";
        return InvalidToWrExternalImage();
      }
    } else {
      const layers::YCbCrDescriptor& desc = mDescriptor.get_YCbCrDescriptor();
      auto cbcrSize = layers::ImageDataSerializer::GetCroppedCbCrSize(desc);

      mYSurface = gfx::Factory::CreateWrappingDataSourceSurface(
          layers::ImageDataSerializer::GetYChannel(GetBuffer(), desc),
          desc.yStride(), desc.display().Size(), gfx::SurfaceFormat::A8);
      mCbSurface = gfx::Factory::CreateWrappingDataSourceSurface(
          layers::ImageDataSerializer::GetCbChannel(GetBuffer(), desc),
          desc.cbCrStride(), cbcrSize, gfx::SurfaceFormat::A8);
      mCrSurface = gfx::Factory::CreateWrappingDataSourceSurface(
          layers::ImageDataSerializer::GetCrChannel(GetBuffer(), desc),
          desc.cbCrStride(), cbcrSize, gfx::SurfaceFormat::A8);
      if (NS_WARN_IF(!mYSurface || !mCbSurface || !mCrSurface)) {
        mYSurface = mCbSurface = mCrSurface = nullptr;
        gfxCriticalNote << "YCbCr Surface is null";
        return InvalidToWrExternalImage();
      }
      if (NS_WARN_IF(
              !mYSurface->Map(gfx::DataSourceSurface::MapType::READ, &mYMap) ||
              !mCbSurface->Map(gfx::DataSourceSurface::MapType::READ,
                               &mCbMap) ||
              !mCrSurface->Map(gfx::DataSourceSurface::MapType::READ,
                               &mCrMap))) {
        mYSurface = mCbSurface = mCrSurface = nullptr;
        gfxCriticalNote << "Failed to map YCbCr Surface";
        return InvalidToWrExternalImage();
      }
    }
    mLocked = true;
  }

  RenderBufferData data = GetBufferDataForRender(aChannelIndex);
  return RawDataToWrExternalImage(data.mData, data.mBufferSize);
}

void RenderBufferTextureHost::Unlock() {
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
RenderBufferTextureHost::GetBufferDataForRender(uint8_t aChannelIndex) {
  MOZ_ASSERT(mFormat != gfx::SurfaceFormat::YUV || aChannelIndex < 3);
  MOZ_ASSERT(mFormat == gfx::SurfaceFormat::YUV || aChannelIndex < 1);
  MOZ_ASSERT(mLocked);

  if (mFormat != gfx::SurfaceFormat::YUV) {
    MOZ_ASSERT(mSurface);

    return RenderBufferData(mMap.mData,
                            mMap.mStride * mSurface->GetSize().height);
  } else {
    MOZ_ASSERT(mYSurface && mCbSurface && mCrSurface);

    switch (aChannelIndex) {
      case 0:
        return RenderBufferData(mYMap.mData,
                                mYMap.mStride * mYSurface->GetSize().height);
        break;
      case 1:
        return RenderBufferData(mCbMap.mData,
                                mCbMap.mStride * mCbSurface->GetSize().height);
        break;
      case 2:
        return RenderBufferData(mCrMap.mData,
                                mCrMap.mStride * mCrSurface->GetSize().height);
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("unexpected to be called");
        return RenderBufferData(nullptr, 0);
    }
  }
}

size_t RenderBufferTextureHost::GetPlaneCount() const {
  switch (mDescriptor.type()) {
    case layers::BufferDescriptor::TYCbCrDescriptor:
      return 3;
    default:
      return 1;
  }
}

gfx::SurfaceFormat RenderBufferTextureHost::GetFormat() const {
  switch (mDescriptor.type()) {
    case layers::BufferDescriptor::TYCbCrDescriptor:
      return gfx::SurfaceFormat::YUV;
    default:
      return mDescriptor.get_RGBDescriptor().format();
  }
}

gfx::ColorDepth RenderBufferTextureHost::GetColorDepth() const {
  switch (mDescriptor.type()) {
    case layers::BufferDescriptor::TYCbCrDescriptor:
      return mDescriptor.get_YCbCrDescriptor().colorDepth();
    default:
      return gfx::ColorDepth::COLOR_8;
  }
}

gfx::YUVRangedColorSpace RenderBufferTextureHost::GetYUVColorSpace() const {
  switch (mDescriptor.type()) {
    case layers::BufferDescriptor::TYCbCrDescriptor:
      return gfx::GetYUVRangedColorSpace(mDescriptor.get_YCbCrDescriptor());
    default:
      return gfx::YUVRangedColorSpace::Default;
  }
}

bool RenderBufferTextureHost::MapPlane(RenderCompositor* aCompositor,
                                       uint8_t aChannelIndex,
                                       PlaneInfo& aPlaneInfo) {
  if (!mBuffer) {
    // We hit some problems to get the shmem.
    gfxCriticalNote << "GetBuffer Failed";
    return false;
  }

  switch (mDescriptor.type()) {
    case layers::BufferDescriptor::TYCbCrDescriptor: {
      const layers::YCbCrDescriptor& desc = mDescriptor.get_YCbCrDescriptor();
      switch (aChannelIndex) {
        case 0:
          aPlaneInfo.mData =
              layers::ImageDataSerializer::GetYChannel(mBuffer, desc);
          aPlaneInfo.mStride = desc.yStride();
          aPlaneInfo.mSize = desc.display().Size();
          break;
        case 1:
          aPlaneInfo.mData =
              layers::ImageDataSerializer::GetCbChannel(mBuffer, desc);
          aPlaneInfo.mStride = desc.cbCrStride();
          aPlaneInfo.mSize =
              layers::ImageDataSerializer::GetCroppedCbCrSize(desc);
          break;
        case 2:
          aPlaneInfo.mData =
              layers::ImageDataSerializer::GetCrChannel(mBuffer, desc);
          aPlaneInfo.mStride = desc.cbCrStride();
          aPlaneInfo.mSize =
              layers::ImageDataSerializer::GetCroppedCbCrSize(desc);
          break;
      }
      break;
    }
    default: {
      const layers::RGBDescriptor& desc = mDescriptor.get_RGBDescriptor();
      aPlaneInfo.mData = mBuffer;
      aPlaneInfo.mStride = layers::ImageDataSerializer::GetRGBStride(desc);
      aPlaneInfo.mSize = desc.size();
      break;
    }
  }
  return true;
}

void RenderBufferTextureHost::UnmapPlanes() {}

}  // namespace wr
}  // namespace mozilla
