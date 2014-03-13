/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef GL_PROVIDER_GLX

#include "X11TextureSourceOGL.h"
#include "mozilla/layers/CompositorOGL.h"
#include "gfxXlibSurface.h"
#include "gfx2DGlue.h"

using namespace mozilla;
using namespace mozilla::layers;
using namespace mozilla::gfx;

X11TextureSourceOGL::X11TextureSourceOGL(CompositorOGL* aCompositor, gfxXlibSurface* aSurface)
  : mCompositor(aCompositor)
  , mSurface(aSurface)
  , mTexture(0)
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
X11TextureSourceOGL::BindTexture(GLenum aTextureUnit, gfx::Filter aFilter)
{
  gl()->fActiveTexture(aTextureUnit);

  if (!mTexture) {
    gl()->fGenTextures(1, &mTexture);

    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);

    gl::sGLXLibrary.BindTexImage(mSurface->XDisplay(), mSurface->GetGLXPixmap());
  } else {
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);
    gl::sGLXLibrary.UpdateTexImage(mSurface->XDisplay(), mSurface->GetGLXPixmap());
  }

  ApplyFilterToBoundTexture(gl(), aFilter, LOCAL_GL_TEXTURE_RECTANGLE_ARB);
}

IntSize
X11TextureSourceOGL::GetSize() const
{
  return ToIntSize(mSurface->GetSize());
}

SurfaceFormat
X11TextureSourceOGL::GetFormat() const {
  gfxContentType type = mSurface->GetContentType();
  return X11TextureSourceOGL::ContentTypeToSurfaceFormat(type);
}

void
X11TextureSourceOGL::SetCompositor(Compositor* aCompositor)
{
  MOZ_ASSERT(!aCompositor || aCompositor->GetBackendType() == LayersBackend::LAYERS_OPENGL);
  if (mCompositor == aCompositor) {
    return;
  }
  DeallocateDeviceData();
  mCompositor = static_cast<CompositorOGL*>(aCompositor);
}

gl::GLContext*
X11TextureSourceOGL::gl() const
{
  return mCompositor ? mCompositor->gl() : nullptr;
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

#endif
