/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_X11TEXTURESOURCEOGL__H
#define MOZILLA_GFX_X11TEXTURESOURCEOGL__H

#ifdef GL_PROVIDER_GLX

#include "mozilla/layers/CompositorOGL.h"
#include "mozilla/layers/TextureHostOGL.h"
#include "mozilla/gfx/2D.h"

namespace mozilla {
namespace layers {

// TextureSource for Xlib-backed surfaces.
class X11TextureSourceOGL
  : public TextureSourceOGL
  , public TextureSource
{
public:
  X11TextureSourceOGL(CompositorOGL* aCompositor, gfxXlibSurface* aSurface);
  ~X11TextureSourceOGL();

  virtual X11TextureSourceOGL* AsSourceOGL() MOZ_OVERRIDE { return this; }

  virtual bool IsValid() const MOZ_OVERRIDE { return !!gl(); } ;

  virtual void BindTexture(GLenum aTextureUnit, gfx::Filter aFilter) MOZ_OVERRIDE;

  virtual gfx::IntSize GetSize() const MOZ_OVERRIDE;

  virtual GLenum GetTextureTarget() const MOZ_OVERRIDE { return LOCAL_GL_TEXTURE_2D; }

  virtual gfx::SurfaceFormat GetFormat() const MOZ_OVERRIDE;

  virtual GLenum GetWrapMode() const MOZ_OVERRIDE { return LOCAL_GL_CLAMP_TO_EDGE; }

  virtual void DeallocateDeviceData() MOZ_OVERRIDE;

  virtual void SetCompositor(Compositor* aCompositor) MOZ_OVERRIDE;

  gl::GLContext* gl() const;

  static gfx::SurfaceFormat ContentTypeToSurfaceFormat(gfxContentType aType);

protected:
  RefPtr<CompositorOGL> mCompositor;
  nsRefPtr<gfxXlibSurface> mSurface;
  RefPtr<gfx::SourceSurface> mSourceSurface;
  GLuint mTexture;
};

} // namespace layers
} // namespace mozilla

#endif

#endif // MOZILLA_GFX_X11TEXTURESOURCEOGL__H
