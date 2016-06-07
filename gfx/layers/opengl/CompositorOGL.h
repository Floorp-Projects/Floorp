/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_COMPOSITOROGL_H
#define MOZILLA_GFX_COMPOSITOROGL_H

#include "ContextStateTracker.h"
#include "gfx2DGlue.h"
#include "GLContextTypes.h"             // for GLContext, etc
#include "GLDefs.h"                     // for GLuint, LOCAL_GL_TEXTURE_2D, etc
#include "OGLShaderProgram.h"           // for ShaderProgramOGL, etc
#include "Units.h"                      // for ScreenPoint
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/Attributes.h"         // for override, final
#include "mozilla/RefPtr.h"             // for already_AddRefed, RefPtr
#include "mozilla/gfx/2D.h"             // for DrawTarget
#include "mozilla/gfx/BaseSize.h"       // for BaseSize
#include "mozilla/gfx/MatrixFwd.h"      // for Matrix4x4
#include "mozilla/gfx/Point.h"          // for IntSize, Point
#include "mozilla/gfx/Rect.h"           // for Rect, IntRect
#include "mozilla/gfx/Types.h"          // for Float, SurfaceFormat, etc
#include "mozilla/layers/Compositor.h"  // for SurfaceInitMode, Compositor, etc
#include "mozilla/layers/CompositorTypes.h"  // for MaskType::MaskType::NumMaskTypes, etc
#include "mozilla/layers/LayersTypes.h"
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsDebug.h"                    // for NS_ASSERTION, NS_WARNING
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR, etc
#include "nsTArray.h"                   // for AutoTArray, nsTArray, etc
#include "nsThreadUtils.h"              // for nsRunnable
#include "nsXULAppAPI.h"                // for XRE_GetProcessType
#include "nscore.h"                     // for NS_IMETHOD
#include "gfxVR.h"

#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 21
#include "nsTHashtable.h"               // for nsTHashtable
#endif

class nsIWidget;

namespace mozilla {

namespace layers {

class CompositingRenderTarget;
class CompositingRenderTargetOGL;
class DataTextureSource;
class GLManagerCompositor;
class TextureSource;
struct Effect;
struct EffectChain;
class GLBlitTextureImageHelper;

#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 21
class ImageHostOverlay;
#endif

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
  explicit PerUnitTexturePoolOGL(gl::GLContext* aGL)
  : mTextureTarget(0) // zero is never a valid texture target
  , mGL(aGL)
  {}

  virtual ~PerUnitTexturePoolOGL()
  {
    DestroyTextures();
  }

  virtual void Clear() override
  {
    DestroyTextures();
  }

  virtual GLuint GetTexture(GLenum aTarget, GLenum aUnit) override;

  virtual void EndFrame() override {}

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
  explicit PerFrameTexturePoolOGL(gl::GLContext* aGL)
  : mTextureTarget(0) // zero is never a valid texture target
  , mGL(aGL)
  {}

  virtual ~PerFrameTexturePoolOGL()
  {
    DestroyTextures();
  }

  virtual void Clear() override
  {
    DestroyTextures();
  }

  virtual GLuint GetTexture(GLenum aTarget, GLenum aUnit) override;

  virtual void EndFrame() override;

protected:
  void DestroyTextures();

  GLenum mTextureTarget;
  RefPtr<gl::GLContext> mGL;
  nsTArray<GLuint> mCreatedTextures;
  nsTArray<GLuint> mUnusedTextures;
};

struct CompositorOGLVRObjects {
  bool mInitialized;

  gfx::VRHMDConfiguration mConfiguration;

  GLuint mDistortionVertices[2];
  GLuint mDistortionIndices[2];
  GLuint mDistortionIndexCount[2];

  GLint mAPosition;
  GLint mATexCoord0;
  GLint mATexCoord1;
  GLint mATexCoord2;
  GLint mAGenericAttribs;

  // The program here implements distortion rendering for VR devices
  // (in this case Oculus only).  We'll need to extend this to support
  // other device types in the future.

  // 0 = TEXTURE_2D, 1 = TEXTURE_RECTANGLE for source
  GLuint mDistortionProgram[2];
  GLint mUTexture[2];
  GLint mUVREyeToSource[2];
  GLint mUVRDestionatinScaleAndOffset[2];
  GLint mUHeight[2];
};

// If you want to make this class not final, first remove calls to virtual
// methods (Destroy) that are made in the destructor.
class CompositorOGL final : public Compositor
{
  typedef mozilla::gl::GLContext GLContext;

  friend class GLManagerCompositor;
  friend class CompositingRenderTargetOGL;

  std::map<ShaderConfigOGL, ShaderProgramOGL*> mPrograms;
public:
  explicit CompositorOGL(CompositorBridgeParent* aParent,
                         widget::CompositorWidgetProxy* aWidget,
                         int aSurfaceWidth = -1, int aSurfaceHeight = -1,
                         bool aUseExternalSurfaceSize = false);

protected:
  virtual ~CompositorOGL();

public:
  virtual CompositorOGL* AsCompositorOGL() override { return this; }

  virtual already_AddRefed<DataTextureSource>
  CreateDataTextureSource(TextureFlags aFlags = TextureFlags::NO_FLAGS) override;

  virtual bool Initialize() override;

  virtual void Destroy() override;

  virtual TextureFactoryIdentifier GetTextureFactoryIdentifier() override
  {
    TextureFactoryIdentifier result =
      TextureFactoryIdentifier(LayersBackend::LAYERS_OPENGL,
                               XRE_GetProcessType(),
                               GetMaxTextureSize(),
                               mFBOTextureTarget == LOCAL_GL_TEXTURE_2D,
                               SupportsPartialTextureUpdate());
    return result;
  }

  virtual already_AddRefed<CompositingRenderTarget>
  CreateRenderTarget(const gfx::IntRect &aRect, SurfaceInitMode aInit) override;

  virtual already_AddRefed<CompositingRenderTarget>
  CreateRenderTargetFromSource(const gfx::IntRect &aRect,
                               const CompositingRenderTarget *aSource,
                               const gfx::IntPoint &aSourcePoint) override;

  virtual void SetRenderTarget(CompositingRenderTarget *aSurface) override;
  virtual CompositingRenderTarget* GetCurrentRenderTarget() const override;

  virtual void DrawQuad(const gfx::Rect& aRect,
                        const gfx::IntRect& aClipRect,
                        const EffectChain &aEffectChain,
                        gfx::Float aOpacity,
                        const gfx::Matrix4x4& aTransform,
                        const gfx::Rect& aVisibleRect) override;

  virtual void EndFrame() override;
  virtual void EndFrameForExternalComposition(const gfx::Matrix& aTransform) override;

  virtual bool SupportsPartialTextureUpdate() override;

  virtual bool CanUseCanvasLayerForSize(const gfx::IntSize &aSize) override
  {
    if (!mGLContext)
      return false;
    int32_t maxSize = GetMaxTextureSize();
    return aSize <= gfx::IntSize(maxSize, maxSize);
  }

  virtual int32_t GetMaxTextureSize() const override;

  /**
   * Set the size of the EGL surface we're rendering to, if we're rendering to
   * an EGL surface.
   */
  virtual void SetDestinationSurfaceSize(const gfx::IntSize& aSize) override;

  virtual void SetScreenRenderOffset(const ScreenPoint& aOffset) override {
    mRenderOffset = aOffset;
  }

  virtual void MakeCurrent(MakeCurrentFlags aFlags = 0) override;

#ifdef MOZ_DUMP_PAINTING
  virtual const char* Name() const override { return "OGL"; }
#endif // MOZ_DUMP_PAINTING

  virtual LayersBackend GetBackendType() const override {
    return LayersBackend::LAYERS_OPENGL;
  }

  virtual void Pause() override;
  virtual bool Resume() override;

  virtual bool HasImageHostOverlays() override
  {
#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 21
    return mImageHostOverlays.Count() > 0;
#else
    return false;
#endif
  }

  virtual void AddImageHostOverlay(ImageHostOverlay* aOverlay) override
  {
#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 21
    mImageHostOverlays.PutEntry(aOverlay);
#endif
  }

  virtual void RemoveImageHostOverlay(ImageHostOverlay* aOverlay) override
  {
#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 21
    mImageHostOverlays.RemoveEntry(aOverlay);
#endif
  }

  GLContext* gl() const { return mGLContext; }
  /**
   * Clear the program state. This must be called
   * before operating on the GLContext directly. */
  void ResetProgram();

  gfx::SurfaceFormat GetFBOFormat() const {
    return gfx::SurfaceFormat::R8G8B8A8;
  }

  GLBlitTextureImageHelper* BlitTextureImageHelper();

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

  void SetProjMatrix(const gfx::Matrix4x4& aProjMatrix) {
    mProjMatrix = aProjMatrix;
  }

  const gfx::IntSize GetDestinationSurfaceSize() const {
    return gfx::IntSize (mSurfaceSize.width, mSurfaceSize.height);
  }

  const ScreenPoint& GetScreenRenderOffset() const {
    return mRenderOffset;
  }

private:
  bool InitializeVR();
  void DestroyVR(GLContext *gl);

  void DrawVRDistortion(const gfx::Rect& aRect,
                        const gfx::IntRect& aClipRect,
                        const EffectChain& aEffectChain,
                        gfx::Float aOpacity,
                        const gfx::Matrix4x4& aTransform);

  void PrepareViewport(CompositingRenderTargetOGL *aRenderTarget);

  /** Widget associated with this compositor */
  LayoutDeviceIntSize mWidgetSize;
  RefPtr<GLContext> mGLContext;
  UniquePtr<GLBlitTextureImageHelper> mBlitTextureImageHelper;
  gfx::Matrix4x4 mProjMatrix;

  /** The size of the surface we are rendering to */
  gfx::IntSize mSurfaceSize;

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
  virtual void ClearRect(const gfx::Rect& aRect) override;

  /* Start a new frame. If aClipRectIn is null and aClipRectOut is non-null,
   * sets *aClipRectOut to the screen dimensions.
   */
  virtual void BeginFrame(const nsIntRegion& aInvalidRegion,
                          const gfx::IntRect *aClipRectIn,
                          const gfx::IntRect& aRenderBounds,
                          const nsIntRegion& aOpaqueRegion,
                          gfx::IntRect *aClipRectOut = nullptr,
                          gfx::IntRect *aRenderBoundsOut = nullptr) override;

  ShaderConfigOGL GetShaderConfigFor(Effect *aEffect,
                                     MaskType aMask = MaskType::MaskNone,
                                     gfx::CompositionOp aOp = gfx::CompositionOp::OP_OVER,
                                     bool aColorMatrix = false,
                                     bool aDEAAEnabled = false) const;
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
  GLuint CreateTexture(const gfx::IntRect& aRect, bool aCopyFromSource, GLuint aSourceFrameBuffer);

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
  gfx::Point3D GetLineCoefficients(const gfx::Point& aPoint1,
                                   const gfx::Point& aPoint2);
  void ActivateProgram(ShaderProgramOGL *aProg);
  void CleanupResources();

  /**
   * Bind the texture behind the current render target as the backdrop for a
   * mix-blend shader.
   */
  void BindBackdrop(ShaderProgramOGL* aProgram, GLuint aBackdrop, GLenum aTexUnit);

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
  GLint FlipY(GLint y) const { return mViewportSize.height - y; }

  RefPtr<CompositorTexturePoolOGL> mTexturePool;

  ContextStateTrackerOGL mContextStateTracker;

  bool mDestroyed;

  /**
   * Size of the OpenGL context's primary framebuffer in pixels. Used by
   * FlipY for the y-flipping calculation and by the DEAA shader.
   */
  gfx::IntSize mViewportSize;

  ShaderProgramOGL *mCurrentProgram;

  CompositorOGLVRObjects mVR;

#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 21
  nsTHashtable<nsPtrHashKey<ImageHostOverlay> > mImageHostOverlays;
#endif

};

} // namespace layers
} // namespace mozilla

#endif /* MOZILLA_GFX_COMPOSITOROGL_H */
