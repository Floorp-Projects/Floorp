/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderAndroidHardwareBufferTextureHost.h"

#include "mozilla/layers/AndroidHardwareBuffer.h"
#include "GLContextEGL.h"
#include "GLLibraryEGL.h"

namespace mozilla {
namespace wr {

RenderAndroidHardwareBufferTextureHost::RenderAndroidHardwareBufferTextureHost(
    layers::AndroidHardwareBuffer* aAndroidHardwareBuffer)
    : mAndroidHardwareBuffer(aAndroidHardwareBuffer),
      mEGLImage(EGL_NO_IMAGE),
      mTextureHandle(0) {
  MOZ_ASSERT(mAndroidHardwareBuffer);
  MOZ_COUNT_CTOR_INHERITED(RenderAndroidHardwareBufferTextureHost,
                           RenderTextureHostOGL);
}

RenderAndroidHardwareBufferTextureHost::
    ~RenderAndroidHardwareBufferTextureHost() {
  MOZ_COUNT_DTOR_INHERITED(RenderAndroidHardwareBufferTextureHost,
                           RenderTextureHostOGL);
  DeleteTextureHandle();
  DestroyEGLImage();
}

GLuint RenderAndroidHardwareBufferTextureHost::GetGLHandle(
    uint8_t aChannelIndex) const {
  return mTextureHandle;
}

gfx::IntSize RenderAndroidHardwareBufferTextureHost::GetSize(
    uint8_t aChannelIndex) const {
  return GetSize();
}

gfx::IntSize RenderAndroidHardwareBufferTextureHost::GetSize() const {
  if (mAndroidHardwareBuffer) {
    return mAndroidHardwareBuffer->mSize;
  }
  return gfx::IntSize();
}

bool RenderAndroidHardwareBufferTextureHost::EnsureLockable(
    wr::ImageRendering aRendering) {
  if (!mAndroidHardwareBuffer) {
    return false;
  }

  if (mTextureHandle) {
    // Update filter if filter was changed.
    if (IsFilterUpdateNecessary(aRendering)) {
      ActivateBindAndTexParameteri(mGL, LOCAL_GL_TEXTURE0,
                                   LOCAL_GL_TEXTURE_EXTERNAL_OES,
                                   mTextureHandle, aRendering);
      // Cache new rendering filter.
      mCachedRendering = aRendering;
    }
    return true;
  }

  if (!mEGLImage) {
    // XXX add crop handling for video
    // Should only happen the first time.
    const auto& gle = gl::GLContextEGL::Cast(mGL);
    const auto& egl = gle->mEgl;

    const EGLint attrs[] = {
        LOCAL_EGL_IMAGE_PRESERVED,
        LOCAL_EGL_TRUE,
        LOCAL_EGL_NONE,
        LOCAL_EGL_NONE,
    };

    EGLClientBuffer clientBuffer = egl->fGetNativeClientBufferANDROID(
        mAndroidHardwareBuffer->GetNativeBuffer());
    mEGLImage =
        egl->fCreateImage(egl->Display(), EGL_NO_CONTEXT,
                          LOCAL_EGL_NATIVE_BUFFER_ANDROID, clientBuffer, attrs);
  }
  MOZ_ASSERT(mEGLImage);

  mGL->fGenTextures(1, &mTextureHandle);
  mGL->fBindTexture(LOCAL_GL_TEXTURE_EXTERNAL, mTextureHandle);
  mGL->fTexParameteri(LOCAL_GL_TEXTURE_EXTERNAL, LOCAL_GL_TEXTURE_WRAP_T,
                      LOCAL_GL_CLAMP_TO_EDGE);
  mGL->fTexParameteri(LOCAL_GL_TEXTURE_EXTERNAL, LOCAL_GL_TEXTURE_WRAP_S,
                      LOCAL_GL_CLAMP_TO_EDGE);
  mGL->fEGLImageTargetTexture2D(LOCAL_GL_TEXTURE_EXTERNAL, mEGLImage);

  ActivateBindAndTexParameteri(mGL, LOCAL_GL_TEXTURE0,
                               LOCAL_GL_TEXTURE_EXTERNAL_OES, mTextureHandle,
                               aRendering);
  // Cache rendering filter.
  mCachedRendering = aRendering;
  return true;
}

wr::WrExternalImage RenderAndroidHardwareBufferTextureHost::Lock(
    uint8_t aChannelIndex, gl::GLContext* aGL, wr::ImageRendering aRendering) {
  MOZ_ASSERT(aChannelIndex == 0);

  if (mGL.get() != aGL) {
    if (mGL) {
      // This should not happen.
      MOZ_ASSERT_UNREACHABLE("Unexpected GL context");
      return InvalidToWrExternalImage();
    }
    mGL = aGL;
  }

  if (!mGL || !mGL->MakeCurrent()) {
    return InvalidToWrExternalImage();
  }

  if (!EnsureLockable(aRendering)) {
    return InvalidToWrExternalImage();
  }

  // XXX Add android Fence handling

  return NativeTextureToWrExternalImage(mTextureHandle, 0, 0, GetSize().width,
                                        GetSize().height);
}

void RenderAndroidHardwareBufferTextureHost::Unlock() {}

void RenderAndroidHardwareBufferTextureHost::DeleteTextureHandle() {
  if (!mTextureHandle) {
    return;
  }
  MOZ_ASSERT(mGL);
  mGL->fDeleteTextures(1, &mTextureHandle);
  mTextureHandle = 0;
}

void RenderAndroidHardwareBufferTextureHost::DestroyEGLImage() {
  if (!mEGLImage) {
    return;
  }
  MOZ_ASSERT(mGL);
  const auto& gle = gl::GLContextEGL::Cast(mGL);
  const auto& egl = gle->mEgl;
  egl->fDestroyImage(egl->Display(), mEGLImage);
  mEGLImage = EGL_NO_IMAGE;
}

}  // namespace wr
}  // namespace mozilla
