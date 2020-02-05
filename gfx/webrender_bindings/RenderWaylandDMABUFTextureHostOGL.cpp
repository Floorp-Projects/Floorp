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
    : mSurface(aSurface) {
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
  return mSurface->GetGLTexture();
}

gfx::IntSize RenderWaylandDMABUFTextureHostOGL::GetSize(
    uint8_t aChannelIndex) const {
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

  bool bindTexture = IsFilterUpdateNecessary(aRendering);

  if (!mSurface->GetGLTexture()) {
    if (!mSurface->CreateEGLImage(mGL)) {
      return InvalidToWrExternalImage();
    }
    bindTexture = true;
  }

  if (bindTexture) {
    // Cache new rendering filter.
    mCachedRendering = aRendering;
    ActivateBindAndTexParameteri(mGL, LOCAL_GL_TEXTURE0, LOCAL_GL_TEXTURE_2D,
                                 mSurface->GetGLTexture(), aRendering);
  }

  return NativeTextureToWrExternalImage(mSurface->GetGLTexture(), 0, 0,
                                        mSurface->GetWidth(),
                                        mSurface->GetHeight());
}

void RenderWaylandDMABUFTextureHostOGL::Unlock() {}

void RenderWaylandDMABUFTextureHostOGL::DeleteTextureHandle() {
  mSurface->ReleaseEGLImage();
}

}  // namespace mozilla::wr
