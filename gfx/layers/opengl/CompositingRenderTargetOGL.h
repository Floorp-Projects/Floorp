/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_COMPOSITINGRENDERTARGETOGL_H
#define MOZILLA_GFX_COMPOSITINGRENDERTARGETOGL_H

#include "mozilla/layers/CompositorOGL.h"
#include "mozilla/gfx/Rect.h"
#include "gfxASurface.h"

#ifdef MOZ_DUMP_PAINTING
#include "mozilla/layers/CompositorOGL.h"
#endif

namespace mozilla {
namespace gl {
  class TextureImage;
  class BindableTexture;
}
namespace layers {

class CompositingRenderTargetOGL : public CompositingRenderTarget
{
  typedef gfxASurface::gfxContentType ContentType;
  typedef mozilla::gl::GLContext GLContext;

  // For lazy initialisation of the GL stuff
  struct InitParams
  {
    InitParams() : mStatus(NO_PARAMS) {}
    InitParams(const gfx::IntSize& aSize,
               GLenum aFBOTextureTarget,
               SurfaceInitMode aInit)
      : mStatus(READY)
      , mSize(aSize)
      , mFBOTextureTarget(aFBOTextureTarget)
      , mInit(aInit)
    {}

    enum {
      NO_PARAMS,
      READY,
      INITIALIZED
    } mStatus;
    gfx::IntSize mSize;
    GLenum mFBOTextureTarget;
    SurfaceInitMode mInit;
  };

public:
  CompositingRenderTargetOGL(CompositorOGL* aCompositor, GLuint aTexure, GLuint aFBO)
    : mInitParams()
    , mTransform()
    , mCompositor(aCompositor)
    , mGL(aCompositor->gl())
    , mTextureHandle(aTexure)
    , mFBO(aFBO)
  {}

  ~CompositingRenderTargetOGL()
  {
    mGL->fDeleteTextures(1, &mTextureHandle);
    mGL->fDeleteFramebuffers(1, &mFBO);
  }

  /**
   * Create a render target around the default FBO, for rendering straight to
   * the window.
   */
  static TemporaryRef<CompositingRenderTargetOGL>
  RenderTargetForWindow(CompositorOGL* aCompositor,
                        const gfx::IntSize& aSize,
                        const gfxMatrix& aTransform)
  {
    RefPtr<CompositingRenderTargetOGL> result
      = new CompositingRenderTargetOGL(aCompositor, 0, 0);
    result->mTransform = aTransform;
    result->mInitParams = InitParams(aSize, 0, INIT_MODE_NONE);
    result->mInitParams.mStatus = InitParams::INITIALIZED;
    return result.forget();
  }

  /**
   * Some initialisation work on the backing FBO and texture.
   * We do this lazily so that when we first set this render target on the
   * compositor we do not have to re-bind the FBO after unbinding it, or
   * alternatively leave the FBO bound after creation.
   */
  void Initialize(const gfx::IntSize& aSize,
                  GLenum aFBOTextureTarget,
                  SurfaceInitMode aInit)
  {
    MOZ_ASSERT(mInitParams.mStatus == InitParams::NO_PARAMS, "Initialized twice?");
    // postpone initialization until we actually want to use this render target
    mInitParams = InitParams(aSize, aFBOTextureTarget, aInit);
  }

  void BindTexture(GLenum aTextureUnit, GLenum aTextureTarget)
  {
    MOZ_ASSERT(mInitParams.mStatus == InitParams::INITIALIZED);
    MOZ_ASSERT(mTextureHandle != 0);
    mGL->fActiveTexture(aTextureUnit);
    mGL->fBindTexture(aTextureTarget, mTextureHandle);
  }

  /**
   * Call when we want to draw into our FBO
   */
  void BindRenderTarget()
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

  GLuint GetFBO() const
  {
    MOZ_ASSERT(mInitParams.mStatus == InitParams::INITIALIZED);
    return mFBO;
  }

  GLuint GetTextureHandle() const
  {
    MOZ_ASSERT(mInitParams.mStatus == InitParams::INITIALIZED);
    return mTextureHandle;
  }

  // TextureSourceOGL
  TextureSourceOGL* AsSourceOGL() MOZ_OVERRIDE
  {
    MOZ_ASSERT(false, "CompositingRenderTargetOGL should not be used as a TextureSource");
    return nullptr;
  }
  gfx::IntSize GetSize() const MOZ_OVERRIDE
  {
    MOZ_ASSERT(false, "CompositingRenderTargetOGL should not be used as a TextureSource");
    return gfx::IntSize(0, 0);
  }

  const gfxMatrix& GetTransform() {
    return mTransform;
  }

#ifdef MOZ_DUMP_PAINTING
  virtual already_AddRefed<gfxImageSurface> Dump(Compositor* aCompositor)
  {
    MOZ_ASSERT(mInitParams.mStatus == InitParams::INITIALIZED);
    CompositorOGL* compositorOGL = static_cast<CompositorOGL*>(aCompositor);
    return mGL->GetTexImage(mTextureHandle, true, compositorOGL->GetFBOFormat());
  }
#endif

private:
  /**
   * Actually do the initialisation. Note that we leave our FBO bound, and so
   * calling this method is only suitable when about to use this render target.
   */
  void InitializeImpl()
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
      msg.AppendPrintf("Framebuffer not complete -- error 0x%x, aFBOTextureTarget 0x%x, aRect.width %d, aRect.height %d",
                       result, mInitParams.mFBOTextureTarget, mInitParams.mSize.width, mInitParams.mSize.height);
      NS_RUNTIMEABORT(msg.get());
    }

    mCompositor->PrepareViewport(mInitParams.mSize, mTransform);
    mGL->fScissor(0, 0, mInitParams.mSize.width, mInitParams.mSize.height);
    if (mInitParams.mInit == INIT_MODE_CLEAR) {
      mGL->fClearColor(0.0, 0.0, 0.0, 0.0);
      mGL->fClear(LOCAL_GL_COLOR_BUFFER_BIT);
    }

    mInitParams.mStatus = InitParams::INITIALIZED;
  }

  InitParams mInitParams;
  gfxMatrix mTransform;
  CompositorOGL* mCompositor;
  GLContext* mGL;
  GLuint mTextureHandle;
  GLuint mFBO;
};

}
}

#endif /* MOZILLA_GFX_SURFACEOGL_H */
