/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MOZILLA_GFX_DRAWTARGETWEBGL_H
#define _MOZILLA_GFX_DRAWTARGETWEBGL_H

#include "GLTypes.h"
#include "mozilla/Array.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/PathSkia.h"
#include "mozilla/LinkedList.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/ThreadLocal.h"
#include "mozilla/ipc/SharedMemoryBasic.h"
#include "mozilla/layers/LayersTypes.h"

#include <vector>

namespace WGR {
struct OutputVertex;
struct PathBuilder;
}  // namespace WGR

namespace mozilla {

class WebGLContext;
class WebGLBuffer;
class WebGLFramebuffer;
class WebGLProgram;
class WebGLRenderbuffer;
class WebGLTexture;
class WebGLUniformLocation;
class WebGLVertexArray;

namespace gl {
class GLContext;
}  // namespace gl

namespace layers {
class RemoteTextureOwnerClient;
}  // namespace layers

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
struct PathVertexRange;

// SharedContextWebgl stores most of the actual WebGL state that may be used by
// any number of DrawTargetWebgl's that use it. Foremost, it holds the actual
// WebGL client context, programs, and buffers for mapping to WebGL.
// Secondarily, it holds shared caches for surfaces, glyphs, paths, and
// shadows so that each DrawTargetWebgl does not require its own cache. It is
// important that SetTarget is called to install the current DrawTargetWebgl
// before actually using the SharedContext, as the individual framebuffers
// and viewport are still maintained in DrawTargetWebgl itself.
class SharedContextWebgl : public mozilla::RefCounted<SharedContextWebgl>,
                           public mozilla::SupportsWeakPtr {
  friend class DrawTargetWebgl;
  friend class SourceSurfaceWebgl;
  friend class TextureHandle;
  friend class SharedTextureHandle;
  friend class StandaloneTexture;

 public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(SharedContextWebgl)

  static already_AddRefed<SharedContextWebgl> Create();

  ~SharedContextWebgl();

  gl::GLContext* GetGLContext();

  void EnterTlsScope();
  void ExitTlsScope();

  bool IsContextLost() const;

  void OnMemoryPressure();

 private:
  SharedContextWebgl();

  WeakPtr<DrawTargetWebgl> mCurrentTarget;
  IntSize mViewportSize;
  // The current integer-aligned scissor rect.
  IntRect mClipRect;
  // The current fractional AA'd clip rect bounds.
  Rect mClipAARect;

  RefPtr<WebGLContext> mWebgl;

  // Avoid spurious state changes by caching last used state.
  RefPtr<WebGLProgram> mLastProgram;
  RefPtr<WebGLTexture> mLastTexture;
  RefPtr<WebGLTexture> mLastClipMask;

  // WebGL shader resources
  RefPtr<WebGLBuffer> mPathVertexBuffer;
  RefPtr<WebGLVertexArray> mPathVertexArray;
  // The current insertion offset into the GPU path buffer.
  uint32_t mPathVertexOffset = 0;
  // The maximum size of the GPU path buffer.
  uint32_t mPathVertexCapacity = 0;
  // The maximum supported type complexity of a GPU path.
  uint32_t mPathMaxComplexity = 0;
  // Whether to accelerate stroked paths with AAStroke.
  bool mPathAAStroke = true;
  // Whether to accelerate stroked paths with WGR.
  bool mPathWGRStroke = false;

  WGR::PathBuilder* mWGRPathBuilder = nullptr;
  // Temporary buffer for generating WGR output into.
  UniquePtr<WGR::OutputVertex[]> mWGROutputBuffer;

  RefPtr<WebGLProgram> mSolidProgram;
  Maybe<uint32_t> mSolidProgramViewport;
  Maybe<uint32_t> mSolidProgramAA;
  Maybe<uint32_t> mSolidProgramTransform;
  Maybe<uint32_t> mSolidProgramColor;
  Maybe<uint32_t> mSolidProgramClipMask;
  Maybe<uint32_t> mSolidProgramClipBounds;
  RefPtr<WebGLProgram> mImageProgram;
  Maybe<uint32_t> mImageProgramViewport;
  Maybe<uint32_t> mImageProgramAA;
  Maybe<uint32_t> mImageProgramTransform;
  Maybe<uint32_t> mImageProgramTexMatrix;
  Maybe<uint32_t> mImageProgramTexBounds;
  Maybe<uint32_t> mImageProgramColor;
  Maybe<uint32_t> mImageProgramSwizzle;
  Maybe<uint32_t> mImageProgramSampler;
  Maybe<uint32_t> mImageProgramClipMask;
  Maybe<uint32_t> mImageProgramClipBounds;

  struct SolidProgramUniformState {
    Maybe<Array<float, 2>> mViewport;
    Maybe<Array<float, 1>> mAA;
    Maybe<Array<float, 6>> mTransform;
    Maybe<Array<float, 4>> mColor;
    Maybe<Array<float, 4>> mClipBounds;
  } mSolidProgramUniformState;

  struct ImageProgramUniformState {
    Maybe<Array<float, 2>> mViewport;
    Maybe<Array<float, 1>> mAA;
    Maybe<Array<float, 6>> mTransform;
    Maybe<Array<float, 6>> mTexMatrix;
    Maybe<Array<float, 4>> mTexBounds;
    Maybe<Array<float, 4>> mColor;
    Maybe<Array<float, 1>> mSwizzle;
    Maybe<Array<float, 4>> mClipBounds;
  } mImageProgramUniformState;

  // Scratch framebuffer used to wrap textures for miscellaneous utility ops.
  RefPtr<WebGLFramebuffer> mScratchFramebuffer;
  // Buffer filled with zero data for initializing textures.
  RefPtr<WebGLBuffer> mZeroBuffer;
  size_t mZeroSize = 0;
  // 1x1 texture with solid white mask for disabling clipping
  RefPtr<WebGLTexture> mNoClipMask;

  uint32_t mMaxTextureSize = 0;
  bool mRasterizationTruncates = false;

  // The current blending operation.
  CompositionOp mLastCompositionOp = CompositionOp::OP_SOURCE;
  // The constant blend color used for the blending operation.
  Maybe<DeviceColor> mLastBlendColor;

  // The cached scissor state. Operations that rely on scissor state should
  // take care to enable or disable the cached scissor state as necessary.
  bool mScissorEnabled = false;
  IntRect mLastScissor = {-1, -1, -1, -1};

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
  // The total reserved memory for empty texture pages that are kept around
  // for future allocations.
  size_t mEmptyTextureMemory = 0;
  // A memory pressure event may signal from another thread that caches should
  // be cleared if possible.
  Atomic<bool> mShouldClearCaches;
  // The total number of DrawTargetWebgls using this shared context.
  size_t mDrawTargetCount = 0;
  // Whether we are inside a scoped usage of TLS MakeCurrent, and what previous
  // value to restore it to when exiting the scope.
  Maybe<bool> mTlsScope;

  bool Initialize();
  bool CreateShaders();
  void ResetPathVertexBuffer(bool aChanged = true);

  void BlendFunc(GLenum aSrcFactor, GLenum aDstFactor);
  void SetBlendState(CompositionOp aOp,
                     const Maybe<DeviceColor>& aColor = Nothing());

  void SetClipRect(const Rect& aClipRect);
  void SetClipRect(const IntRect& aClipRect) { SetClipRect(Rect(aClipRect)); }
  bool SetClipMask(const RefPtr<WebGLTexture>& aTex);
  bool SetNoClipMask();
  bool HasClipMask() const {
    return mLastClipMask && mLastClipMask != mNoClipMask;
  }

  Maybe<uint32_t> GetUniformLocation(const RefPtr<WebGLProgram>& prog,
                                     const std::string& aName) const;

  template <class T, size_t N>
  void UniformData(GLenum aFuncElemType, const Maybe<uint32_t>& aLoc,
                   const Array<T, N>& aData);

  // Avoids redundant UniformData calls by caching the previously set value.
  template <class T, size_t N>
  void MaybeUniformData(GLenum aFuncElemType, const Maybe<uint32_t>& aLoc,
                        const Array<T, N>& aData, Maybe<Array<T, N>>& aCached);

  bool IsCurrentTarget(DrawTargetWebgl* aDT) const {
    return aDT == mCurrentTarget;
  }
  bool SetTarget(DrawTargetWebgl* aDT);

  // Reset the current target.
  void ClearTarget() { mCurrentTarget = nullptr; }
  // Reset the last used texture to force binding next use.
  void ClearLastTexture(bool aFullClear = false);

  bool SupportsPattern(const Pattern& aPattern);

  void EnableScissor(const IntRect& aRect);
  void DisableScissor();

  void SetTexFilter(WebGLTexture* aTex, bool aFilter);
  void InitTexParameters(WebGLTexture* aTex, bool aFilter = true);

  bool ReadInto(uint8_t* aDstData, int32_t aDstStride, SurfaceFormat aFormat,
                const IntRect& aBounds, TextureHandle* aHandle = nullptr);
  already_AddRefed<DataSourceSurface> ReadSnapshot(
      TextureHandle* aHandle = nullptr);
  already_AddRefed<TextureHandle> WrapSnapshot(const IntSize& aSize,
                                               SurfaceFormat aFormat,
                                               RefPtr<WebGLTexture> aTex);
  already_AddRefed<TextureHandle> CopySnapshot(
      const IntRect& aRect, TextureHandle* aHandle = nullptr);

  already_AddRefed<WebGLTexture> GetCompatibleSnapshot(
      SourceSurface* aSurface) const;
  bool IsCompatibleSurface(SourceSurface* aSurface) const;

  bool UploadSurface(DataSourceSurface* aData, SurfaceFormat aFormat,
                     const IntRect& aSrcRect, const IntPoint& aDstOffset,
                     bool aInit, bool aZero = false,
                     const RefPtr<WebGLTexture>& aTex = nullptr);
  already_AddRefed<TextureHandle> AllocateTextureHandle(
      SurfaceFormat aFormat, const IntSize& aSize, bool aAllowShared = true,
      bool aRenderable = false);
  void DrawQuad();
  void DrawTriangles(const PathVertexRange& aRange);
  bool DrawRectAccel(const Rect& aRect, const Pattern& aPattern,
                     const DrawOptions& aOptions,
                     Maybe<DeviceColor> aMaskColor = Nothing(),
                     RefPtr<TextureHandle>* aHandle = nullptr,
                     bool aTransformed = true, bool aClipped = true,
                     bool aAccelOnly = false, bool aForceUpdate = false,
                     const StrokeOptions* aStrokeOptions = nullptr,
                     const PathVertexRange* aVertexRange = nullptr,
                     const Matrix* aRectXform = nullptr);

  already_AddRefed<TextureHandle> DrawStrokeMask(
      const PathVertexRange& aVertexRange, const IntSize& aSize);
  bool DrawPathAccel(const Path* aPath, const Pattern& aPattern,
                     const DrawOptions& aOptions,
                     const StrokeOptions* aStrokeOptions = nullptr,
                     bool aAllowStrokeAlpha = false,
                     const ShadowOptions* aShadow = nullptr,
                     bool aCacheable = true,
                     const Matrix* aPathXform = nullptr);

  bool DrawGlyphsAccel(ScaledFont* aFont, const GlyphBuffer& aBuffer,
                       const Pattern& aPattern, const DrawOptions& aOptions,
                       const StrokeOptions* aStrokeOptions,
                       bool aUseSubpixelAA);

  void PruneTextureHandle(const RefPtr<TextureHandle>& aHandle);
  bool PruneTextureMemory(size_t aMargin = 0, bool aPruneUnused = true);

  bool RemoveSharedTexture(const RefPtr<SharedTexture>& aTexture);
  bool RemoveStandaloneTexture(const RefPtr<StandaloneTexture>& aTexture);

  void UnlinkSurfaceTextures();
  void UnlinkSurfaceTexture(const RefPtr<TextureHandle>& aHandle);
  void UnlinkGlyphCaches();

  void ClearAllTextures();
  void ClearEmptyTextureMemory();
  void ClearCachesIfNecessary();

  void CachePrefs();
};

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
  friend class SourceSurfaceWebgl;
  friend class SharedContextWebgl;

 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(DrawTargetWebgl, override)

 private:
  IntSize mSize;
  RefPtr<WebGLFramebuffer> mFramebuffer;
  RefPtr<WebGLTexture> mTex;
  RefPtr<WebGLTexture> mClipMask;
  // The integer-aligned, scissor-compatible conservative bounds of the clip.
  IntRect mClipBounds;
  // The fractional, AA'd bounds of the clip rect, if applicable.
  Rect mClipAARect;
  RefPtr<DrawTargetSkia> mSkia;
  // Skia DT pointing to the same pixel data, but without any applied clips.
  RefPtr<DrawTargetSkia> mSkiaNoClip;
  // The Shmem backing the Skia DT, if applicable.
  RefPtr<mozilla::ipc::SharedMemoryBasic> mShmem;
  // The currently cached snapshot of the WebGL context
  RefPtr<SourceSurfaceWebgl> mSnapshot;
  // The mappable size of mShmem.
  uint32_t mShmemSize = 0;
  // Whether the framebuffer is still in the initially clear state.
  bool mIsClear = true;
  // Whether or not the Skia target has valid contents and is being drawn to
  bool mSkiaValid = false;
  // Whether or not Skia layering over the WebGL context is enabled
  bool mSkiaLayer = false;
  // Whether the WebGL target was clear when the Skia layer was established.
  bool mSkiaLayerClear = false;
  // Whether or not the WebGL context has valid contents and is being drawn to
  bool mWebglValid = true;
  // Whether or not the clip state has changed since last used by
  // SharedContextWebgl.
  bool mClipChanged = true;
  // Whether or not the clip state needs to be refreshed. Sometimes the clip
  // state may be overwritten and require a refresh later, even though it has
  // not changed.
  bool mRefreshClipState = true;
  // The number of layers currently pushed.
  int32_t mLayerDepth = 0;

  RefPtr<TextureHandle> mSnapshotTexture;

  // Cached unit circle path
  RefPtr<Path> mUnitCirclePath;

  // Store a log of clips currently pushed so that they can be used to init
  // the clip state of temporary DTs.
  struct ClipStack {
    Matrix mTransform;
    Rect mRect;
    RefPtr<const Path> mPath;

    bool operator==(const ClipStack& aOther) const;
  };

  std::vector<ClipStack> mClipStack;

  // The previous state of the clip stack when a mask was generated.
  std::vector<ClipStack> mCachedClipStack;

  // UsageProfile stores per-frame counters for significant profiling events
  // that assist in determining whether acceleration should still be used for
  // a Canvas2D user.
  struct UsageProfile {
    uint32_t mFailedFrames = 0;
    uint32_t mFrameCount = 0;
    uint32_t mCacheMisses = 0;
    uint32_t mCacheHits = 0;
    uint32_t mUncachedDraws = 0;
    uint32_t mLayers = 0;
    uint32_t mReadbacks = 0;
    uint32_t mFallbacks = 0;

    void BeginFrame();
    void EndFrame();
    bool RequiresRefresh() const;

    void OnCacheMiss() { ++mCacheMisses; }
    void OnCacheHit() { ++mCacheHits; }
    void OnUncachedDraw() { ++mUncachedDraws; }
    void OnLayer() { ++mLayers; }
    void OnReadback() { ++mReadbacks; }
    void OnFallback() { ++mFallbacks; }
  };

  UsageProfile mProfile;

  RefPtr<SharedContextWebgl> mSharedContext;

 public:
  DrawTargetWebgl();
  ~DrawTargetWebgl();

  static bool CanCreate(const IntSize& aSize, SurfaceFormat aFormat);
  static already_AddRefed<DrawTargetWebgl> Create(
      const IntSize& aSize, SurfaceFormat aFormat,
      const RefPtr<SharedContextWebgl>& aSharedContext);

  bool Init(const IntSize& aSize, SurfaceFormat aFormat,
            const RefPtr<SharedContextWebgl>& aSharedContext);

  bool IsValid() const override;

  DrawTargetType GetType() const override {
    return DrawTargetType::HARDWARE_RASTER;
  }
  BackendType GetBackendType() const override { return BackendType::WEBGL; }
  IntSize GetSize() const override { return mSize; }
  const RefPtr<SharedContextWebgl>& GetSharedContext() const {
    return mSharedContext;
  }

  bool HasDataSnapshot() const;
  void PrepareData();
  already_AddRefed<SourceSurface> GetDataSnapshot();
  already_AddRefed<SourceSurface> Snapshot() override;
  already_AddRefed<SourceSurface> GetOptimizedSnapshot(DrawTarget* aTarget);
  already_AddRefed<SourceSurface> GetBackingSurface() override;
  void DetachAllSnapshots() override;

  void BeginFrame(bool aInvalidContents = false);
  void EndFrame();
  bool RequiresRefresh() const { return mProfile.RequiresRefresh(); }

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
                             const ShadowOptions& aShadow,
                             CompositionOp aOperator) override;
  void DrawShadow(const Path* aPath, const Pattern& aPattern,
                  const ShadowOptions& aShadow, const DrawOptions& aOptions,
                  const StrokeOptions* aStrokeOptions = nullptr) override;

  void ClearRect(const Rect& aRect) override;
  void CopySurface(SourceSurface* aSurface, const IntRect& aSourceRect,
                   const IntPoint& aDestination) override;
  void FillRect(const Rect& aRect, const Pattern& aPattern,
                const DrawOptions& aOptions = DrawOptions()) override;
  void StrokeRect(const Rect& aRect, const Pattern& aPattern,
                  const StrokeOptions& aStrokeOptions = StrokeOptions(),
                  const DrawOptions& aOptions = DrawOptions()) override;
  bool StrokeLineAccel(const Point& aStart, const Point& aEnd,
                       const Pattern& aPattern,
                       const StrokeOptions& aStrokeOptions,
                       const DrawOptions& aOptions, bool aClosed = false);
  void StrokeLine(const Point& aStart, const Point& aEnd,
                  const Pattern& aPattern,
                  const StrokeOptions& aStrokeOptions = StrokeOptions(),
                  const DrawOptions& aOptions = DrawOptions()) override;
  void Stroke(const Path* aPath, const Pattern& aPattern,
              const StrokeOptions& aStrokeOptions = StrokeOptions(),
              const DrawOptions& aOptions = DrawOptions()) override;
  void Fill(const Path* aPath, const Pattern& aPattern,
            const DrawOptions& aOptions = DrawOptions()) override;
  void FillCircle(const Point& aOrigin, float aRadius, const Pattern& aPattern,
                  const DrawOptions& aOptions = DrawOptions()) override;
  void StrokeCircle(const Point& aOrigin, float aRadius,
                    const Pattern& aPattern,
                    const StrokeOptions& aStrokeOptions = StrokeOptions(),
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
  bool RemoveAllClips() override;
  void CopyToFallback(DrawTarget* aDT);
  void PushLayer(bool aOpaque, Float aOpacity, SourceSurface* aMask,
                 const Matrix& aMaskTransform,
                 const IntRect& aBounds = IntRect(),
                 bool aCopyBackground = false) override;
  void PushLayerWithBlend(
      bool aOpaque, Float aOpacity, SourceSurface* aMask,
      const Matrix& aMaskTransform, const IntRect& aBounds = IntRect(),
      bool aCopyBackground = false,
      CompositionOp aCompositionOp = CompositionOp::OP_OVER) override;
  void PopLayer() override;
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

  void CopyToSwapChain(
      layers::RemoteTextureId aId, layers::RemoteTextureOwnerId aOwnerId,
      layers::RemoteTextureOwnerClient* aOwnerClient = nullptr);

  void OnMemoryPressure() { mSharedContext->OnMemoryPressure(); }

  operator std::string() const {
    std::stringstream stream;
    stream << "DrawTargetWebgl(" << this << ")";
    return stream.str();
  }

  mozilla::ipc::SharedMemoryBasic::Handle TakeShmemHandle() const {
    return mShmem ? mShmem->TakeHandle()
                  : mozilla::ipc::SharedMemoryBasic::NULLHandle();
  }

  uint32_t GetShmemSize() const { return mShmemSize; }

 private:
  bool SupportsPattern(const Pattern& aPattern) {
    return mSharedContext->SupportsPattern(aPattern);
  }

  bool SetSimpleClipRect();
  bool GenerateComplexClipMask();
  bool PrepareContext(bool aClipped = true);

  void DrawRectFallback(const Rect& aRect, const Pattern& aPattern,
                        const DrawOptions& aOptions,
                        Maybe<DeviceColor> aMaskColor = Nothing(),
                        bool aTransform = true, bool aClipped = true,
                        const StrokeOptions* aStrokeOptions = nullptr);
  bool DrawRect(const Rect& aRect, const Pattern& aPattern,
                const DrawOptions& aOptions,
                Maybe<DeviceColor> aMaskColor = Nothing(),
                RefPtr<TextureHandle>* aHandle = nullptr,
                bool aTransformed = true, bool aClipped = true,
                bool aAccelOnly = false, bool aForceUpdate = false,
                const StrokeOptions* aStrokeOptions = nullptr);

  ColorPattern GetClearPattern() const;

  bool RectContainsViewport(const Rect& aRect) const;

  bool ShouldAccelPath(const DrawOptions& aOptions,
                       const StrokeOptions* aStrokeOptions);
  void DrawPath(const Path* aPath, const Pattern& aPattern,
                const DrawOptions& aOptions,
                const StrokeOptions* aStrokeOptions = nullptr,
                bool aAllowStrokeAlpha = false);
  void DrawCircle(const Point& aOrigin, float aRadius, const Pattern& aPattern,
                  const DrawOptions& aOptions,
                  const StrokeOptions* aStrokeOptions = nullptr);

  bool MarkChanged();

  bool ReadIntoSkia();
  void FlattenSkia();
  bool FlushFromSkia();

  void MarkSkiaChanged(bool aOverwrite = false);
  void MarkSkiaChanged(const DrawOptions& aOptions);

  bool ShouldUseSubpixelAA(ScaledFont* aFont, const DrawOptions& aOptions);

  bool ReadInto(uint8_t* aDstData, int32_t aDstStride);
  already_AddRefed<DataSourceSurface> ReadSnapshot();
  already_AddRefed<TextureHandle> CopySnapshot(const IntRect& aRect);
  already_AddRefed<TextureHandle> CopySnapshot() {
    return CopySnapshot(GetRect());
  }

  void ClearSnapshot(bool aCopyOnWrite = true, bool aNeedHandle = false);

  bool CreateFramebuffer();

  // PrepareContext may sometimes be used recursively. When this occurs, ensure
  // that clip state is restored after the context is used.
  struct AutoRestoreContext {
    DrawTargetWebgl* mTarget;
    Rect mClipAARect;
    RefPtr<WebGLTexture> mLastClipMask;

    explicit AutoRestoreContext(DrawTargetWebgl* aTarget);

    ~AutoRestoreContext();
  };
};

}  // namespace gfx
}  // namespace mozilla

#endif  // _MOZILLA_GFX_DRAWTARGETWEBGL_H
