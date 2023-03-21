/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RENDERBUFFERTEXTUREHOST_H
#define MOZILLA_GFX_RENDERBUFFERTEXTUREHOST_H

#include "RenderTextureHostSWGL.h"

namespace mozilla {
namespace wr {

class RenderBufferTextureHost final : public RenderTextureHostSWGL {
 public:
  RenderBufferTextureHost(uint8_t* aBuffer,
                          const layers::BufferDescriptor& aDescriptor);

  // RenderTextureHost
  wr::WrExternalImage Lock(uint8_t aChannelIndex, gl::GLContext* aGL,
                           wr::ImageRendering aRendering) override;
  void Unlock() override;

  size_t Bytes() override {
    return mSize.width * mSize.height * BytesPerPixel(mFormat);
  }
  class RenderBufferData {
   public:
    RenderBufferData(uint8_t* aData, size_t aBufferSize)
        : mData(aData), mBufferSize(aBufferSize) {}
    const uint8_t* mData;
    size_t mBufferSize;
  };

  RenderBufferData GetBufferDataForRender(uint8_t aChannelIndex);

  // RenderTextureHostSWGL
  size_t GetPlaneCount() const override;

  gfx::SurfaceFormat GetFormat() const override;

  gfx::ColorDepth GetColorDepth() const override;

  gfx::YUVRangedColorSpace GetYUVColorSpace() const override;

  bool MapPlane(RenderCompositor* aCompositor, uint8_t aChannelIndex,
                PlaneInfo& aPlaneInfo) override;

  void UnmapPlanes() override;

 private:
  virtual ~RenderBufferTextureHost();

  uint8_t* GetBuffer() const { return mBuffer; }

  uint8_t* mBuffer;
  layers::BufferDescriptor mDescriptor;
  gfx::IntSize mSize;
  gfx::SurfaceFormat mFormat;

  RefPtr<gfx::DataSourceSurface> mSurface;
  gfx::DataSourceSurface::MappedSurface mMap;

  RefPtr<gfx::DataSourceSurface> mYSurface;
  RefPtr<gfx::DataSourceSurface> mCbSurface;
  RefPtr<gfx::DataSourceSurface> mCrSurface;
  gfx::DataSourceSurface::MappedSurface mYMap;
  gfx::DataSourceSurface::MappedSurface mCbMap;
  gfx::DataSourceSurface::MappedSurface mCrMap;

  bool mLocked;
};

}  // namespace wr
}  // namespace mozilla

#endif  // MOZILLA_GFX_RENDERBUFFERTEXTUREHOST_H
