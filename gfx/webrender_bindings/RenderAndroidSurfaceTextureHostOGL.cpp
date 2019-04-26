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
      mContinuousUpdate(aContinuousUpdate),
      mPrepareStatus(STATUS_NONE),
      mAttachedToGLContext(false) {
  MOZ_COUNT_CTOR_INHERITED(RenderAndroidSurfaceTextureHostOGL,
                           RenderTextureHostOGL);

  if (mSurfTex) {
    mSurfTex->IncrementUse();
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

  if (mContinuousUpdate) {
    MOZ_ASSERT(!mSurfTex->IsSingleBuffer());
    mSurfTex->UpdateTexImage();
  } else if (mPrepareStatus == STATUS_PREPARE_NEEDED) {
    MOZ_ASSERT(!mSurfTex->IsSingleBuffer());
    // When SurfaceTexture is not single buffer mode, call UpdateTexImage() once
    // just before rendering. During playing video, one SurfaceTexture is used
    // for all RenderAndroidSurfaceTextureHostOGLs of video.
    mSurfTex->UpdateTexImage();
    mPrepareStatus = STATUS_PREPARED;
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

bool RenderAndroidSurfaceTextureHostOGL::CheckIfAttachedToGLContext() {
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
    return false;
  }

  mAttachedToGLContext = true;
  return true;
}

void RenderAndroidSurfaceTextureHostOGL::PrepareForUse() {
  // When SurfaceTexture is single buffer mode, UpdateTexImage needs to be
  // called only once for each publish. If UpdateTexImage is called more
  // than once, it causes hang on puglish side. And UpdateTexImage needs to
  // be called on render thread, since the SurfaceTexture is consumed on render
  // thread.
  MOZ_ASSERT(RenderThread::IsInRenderThread());
  MOZ_ASSERT(mPrepareStatus == STATUS_NONE);

  if (mContinuousUpdate) {
    return;
  }

  mPrepareStatus = STATUS_MIGHT_BE_USED;

  if (mSurfTex && mSurfTex->IsSingleBuffer()) {
    // When SurfaceTexture is single buffer mode, it is OK to call
    // UpdateTexImage() here.
    EnsureAttachedToGLContext();
    mSurfTex->UpdateTexImage();
    mPrepareStatus = STATUS_PREPARED;
  } else {
    // If SurfaceTexture is already bound to gl texture, it is already used for
    // rendering to WebRender.
    // During video playback, there is a case that rendering is skipped. In this
    // case, NofityForUse() is not called. But we want to call UpdateTexImage()
    // for adjusting SurfaceTexture's buffer status.
    if (CheckIfAttachedToGLContext()) {
      mPrepareStatus = STATUS_PREPARE_NEEDED;
    }
  }
}

void RenderAndroidSurfaceTextureHostOGL::NofityForUse() {
  MOZ_ASSERT(RenderThread::IsInRenderThread());

  if (mPrepareStatus == STATUS_MIGHT_BE_USED) {
    // This happens when SurfaceTexture of video is rendered on WebRender and
    // SurfaceTexture is not bounded to gl context yet.
    // It is necessary to handle a case that SurfaceTexture is not rendered on
    // WebRender, instead rendered to WebGL.
    // It is not a good way. But it is same to Compositor rendering.
    MOZ_ASSERT(!mSurfTex || !mSurfTex->IsSingleBuffer());
    EnsureAttachedToGLContext();
    mPrepareStatus = STATUS_PREPARE_NEEDED;
  }
}

void RenderAndroidSurfaceTextureHostOGL::NotifyNotUsed() {
  MOZ_ASSERT(RenderThread::IsInRenderThread());

  if (mSurfTex && mSurfTex->IsSingleBuffer() &&
      mPrepareStatus == STATUS_PREPARED) {
    EnsureAttachedToGLContext();
    // Release SurfaceTexture's buffer to client side.
    mGL->MakeCurrent();
    mSurfTex->ReleaseTexImage();
  } else if (mSurfTex && mPrepareStatus == STATUS_PREPARE_NEEDED) {
    // This could happen when video frame was skipped. UpdateTexImage() neeeds
    // to be called for adjusting SurfaceTexture's buffer status.
    MOZ_ASSERT(!mSurfTex->IsSingleBuffer());
    EnsureAttachedToGLContext();
    mSurfTex->UpdateTexImage();
  }

  mPrepareStatus = STATUS_NONE;
}

}  // namespace wr
}  // namespace mozilla
