/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_WEBRENDERAPI_H
#define MOZILLA_LAYERS_WEBRENDERAPI_H

#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/gfx/CompositorHitTestInfo.h"
#include "mozilla/layers/IpcResourceUpdateQueue.h"
#include "mozilla/layers/ScrollableLayerGuid.h"
#include "mozilla/layers/SyncObject.h"
#include "mozilla/layers/WebRenderCompositionRecorder.h"
#include "mozilla/Range.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/webrender/webrender_ffi.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "GLTypes.h"
#include "Units.h"

class nsDisplayItem;
class nsPaintedDisplayItem;
class nsDisplayTransform;

#undef None

namespace mozilla {

struct ActiveScrolledRoot;

namespace widget {
class CompositorWidget;
}

namespace layers {
class CompositorBridgeParent;
class DisplayItemCache;
class WebRenderBridgeParent;
class RenderRootStateManager;
struct RenderRootDisplayListData;
}  // namespace layers

namespace layout {
class TextDrawTarget;
}

namespace wr {

class DisplayListBuilder;
class RendererOGL;
class RendererEvent;

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
};

class TransactionBuilder final {
 public:
  explicit TransactionBuilder(bool aUseSceneBuilderThread = true);

  ~TransactionBuilder();

  void SetLowPriority(bool aIsLowPriority);

  void UpdateEpoch(PipelineId aPipelineId, Epoch aEpoch);

  void SetRootPipeline(PipelineId aPipelineId);

  void RemovePipeline(PipelineId aPipelineId);

  void SetDisplayList(const gfx::DeviceColor& aBgColor, Epoch aEpoch,
                      const wr::LayoutSize& aViewportSize,
                      wr::WrPipelineId pipeline_id,
                      const wr::LayoutSize& content_size,
                      wr::BuiltDisplayListDescriptor dl_descriptor,
                      wr::Vec<uint8_t>& dl_data);

  void ClearDisplayList(Epoch aEpoch, wr::WrPipelineId aPipeline);

  void GenerateFrame();

  void InvalidateRenderedFrame();

  void UpdateDynamicProperties(
      const nsTArray<wr::WrOpacityProperty>& aOpacityArray,
      const nsTArray<wr::WrTransformProperty>& aTransformArray,
      const nsTArray<wr::WrColorProperty>& aColorArray);

  void SetDocumentView(const LayoutDeviceIntRect& aDocRect);

  void UpdateScrollPosition(
      const wr::WrPipelineId& aPipelineId,
      const layers::ScrollableLayerGuid::ViewID& aScrollId,
      const wr::LayoutPoint& aScrollPosition);

  bool IsEmpty() const;

  bool IsResourceUpdatesEmpty() const;

  bool IsRenderedFrameInvalidated() const;

  void AddImage(wr::ImageKey aKey, const ImageDescriptor& aDescriptor,
                wr::Vec<uint8_t>& aBytes);

  void AddBlobImage(wr::BlobImageKey aKey, const ImageDescriptor& aDescriptor,
                    wr::Vec<uint8_t>& aBytes,
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

  void UpdateQualitySettings(bool aAllowSacrificingSubpixelAA);

  void Notify(wr::Checkpoint aWhen, UniquePtr<NotificationHandler> aHandler);

  void Clear();

  bool UseSceneBuilderThread() const { return mUseSceneBuilderThread; }
  Transaction* Raw() { return mTxn; }

 protected:
  bool mUseSceneBuilderThread;
  Transaction* mTxn;
};

class TransactionWrapper final {
 public:
  explicit TransactionWrapper(Transaction* aTxn);

  void AppendTransformProperties(
      const nsTArray<wr::WrTransformProperty>& aTransformArray);
  void UpdateScrollPosition(
      const wr::WrPipelineId& aPipelineId,
      const layers::ScrollableLayerGuid::ViewID& aScrollId,
      const wr::LayoutPoint& aScrollPosition);
  void UpdatePinchZoom(float aZoom);
  void UpdateIsTransformAsyncZooming(uint64_t aAnimationId, bool aIsZooming);

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
      const wr::WrWindowId& aWindowId, LayoutDeviceIntSize aSize);

  static void SendTransactions(
      const RenderRootArray<RefPtr<WebRenderAPI>>& aApis,
      RenderRootArray<TransactionBuilder*>& aTxns);

  already_AddRefed<WebRenderAPI> CreateDocument(LayoutDeviceIntSize aSize,
                                                int8_t aLayerIndex,
                                                wr::RenderRoot aRenderRoot);

  already_AddRefed<WebRenderAPI> Clone();

  wr::WindowId GetId() const { return mId; }

  /// Do a non-blocking hit-testing query on a shared version of the hit
  /// testing information.
  std::vector<WrHitResult> HitTest(const wr::WorldPoint& aPoint);

  void SendTransaction(TransactionBuilder& aTxn);

  void SetFrameStartTime(const TimeStamp& aTime);

  void RunOnRenderThread(UniquePtr<RendererEvent> aEvent);

  void Readback(const TimeStamp& aStartTime, gfx::IntSize aSize,
                const gfx::SurfaceFormat& aFormat,
                const Range<uint8_t>& aBuffer);

  void ClearAllCaches();
  void EnableNativeCompositor(bool aEnable);
  void EnableMultithreading(bool aEnable);
  void SetBatchingLookback(uint32_t aCount);

  void Pause();
  bool Resume();

  void WakeSceneBuilder();
  void FlushSceneBuilder();

  void NotifyMemoryPressure();
  void AccumulateMemoryReport(wr::MemoryReport*);

  wr::WrIdNamespace GetNamespace();
  wr::RenderRoot GetRenderRoot() const { return mRenderRoot; }
  uint32_t GetMaxTextureSize() const { return mMaxTextureSize; }
  bool GetUseANGLE() const { return mUseANGLE; }
  bool GetUseDComp() const { return mUseDComp; }
  bool GetUseTripleBuffering() const { return mUseTripleBuffering; }
  layers::SyncHandle GetSyncHandle() const { return mSyncHandle; }

  void Capture();

  void ToggleCaptureSequence();

  void SetTransactionLogging(bool aValue);

  void SetCompositionRecorder(
      UniquePtr<layers::WebRenderCompositionRecorder> aRecorder);

  typedef MozPromise<bool, nsresult, true> WriteCollectedFramesPromise;
  typedef MozPromise<layers::CollectedFrames, nsresult, true>
      GetCollectedFramesPromise;

  /**
   * Write the frames collected by the |WebRenderCompositionRecorder| to disk.
   *
   * If there is not currently a recorder, this is a no-op.
   */
  RefPtr<WriteCollectedFramesPromise> WriteCollectedFrames();

  /**
   * Return the frames collected by the |WebRenderCompositionRecorder| encoded
   * as data URIs.
   *
   * If there is not currently a recorder, this is a no-op and the promise will
   * be rejected.
   */
  RefPtr<GetCollectedFramesPromise> GetCollectedFrames();

 protected:
  WebRenderAPI(wr::DocumentHandle* aHandle, wr::WindowId aId,
               uint32_t aMaxTextureSize, bool aUseANGLE, bool aUseDComp,
               bool aUseTripleBuffering, layers::SyncHandle aSyncHandle,
               wr::RenderRoot aRenderRoot);

  ~WebRenderAPI();
  // Should be used only for shutdown handling
  void WaitFlushed();

  void UpdateDebugFlags(uint32_t aFlags);

  wr::DocumentHandle* mDocHandle;
  wr::WindowId mId;
  int32_t mMaxTextureSize;
  bool mUseANGLE;
  bool mUseDComp;
  bool mUseTripleBuffering;
  bool mCaptureSequence;
  layers::SyncHandle mSyncHandle;
  wr::RenderRoot mRenderRoot;

  // We maintain alive the root api to know when to shut the render backend
  // down, and the root api for the document to know when to delete the
  // document. mRootApi is null for the api object that owns the channel (and is
  // responsible for shutting it down), and mRootDocumentApi is null for the api
  // object owning (and responsible for destroying) a given document. All api
  // objects in the same window use the same channel, and some api objects write
  // to the same document (but there is only one owner for each channel and for
  // each document).
  RefPtr<wr::WebRenderAPI> mRootApi;
  RefPtr<wr::WebRenderAPI> mRootDocumentApi;

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
      : mApi(aApi), mTxn(aTxn) {}

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
            wr::TransformStyle::Flat,
            wr::WrReferenceFrameKind::Transform,
            nullptr,
            /* prim_flags = */ wr::PrimitiveFlags::IS_BACKFACE_VISIBLE,
            /* cache_tiles = */ false,
            wr::MixBlendMode::Normal,
            /* is_backdrop_root = */ false} {}

  void SetPreserve3D(bool aPreserve) {
    transform_style =
        aPreserve ? wr::TransformStyle::Preserve3D : wr::TransformStyle::Flat;
  }

  nsTArray<wr::FilterOp> mFilters;
  nsTArray<wr::WrFilterData> mFilterDatas;
  wr::LayoutRect mBounds = wr::ToLayoutRect(LayoutDeviceRect());
  const gfx::Matrix4x4* mBoundTransform = nullptr;
  const gfx::Matrix4x4* mTransformPtr = nullptr;
  Maybe<nsDisplayTransform*> mDeferredTransformItem;
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
  DisplayListBuilder(wr::PipelineId aId, const wr::LayoutSize& aContentSize,
                     size_t aCapacity = 0,
                     layers::DisplayItemCache* aCache = nullptr,
                     RenderRoot aRenderRoot = RenderRoot::Default);
  DisplayListBuilder(DisplayListBuilder&&) = default;

  ~DisplayListBuilder();

  void Save();
  void Restore();
  void ClearSave();

  usize Dump(usize aIndent, const Maybe<usize>& aStart,
             const Maybe<usize>& aEnd);
  void DumpSerializedDisplayList();

  void Finalize(wr::LayoutSize& aOutContentSizes,
                wr::BuiltDisplayList& aOutDisplayList);
  void Finalize(layers::RenderRootDisplayListData& aOutTransaction);

  RenderRoot GetRenderRoot() const { return mRenderRoot; }

  Maybe<wr::WrSpatialId> PushStackingContext(
      const StackingContextParams& aParams, const wr::LayoutRect& aBounds,
      const wr::RasterSpace& aRasterSpace);
  void PopStackingContext(bool aIsReferenceFrame);

  wr::WrClipChainId DefineClipChain(const nsTArray<wr::WrClipId>& aClips,
                                    bool aParentWithCurrentChain = false);

  wr::WrClipId DefineClip(
      const Maybe<wr::WrSpaceAndClip>& aParent, const wr::LayoutRect& aClipRect,
      const nsTArray<wr::ComplexClipRegion>* aComplex = nullptr);

  wr::WrClipId DefineImageMaskClip(const wr::ImageMask& aMask);

  wr::WrSpatialId DefineStickyFrame(const wr::LayoutRect& aContentRect,
                                    const float* aTopMargin,
                                    const float* aRightMargin,
                                    const float* aBottomMargin,
                                    const float* aLeftMargin,
                                    const StickyOffsetBounds& aVerticalBounds,
                                    const StickyOffsetBounds& aHorizontalBounds,
                                    const wr::LayoutVector2D& aAppliedOffset);

  Maybe<wr::WrSpaceAndClip> GetScrollIdForDefinedScrollLayer(
      layers::ScrollableLayerGuid::ViewID aViewId) const;
  wr::WrSpaceAndClip DefineScrollLayer(
      const layers::ScrollableLayerGuid::ViewID& aViewId,
      const Maybe<wr::WrSpaceAndClip>& aParent,
      const wr::LayoutRect& aContentRect, const wr::LayoutRect& aClipRect,
      const wr::LayoutPoint& aScrollOffset);

  void PushRect(const wr::LayoutRect& aBounds, const wr::LayoutRect& aClip,
                bool aIsBackfaceVisible, const wr::ColorF& aColor);
  void PushRectWithAnimation(const wr::LayoutRect& aBounds,
                             const wr::LayoutRect& aClip,
                             bool aIsBackfaceVisible, const wr::ColorF& aColor,
                             const WrAnimationProperty* aAnimation);
  void PushRoundedRect(const wr::LayoutRect& aBounds,
                       const wr::LayoutRect& aClip, bool aIsBackfaceVisible,
                       const wr::ColorF& aColor);
  void PushHitTest(const wr::LayoutRect& aBounds, const wr::LayoutRect& aClip,
                   bool aIsBackfaceVisible);
  void PushClearRect(const wr::LayoutRect& aBounds);
  void PushClearRectWithComplexRegion(const wr::LayoutRect& aBounds,
                                      const wr::ComplexClipRegion& aRegion);

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
                 bool aIsBackfaceVisible, wr::ImageRendering aFilter,
                 wr::ImageKey aImage, bool aPremultipliedAlpha = true,
                 const wr::ColorF& aColor = wr::ColorF{1.0f, 1.0f, 1.0f, 1.0f},
                 bool aPreferCompositorSurface = false);

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
      bool aPreferCompositorSurface = false);

  void PushNV12Image(const wr::LayoutRect& aBounds, const wr::LayoutRect& aClip,
                     bool aIsBackfaceVisible, wr::ImageKey aImageChannel0,
                     wr::ImageKey aImageChannel1, wr::WrColorDepth aColorDepth,
                     wr::WrYuvColorSpace aColorSpace,
                     wr::WrColorRange aColorRange, wr::ImageRendering aFilter,
                     bool aPreferCompositorSurface = false);

  void PushYCbCrInterleavedImage(
      const wr::LayoutRect& aBounds, const wr::LayoutRect& aClip,
      bool aIsBackfaceVisible, wr::ImageKey aImageChannel0,
      wr::WrColorDepth aColorDepth, wr::WrYuvColorSpace aColorSpace,
      wr::WrColorRange aColorRange, wr::ImageRendering aFilter,
      bool aPreferCompositorSurface = false);

  void PushIFrame(const wr::LayoutRect& aBounds, bool aIsBackfaceVisible,
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
                          wr::ExtendMode aExtendMode,
                          const wr::LayoutSideOffsets& aOutset);

  void PushBorderRadialGradient(
      const wr::LayoutRect& aBounds, const wr::LayoutRect& aClip,
      bool aIsBackfaceVisible, const wr::LayoutSideOffsets& aWidths, bool aFill,
      const wr::LayoutPoint& aCenter, const wr::LayoutSize& aRadius,
      const nsTArray<wr::GradientStop>& aStops, wr::ExtendMode aExtendMode,
      const wr::LayoutSideOffsets& aOutset);

  void PushBorderConicGradient(
      const wr::LayoutRect& aBounds, const wr::LayoutRect& aClip,
      bool aIsBackfaceVisible, const wr::LayoutSideOffsets& aWidths, bool aFill,
      const wr::LayoutPoint& aCenter, const float aAngle,
      const nsTArray<wr::GradientStop>& aStops, wr::ExtendMode aExtendMode,
      const wr::LayoutSideOffsets& aOutset);

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
  void CancelGroup();

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

  // Checks to see if the innermost enclosing fixed pos item has the same
  // ASR. If so, it returns the scroll target for that fixed-pos item.
  // Otherwise, it returns Nothing().
  Maybe<layers::ScrollableLayerGuid::ViewID> GetContainingFixedPosScrollTarget(
      const ActiveScrolledRoot* aAsr);

  Maybe<SideBits> GetContainingFixedPosSideBits(const ActiveScrolledRoot* aAsr);

  // Set the hit-test info to be used for all display items until the next call
  // to SetHitTestInfo or ClearHitTestInfo.
  void SetHitTestInfo(const layers::ScrollableLayerGuid::ViewID& aScrollId,
                      gfx::CompositorHitTestInfo aHitInfo, SideBits aSideBits);
  // Clears the hit-test info so that subsequent display items will not have it.
  void ClearHitTestInfo();

  already_AddRefed<gfxContext> GetTextContext(
      wr::IpcResourceUpdateQueue& aResources,
      const layers::StackingContextHelper& aSc,
      layers::RenderRootStateManager* aManager, nsDisplayItem* aItem,
      nsRect& aBounds, const gfx::Point& aDeviceOffset);

  // Try to avoid using this when possible.
  wr::WrState* Raw() { return mWrState; }

  void SetClipChainLeaf(const Maybe<wr::LayoutRect>& aClipRect) {
    mClipChainLeaf = aClipRect;
  }

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
  std::unordered_map<layers::ScrollableLayerGuid::ViewID, wr::WrSpaceAndClip>
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
  RefPtr<gfxContext> mCachedContext;

  FixedPosScrollTargetTracker* mActiveFixedPosTracker;

  wr::PipelineId mPipelineId;
  wr::LayoutSize mContentSize;

  nsTArray<wr::PipelineId> mRemotePipelineIds;
  RenderRoot mRenderRoot;

  layers::DisplayItemCache* mDisplayItemCache;
  Maybe<uint16_t> mCurrentCacheSlot;

  friend class WebRenderAPI;
  friend class SpaceAndClipChainHelper;
};

// This is a RAII class that overrides the current Wr's SpatialId and
// ClipChainId.
class MOZ_RAII SpaceAndClipChainHelper final {
 public:
  SpaceAndClipChainHelper(DisplayListBuilder& aBuilder,
                          wr::WrSpaceAndClipChain aSpaceAndClipChain
                              MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : mBuilder(aBuilder),
        mOldSpaceAndClipChain(aBuilder.mCurrentSpaceAndClipChain) {
    aBuilder.mCurrentSpaceAndClipChain = aSpaceAndClipChain;
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
  }
  SpaceAndClipChainHelper(DisplayListBuilder& aBuilder,
                          wr::WrSpatialId aSpatialId
                              MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : mBuilder(aBuilder),
        mOldSpaceAndClipChain(aBuilder.mCurrentSpaceAndClipChain) {
    aBuilder.mCurrentSpaceAndClipChain.space = aSpatialId;
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
  }
  SpaceAndClipChainHelper(DisplayListBuilder& aBuilder,
                          wr::WrClipChainId aClipChainId
                              MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : mBuilder(aBuilder),
        mOldSpaceAndClipChain(aBuilder.mCurrentSpaceAndClipChain) {
    aBuilder.mCurrentSpaceAndClipChain.clip_chain = aClipChainId.id;
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
  }

  ~SpaceAndClipChainHelper() {
    mBuilder.mCurrentSpaceAndClipChain = mOldSpaceAndClipChain;
  }

 private:
  SpaceAndClipChainHelper(const SpaceAndClipChainHelper&) = delete;

  DisplayListBuilder& mBuilder;
  wr::WrSpaceAndClipChain mOldSpaceAndClipChain;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

Maybe<wr::ImageFormat> SurfaceFormatToImageFormat(gfx::SurfaceFormat aFormat);

}  // namespace wr
}  // namespace mozilla

#endif
