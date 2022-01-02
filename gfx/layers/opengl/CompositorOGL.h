/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_COMPOSITOROGL_H
#define MOZILLA_GFX_COMPOSITOROGL_H

#include <map>
#include <unordered_map>
#include <unordered_set>

#include "gfx2DGlue.h"
#include "GLContextTypes.h"         // for GLContext, etc
#include "GLDefs.h"                 // for GLuint, LOCAL_GL_TEXTURE_2D, etc
#include "OGLShaderConfig.h"        // for ShaderConfigOGL
#include "Units.h"                  // for ScreenPoint
#include "mozilla/Assertions.h"     // for MOZ_ASSERT, etc
#include "mozilla/Attributes.h"     // for override, final
#include "mozilla/RefPtr.h"         // for already_AddRefed, RefPtr
#include "mozilla/gfx/2D.h"         // for DrawTarget
#include "mozilla/gfx/BaseSize.h"   // for BaseSize
#include "mozilla/gfx/MatrixFwd.h"  // for Matrix4x4
#include "mozilla/gfx/Point.h"      // for IntSize, Point
#include "mozilla/gfx/Rect.h"       // for Rect, IntRect
#include "mozilla/gfx/Triangle.h"   // for Triangle
#include "mozilla/gfx/Types.h"      // for Float, SurfaceFormat, etc
#include "mozilla/ipc/FileDescriptor.h"
#include "mozilla/layers/Compositor.h"  // for SurfaceInitMode, Compositor, etc
#include "mozilla/layers/CompositorTypes.h"  // for MaskType::MaskType::NumMaskTypes, etc
#include "mozilla/layers/LayersTypes.h"
#include "nsCOMPtr.h"         // for already_AddRefed
#include "nsDebug.h"          // for NS_ASSERTION, NS_WARNING
#include "nsISupportsImpl.h"  // for MOZ_COUNT_CTOR, etc
#include "nsTArray.h"         // for AutoTArray, nsTArray, etc
#include "nsThreadUtils.h"    // for nsRunnable
#include "nsXULAppAPI.h"      // for XRE_GetProcessType
#include "nscore.h"           // for NS_IMETHOD

class nsIWidget;

namespace mozilla {

namespace layers {

class CompositingRenderTarget;
class CompositingRenderTargetOGL;
class DataTextureSource;
class ShaderProgramOGL;
class ShaderProgramOGLsHolder;
class TextureSource;
class TextureSourceOGL;
class BufferTextureHost;
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
class CompositorTexturePoolOGL {
 protected:
  virtual ~CompositorTexturePoolOGL() = default;

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
class PerUnitTexturePoolOGL : public CompositorTexturePoolOGL {
 public:
  explicit PerUnitTexturePoolOGL(gl::GLContext* aGL);
  virtual ~PerUnitTexturePoolOGL();

  void Clear() override { DestroyTextures(); }

  GLuint GetTexture(GLenum aTarget, GLenum aUnit) override;

  void EndFrame() override {}

 protected:
  void DestroyTextures();

  GLenum mTextureTarget;
  nsTArray<GLuint> mTextures;
  RefPtr<gl::GLContext> mGL;
};

// If you want to make this class not final, first remove calls to virtual
// methods (Destroy) that are made in the destructor.
class CompositorOGL final : public Compositor {
  typedef mozilla::gl::GLContext GLContext;

  friend class CompositingRenderTargetOGL;

  RefPtr<ShaderProgramOGLsHolder> mProgramsHolder;

 public:
  explicit CompositorOGL(widget::CompositorWidget* aWidget,
                         int aSurfaceWidth = -1, int aSurfaceHeight = -1,
                         bool aUseExternalSurfaceSize = false);

 protected:
  virtual ~CompositorOGL();

 public:
  CompositorOGL* AsCompositorOGL() override { return this; }

  already_AddRefed<DataTextureSource> CreateDataTextureSource(
      TextureFlags aFlags = TextureFlags::NO_FLAGS) override;

  bool Initialize(GLContext* aGLContext,
                  RefPtr<ShaderProgramOGLsHolder> aProgramsHolder,
                  nsCString* const out_failureReason);

  bool Initialize(nsCString* const out_failureReason) override;

  void Destroy() override;

  // Returns a render target for the native layer.
  // aInvalidRegion is in window coordinates, i.e. in the same space as
  // aNativeLayer->GetPosition().
  already_AddRefed<CompositingRenderTargetOGL> RenderTargetForNativeLayer(
      NativeLayer* aNativeLayer, const gfx::IntRegion& aInvalidRegion);

  already_AddRefed<CompositingRenderTarget> CreateRenderTarget(
      const gfx::IntRect& aRect, SurfaceInitMode aInit) override;

  void SetRenderTarget(CompositingRenderTarget* aSurface) override;
  already_AddRefed<CompositingRenderTarget> GetCurrentRenderTarget()
      const override;
  already_AddRefed<CompositingRenderTarget> GetWindowRenderTarget()
      const override;

  bool ReadbackRenderTarget(CompositingRenderTarget* aSource,
                            AsyncReadbackBuffer* aDest) override;

  already_AddRefed<AsyncReadbackBuffer> CreateAsyncReadbackBuffer(
      const gfx::IntSize& aSize) override;

  bool BlitRenderTarget(CompositingRenderTarget* aSource,
                        const gfx::IntSize& aSourceSize,
                        const gfx::IntSize& aDestSize) override;

  void DrawQuad(const gfx::Rect& aRect, const gfx::IntRect& aClipRect,
                const EffectChain& aEffectChain, gfx::Float aOpacity,
                const gfx::Matrix4x4& aTransform,
                const gfx::Rect& aVisibleRect) override;

  void EndFrame() override;

  int32_t GetMaxTextureSize() const override;

  /**
   * Set the size of the EGL surface we're rendering to, if we're rendering to
   * an EGL surface.
   */
  void SetDestinationSurfaceSize(const gfx::IntSize& aSize) override;

  typedef uint32_t MakeCurrentFlags;
  static const MakeCurrentFlags ForceMakeCurrent = 0x1;
  void MakeCurrent(MakeCurrentFlags aFlags = 0);

#ifdef MOZ_DUMP_PAINTING
  const char* Name() const override { return "OGL"; }
#endif  // MOZ_DUMP_PAINTING

  void Pause() override;
  bool Resume() override;

  GLContext* gl() const { return mGLContext; }
  GLContext* GetGLContext() const override { return mGLContext; }

  /**
   * Clear the program state. This must be called
   * before operating on the GLContext directly. */
  void ResetProgram();

  gfx::SurfaceFormat GetFBOFormat() const {
    return gfx::SurfaceFormat::R8G8B8A8;
  }

  /**
   * The compositor provides with temporary textures for use with direct
   * textruing.
   */
  GLuint GetTemporaryTexture(GLenum aTarget, GLenum aUnit);

  const gfx::IntSize GetDestinationSurfaceSize() const {
    return gfx::IntSize(mSurfaceSize.width, mSurfaceSize.height);
  }

  /**
   * Allow the origin of the surface to be offset so that content does not
   * start at (0, 0) on the surface.
   */
  void SetSurfaceOrigin(const ScreenIntPoint& aOrigin) {
    mSurfaceOrigin = aOrigin;
  }

  // Register TextureSource which own device data that have to be deleted before
  // destroying this CompositorOGL.
  void RegisterTextureSource(TextureSource* aTextureSource);
  void UnregisterTextureSource(TextureSource* aTextureSource);

  ipc::FileDescriptor GetReleaseFence();

 private:
  template <typename Geometry>
  void DrawGeometry(const Geometry& aGeometry, const gfx::Rect& aRect,
                    const gfx::IntRect& aClipRect,
                    const EffectChain& aEffectChain, gfx::Float aOpacity,
                    const gfx::Matrix4x4& aTransform,
                    const gfx::Rect& aVisibleRect);

  void PrepareViewport(CompositingRenderTargetOGL* aRenderTarget);

  void InsertFrameDoneSync();

  bool NeedToRecreateFullWindowRenderTarget() const;

  /** Widget associated with this compositor */
  LayoutDeviceIntSize mWidgetSize;
  RefPtr<GLContext> mGLContext;
  bool mOwnsGLContext = true;
  RefPtr<SurfacePoolHandle> mSurfacePoolHandle;
  gfx::Matrix4x4 mProjMatrix;
  bool mCanRenderToDefaultFramebuffer = true;

  /** The size of the surface we are rendering to */
  gfx::IntSize mSurfaceSize;

  /** The origin of the content on the surface */
  ScreenIntPoint mSurfaceOrigin;

  already_AddRefed<mozilla::gl::GLContext> CreateContext();

  /** Texture target to use for FBOs */
  GLenum mFBOTextureTarget;

  /** Currently bound render target */
  RefPtr<CompositingRenderTargetOGL> mCurrentRenderTarget;

  // The 1x1 dummy render target that's the "current" render target between
  // BeginFrameForNativeLayers and EndFrame but outside pairs of
  // Begin/EndRenderingToNativeLayer. Created on demand.
  RefPtr<CompositingRenderTarget> mNativeLayersReferenceRT;

  // The render target that profiler screenshots / frame recording read from.
  // This will be the actual window framebuffer when rendering to a window, and
  // it will be mFullWindowRenderTarget when rendering to native layers.
  RefPtr<CompositingRenderTargetOGL> mWindowRenderTarget;

  // Non-null when using native layers and frame recording is requested.
  // EndNormalDrawing() maintains a copy of the entire window contents in this
  // render target, by copying from the native layer render targets.
  RefPtr<CompositingRenderTargetOGL> mFullWindowRenderTarget;

  /**
   * VBO that has some basics in it for a textured quad, including vertex
   * coords and texcoords.
   */
  GLuint mQuadVBO;

  /**
   * VBO that stores dynamic triangle geometry.
   */
  GLuint mTriangleVBO;

  // Used to apply back-pressure in WaitForPreviousFrameDoneSync().
  GLsync mPreviousFrameDoneSync;
  GLsync mThisFrameDoneSync;

  bool mHasBGRA;

  /**
   * When rendering to some EGL surfaces (e.g. on Android), we rely on being
   * told about size changes (via SetSurfaceSize) rather than pulling this
   * information from the widget.
   */
  bool mUseExternalSurfaceSize;

  /**
   * Have we had DrawQuad calls since the last frame was rendered?
   */
  bool mFrameInProgress;

  // Only true between BeginFromeForNativeLayers and EndFrame, and only if the
  // full window render target needed to be recreated in the current frame.
  bool mShouldInvalidateWindow = false;

  /* Start a new frame.
   */
  Maybe<gfx::IntRect> BeginFrameForWindow(
      const nsIntRegion& aInvalidRegion, const Maybe<gfx::IntRect>& aClipRect,
      const gfx::IntRect& aRenderBounds,
      const nsIntRegion& aOpaqueRegion) override;

  Maybe<gfx::IntRect> BeginFrame(const nsIntRegion& aInvalidRegion,
                                 const Maybe<gfx::IntRect>& aClipRect,
                                 const gfx::IntRect& aRenderBounds,
                                 const nsIntRegion& aOpaqueRegion);

  ShaderConfigOGL GetShaderConfigFor(Effect* aEffect,
                                     bool aDEAAEnabled = false) const;

  ShaderProgramOGL* GetShaderProgramFor(const ShaderConfigOGL& aConfig);

  void ApplyPrimitiveConfig(ShaderConfigOGL& aConfig, const gfx::Rect&) {
    aConfig.SetDynamicGeometry(false);
  }

  void ApplyPrimitiveConfig(ShaderConfigOGL& aConfig,
                            const nsTArray<gfx::TexturedTriangle>&) {
    aConfig.SetDynamicGeometry(true);
  }

  /**
   * Create a FBO backed by a texture.
   * Note that the texture target type will be
   * of the type returned by FBOTextureTarget; different
   * shaders are required to sample from the different
   * texture types.
   */
  void CreateFBOWithTexture(const gfx::IntRect& aRect, bool aCopyFromSource,
                            GLuint aSourceFrameBuffer, GLuint* aFBO,
                            GLuint* aTexture,
                            gfx::IntSize* aAllocSize = nullptr);

  GLuint CreateTexture(const gfx::IntRect& aRect, bool aCopyFromSource,
                       GLuint aSourceFrameBuffer,
                       gfx::IntSize* aAllocSize = nullptr);

  gfx::Point3D GetLineCoefficients(const gfx::Point& aPoint1,
                                   const gfx::Point& aPoint2);

  void CleanupResources();

  void BindAndDrawQuads(ShaderProgramOGL* aProg, int aQuads,
                        const gfx::Rect* aLayerRect,
                        const gfx::Rect* aTextureRect);

  void BindAndDrawQuad(ShaderProgramOGL* aProg, const gfx::Rect& aLayerRect,
                       const gfx::Rect& aTextureRect = gfx::Rect(0.0f, 0.0f,
                                                                 1.0f, 1.0f)) {
    gfx::Rect layerRects[4];
    gfx::Rect textureRects[4];
    layerRects[0] = aLayerRect;
    textureRects[0] = aTextureRect;
    BindAndDrawQuads(aProg, 1, layerRects, textureRects);
  }

  void BindAndDrawGeometry(ShaderProgramOGL* aProgram, const gfx::Rect& aRect);

  void BindAndDrawGeometry(ShaderProgramOGL* aProgram,
                           const nsTArray<gfx::TexturedTriangle>& aTriangles);

  void BindAndDrawGeometryWithTextureRect(ShaderProgramOGL* aProg,
                                          const gfx::Rect& aRect,
                                          const gfx::Rect& aTexCoordRect,
                                          TextureSource* aTexture);

  void BindAndDrawGeometryWithTextureRect(
      ShaderProgramOGL* aProg,
      const nsTArray<gfx::TexturedTriangle>& aTriangles,
      const gfx::Rect& aTexCoordRect, TextureSource* aTexture);

  void InitializeVAO(const GLuint aAttribIndex, const GLint aComponents,
                     const GLsizei aStride, const size_t aOffset);

  gfx::Rect GetTextureCoordinates(gfx::Rect textureRect,
                                  TextureSource* aTexture);

  /**
   * Copies the content of the current render target to the set transaction
   * target.
   */
  void CopyToTarget(gfx::DrawTarget* aTarget, const nsIntPoint& aTopLeft,
                    const gfx::Matrix& aWorldMatrix);

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

  // The DrawTarget from BeginFrameForTarget, which EndFrame needs to copy the
  // window contents into.
  // Only non-null between BeginFrameForTarget and EndFrame.
  RefPtr<gfx::DrawTarget> mTarget;
  gfx::IntRect mTargetBounds;

  RefPtr<CompositorTexturePoolOGL> mTexturePool;

  // The native layer that we're currently rendering to, if any.
  // Non-null only between BeginFrame and EndFrame if BeginFrame has been called
  // with a non-null aNativeLayer.
  RefPtr<NativeLayer> mCurrentNativeLayer;

#ifdef MOZ_WIDGET_GTK
  // Hold TextureSources which own device data that have to be deleted before
  // destroying this CompositorOGL.
  std::unordered_set<TextureSource*> mRegisteredTextureSources;
#endif

  // FileDescriptor of release fence.
  // Release fence is a fence that is used for waiting until usage/composite of
  // AHardwareBuffer is ended. The fence is delivered to client side via
  // ImageBridge. It is used only on android.
  ipc::FileDescriptor mReleaseFenceFd;

  bool mDestroyed;

  /**
   * Size of the OpenGL context's primary framebuffer in pixels. Used by
   * FlipY for the y-flipping calculation and by the DEAA shader.
   */
  gfx::IntSize mViewportSize;

  gfx::IntRegion mCurrentFrameInvalidRegion;
};

}  // namespace layers
}  // namespace mozilla

#endif /* MOZILLA_GFX_COMPOSITOROGL_H */
