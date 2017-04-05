/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RENDERTEXTUREHOSTOGL_H
#define MOZILLA_GFX_RENDERTEXTUREHOSTOGL_H

#include "RenderTextureHost.h"

namespace mozilla {
namespace wr {

class RenderMacIOSurfaceTextureHostOGL;

class RenderTextureHostOGL : public RenderTextureHost
{
public:
  RenderTextureHostOGL();

  virtual void SetGLContext(gl::GLContext* aContext) = 0;

  virtual GLuint GetGLHandle() = 0;

  virtual RenderTextureHostOGL* AsTextureHostOGL() { return this; }
  virtual RenderMacIOSurfaceTextureHostOGL* AsMacIOSurfaceTextureHostOGL() { return nullptr; }

protected:
  virtual ~RenderTextureHostOGL();
};

} // namespace wr
} // namespace mozilla

#endif // MOZILLA_GFX_RENDERTEXTUREHOSTOGL_H
