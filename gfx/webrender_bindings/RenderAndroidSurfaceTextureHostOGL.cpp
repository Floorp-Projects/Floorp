/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderAndroidSurfaceTextureHostOGL.h"

#include "mozilla/gfx/Logging.h"
#include "GLContext.h"

namespace mozilla {
namespace wr {

RenderAndroidSurfaceTextureHostOGL::RenderAndroidSurfaceTextureHostOGL(
    const java::GeckoSurfaceTexture::GlobalRef& aSurfTex, gfx::IntSize aSize,
    gfx::SurfaceFormat aFormat, bool aContinuousUpdate)
    : mSurfTex(aSurfTex), mSize(aSize) {
  MOZ_COUNT_CTOR_INHERITED(RenderAndroidSurfaceTextureHostOGL,
                           RenderTextureHostOGL);

  if (mSurfTex) {
    mSurfTex->IncrementUse();
  }
}

RenderAndroidSurfaceTextureHostOGL::~RenderAndroidSurfaceTextureHostOGL() {
  MOZ_COUNT_DTOR_INHERITED(RenderAndroidSurfaceTextureHostOGL,
                           RenderTextureHostOGL);
  DeleteTextureHandle();
  if (mSurfTex) {
    mSurfTex->DecrementUse();
  }
}

GLuint RenderAndroidSurfaceTextureHostOGL::GetGLHandle(
    uint8_t aChannelIndex) const {
  if (!mSurfTex) {
    return 0;
  }
  return mSurfTex->GetTexName();
}

gfx::IntSize RenderAndroidSurfaceTextureHostOGL::GetSize(
    uint8_t aChannelIndex) const {
  return mSize;
}

wr::WrExternalImage RenderAndroidSurfaceTextureHostOGL::Lock(
    uint8_t aChannelIndex, gl::GLContext* aGL, wr::ImageRendering aRendering) {
  MOZ_ASSERT(aChannelIndex == 0);

  if (mGL.get() != aGL) {
    // release the texture handle in the previous gl context
    DeleteTextureHandle();
    mGL = aGL;
    mGL->MakeCurrent();
  }

  if (!mSurfTex || !mGL || !mGL->MakeCurrent()) {
    return InvalidToWrExternalImage();
  }

  if (!mSurfTex->IsAttachedToGLContext((int64_t)mGL.get())) {
    GLuint texName;
    mGL->fGenTextures(1, &texName);
    // Cache rendering filter.
    mCachedRendering = aRendering;
    ActivateBindAndTexParameteri(mGL, LOCAL_GL_TEXTURE0,
                                 LOCAL_GL_TEXTURE_EXTERNAL_OES, texName,
                                 aRendering);

    if (NS_FAILED(mSurfTex->AttachToGLContext((int64_t)mGL.get(), texName))) {
      MOZ_ASSERT(0);
      mGL->fDeleteTextures(1, &texName);
      return InvalidToWrExternalImage();
      ;
    }
  } else if (IsFilterUpdateNecessary(aRendering)) {
    // Cache new rendering filter.
    mCachedRendering = aRendering;
    ActivateBindAndTexParameteri(mGL, LOCAL_GL_TEXTURE0,
                                 LOCAL_GL_TEXTURE_EXTERNAL_OES,
                                 mSurfTex->GetTexName(), aRendering);
  }

  // XXX Call UpdateTexImage() only when it is necessary.
  // For now, alyways call UpdateTexImage().
  // if (mContinuousUpdate) {
  mSurfTex->UpdateTexImage();
  //}

  return NativeTextureToWrExternalImage(mSurfTex->GetTexName(), 0, 0,
                                        mSize.width, mSize.height);
}

void RenderAndroidSurfaceTextureHostOGL::Unlock() {}

void RenderAndroidSurfaceTextureHostOGL::DeleteTextureHandle() {
  // XXX Do we need to call mSurfTex->DetachFromGLContext() here?
  // But Surface texture is shared among many SurfaceTextureHosts.
}

}  // namespace wr
}  // namespace mozilla
