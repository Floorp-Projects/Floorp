/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderEGLImageTextureHost.h"

#include "mozilla/gfx/Logging.h"
#include "GLContext.h"
#include "GLLibraryEGL.h"

namespace mozilla {
namespace wr {

RenderEGLImageTextureHost::RenderEGLImageTextureHost(EGLImage aImage,
                                                     EGLSync aSync,
                                                     gfx::IntSize aSize)
    : mImage(aImage),
      mSync(aSync),
      mSize(aSize),
      mTextureTarget(LOCAL_GL_TEXTURE_2D),
      mTextureHandle(0) {
  MOZ_COUNT_CTOR_INHERITED(RenderEGLImageTextureHost, RenderTextureHostOGL);
}

RenderEGLImageTextureHost::~RenderEGLImageTextureHost() {
  MOZ_COUNT_DTOR_INHERITED(RenderEGLImageTextureHost, RenderTextureHostOGL);
  DeleteTextureHandle();
}

GLuint RenderEGLImageTextureHost::GetGLHandle(uint8_t aChannelIndex) const {
  return mTextureHandle;
}

gfx::IntSize RenderEGLImageTextureHost::GetSize(uint8_t aChannelIndex) const {
  return mSize;
}

wr::WrExternalImage RenderEGLImageTextureHost::Lock(
    uint8_t aChannelIndex, gl::GLContext* aGL, wr::ImageRendering aRendering) {
  MOZ_ASSERT(aChannelIndex == 0);

  if (mGL.get() != aGL) {
    if (mGL) {
      // This should not happen. SharedSurface_EGLImage is created only in
      // parent process.
      MOZ_ASSERT_UNREACHABLE("Unexpected GL context");
      return InvalidToWrExternalImage();
    }
    mGL = aGL;
  }

  if (!mImage || !mGL || !mGL->MakeCurrent()) {
    return InvalidToWrExternalImage();
  }

  EGLint status = LOCAL_EGL_CONDITION_SATISFIED;
  if (mSync) {
    auto* egl = gl::GLLibraryEGL::Get();
    MOZ_ASSERT(egl->IsExtensionSupported(gl::GLLibraryEGL::KHR_fence_sync));
    status = egl->fClientWaitSync(egl->Display(), mSync, 0, LOCAL_EGL_FOREVER);
    // We do not need to delete sync here. It is deleted by
    // SharedSurface_EGLImage.
    mSync = 0;
  }

  if (status != LOCAL_EGL_CONDITION_SATISFIED) {
    MOZ_ASSERT(
        status != 0,
        "ClientWaitSync generated an error. Has mSync already been destroyed?");
    return InvalidToWrExternalImage();
  }

  if (!mTextureHandle) {
    mTextureTarget = mGL->GetPreferredEGLImageTextureTarget();
    MOZ_ASSERT(mTextureTarget == LOCAL_GL_TEXTURE_2D ||
               mTextureTarget == LOCAL_GL_TEXTURE_EXTERNAL);

    mGL->fGenTextures(1, &mTextureHandle);
    // Cache rendering filter.
    mCachedRendering = aRendering;
    ActivateBindAndTexParameteri(mGL, LOCAL_GL_TEXTURE0, mTextureTarget,
                                 mTextureHandle, aRendering);
    mGL->fEGLImageTargetTexture2D(mTextureTarget, mImage);
  } else if (IsFilterUpdateNecessary(aRendering)) {
    // Cache new rendering filter.
    mCachedRendering = aRendering;
    ActivateBindAndTexParameteri(mGL, LOCAL_GL_TEXTURE0, mTextureTarget,
                                 mTextureHandle, aRendering);
  }

  return NativeTextureToWrExternalImage(mTextureHandle, 0, 0, mSize.width,
                                        mSize.height);
}

void RenderEGLImageTextureHost::Unlock() {}

void RenderEGLImageTextureHost::DeleteTextureHandle() {
  if (mTextureHandle) {
    // XXX recycle gl texture, since SharedSurface_EGLImage and
    // RenderEGLImageTextureHost is not recycled.
    mGL->fDeleteTextures(1, &mTextureHandle);
    mTextureHandle = 0;
  }
}

}  // namespace wr
}  // namespace mozilla
