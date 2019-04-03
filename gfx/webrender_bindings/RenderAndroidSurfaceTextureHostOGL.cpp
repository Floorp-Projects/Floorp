/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderAndroidSurfaceTextureHostOGL.h"

#include "mozilla/gfx/Logging.h"
#include "mozilla/webrender/RenderThread.h"
#include "GLContext.h"

namespace mozilla {
namespace wr {

RenderAndroidSurfaceTextureHostOGL::RenderAndroidSurfaceTextureHostOGL(
    const java::GeckoSurfaceTexture::GlobalRef& aSurfTex, gfx::IntSize aSize,
    gfx::SurfaceFormat aFormat, bool aContinuousUpdate)
    : mSurfTex(aSurfTex),
      mSize(aSize),
      mIsPrepared(false),
      mAttachedToGLContext(false) {
  MOZ_COUNT_CTOR_INHERITED(RenderAndroidSurfaceTextureHostOGL,
                           RenderTextureHostOGL);

  if (mSurfTex) {
    mSurfTex->IncrementUse();
  }

  if (mSurfTex && !mSurfTex->IsSingleBuffer()) {
    mIsPrepared = true;
  }
}

RenderAndroidSurfaceTextureHostOGL::~RenderAndroidSurfaceTextureHostOGL() {
  MOZ_ASSERT(RenderThread::IsInRenderThread());
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
    // This should not happen. On android, SharedGL is used.
    MOZ_ASSERT_UNREACHABLE("Unexpected GL context");
    return InvalidToWrExternalImage();
  }

  if (!mSurfTex || !mGL || !mGL->MakeCurrent()) {
    return InvalidToWrExternalImage();
  }

  if (!mAttachedToGLContext) {
    // Cache new rendering filter.
    mCachedRendering = aRendering;
    if (!EnsureAttachedToGLContext()) {
      return InvalidToWrExternalImage();
    }
  } else if (IsFilterUpdateNecessary(aRendering)) {
    // Cache new rendering filter.
    mCachedRendering = aRendering;
    ActivateBindAndTexParameteri(mGL, LOCAL_GL_TEXTURE0,
                                 LOCAL_GL_TEXTURE_EXTERNAL_OES,
                                 mSurfTex->GetTexName(), aRendering);
  }

  // Bug 1507078, Call UpdateTexImage() only when mContinuousUpdate is true
  if (mSurfTex && !mSurfTex->IsSingleBuffer()) {
    mSurfTex->UpdateTexImage();
  }

  return NativeTextureToWrExternalImage(mSurfTex->GetTexName(), 0, 0,
                                        mSize.width, mSize.height);
}

void RenderAndroidSurfaceTextureHostOGL::Unlock() {}

void RenderAndroidSurfaceTextureHostOGL::DeleteTextureHandle() {
  NotifyNotUsed();
}

bool RenderAndroidSurfaceTextureHostOGL::EnsureAttachedToGLContext() {
  if (mAttachedToGLContext) {
    return true;
  }

  if (!mGL) {
    mGL = RenderThread::Get()->SharedGL();
  }

  if (!mSurfTex || !mGL || !mGL->MakeCurrent()) {
    return false;
  }

  if (!mSurfTex->IsAttachedToGLContext((int64_t)mGL.get())) {
    GLuint texName;
    mGL->fGenTextures(1, &texName);
    ActivateBindAndTexParameteri(mGL, LOCAL_GL_TEXTURE0,
                                 LOCAL_GL_TEXTURE_EXTERNAL_OES, texName,
                                 mCachedRendering);

    if (NS_FAILED(mSurfTex->AttachToGLContext((int64_t)mGL.get(), texName))) {
      MOZ_ASSERT(0);
      mGL->fDeleteTextures(1, &texName);
      return false;
    }
  }

  mAttachedToGLContext = true;
  return true;
}

void RenderAndroidSurfaceTextureHostOGL::PrepareForUse() {
  // When SurfaceTexture is single buffer mode, UpdateTexImage neeeds to be
  // called only once for each publish. If UpdateTexImage is called more
  // than once, it causes hang on puglish side. And UpdateTexImage needs to
  // be called on render thread, since the SurfaceTexture is consumed on render
  // thread.

  MOZ_ASSERT(RenderThread::IsInRenderThread());

  EnsureAttachedToGLContext();

  if (mSurfTex && mSurfTex->IsSingleBuffer() && !mIsPrepared) {
    mSurfTex->UpdateTexImage();
    mIsPrepared = true;
  }
}

void RenderAndroidSurfaceTextureHostOGL::NotifyNotUsed() {
  MOZ_ASSERT(RenderThread::IsInRenderThread());

  EnsureAttachedToGLContext();

  if (mSurfTex && mSurfTex->IsSingleBuffer() && mIsPrepared) {
    mGL->MakeCurrent();
    mSurfTex->ReleaseTexImage();
    mIsPrepared = false;
  }
}

}  // namespace wr
}  // namespace mozilla
