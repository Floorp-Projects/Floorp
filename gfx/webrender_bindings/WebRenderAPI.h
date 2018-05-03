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
#include "mozilla/layers/SyncObject.h"
#include "mozilla/Range.h"
#include "mozilla/webrender/webrender_ffi.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "FrameMetrics.h"
#include "GLTypes.h"
#include "Units.h"

namespace mozilla {

struct DisplayItemClipChain;

namespace widget {
class CompositorWidget;
}

namespace layers {
class CompositorBridgeParent;
class WebRenderBridgeParent;
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


class TransactionBuilder {
public:
  TransactionBuilder();

  ~TransactionBuilder();

  void UpdateEpoch(PipelineId aPipelineId, Epoch aEpoch);

  void SetRootPipeline(PipelineId aPipelineId);

  void RemovePipeline(PipelineId aPipelineId);

  void SetDisplayList(gfx::Color aBgColor,
                      Epoch aEpoch,
                      mozilla::LayerSize aViewportSize,
                      wr::WrPipelineId pipeline_id,
                      const wr::LayoutSize& content_size,
                      wr::BuiltDisplayListDescriptor dl_descriptor,
                      wr::Vec<uint8_t>& dl_data);

  void ClearDisplayList(Epoch aEpoch, wr::WrPipelineId aPipeline);

  void GenerateFrame();

  void UpdateDynamicProperties(const nsTArray<wr::WrOpacityProperty>& aOpacityArray,
                               const nsTArray<wr::WrTransformProperty>& aTransformArray);
  void AppendTransformProperties(const nsTArray<wr::WrTransformProperty>& aTransformArray);

  void SetWindowParameters(const LayoutDeviceIntSize& aWindowSize,
                           const LayoutDeviceIntRect& aDocRect);

  void UpdateScrollPosition(const wr::WrPipelineId& aPipelineId,
                            const layers::FrameMetrics::ViewID& aScrollId,
                            const wr::LayoutPoint& aScrollPosition);

  bool IsEmpty() const;

  void AddImage(wr::ImageKey aKey,
                const ImageDescriptor& aDescriptor,
                wr::Vec<uint8_t>& aBytes);

  void AddBlobImage(wr::ImageKey aKey,
                    const ImageDescriptor& aDescriptor,
                    wr::Vec<uint8_t>& aBytes);

  void AddExternalImageBuffer(ImageKey key,
                              const ImageDescriptor& aDescriptor,
                              ExternalImageId aHandle);

  void AddExternalImage(ImageKey key,
                        const ImageDescriptor& aDescriptor,
                        ExternalImageId aExtID,
                        wr::WrExternalImageBufferType aBufferType,
                        uint8_t aChannelIndex = 0);

  void UpdateImageBuffer(wr::ImageKey aKey,
                         const ImageDescriptor& aDescriptor,
                         wr::Vec<uint8_t>& aBytes);

  void UpdateBlobImage(wr::ImageKey aKey,
                       const ImageDescriptor& aDescriptor,
                       wr::Vec<uint8_t>& aBytes,
                       const wr::DeviceUintRect& aDirtyRect);

  void UpdateExternalImage(ImageKey aKey,
                           const ImageDescriptor& aDescriptor,
                           ExternalImageId aExtID,
                           wr::WrExternalImageBufferType aBufferType,
                           uint8_t aChannelIndex = 0);

  void DeleteImage(wr::ImageKey aKey);

  void AddRawFont(wr::FontKey aKey, wr::Vec<uint8_t>& aBytes, uint32_t aIndex);

  void AddFontDescriptor(wr::FontKey aKey, wr::Vec<uint8_t>& aBytes, uint32_t aIndex);

  void DeleteFont(wr::FontKey aKey);

  void AddFontInstance(wr::FontInstanceKey aKey,
                       wr::FontKey aFontKey,
                       float aGlyphSize,
                       const wr::FontInstanceOptions* aOptions,
                       const wr::FontInstancePlatformOptions* aPlatformOptions,
                       wr::Vec<uint8_t>& aVariations);

  void DeleteFontInstance(wr::FontInstanceKey aKey);

  void Clear();

  Transaction* Raw() { return mTxn; }
  wr::ResourceUpdates* RawUpdates() { return mResourceUpdates; }
protected:
  Transaction* mTxn;
  wr::ResourceUpdates* mResourceUpdates;
};

class TransactionWrapper
{
public:
  explicit TransactionWrapper(Transaction* aTxn);

  void AppendTransformProperties(const nsTArray<wr::WrTransformProperty>& aTransformArray);
  void UpdateScrollPosition(const wr::WrPipelineId& aPipelineId,
                            const layers::FrameMetrics::ViewID& aScrollId,
                            const wr::LayoutPoint& aScrollPosition);
private:
  Transaction* mTxn;
};

class WebRenderAPI
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WebRenderAPI);

public:
  /// This can be called on the compositor thread only.
  static already_AddRefed<WebRenderAPI> Create(layers::CompositorBridgeParent* aBridge,
                                               RefPtr<widget::CompositorWidget>&& aWidget,
                                               const wr::WrWindowId& aWindowId,
                                               LayoutDeviceIntSize aSize);

  already_AddRefed<WebRenderAPI> CreateDocument(LayoutDeviceIntSize aSize, int8_t aLayerIndex);

  // Redirect the WR's log to gfxCriticalError/Note.
  static void InitRustLogForGpuProcess();
  static void ShutdownRustLogForGpuProcess();

  already_AddRefed<WebRenderAPI> Clone();

  wr::WindowId GetId() const { return mId; }

  bool HitTest(const wr::WorldPoint& aPoint,
               wr::WrPipelineId& aOutPipelineId,
               layers::FrameMetrics::ViewID& aOutScrollId,
               gfx::CompositorHitTestInfo& aOutHitInfo);

  void SendTransaction(TransactionBuilder& aTxn);

  void SetFrameStartTime(const TimeStamp& aTime);

  void RunOnRenderThread(UniquePtr<RendererEvent> aEvent);

  void Readback(gfx::IntSize aSize, uint8_t *aBuffer, uint32_t aBufferSize);

  void Pause();
  bool Resume();

  void WakeSceneBuilder();
  void FlushSceneBuilder();

  wr::WrIdNamespace GetNamespace();
  uint32_t GetMaxTextureSize() const { return mMaxTextureSize; }
  bool GetUseANGLE() const { return mUseANGLE; }
  layers::SyncHandle GetSyncHandle() const { return mSyncHandle; }

  void Capture();

protected:
  WebRenderAPI(wr::DocumentHandle* aHandle, wr::WindowId aId, uint32_t aMaxTextureSize, bool aUseANGLE, layers::SyncHandle aSyncHandle)
    : mDocHandle(aHandle)
    , mId(aId)
    , mMaxTextureSize(aMaxTextureSize)
    , mUseANGLE(aUseANGLE)
    , mSyncHandle(aSyncHandle)
  {}

  ~WebRenderAPI();
  // Should be used only for shutdown handling
  void WaitFlushed();

  wr::DocumentHandle* mDocHandle;
  wr::WindowId mId;
  uint32_t mMaxTextureSize;
  bool mUseANGLE;
  layers::SyncHandle mSyncHandle;

  // We maintain alive the root api to know when to shut the render backend down,
  // and the root api for the document to know when to delete the document.
  // mRootApi is null for the api object that owns the channel (and is responsible
  // for shutting it down), and mRootDocumentApi is null for the api object owning
  // (and responsible for destroying) a given document.
  // All api objects in the same window use the same channel, and some api objects
  // write to the same document (but there is only one owner for each channel and
  // for each document).
  RefPtr<wr::WebRenderAPI> mRootApi;
  RefPtr<wr::WebRenderAPI> mRootDocumentApi;

  friend class DisplayListBuilder;
  friend class layers::WebRenderBridgeParent;
};

/// This is a simple C++ wrapper around WrState defined in the rust bindings.
/// We may want to turn this into a direct wrapper on top of WebRenderFrameBuilder
/// instead, so the interface may change a bit.
class DisplayListBuilder {
public:
  explicit DisplayListBuilder(wr::PipelineId aId,
                              const wr::LayoutSize& aContentSize,
                              size_t aCapacity = 0);
  DisplayListBuilder(DisplayListBuilder&&) = default;

  ~DisplayListBuilder();

  void Save();
  void Restore();
  void ClearSave();
  void Dump();

  void Finalize(wr::LayoutSize& aOutContentSize,
                wr::BuiltDisplayList& aOutDisplayList);

  void PushStackingContext(const wr::LayoutRect& aBounds, // TODO: We should work with strongly typed rects
                           const wr::WrClipId* aClipNodeId,
                           const wr::WrAnimationProperty* aAnimation,
                           const float* aOpacity,
                           const gfx::Matrix4x4* aTransform,
                           wr::TransformStyle aTransformStyle,
                           const gfx::Matrix4x4* aPerspective,
                           const wr::MixBlendMode& aMixBlendMode,
                           const nsTArray<wr::WrFilterOp>& aFilters,
                           bool aIsBackfaceVisible);
  void PopStackingContext();

  wr::WrClipId DefineClip(const Maybe<wr::WrScrollId>& aAncestorScrollId,
                          const Maybe<wr::WrClipId>& aAncestorClipId,
                          const wr::LayoutRect& aClipRect,
                          const nsTArray<wr::ComplexClipRegion>* aComplex = nullptr,
                          const wr::WrImageMask* aMask = nullptr);
  void PushClip(const wr::WrClipId& aClipId, const DisplayItemClipChain* aParent = nullptr);
  void PopClip(const DisplayItemClipChain* aParent = nullptr);
  Maybe<wr::WrClipId> GetCacheOverride(const DisplayItemClipChain* aParent);

  wr::WrStickyId DefineStickyFrame(const wr::LayoutRect& aContentRect,
                                   const float* aTopMargin,
                                   const float* aRightMargin,
                                   const float* aBottomMargin,
                                   const float* aLeftMargin,
                                   const StickyOffsetBounds& aVerticalBounds,
                                   const StickyOffsetBounds& aHorizontalBounds,
                                   const wr::LayoutVector2D& aAppliedOffset);
  void PushStickyFrame(const wr::WrStickyId& aStickyId,
                       const DisplayItemClipChain* aParent);
  void PopStickyFrame(const DisplayItemClipChain* aParent);

  Maybe<wr::WrScrollId> GetScrollIdForDefinedScrollLayer(layers::FrameMetrics::ViewID aViewId) const;
  wr::WrScrollId DefineScrollLayer(const layers::FrameMetrics::ViewID& aViewId,
                                   const Maybe<wr::WrScrollId>& aAncestorScrollId,
                                   const Maybe<wr::WrClipId>& aAncestorClipId,
                                   const wr::LayoutRect& aContentRect, // TODO: We should work with strongly typed rects
                                   const wr::LayoutRect& aClipRect);
  void PushScrollLayer(const wr::WrScrollId& aScrollId);
  void PopScrollLayer();

  void PushClipAndScrollInfo(const wr::WrScrollId& aScrollId,
                             const wr::WrClipId* aClipId);
  void PopClipAndScrollInfo();

  void PushRect(const wr::LayoutRect& aBounds,
                const wr::LayoutRect& aClip,
                bool aIsBackfaceVisible,
                const wr::ColorF& aColor);

  void PushClearRect(const wr::LayoutRect& aBounds);

  void PushLinearGradient(const wr::LayoutRect& aBounds,
                          const wr::LayoutRect& aClip,
                          bool aIsBackfaceVisible,
                          const wr::LayoutPoint& aStartPoint,
                          const wr::LayoutPoint& aEndPoint,
                          const nsTArray<wr::GradientStop>& aStops,
                          wr::ExtendMode aExtendMode,
                          const wr::LayoutSize aTileSize,
                          const wr::LayoutSize aTileSpacing);

  void PushRadialGradient(const wr::LayoutRect& aBounds,
                          const wr::LayoutRect& aClip,
                          bool aIsBackfaceVisible,
                          const wr::LayoutPoint& aCenter,
                          const wr::LayoutSize& aRadius,
                          const nsTArray<wr::GradientStop>& aStops,
                          wr::ExtendMode aExtendMode,
                          const wr::LayoutSize aTileSize,
                          const wr::LayoutSize aTileSpacing);

  void PushImage(const wr::LayoutRect& aBounds,
                 const wr::LayoutRect& aClip,
                 bool aIsBackfaceVisible,
                 wr::ImageRendering aFilter,
                 wr::ImageKey aImage,
                 bool aPremultipliedAlpha = true);

  void PushImage(const wr::LayoutRect& aBounds,
                 const wr::LayoutRect& aClip,
                 bool aIsBackfaceVisible,
                 const wr::LayoutSize& aStretchSize,
                 const wr::LayoutSize& aTileSpacing,
                 wr::ImageRendering aFilter,
                 wr::ImageKey aImage,
                 bool aPremultipliedAlpha = true);

  void PushYCbCrPlanarImage(const wr::LayoutRect& aBounds,
                            const wr::LayoutRect& aClip,
                            bool aIsBackfaceVisible,
                            wr::ImageKey aImageChannel0,
                            wr::ImageKey aImageChannel1,
                            wr::ImageKey aImageChannel2,
                            wr::WrYuvColorSpace aColorSpace,
                            wr::ImageRendering aFilter);

  void PushNV12Image(const wr::LayoutRect& aBounds,
                     const wr::LayoutRect& aClip,
                     bool aIsBackfaceVisible,
                     wr::ImageKey aImageChannel0,
                     wr::ImageKey aImageChannel1,
                     wr::WrYuvColorSpace aColorSpace,
                     wr::ImageRendering aFilter);

  void PushYCbCrInterleavedImage(const wr::LayoutRect& aBounds,
                                 const wr::LayoutRect& aClip,
                                 bool aIsBackfaceVisible,
                                 wr::ImageKey aImageChannel0,
                                 wr::WrYuvColorSpace aColorSpace,
                                 wr::ImageRendering aFilter);

  void PushIFrame(const wr::LayoutRect& aBounds,
                  bool aIsBackfaceVisible,
                  wr::PipelineId aPipeline);

  // XXX WrBorderSides are passed with Range.
  // It is just to bypass compiler bug. See Bug 1357734.
  void PushBorder(const wr::LayoutRect& aBounds,
                  const wr::LayoutRect& aClip,
                  bool aIsBackfaceVisible,
                  const wr::BorderWidths& aWidths,
                  const Range<const wr::BorderSide>& aSides,
                  const wr::BorderRadius& aRadius);

  void PushBorderImage(const wr::LayoutRect& aBounds,
                       const wr::LayoutRect& aClip,
                       bool aIsBackfaceVisible,
                       const wr::BorderWidths& aWidths,
                       wr::ImageKey aImage,
                       const wr::NinePatchDescriptor& aPatch,
                       const wr::SideOffsets2D<float>& aOutset,
                       const wr::RepeatMode& aRepeatHorizontal,
                       const wr::RepeatMode& aRepeatVertical);

  void PushBorderGradient(const wr::LayoutRect& aBounds,
                          const wr::LayoutRect& aClip,
                          bool aIsBackfaceVisible,
                          const wr::BorderWidths& aWidths,
                          const wr::LayoutPoint& aStartPoint,
                          const wr::LayoutPoint& aEndPoint,
                          const nsTArray<wr::GradientStop>& aStops,
                          wr::ExtendMode aExtendMode,
                          const wr::SideOffsets2D<float>& aOutset);

  void PushBorderRadialGradient(const wr::LayoutRect& aBounds,
                                const wr::LayoutRect& aClip,
                                bool aIsBackfaceVisible,
                                const wr::BorderWidths& aWidths,
                                const wr::LayoutPoint& aCenter,
                                const wr::LayoutSize& aRadius,
                                const nsTArray<wr::GradientStop>& aStops,
                                wr::ExtendMode aExtendMode,
                                const wr::SideOffsets2D<float>& aOutset);

  void PushText(const wr::LayoutRect& aBounds,
                const wr::LayoutRect& aClip,
                bool aIsBackfaceVisible,
                const wr::ColorF& aColor,
                wr::FontInstanceKey aFontKey,
                Range<const wr::GlyphInstance> aGlyphBuffer,
                const wr::GlyphOptions* aGlyphOptions = nullptr);

  void PushLine(const wr::LayoutRect& aClip,
                bool aIsBackfaceVisible,
                const wr::Line& aLine);

  void PushShadow(const wr::LayoutRect& aBounds,
                      const wr::LayoutRect& aClip,
                      bool aIsBackfaceVisible,
                      const wr::Shadow& aShadow);

  void PopAllShadows();



  void PushBoxShadow(const wr::LayoutRect& aRect,
                     const wr::LayoutRect& aClip,
                     bool aIsBackfaceVisible,
                     const wr::LayoutRect& aBoxBounds,
                     const wr::LayoutVector2D& aOffset,
                     const wr::ColorF& aColor,
                     const float& aBlurRadius,
                     const float& aSpreadRadius,
                     const wr::BorderRadius& aBorderRadius,
                     const wr::BoxShadowClipMode& aClipMode);

  // Returns the clip id that was most recently pushed with PushClip and that
  // has not yet been popped with PopClip. Return Nothing() if the clip stack
  // is empty.
  Maybe<wr::WrClipId> TopmostClipId();
  // Same as TopmostClipId() but for scroll layers.
  wr::WrScrollId TopmostScrollId();
  // If the topmost item on the stack is a clip or a scroll layer
  bool TopmostIsClip();

  // Set the hit-test info to be used for all display items until the next call
  // to SetHitTestInfo or ClearHitTestInfo.
  void SetHitTestInfo(const layers::FrameMetrics::ViewID& aScrollId,
                      gfx::CompositorHitTestInfo aHitInfo);
  // Clears the hit-test info so that subsequent display items will not have it.
  void ClearHitTestInfo();

  // Try to avoid using this when possible.
  wr::WrState* Raw() { return mWrState; }

  // Return true if the current clip stack has any extra clip.
  bool HasExtraClip() { return !mCacheOverride.empty(); }

protected:
  void PushCacheOverride(const DisplayItemClipChain* aParent,
                         const wr::WrClipId& aClipId);
  void PopCacheOverride(const DisplayItemClipChain* aParent);

  wr::WrState* mWrState;

  // Track the stack of clip ids and scroll layer ids that have been pushed
  // (by PushClip and PushScrollLayer/PushClipAndScrollInfo, respectively) and
  // haven't yet been popped.
  std::vector<wr::ScrollOrClipId> mClipStack;

  // Track each scroll id that we encountered. We use this structure to
  // ensure that we don't define a particular scroll layer multiple times,
  // as that results in undefined behaviour in WR.
  std::unordered_map<layers::FrameMetrics::ViewID, wr::WrScrollId> mScrollIds;

  // A map that holds the cache overrides creates by "out of band" clips, i.e.
  // clips that are generated by display items but that ScrollingLayersHelper
  // doesn't know about. These are called "cache overrides" because while we're
  // inside one of these clips, the WR clip stack is different from what
  // ScrollingLayersHelper thinks it actually is (because of the out-of-band
  // clip that was pushed onto the stack) and so ScrollingLayersHelper cannot
  // use its clip cache as-is. Instead, any time ScrollingLayersHelper wants
  // to define a new clip as a child of clip X, it should first check the
  // cache overrides to see if there is an out-of-band clip Y that is already a
  // child of X, and then define its clip as a child of Y instead. This map
  // stores X -> ClipId of Y, which allows ScrollingLayersHelper to do the
  // necessary lookup. Note that there theoretically might be multiple
  // different "Y" clips which is why we need a vector.
  std::unordered_map<const DisplayItemClipChain*, std::vector<wr::WrClipId>> mCacheOverride;

  friend class WebRenderAPI;
};

Maybe<wr::ImageFormat>
SurfaceFormatToImageFormat(gfx::SurfaceFormat aFormat);

} // namespace wr
} // namespace mozilla

#endif
