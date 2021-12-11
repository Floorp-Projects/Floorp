/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RENDEREXTERNALTEXTUREHOST_H
#define MOZILLA_GFX_RENDEREXTERNALTEXTUREHOST_H

#include "mozilla/layers/TextureHostOGL.h"
#include "RenderTextureHostSWGL.h"

namespace mozilla {
namespace wr {

/**
 * RenderExternalTextureHost manages external textures used by WebRender on Mac.
 * The motivation for this is to be able to use Apple Client Storage OpenGL
 * extension, which makes it possible to avoid some copies during texture
 * upload. This is especially helpful for high resolution video.
 */
class RenderExternalTextureHost final : public RenderTextureHostSWGL {
 public:
  RenderExternalTextureHost(uint8_t* aBuffer,
                            const layers::BufferDescriptor& aDescriptor);

  wr::WrExternalImage Lock(uint8_t aChannelIndex, gl::GLContext* aGL,
                           wr::ImageRendering aRendering) override;
  void Unlock() override;
  void PrepareForUse() override;
  size_t Bytes() override {
    return mSize.width * mSize.height * BytesPerPixel(mFormat);
  }

  // RenderTextureHostSWGL
  size_t GetPlaneCount() const override;

  gfx::SurfaceFormat GetFormat() const override;

  gfx::ColorDepth GetColorDepth() const override;

  gfx::YUVRangedColorSpace GetYUVColorSpace() const override;

  bool MapPlane(RenderCompositor* aCompositor, uint8_t aChannelIndex,
                PlaneInfo& aPlaneInfo) override;

  void UnmapPlanes() override;

 private:
  ~RenderExternalTextureHost();

  bool CreateSurfaces();
  void DeleteSurfaces();
  void DeleteTextures();

  uint8_t* GetBuffer() const { return mBuffer; }
  bool InitializeIfNeeded();
  bool IsReadyForDeletion();
  bool IsYUV() const { return mFormat == gfx::SurfaceFormat::YUV; }
  size_t PlaneCount() const { return IsYUV() ? 3 : 1; }
  void UpdateTexture(size_t aIndex);
  void UpdateTextures(wr::ImageRendering aRendering);

  uint8_t* mBuffer;
  layers::BufferDescriptor mDescriptor;

  bool mInitialized;
  bool mTextureUpdateNeeded;

  gfx::IntSize mSize;
  gfx::SurfaceFormat mFormat;

  RefPtr<gl::GLContext> mGL;
  RefPtr<gfx::DataSourceSurface> mSurfaces[3];
  RefPtr<layers::DirectMapTextureSource> mTextureSources[3];
  wr::WrExternalImage mImages[3];
};

}  // namespace wr
}  // namespace mozilla

#endif  // MOZILLA_GFX_RENDEREXTERNALTEXTUREHOST_H
