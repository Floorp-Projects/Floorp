/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_COMPOSITOROGL_H
#define MOZILLA_GFX_COMPOSITOROGL_H

#include "gfx2DGlue.h"
#include "GLContextTypes.h"             // for GLContext, etc
#include "GLDefs.h"                     // for GLuint, LOCAL_GL_TEXTURE_2D, etc
#include "OGLShaderProgram.h"           // for ShaderProgramOGL, etc
#include "Units.h"                      // for ScreenPoint
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/Attributes.h"         // for MOZ_OVERRIDE, MOZ_FINAL
#include "mozilla/RefPtr.h"             // for TemporaryRef, RefPtr
#include "mozilla/gfx/2D.h"             // for DrawTarget
#include "mozilla/gfx/BaseSize.h"       // for BaseSize
#include "mozilla/gfx/Point.h"          // for IntSize, Point
#include "mozilla/gfx/Rect.h"           // for Rect, IntRect
#include "mozilla/gfx/Types.h"          // for Float, SurfaceFormat, etc
#include "mozilla/layers/Compositor.h"  // for SurfaceInitMode, Compositor, etc
#include "mozilla/layers/CompositorTypes.h"  // for MaskType::MaskType::NumMaskTypes, etc
#include "mozilla/layers/LayersTypes.h"
#include "nsAutoPtr.h"                  // for nsRefPtr, nsAutoPtr
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsDebug.h"                    // for NS_ASSERTION, NS_WARNING
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR, etc
#include "nsSize.h"                     // for nsIntSize
#include "nsTArray.h"                   // for nsAutoTArray, nsTArray, etc
#include "nsThreadUtils.h"              // for nsRunnable
#include "nsXULAppAPI.h"                // for XRE_GetProcessType
#include "nscore.h"                     // for NS_IMETHOD
#ifdef MOZ_WIDGET_GONK
#include <ui/GraphicBuffer.h>
#endif

class gfx3DMatrix;
class nsIWidget;

namespace mozilla {
class TimeStamp;

namespace gfx {
class Matrix4x4;
}

namespace layers {

class CompositingRenderTarget;
class CompositingRenderTargetOGL;
class DataTextureSource;
class GLManagerCompositor;
class TextureSource;
struct Effect;
struct EffectChain;

/**
 * Interface for pools of temporary gl textures for the compositor.
 * The textures are fully owned by the pool, so the latter is responsible
 * calling fDeleteTextures accordingly.
 * Users of GetTexture receive a texture that is only valid for the duration
 * of the current frame.
 * This is primarily intended for direct texturing APIs that need to attach
 * shared objects (such as an EGLImage) to a gl texture.
 */
class CompositorTexturePoolOGL
{
protected:
  virtual ~CompositorTexturePoolOGL() {}

public:
  NS_INLINE_DECL_REFCOUNTING(CompositorTexturePoolOGL)

  virtual void Clear() = 0;

  virtual GLuint GetTexture(GLenum aTarget, GLenum aEnum) = 0;

  virtual void EndFrame() = 0;
};

/**
 * Agressively reuses textures. One gl texture per texture unit in total.
 * So far this hasn't shown the best results on b2g.
 */
class PerUnitTexturePoolOGL : public CompositorTexturePoolOGL
{
public:
  PerUnitTexturePoolOGL(gl::GLContext* aGL)
  : mTextureTarget(0) // zero is never a valid texture target
  , mGL(aGL)
  {}

  virtual ~PerUnitTexturePoolOGL()
  {
    DestroyTextures();
  }

  virtual void Clear() MOZ_OVERRIDE
  {
    DestroyTextures();
  }

  virtual GLuint GetTexture(GLenum aTarget, GLenum aUnit) MOZ_OVERRIDE;

  virtual void EndFrame() MOZ_OVERRIDE {}

protected:
  void DestroyTextures();

  GLenum mTextureTarget;
  nsTArray<GLuint> mTextures;
  RefPtr<gl::GLContext> mGL;
};

/**
 * Reuse gl textures from a pool of textures that haven't yet been
 * used during the current frame.
 * All the textures that are not used at the end of a frame are
 * deleted.
 * This strategy seems to work well with gralloc textures because destroying
 * unused textures which are bound to gralloc buffers let drivers know that it
 * can unlock the gralloc buffers.
 */
class PerFrameTexturePoolOGL : public CompositorTexturePoolOGL
{
public:
  PerFrameTexturePoolOGL(gl::GLContext* aGL)
  : mTextureTarget(0) // zero is never a valid texture target
  , mGL(aGL)
  {}

  virtual ~PerFrameTexturePoolOGL()
  {
    DestroyTextures();
  }

  virtual void Clear() MOZ_OVERRIDE
  {
    DestroyTextures();
  }

  virtual GLuint GetTexture(GLenum aTarget, GLenum aUnit) MOZ_OVERRIDE;

  virtual void EndFrame() MOZ_OVERRIDE;

protected:
  void DestroyTextures();

  GLenum mTextureTarget;
  RefPtr<gl::GLContext> mGL;
  nsTArray<GLuint> mCreatedTextures;
  nsTArray<GLuint> mUnusedTextures;
};

// If you want to make this class not MOZ_FINAL, first remove calls to virtual
// methods (Destroy) that are made in the destructor.
class CompositorOGL MOZ_FINAL : public Compositor
{
  typedef mozilla::gl::GLContext GLContext;

  friend class GLManagerCompositor;

  std::map<ShaderConfigOGL, ShaderProgramOGL*> mPrograms;
public:
  CompositorOGL(nsIWidget *aWidget, int aSurfaceWidth = -1, int aSurfaceHeight = -1,
                bool aUseExternalSurfaceSize = false);

  virtual ~CompositorOGL();

  virtual TemporaryRef<DataTextureSource>
  CreateDataTextureSource(TextureFlags aFlags = TextureFlags::NO_FLAGS) MOZ_OVERRIDE;

  virtual bool Initialize() MOZ_OVERRIDE;

  virtual void Destroy() MOZ_OVERRIDE;

  virtual TextureFactoryIdentifier GetTextureFactoryIdentifier() MOZ_OVERRIDE
  {
    TextureFactoryIdentifier result =
      TextureFactoryIdentifier(LayersBackend::LAYERS_OPENGL,
                               XRE_GetProcessType(),
                               GetMaxTextureSize(),
                               mFBOTextureTarget == LOCAL_GL_TEXTURE_2D,
                               SupportsPartialTextureUpdate());
    result.mSupportedBlendModes += gfx::CompositionOp::OP_SCREEN;
    result.mSupportedBlendModes += gfx::CompositionOp::OP_MULTIPLY;
    return result;
  }

  virtual TemporaryRef<CompositingRenderTarget>
  CreateRenderTarget(const gfx::IntRect &aRect, SurfaceInitMode aInit) MOZ_OVERRIDE;

  virtual TemporaryRef<CompositingRenderTarget>
  CreateRenderTargetFromSource(const gfx::IntRect &aRect,
                               const CompositingRenderTarget *aSource,
                               const gfx::IntPoint &aSourcePoint) MOZ_OVERRIDE;

  virtual void SetRenderTarget(CompositingRenderTarget *aSurface) MOZ_OVERRIDE;
  virtual CompositingRenderTarget* GetCurrentRenderTarget() const MOZ_OVERRIDE;

  virtual void DrawQuad(const gfx::Rect& aRect,
                        const gfx::Rect& aClipRect,
                        const EffectChain &aEffectChain,
                        gfx::Float aOpacity,
                        const gfx::Matrix4x4 &aTransform) MOZ_OVERRIDE;

  virtual void EndFrame() MOZ_OVERRIDE;
  virtual void SetFBAcquireFence(Layer* aLayer) MOZ_OVERRIDE;
  virtual void EndFrameForExternalComposition(const gfx::Matrix& aTransform) MOZ_OVERRIDE;
  virtual void AbortFrame() MOZ_OVERRIDE;

  virtual bool SupportsPartialTextureUpdate() MOZ_OVERRIDE;

  virtual bool CanUseCanvasLayerForSize(const gfx::IntSize &aSize) MOZ_OVERRIDE
  {
    if (!mGLContext)
      return false;
    int32_t maxSize = GetMaxTextureSize();
    return aSize <= gfx::IntSize(maxSize, maxSize);
  }

  virtual int32_t GetMaxTextureSize() const MOZ_OVERRIDE;

  /**
   * Set the size of the EGL surface we're rendering to, if we're rendering to
   * an EGL surface.
   */
  virtual void SetDestinationSurfaceSize(const gfx::IntSize& aSize) MOZ_OVERRIDE;

  virtual void SetScreenRenderOffset(const ScreenPoint& aOffset) MOZ_OVERRIDE {
    mRenderOffset = aOffset;
  }

  virtual void MakeCurrent(MakeCurrentFlags aFlags = 0) MOZ_OVERRIDE;

  virtual void PrepareViewport(const gfx::IntSize& aSize,
                               const gfx::Matrix& aWorldTransform) MOZ_OVERRIDE;


#ifdef MOZ_DUMP_PAINTING
  virtual const char* Name() const MOZ_OVERRIDE { return "OGL"; }
#endif // MOZ_DUMP_PAINTING

  virtual LayersBackend GetBackendType() const MOZ_OVERRIDE {
    return LayersBackend::LAYERS_OPENGL;
  }

  virtual void Pause() MOZ_OVERRIDE;
  virtual bool Resume() MOZ_OVERRIDE;

  virtual nsIWidget* GetWidget() const MOZ_OVERRIDE { return mWidget; }

  GLContext* gl() const { return mGLContext; }
  gfx::SurfaceFormat GetFBOFormat() const {
    return gfx::SurfaceFormat::R8G8B8A8;
  }

  /**
   * The compositor provides with temporary textures for use with direct
   * textruing like gralloc texture.
   * Doing so lets us use gralloc the way it has been designed to be used
   * (see https://wiki.mozilla.org/Platform/GFX/Gralloc)
   */
  GLuint GetTemporaryTexture(GLenum aTarget, GLenum aUnit);

  const gfx::Matrix4x4& GetProjMatrix() const {
    return mProjMatrix;
  }
private:
  virtual gfx::IntSize GetWidgetSize() const MOZ_OVERRIDE
  {
    return gfx::ToIntSize(mWidgetSize);
  }

  /** Widget associated with this compositor */
  nsIWidget *mWidget;
  nsIntSize mWidgetSize;
  nsRefPtr<GLContext> mGLContext;
  gfx::Matrix4x4 mProjMatrix;

  /** The size of the surface we are rendering to */
  nsIntSize mSurfaceSize;

  ScreenPoint mRenderOffset;

  already_AddRefed<mozilla::gl::GLContext> CreateContext();

  /** Texture target to use for FBOs */
  GLenum mFBOTextureTarget;

  /** Currently bound render target */
  RefPtr<CompositingRenderTargetOGL> mCurrentRenderTarget;
#ifdef DEBUG
  CompositingRenderTargetOGL* mWindowRenderTarget;
#endif

  /**
   * VBO that has some basics in it for a textured quad, including vertex
   * coords and texcoords.
   */
  GLuint mQuadVBO;

  bool mHasBGRA;

  /**
   * When rendering to some EGL surfaces (e.g. on Android), we rely on being told
   * about size changes (via SetSurfaceSize) rather than pulling this information
   * from the widget.
   */
  bool mUseExternalSurfaceSize;

  /**
   * Have we had DrawQuad calls since the last frame was rendered?
   */
  bool mFrameInProgress;

  /*
   * Clear aRect on current render target.
   */
  virtual void ClearRect(const gfx::Rect& aRect) MOZ_OVERRIDE;

  /* Start a new frame. If aClipRectIn is null and aClipRectOut is non-null,
   * sets *aClipRectOut to the screen dimensions.
   */
  virtual void BeginFrame(const nsIntRegion& aInvalidRegion,
                          const gfx::Rect *aClipRectIn,
                          const gfx::Matrix& aTransform,
                          const gfx::Rect& aRenderBounds,
                          gfx::Rect *aClipRectOut = nullptr,
                          gfx::Rect *aRenderBoundsOut = nullptr) MOZ_OVERRIDE;

  ShaderConfigOGL GetShaderConfigFor(Effect *aEffect,
                                     MaskType aMask = MaskType::MaskNone,
                                     gfx::CompositionOp aOp = gfx::CompositionOp::OP_OVER) const;
  ShaderProgramOGL* GetShaderProgramFor(const ShaderConfigOGL &aConfig);

  /**
   * Create a FBO backed by a texture.
   * Note that the texture target type will be
   * of the type returned by FBOTextureTarget; different
   * shaders are required to sample from the different
   * texture types.
   */
  void CreateFBOWithTexture(const gfx::IntRect& aRect, bool aCopyFromSource,
                            GLuint aSourceFrameBuffer,
                            GLuint *aFBO, GLuint *aTexture);

  void BindAndDrawQuads(ShaderProgramOGL *aProg,
                        int aQuads,
                        const gfx::Rect* aLayerRect,
                        const gfx::Rect* aTextureRect);
  void BindAndDrawQuad(ShaderProgramOGL *aProg,
                       const gfx::Rect& aLayerRect,
                       const gfx::Rect& aTextureRect = gfx::Rect(0.0f, 0.0f, 1.0f, 1.0f)) {
    gfx::Rect layerRects[4];
    gfx::Rect textureRects[4];
    layerRects[0] = aLayerRect;
    textureRects[0] = aTextureRect;
    BindAndDrawQuads(aProg, 1, layerRects, textureRects);
  }
  void BindAndDrawQuadWithTextureRect(ShaderProgramOGL *aProg,
                                      const gfx::Rect& aRect,
                                      const gfx::Rect& aTexCoordRect,
                                      TextureSource *aTexture);

  void CleanupResources();

  /**
   * Copies the content of our backbuffer to the set transaction target.
   * Does not restore the target FBO, so only call from EndFrame.
   */
  void CopyToTarget(gfx::DrawTarget* aTarget, const nsIntPoint& aTopLeft, const gfx::Matrix& aWorldMatrix);

  /**
   * Implements the flipping of the y-axis to convert from layers/compositor
   * coordinates to OpenGL coordinates.
   *
   * Indeed, the only coordinate system that OpenGL knows has the y-axis
   * pointing upwards, but the layers/compositor coordinate system has the
   * y-axis pointing downwards, for good reason as Web pages are typically
   * scrolled downwards. So, some flipping has to take place; FlippedY does it.
   */
  GLint FlipY(GLint y) const { return mHeight - y; }

  RefPtr<CompositorTexturePoolOGL> mTexturePool;

  bool mDestroyed;

  /**
   * Height of the OpenGL context's primary framebuffer in pixels. Used by
   * FlipY for the y-flipping calculation.
   */
  GLint mHeight;
};

}
}

#endif /* MOZILLA_GFX_COMPOSITOROGL_H */
