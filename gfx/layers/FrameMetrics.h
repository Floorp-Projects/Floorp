/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_FRAMEMETRICS_H
#define GFX_FRAMEMETRICS_H

#include <stdint.h>                     // for uint32_t, uint64_t
#include "Units.h"                      // for CSSRect, CSSPixel, etc
#include "mozilla/HashFunctions.h"      // for HashGeneric
#include "mozilla/Maybe.h"
#include "mozilla/gfx/BasePoint.h"      // for BasePoint
#include "mozilla/gfx/Rect.h"           // for RoundedIn
#include "mozilla/gfx/ScaleFactor.h"    // for ScaleFactor
#include "mozilla/gfx/Logging.h"        // for Log
#include "mozilla/StaticPtr.h"          // for StaticAutoPtr
#include "mozilla/TimeStamp.h"          // for TimeStamp
#include "nsString.h"
#include "nsStyleCoord.h"               // for nsStyleCoord

namespace IPC {
template <typename T> struct ParamTraits;
} // namespace IPC

namespace mozilla {
namespace layers {

/**
 * Helper struct to hold a couple of fields that can be updated as part of
 * an empty transaction.
 */
struct ScrollUpdateInfo {
  uint32_t mScrollGeneration;
  CSSPoint mScrollOffset;
};

/**
 * The viewport and displayport metrics for the painted frame at the
 * time of a layer-tree transaction.  These metrics are especially
 * useful for shadow layers, because the metrics values are updated
 * atomically with new pixels.
 */
struct FrameMetrics {
  friend struct IPC::ParamTraits<mozilla::layers::FrameMetrics>;
public:
  // We use IDs to identify frames across processes.
  typedef uint64_t ViewID;
  static const ViewID NULL_SCROLL_ID;   // This container layer does not scroll.
  static const ViewID START_SCROLL_ID = 2;  // This is the ID that scrolling subframes
                                        // will begin at.

  enum ScrollOffsetUpdateType : uint8_t {
    eNone,          // The default; the scroll offset was not updated
    eMainThread,    // The scroll offset was updated by the main thread.
    ePending,       // The scroll offset was updated on the main thread, but not
                    // painted, so the layer texture data is still at the old
                    // offset.
    eUserAction,    // In an APZ repaint request, this means the APZ generated
                    // the scroll position based on user action (the alternative
                    // is eNone which means it's just request a repaint because
                    // it got a scroll update from the main thread).
    eRestore,       // The scroll offset was updated by the main thread, but as
                    // a restore from history or after a frame reconstruction.
                    // In this case, APZ can ignore the offset change if the
                    // user has done an APZ scroll already.

    eSentinel       // For IPC use only
  };

  FrameMetrics()
    : mScrollId(NULL_SCROLL_ID)
    , mPresShellResolution(1)
    , mCompositionBounds(0, 0, 0, 0)
    , mDisplayPort(0, 0, 0, 0)
    , mCriticalDisplayPort(0, 0, 0, 0)
    , mScrollableRect(0, 0, 0, 0)
    , mCumulativeResolution()
    , mDevPixelsPerCSSPixel(1)
    , mScrollOffset(0, 0)
    , mZoom()
    , mScrollGeneration(0)
    , mSmoothScrollOffset(0, 0)
    , mRootCompositionSize(0, 0)
    , mDisplayPortMargins(0, 0, 0, 0)
    , mPresShellId(-1)
    , mViewport(0, 0, 0, 0)
    , mExtraResolution()
    , mPaintRequestTime()
    , mScrollUpdateType(eNone)
    , mIsRootContent(false)
    , mDoSmoothScroll(false)
    , mUseDisplayPortMargins(false)
    , mIsScrollInfoLayer(false)
  {
  }

  // Default copy ctor and operator= are fine

  bool operator==(const FrameMetrics& aOther) const
  {
    // Put mScrollId at the top since it's the most likely one to fail.
    return mScrollId == aOther.mScrollId &&
           mPresShellResolution == aOther.mPresShellResolution &&
           mCompositionBounds.IsEqualEdges(aOther.mCompositionBounds) &&
           mDisplayPort.IsEqualEdges(aOther.mDisplayPort) &&
           mCriticalDisplayPort.IsEqualEdges(aOther.mCriticalDisplayPort) &&
           mScrollableRect.IsEqualEdges(aOther.mScrollableRect) &&
           mCumulativeResolution == aOther.mCumulativeResolution &&
           mDevPixelsPerCSSPixel == aOther.mDevPixelsPerCSSPixel &&
           mScrollOffset == aOther.mScrollOffset &&
           // don't compare mZoom
           mScrollGeneration == aOther.mScrollGeneration &&
           mSmoothScrollOffset == aOther.mSmoothScrollOffset &&
           mRootCompositionSize == aOther.mRootCompositionSize &&
           mDisplayPortMargins == aOther.mDisplayPortMargins &&
           mPresShellId == aOther.mPresShellId &&
           mViewport.IsEqualEdges(aOther.mViewport) &&
           mExtraResolution == aOther.mExtraResolution &&
           mPaintRequestTime == aOther.mPaintRequestTime &&
           mScrollUpdateType == aOther.mScrollUpdateType &&
           mIsRootContent == aOther.mIsRootContent &&
           mDoSmoothScroll == aOther.mDoSmoothScroll &&
           mUseDisplayPortMargins == aOther.mUseDisplayPortMargins &&
           mIsScrollInfoLayer == aOther.mIsScrollInfoLayer;
  }

  bool operator!=(const FrameMetrics& aOther) const
  {
    return !operator==(aOther);
  }

  bool IsScrollable() const
  {
    return mScrollId != NULL_SCROLL_ID;
  }

  CSSToScreenScale2D DisplayportPixelsPerCSSPixel() const
  {
    // Note: use 'mZoom * ParentLayerToLayerScale(1.0f)' as the CSS-to-Layer scale
    // instead of LayersPixelsPerCSSPixel(), because displayport calculations
    // are done in the context of a repaint request, where we ask Layout to
    // repaint at a new resolution that includes any async zoom. Until this
    // repaint request is processed, LayersPixelsPerCSSPixel() does not yet
    // include the async zoom, but it will when the displayport is interpreted
    // for the repaint.
    return mZoom * ParentLayerToLayerScale(1.0f) / mExtraResolution;
  }

  CSSToLayerScale2D LayersPixelsPerCSSPixel() const
  {
    return mDevPixelsPerCSSPixel * mCumulativeResolution;
  }

  // Get the amount by which this frame has been zoomed since the last repaint.
  LayerToParentLayerScale GetAsyncZoom() const
  {
    // The async portion of the zoom should be the same along the x and y
    // axes.
    return (mZoom / LayersPixelsPerCSSPixel()).ToScaleFactor();
  }

  // Ensure the scrollableRect is at least as big as the compositionBounds
  // because the scrollableRect can be smaller if the content is not large
  // and the scrollableRect hasn't been updated yet.
  // We move the scrollableRect up because we don't know if we can move it
  // down. i.e. we know that scrollableRect can go back as far as zero.
  // but we don't know how much further ahead it can go.
  CSSRect GetExpandedScrollableRect() const
  {
    CSSRect scrollableRect = mScrollableRect;
    CSSSize compSize = CalculateCompositedSizeInCssPixels();
    if (scrollableRect.width < compSize.width) {
      scrollableRect.x = std::max(0.f,
                                  scrollableRect.x - (compSize.width - scrollableRect.width));
      scrollableRect.width = compSize.width;
    }

    if (scrollableRect.height < compSize.height) {
      scrollableRect.y = std::max(0.f,
                                  scrollableRect.y - (compSize.height - scrollableRect.height));
      scrollableRect.height = compSize.height;
    }

    return scrollableRect;
  }

  CSSSize CalculateCompositedSizeInCssPixels() const
  {
    if (GetZoom() == CSSToParentLayerScale2D(0, 0)) {
      return CSSSize();  // avoid division by zero
    }
    return mCompositionBounds.Size() / GetZoom();
  }

  CSSRect CalculateCompositedRectInCssPixels() const
  {
    if (GetZoom() == CSSToParentLayerScale2D(0, 0)) {
      return CSSRect();  // avoid division by zero
    }
    return mCompositionBounds / GetZoom();
  }

  CSSSize CalculateBoundedCompositedSizeInCssPixels() const
  {
    CSSSize size = CalculateCompositedSizeInCssPixels();
    size.width = std::min(size.width, mRootCompositionSize.width);
    size.height = std::min(size.height, mRootCompositionSize.height);
    return size;
  }

  CSSRect CalculateScrollRange() const
  {
    CSSSize scrollPortSize = CalculateCompositedSizeInCssPixels();
    CSSRect scrollRange = mScrollableRect;
    scrollRange.width = std::max(scrollRange.width - scrollPortSize.width, 0.0f);
    scrollRange.height = std::max(scrollRange.height - scrollPortSize.height, 0.0f);
    return scrollRange;
  }

  void ScrollBy(const CSSPoint& aPoint)
  {
    mScrollOffset += aPoint;
  }

  void ZoomBy(float aScale)
  {
    ZoomBy(gfxSize(aScale, aScale));
  }

  void ZoomBy(const gfxSize& aScale)
  {
    mZoom.xScale *= aScale.width;
    mZoom.yScale *= aScale.height;
  }

  void CopyScrollInfoFrom(const FrameMetrics& aOther)
  {
    mScrollOffset = aOther.mScrollOffset;
    mScrollGeneration = aOther.mScrollGeneration;
  }

  void CopySmoothScrollInfoFrom(const FrameMetrics& aOther)
  {
    mSmoothScrollOffset = aOther.mSmoothScrollOffset;
    mScrollGeneration = aOther.mScrollGeneration;
    mDoSmoothScroll = aOther.mDoSmoothScroll;
  }

  void UpdatePendingScrollInfo(const ScrollUpdateInfo& aInfo)
  {
    mScrollOffset = aInfo.mScrollOffset;
    mScrollGeneration = aInfo.mScrollGeneration;
    mScrollUpdateType = ePending;
  }

  void SetRepaintDrivenByUserAction(bool aUserAction)
  {
    mScrollUpdateType = aUserAction ? eUserAction : eNone;
  }

public:
  void SetPresShellResolution(float aPresShellResolution)
  {
    mPresShellResolution = aPresShellResolution;
  }

  float GetPresShellResolution() const
  {
    return mPresShellResolution;
  }

  void SetCompositionBounds(const ParentLayerRect& aCompositionBounds)
  {
    mCompositionBounds = aCompositionBounds;
  }

  const ParentLayerRect& GetCompositionBounds() const
  {
    return mCompositionBounds;
  }

  void SetDisplayPort(const CSSRect& aDisplayPort)
  {
    mDisplayPort = aDisplayPort;
  }

  const CSSRect& GetDisplayPort() const
  {
    return mDisplayPort;
  }

  void SetCriticalDisplayPort(const CSSRect& aCriticalDisplayPort)
  {
    mCriticalDisplayPort = aCriticalDisplayPort;
  }

  const CSSRect& GetCriticalDisplayPort() const
  {
    return mCriticalDisplayPort;
  }

  void SetCumulativeResolution(const LayoutDeviceToLayerScale2D& aCumulativeResolution)
  {
    mCumulativeResolution = aCumulativeResolution;
  }

  const LayoutDeviceToLayerScale2D& GetCumulativeResolution() const
  {
    return mCumulativeResolution;
  }

  void SetDevPixelsPerCSSPixel(const CSSToLayoutDeviceScale& aDevPixelsPerCSSPixel)
  {
    mDevPixelsPerCSSPixel = aDevPixelsPerCSSPixel;
  }

  const CSSToLayoutDeviceScale& GetDevPixelsPerCSSPixel() const
  {
    return mDevPixelsPerCSSPixel;
  }

  void SetIsRootContent(bool aIsRootContent)
  {
    mIsRootContent = aIsRootContent;
  }

  bool IsRootContent() const
  {
    return mIsRootContent;
  }

  void SetScrollOffset(const CSSPoint& aScrollOffset)
  {
    mScrollOffset = aScrollOffset;
  }

  const CSSPoint& GetScrollOffset() const
  {
    return mScrollOffset;
  }

  void SetSmoothScrollOffset(const CSSPoint& aSmoothScrollDestination)
  {
    mSmoothScrollOffset = aSmoothScrollDestination;
  }

  const CSSPoint& GetSmoothScrollOffset() const
  {
    return mSmoothScrollOffset;
  }

  void SetZoom(const CSSToParentLayerScale2D& aZoom)
  {
    mZoom = aZoom;
  }

  const CSSToParentLayerScale2D& GetZoom() const
  {
    return mZoom;
  }

  void SetScrollOffsetUpdated(uint32_t aScrollGeneration)
  {
    mScrollUpdateType = eMainThread;
    mScrollGeneration = aScrollGeneration;
  }

  void SetScrollOffsetRestored(uint32_t aScrollGeneration)
  {
    mScrollUpdateType = eRestore;
    mScrollGeneration = aScrollGeneration;
  }

  void SetSmoothScrollOffsetUpdated(int32_t aScrollGeneration)
  {
    mDoSmoothScroll = true;
    mScrollGeneration = aScrollGeneration;
  }

  ScrollOffsetUpdateType GetScrollUpdateType() const
  {
    return mScrollUpdateType;
  }

  bool GetScrollOffsetUpdated() const
  {
    return mScrollUpdateType != eNone;
  }

  bool GetDoSmoothScroll() const
  {
    return mDoSmoothScroll;
  }

  uint32_t GetScrollGeneration() const
  {
    return mScrollGeneration;
  }

  ViewID GetScrollId() const
  {
    return mScrollId;
  }

  void SetScrollId(ViewID scrollId)
  {
    mScrollId = scrollId;
  }

  void SetRootCompositionSize(const CSSSize& aRootCompositionSize)
  {
    mRootCompositionSize = aRootCompositionSize;
  }

  const CSSSize& GetRootCompositionSize() const
  {
    return mRootCompositionSize;
  }

  void SetDisplayPortMargins(const ScreenMargin& aDisplayPortMargins)
  {
    mDisplayPortMargins = aDisplayPortMargins;
  }

  const ScreenMargin& GetDisplayPortMargins() const
  {
    return mDisplayPortMargins;
  }

  void SetUseDisplayPortMargins(bool aValue)
  {
    mUseDisplayPortMargins = aValue;
  }

  bool GetUseDisplayPortMargins() const
  {
    return mUseDisplayPortMargins;
  }

  uint32_t GetPresShellId() const
  {
    return mPresShellId;
  }

  void SetPresShellId(uint32_t aPresShellId)
  {
    mPresShellId = aPresShellId;
  }

  void SetViewport(const CSSRect& aViewport)
  {
    mViewport = aViewport;
  }

  const CSSRect& GetViewport() const
  {
    return mViewport;
  }

  void SetExtraResolution(const ScreenToLayerScale2D& aExtraResolution)
  {
    mExtraResolution = aExtraResolution;
  }

  const ScreenToLayerScale2D& GetExtraResolution() const
  {
    return mExtraResolution;
  }

  const CSSRect& GetScrollableRect() const
  {
    return mScrollableRect;
  }

  void SetScrollableRect(const CSSRect& aScrollableRect)
  {
    mScrollableRect = aScrollableRect;
  }

  void SetPaintRequestTime(const TimeStamp& aTime) {
    mPaintRequestTime = aTime;
  }
  const TimeStamp& GetPaintRequestTime() const {
    return mPaintRequestTime;
  }

  void SetIsScrollInfoLayer(bool aIsScrollInfoLayer) {
    mIsScrollInfoLayer = aIsScrollInfoLayer;
  }
  bool IsScrollInfoLayer() const {
    return mIsScrollInfoLayer;
  }

private:
  // A unique ID assigned to each scrollable frame.
  ViewID mScrollId;

  // The pres-shell resolution that has been induced on the document containing
  // this scroll frame as a result of zooming this scroll frame (whether via
  // user action, or choosing an initial zoom level on page load). This can
  // only be different from 1.0 for frames that are zoomable, which currently
  // is just the root content document's root scroll frame (mIsRoot = true).
  // This is a plain float rather than a ScaleFactor because in and of itself
  // it does not convert between any coordinate spaces for which we have names.
  float mPresShellResolution;

  // This is the area within the widget that we're compositing to. It is in the
  // same coordinate space as the reference frame for the scrolled frame.
  //
  // This is useful because, on mobile, the viewport and composition dimensions
  // are not always the same. In this case, we calculate the displayport using
  // an area bigger than the region we're compositing to. If we used the
  // viewport dimensions to calculate the displayport, we'd run into situations
  // where we're prerendering the wrong regions and the content may be clipped,
  // or too much of it prerendered. If the composition dimensions are the same
  // as the viewport dimensions, there is no need for this and we can just use
  // the viewport instead.
  //
  // This value is valid for nested scrollable layers as well, and is still
  // relative to the layer tree origin. This value is provided by Gecko at
  // layout/paint time.
  ParentLayerRect mCompositionBounds;

  // The area of a frame's contents that has been painted, relative to
  // mCompositionBounds.
  //
  // Note that this is structured in such a way that it doesn't depend on the
  // method layout uses to scroll content.
  //
  // May be larger or smaller than |mScrollableRect|.
  //
  // To pre-render a margin of 100 CSS pixels around the window,
  // { x = -100, y = - 100,
  //   width = window.innerWidth + 200, height = window.innerHeight + 200 }
  CSSRect mDisplayPort;

  // If non-empty, the area of a frame's contents that is considered critical
  // to paint. Area outside of this area (i.e. area inside mDisplayPort, but
  // outside of mCriticalDisplayPort) is considered low-priority, and may be
  // painted with lower precision, or not painted at all.
  //
  // The same restrictions for mDisplayPort apply here.
  CSSRect mCriticalDisplayPort;

  // The scrollable bounds of a frame. This is determined by reflow.
  // Ordinarily the x and y will be 0 and the width and height will be the
  // size of the element being scrolled. However for RTL pages or elements
  // the x value may be negative.
  //
  // For scrollable frames that are overflow:hidden the x and y are usually
  // set to the value of the current scroll offset, and the width and height
  // will match the composition bounds width and height. In effect this reduces
  // the scrollable range to 0.
  //
  // This is in the same coordinate space as |mScrollOffset|, but a different
  // coordinate space than |mViewport| and |mDisplayPort|. Note also that this
  // coordinate system is understood by window.scrollTo().
  //
  // This is valid on any layer unless it has no content.
  CSSRect mScrollableRect;

  // The cumulative resolution that the current frame has been painted at.
  // This is the product of the pres-shell resolutions of the document
  // containing this scroll frame and its ancestors, and any css-driven
  // resolution. This information is provided by Gecko at layout/paint time.
  // Note that this is allowed to have different x- and y-scales, but only
  // for subframes (mIsRoot = false). (The same applies to other scales that
  // "inherit" the 2D-ness of this one, such as mZoom.)
  LayoutDeviceToLayerScale2D mCumulativeResolution;

  // New fields from now on should be made private and old fields should
  // be refactored to be private.

  // The conversion factor between CSS pixels and device pixels for this frame.
  // This can vary based on a variety of things, such as reflowing-zoom. The
  // conversion factor for device pixels to layers pixels is just the
  // resolution.
  CSSToLayoutDeviceScale mDevPixelsPerCSSPixel;

  // The position of the top-left of the CSS viewport, relative to the document
  // (or the document relative to the viewport, if that helps understand it).
  //
  // Thus it is relative to the document. It is in the same coordinate space as
  // |mScrollableRect|, but a different coordinate space than |mViewport| and
  // |mDisplayPort|.
  //
  // It is required that the rect:
  // { x = mScrollOffset.x, y = mScrollOffset.y,
  //   width = mCompositionBounds.x / mResolution.scale,
  //   height = mCompositionBounds.y / mResolution.scale }
  // Be within |mScrollableRect|.
  //
  // This is valid for any layer, but is always relative to this frame and
  // not any parents, regardless of parent transforms.
  CSSPoint mScrollOffset;

  // The "user zoom". Content is painted by gecko at mResolution * mDevPixelsPerCSSPixel,
  // but will be drawn to the screen at mZoom. In the steady state, the
  // two will be the same, but during an async zoom action the two may
  // diverge. This information is initialized in Gecko but updated in the APZC.
  CSSToParentLayerScale2D mZoom;

  // The scroll generation counter used to acknowledge the scroll offset update.
  uint32_t mScrollGeneration;

  // If mDoSmoothScroll is true, the scroll offset will be animated smoothly
  // to this value.
  CSSPoint mSmoothScrollOffset;

  // The size of the root scrollable's composition bounds, but in local CSS pixels.
  CSSSize mRootCompositionSize;

  // A display port expressed as layer margins that apply to the rect of what
  // is drawn of the scrollable element.
  ScreenMargin mDisplayPortMargins;

  uint32_t mPresShellId;

  // The CSS viewport, which is the dimensions we're using to constrain the
  // <html> element of this frame, relative to the top-left of the layer. Note
  // that its offset is structured in such a way that it doesn't depend on the
  // method layout uses to scroll content.
  //
  // This is mainly useful on the root layer, however nested iframes can have
  // their own viewport, which will just be the size of the window of the
  // iframe. For layers that don't correspond to a document, this metric is
  // meaningless and invalid.
  CSSRect mViewport;

  // The extra resolution at which content in this scroll frame is drawn beyond
  // that necessary to draw one Layer pixel per Screen pixel.
  ScreenToLayerScale2D mExtraResolution;

  // The time at which the APZC last requested a repaint for this scrollframe.
  TimeStamp mPaintRequestTime;

  // Whether mScrollOffset was updated by something other than the APZ code, and
  // if the APZC receiving this metrics should update its local copy.
  ScrollOffsetUpdateType mScrollUpdateType;

  // Whether or not this is the root scroll frame for the root content document.
  bool mIsRootContent:1;

  // When mDoSmoothScroll, the scroll offset should be animated to
  // smoothly transition to mScrollOffset rather than be updated instantly.
  bool mDoSmoothScroll:1;

  // If this is true then we use the display port margins on this metrics,
  // otherwise use the display port rect.
  bool mUseDisplayPortMargins:1;

  // Whether or not this frame has a "scroll info layer" to capture events.
  bool mIsScrollInfoLayer:1;

  // WARNING!!!!
  //
  // When adding a new field:
  //
  //  - First, consider whether the field can be added to ScrollMetadata
  //    instead. If so, prefer that.
  //
  //  - Otherwise, the following places should be updated to include them
  //    (as needed):
  //      FrameMetrics::operator ==
  //      AsyncPanZoomController::NotifyLayersUpdated
  //      The ParamTraits specialization in GfxMessageUtils.h
  //
  // Please add new fields above this comment.

  // Private helpers for IPC purposes
  void SetDoSmoothScroll(bool aValue) {
    mDoSmoothScroll = aValue;
  }
};

struct ScrollSnapInfo {
  ScrollSnapInfo()
    : mScrollSnapTypeX(NS_STYLE_SCROLL_SNAP_TYPE_NONE)
    , mScrollSnapTypeY(NS_STYLE_SCROLL_SNAP_TYPE_NONE)
  {}

  bool operator==(const ScrollSnapInfo& aOther) const
  {
    return mScrollSnapTypeX == aOther.mScrollSnapTypeX &&
           mScrollSnapTypeY == aOther.mScrollSnapTypeY &&
           mScrollSnapIntervalX == aOther.mScrollSnapIntervalX &&
           mScrollSnapIntervalY == aOther.mScrollSnapIntervalY &&
           mScrollSnapDestination == aOther.mScrollSnapDestination &&
           mScrollSnapCoordinates == aOther.mScrollSnapCoordinates;
  }

  bool HasScrollSnapping() const
  {
    return mScrollSnapTypeY != NS_STYLE_SCROLL_SNAP_TYPE_NONE ||
           mScrollSnapTypeX != NS_STYLE_SCROLL_SNAP_TYPE_NONE;
  }

  // The scroll frame's scroll-snap-type.
  // One of NS_STYLE_SCROLL_SNAP_{NONE, MANDATORY, PROXIMITY}.
  uint8_t mScrollSnapTypeX;
  uint8_t mScrollSnapTypeY;

  // The intervals derived from the scroll frame's scroll-snap-points.
  Maybe<nscoord> mScrollSnapIntervalX;
  Maybe<nscoord> mScrollSnapIntervalY;

  // The scroll frame's scroll-snap-destination, in cooked form (to avoid
  // shipping the raw nsStyleCoord::CalcValue over IPC).
  nsPoint mScrollSnapDestination;

  // The scroll-snap-coordinates of any descendant frames of the scroll frame,
  // relative to the origin of the scrolled frame.
  nsTArray<nsPoint> mScrollSnapCoordinates;
};

/**
 * A clip that applies to a layer, that may be scrolled by some of the
 * scroll frames associated with the layer.
 */
struct LayerClip {
  friend struct IPC::ParamTraits<mozilla::layers::LayerClip>;

public:
  LayerClip()
    : mClipRect()
    , mMaskLayerIndex()
  {}

  explicit LayerClip(const ParentLayerIntRect& aClipRect)
    : mClipRect(aClipRect)
    , mMaskLayerIndex()
  {}

  bool operator==(const LayerClip& aOther) const
  {
    return mClipRect == aOther.mClipRect &&
           mMaskLayerIndex == aOther.mMaskLayerIndex;
  }

  void SetClipRect(const ParentLayerIntRect& aClipRect) {
    mClipRect = aClipRect;
  }
  const ParentLayerIntRect& GetClipRect() const {
    return mClipRect;
  }

  void SetMaskLayerIndex(const Maybe<size_t>& aIndex) {
    mMaskLayerIndex = aIndex;
  }
  const Maybe<size_t>& GetMaskLayerIndex() const {
    return mMaskLayerIndex;
  }

private:
  ParentLayerIntRect mClipRect;

  // Optionally, specifies a mask layer that's part of the clip.
  // This is an index into the MetricsMaskLayers array on the Layer.
  Maybe<size_t> mMaskLayerIndex;
};

typedef Maybe<LayerClip> MaybeLayerClip;  // for passing over IPDL

/**
 * Metadata about a scroll frame that's stored in the layer tree for use by
 * the compositor (including APZ). This includes the scroll frame's FrameMetrics,
 * as well as other metadata. We don't put the other metadata into FrameMetrics
 * to avoid FrameMetrics becoming too bloated (as a FrameMetrics is e.g. sent
 * over IPC for every repaint request for every active scroll frame).
 */
struct ScrollMetadata {
  friend struct IPC::ParamTraits<mozilla::layers::ScrollMetadata>;

  typedef FrameMetrics::ViewID ViewID;
public:
  static StaticAutoPtr<const ScrollMetadata> sNullMetadata;   // We sometimes need an empty metadata

  ScrollMetadata()
    : mMetrics()
    , mSnapInfo()
    , mScrollParentId(FrameMetrics::NULL_SCROLL_ID)
    , mBackgroundColor()
    , mContentDescription()
    , mLineScrollAmount(0, 0)
    , mPageScrollAmount(0, 0)
    , mScrollClip()
    , mHasScrollgrab(false)
    , mAllowVerticalScrollWithWheel(false)
    , mIsLayersIdRoot(false)
    , mUsesContainerScrolling(false)
    , mForceDisableApz(false)
  {}

  bool operator==(const ScrollMetadata& aOther) const
  {
    return mMetrics == aOther.mMetrics &&
           mSnapInfo == aOther.mSnapInfo &&
           mScrollParentId == aOther.mScrollParentId &&
           mBackgroundColor == aOther.mBackgroundColor &&
           // don't compare mContentDescription
           mLineScrollAmount == aOther.mLineScrollAmount &&
           mPageScrollAmount == aOther.mPageScrollAmount &&
           mScrollClip == aOther.mScrollClip &&
           mHasScrollgrab == aOther.mHasScrollgrab &&
           mAllowVerticalScrollWithWheel == aOther.mAllowVerticalScrollWithWheel &&
           mIsLayersIdRoot == aOther.mIsLayersIdRoot &&
           mUsesContainerScrolling == aOther.mUsesContainerScrolling &&
           mForceDisableApz == aOther.mForceDisableApz;
  }

  bool operator!=(const ScrollMetadata& aOther) const
  {
    return !operator==(aOther);
  }

  bool IsDefault() const
  {
    ScrollMetadata def;

    def.mMetrics.SetPresShellId(mMetrics.GetPresShellId());
    return (def == *this);
  }

  FrameMetrics& GetMetrics() { return mMetrics; }
  const FrameMetrics& GetMetrics() const { return mMetrics; }

  void SetSnapInfo(ScrollSnapInfo&& aSnapInfo) {
    mSnapInfo = Move(aSnapInfo);
  }
  const ScrollSnapInfo& GetSnapInfo() const { return mSnapInfo; }

  ViewID GetScrollParentId() const {
    return mScrollParentId;
  }

  void SetScrollParentId(ViewID aParentId) {
    mScrollParentId = aParentId;
  }
  const gfx::Color& GetBackgroundColor() const {
    return mBackgroundColor;
  }
  void SetBackgroundColor(const gfx::Color& aBackgroundColor) {
    mBackgroundColor = aBackgroundColor;
  }
  const nsCString& GetContentDescription() const {
    return mContentDescription;
  }
  void SetContentDescription(const nsCString& aContentDescription) {
    mContentDescription = aContentDescription;
  }
  const LayoutDeviceIntSize& GetLineScrollAmount() const {
    return mLineScrollAmount;
  }
  void SetLineScrollAmount(const LayoutDeviceIntSize& size) {
    mLineScrollAmount = size;
  }
  const LayoutDeviceIntSize& GetPageScrollAmount() const {
    return mPageScrollAmount;
  }
  void SetPageScrollAmount(const LayoutDeviceIntSize& size) {
    mPageScrollAmount = size;
  }

  void SetScrollClip(const Maybe<LayerClip>& aScrollClip) {
    mScrollClip = aScrollClip;
  }
  const Maybe<LayerClip>& GetScrollClip() const {
    return mScrollClip;
  }
  bool HasScrollClip() const {
    return mScrollClip.isSome();
  }
  const LayerClip& ScrollClip() const {
    return mScrollClip.ref();
  }
  LayerClip& ScrollClip() {
    return mScrollClip.ref();
  }

  bool HasMaskLayer() const {
    return HasScrollClip() && ScrollClip().GetMaskLayerIndex();
  }
  Maybe<ParentLayerIntRect> GetClipRect() const {
    return mScrollClip.isSome() ? Some(mScrollClip->GetClipRect()) : Nothing();
  }

  void SetHasScrollgrab(bool aHasScrollgrab) {
    mHasScrollgrab = aHasScrollgrab;
  }
  bool GetHasScrollgrab() const {
    return mHasScrollgrab;
  }
  bool AllowVerticalScrollWithWheel() const {
    return mAllowVerticalScrollWithWheel;
  }
  void SetAllowVerticalScrollWithWheel(bool aValue) {
    mAllowVerticalScrollWithWheel = aValue;
  }
  void SetIsLayersIdRoot(bool aValue) {
    mIsLayersIdRoot = aValue;
  }
  bool IsLayersIdRoot() const {
    return mIsLayersIdRoot;
  }
  // Implemented out of line because the implementation needs gfxPrefs.h
  // and we don't want to include that from FrameMetrics.h.
  void SetUsesContainerScrolling(bool aValue);
  bool UsesContainerScrolling() const {
    return mUsesContainerScrolling;
  }
  void SetForceDisableApz(bool aForceDisable) {
    mForceDisableApz = aForceDisable;
  }
  bool IsApzForceDisabled() const {
    return mForceDisableApz;
  }

private:
  FrameMetrics mMetrics;

  // Information used to determine where to snap to for a given scroll.
  ScrollSnapInfo mSnapInfo;

  // The ViewID of the scrollable frame to which overscroll should be handed off.
  ViewID mScrollParentId;

  // The background color to use when overscrolling.
  gfx::Color mBackgroundColor;

  // A description of the content element corresponding to this frame.
  // This is empty unless this is a scrollable layer and the
  // apz.printtree pref is turned on.
  nsCString mContentDescription;

  // The value of GetLineScrollAmount(), for scroll frames.
  LayoutDeviceIntSize mLineScrollAmount;

  // The value of GetPageScrollAmount(), for scroll frames.
  LayoutDeviceIntSize mPageScrollAmount;

  // A clip to apply when compositing the layer bearing this ScrollMetadata,
  // after applying any transform arising from scrolling this scroll frame.
  // Note that, unlike most other fields of ScrollMetadata, this is allowed
  // to differ between different layers scrolled by the same scroll frame.
  // TODO: Group the fields of ScrollMetadata into sub-structures to separate
  // fields with this property better.
  Maybe<LayerClip> mScrollClip;

  // Whether or not this frame is for an element marked 'scrollgrab'.
  bool mHasScrollgrab:1;

  // Whether or not the frame can be vertically scrolled with a mouse wheel.
  bool mAllowVerticalScrollWithWheel:1;

  // Whether these framemetrics are for the root scroll frame (root element if
  // we don't have a root scroll frame) for its layers id.
  bool mIsLayersIdRoot:1;

  // True if scrolling using containers, false otherwise. This can be removed
  // when containerful scrolling is eliminated.
  bool mUsesContainerScrolling:1;

  // Whether or not the compositor should actually do APZ-scrolling on this
  // scrollframe.
  bool mForceDisableApz:1;

  // WARNING!!!!
  //
  // When adding new fields to ScrollMetadata, the following places should be
  // updated to include them (as needed):
  //    ScrollMetadata::operator ==
  //    AsyncPanZoomController::NotifyLayersUpdated
  //    The ParamTraits specialization in GfxMessageUtils.h
  //
  // Please add new fields above this comment.
};

/**
 * This class allows us to uniquely identify a scrollable layer. The
 * mLayersId identifies the layer tree (corresponding to a child process
 * and/or tab) that the scrollable layer belongs to. The mPresShellId
 * is a temporal identifier (corresponding to the document loaded that
 * contains the scrollable layer, which may change over time). The
 * mScrollId corresponds to the actual frame that is scrollable.
 */
struct ScrollableLayerGuid {
  uint64_t mLayersId;
  uint32_t mPresShellId;
  FrameMetrics::ViewID mScrollId;

  ScrollableLayerGuid()
    : mLayersId(0)
    , mPresShellId(0)
    , mScrollId(0)
  {
  }

  ScrollableLayerGuid(uint64_t aLayersId, uint32_t aPresShellId,
                      FrameMetrics::ViewID aScrollId)
    : mLayersId(aLayersId)
    , mPresShellId(aPresShellId)
    , mScrollId(aScrollId)
  {
  }

  ScrollableLayerGuid(uint64_t aLayersId, const FrameMetrics& aMetrics)
    : mLayersId(aLayersId)
    , mPresShellId(aMetrics.GetPresShellId())
    , mScrollId(aMetrics.GetScrollId())
  {
  }

  ScrollableLayerGuid(const ScrollableLayerGuid& other)
    : mLayersId(other.mLayersId)
    , mPresShellId(other.mPresShellId)
    , mScrollId(other.mScrollId)
  {
  }

  ~ScrollableLayerGuid()
  {
  }

  bool operator==(const ScrollableLayerGuid& other) const
  {
    return mLayersId == other.mLayersId
        && mPresShellId == other.mPresShellId
        && mScrollId == other.mScrollId;
  }

  bool operator!=(const ScrollableLayerGuid& other) const
  {
    return !(*this == other);
  }

  bool operator<(const ScrollableLayerGuid& other) const
  {
    if (mLayersId < other.mLayersId) {
      return true;
    }
    if (mLayersId == other.mLayersId) {
      if (mPresShellId < other.mPresShellId) {
        return true;
      }
      if (mPresShellId == other.mPresShellId) {
        return mScrollId < other.mScrollId;
      }
    }
    return false;
  }

  PLDHashNumber Hash() const
  {
    return HashGeneric(mLayersId, mPresShellId, mScrollId);
  }
};

template <int LogLevel>
gfx::Log<LogLevel>& operator<<(gfx::Log<LogLevel>& log, const ScrollableLayerGuid& aGuid) {
  return log << '(' << aGuid.mLayersId << ',' << aGuid.mPresShellId << ',' << aGuid.mScrollId << ')';
}

struct ZoomConstraints {
  bool mAllowZoom;
  bool mAllowDoubleTapZoom;
  CSSToParentLayerScale mMinZoom;
  CSSToParentLayerScale mMaxZoom;

  ZoomConstraints()
    : mAllowZoom(true)
    , mAllowDoubleTapZoom(true)
  {
    MOZ_COUNT_CTOR(ZoomConstraints);
  }

  ZoomConstraints(bool aAllowZoom,
                  bool aAllowDoubleTapZoom,
                  const CSSToParentLayerScale& aMinZoom,
                  const CSSToParentLayerScale& aMaxZoom)
    : mAllowZoom(aAllowZoom)
    , mAllowDoubleTapZoom(aAllowDoubleTapZoom)
    , mMinZoom(aMinZoom)
    , mMaxZoom(aMaxZoom)
  {
    MOZ_COUNT_CTOR(ZoomConstraints);
  }

  ZoomConstraints(const ZoomConstraints& other)
    : mAllowZoom(other.mAllowZoom)
    , mAllowDoubleTapZoom(other.mAllowDoubleTapZoom)
    , mMinZoom(other.mMinZoom)
    , mMaxZoom(other.mMaxZoom)
  {
    MOZ_COUNT_CTOR(ZoomConstraints);
  }

  ~ZoomConstraints()
  {
    MOZ_COUNT_DTOR(ZoomConstraints);
  }

  bool operator==(const ZoomConstraints& other) const
  {
    return mAllowZoom == other.mAllowZoom
        && mAllowDoubleTapZoom == other.mAllowDoubleTapZoom
        && mMinZoom == other.mMinZoom
        && mMaxZoom == other.mMaxZoom;
  }

  bool operator!=(const ZoomConstraints& other) const
  {
    return !(*this == other);
  }
};

struct ScrollableLayerGuidHash
{
  std::size_t operator()(const ScrollableLayerGuid& Guid) const
  {
    return Guid.Hash();
  }
};


typedef Maybe<ZoomConstraints> MaybeZoomConstraints;

} // namespace layers
} // namespace mozilla

#endif /* GFX_FRAMEMETRICS_H */
