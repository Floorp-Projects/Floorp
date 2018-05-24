/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "X11TextureSourceOGL.h"
#include "gfxXlibSurface.h"
#include "gfx2DGlue.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

X11TextureSourceOGL::X11TextureSourceOGL(CompositorOGL* aCompositor, gfxXlibSurface* aSurface)
  : mGL(aCompositor->gl())
  , mSurface(aSurface)
  , mTexture(0)
  , mUpdated(false)
{
}

X11TextureSourceOGL::~X11TextureSourceOGL()
{
  DeallocateDeviceData();
}

void
X11TextureSourceOGL::DeallocateDeviceData()
{
  if (mTexture) {
    if (gl() && gl()->MakeCurrent()) {
      gl::sGLXLibrary.ReleaseTexImage(mSurface->XDisplay(), mSurface->GetGLXPixmap());
      gl()->fDeleteTextures(1, &mTexture);
      mTexture = 0;
    }
  }
}

void
X11TextureSourceOGL::BindTexture(GLenum aTextureUnit,
                                 gfx::SamplingFilter aSamplingFilter)
{
  gl()->fActiveTexture(aTextureUnit);

  if (!mTexture) {
    gl()->fGenTextures(1, &mTexture);

    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);

    gl::sGLXLibrary.BindTexImage(mSurface->XDisplay(), mSurface->GetGLXPixmap());
  } else {
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);
    if (mUpdated) {
      gl::sGLXLibrary.UpdateTexImage(mSurface->XDisplay(), mSurface->GetGLXPixmap());
      mUpdated = false;
    }
  }

  ApplySamplingFilterToBoundTexture(gl(), aSamplingFilter, LOCAL_GL_TEXTURE_2D);
}

IntSize
X11TextureSourceOGL::GetSize() const
{
  return mSurface->GetSize();
}

SurfaceFormat
X11TextureSourceOGL::GetFormat() const {
  gfxContentType type = mSurface->GetContentType();
  return X11TextureSourceOGL::ContentTypeToSurfaceFormat(type);
}

void
X11TextureSourceOGL::SetTextureSourceProvider(TextureSourceProvider* aProvider)
{
  gl::GLContext* newGL = aProvider ? aProvider->GetGLContext() : nullptr;
  if (mGL != newGL) {
    DeallocateDeviceData();
  }
  mGL = newGL;
}

SurfaceFormat
X11TextureSourceOGL::ContentTypeToSurfaceFormat(gfxContentType aType)
{
  // X11 uses a switched format and the OGL compositor
  // doesn't support ALPHA / A8.
  switch (aType) {
    case gfxContentType::COLOR:
      return SurfaceFormat::R8G8B8X8;
    case gfxContentType::COLOR_ALPHA:
      return SurfaceFormat::R8G8B8A8;
    default:
      return SurfaceFormat::UNKNOWN;
  }
}

}
}

