/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderDMABUFTextureHostOGL.h"

#include "GLContextEGL.h"
#include "mozilla/gfx/Logging.h"
#include "ScopedGLHelpers.h"

namespace mozilla::wr {

RenderDMABUFTextureHostOGL::RenderDMABUFTextureHostOGL(DMABufSurface* aSurface)
    : mSurface(aSurface) {
  MOZ_COUNT_CTOR_INHERITED(RenderDMABUFTextureHostOGL, RenderTextureHostOGL);
}

RenderDMABUFTextureHostOGL::~RenderDMABUFTextureHostOGL() {
  MOZ_COUNT_DTOR_INHERITED(RenderDMABUFTextureHostOGL, RenderTextureHostOGL);
  DeleteTextureHandle();
}

GLuint RenderDMABUFTextureHostOGL::GetGLHandle(uint8_t aChannelIndex) const {
  return mSurface->GetTexture(aChannelIndex);
}

gfx::IntSize RenderDMABUFTextureHostOGL::GetSize(uint8_t aChannelIndex) const {
  return gfx::IntSize(mSurface->GetWidth(aChannelIndex),
                      mSurface->GetHeight(aChannelIndex));
}

wr::WrExternalImage RenderDMABUFTextureHostOGL::Lock(
    uint8_t aChannelIndex, gl::GLContext* aGL, wr::ImageRendering aRendering) {
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

  if (!mSurface->GetTexture(aChannelIndex)) {
    if (!mSurface->CreateTexture(mGL, aChannelIndex)) {
      return InvalidToWrExternalImage();
    }
    bindTexture = true;
  }

  if (bindTexture) {
    // Cache new rendering filter.
    mCachedRendering = aRendering;
    ActivateBindAndTexParameteri(mGL, LOCAL_GL_TEXTURE0, LOCAL_GL_TEXTURE_2D,
                                 mSurface->GetTexture(aChannelIndex),
                                 aRendering);
  }

  return NativeTextureToWrExternalImage(mSurface->GetTexture(aChannelIndex), 0,
                                        0, mSurface->GetWidth(aChannelIndex),
                                        mSurface->GetHeight(aChannelIndex));
}

void RenderDMABUFTextureHostOGL::Unlock() {}

void RenderDMABUFTextureHostOGL::DeleteTextureHandle() {
  mSurface->ReleaseTextures();
}

}  // namespace mozilla::wr
