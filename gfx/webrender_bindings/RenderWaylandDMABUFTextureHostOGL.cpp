/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderWaylandDMABUFTextureHostOGL.h"

#include "GLContextEGL.h"
#include "mozilla/gfx/Logging.h"
#include "ScopedGLHelpers.h"

namespace mozilla::wr {

RenderWaylandDMABUFTextureHostOGL::RenderWaylandDMABUFTextureHostOGL(
    WaylandDMABufSurface* aSurface)
    : mSurface(aSurface), mTextureHandle(0) {
  MOZ_COUNT_CTOR_INHERITED(RenderWaylandDMABUFTextureHostOGL,
                           RenderTextureHostOGL);
}

RenderWaylandDMABUFTextureHostOGL::~RenderWaylandDMABUFTextureHostOGL() {
  MOZ_COUNT_DTOR_INHERITED(RenderWaylandDMABUFTextureHostOGL,
                           RenderTextureHostOGL);
  DeleteTextureHandle();
}

GLuint RenderWaylandDMABUFTextureHostOGL::GetGLHandle(
    uint8_t aChannelIndex) const {
  MOZ_ASSERT(mSurface);
  return mTextureHandle;
}

gfx::IntSize RenderWaylandDMABUFTextureHostOGL::GetSize(
    uint8_t aChannelIndex) const {
  if (!mSurface) {
    return gfx::IntSize();
  }
  return gfx::IntSize(mSurface->GetWidth(), mSurface->GetHeight());
}

wr::WrExternalImage RenderWaylandDMABUFTextureHostOGL::Lock(
    uint8_t aChannelIndex, gl::GLContext* aGL, wr::ImageRendering aRendering) {
  MOZ_ASSERT(aChannelIndex == 0);

  if (mGL.get() != aGL) {
    if (mGL) {
      // This should not happen. EGLImage is created only in
      // parent process.
      MOZ_ASSERT_UNREACHABLE("Unexpected GL context");
      return InvalidToWrExternalImage();
    }
    mGL = aGL;
  }

  if (!mGL || !mGL->MakeCurrent()) {
    return InvalidToWrExternalImage();
  }

  if (!mTextureHandle) {
    if (!mSurface->CreateEGLImage(mGL)) {
      return InvalidToWrExternalImage();
    }

    mGL->fGenTextures(1, &mTextureHandle);
    // Cache rendering filter.
    mCachedRendering = aRendering;
    ActivateBindAndTexParameteri(mGL, LOCAL_GL_TEXTURE0, LOCAL_GL_TEXTURE_2D,
                                 mTextureHandle, aRendering);
    mGL->fEGLImageTargetTexture2D(LOCAL_GL_TEXTURE_2D, mSurface->GetEGLImage());
  } else if (IsFilterUpdateNecessary(aRendering)) {
    // Cache new rendering filter.
    mCachedRendering = aRendering;
    ActivateBindAndTexParameteri(mGL, LOCAL_GL_TEXTURE0, LOCAL_GL_TEXTURE_2D,
                                 mTextureHandle, aRendering);
  }

  return NativeTextureToWrExternalImage(
      mTextureHandle, 0, 0, mSurface->GetWidth(), mSurface->GetHeight());
}

void RenderWaylandDMABUFTextureHostOGL::Unlock() {}

void RenderWaylandDMABUFTextureHostOGL::DeleteTextureHandle() {
  if (mTextureHandle) {
    // XXX recycle gl texture, since SharedSurface_EGLImage and
    // RenderEGLImageTextureHost is not recycled.
    mGL->fDeleteTextures(1, &mTextureHandle);
    mTextureHandle = 0;
  }
}

}  // namespace mozilla::wr
