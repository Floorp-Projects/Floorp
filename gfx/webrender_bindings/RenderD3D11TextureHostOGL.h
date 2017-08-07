/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RENDERD3D11TEXTUREHOSTOGL_H
#define MOZILLA_GFX_RENDERD3D11TEXTUREHOSTOGL_H

#include "RenderTextureHostOGL.h"

namespace mozilla {

namespace wr {

class RenderDXGITextureHostOGL final : public RenderTextureHostOGL
{
public:
  explicit RenderDXGITextureHostOGL(WindowsHandle aHandle,
                                    gfx::SurfaceFormat aFormat,
                                    gfx::IntSize aSize);

  virtual void SetGLContext(gl::GLContext* aContext) override;

  virtual bool Lock() override;
  virtual void Unlock() override;

  virtual gfx::IntSize GetSize(uint8_t aChannelIndex) const;
  virtual GLuint GetGLHandle(uint8_t aChannelIndex) const;

private:
  virtual ~RenderDXGITextureHostOGL();

  // We could use NV12 format for this texture. So, we might have 2 gl texture
  // handles for Y and CbCr data.
  GLuint mTextureHandle[2];

  gfx::SurfaceFormat mFormat;
  gfx::IntSize mSize;
};

} // namespace wr
} // namespace mozilla

#endif // MOZILLA_GFX_RENDERD3D11TEXTUREHOSTOGL_H
