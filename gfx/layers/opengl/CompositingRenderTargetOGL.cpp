/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositingRenderTargetOGL.h"
#include "GLContext.h"
#include "GLReadTexImageHelper.h"
#include "mozilla/gfx/2D.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;
using namespace mozilla::gl;

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
      // The main framebuffer (0) of non-offscreen contexts
      // might be backed by a EGLSurface that needs to be renewed.
      if (mFBO == 0 && !mGL->IsOffscreen()) {
        mGL->RenewSurface();
        result = mGL->fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);
      }
      if (result != LOCAL_GL_FRAMEBUFFER_COMPLETE) {
        nsAutoCString msg;
        msg.AppendPrintf("Framebuffer not complete -- CheckFramebufferStatus returned 0x%x, "
                         "GLContext=%p, IsOffscreen()=%d, mFBO=%d, aFBOTextureTarget=0x%x, "
                         "aRect.width=%d, aRect.height=%d",
                         result, mGL, mGL->IsOffscreen(), mFBO, mInitParams.mFBOTextureTarget,
                         mInitParams.mSize.width, mInitParams.mSize.height);
        NS_WARNING(msg.get());
      }
    }

    mCompositor->PrepareViewport(mInitParams.mSize, mTransform);
  }
}

#ifdef MOZ_DUMP_PAINTING
TemporaryRef<DataSourceSurface>
CompositingRenderTargetOGL::Dump(Compositor* aCompositor)
{
  MOZ_ASSERT(mInitParams.mStatus == InitParams::INITIALIZED);
  CompositorOGL* compositorOGL = static_cast<CompositorOGL*>(aCompositor);
  return ReadBackSurface(mGL, mTextureHandle, true, compositorOGL->GetFBOFormat());
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

}
}
