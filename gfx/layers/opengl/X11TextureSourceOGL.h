/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_X11TEXTURESOURCEOGL__H
#define MOZILLA_GFX_X11TEXTURESOURCEOGL__H

#ifdef GL_PROVIDER_GLX

#include "mozilla/layers/CompositorOGL.h"
#include "mozilla/layers/TextureHostOGL.h"
#include "mozilla/layers/X11TextureHost.h"
#include "mozilla/gfx/2D.h"

namespace mozilla {
namespace layers {

// TextureSource for Xlib-backed surfaces.
class X11TextureSourceOGL
  : public TextureSourceOGL
  , public X11TextureSource
{
public:
  X11TextureSourceOGL(CompositorOGL* aCompositor, gfxXlibSurface* aSurface);
  ~X11TextureSourceOGL();

  virtual X11TextureSourceOGL* AsSourceOGL() override { return this; }

  virtual bool IsValid() const override { return !!gl(); } ;

  virtual void BindTexture(GLenum aTextureUnit, gfx::Filter aFilter) override;

  virtual gfx::IntSize GetSize() const override;

  virtual GLenum GetTextureTarget() const override { return LOCAL_GL_TEXTURE_2D; }

  virtual gfx::SurfaceFormat GetFormat() const override;

  virtual GLenum GetWrapMode() const override { return LOCAL_GL_CLAMP_TO_EDGE; }

  virtual void DeallocateDeviceData() override;

  virtual void SetCompositor(Compositor* aCompositor) override;

  virtual void Updated() override { mUpdated = true; }

  gl::GLContext* gl() const;

  static gfx::SurfaceFormat ContentTypeToSurfaceFormat(gfxContentType aType);

protected:
  RefPtr<CompositorOGL> mCompositor;
  nsRefPtr<gfxXlibSurface> mSurface;
  RefPtr<gfx::SourceSurface> mSourceSurface;
  GLuint mTexture;
  bool mUpdated;
};

} // namespace layers
} // namespace mozilla

#endif

#endif // MOZILLA_GFX_X11TEXTURESOURCEOGL__H
