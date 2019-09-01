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

  enum class GLResourceOwnership : uint8_t {
    // Framebuffer and texture will be deleted when the RenderTarget is
    // destroyed.
    OWNED_BY_RENDER_TARGET,

    // Framebuffer and texture are only used by the RenderTarget, but never
    // deleted.
    EXTERNALLY_OWNED
  };

  struct InitParams {
    GLenum mFBOTextureTarget;
    SurfaceInitMode mInitMode;
  };

 public:
  ~CompositingRenderTargetOGL();

  const char* Name() const override { return "CompositingRenderTargetOGL"; }

  /**
   * Create a render target around the default FBO, for rendering straight to
   * the window.
   */
  static already_AddRefed<CompositingRenderTargetOGL> CreateForWindow(
      CompositorOGL* aCompositor, const gfx::IntSize& aSize) {
    RefPtr<CompositingRenderTargetOGL> result = new CompositingRenderTargetOGL(
        aCompositor, gfx::IntRect(gfx::IntPoint(), aSize), gfx::IntPoint(),
        aSize, GLResourceOwnership::EXTERNALLY_OWNED, 0, 0, Nothing());
    return result.forget();
  }

  static already_AddRefed<CompositingRenderTargetOGL>
  CreateForNewFBOAndTakeOwnership(CompositorOGL* aCompositor, GLuint aTexture,
                                  GLuint aFBO, const gfx::IntRect& aRect,
                                  const gfx::IntPoint& aClipSpaceOrigin,
                                  const gfx::IntSize& aPhySize,
                                  GLenum aFBOTextureTarget,
                                  SurfaceInitMode aInit) {
    RefPtr<CompositingRenderTargetOGL> result = new CompositingRenderTargetOGL(
        aCompositor, aRect, aClipSpaceOrigin, aPhySize,
        GLResourceOwnership::OWNED_BY_RENDER_TARGET, aTexture, aFBO,
        Some(InitParams{aFBOTextureTarget, aInit}));
    return result.forget();
  }

  static already_AddRefed<CompositingRenderTargetOGL>
  CreateForExternallyOwnedFBO(CompositorOGL* aCompositor, GLuint aFBO,
                              const gfx::IntRect& aRect,
                              const gfx::IntPoint& aClipSpaceOrigin) {
    RefPtr<CompositingRenderTargetOGL> result = new CompositingRenderTargetOGL(
        aCompositor, aRect, aClipSpaceOrigin, aRect.Size(),
        GLResourceOwnership::EXTERNALLY_OWNED, 0, aFBO, Nothing());
    return result.forget();
  }

  void BindTexture(GLenum aTextureUnit, GLenum aTextureTarget);

  /**
   * Call when we want to draw into our FBO
   */
  void BindRenderTarget();

  bool IsWindow() { return mFBO == 0; }

  GLuint GetFBO() const;

  GLuint GetTextureHandle() const {
    MOZ_ASSERT(!mNeedInitialization);
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
  gfx::IntSize GetSize() const override { return mSize; }

  // The point that DrawGeometry's aClipRect is relative to. Will be (0, 0) for
  // root render targets and equal to GetOrigin() for non-root render targets.
  gfx::IntPoint GetClipSpaceOrigin() const { return mClipSpaceOrigin; }

  gfx::SurfaceFormat GetFormat() const override {
    // XXX - Should it be implemented ? is the above assert true ?
    MOZ_ASSERT(false, "Not implemented");
    return gfx::SurfaceFormat::UNKNOWN;
  }

  // In render target coordinates, i.e. the same space as GetOrigin().
  // NOT relative to mClipSpaceOrigin!
  void SetClipRect(const Maybe<gfx::IntRect>& aRect) { mClipRect = aRect; }
  const Maybe<gfx::IntRect>& GetClipRect() const { return mClipRect; }

#ifdef MOZ_DUMP_PAINTING
  already_AddRefed<gfx::DataSourceSurface> Dump(
      Compositor* aCompositor) override;
#endif

  const gfx::IntSize& GetInitSize() const { return mSize; }
  const gfx::IntSize& GetPhysicalSize() const { return mPhySize; }

 protected:
  CompositingRenderTargetOGL(CompositorOGL* aCompositor,
                             const gfx::IntRect& aRect,
                             const gfx::IntPoint& aClipSpaceOrigin,
                             const gfx::IntSize& aPhySize,
                             GLResourceOwnership aGLResourceOwnership,
                             GLuint aTexure, GLuint aFBO,
                             const Maybe<InitParams>& aNeedInitialization)
      : CompositingRenderTarget(aRect.TopLeft()),
        mNeedInitialization(aNeedInitialization),
        mSize(aRect.Size()),
        mPhySize(aPhySize),
        mCompositor(aCompositor),
        mGL(aCompositor->gl()),
        mClipSpaceOrigin(aClipSpaceOrigin),
        mGLResourceOwnership(aGLResourceOwnership),
        mTextureHandle(aTexure),
        mFBO(aFBO) {
    MOZ_ASSERT(mGL);
  }

  /**
   * Actually do the initialisation.
   * We do this lazily so that when we first set this render target on the
   * compositor we do not have to re-bind the FBO after unbinding it, or
   * alternatively leave the FBO bound after creation. Note that we leave our
   * FBO bound, and so calling this method is only suitable when about to use
   * this render target.
   */
  void Initialize(GLenum aFBOTextureTarget);

  /**
   * Some() between construction and Initialize, if initialization was
   * requested.
   */
  Maybe<InitParams> mNeedInitialization;

  /*
   * Users of render target would draw in logical size, but it is
   * actually drawn to a surface in physical size.  GL surfaces have
   * a limitation on their size, a smaller surface would be
   * allocated for the render target if the caller requests in a
   * size too big.
   */
  gfx::IntSize mSize;     // Logical size, the expected by callers.
  gfx::IntSize mPhySize;  // Physical size, the real size of the surface.

  /**
   * There is temporary a cycle between the compositor and the render target,
   * each having a strong ref to the other. The compositor's reference to
   * the target is always cleared at the end of a frame.
   */
  RefPtr<CompositorOGL> mCompositor;
  RefPtr<GLContext> mGL;
  Maybe<gfx::IntRect> mClipRect;
  gfx::IntPoint mClipSpaceOrigin;
  GLResourceOwnership mGLResourceOwnership;
  GLuint mTextureHandle;
  GLuint mFBO;
};

}  // namespace layers
}  // namespace mozilla

#endif /* MOZILLA_GFX_SURFACEOGL_H */
