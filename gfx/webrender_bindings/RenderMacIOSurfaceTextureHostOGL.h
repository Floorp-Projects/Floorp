/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
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

class RenderMacIOSurfaceTextureHostOGL final : public RenderTextureHostOGL
{
public:
  explicit RenderMacIOSurfaceTextureHostOGL(MacIOSurface* aSurface);

  virtual bool Lock() override;
  virtual void Unlock() override;

  virtual void SetGLContext(gl::GLContext* aContext) override;

  virtual gfx::IntSize GetSize(uint8_t aChannelIndex) const override;
  virtual GLuint GetGLHandle(uint8_t aChannelIndex) const override;

private:
  virtual ~RenderMacIOSurfaceTextureHostOGL();
  void DeleteTextureHandle();

  RefPtr<MacIOSurface> mSurface;
  RefPtr<gl::GLContext> mGL;
  GLuint mTextureHandles[3];
};

} // namespace wr
} // namespace mozilla

#endif // MOZILLA_GFX_RENDERMACIOSURFACETEXTUREHOSTOGL_H
