/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderBufferTextureHostSWGL.h"

#include "mozilla/gfx/Logging.h"
#include "mozilla/layers/ImageDataSerializer.h"

namespace mozilla {
namespace wr {

RenderBufferTextureHostSWGL::RenderBufferTextureHostSWGL(
    uint8_t* aBuffer, const layers::BufferDescriptor& aDescriptor)
    : mBuffer(aBuffer), mDescriptor(aDescriptor) {
  MOZ_COUNT_CTOR_INHERITED(RenderBufferTextureHostSWGL, RenderTextureHostSWGL);

  switch (mDescriptor.type()) {
    case layers::BufferDescriptor::TYCbCrDescriptor:
    case layers::BufferDescriptor::TRGBDescriptor:
      MOZ_RELEASE_ASSERT(mBuffer != nullptr);
      break;
    default:
      gfxCriticalError() << "Bad buffer host descriptor "
                         << (int)mDescriptor.type();
      MOZ_CRASH("GFX: Bad descriptor");
  }
}

RenderBufferTextureHostSWGL::~RenderBufferTextureHostSWGL() {
  MOZ_COUNT_DTOR_INHERITED(RenderBufferTextureHostSWGL, RenderTextureHostSWGL);
}

size_t RenderBufferTextureHostSWGL::GetPlaneCount() const {
  switch (mDescriptor.type()) {
    case layers::BufferDescriptor::TYCbCrDescriptor:
      return 3;
    default:
      return 1;
  }
}

gfx::SurfaceFormat RenderBufferTextureHostSWGL::GetFormat() const {
  switch (mDescriptor.type()) {
    case layers::BufferDescriptor::TYCbCrDescriptor:
      return gfx::SurfaceFormat::YUV;
    default:
      return mDescriptor.get_RGBDescriptor().format();
  }
}

gfx::ColorDepth RenderBufferTextureHostSWGL::GetColorDepth() const {
  switch (mDescriptor.type()) {
    case layers::BufferDescriptor::TYCbCrDescriptor:
      return mDescriptor.get_YCbCrDescriptor().colorDepth();
    default:
      return gfx::ColorDepth::COLOR_8;
  }
}

gfx::YUVColorSpace RenderBufferTextureHostSWGL::GetYUVColorSpace() const {
  switch (mDescriptor.type()) {
    case layers::BufferDescriptor::TYCbCrDescriptor:
      return mDescriptor.get_YCbCrDescriptor().yUVColorSpace();
    default:
      return gfx::YUVColorSpace::UNKNOWN;
  }
}

bool RenderBufferTextureHostSWGL::MapPlane(uint8_t aChannelIndex,
                                           PlaneInfo& aPlaneInfo) {
  switch (mDescriptor.type()) {
    case layers::BufferDescriptor::TYCbCrDescriptor: {
      const layers::YCbCrDescriptor& desc = mDescriptor.get_YCbCrDescriptor();
      switch (aChannelIndex) {
        case 0:
          aPlaneInfo.mData =
              layers::ImageDataSerializer::GetYChannel(mBuffer, desc);
          aPlaneInfo.mStride = desc.yStride();
          aPlaneInfo.mSize = desc.ySize();
          break;
        case 1:
          aPlaneInfo.mData =
              layers::ImageDataSerializer::GetCbChannel(mBuffer, desc);
          aPlaneInfo.mStride = desc.cbCrStride();
          aPlaneInfo.mSize = desc.cbCrSize();
          break;
        case 2:
          aPlaneInfo.mData =
              layers::ImageDataSerializer::GetCrChannel(mBuffer, desc);
          aPlaneInfo.mStride = desc.cbCrStride();
          aPlaneInfo.mSize = desc.cbCrSize();
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

void RenderBufferTextureHostSWGL::UnmapPlanes() {}

}  // namespace wr
}  // namespace mozilla
