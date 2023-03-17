/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderTextureHost.h"

#include "GLContext.h"
#include "RenderThread.h"

namespace mozilla {
namespace wr {

void ActivateBindAndTexParameteri(gl::GLContext* aGL, GLenum aActiveTexture,
                                  GLenum aBindTarget, GLuint aBindTexture) {
  aGL->fActiveTexture(aActiveTexture);
  aGL->fBindTexture(aBindTarget, aBindTexture);
  // Initialize the mip filters to linear by default.
  aGL->fTexParameteri(aBindTarget, LOCAL_GL_TEXTURE_MIN_FILTER,
                      LOCAL_GL_LINEAR);
  aGL->fTexParameteri(aBindTarget, LOCAL_GL_TEXTURE_MAG_FILTER,
                      LOCAL_GL_LINEAR);
}

RenderTextureHost::RenderTextureHost() : mIsFromDRMSource(false) {
  MOZ_COUNT_CTOR(RenderTextureHost);
}

RenderTextureHost::~RenderTextureHost() {
  MOZ_ASSERT(RenderThread::IsInRenderThread());
  MOZ_COUNT_DTOR(RenderTextureHost);
}

wr::WrExternalImage RenderTextureHost::Lock(uint8_t aChannelIndex,
                                            gl::GLContext* aGL) {
  return InvalidToWrExternalImage();
}

wr::WrExternalImage RenderTextureHost::LockSWGL(uint8_t aChannelIndex,
                                                void* aContext,
                                                RenderCompositor* aCompositor) {
  return InvalidToWrExternalImage();
}

std::pair<gfx::Point, gfx::Point> RenderTextureHost::GetUvCoords(
    gfx::IntSize aTextureSize) const {
  return std::make_pair(gfx::Point(0.0, 0.0),
                        gfx::Point(static_cast<float>(aTextureSize.width),
                                   static_cast<float>(aTextureSize.height)));
}

void RenderTextureHost::Destroy() {
  MOZ_ASSERT_UNREACHABLE("unexpected to be called");
}

}  // namespace wr
}  // namespace mozilla
