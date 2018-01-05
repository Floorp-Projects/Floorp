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
class CompositorBridgeParentBase;
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

/// Updates to retained resources such as images and fonts, applied within the
/// same transaction.
class ResourceUpdateQueue {
public:
  ResourceUpdateQueue();
  ~ResourceUpdateQueue();
  ResourceUpdateQueue(ResourceUpdateQueue&&);
  ResourceUpdateQueue(const ResourceUpdateQueue&) = delete;
  ResourceUpdateQueue& operator=(ResourceUpdateQueue&&);
  ResourceUpdateQueue& operator=(const ResourceUpdateQueue&) = delete;

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
                        WrExternalImageBufferType aBufferType,
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

  // Try to avoid using this when possible.
  wr::ResourceUpdates* Raw() { return mUpdates; }

protected:
  explicit ResourceUpdateQueue(wr::ResourceUpdates* aUpdates)
  : mUpdates(aUpdates) {}

  wr::ResourceUpdates* mUpdates;
};

class WebRenderAPI
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WebRenderAPI);

public:
  /// This can be called on the compositor thread only.
  static already_AddRefed<WebRenderAPI> Create(layers::CompositorBridgeParentBase* aBridge,
                                               RefPtr<widget::CompositorWidget>&& aWidget,
                                               LayoutDeviceIntSize aSize);

  // Redirect the WR's log to gfxCriticalError/Note.
  static void InitExternalLogHandler();
  static void ShutdownExternalLogHandler();

  already_AddRefed<WebRenderAPI> Clone();

  wr::WindowId GetId() const { return mId; }

  void UpdateScrollPosition(const wr::WrPipelineId& aPipelineId,
                            const layers::FrameMetrics::ViewID& aScrollId,
                            const wr::LayoutPoint& aScrollPosition);
  bool HitTest(const wr::WorldPoint& aPoint,
               wr::WrPipelineId& aOutPipelineId,
               layers::FrameMetrics::ViewID& aOutScrollId,
               gfx::CompositorHitTestInfo& aOutHitInfo);

  void GenerateFrame();
  void GenerateFrame(const nsTArray<wr::WrOpacityProperty>& aOpacityArray,
                     const nsTArray<wr::WrTransformProperty>& aTransformArray);

  void SetWindowParameters(LayoutDeviceIntSize size);

  void SetDisplayList(gfx::Color aBgColor,
                      Epoch aEpoch,
                      mozilla::LayerSize aViewportSize,
                      wr::WrPipelineId pipeline_id,
                      const wr::LayoutSize& content_size,
                      wr::BuiltDisplayListDescriptor dl_descriptor,
                      wr::Vec<uint8_t>& dl_data,
                      ResourceUpdateQueue& aResources);

  void ClearDisplayList(Epoch aEpoch, wr::WrPipelineId pipeline_id);

  void SetRootPipeline(wr::PipelineId aPipeline);

  void RemovePipeline(wr::PipelineId aPipeline);

  void UpdateResources(ResourceUpdateQueue& aUpdates);

  void UpdatePipelineResources(ResourceUpdateQueue& aUpdates, PipelineId aPipeline, Epoch aEpoch);

  void SetFrameStartTime(const TimeStamp& aTime);

  void RunOnRenderThread(UniquePtr<RendererEvent> aEvent);

  void Readback(gfx::IntSize aSize, uint8_t *aBuffer, uint32_t aBufferSize);

  void Pause();
  bool Resume();

  wr::WrIdNamespace GetNamespace();
  uint32_t GetMaxTextureSize() const { return mMaxTextureSize; }
  bool GetUseANGLE() const { return mUseANGLE; }
  layers::SyncHandle GetSyncHandle() const { return mSyncHandle; }

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
  RefPtr<wr::WebRenderAPI> mRootApi;

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
                           const wr::WrAnimationProperty* aAnimation,
                           const float* aOpacity,
                           const gfx::Matrix4x4* aTransform,
                           wr::TransformStyle aTransformStyle,
                           const gfx::Matrix4x4* aPerspective,
                           const wr::MixBlendMode& aMixBlendMode,
                           const nsTArray<wr::WrFilterOp>& aFilters,
                           bool aIsBackfaceVisible);
  void PopStackingContext();

  wr::WrClipId DefineClip(const Maybe<layers::FrameMetrics::ViewID>& aAncestorScrollId,
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

  bool IsScrollLayerDefined(layers::FrameMetrics::ViewID aScrollId) const;
  void DefineScrollLayer(const layers::FrameMetrics::ViewID& aScrollId,
                         const Maybe<layers::FrameMetrics::ViewID>& aAncestorScrollId,
                         const Maybe<wr::WrClipId>& aAncestorClipId,
                         const wr::LayoutRect& aContentRect, // TODO: We should work with strongly typed rects
                         const wr::LayoutRect& aClipRect);
  void PushScrollLayer(const layers::FrameMetrics::ViewID& aScrollId);
  void PopScrollLayer();

  void PushClipAndScrollInfo(const layers::FrameMetrics::ViewID& aScrollId,
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
                 wr::ImageKey aImage);

  void PushImage(const wr::LayoutRect& aBounds,
                 const wr::LayoutRect& aClip,
                 bool aIsBackfaceVisible,
                 const wr::LayoutSize& aStretchSize,
                 const wr::LayoutSize& aTileSpacing,
                 wr::ImageRendering aFilter,
                 wr::ImageKey aImage);

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
  layers::FrameMetrics::ViewID TopmostScrollId();
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
  std::unordered_set<layers::FrameMetrics::ViewID> mScrollIdsDefined;

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
