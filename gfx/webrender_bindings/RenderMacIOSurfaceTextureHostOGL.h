/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RENDERMACIOSURFACETEXTUREHOSTOGL_H
#define MOZILLA_GFX_RENDERMACIOSURFACETEXTUREHOSTOGL_H

#include "mozilla/gfx/MacIOSurface.h"
#include "mozilla/layers/TextureHostOGL.h"
#include "RenderTextureHostOGL.h"

namespace mozilla {

namespace layers {
class SurfaceDescriptorMacIOSurface;
}

namespace wr {

class RenderMacIOSurfaceTextureHostOGL final : public RenderTextureHostOGL {
 public:
  explicit RenderMacIOSurfaceTextureHostOGL(MacIOSurface* aSurface);

  wr::WrExternalImage Lock(uint8_t aChannelIndex, gl::GLContext* aGL,
                           wr::ImageRendering aRendering) override;
  void Unlock() override;

  gfx::IntSize GetSize(uint8_t aChannelIndex) const override;
  GLuint GetGLHandle(uint8_t aChannelIndex) const override;

 private:
  virtual ~RenderMacIOSurfaceTextureHostOGL();
  void DeleteTextureHandle();

  RefPtr<MacIOSurface> mSurface;
  RefPtr<gl::GLContext> mGL;
  GLuint mTextureHandles[3];
};

}  // namespace wr
}  // namespace mozilla

#endif  // MOZILLA_GFX_RENDERMACIOSURFACETEXTUREHOSTOGL_H
