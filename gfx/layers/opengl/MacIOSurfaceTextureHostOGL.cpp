/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MacIOSurfaceTextureHostOGL.h"
#include "mozilla/gfx/MacIOSurface.h"
#include "GLContextCGL.h"

namespace mozilla {
namespace layers {

MacIOSurfaceTextureHostOGL::MacIOSurfaceTextureHostOGL(TextureFlags aFlags,
                                                       const SurfaceDescriptorMacIOSurface& aDescriptor)
  : TextureHost(aFlags)
{
  MOZ_COUNT_CTOR(MacIOSurfaceTextureHostOGL);
  mSurface = MacIOSurface::LookupSurface(aDescriptor.surfaceId(),
                                         aDescriptor.scaleFactor(),
                                         !aDescriptor.isOpaque());
}

MacIOSurfaceTextureHostOGL::~MacIOSurfaceTextureHostOGL()
{
  MOZ_COUNT_DTOR(MacIOSurfaceTextureHostOGL);
}

GLTextureSource*
MacIOSurfaceTextureHostOGL::CreateTextureSourceForPlane(size_t aPlane)
{
  GLuint textureHandle;
  gl::GLContext* gl = mProvider->GetGLContext();
  gl->fGenTextures(1, &textureHandle);
  gl->fBindTexture(LOCAL_GL_TEXTURE_RECTANGLE_ARB, textureHandle);
  gl->fTexParameteri(LOCAL_GL_TEXTURE_RECTANGLE_ARB, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);
  gl->fTexParameteri(LOCAL_GL_TEXTURE_RECTANGLE_ARB, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);

  mSurface->CGLTexImageIOSurface2D(gl::GLContextCGL::Cast(gl)->GetCGLContext(), aPlane);

  return new GLTextureSource(mProvider, textureHandle, LOCAL_GL_TEXTURE_RECTANGLE_ARB,
                             gfx::IntSize(mSurface->GetDevicePixelWidth(aPlane),
                                          mSurface->GetDevicePixelHeight(aPlane)),
                             // XXX: This isn't really correct (but isn't used), we should be using the
                             // format of the individual plane, not of the whole buffer.
                             mSurface->GetFormat());
}

bool
MacIOSurfaceTextureHostOGL::Lock()
{
  if (!gl() || !gl()->MakeCurrent() || !mSurface) {
    return false;
  }

  if (!mTextureSource) {
    mTextureSource = CreateTextureSourceForPlane(0);

    RefPtr<TextureSource> prev = mTextureSource;
    for (size_t i = 1; i < mSurface->GetPlaneCount(); i++) {
      RefPtr<TextureSource> next = CreateTextureSourceForPlane(i);
      prev->SetNextSibling(next);
      prev = next;
    }
  }
  return true;
}

void
MacIOSurfaceTextureHostOGL::SetTextureSourceProvider(TextureSourceProvider* aProvider)
{
  if (!aProvider || !aProvider->GetGLContext()) {
    mTextureSource = nullptr;
    mProvider = nullptr;
    return;
  }

  if (mProvider != aProvider) {
    // Cannot share GL texture identifiers across compositors.
    mTextureSource = nullptr;
  }

  mProvider = aProvider;
}

gfx::SurfaceFormat
MacIOSurfaceTextureHostOGL::GetFormat() const {
  return mSurface->GetFormat();
}

gfx::SurfaceFormat
MacIOSurfaceTextureHostOGL::GetReadFormat() const {
  return mSurface->GetReadFormat();
}

gfx::IntSize
MacIOSurfaceTextureHostOGL::GetSize() const {
  if (!mSurface) {
    return gfx::IntSize();
  }
  return gfx::IntSize(mSurface->GetDevicePixelWidth(),
                      mSurface->GetDevicePixelHeight());
}

gl::GLContext*
MacIOSurfaceTextureHostOGL::gl() const
{
  return mProvider ? mProvider->GetGLContext() : nullptr;
}

} // namespace layers
} // namespace mozilla
