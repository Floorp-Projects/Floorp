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
                                  GLenum aBindTarget, GLuint aBindTexture,
                                  wr::ImageRendering aRendering) {
  aGL->fActiveTexture(aActiveTexture);
  aGL->fBindTexture(aBindTarget, aBindTexture);
  aGL->fTexParameteri(aBindTarget, LOCAL_GL_TEXTURE_MIN_FILTER,
                      aRendering == wr::ImageRendering::Pixelated
                          ? LOCAL_GL_NEAREST
                          : LOCAL_GL_LINEAR);
  aGL->fTexParameteri(aBindTarget, LOCAL_GL_TEXTURE_MAG_FILTER,
                      aRendering == wr::ImageRendering::Pixelated
                          ? LOCAL_GL_NEAREST
                          : LOCAL_GL_LINEAR);
}

RenderTextureHost::RenderTextureHost()
    : mCachedRendering(wr::ImageRendering::Auto) {
  MOZ_COUNT_CTOR(RenderTextureHost);
}

RenderTextureHost::~RenderTextureHost() {
  MOZ_ASSERT(RenderThread::IsInRenderThread());
  MOZ_COUNT_DTOR(RenderTextureHost);
}

bool RenderTextureHost::IsFilterUpdateNecessary(wr::ImageRendering aRendering) {
  return mCachedRendering != aRendering;
}

}  // namespace wr
}  // namespace mozilla
