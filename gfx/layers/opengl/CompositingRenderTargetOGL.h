/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_COMPOSITINGRENDERTARGETOGL_H
#define MOZILLA_GFX_COMPOSITINGRENDERTARGETOGL_H

#include "GLContextTypes.h"             // for GLContext
#include "GLDefs.h"                     // for GLenum, LOCAL_GL_FRAMEBUFFER, etc
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/Attributes.h"         // for override
#include "mozilla/RefPtr.h"             // for RefPtr, already_AddRefed
#include "mozilla/gfx/Point.h"          // for IntSize, IntSizeTyped
#include "mozilla/gfx/Types.h"          // for SurfaceFormat, etc
#include "mozilla/layers/Compositor.h"  // for SurfaceInitMode, etc
#include "mozilla/layers/TextureHost.h"    // for CompositingRenderTarget
#include "mozilla/layers/CompositorOGL.h"  // for CompositorOGL
#include "mozilla/mozalloc.h"              // for operator new
#include "nsAString.h"
#include "nsCOMPtr.h"  // for already_AddRefed
#include "nsDebug.h"   // for NS_ERROR, NS_WARNING
#include "nsString.h"  // for nsAutoCString

namespace mozilla {
namespace gl {
class BindableTexture;
}  // namespace gl
namespace gfx {
class DataSourceSurface;
}  // namespace gfx

namespace layers {

class TextureSource;

class CompositingRenderTargetOGL : public CompositingRenderTarget {
  typedef mozilla::gl::GLContext GLContext;

  friend class CompositorOGL;

  // For lazy initialisation of the GL stuff
  struct InitParams {
    InitParams()
        : mStatus(NO_PARAMS), mFBOTextureTarget(0), mInit(INIT_MODE_NONE) {}
    InitParams(const gfx::IntSize& aSize, const gfx::IntSize& aPhySize,
               GLenum aFBOTextureTarget, SurfaceInitMode aInit)
        : mStatus(READY),
          mSize(aSize),
          mPhySize(aPhySize),
          mFBOTextureTarget(aFBOTextureTarget),
          mInit(aInit) {}

    enum { NO_PARAMS, READY, INITIALIZED } mStatus;
    /*
     * Users of render target would draw in logical size, but it is
     * actually drawn to a surface in physical size.  GL surfaces have
     * a limitation on their size, a smaller surface would be
     * allocated for the render target if the caller requests in a
     * size too big.
     */
    gfx::IntSize mSize;     // Logical size, the expected by callers.
    gfx::IntSize mPhySize;  // Physical size, the real size of the surface.
    GLenum mFBOTextureTarget;
    SurfaceInitMode mInit;
  };

 public:
  CompositingRenderTargetOGL(CompositorOGL* aCompositor,
                             const gfx::IntPoint& aOrigin, GLuint aTexure,
                             GLuint aFBO)
      : CompositingRenderTarget(aOrigin),
        mInitParams(),
        mCompositor(aCompositor),
        mGL(aCompositor->gl()),
        mTextureHandle(aTexure),
        mFBO(aFBO) {
    MOZ_ASSERT(mGL);
  }

  ~CompositingRenderTargetOGL();

  const char* Name() const override { return "CompositingRenderTargetOGL"; }

  /**
   * Create a render target around the default FBO, for rendering straight to
   * the window.
   */
  static already_AddRefed<CompositingRenderTargetOGL> RenderTargetForWindow(
      CompositorOGL* aCompositor, const gfx::IntSize& aSize) {
    RefPtr<CompositingRenderTargetOGL> result =
        new CompositingRenderTargetOGL(aCompositor, gfx::IntPoint(), 0, 0);
    result->mInitParams = InitParams(aSize, aSize, 0, INIT_MODE_NONE);
    result->mInitParams.mStatus = InitParams::INITIALIZED;
    return result.forget();
  }

  /**
   * Some initialisation work on the backing FBO and texture.
   * We do this lazily so that when we first set this render target on the
   * compositor we do not have to re-bind the FBO after unbinding it, or
   * alternatively leave the FBO bound after creation.
   */
  void Initialize(const gfx::IntSize& aSize, const gfx::IntSize& aPhySize,
                  GLenum aFBOTextureTarget, SurfaceInitMode aInit) {
    MOZ_ASSERT(mInitParams.mStatus == InitParams::NO_PARAMS,
               "Initialized twice?");
    // postpone initialization until we actually want to use this render target
    mInitParams = InitParams(aSize, aPhySize, aFBOTextureTarget, aInit);
  }

  void BindTexture(GLenum aTextureUnit, GLenum aTextureTarget);

  /**
   * Call when we want to draw into our FBO
   */
  void BindRenderTarget();

  bool IsWindow() { return mFBO == 0; }

  GLuint GetFBO() const;

  GLuint GetTextureHandle() const {
    MOZ_ASSERT(mInitParams.mStatus == InitParams::INITIALIZED);
    return mTextureHandle;
  }

  // TextureSourceOGL
  TextureSourceOGL* AsSourceOGL() override {
    // XXX - Bug 900770
    MOZ_ASSERT(
        false,
        "CompositingRenderTargetOGL should not be used as a TextureSource");
    return nullptr;
  }
  gfx::IntSize GetSize() const override { return mInitParams.mSize; }

  gfx::SurfaceFormat GetFormat() const override {
    // XXX - Should it be implemented ? is the above assert true ?
    MOZ_ASSERT(false, "Not implemented");
    return gfx::SurfaceFormat::UNKNOWN;
  }

  void SetClipRect(const Maybe<gfx::IntRect>& aRect) { mClipRect = aRect; }
  const Maybe<gfx::IntRect>& GetClipRect() const { return mClipRect; }

#ifdef MOZ_DUMP_PAINTING
  already_AddRefed<gfx::DataSourceSurface> Dump(
      Compositor* aCompositor) override;
#endif

  const gfx::IntSize& GetInitSize() const { return mInitParams.mSize; }

 private:
  /**
   * Actually do the initialisation. Note that we leave our FBO bound, and so
   * calling this method is only suitable when about to use this render target.
   */
  void InitializeImpl();

  InitParams mInitParams;
  /**
   * There is temporary a cycle between the compositor and the render target,
   * each having a strong ref to the other. The compositor's reference to
   * the target is always cleared at the end of a frame.
   */
  RefPtr<CompositorOGL> mCompositor;
  RefPtr<GLContext> mGL;
  Maybe<gfx::IntRect> mClipRect;
  GLuint mTextureHandle;
  GLuint mFBO;
};

}  // namespace layers
}  // namespace mozilla

#endif /* MOZILLA_GFX_SURFACEOGL_H */
