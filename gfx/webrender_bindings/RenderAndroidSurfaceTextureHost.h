/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RENDERANDROIDSURFACETEXTUREHOST_H
#define MOZILLA_GFX_RENDERANDROIDSURFACETEXTUREHOST_H

#include "mozilla/java/GeckoSurfaceTextureWrappers.h"
#include "mozilla/layers/TextureHostOGL.h"
#include "RenderTextureHostSWGL.h"

namespace mozilla {

namespace gfx {
class DataSourceSurface;
}

namespace wr {

class RenderAndroidSurfaceTextureHost final : public RenderTextureHostSWGL {
 public:
  explicit RenderAndroidSurfaceTextureHost(
      const java::GeckoSurfaceTexture::GlobalRef& aSurfTex, gfx::IntSize aSize,
      gfx::SurfaceFormat aFormat, bool aContinuousUpdate,
      Maybe<gfx::Matrix4x4> aTransformOverride);

  wr::WrExternalImage Lock(uint8_t aChannelIndex, gl::GLContext* aGL,
                           wr::ImageRendering aRendering) override;
  void Unlock() override;

  size_t Bytes() override {
    return mSize.width * mSize.height * BytesPerPixel(mFormat);
  }

  void PrepareForUse() override;
  void NotifyForUse() override;
  void NotifyNotUsed() override;

  // RenderTextureHostSWGL
  gfx::SurfaceFormat GetFormat() const override;
  gfx::ColorDepth GetColorDepth() const override {
    return gfx::ColorDepth::COLOR_8;
  }
  size_t GetPlaneCount() const override { return 1; }
  bool MapPlane(RenderCompositor* aCompositor, uint8_t aChannelIndex,
                PlaneInfo& aPlaneInfo) override;
  void UnmapPlanes() override;

  RenderAndroidSurfaceTextureHost* AsRenderAndroidSurfaceTextureHost()
      override {
    return this;
  }

  mozilla::java::GeckoSurfaceTexture::GlobalRef mSurfTex;
  const gfx::IntSize mSize;
  const gfx::SurfaceFormat mFormat;
  // mContinuousUpdate was used for rendering video in the past.
  // It is not used on current gecko.
  const bool mContinuousUpdate;
  const Maybe<gfx::Matrix4x4> mTransformOverride;

 private:
  virtual ~RenderAndroidSurfaceTextureHost();
  bool EnsureAttachedToGLContext();

  already_AddRefed<gfx::DataSourceSurface> ReadTexImage();

  // Returns the UV coordinates to be used when sampling the texture, taking in
  // to account the SurfaceTexture's transform if applicable.
  std::pair<gfx::Point, gfx::Point> GetUvCoords(
      gfx::IntSize aTextureSize) const override;

  enum PrepareStatus {
    STATUS_NONE,
    STATUS_MIGHT_BE_USED_BY_WR,
    STATUS_UPDATE_TEX_IMAGE_NEEDED,
    STATUS_PREPARED
  };

  PrepareStatus mPrepareStatus;
  bool mAttachedToGLContext;

  RefPtr<gl::GLContext> mGL;

  RefPtr<gfx::DataSourceSurface> mReadback;
};

}  // namespace wr
}  // namespace mozilla

#endif  // MOZILLA_GFX_RENDERANDROIDSURFACETEXTUREHOST_H
