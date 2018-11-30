/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RENDERANDROIDSURFACETEXTUREHOSTOGL_H
#define MOZILLA_GFX_RENDERANDROIDSURFACETEXTUREHOSTOGL_H

#include "GeneratedJNIWrappers.h"
#include "mozilla/layers/TextureHostOGL.h"
#include "RenderTextureHostOGL.h"

namespace mozilla {

namespace wr {

class RenderAndroidSurfaceTextureHostOGL final : public RenderTextureHostOGL {
 public:
  explicit RenderAndroidSurfaceTextureHostOGL(
      const java::GeckoSurfaceTexture::GlobalRef& aSurfTex, gfx::IntSize aSize,
      gfx::SurfaceFormat aFormat, bool aContinuousUpdate);

  wr::WrExternalImage Lock(uint8_t aChannelIndex, gl::GLContext* aGL,
                           wr::ImageRendering aRendering) override;
  void Unlock() override;

  virtual gfx::IntSize GetSize(uint8_t aChannelIndex) const override;
  virtual GLuint GetGLHandle(uint8_t aChannelIndex) const override;

 private:
  virtual ~RenderAndroidSurfaceTextureHostOGL();
  void DeleteTextureHandle();

  const mozilla::java::GeckoSurfaceTexture::GlobalRef mSurfTex;
  const gfx::IntSize mSize;
  // XXX const bool mContinuousUpdate;
  // XXX const bool mIgnoreTransform;

  RefPtr<gl::GLContext> mGL;
};

}  // namespace wr
}  // namespace mozilla

#endif  // MOZILLA_GFX_RENDERANDROIDSURFACETEXTUREHOSTOGL_H
