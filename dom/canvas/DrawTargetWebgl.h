/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MOZILLA_GFX_DRAWTARGETWEBGL_H
#define _MOZILLA_GFX_DRAWTARGETWEBGL_H

#include "mozilla/gfx/2D.h"
#include "mozilla/LinkedList.h"
#include "mozilla/WeakPtr.h"
#include <vector>

namespace mozilla {

class ClientWebGLContext;
class WebGLBufferJS;
class WebGLFramebufferJS;
class WebGLProgramJS;
class WebGLRenderbufferJS;
class WebGLTextureJS;
class WebGLUniformLocationJS;
class WebGLVertexArrayJS;

namespace layers {
class SurfaceDescriptor;
}

namespace gfx {

class DataSourceSurface;
class DrawTargetSkia;
class DrawTargetWebgl;
class PathSkia;
class SourceSurfaceSkia;
class SourceSurfaceWebgl;

class TextureHandle;
class SharedTexture;
class SharedTextureHandle;
class StandaloneTexture;
class GlyphCache;
class PathCache;

// DrawTargetWebgl implements a subset of the DrawTarget API suitable for use
// by CanvasRenderingContext2D. It maps these to a client WebGL context so that
// they can be accelerated where possible by WebGL. It manages both routing to
// appropriate shaders and texture allocation/caching for surfaces. For commands
// that are not feasible to accelerate with WebGL, it mirrors state to a backup
// DrawTargetSkia that can be used as a fallback software renderer. Multiple
// instances of DrawTargetWebgl within a process will actually share a single
// WebGL context so that data can be more easily interchanged between them and
// also to enable more reasonable limiting of resource usage.
class DrawTargetWebgl : public DrawTarget, public SupportsWeakPtr {
  friend class SharedTextureHandle;
  friend class StandaloneTexture;
  friend class TextureHandle;
  friend class SourceSurfaceWebgl;
  friend class AutoSaveContext;

 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(DrawTargetWebgl, override)

 private:
  IntSize mSize;
  RefPtr<WebGLFramebufferJS> mFramebuffer;
  RefPtr<WebGLTextureJS> mTex;
  RefPtr<DrawTargetSkia> mSkia;
  // The currently cached snapshot of the WebGL context
  RefPtr<DataSourceSurface> mSnapshot;
  // Whether or not the Skia target has valid contents and is being drawn to
  bool mSkiaValid = false;
  // Whether or not Skia layering over the WebGL context is enabled
  bool mSkiaLayer = false;
  // Whether or not the WebGL context has valid contents and is being drawn to
  bool mWebglValid = false;
  // Whether or not the clip state has changed since last used by SharedContext.
  bool mClipDirty = true;
  // The framebuffer has been modified and should be copied to the swap chain.
  bool mNeedsPresent = true;

  RefPtr<TextureHandle> mSnapshotTexture;

  // SharedContext stores most of the actual WebGL state that may be used by
  // any number of DrawTargetWebgl's that use it. Foremost, it holds the actual
  // WebGL client context, programs, and buffers for mapping to WebGL.
  // Secondarily, it holds shared caches for surfaces, glyphs, paths, and
  // shadows so that each DrawTargetWebgl does not require its own cache. It is
  // important that SetTarget is called to install the current DrawTargetWebgl
  // before actually using the SharedContext, as the individual framebuffers
  // and viewport are still maintained in DrawTargetWebgl itself.
  class SharedContext : public mozilla::RefCounted<SharedContext>,
                        public mozilla::SupportsWeakPtr {
   public:
    MOZ_DECLARE_REFCOUNTED_TYPENAME(SharedContext)

    SharedContext();
    ~SharedContext();

    int32_t mNumTargets = 0;
    WeakPtr<DrawTargetWebgl> mCurrentTarget;
    IntSize mViewportSize;
    IntRect mClipRect;

    RefPtr<ClientWebGLContext> mWebgl;

    // Avoid spurious state changes by caching last used state.
    RefPtr<WebGLProgramJS> mLastProgram;
    RefPtr<WebGLTextureJS> mLastTexture;

    // WebGL shader resources
    RefPtr<WebGLBufferJS> mVertexBuffer;
    RefPtr<WebGLVertexArrayJS> mVertexArray;
    RefPtr<WebGLProgramJS> mSolidProgram;
    RefPtr<WebGLUniformLocationJS> mSolidProgramTransform;
    RefPtr<WebGLUniformLocationJS> mSolidProgramColor;
    RefPtr<WebGLProgramJS> mImageProgram;
    RefPtr<WebGLUniformLocationJS> mImageProgramTransform;
    RefPtr<WebGLUniformLocationJS> mImageProgramTexMatrix;
    RefPtr<WebGLUniformLocationJS> mImageProgramTexBounds;
    RefPtr<WebGLUniformLocationJS> mImageProgramColor;
    RefPtr<WebGLUniformLocationJS> mImageProgramSwizzle;
    RefPtr<WebGLUniformLocationJS> mImageProgramSampler;

    // Scratch framebuffer used to wrap textures for miscellaneous utility ops.
    RefPtr<WebGLFramebufferJS> mScratchFramebuffer;

    uint32_t mMaxTextureSize = 0;

    CompositionOp mLastCompositionOp = CompositionOp::OP_SOURCE;

    // A most-recently-used list of allocated texture handles.
    LinkedList<RefPtr<TextureHandle>> mTextureHandles;
    size_t mNumTextureHandles = 0;
    // User data key linking a SourceSurface with its TextureHandle.
    UserDataKey mTextureHandleKey = {0};
    // User data key linking a SourceSurface with its shadow blur TextureHandle.
    UserDataKey mShadowTextureKey = {0};
    // User data key linking a ScaledFont with its GlyphCache.
    UserDataKey mGlyphCacheKey = {0};
    // List of all GlyphCaches currently allocated to fonts.
    LinkedList<GlyphCache> mGlyphCaches;
    // Cache of rasterized paths.
    UniquePtr<PathCache> mPathCache;
    // Collection of allocated shared texture pages that may be shared amongst
    // many handles.
    std::vector<RefPtr<SharedTexture>> mSharedTextures;
    // Collection of allocated standalone textures that have a single assigned
    // handle.
    std::vector<RefPtr<StandaloneTexture>> mStandaloneTextures;
    size_t mUsedTextureMemory = 0;
    size_t mTotalTextureMemory = 0;

    const Matrix& GetTransform() const { return mCurrentTarget->mTransform; }

    bool IsContextLost() const;

    bool Initialize();
    bool CreateShaders();

    void SetBlendState(CompositionOp aOp);

    void SetClipRect(const IntRect& aClipRect) { mClipRect = aClipRect; }

    bool IsCurrentTarget(DrawTargetWebgl* aDT) const {
      return aDT == mCurrentTarget;
    }
    bool SetTarget(DrawTargetWebgl* aDT);

    // Reset the current target.
    void ClearTarget() { mCurrentTarget = nullptr; }
    // Reset the last used texture to force binding next use.
    void ClearLastTexture();

    bool SupportsPattern(const Pattern& aPattern);

    void InitTexParameters(WebGLTextureJS* aTex);

    bool ReadInto(uint8_t* aDstData, int32_t aDstStride, SurfaceFormat aFormat,
                  const IntRect& aBounds, TextureHandle* aHandle = nullptr);
    already_AddRefed<DataSourceSurface> ReadSnapshot(
        TextureHandle* aHandle = nullptr);
    already_AddRefed<TextureHandle> WrapSnapshot(const IntSize& aSize,
                                                 SurfaceFormat aFormat,
                                                 RefPtr<WebGLTextureJS> aTex);
    already_AddRefed<TextureHandle> CopySnapshot();

    already_AddRefed<WebGLTextureJS> GetCompatibleSnapshot(
        SourceSurface* aSurface);

    bool UploadSurface(DataSourceSurface* aData, SurfaceFormat aFormat,
                       const IntRect& aSrcRect, const IntPoint& aDstOffset,
                       bool aInit);
    bool DrawRectAccel(const Rect& aRect, const Pattern& aPattern,
                       const DrawOptions& aOptions,
                       Maybe<DeviceColor> aMaskColor = Nothing(),
                       RefPtr<TextureHandle>* aHandle = nullptr,
                       bool aTransformed = true, bool aClipped = true,
                       bool aAccelOnly = false, bool aForceUpdate = false,
                       const StrokeOptions* aStrokeOptions = nullptr);

    bool DrawPathAccel(const Path* aPath, const Pattern& aPattern,
                       const DrawOptions& aOptions,
                       const StrokeOptions* aStrokeOptions = nullptr);

    bool DrawSurfaceWithShadowAccel(SourceSurface* aSurface, const Point& aDest,
                                    const DeviceColor& aColor,
                                    const Point& aOffset, Float aSigma,
                                    CompositionOp aOperator);

    bool FillGlyphsAccel(ScaledFont* aFont, const GlyphBuffer& aBuffer,
                         const Pattern& aPattern, const DrawOptions& aOptions,
                         bool aUseSubpixelAA);

    void PruneTextureHandle(const RefPtr<TextureHandle>& aHandle);
    bool PruneTextureMemory(size_t aMargin = 0, bool aPruneUnused = true);

    bool RemoveSharedTexture(const RefPtr<SharedTexture>& aTexture);
    bool RemoveStandaloneTexture(const RefPtr<StandaloneTexture>& aTexture);

    void UnlinkSurfaceTextures();
    void UnlinkSurfaceTexture(const RefPtr<TextureHandle>& aHandle);
    void UnlinkGlyphCaches();
  };

  RefPtr<SharedContext> mSharedContext;

  static RefPtr<SharedContext> sSharedContext;

 public:
  DrawTargetWebgl();
  ~DrawTargetWebgl();

  static already_AddRefed<DrawTargetWebgl> Create(const IntSize& aSize,
                                                  SurfaceFormat aFormat);

  bool Init(const IntSize& aSize, SurfaceFormat aFormat);

  bool IsValid() const override;

  DrawTargetType GetType() const override {
    return DrawTargetType::HARDWARE_RASTER;
  }
  BackendType GetBackendType() const override { return BackendType::WEBGL; }
  IntSize GetSize() const override { return mSize; }

  already_AddRefed<SourceSurface> Snapshot() override;
  already_AddRefed<SourceSurface> GetBackingSurface() override;
  void DetachAllSnapshots() override;

  void BeginFrame(const IntRect& aPersistedRect);
  void EndFrame();

  bool LockBits(uint8_t** aData, IntSize* aSize, int32_t* aStride,
                SurfaceFormat* aFormat, IntPoint* aOrigin = nullptr) override;
  void ReleaseBits(uint8_t* aData) override;

  void Flush() override {}
  void DrawSurface(
      SourceSurface* aSurface, const Rect& aDest, const Rect& aSource,
      const DrawSurfaceOptions& aSurfOptions = DrawSurfaceOptions(),
      const DrawOptions& aOptions = DrawOptions()) override;
  void DrawFilter(FilterNode* aNode, const Rect& aSourceRect,
                  const Point& aDestPoint,
                  const DrawOptions& aOptions = DrawOptions()) override;
  void DrawSurfaceWithShadow(SourceSurface* aSurface, const Point& aDest,
                             const DeviceColor& aColor, const Point& aOffset,
                             Float aSigma, CompositionOp aOperator) override;
  void ClearRect(const Rect& aRect) override;
  void CopySurface(SourceSurface* aSurface, const IntRect& aSourceRect,
                   const IntPoint& aDestination) override;
  void FillRect(const Rect& aRect, const Pattern& aPattern,
                const DrawOptions& aOptions = DrawOptions()) override;
  void StrokeRect(const Rect& aRect, const Pattern& aPattern,
                  const StrokeOptions& aStrokeOptions = StrokeOptions(),
                  const DrawOptions& aOptions = DrawOptions()) override;
  void StrokeLine(const Point& aStart, const Point& aEnd,
                  const Pattern& aPattern,
                  const StrokeOptions& aStrokeOptions = StrokeOptions(),
                  const DrawOptions& aOptions = DrawOptions()) override;
  void Stroke(const Path* aPath, const Pattern& aPattern,
              const StrokeOptions& aStrokeOptions = StrokeOptions(),
              const DrawOptions& aOptions = DrawOptions()) override;
  void Fill(const Path* aPath, const Pattern& aPattern,
            const DrawOptions& aOptions = DrawOptions()) override;

  void SetPermitSubpixelAA(bool aPermitSubpixelAA) override;
  void FillGlyphs(ScaledFont* aFont, const GlyphBuffer& aBuffer,
                  const Pattern& aPattern,
                  const DrawOptions& aOptions = DrawOptions()) override;
  void StrokeGlyphs(ScaledFont* aFont, const GlyphBuffer& aBuffer,
                    const Pattern& aPattern,
                    const StrokeOptions& aStrokeOptions = StrokeOptions(),
                    const DrawOptions& aOptions = DrawOptions()) override;
  void Mask(const Pattern& aSource, const Pattern& aMask,
            const DrawOptions& aOptions = DrawOptions()) override;
  void MaskSurface(const Pattern& aSource, SourceSurface* aMask, Point aOffset,
                   const DrawOptions& aOptions = DrawOptions()) override;
  bool Draw3DTransformedSurface(SourceSurface* aSurface,
                                const Matrix4x4& aMatrix) override;
  void PushClip(const Path* aPath) override;
  void PushClipRect(const Rect& aRect) override;
  void PushDeviceSpaceClipRects(const IntRect* aRects,
                                uint32_t aCount) override;
  void PopClip() override;
  void PushLayer(bool aOpaque, Float aOpacity, SourceSurface* aMask,
                 const Matrix& aMaskTransform,
                 const IntRect& aBounds = IntRect(),
                 bool aCopyBackground = false) override {
    MOZ_ASSERT(false);
  }
  void PushLayerWithBlend(
      bool aOpaque, Float aOpacity, SourceSurface* aMask,
      const Matrix& aMaskTransform, const IntRect& aBounds = IntRect(),
      bool aCopyBackground = false,
      CompositionOp aCompositionOp = CompositionOp::OP_OVER) override {
    MOZ_ASSERT(false);
  }
  void PopLayer() override { MOZ_ASSERT(false); }
  already_AddRefed<SourceSurface> CreateSourceSurfaceFromData(
      unsigned char* aData, const IntSize& aSize, int32_t aStride,
      SurfaceFormat aFormat) const override;
  already_AddRefed<SourceSurface> OptimizeSourceSurface(
      SourceSurface* aSurface) const override;
  already_AddRefed<SourceSurface> OptimizeSourceSurfaceForUnknownAlpha(
      SourceSurface* aSurface) const override;
  already_AddRefed<SourceSurface> CreateSourceSurfaceFromNativeSurface(
      const NativeSurface& aSurface) const override;
  already_AddRefed<DrawTarget> CreateSimilarDrawTarget(
      const IntSize& aSize, SurfaceFormat aFormat) const override;
  bool CanCreateSimilarDrawTarget(const IntSize& aSize,
                                  SurfaceFormat aFormat) const override;
  RefPtr<DrawTarget> CreateClippedDrawTarget(const Rect& aBounds,
                                             SurfaceFormat aFormat) override;

  already_AddRefed<PathBuilder> CreatePathBuilder(
      FillRule aFillRule = FillRule::FILL_WINDING) const override;
  already_AddRefed<GradientStops> CreateGradientStops(
      GradientStop* aStops, uint32_t aNumStops,
      ExtendMode aExtendMode = ExtendMode::CLAMP) const override;
  already_AddRefed<FilterNode> CreateFilter(FilterType aType) override;
  void SetTransform(const Matrix& aTransform) override;
  void* GetNativeSurface(NativeSurfaceType aType) override;

  Maybe<layers::SurfaceDescriptor> GetFrontBuffer();

  bool CopySnapshotTo(DrawTarget* aDT);

  operator std::string() const {
    std::stringstream stream;
    stream << "DrawTargetWebgl(" << this << ")";
    return stream.str();
  }

 private:
  bool SupportsPattern(const Pattern& aPattern) {
    return mSharedContext->SupportsPattern(aPattern);
  }

  bool PrepareContext(bool aClipped = true);

  void DrawRectFallback(const Rect& aRect, const Pattern& aPattern,
                        const DrawOptions& aOptions,
                        Maybe<DeviceColor> aMaskColor, bool aTransform,
                        bool aClipped, const StrokeOptions* aStrokeOptions);
  bool DrawRect(const Rect& aRect, const Pattern& aPattern,
                const DrawOptions& aOptions,
                Maybe<DeviceColor> aMaskColor = Nothing(),
                RefPtr<TextureHandle>* aHandle = nullptr,
                bool aTransformed = true, bool aClipped = true,
                bool aAccelOnly = false, bool aForceUpdate = false,
                const StrokeOptions* aStrokeOptions = nullptr);
  void DrawPath(const Path* aPath, const Pattern& aPattern,
                const DrawOptions& aOptions,
                const StrokeOptions* aStrokeOptions = nullptr);

  bool MarkChanged();

  void ReadIntoSkia();
  void FlattenSkia();
  bool FlushFromSkia();

  void MarkSkiaChanged() {
    if (!mSkiaValid) {
      ReadIntoSkia();
    } else if (mSkiaLayer) {
      FlattenSkia();
    }
    mWebglValid = false;
  }

  void MarkSkiaChanged(const DrawOptions& aOptions);

  bool ReadInto(uint8_t* aDstData, int32_t aDstStride);
  already_AddRefed<DataSourceSurface> ReadSnapshot();
  already_AddRefed<TextureHandle> CopySnapshot();
  void ClearSnapshot(bool aCopyOnWrite = true);

  bool CreateFramebuffer();

  // PrepareContext may sometimes be used recursively. When this occurs, ensure
  // that clip state is restored after the context is used.
  struct AutoRestoreContext {
    DrawTargetWebgl* mTarget;
    IntRect mClipRect;

    explicit AutoRestoreContext(DrawTargetWebgl* aTarget)
        : mTarget(aTarget), mClipRect(aTarget->mSharedContext->mClipRect) {}

    ~AutoRestoreContext() {
      mTarget->mSharedContext->SetClipRect(mClipRect);
      mTarget->mClipDirty = true;
    }
  };
};

}  // namespace gfx
}  // namespace mozilla

#endif  // _MOZILLA_GFX_DRAWTARGETWEBGL_H
