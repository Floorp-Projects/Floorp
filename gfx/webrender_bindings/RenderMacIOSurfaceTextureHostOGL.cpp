/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderMacIOSurfaceTextureHostOGL.h"

#include "GLContextCGL.h"
#include "mozilla/gfx/Logging.h"
#include "ScopedGLHelpers.h"

namespace mozilla {
namespace wr {

RenderMacIOSurfaceTextureHostOGL::RenderMacIOSurfaceTextureHostOGL(MacIOSurface* aSurface)
  : mTextureHandle(0)
{
  MOZ_COUNT_CTOR_INHERITED(RenderMacIOSurfaceTextureHostOGL, RenderTextureHostOGL);

  mSurface = aSurface;
}

RenderMacIOSurfaceTextureHostOGL::~RenderMacIOSurfaceTextureHostOGL()
{
  MOZ_COUNT_DTOR_INHERITED(RenderMacIOSurfaceTextureHostOGL, RenderTextureHostOGL);
  DeleteTextureHandle();
}

bool
RenderMacIOSurfaceTextureHostOGL::Lock()
{
  if (!mSurface || !mGL || !mGL->MakeCurrent()) {
    return false;
  }

  if (!mTextureHandle) {
    // xxx: should we need to handle the PlaneCount 3 iosurface?
    MOZ_ASSERT(mSurface->GetPlaneCount() == 0);
    MOZ_ASSERT(gl::GLContextCGL::Cast(mGL.get())->GetCGLContext());

    mGL->fGenTextures(1, &mTextureHandle);
    mGL->fActiveTexture(LOCAL_GL_TEXTURE0);
    gl::ScopedBindTexture texture(mGL, mTextureHandle, LOCAL_GL_TEXTURE_RECTANGLE_ARB);
    mGL->fTexParameteri(LOCAL_GL_TEXTURE_RECTANGLE_ARB, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);
    mGL->fTexParameteri(LOCAL_GL_TEXTURE_RECTANGLE_ARB, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);
    mSurface->CGLTexImageIOSurface2D(gl::GLContextCGL::Cast(mGL.get())->GetCGLContext(), 0);
  }

  return true;
}

void
RenderMacIOSurfaceTextureHostOGL::Unlock()
{

}

void
RenderMacIOSurfaceTextureHostOGL::SetGLContext(gl::GLContext* aContext)
{
  if (mGL.get() != aContext) {
    // release the texture handle in the previous gl context
    DeleteTextureHandle();
    mGL = aContext;
  }
}

void
RenderMacIOSurfaceTextureHostOGL::DeleteTextureHandle()
{
  if (mTextureHandle != 0 && mGL && mGL->MakeCurrent()) {
    mGL->fDeleteTextures(1, &mTextureHandle);
  }
  mTextureHandle = 0;
}

} // namespace wr
} // namespace mozilla
