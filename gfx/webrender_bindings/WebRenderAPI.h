/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_WEBRENDERAPI_H
#define MOZILLA_LAYERS_WEBRENDERAPI_H

#include <queue>
#include <stdint.h>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/gfx/CompositorHitTestInfo.h"
#include "mozilla/layers/AsyncImagePipelineOp.h"
#include "mozilla/layers/IpcResourceUpdateQueue.h"
#include "mozilla/layers/RemoteTextureMap.h"
#include "mozilla/layers/ScrollableLayerGuid.h"
#include "mozilla/layers/SyncObject.h"
#include "mozilla/layers/CompositionRecorder.h"
#include "mozilla/MozPromise.h"
#include "mozilla/Range.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/VsyncDispatcher.h"
#include "mozilla/webrender/webrender_ffi.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "nsString.h"
#include "GLTypes.h"
#include "Units.h"

class gfxContext;

#undef None

namespace mozilla {

class nsDisplayItem;
class nsPaintedDisplayItem;
class nsDisplayTransform;
class nsDisplayListBuilder;
struct DisplayItemClipChain;

struct ActiveScrolledRoot;

namespace widget {
class CompositorWidget;
}

namespace layers {
class CompositorBridgeParent;
class DisplayItemCache;
class WebRenderBridgeParent;
class RenderRootStateManager;
class StackingContextHelper;
struct DisplayListData;
}  // namespace layers

namespace layout {
class TextDrawTarget;
}

namespace wr {

class DisplayListBuilder;
class RendererOGL;
class RendererEvent;
class WebRenderAPI;

// This isn't part of WR's API, but we define it here to simplify layout's
// logic and data plumbing.
struct Line {
  wr::LayoutRect bounds;
  float wavyLineThickness;
  wr::LineOrientation orientation;
  wr::ColorF color;
  wr::LineStyle style;
};

/// A handler that can be bundled into a transaction and notified at specific
/// points in the rendering pipeline, such as after scene building or after
/// frame building.
///
/// If for any reason the handler is dropped before reaching the requested
/// point, it is notified with the value Checkpoint::TransactionDropped.
/// So it is safe to assume that the handler will be notified "at some point".
class NotificationHandler {
 public:
  virtual void Notify(wr::Checkpoint aCheckpoint) = 0;
  virtual ~NotificationHandler() = default;
};

struct WrHitResult {
  layers::LayersId mLayersId;
  layers::ScrollableLayerGuid::ViewID mScrollId;
  gfx::CompositorHitTestInfo mHitInfo;
  SideBits mSideBits;
  Maybe<uint64_t> mAnimationId;
};

class TransactionBuilder final {
 public:
  explicit TransactionBuilder(WebRenderAPI* aApi,
                              bool aUseSceneBuilderThread = true);

  TransactionBuilder(WebRenderAPI* aApi, Transaction* aTxn,
                     bool aUseSceneBuilderThread, bool aOwnsData);

  ~TransactionBuilder();

  void SetLowPriority(bool aIsLowPriority);

  void UpdateEpoch(PipelineId aPipelineId, Epoch aEpoch);

  void SetRootPipeline(PipelineId aPipelineId);

  void RemovePipeline(PipelineId aPipelineId);

  void SetDisplayList(Epoch aEpoch, wr::WrPipelineId pipeline_id,
                      wr::BuiltDisplayListDescriptor dl_descriptor,
                      wr::Vec<uint8_t>& dl_items_data,
                      wr::Vec<uint8_t>& dl_cache_data,
                      wr::Vec<uint8_t>& dl_spatial_tree);

  void ClearDisplayList(Epoch aEpoch, wr::WrPipelineId aPipeline);

  void GenerateFrame(const VsyncId& aVsyncId, wr::RenderReasons aReasons);

  void InvalidateRenderedFrame(wr::RenderReasons aReasons);

  void SetDocumentView(const LayoutDeviceIntRect& aDocRect);

  bool IsEmpty() const;

  bool IsResourceUpdatesEmpty() const;

  bool IsRenderedFrameInvalidated() const;

  void AddImage(wr::ImageKey aKey, const ImageDescriptor& aDescriptor,
                wr::Vec<uint8_t>& aBytes);

  void AddBlobImage(wr::BlobImageKey aKey, const ImageDescriptor& aDescriptor,
                    uint16_t aTileSize, wr::Vec<uint8_t>& aBytes,
                    const wr::DeviceIntRect& aVisibleRect);

  void AddExternalImageBuffer(ImageKey key, const ImageDescriptor& aDescriptor,
                              ExternalImageId aHandle);

  void AddExternalImage(ImageKey key, const ImageDescriptor& aDescriptor,
                        ExternalImageId aExtID,
                        wr::ExternalImageType aImageType,
                        uint8_t aChannelIndex = 0);

  void UpdateImageBuffer(wr::ImageKey aKey, const ImageDescriptor& aDescriptor,
                         wr::Vec<uint8_t>& aBytes);

  void UpdateBlobImage(wr::BlobImageKey aKey,
                       const ImageDescriptor& aDescriptor,
                       wr::Vec<uint8_t>& aBytes,
                       const wr::DeviceIntRect& aVisibleRect,
                       const wr::LayoutIntRect& aDirtyRect);

  void UpdateExternalImage(ImageKey aKey, const ImageDescriptor& aDescriptor,
                           ExternalImageId aExtID,
                           wr::ExternalImageType aImageType,
                           uint8_t aChannelIndex = 0);

  void UpdateExternalImageWithDirtyRect(ImageKey aKey,
                                        const ImageDescriptor& aDescriptor,
                                        ExternalImageId aExtID,
                                        wr::ExternalImageType aImageType,
                                        const wr::DeviceIntRect& aDirtyRect,
                                        uint8_t aChannelIndex = 0);

  void SetBlobImageVisibleArea(BlobImageKey aKey,
                               const wr::DeviceIntRect& aArea);

  void DeleteImage(wr::ImageKey aKey);

  void DeleteBlobImage(wr::BlobImageKey aKey);

  void AddRawFont(wr::FontKey aKey, wr::Vec<uint8_t>& aBytes, uint32_t aIndex);

  void AddFontDescriptor(wr::FontKey aKey, wr::Vec<uint8_t>& aBytes,
                         uint32_t aIndex);

  void DeleteFont(wr::FontKey aKey);

  void AddFontInstance(wr::FontInstanceKey aKey, wr::FontKey aFontKey,
                       float aGlyphSize,
                       const wr::FontInstanceOptions* aOptions,
                       const wr::FontInstancePlatformOptions* aPlatformOptions,
                       wr::Vec<uint8_t>& aVariations);

  void DeleteFontInstance(wr::FontInstanceKey aKey);

  void UpdateQualitySettings(bool aForceSubpixelAAWherePossible);

  void Notify(wr::Checkpoint aWhen, UniquePtr<NotificationHandler> aHandler);

  void Clear();

  Transaction* Take();

  bool UseSceneBuilderThread() const { return mUseSceneBuilderThread; }
  layers::WebRenderBackend GetBackendType() { return mApiBackend; }
  Transaction* Raw() { return mTxn; }

 protected:
  Transaction* mTxn;
  bool mUseSceneBuilderThread;
  layers::WebRenderBackend mApiBackend;
  bool mOwnsData;
};

class TransactionWrapper final {
 public:
  explicit TransactionWrapper(Transaction* aTxn);

  void AppendDynamicProperties(
      const nsTArray<wr::WrOpacityProperty>& aOpacityArray,
      const nsTArray<wr::WrTransformProperty>& aTransformArray,
      const nsTArray<wr::WrColorProperty>& aColorArray);
  void AppendTransformProperties(
      const nsTArray<wr::WrTransformProperty>& aTransformArray);
  void UpdateScrollPosition(
      const wr::WrPipelineId& aPipelineId,
      const layers::ScrollableLayerGuid::ViewID& aScrollId,
      const nsTArray<wr::SampledScrollOffset>& aSampledOffsets);
  void UpdateIsTransformAsyncZooming(uint64_t aAnimationId, bool aIsZooming);
  void AddMinimapData(const wr::WrPipelineId& aPipelineId,
                      const layers::ScrollableLayerGuid::ViewID& aScrollId,
                      const MinimapData& aMinimapData);

 private:
  Transaction* mTxn;
};

class WebRenderAPI final {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WebRenderAPI);

 public:
  /// This can be called on the compositor thread only.
  static already_AddRefed<WebRenderAPI> Create(
      layers::CompositorBridgeParent* aBridge,
      RefPtr<widget::CompositorWidget>&& aWidget,
      const wr::WrWindowId& aWindowId, LayoutDeviceIntSize aSize,
      layers::WindowKind aWindowKind, nsACString& aError);

  already_AddRefed<WebRenderAPI> Clone();

  void DestroyRenderer();

  wr::WindowId GetId() const { return mId; }

  /// Do a non-blocking hit-testing query on a shared version of the hit
  /// testing information.
  std::vector<WrHitResult> HitTest(const wr::WorldPoint& aPoint);

  void SendTransaction(TransactionBuilder& aTxn);

  void SetFrameStartTime(const TimeStamp& aTime);

  void RunOnRenderThread(UniquePtr<RendererEvent> aEvent);

  void Readback(const TimeStamp& aStartTime, gfx::IntSize aSize,
                const gfx::SurfaceFormat& aFormat,
                const Range<uint8_t>& aBuffer, bool* aNeedsYFlip);

  void ClearAllCaches();
  void EnableNativeCompositor(bool aEnable);
  void SetBatchingLookback(uint32_t aCount);
  void SetBool(wr::BoolParameter, bool value);
  void SetInt(wr::IntParameter, int32_t value);

  void SetClearColor(const gfx::DeviceColor& aColor);
  void SetProfilerUI(const nsACString& aUIString);

  void Pause();
  bool Resume();

  void WakeSceneBuilder();
  void FlushSceneBuilder();

  void NotifyMemoryPressure();
  void AccumulateMemoryReport(wr::MemoryReport*);

  wr::WrIdNamespace GetNamespace();
  layers::WebRenderBackend GetBackendType() { return mBackend; }
  layers::WebRenderCompositor GetCompositorType() { return mCompositor; }
  uint32_t GetMaxTextureSize() const { return mMaxTextureSize; }
  bool GetUseANGLE() const { return mUseANGLE; }
  bool GetUseDComp() const { return mUseDComp; }
  bool GetUseTripleBuffering() const { return mUseTripleBuffering; }
  bool SupportsExternalBufferTextures() const {
    return mSupportsExternalBufferTextures;
  }
  layers::SyncHandle GetSyncHandle() const { return mSyncHandle; }

  void Capture();

  void StartCaptureSequence(const nsACString& aPath, uint32_t aFlags);
  void StopCaptureSequence();

  void BeginRecording(const TimeStamp& aRecordingStart,
                      wr::PipelineId aRootPipelineId);

  typedef MozPromise<layers::FrameRecording, nsresult, true>
      EndRecordingPromise;

  RefPtr<EndRecordingPromise> EndRecording();

  layers::RemoteTextureInfoList* GetPendingRemoteTextureInfoList();
  layers::AsyncImagePipelineOps* GetPendingAsyncImagePipelineOps(
      TransactionBuilder& aTxn);

  void FlushPendingWrTransactionEventsWithoutWait();
  void FlushPendingWrTransactionEventsWithWait();

  wr::WebRenderAPI* GetRootAPI();

 protected:
  WebRenderAPI(wr::DocumentHandle* aHandle, wr::WindowId aId,
               layers::WebRenderBackend aBackend,
               layers::WebRenderCompositor aCompositor,
               uint32_t aMaxTextureSize, bool aUseANGLE, bool aUseDComp,
               bool aUseTripleBuffering, bool aSupportsExternalBufferTextures,
               layers::SyncHandle aSyncHandle,
               wr::WebRenderAPI* aRootApi = nullptr,
               wr::WebRenderAPI* aRootDocumentApi = nullptr);

  ~WebRenderAPI();
  // Should be used only for shutdown handling
  void WaitFlushed();

  void UpdateDebugFlags(uint32_t aFlags);
  bool CheckIsRemoteTextureReady(layers::RemoteTextureInfoList* aList,
                                 const TimeStamp& aTimeStamp);
  void WaitRemoteTextureReady(layers::RemoteTextureInfoList* aList);

  enum class RemoteTextureWaitType : uint8_t {
    AsyncWait = 0,
    FlushWithWait = 1,
    FlushWithoutWait = 2
  };

  void HandleWrTransactionEvents(RemoteTextureWaitType aType);

  class WrTransactionEvent {
   public:
    enum class Tag {
      Transaction,
      PendingRemoteTextures,
      PendingAsyncImagePipelineOps,
    };
    const Tag mTag;
    const TimeStamp mTimeStamp;

   private:
    WrTransactionEvent(const Tag aTag,
                       UniquePtr<TransactionBuilder>&& aTransaction)
        : mTag(aTag),
          mTimeStamp(TimeStamp::Now()),
          mTransaction(std::move(aTransaction)) {
      MOZ_ASSERT(mTag == Tag::Transaction);
    }
    WrTransactionEvent(
        const Tag aTag,
        UniquePtr<layers::RemoteTextureInfoList>&& aPendingRemoteTextures)
        : mTag(aTag),
          mTimeStamp(TimeStamp::Now()),
          mPendingRemoteTextures(std::move(aPendingRemoteTextures)) {
      MOZ_ASSERT(mTag == Tag::PendingRemoteTextures);
    }
    WrTransactionEvent(const Tag aTag,
                       UniquePtr<layers::AsyncImagePipelineOps>&&
                           aPendingAsyncImagePipelineOps,
                       UniquePtr<TransactionBuilder>&& aTransaction)
        : mTag(aTag),
          mTimeStamp(TimeStamp::Now()),
          mPendingAsyncImagePipelineOps(
              std::move(aPendingAsyncImagePipelineOps)),
          mTransaction(std::move(aTransaction)) {
      MOZ_ASSERT(mTag == Tag::PendingAsyncImagePipelineOps);
    }

    UniquePtr<layers::RemoteTextureInfoList> mPendingRemoteTextures;
    UniquePtr<layers::AsyncImagePipelineOps> mPendingAsyncImagePipelineOps;
    UniquePtr<TransactionBuilder> mTransaction;

   public:
    static WrTransactionEvent Transaction(WebRenderAPI* aApi,
                                          wr::Transaction* aTxn,
                                          bool aUseSceneBuilderThread) {
      auto transaction = MakeUnique<TransactionBuilder>(
          aApi, aTxn, aUseSceneBuilderThread, /* aOwnsData */ true);
      return WrTransactionEvent(Tag::Transaction, std::move(transaction));
    }

    static WrTransactionEvent PendingRemoteTextures(
        UniquePtr<layers::RemoteTextureInfoList>&& aPendingRemoteTextures) {
      return WrTransactionEvent(Tag::PendingRemoteTextures,
                                std::move(aPendingRemoteTextures));
    }

    static WrTransactionEvent PendingAsyncImagePipelineOps(
        UniquePtr<layers::AsyncImagePipelineOps>&&
            aPendingAsyncImagePipelineOps,
        WebRenderAPI* aApi, wr::Transaction* aTxn,
        bool aUseSceneBuilderThread) {
      auto transaction = MakeUnique<TransactionBuilder>(
          aApi, aTxn, aUseSceneBuilderThread, /* aOwnsData */ false);
      return WrTransactionEvent(Tag::PendingAsyncImagePipelineOps,
                                std::move(aPendingAsyncImagePipelineOps),
                                std::move(transaction));
    }

    wr::Transaction* RawTransaction() {
      if (mTag == Tag::Transaction) {
        return mTransaction->Raw();
      }
      MOZ_ASSERT_UNREACHABLE("unexpected to be called");
      return nullptr;
    }

    TransactionBuilder* GetTransactionBuilder() {
      if (mTag == Tag::PendingAsyncImagePipelineOps) {
        return mTransaction.get();
      }
      MOZ_CRASH("Should not be called");
      return nullptr;
    }

    bool UseSceneBuilderThread() {
      if (mTag == Tag::Transaction) {
        return mTransaction->UseSceneBuilderThread();
      }
      MOZ_ASSERT_UNREACHABLE("unexpected to be called");
      return true;
    }

    layers::RemoteTextureInfoList* RemoteTextureInfoList() {
      if (mTag == Tag::PendingRemoteTextures) {
        MOZ_ASSERT(mPendingRemoteTextures);
        return mPendingRemoteTextures.get();
      }
      MOZ_ASSERT_UNREACHABLE("unexpected to be called");
      return nullptr;
    }

    layers::AsyncImagePipelineOps* AsyncImagePipelineOps() {
      if (mTag == Tag::PendingAsyncImagePipelineOps) {
        MOZ_ASSERT(mPendingAsyncImagePipelineOps);
        return mPendingAsyncImagePipelineOps.get();
      }
      MOZ_ASSERT_UNREACHABLE("unexpected to be called");
      return nullptr;
    }
  };

  wr::DocumentHandle* mDocHandle;
  wr::WindowId mId;
  layers::WebRenderBackend mBackend;
  layers::WebRenderCompositor mCompositor;
  int32_t mMaxTextureSize;
  bool mUseANGLE;
  bool mUseDComp;
  bool mUseTripleBuffering;
  bool mSupportsExternalBufferTextures;
  bool mCaptureSequence;
  layers::SyncHandle mSyncHandle;
  bool mRendererDestroyed;

  UniquePtr<layers::RemoteTextureInfoList> mPendingRemoteTextureInfoList;
  UniquePtr<layers::AsyncImagePipelineOps> mPendingAsyncImagePipelineOps;
  std::queue<WrTransactionEvent> mPendingWrTransactionEvents;

  // We maintain alive the root api to know when to shut the render backend
  // down, and the root api for the document to know when to delete the
  // document. mRootApi is null for the api object that owns the channel (and is
  // responsible for shutting it down), and mRootDocumentApi is null for the api
  // object owning (and responsible for destroying) a given document. All api
  // objects in the same window use the same channel, and some api objects write
  // to the same document (but there is only one owner for each channel and for
  // each document).
  const RefPtr<wr::WebRenderAPI> mRootApi;
  const RefPtr<wr::WebRenderAPI> mRootDocumentApi;

  friend class DisplayListBuilder;
  friend class layers::WebRenderBridgeParent;
};

// This is a RAII class that automatically sends the transaction on
// destruction. This is useful for code that has multiple exit points and we
// want to ensure that the stuff accumulated in the transaction gets sent
// regardless of which exit we take. Note that if the caller explicitly calls
// mApi->SendTransaction() that's fine too because that empties out the
// TransactionBuilder and leaves it as a valid empty transaction, so calling
// SendTransaction on it again ends up being a no-op.
class MOZ_RAII AutoTransactionSender {
 public:
  AutoTransactionSender(WebRenderAPI* aApi, TransactionBuilder* aTxn)
      : mApi(aApi), mTxn(aTxn) {
    MOZ_RELEASE_ASSERT(mApi);
    MOZ_RELEASE_ASSERT(aTxn);
  }

  ~AutoTransactionSender() { mApi->SendTransaction(*mTxn); }

 private:
  WebRenderAPI* mApi;
  TransactionBuilder* mTxn;
};

/**
 * A set of optional parameters for stacking context creation.
 */
struct MOZ_STACK_CLASS StackingContextParams : public WrStackingContextParams {
  StackingContextParams()
      : WrStackingContextParams{
            WrStackingContextClip::None(),
            nullptr,
            nullptr,
            nullptr,
            wr::TransformStyle::Flat,
            wr::WrReferenceFrameKind::Transform,
            false,
            false,
            false,
            nullptr,
            /* prim_flags = */ wr::PrimitiveFlags::IS_BACKFACE_VISIBLE,
            wr::MixBlendMode::Normal,
            wr::StackingContextFlags{0}} {}

  void SetPreserve3D(bool aPreserve) {
    transform_style =
        aPreserve ? wr::TransformStyle::Preserve3D : wr::TransformStyle::Flat;
  }

  // Fill this in only if this is for the root StackingContextHelper.
  nsIFrame* mRootReferenceFrame = nullptr;
  nsTArray<wr::FilterOp> mFilters;
  nsTArray<wr::WrFilterData> mFilterDatas;
  wr::LayoutRect mBounds = wr::ToLayoutRect(LayoutDeviceRect());
  const gfx::Matrix4x4* mBoundTransform = nullptr;
  const wr::WrTransformInfo* mTransformPtr = nullptr;
  nsDisplayTransform* mDeferredTransformItem = nullptr;
  // Whether the stacking context is possibly animated. This alters how
  // coordinates are transformed/snapped to invalidate less when transforms
  // change frequently.
  bool mAnimated = false;
  // Whether items should be rasterized in a local space that is (mostly)
  // invariant to transforms, i.e. disabling subpixel AA and screen space pixel
  // snapping on text runs that would only make sense in screen space.
  bool mRasterizeLocally = false;
};

/// This is a simple C++ wrapper around WrState defined in the rust bindings.
/// We may want to turn this into a direct wrapper on top of
/// WebRenderFrameBuilder instead, so the interface may change a bit.
class DisplayListBuilder final {
 public:
  explicit DisplayListBuilder(wr::PipelineId aId,
                              layers::WebRenderBackend aBackend);
  DisplayListBuilder(DisplayListBuilder&&) = default;

  ~DisplayListBuilder();

  void Save();
  void Restore();
  void ClearSave();

  usize Dump(usize aIndent, const Maybe<usize>& aStart,
             const Maybe<usize>& aEnd);
  void DumpSerializedDisplayList();

  void Begin(layers::DisplayItemCache* aCache = nullptr);
  void End(wr::BuiltDisplayList& aOutDisplayList);
  void End(layers::DisplayListData& aOutTransaction);

  Maybe<wr::WrSpatialId> PushStackingContext(
      const StackingContextParams& aParams, const wr::LayoutRect& aBounds,
      const wr::RasterSpace& aRasterSpace);
  void PopStackingContext(bool aIsReferenceFrame);

  wr::WrClipChainId DefineClipChain(const nsTArray<wr::WrClipId>& aClips,
                                    bool aParentWithCurrentChain = false);

  wr::WrClipId DefineImageMaskClip(const wr::ImageMask& aMask,
                                   const nsTArray<wr::LayoutPoint>&,
                                   wr::FillRule);
  wr::WrClipId DefineRoundedRectClip(Maybe<wr::WrSpatialId> aSpace,
                                     const wr::ComplexClipRegion& aComplex);
  wr::WrClipId DefineRectClip(Maybe<wr::WrSpatialId> aSpace,
                              wr::LayoutRect aClipRect);

  wr::WrSpatialId DefineStickyFrame(
      const wr::LayoutRect& aContentRect, const float* aTopMargin,
      const float* aRightMargin, const float* aBottomMargin,
      const float* aLeftMargin, const StickyOffsetBounds& aVerticalBounds,
      const StickyOffsetBounds& aHorizontalBounds,
      const wr::LayoutVector2D& aAppliedOffset, wr::SpatialTreeItemKey aKey);

  Maybe<wr::WrSpatialId> GetScrollIdForDefinedScrollLayer(
      layers::ScrollableLayerGuid::ViewID aViewId) const;
  wr::WrSpatialId DefineScrollLayer(
      const layers::ScrollableLayerGuid::ViewID& aViewId,
      const Maybe<wr::WrSpatialId>& aParent, const wr::LayoutRect& aContentRect,
      const wr::LayoutRect& aClipRect, const wr::LayoutVector2D& aScrollOffset,
      wr::APZScrollGeneration aScrollOffsetGeneration,
      wr::HasScrollLinkedEffect aHasScrollLinkedEffect,
      wr::SpatialTreeItemKey aKey);

  void PushRect(const wr::LayoutRect& aBounds, const wr::LayoutRect& aClip,
                bool aIsBackfaceVisible, bool aForceAntiAliasing,
                bool aIsCheckerboard, const wr::ColorF& aColor);
  void PushRectWithAnimation(const wr::LayoutRect& aBounds,
                             const wr::LayoutRect& aClip,
                             bool aIsBackfaceVisible, const wr::ColorF& aColor,
                             const WrAnimationProperty* aAnimation);
  void PushRoundedRect(const wr::LayoutRect& aBounds,
                       const wr::LayoutRect& aClip, bool aIsBackfaceVisible,
                       const wr::ColorF& aColor);
  void PushHitTest(const wr::LayoutRect& aBounds, const wr::LayoutRect& aClip,
                   bool aIsBackfaceVisible,
                   const layers::ScrollableLayerGuid::ViewID& aScrollId,
                   const gfx::CompositorHitTestInfo& aHitInfo,
                   SideBits aSideBits);
  void PushClearRect(const wr::LayoutRect& aBounds);

  void PushBackdropFilter(const wr::LayoutRect& aBounds,
                          const wr::ComplexClipRegion& aRegion,
                          const nsTArray<wr::FilterOp>& aFilters,
                          const nsTArray<wr::WrFilterData>& aFilterDatas,
                          bool aIsBackfaceVisible);

  void PushLinearGradient(const wr::LayoutRect& aBounds,
                          const wr::LayoutRect& aClip, bool aIsBackfaceVisible,
                          const wr::LayoutPoint& aStartPoint,
                          const wr::LayoutPoint& aEndPoint,
                          const nsTArray<wr::GradientStop>& aStops,
                          wr::ExtendMode aExtendMode,
                          const wr::LayoutSize aTileSize,
                          const wr::LayoutSize aTileSpacing);

  void PushRadialGradient(const wr::LayoutRect& aBounds,
                          const wr::LayoutRect& aClip, bool aIsBackfaceVisible,
                          const wr::LayoutPoint& aCenter,
                          const wr::LayoutSize& aRadius,
                          const nsTArray<wr::GradientStop>& aStops,
                          wr::ExtendMode aExtendMode,
                          const wr::LayoutSize aTileSize,
                          const wr::LayoutSize aTileSpacing);

  void PushConicGradient(const wr::LayoutRect& aBounds,
                         const wr::LayoutRect& aClip, bool aIsBackfaceVisible,
                         const wr::LayoutPoint& aCenter, const float aAngle,
                         const nsTArray<wr::GradientStop>& aStops,
                         wr::ExtendMode aExtendMode,
                         const wr::LayoutSize aTileSize,
                         const wr::LayoutSize aTileSpacing);

  void PushImage(const wr::LayoutRect& aBounds, const wr::LayoutRect& aClip,
                 bool aIsBackfaceVisible, bool aForceAntiAliasing,
                 wr::ImageRendering aFilter, wr::ImageKey aImage,
                 bool aPremultipliedAlpha = true,
                 const wr::ColorF& aColor = wr::ColorF{1.0f, 1.0f, 1.0f, 1.0f},
                 bool aPreferCompositorSurface = false,
                 bool aSupportsExternalCompositing = false);

  void PushRepeatingImage(
      const wr::LayoutRect& aBounds, const wr::LayoutRect& aClip,
      bool aIsBackfaceVisible, const wr::LayoutSize& aStretchSize,
      const wr::LayoutSize& aTileSpacing, wr::ImageRendering aFilter,
      wr::ImageKey aImage, bool aPremultipliedAlpha = true,
      const wr::ColorF& aColor = wr::ColorF{1.0f, 1.0f, 1.0f, 1.0f});

  void PushYCbCrPlanarImage(
      const wr::LayoutRect& aBounds, const wr::LayoutRect& aClip,
      bool aIsBackfaceVisible, wr::ImageKey aImageChannel0,
      wr::ImageKey aImageChannel1, wr::ImageKey aImageChannel2,
      wr::WrColorDepth aColorDepth, wr::WrYuvColorSpace aColorSpace,
      wr::WrColorRange aColorRange, wr::ImageRendering aFilter,
      bool aPreferCompositorSurface = false,
      bool aSupportsExternalCompositing = false);

  void PushNV12Image(const wr::LayoutRect& aBounds, const wr::LayoutRect& aClip,
                     bool aIsBackfaceVisible, wr::ImageKey aImageChannel0,
                     wr::ImageKey aImageChannel1, wr::WrColorDepth aColorDepth,
                     wr::WrYuvColorSpace aColorSpace,
                     wr::WrColorRange aColorRange, wr::ImageRendering aFilter,
                     bool aPreferCompositorSurface = false,
                     bool aSupportsExternalCompositing = false);

  void PushP010Image(const wr::LayoutRect& aBounds, const wr::LayoutRect& aClip,
                     bool aIsBackfaceVisible, wr::ImageKey aImageChannel0,
                     wr::ImageKey aImageChannel1, wr::WrColorDepth aColorDepth,
                     wr::WrYuvColorSpace aColorSpace,
                     wr::WrColorRange aColorRange, wr::ImageRendering aFilter,
                     bool aPreferCompositorSurface = false,
                     bool aSupportsExternalCompositing = false);

  void PushYCbCrInterleavedImage(
      const wr::LayoutRect& aBounds, const wr::LayoutRect& aClip,
      bool aIsBackfaceVisible, wr::ImageKey aImageChannel0,
      wr::WrColorDepth aColorDepth, wr::WrYuvColorSpace aColorSpace,
      wr::WrColorRange aColorRange, wr::ImageRendering aFilter,
      bool aPreferCompositorSurface = false,
      bool aSupportsExternalCompositing = false);

  void PushIFrame(const LayoutDeviceRect& aDevPxBounds, bool aIsBackfaceVisible,
                  wr::PipelineId aPipeline, bool aIgnoreMissingPipeline);

  // XXX WrBorderSides are passed with Range.
  // It is just to bypass compiler bug. See Bug 1357734.
  void PushBorder(const wr::LayoutRect& aBounds, const wr::LayoutRect& aClip,
                  bool aIsBackfaceVisible, const wr::LayoutSideOffsets& aWidths,
                  const Range<const wr::BorderSide>& aSides,
                  const wr::BorderRadius& aRadius,
                  wr::AntialiasBorder = wr::AntialiasBorder::Yes);

  void PushBorderImage(const wr::LayoutRect& aBounds,
                       const wr::LayoutRect& aClip, bool aIsBackfaceVisible,
                       const wr::WrBorderImage& aParams);

  void PushBorderGradient(const wr::LayoutRect& aBounds,
                          const wr::LayoutRect& aClip, bool aIsBackfaceVisible,
                          const wr::LayoutSideOffsets& aWidths,
                          const int32_t aWidth, const int32_t aHeight,
                          bool aFill, const wr::DeviceIntSideOffsets& aSlice,
                          const wr::LayoutPoint& aStartPoint,
                          const wr::LayoutPoint& aEndPoint,
                          const nsTArray<wr::GradientStop>& aStops,
                          wr::ExtendMode aExtendMode);

  void PushBorderRadialGradient(
      const wr::LayoutRect& aBounds, const wr::LayoutRect& aClip,
      bool aIsBackfaceVisible, const wr::LayoutSideOffsets& aWidths, bool aFill,
      const wr::LayoutPoint& aCenter, const wr::LayoutSize& aRadius,
      const nsTArray<wr::GradientStop>& aStops, wr::ExtendMode aExtendMode);

  void PushBorderConicGradient(
      const wr::LayoutRect& aBounds, const wr::LayoutRect& aClip,
      bool aIsBackfaceVisible, const wr::LayoutSideOffsets& aWidths, bool aFill,
      const wr::LayoutPoint& aCenter, const float aAngle,
      const nsTArray<wr::GradientStop>& aStops, wr::ExtendMode aExtendMode);

  void PushText(const wr::LayoutRect& aBounds, const wr::LayoutRect& aClip,
                bool aIsBackfaceVisible, const wr::ColorF& aColor,
                wr::FontInstanceKey aFontKey,
                Range<const wr::GlyphInstance> aGlyphBuffer,
                const wr::GlyphOptions* aGlyphOptions = nullptr);

  void PushLine(const wr::LayoutRect& aClip, bool aIsBackfaceVisible,
                const wr::Line& aLine);

  void PushShadow(const wr::LayoutRect& aBounds, const wr::LayoutRect& aClip,
                  bool aIsBackfaceVisible, const wr::Shadow& aShadow,
                  bool aShouldInflate);

  void PopAllShadows();

  void PushBoxShadow(const wr::LayoutRect& aRect, const wr::LayoutRect& aClip,
                     bool aIsBackfaceVisible, const wr::LayoutRect& aBoxBounds,
                     const wr::LayoutVector2D& aOffset,
                     const wr::ColorF& aColor, const float& aBlurRadius,
                     const float& aSpreadRadius,
                     const wr::BorderRadius& aBorderRadius,
                     const wr::BoxShadowClipMode& aClipMode);

  /**
   * Notifies the DisplayListBuilder that it can group together WR display items
   * that are pushed until |CancelGroup()| or |FinishGroup()| call.
   */
  void StartGroup(nsPaintedDisplayItem* aItem);

  /**
   * Cancels grouping of the display items and discards all the display items
   * pushed between the |StartGroup()| and |CancelGroup()| calls.
   */
  void CancelGroup(const bool aDiscard = false);

  /**
   * Finishes the display item group. The group is stored in WebRender backend,
   * and can be reused with |ReuseItem()|, if the Gecko display item is reused.
   */
  void FinishGroup();

  /**
   * Try to reuse the previously created WebRender display items for the given
   * Gecko display item |aItem|.
   * Returns true if the items were reused, otherwise returns false.
   */
  bool ReuseItem(nsPaintedDisplayItem* aItem);

  uint64_t CurrentClipChainId() const {
    return mCurrentSpaceAndClipChain.clip_chain;
  }

  const wr::WrSpaceAndClipChain& CurrentSpaceAndClipChain() const {
    return mCurrentSpaceAndClipChain;
  }

  const wr::PipelineId& CurrentPipelineId() const { return mPipelineId; }
  layers::WebRenderBackend GetBackendType() const { return mBackend; }

  // Checks to see if the innermost enclosing fixed pos item has the same
  // ASR. If so, it returns the scroll target for that fixed-pos item.
  // Otherwise, it returns Nothing().
  Maybe<layers::ScrollableLayerGuid::ViewID> GetContainingFixedPosScrollTarget(
      const ActiveScrolledRoot* aAsr);

  Maybe<SideBits> GetContainingFixedPosSideBits(const ActiveScrolledRoot* aAsr);

  gfxContext* GetTextContext(wr::IpcResourceUpdateQueue& aResources,
                             const layers::StackingContextHelper& aSc,
                             layers::RenderRootStateManager* aManager,
                             nsDisplayItem* aItem, nsRect& aBounds,
                             const gfx::Point& aDeviceOffset);

  // Try to avoid using this when possible.
  wr::WrState* Raw() { return mWrState; }

  void SetClipChainLeaf(const Maybe<wr::LayoutRect>& aClipRect) {
    mClipChainLeaf = aClipRect;
  }

  // Used for opacity flattening. When we flatten away an opacity item,
  // we push the opacity value onto the builder.
  // Descendant items should pull the inherited opacity during
  // their CreateWebRenderCommands implementation. This can only happen if all
  // descendant items reported supporting this functionality, via
  // nsDisplayItem::CanApplyOpacity.
  float GetInheritedOpacity() { return mInheritedOpacity; }
  void SetInheritedOpacity(float aOpacity) { mInheritedOpacity = aOpacity; }
  const DisplayItemClipChain* GetInheritedClipChain() {
    return mInheritedClipChain;
  }
  void PushInheritedClipChain(nsDisplayListBuilder* aBuilder,
                              const DisplayItemClipChain* aClipChain);
  void SetInheritedClipChain(const DisplayItemClipChain* aClipChain) {
    mInheritedClipChain = aClipChain;
  }

  layers::DisplayItemCache* GetDisplayItemCache() { return mDisplayItemCache; }

  // A chain of RAII objects, each holding a (ASR, ViewID, SideBits) tuple of
  // data. The topmost object is pointed to by the mActiveFixedPosTracker
  // pointer in the wr::DisplayListBuilder.
  class MOZ_RAII FixedPosScrollTargetTracker final {
   public:
    FixedPosScrollTargetTracker(DisplayListBuilder& aBuilder,
                                const ActiveScrolledRoot* aAsr,
                                layers::ScrollableLayerGuid::ViewID aScrollId,
                                SideBits aSideBits);
    ~FixedPosScrollTargetTracker();
    Maybe<layers::ScrollableLayerGuid::ViewID> GetScrollTargetForASR(
        const ActiveScrolledRoot* aAsr);
    Maybe<SideBits> GetSideBitsForASR(const ActiveScrolledRoot* aAsr);

   private:
    FixedPosScrollTargetTracker* mParentTracker;
    DisplayListBuilder& mBuilder;
    const ActiveScrolledRoot* mAsr;
    layers::ScrollableLayerGuid::ViewID mScrollId;
    SideBits mSideBits;
  };

 protected:
  wr::LayoutRect MergeClipLeaf(const wr::LayoutRect& aClip) {
    if (mClipChainLeaf) {
      return wr::IntersectLayoutRect(*mClipChainLeaf, aClip);
    }
    return aClip;
  }

  // See the implementation of PushShadow for details on these methods.
  void SuspendClipLeafMerging();
  void ResumeClipLeafMerging();

  wr::WrState* mWrState;

  // Track each scroll id that we encountered. We use this structure to
  // ensure that we don't define a particular scroll layer multiple times,
  // as that results in undefined behaviour in WR.
  std::unordered_map<layers::ScrollableLayerGuid::ViewID, wr::WrSpatialId>
      mScrollIds;

  wr::WrSpaceAndClipChain mCurrentSpaceAndClipChain;

  // Contains the current leaf of the clip chain to be merged with the
  // display item's clip rect when pushing an item. May be set to Nothing() if
  // there is no clip rect to merge with.
  Maybe<wr::LayoutRect> mClipChainLeaf;

  // Versions of the above that are on hold while SuspendClipLeafMerging is on
  // (see the implementation of PushShadow for details).
  Maybe<wr::WrSpaceAndClipChain> mSuspendedSpaceAndClipChain;
  Maybe<wr::LayoutRect> mSuspendedClipChainLeaf;

  RefPtr<layout::TextDrawTarget> mCachedTextDT;
  mozilla::UniquePtr<gfxContext> mCachedContext;

  FixedPosScrollTargetTracker* mActiveFixedPosTracker;

  wr::PipelineId mPipelineId;
  layers::WebRenderBackend mBackend;

  layers::DisplayItemCache* mDisplayItemCache;
  Maybe<uint16_t> mCurrentCacheSlot;
  float mInheritedOpacity = 1.0f;
  const DisplayItemClipChain* mInheritedClipChain = nullptr;

  friend class WebRenderAPI;
  friend class SpaceAndClipChainHelper;
};

// This is a RAII class that overrides the current Wr's SpatialId and
// ClipChainId.
class MOZ_RAII SpaceAndClipChainHelper final {
 public:
  SpaceAndClipChainHelper(DisplayListBuilder& aBuilder,
                          wr::WrSpaceAndClipChain aSpaceAndClipChain)
      : mBuilder(aBuilder),
        mOldSpaceAndClipChain(aBuilder.mCurrentSpaceAndClipChain) {
    aBuilder.mCurrentSpaceAndClipChain = aSpaceAndClipChain;
  }
  SpaceAndClipChainHelper(DisplayListBuilder& aBuilder,
                          wr::WrSpatialId aSpatialId)
      : mBuilder(aBuilder),
        mOldSpaceAndClipChain(aBuilder.mCurrentSpaceAndClipChain) {
    aBuilder.mCurrentSpaceAndClipChain.space = aSpatialId;
  }
  SpaceAndClipChainHelper(DisplayListBuilder& aBuilder,
                          wr::WrClipChainId aClipChainId)
      : mBuilder(aBuilder),
        mOldSpaceAndClipChain(aBuilder.mCurrentSpaceAndClipChain) {
    aBuilder.mCurrentSpaceAndClipChain.clip_chain = aClipChainId.id;
  }

  ~SpaceAndClipChainHelper() {
    mBuilder.mCurrentSpaceAndClipChain = mOldSpaceAndClipChain;
  }

 private:
  SpaceAndClipChainHelper(const SpaceAndClipChainHelper&) = delete;

  DisplayListBuilder& mBuilder;
  wr::WrSpaceAndClipChain mOldSpaceAndClipChain;
};

Maybe<wr::ImageFormat> SurfaceFormatToImageFormat(gfx::SurfaceFormat aFormat);

}  // namespace wr
}  // namespace mozilla

#endif
