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
  MOZ_ASSERT((mPrepareStatus == STATUS_PREPARED) ||
             (!mSurfTex->IsSingleBuffer() &&
              mPrepareStatus == STATUS_UPDATE_TEX_IMAGE_NEEDED));

  if (!EnsureAttachedToGLContext()) {
    return InvalidToWrExternalImage();
  }

  if (mGL.get() != aGL) {
    // This should not happen. On android, SharedGL is used.
    MOZ_ASSERT_UNREACHABLE("Unexpected GL context");
    return InvalidToWrExternalImage();
  }

  if (!mSurfTex || !mGL || !mGL->MakeCurrent()) {
    return InvalidToWrExternalImage();
  }

  if (IsFilterUpdateNecessary(aRendering)) {
    // Cache new rendering filter.
    mCachedRendering = aRendering;
    ActivateBindAndTexParameteri(mGL, LOCAL_GL_TEXTURE0,
                                 LOCAL_GL_TEXTURE_EXTERNAL_OES,
                                 mSurfTex->GetTexName(), aRendering);
  }

  if (mContinuousUpdate) {
    MOZ_ASSERT(!mSurfTex->IsSingleBuffer());
    mSurfTex->UpdateTexImage();
  } else if (mPrepareStatus == STATUS_UPDATE_TEX_IMAGE_NEEDED) {
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

void RenderAndroidSurfaceTextureHostOGL::DetachedFromGLContext() {
  mAttachedToGLContext = false;
}

bool RenderAndroidSurfaceTextureHostOGL::EnsureAttachedToGLContext() {
  // During handling WebRenderError, GeckoSurfaceTexture should not be attached
  // to GLContext.
  if (RenderThread::Get()->IsHandlingWebRenderError()) {
    return false;
  }

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
  // When SurfaceTexture is single buffer mode, UpdateTexImage needs to be
  // called only once for each publish. If UpdateTexImage is called more
  // than once, it causes hang on puglish side. And UpdateTexImage needs to
  // be called on render thread, since the SurfaceTexture is consumed on render
  // thread.
  MOZ_ASSERT(RenderThread::IsInRenderThread());
  MOZ_ASSERT(mPrepareStatus == STATUS_NONE);

  if (!EnsureAttachedToGLContext()) {
    return;
  }

  if (mContinuousUpdate) {
    return;
  }

  MOZ_ASSERT(mSurfTex);
  mPrepareStatus = STATUS_UPDATE_TEX_IMAGE_NEEDED;

  if (mSurfTex->IsSingleBuffer()) {
    // When SurfaceTexture is single buffer mode, it is OK to call
    // UpdateTexImage() here.
    mSurfTex->UpdateTexImage();
    mPrepareStatus = STATUS_PREPARED;
  }
}

void RenderAndroidSurfaceTextureHostOGL::NotifyNotUsed() {
  MOZ_ASSERT(RenderThread::IsInRenderThread());

  if (!mSurfTex) {
    MOZ_ASSERT(mPrepareStatus == STATUS_NONE);
    return;
  }

  if (mSurfTex->IsSingleBuffer()) {
    MOZ_ASSERT(mPrepareStatus == STATUS_PREPARED);
    MOZ_ASSERT(mAttachedToGLContext);
    // Release SurfaceTexture's buffer to client side.
    mGL->MakeCurrent();
    mSurfTex->ReleaseTexImage();
  } else if (mPrepareStatus == STATUS_UPDATE_TEX_IMAGE_NEEDED) {
    MOZ_ASSERT(mAttachedToGLContext);
    // This could happen when video frame was skipped. UpdateTexImage() neeeds
    // to be called for adjusting SurfaceTexture's buffer status.
    mSurfTex->UpdateTexImage();
  }

  mPrepareStatus = STATUS_NONE;
}

}  // namespace wr
}  // namespace mozilla
