/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositingRenderTargetOGL.h"
#include "GLContext.h"

using namespace mozilla;
using namespace mozilla::layers;

CompositingRenderTargetOGL::~CompositingRenderTargetOGL()
{
  mGL->fDeleteTextures(1, &mTextureHandle);
  mGL->fDeleteFramebuffers(1, &mFBO);
}

void
CompositingRenderTargetOGL::BindTexture(GLenum aTextureUnit, GLenum aTextureTarget)
{
  MOZ_ASSERT(mInitParams.mStatus == InitParams::INITIALIZED);
  MOZ_ASSERT(mTextureHandle != 0);
  mGL->fActiveTexture(aTextureUnit);
  mGL->fBindTexture(aTextureTarget, mTextureHandle);
}

void
CompositingRenderTargetOGL::BindRenderTarget()
{
  if (mInitParams.mStatus != InitParams::INITIALIZED) {
    InitializeImpl();
  } else {
    MOZ_ASSERT(mInitParams.mStatus == InitParams::INITIALIZED);
    mGL->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, mFBO);
    GLenum result = mGL->fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);
    if (result != LOCAL_GL_FRAMEBUFFER_COMPLETE) {
      nsAutoCString msg;
      msg.AppendPrintf("Framebuffer not complete -- error 0x%x, aFBOTextureTarget 0x%x, aRect.width %d, aRect.height %d",
                        result, mInitParams.mFBOTextureTarget, mInitParams.mSize.width, mInitParams.mSize.height);
      NS_WARNING(msg.get());
    }

    mCompositor->PrepareViewport(mInitParams.mSize, mTransform);
  }
}

#ifdef MOZ_DUMP_PAINTING
already_AddRefed<gfxImageSurface>
CompositingRenderTargetOGL::Dump(Compositor* aCompositor)
{
  MOZ_ASSERT(mInitParams.mStatus == InitParams::INITIALIZED);
  CompositorOGL* compositorOGL = static_cast<CompositorOGL*>(aCompositor);
  return mGL->GetTexImage(mTextureHandle, true, compositorOGL->GetFBOFormat());
}
#endif

void
CompositingRenderTargetOGL::InitializeImpl()
{
  MOZ_ASSERT(mInitParams.mStatus == InitParams::READY);

  mGL->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, mFBO);
  mGL->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER,
                              LOCAL_GL_COLOR_ATTACHMENT0,
                              mInitParams.mFBOTextureTarget,
                              mTextureHandle,
                              0);

  // Making this call to fCheckFramebufferStatus prevents a crash on
  // PowerVR. See bug 695246.
  GLenum result = mGL->fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);
  if (result != LOCAL_GL_FRAMEBUFFER_COMPLETE) {
    nsAutoCString msg;
    msg.AppendPrintf("Framebuffer not complete -- error 0x%x, aFBOTextureTarget 0x%x, mFBO %d, mTextureHandle %d, aRect.width %d, aRect.height %d",
                      result, mInitParams.mFBOTextureTarget, mFBO, mTextureHandle, mInitParams.mSize.width, mInitParams.mSize.height);
    NS_ERROR(msg.get());
  }

  mCompositor->PrepareViewport(mInitParams.mSize, mTransform);
  mGL->fScissor(0, 0, mInitParams.mSize.width, mInitParams.mSize.height);
  if (mInitParams.mInit == INIT_MODE_CLEAR) {
    mGL->fClearColor(0.0, 0.0, 0.0, 0.0);
    mGL->fClear(LOCAL_GL_COLOR_BUFFER_BIT);
  }

  mInitParams.mStatus = InitParams::INITIALIZED;
}
