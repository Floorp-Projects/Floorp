/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_FRAMEMETRICS_H
#define GFX_FRAMEMETRICS_H

#include <stdint.h>  // for uint8_t, uint32_t, uint64_t
#include <iosfwd>

#include "Units.h"                  // for CSSRect, CSSPixel, etc
#include "mozilla/DefineEnum.h"     // for MOZ_DEFINE_ENUM
#include "mozilla/HashFunctions.h"  // for HashGeneric
#include "mozilla/Maybe.h"
#include "mozilla/gfx/BasePoint.h"               // for BasePoint
#include "mozilla/gfx/Rect.h"                    // for RoundedIn
#include "mozilla/gfx/ScaleFactor.h"             // for ScaleFactor
#include "mozilla/gfx/Logging.h"                 // for Log
#include "mozilla/layers/LayersTypes.h"          // for ScrollDirection
#include "mozilla/layers/ScrollableLayerGuid.h"  // for ScrollableLayerGuid
#include "mozilla/ScrollPositionUpdate.h"        // for ScrollPositionUpdate
#include "mozilla/StaticPtr.h"                   // for StaticAutoPtr
#include "mozilla/TimeStamp.h"                   // for TimeStamp
#include "nsTHashMap.h"                          // for nsTHashMap
#include "nsString.h"
#include "PLDHashTable.h"  // for PLDHashNumber

struct nsStyleDisplay;
namespace mozilla {
enum class StyleScrollSnapStrictness : uint8_t;
enum class StyleOverscrollBehavior : uint8_t;
class WritingMode;
}  // namespace mozilla

namespace IPC {
template <typename T>
struct ParamTraits;
}  // namespace IPC

namespace mozilla {
namespace layers {

/**
 * Metrics about a scroll frame that are sent to the compositor and used
 * by APZ.
 *
 * This is used for two main purposes:
 *
 *   (1) Sending information about a scroll frame to the compositor and APZ
 *       as part of a layers or WebRender transaction.
 *   (2) Storing information about a scroll frame in APZ that persists
 *       between transactions.
 *
 * TODO: Separate these two uses into two distinct structures.
 *
 * A related class, RepaintRequest, is used for sending information about a
 * scroll frame back from the compositor to the main thread when requesting
 * a repaint of the scroll frame's contents.
 */
struct FrameMetrics {
  friend struct IPC::ParamTraits<mozilla::layers::FrameMetrics>;
  friend std::ostream& operator<<(std::ostream& aStream,
                                  const FrameMetrics& aMetrics);

  typedef ScrollableLayerGuid::ViewID ViewID;

 public:
  // clang-format off
  MOZ_DEFINE_ENUM_WITH_BASE_AT_CLASS_SCOPE(
    ScrollOffsetUpdateType, uint8_t, (
      eNone,          // The default; the scroll offset was not updated
      eMainThread,    // The scroll offset was updated by the main thread.
      eRestore        // The scroll offset was updated by the main thread, but
                      // as a restore from history or after a frame
                      // reconstruction.  In this case, APZ can ignore the
                      // offset change if the user has done an APZ scroll
                      // already.
  ));
  // clang-format on

  FrameMetrics()
      : mScrollId(ScrollableLayerGuid::NULL_SCROLL_ID),
        mPresShellResolution(1),
        mCompositionBounds(0, 0, 0, 0),
        mDisplayPort(0, 0, 0, 0),
        mCriticalDisplayPort(0, 0, 0, 0),
        mScrollableRect(0, 0, 0, 0),
        mCumulativeResolution(),
        mDevPixelsPerCSSPixel(1),
        mScrollOffset(0, 0),
        mZoom(),
        mBoundingCompositionSize(0, 0),
        mPresShellId(-1),
        mLayoutViewport(0, 0, 0, 0),
        mExtraResolution(),
        mPaintRequestTime(),
        mVisualDestination(0, 0),
        mVisualScrollUpdateType(eNone),
        mCompositionSizeWithoutDynamicToolbar(),
        mIsRootContent(false),
        mIsScrollInfoLayer(false),
        mHasNonZeroDisplayPortMargins(false),
        mMinimalDisplayPort(false) {}

  // Default copy ctor and operator= are fine

  bool operator==(const FrameMetrics& aOther) const {
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
           mBoundingCompositionSize == aOther.mBoundingCompositionSize &&
           mPresShellId == aOther.mPresShellId &&
           mLayoutViewport.IsEqualEdges(aOther.mLayoutViewport) &&
           mExtraResolution == aOther.mExtraResolution &&
           mPaintRequestTime == aOther.mPaintRequestTime &&
           mVisualDestination == aOther.mVisualDestination &&
           mVisualScrollUpdateType == aOther.mVisualScrollUpdateType &&
           mIsRootContent == aOther.mIsRootContent &&
           mIsScrollInfoLayer == aOther.mIsScrollInfoLayer &&
           mHasNonZeroDisplayPortMargins ==
               aOther.mHasNonZeroDisplayPortMargins &&
           mMinimalDisplayPort == aOther.mMinimalDisplayPort &&
           mFixedLayerMargins == aOther.mFixedLayerMargins &&
           mCompositionSizeWithoutDynamicToolbar ==
               aOther.mCompositionSizeWithoutDynamicToolbar;
  }

  bool operator!=(const FrameMetrics& aOther) const {
    return !operator==(aOther);
  }

  bool IsScrollable() const {
    return mScrollId != ScrollableLayerGuid::NULL_SCROLL_ID;
  }

  CSSToScreenScale2D DisplayportPixelsPerCSSPixel() const {
    // Note: use 'mZoom * ParentLayerToLayerScale(1.0f)' as the CSS-to-Layer
    // scale instead of LayersPixelsPerCSSPixel(), because displayport
    // calculations are done in the context of a repaint request, where we ask
    // Layout to repaint at a new resolution that includes any async zoom. Until
    // this repaint request is processed, LayersPixelsPerCSSPixel() does not yet
    // include the async zoom, but it will when the displayport is interpreted
    // for the repaint.
    return mZoom * ParentLayerToLayerScale(1.0f) / mExtraResolution;
  }

  CSSToLayerScale2D LayersPixelsPerCSSPixel() const {
    return mDevPixelsPerCSSPixel * mCumulativeResolution;
  }

  // Get the amount by which this frame has been zoomed since the last repaint.
  LayerToParentLayerScale GetAsyncZoom() const {
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
  CSSRect GetExpandedScrollableRect() const {
    CSSRect scrollableRect = mScrollableRect;
    CSSSize compSize = CalculateCompositedSizeInCssPixels();
    if (scrollableRect.Width() < compSize.width) {
      scrollableRect.SetRectX(
          std::max(0.f, scrollableRect.X() -
                            (compSize.width - scrollableRect.Width())),
          compSize.width);
    }

    if (scrollableRect.Height() < compSize.height) {
      scrollableRect.SetRectY(
          std::max(0.f, scrollableRect.Y() -
                            (compSize.height - scrollableRect.Height())),
          compSize.height);
    }

    return scrollableRect;
  }

  CSSSize CalculateCompositedSizeInCssPixels() const {
    if (GetZoom() == CSSToParentLayerScale2D(0, 0)) {
      return CSSSize();  // avoid division by zero
    }
    return mCompositionBounds.Size() / GetZoom();
  }

  /*
   * Calculate the composition bounds of this frame in the CSS pixels of
   * the content surrounding the scroll frame. (This can be thought of as
   * "parent CSS" pixels).
   * Note that it does not make sense to ask for the composition bounds in the
   * CSS pixels of the scrolled content (that is, regular CSS pixels),
   * because the origin of the composition bounds is not meaningful in that
   * coordinate space. (The size is, use CalculateCompositedSizeInCssPixels()
   * for that.)
   */
  CSSRect CalculateCompositionBoundsInCssPixelsOfSurroundingContent() const {
    if (GetZoom() == CSSToParentLayerScale2D(0, 0)) {
      return CSSRect();  // avoid division by zero
    }
    // The CSS pixels of the scrolled content and the CSS pixels of the
    // surrounding content only differ if the scrolled content is rendered
    // at a higher resolution, and the difference is the resolution.
    return mCompositionBounds / GetZoom() * CSSToCSSScale{mPresShellResolution};
  }

  CSSSize CalculateBoundedCompositedSizeInCssPixels() const {
    CSSSize size = CalculateCompositedSizeInCssPixels();
    size.width = std::min(size.width, mBoundingCompositionSize.width);
    size.height = std::min(size.height, mBoundingCompositionSize.height);
    return size;
  }

  CSSRect CalculateScrollRange() const {
    CSSSize scrollPortSize = CalculateCompositedSizeInCssPixels();
    CSSRect scrollRange = mScrollableRect;
    scrollRange.SetWidth(
        std::max(scrollRange.Width() - scrollPortSize.width, 0.0f));
    scrollRange.SetHeight(
        std::max(scrollRange.Height() - scrollPortSize.height, 0.0f));
    return scrollRange;
  }

  void ScrollBy(const CSSPoint& aPoint) {
    SetVisualScrollOffset(GetVisualScrollOffset() + aPoint);
  }

  void ZoomBy(float aScale) { ZoomBy(gfxSize(aScale, aScale)); }

  void ZoomBy(const gfxSize& aScale) {
    mZoom.xScale *= aScale.width;
    mZoom.yScale *= aScale.height;
  }

  /*
   * Compares an APZ frame metrics with an incoming content frame metrics
   * to see if APZ has a scroll offset that has not been incorporated into
   * the content frame metrics.
   */
  bool HasPendingScroll(const FrameMetrics& aContentFrameMetrics) const {
    return GetVisualScrollOffset() !=
           aContentFrameMetrics.GetVisualScrollOffset();
  }

  /*
   * Returns true if the layout scroll offset or visual scroll offset changed.
   */
  bool ApplyScrollUpdateFrom(const ScrollPositionUpdate& aUpdate);

  /**
   * Applies the relative scroll offset update contained in aOther to the
   * scroll offset contained in this. The scroll delta is clamped to the
   * scrollable region.
   *
   * @returns The clamped scroll offset delta that was applied
   */
  CSSPoint ApplyRelativeScrollUpdateFrom(const ScrollPositionUpdate& aUpdate);

  CSSPoint ApplyPureRelativeScrollUpdateFrom(
      const ScrollPositionUpdate& aUpdate);

  void UpdatePendingScrollInfo(const ScrollPositionUpdate& aInfo);

 public:
  void SetPresShellResolution(float aPresShellResolution) {
    mPresShellResolution = aPresShellResolution;
  }

  float GetPresShellResolution() const { return mPresShellResolution; }

  void SetCompositionBounds(const ParentLayerRect& aCompositionBounds) {
    mCompositionBounds = aCompositionBounds;
  }

  const ParentLayerRect& GetCompositionBounds() const {
    return mCompositionBounds;
  }

  void SetDisplayPort(const CSSRect& aDisplayPort) {
    mDisplayPort = aDisplayPort;
  }

  const CSSRect& GetDisplayPort() const { return mDisplayPort; }

  void SetCriticalDisplayPort(const CSSRect& aCriticalDisplayPort) {
    mCriticalDisplayPort = aCriticalDisplayPort;
  }

  const CSSRect& GetCriticalDisplayPort() const { return mCriticalDisplayPort; }

  void SetCumulativeResolution(
      const LayoutDeviceToLayerScale2D& aCumulativeResolution) {
    mCumulativeResolution = aCumulativeResolution;
  }

  const LayoutDeviceToLayerScale2D& GetCumulativeResolution() const {
    return mCumulativeResolution;
  }

  void SetDevPixelsPerCSSPixel(
      const CSSToLayoutDeviceScale& aDevPixelsPerCSSPixel) {
    mDevPixelsPerCSSPixel = aDevPixelsPerCSSPixel;
  }

  const CSSToLayoutDeviceScale& GetDevPixelsPerCSSPixel() const {
    return mDevPixelsPerCSSPixel;
  }

  void SetIsRootContent(bool aIsRootContent) {
    mIsRootContent = aIsRootContent;
  }

  bool IsRootContent() const { return mIsRootContent; }

  // Set scroll offset, first clamping to the scroll range.
  // Return true if it changed.
  bool ClampAndSetVisualScrollOffset(const CSSPoint& aScrollOffset) {
    CSSPoint offsetBefore = GetVisualScrollOffset();
    SetVisualScrollOffset(CalculateScrollRange().ClampPoint(aScrollOffset));
    return (offsetBefore != GetVisualScrollOffset());
  }

  CSSPoint GetLayoutScrollOffset() const { return mLayoutViewport.TopLeft(); }
  // Returns true if it changed.
  bool SetLayoutScrollOffset(const CSSPoint& aLayoutScrollOffset) {
    CSSPoint offsetBefore = GetLayoutScrollOffset();
    mLayoutViewport.MoveTo(aLayoutScrollOffset);
    return (offsetBefore != GetLayoutScrollOffset());
  }

  const CSSPoint& GetVisualScrollOffset() const { return mScrollOffset; }
  void SetVisualScrollOffset(const CSSPoint& aVisualScrollOffset) {
    mScrollOffset = aVisualScrollOffset;
  }

  void SetZoom(const CSSToParentLayerScale2D& aZoom) { mZoom = aZoom; }

  const CSSToParentLayerScale2D& GetZoom() const { return mZoom; }

  void SetScrollGeneration(const ScrollGeneration& aScrollGeneration) {
    mScrollGeneration = aScrollGeneration;
  }

  ScrollGeneration GetScrollGeneration() const { return mScrollGeneration; }

  ViewID GetScrollId() const { return mScrollId; }

  void SetScrollId(ViewID scrollId) { mScrollId = scrollId; }

  void SetBoundingCompositionSize(const CSSSize& aBoundingCompositionSize) {
    mBoundingCompositionSize = aBoundingCompositionSize;
  }

  const CSSSize& GetBoundingCompositionSize() const {
    return mBoundingCompositionSize;
  }

  uint32_t GetPresShellId() const { return mPresShellId; }

  void SetPresShellId(uint32_t aPresShellId) { mPresShellId = aPresShellId; }

  void SetLayoutViewport(const CSSRect& aLayoutViewport) {
    mLayoutViewport = aLayoutViewport;
  }

  const CSSRect& GetLayoutViewport() const { return mLayoutViewport; }

  CSSRect GetVisualViewport() const {
    return CSSRect(GetVisualScrollOffset(),
                   CalculateCompositedSizeInCssPixels());
  }

  void SetExtraResolution(const ScreenToLayerScale2D& aExtraResolution) {
    mExtraResolution = aExtraResolution;
  }

  const ScreenToLayerScale2D& GetExtraResolution() const {
    return mExtraResolution;
  }

  const CSSRect& GetScrollableRect() const { return mScrollableRect; }

  void SetScrollableRect(const CSSRect& aScrollableRect) {
    mScrollableRect = aScrollableRect;
  }

  // If the frame is in vertical-RTL writing mode(E.g. "writing-mode:
  // vertical-rl" in CSS), or if it's in horizontal-RTL writing-mode(E.g.
  // "writing-mode: horizontal-tb; direction: rtl;" in CSS), then this function
  // returns true. From the representation perspective, frames whose horizontal
  // contents start at rightside also cause their horizontal scrollbars, if any,
  // initially start at rightside. So we can also learn about the initial side
  // of the horizontal scrollbar for the frame by calling this function.
  bool IsHorizontalContentRightToLeft() const { return mScrollableRect.x < 0; }

  void SetPaintRequestTime(const TimeStamp& aTime) {
    mPaintRequestTime = aTime;
  }
  const TimeStamp& GetPaintRequestTime() const { return mPaintRequestTime; }

  void SetIsScrollInfoLayer(bool aIsScrollInfoLayer) {
    mIsScrollInfoLayer = aIsScrollInfoLayer;
  }
  bool IsScrollInfoLayer() const { return mIsScrollInfoLayer; }

  void SetHasNonZeroDisplayPortMargins(bool aHasNonZeroDisplayPortMargins) {
    mHasNonZeroDisplayPortMargins = aHasNonZeroDisplayPortMargins;
  }
  bool HasNonZeroDisplayPortMargins() const {
    return mHasNonZeroDisplayPortMargins;
  }

  void SetMinimalDisplayPort(bool aMinimalDisplayPort) {
    mMinimalDisplayPort = aMinimalDisplayPort;
  }
  bool IsMinimalDisplayPort() const { return mMinimalDisplayPort; }

  void SetVisualDestination(const CSSPoint& aVisualDestination) {
    mVisualDestination = aVisualDestination;
  }
  const CSSPoint& GetVisualDestination() const { return mVisualDestination; }

  void SetVisualScrollUpdateType(ScrollOffsetUpdateType aUpdateType) {
    mVisualScrollUpdateType = aUpdateType;
  }
  ScrollOffsetUpdateType GetVisualScrollUpdateType() const {
    return mVisualScrollUpdateType;
  }

  // Determine if the visual viewport is outside of the layout viewport and
  // adjust the x,y-offset in mLayoutViewport accordingly. This is necessary to
  // allow APZ to async-scroll the layout viewport.
  //
  // This is a no-op if mIsRootContent is false.
  void RecalculateLayoutViewportOffset();

  void SetFixedLayerMargins(const ScreenMargin& aFixedLayerMargins) {
    mFixedLayerMargins = aFixedLayerMargins;
  }
  const ScreenMargin& GetFixedLayerMargins() const {
    return mFixedLayerMargins;
  }

  void SetCompositionSizeWithoutDynamicToolbar(const ParentLayerSize& aSize) {
    MOZ_ASSERT(mIsRootContent);
    mCompositionSizeWithoutDynamicToolbar = aSize;
  }
  const ParentLayerSize& GetCompositionSizeWithoutDynamicToolbar() const {
    MOZ_ASSERT(mIsRootContent);
    return mCompositionSizeWithoutDynamicToolbar;
  }

  // Helper function for RecalculateViewportOffset(). Exposed so that
  // APZC can perform the operation on other copies of the layout
  // and visual viewport rects (e.g. the "effective" ones used to implement
  // the frame delay).
  // Modifies |aLayoutViewport| to continue enclosing |aVisualViewport|
  // if possible.
  // The layout viewport needs to remain clamped to the scrollable rect,
  // and we pass in the scrollable rect so this function can maintain that
  // constraint.
  static void KeepLayoutViewportEnclosingVisualViewport(
      const CSSRect& aVisualViewport, const CSSRect& aScrollableRect,
      CSSRect& aLayoutViewport);

 private:
  // A ID assigned to each scrollable frame, unique within each LayersId..
  ViewID mScrollId;

  // The pres-shell resolution that has been induced on the document containing
  // this scroll frame as a result of zooming this scroll frame (whether via
  // user action, or choosing an initial zoom level on page load). This can
  // only be different from 1.0 for frames that are zoomable, which currently
  // is just the root content document's root scroll frame
  // (mIsRootContent = true).
  // This is a plain float rather than a ScaleFactor because in and of itself
  // it does not convert between any coordinate spaces for which we have names.
  float mPresShellResolution;

  // This is the area within the widget that we're compositing to. It is in the
  // layer coordinates of the scrollable content's parent layer.
  //
  // The size of the composition bounds corresponds to the size of the scroll
  // frame's scroll port (but in a coordinate system where the size does not
  // change during zooming).
  //
  // The origin of the composition bounds is relative to the layer tree origin.
  // Unlike the scroll port's origin, it does not change during scrolling of
  // the scrollable layer to which it is associated. However, it may change due
  // to scrolling of ancestor layers.
  //
  // This value is provided by Gecko at layout/paint time.
  ParentLayerRect mCompositionBounds;

  // The area of a scroll frame's contents that has been painted, relative to
  // GetLayoutScrollOffset().
  //
  // Should not be larger than GetExpandedScrollableRect().
  //
  // To pre-render a margin of 100 CSS pixels around the scroll port,
  // { x = -100, y = - 100,
  //   width = scrollPort.width + 200, height = scrollPort.height + 200 }
  // where scrollPort = CalculateCompositedSizeInCssPixels().
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
  // coordinate space than |mDisplayPort|. Note also that this coordinate
  // system is understood by window.scrollTo().
  CSSRect mScrollableRect;

  // The cumulative resolution that the current frame has been painted at.
  // This is the product of the pres-shell resolutions of the document
  // containing this scroll frame and its ancestors, and any css-driven
  // resolution. This information is provided by Gecko at layout/paint time.
  // Note that this is allowed to have different x- and y-scales, but only
  // for subframes (mIsRootContent = false). (The same applies to other scales
  // that "inherit" the 2D-ness of this one, such as mZoom.)
  LayoutDeviceToLayerScale2D mCumulativeResolution;

  // The conversion factor between CSS pixels and device pixels for this frame.
  // This can vary based on a variety of things, such as reflowing-zoom.
  CSSToLayoutDeviceScale mDevPixelsPerCSSPixel;

  // The position of the top-left of the scroll frame's scroll port, relative
  // to the scrollable content's origin.
  //
  // This is in the same coordinate space as |mScrollableRect|, but a different
  // coordinate space than |mDisplayPort|.
  //
  // It is required that the rect:
  // { x = mScrollOffset.x, y = mScrollOffset.y,
  //   width = scrollPort.width,
  //   height = scrollPort.height }
  // (where scrollPort = CalculateCompositedSizeInCssPixels())
  // be within |mScrollableRect|.
  CSSPoint mScrollOffset;

  // The "user zoom". Content is painted by gecko at mCumulativeResolution *
  // mDevPixelsPerCSSPixel, but will be drawn to the screen at mZoom. In the
  // steady state, the two will be the same, but during an async zoom action the
  // two may diverge. This information is initialized in Gecko but updated in
  // the APZC.
  CSSToParentLayerScale2D mZoom;

  // The scroll generation counter used to acknowledge the scroll offset update.
  ScrollGeneration mScrollGeneration;

  // A bounding size for our composition bounds (no larger than the
  // cross-process RCD-RSF's composition size), in local CSS pixels.
  CSSSize mBoundingCompositionSize;

  uint32_t mPresShellId;

  // For a root scroll frame (RSF), the document's layout viewport
  // (sometimes called "CSS viewport" in older code).
  //
  // Its size is the dimensions we're using to constrain the <html> element
  // of the document (i.e. the initial containing block (ICB) size).
  //
  // Its origin is the RSF's layout scroll position, i.e. the scroll position
  // exposed to web content via window.scrollX/Y.
  //
  // Note that only the root content document's RSF has a layout viewport
  // that's distinct from the visual viewport. For an iframe RSF, the two
  // are the same.
  //
  // For a scroll frame that is not an RSF, this metric is meaningless and
  // invalid.
  CSSRect mLayoutViewport;

  // The extra resolution at which content in this scroll frame is drawn beyond
  // that necessary to draw one Layer pixel per Screen pixel.
  ScreenToLayerScale2D mExtraResolution;

  // The time at which the APZC last requested a repaint for this scroll frame.
  TimeStamp mPaintRequestTime;

  // These fields are used when the main thread wants to set a visual viewport
  // offset that's distinct from the layout viewport offset.
  // In this case, mVisualScrollUpdateType is set to eMainThread, and
  // mVisualDestination is set to desired visual destination (relative
  // to the document, like mScrollOffset).
  CSSPoint mVisualDestination;
  ScrollOffsetUpdateType mVisualScrollUpdateType;

  // 'fixed layer margins' on the main-thread. This is only used for the
  // root-content scroll frame.
  ScreenMargin mFixedLayerMargins;

  // Similar to mCompositionBounds.Size() but not including the dynamic toolbar
  // height.
  // If we are not using a dynamic toolbar, this has the same value as
  // mCompositionBounds.Size().
  ParentLayerSize mCompositionSizeWithoutDynamicToolbar;

  // Whether or not this is the root scroll frame for the root content document.
  bool mIsRootContent : 1;

  // True if this scroll frame is a scroll info layer. A scroll info layer is
  // not layerized and its content cannot be truly async-scrolled, but its
  // metrics are still sent to and updated by the compositor, with the updates
  // being reflected on the next paint rather than the next composite.
  bool mIsScrollInfoLayer : 1;

  // Whether there are non-zero display port margins set on this element.
  bool mHasNonZeroDisplayPortMargins : 1;

  // Whether this scroll frame is using a minimal display port, which means that
  // any set display port margins are ignored when calculating the display port
  // and instead zero margins are used and further no tile or alignment
  // boundaries are used that could potentially expand the size.
  bool mMinimalDisplayPort : 1;

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
  //      The ParamTraits specialization in LayersMessageUtils.h
  //
  // Please add new fields above this comment.
};

struct ScrollSnapInfo {
  ScrollSnapInfo();

  bool operator==(const ScrollSnapInfo& aOther) const {
    return mScrollSnapStrictnessX == aOther.mScrollSnapStrictnessX &&
           mScrollSnapStrictnessY == aOther.mScrollSnapStrictnessY &&
           mSnapPositionX == aOther.mSnapPositionX &&
           mSnapPositionY == aOther.mSnapPositionY &&
           mXRangeWiderThanSnapport == aOther.mXRangeWiderThanSnapport &&
           mYRangeWiderThanSnapport == aOther.mYRangeWiderThanSnapport &&
           mSnapportSize == aOther.mSnapportSize;
  }

  bool HasScrollSnapping() const;
  bool HasSnapPositions() const;

  void InitializeScrollSnapStrictness(WritingMode aWritingMode,
                                      const nsStyleDisplay* aDisplay);

  // The scroll frame's scroll-snap-type.
  StyleScrollSnapStrictness mScrollSnapStrictnessX;
  StyleScrollSnapStrictness mScrollSnapStrictnessY;

  // The scroll positions corresponding to scroll-snap-align values.
  CopyableTArray<nscoord> mSnapPositionX;
  CopyableTArray<nscoord> mSnapPositionY;

  struct ScrollSnapRange {
    ScrollSnapRange() = default;

    ScrollSnapRange(nscoord aStart, nscoord aEnd)
        : mStart(aStart), mEnd(aEnd) {}

    nscoord mStart;
    nscoord mEnd;
    bool operator==(const ScrollSnapRange& aOther) const {
      return mStart == aOther.mStart && mEnd == aOther.mEnd;
    }

    // Returns true if |aPoint| is a valid snap position in this range.
    bool IsValid(nscoord aPoint, nscoord aSnapportSize) const {
      MOZ_ASSERT(mEnd - mStart > aSnapportSize);
      return mStart <= aPoint && aPoint <= mEnd - aSnapportSize;
    }
  };
  // An array of the range that the target element is larger than the snapport
  // on the axis.
  // Snap positions in this range will be valid snap positions in the case where
  // the distance between the closest snap position and the second closest snap
  // position is still larger than the snapport size.
  // See https://drafts.csswg.org/css-scroll-snap-1/#snap-overflow
  //
  // Note: This range contains scroll-margin values.
  CopyableTArray<ScrollSnapRange> mXRangeWiderThanSnapport;
  CopyableTArray<ScrollSnapRange> mYRangeWiderThanSnapport;

  // Note: This snapport size has been already deflated by scroll-padding.
  nsSize mSnapportSize;
};

// clang-format off
MOZ_DEFINE_ENUM_CLASS_WITH_BASE(
  OverscrollBehavior, uint8_t, (
    Auto,
    Contain,
    None
));
// clang-format on

std::ostream& operator<<(std::ostream& aStream,
                         const OverscrollBehavior& aBehavior);

struct OverscrollBehaviorInfo {
  OverscrollBehaviorInfo();

  // Construct from StyleOverscrollBehavior values.
  static OverscrollBehaviorInfo FromStyleConstants(
      StyleOverscrollBehavior aBehaviorX, StyleOverscrollBehavior aBehaviorY);

  bool operator==(const OverscrollBehaviorInfo& aOther) const;
  friend std::ostream& operator<<(std::ostream& aStream,
                                  const OverscrollBehaviorInfo& aInfo);

  OverscrollBehavior mBehaviorX;
  OverscrollBehavior mBehaviorY;
};

/**
 * A clip that applies to a layer, that may be scrolled by some of the
 * scroll frames associated with the layer.
 */
struct LayerClip {
  friend struct IPC::ParamTraits<mozilla::layers::LayerClip>;

 public:
  LayerClip() : mClipRect(), mMaskLayerIndex() {}

  explicit LayerClip(const ParentLayerIntRect& aClipRect)
      : mClipRect(aClipRect), mMaskLayerIndex() {}

  bool operator==(const LayerClip& aOther) const {
    return mClipRect == aOther.mClipRect &&
           mMaskLayerIndex == aOther.mMaskLayerIndex;
  }

  void SetClipRect(const ParentLayerIntRect& aClipRect) {
    mClipRect = aClipRect;
  }
  const ParentLayerIntRect& GetClipRect() const { return mClipRect; }

  void SetMaskLayerIndex(const Maybe<size_t>& aIndex) {
    mMaskLayerIndex = aIndex;
  }
  const Maybe<size_t>& GetMaskLayerIndex() const { return mMaskLayerIndex; }

 private:
  ParentLayerIntRect mClipRect;

  // Optionally, specifies a mask layer that's part of the clip.
  // This is an index into the MetricsMaskLayers array on the Layer.
  Maybe<size_t> mMaskLayerIndex;
};

typedef Maybe<LayerClip> MaybeLayerClip;  // for passing over IPDL

/**
 * Metadata about a scroll frame that's sent to the compositor during a layers
 * or WebRender transaction, and also stored by APZ between transactions.
 * This includes the scroll frame's FrameMetrics, as well as other metadata.
 * We don't put the other metadata into FrameMetrics to avoid FrameMetrics
 * becoming too bloated (as a FrameMetrics is e.g. stored in memory shared
 * with the content process).
 */
struct ScrollMetadata {
  friend struct IPC::ParamTraits<mozilla::layers::ScrollMetadata>;
  friend std::ostream& operator<<(std::ostream& aStream,
                                  const ScrollMetadata& aMetadata);

  typedef ScrollableLayerGuid::ViewID ViewID;

 public:
  static StaticAutoPtr<const ScrollMetadata>
      sNullMetadata;  // We sometimes need an empty metadata

  ScrollMetadata()
      : mMetrics(),
        mSnapInfo(),
        mScrollParentId(ScrollableLayerGuid::NULL_SCROLL_ID),
        mBackgroundColor(),
        mContentDescription(),
        mLineScrollAmount(0, 0),
        mPageScrollAmount(0, 0),
        mScrollClip(),
        mHasScrollgrab(false),
        mIsLayersIdRoot(false),
        mIsAutoDirRootContentRTL(false),
        mForceDisableApz(false),
        mResolutionUpdated(false),
        mIsRDMTouchSimulationActive(false),
        mDidContentGetPainted(true),
        mOverscrollBehavior() {}

  bool operator==(const ScrollMetadata& aOther) const {
    return mMetrics == aOther.mMetrics && mSnapInfo == aOther.mSnapInfo &&
           mScrollParentId == aOther.mScrollParentId &&
           mBackgroundColor == aOther.mBackgroundColor &&
           // don't compare mContentDescription
           mLineScrollAmount == aOther.mLineScrollAmount &&
           mPageScrollAmount == aOther.mPageScrollAmount &&
           mScrollClip == aOther.mScrollClip &&
           mHasScrollgrab == aOther.mHasScrollgrab &&
           mIsLayersIdRoot == aOther.mIsLayersIdRoot &&
           mIsAutoDirRootContentRTL == aOther.mIsAutoDirRootContentRTL &&
           mForceDisableApz == aOther.mForceDisableApz &&
           mResolutionUpdated == aOther.mResolutionUpdated &&
           mIsRDMTouchSimulationActive == aOther.mIsRDMTouchSimulationActive &&
           mDidContentGetPainted == aOther.mDidContentGetPainted &&
           mDisregardedDirection == aOther.mDisregardedDirection &&
           mOverscrollBehavior == aOther.mOverscrollBehavior &&
           mScrollUpdates == aOther.mScrollUpdates;
  }

  bool operator!=(const ScrollMetadata& aOther) const {
    return !operator==(aOther);
  }

  bool IsDefault() const {
    ScrollMetadata def;

    def.mMetrics.SetPresShellId(mMetrics.GetPresShellId());
    return (def == *this);
  }

  FrameMetrics& GetMetrics() { return mMetrics; }
  const FrameMetrics& GetMetrics() const { return mMetrics; }

  void SetSnapInfo(ScrollSnapInfo&& aSnapInfo) {
    mSnapInfo = std::move(aSnapInfo);
  }
  const ScrollSnapInfo& GetSnapInfo() const { return mSnapInfo; }

  ViewID GetScrollParentId() const { return mScrollParentId; }

  void SetScrollParentId(ViewID aParentId) { mScrollParentId = aParentId; }
  const gfx::DeviceColor& GetBackgroundColor() const {
    return mBackgroundColor;
  }
  void SetBackgroundColor(const gfx::sRGBColor& aBackgroundColor);
  const nsCString& GetContentDescription() const { return mContentDescription; }
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
  const Maybe<LayerClip>& GetScrollClip() const { return mScrollClip; }
  bool HasScrollClip() const { return mScrollClip.isSome(); }
  const LayerClip& ScrollClip() const { return mScrollClip.ref(); }
  LayerClip& ScrollClip() { return mScrollClip.ref(); }

  bool HasMaskLayer() const {
    return HasScrollClip() && ScrollClip().GetMaskLayerIndex();
  }
  Maybe<ParentLayerIntRect> GetClipRect() const {
    return mScrollClip.isSome() ? Some(mScrollClip->GetClipRect()) : Nothing();
  }

  void SetHasScrollgrab(bool aHasScrollgrab) {
    mHasScrollgrab = aHasScrollgrab;
  }
  bool GetHasScrollgrab() const { return mHasScrollgrab; }
  void SetIsLayersIdRoot(bool aValue) { mIsLayersIdRoot = aValue; }
  bool IsLayersIdRoot() const { return mIsLayersIdRoot; }
  void SetIsAutoDirRootContentRTL(bool aValue) {
    mIsAutoDirRootContentRTL = aValue;
  }
  bool IsAutoDirRootContentRTL() const { return mIsAutoDirRootContentRTL; }
  void SetForceDisableApz(bool aForceDisable) {
    mForceDisableApz = aForceDisable;
  }
  bool IsApzForceDisabled() const { return mForceDisableApz; }
  void SetResolutionUpdated(bool aUpdated) { mResolutionUpdated = aUpdated; }
  bool IsResolutionUpdated() const { return mResolutionUpdated; }

  void SetIsRDMTouchSimulationActive(bool aValue) {
    mIsRDMTouchSimulationActive = aValue;
  }
  bool GetIsRDMTouchSimulationActive() const {
    return mIsRDMTouchSimulationActive;
  }

  bool DidContentGetPainted() const { return mDidContentGetPainted; }

 private:
  // For use in IPC only
  void SetDidContentGetPainted(bool aValue) { mDidContentGetPainted = aValue; }

 public:
  // For more details about the concept of a disregarded direction, refer to the
  // code which defines mDisregardedDirection.
  Maybe<ScrollDirection> GetDisregardedDirection() const {
    return mDisregardedDirection;
  }
  void SetDisregardedDirection(const Maybe<ScrollDirection>& aValue) {
    mDisregardedDirection = aValue;
  }

  void SetOverscrollBehavior(
      const OverscrollBehaviorInfo& aOverscrollBehavior) {
    mOverscrollBehavior = aOverscrollBehavior;
  }
  const OverscrollBehaviorInfo& GetOverscrollBehavior() const {
    return mOverscrollBehavior;
  }

  void SetScrollUpdates(const nsTArray<ScrollPositionUpdate>& aUpdates) {
    mScrollUpdates = aUpdates;
  }

  const nsTArray<ScrollPositionUpdate>& GetScrollUpdates() const {
    return mScrollUpdates;
  }

  void UpdatePendingScrollInfo(nsTArray<ScrollPositionUpdate>&& aUpdates) {
    MOZ_ASSERT(!aUpdates.IsEmpty());
    mMetrics.UpdatePendingScrollInfo(aUpdates.LastElement());

    mDidContentGetPainted = false;
    mScrollUpdates.Clear();
    mScrollUpdates.AppendElements(std::move(aUpdates));
  }

 private:
  FrameMetrics mMetrics;

  // Information used to determine where to snap to for a given scroll.
  ScrollSnapInfo mSnapInfo;

  // The ViewID of the scrollable frame to which overscroll should be handed
  // off.
  ViewID mScrollParentId;

  // The background color to use when overscrolling.
  gfx::DeviceColor mBackgroundColor;

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
  bool mHasScrollgrab : 1;

  // Whether these framemetrics are for the root scroll frame (root element if
  // we don't have a root scroll frame) for its layers id.
  bool mIsLayersIdRoot : 1;

  // The AutoDirRootContent is the <body> element in an HTML document, or the
  // root scrollframe if there is no body. This member variable indicates
  // whether this element's content in the horizontal direction starts from
  // right to left (e.g. it's true either if "writing-mode: vertical-rl", or
  // "writing-mode: horizontal-tb; direction: rtl" in CSS).
  // When we do auto-dir scrolling (@see mozilla::WheelDeltaAdjustmentStrategy
  // or refer to bug 1358017 for details), setting a pref can make the code use
  // the writing mode of this root element instead of the target scrollframe,
  // and so we need to know if the writing mode is RTL or not.
  bool mIsAutoDirRootContentRTL : 1;

  // Whether or not the compositor should actually do APZ-scrolling on this
  // scrollframe.
  bool mForceDisableApz : 1;

  // Whether the pres shell resolution stored in mMetrics reflects a change
  // originated by the main thread.
  bool mResolutionUpdated : 1;

  // Whether or not RDM and touch simulation are active for this document.
  // It's important to note that if RDM is active then this field will be
  // true for the content document but NOT the chrome document containing
  // the browser UI and RDM controls.
  bool mIsRDMTouchSimulationActive : 1;

  // Whether this metadata is part of a transaction that also repainted the
  // content (i.e. updated the displaylist or textures). This gets set to false
  // for "paint-skip" transactions, where the main thread doesn't repaint but
  // instead requests APZ to update the compositor scroll offset instead. APZ
  // needs to be able to distinguish these paint-skip transactions so that it
  // can use the correct transforms.
  bool mDidContentGetPainted : 1;

  // The disregarded direction means the direction which is disregarded anyway,
  // even if the scroll frame overflows in that direction and the direction is
  // specified as scrollable. This could happen in some scenarios, for instance,
  // a single-line text control frame should disregard wheel scroll in
  // its block-flow direction even if it overflows in that direction.
  Maybe<ScrollDirection> mDisregardedDirection;

  // The overscroll behavior for this scroll frame.
  OverscrollBehaviorInfo mOverscrollBehavior;

  // The ordered list of scroll position updates for this scroll frame since
  // the last transaction.
  CopyableTArray<ScrollPositionUpdate> mScrollUpdates;

  // WARNING!!!!
  //
  // When adding new fields to ScrollMetadata, the following places should be
  // updated to include them (as needed):
  //    1. ScrollMetadata::operator ==
  //    2. AsyncPanZoomController::NotifyLayersUpdated
  //    3. The ParamTraits specialization in LayersMessageUtils.h
  //
  // Please add new fields above this comment.
};

typedef nsTHashMap<ScrollableLayerGuid::ViewIDHashKey,
                   nsTArray<ScrollPositionUpdate>>
    ScrollUpdatesMap;

}  // namespace layers
}  // namespace mozilla

#endif /* GFX_FRAMEMETRICS_H */
