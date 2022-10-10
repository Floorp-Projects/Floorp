/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RENDEREGLIMAGETEXTUREHOST_H
#define MOZILLA_GFX_RENDEREGLIMAGETEXTUREHOST_H

#include "mozilla/layers/TextureHostOGL.h"
#include "RenderTextureHost.h"

namespace mozilla {

namespace wr {

// RenderEGLImageTextureHost is created only for SharedSurface_EGLImage that is
// created in parent process.
class RenderEGLImageTextureHost final : public RenderTextureHost {
 public:
  RenderEGLImageTextureHost(EGLImage aImage, EGLSync aSync, gfx::IntSize aSize);

  wr::WrExternalImage Lock(uint8_t aChannelIndex, gl::GLContext* aGL) override;
  void Unlock() override;
  size_t Bytes() override {
    // XXX: we don't have a format so we can't get bpp.
    return mSize.width * mSize.height;
  }

 private:
  virtual ~RenderEGLImageTextureHost();
  void DeleteTextureHandle();

  const EGLImage mImage;
  EGLSync mSync;
  const gfx::IntSize mSize;

  RefPtr<gl::GLContext> mGL;
  GLenum mTextureTarget;
  GLuint mTextureHandle;
};

}  // namespace wr
}  // namespace mozilla

#endif  // MOZILLA_GFX_RENDEREGLIMAGETEXTUREHOST_H
