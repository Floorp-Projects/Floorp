/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_REPAINTREQUEST_H
#define GFX_REPAINTREQUEST_H

#include <stdint.h>  // for uint8_t, uint32_t, uint64_t

#include "FrameMetrics.h"             // for FrameMetrics
#include "mozilla/DefineEnum.h"       // for MOZ_DEFINE_ENUM
#include "mozilla/gfx/BasePoint.h"    // for BasePoint
#include "mozilla/gfx/Rect.h"         // for RoundedIn
#include "mozilla/gfx/ScaleFactor.h"  // for ScaleFactor
#include "mozilla/TimeStamp.h"        // for TimeStamp
#include "Units.h"                    // for CSSRect, CSSPixel, etc

namespace IPC {
template <typename T>
struct ParamTraits;
}  // namespace IPC

namespace mozilla {
namespace layers {

struct RepaintRequest {
  friend struct IPC::ParamTraits<mozilla::layers::RepaintRequest>;

 public:
  // clang-format off
  MOZ_DEFINE_ENUM_WITH_BASE_AT_CLASS_SCOPE(
    ScrollOffsetUpdateType, uint8_t, (
        eNone,             // The default; the scroll offset was not updated.
        eUserAction,       // The scroll offset was updated by APZ in response
                           // to user action.
        eVisualUpdate      // The scroll offset was updated by APZ in response
                           // to a visual scroll update request from the
                           // main thread.
  ));
  // clang-format on

  RepaintRequest()
      : mScrollId(ScrollableLayerGuid::NULL_SCROLL_ID),
        mPresShellResolution(1),
        mCompositionBounds(0, 0, 0, 0),
        mCumulativeResolution(),
        mDevPixelsPerCSSPixel(1),
        mScrollOffset(0, 0),
        mZoom(),
        mScrollGeneration(0),
        mDisplayPortMargins(0, 0, 0, 0),
        mPresShellId(-1),
        mLayoutViewport(0, 0, 0, 0),
        mExtraResolution(),
        mPaintRequestTime(),
        mScrollUpdateType(eNone),
        mIsRootContent(false),
        mIsAnimationInProgress(false),
        mIsScrollInfoLayer(false) {}

  RepaintRequest(const FrameMetrics& aOther,
                 const ScrollOffsetUpdateType aScrollUpdateType,
                 bool aIsAnimationInProgress)
      : mScrollId(aOther.GetScrollId()),
        mPresShellResolution(aOther.GetPresShellResolution()),
        mCompositionBounds(aOther.GetCompositionBounds()),
        mCumulativeResolution(aOther.GetCumulativeResolution()),
        mDevPixelsPerCSSPixel(aOther.GetDevPixelsPerCSSPixel()),
        mScrollOffset(aOther.GetVisualScrollOffset()),
        mZoom(aOther.GetZoom()),
        mScrollGeneration(aOther.GetScrollGeneration()),
        mDisplayPortMargins(aOther.GetDisplayPortMargins()),
        mPresShellId(aOther.GetPresShellId()),
        mLayoutViewport(aOther.GetLayoutViewport()),
        mExtraResolution(aOther.GetExtraResolution()),
        mPaintRequestTime(aOther.GetPaintRequestTime()),
        mScrollUpdateType(aScrollUpdateType),
        mIsRootContent(aOther.IsRootContent()),
        mIsAnimationInProgress(aIsAnimationInProgress),
        mIsScrollInfoLayer(aOther.IsScrollInfoLayer()) {}

  // Default copy ctor and operator= are fine

  bool operator==(const RepaintRequest& aOther) const {
    // Put mScrollId at the top since it's the most likely one to fail.
    return mScrollId == aOther.mScrollId &&
           mPresShellResolution == aOther.mPresShellResolution &&
           mCompositionBounds.IsEqualEdges(aOther.mCompositionBounds) &&
           mCumulativeResolution == aOther.mCumulativeResolution &&
           mDevPixelsPerCSSPixel == aOther.mDevPixelsPerCSSPixel &&
           mScrollOffset == aOther.mScrollOffset &&
           // don't compare mZoom
           mScrollGeneration == aOther.mScrollGeneration &&
           mDisplayPortMargins == aOther.mDisplayPortMargins &&
           mPresShellId == aOther.mPresShellId &&
           mLayoutViewport.IsEqualEdges(aOther.mLayoutViewport) &&
           mExtraResolution == aOther.mExtraResolution &&
           mPaintRequestTime == aOther.mPaintRequestTime &&
           mScrollUpdateType == aOther.mScrollUpdateType &&
           mIsRootContent == aOther.mIsRootContent &&
           mIsAnimationInProgress == aOther.mIsAnimationInProgress &&
           mIsScrollInfoLayer == aOther.mIsScrollInfoLayer;
  }

  bool operator!=(const RepaintRequest& aOther) const {
    return !operator==(aOther);
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

  CSSSize CalculateCompositedSizeInCssPixels() const {
    if (GetZoom() == CSSToParentLayerScale2D(0, 0)) {
      return CSSSize();  // avoid division by zero
    }
    return mCompositionBounds.Size() / GetZoom();
  }

  float GetPresShellResolution() const { return mPresShellResolution; }

  const ParentLayerRect& GetCompositionBounds() const {
    return mCompositionBounds;
  }

  const LayoutDeviceToLayerScale2D& GetCumulativeResolution() const {
    return mCumulativeResolution;
  }

  const CSSToLayoutDeviceScale& GetDevPixelsPerCSSPixel() const {
    return mDevPixelsPerCSSPixel;
  }

  bool IsAnimationInProgress() const { return mIsAnimationInProgress; }

  bool IsRootContent() const { return mIsRootContent; }

  CSSPoint GetLayoutScrollOffset() const { return mLayoutViewport.TopLeft(); }

  const CSSPoint& GetVisualScrollOffset() const { return mScrollOffset; }

  const CSSToParentLayerScale2D& GetZoom() const { return mZoom; }

  ScrollOffsetUpdateType GetScrollUpdateType() const {
    return mScrollUpdateType;
  }

  bool GetScrollOffsetUpdated() const { return mScrollUpdateType != eNone; }

  uint32_t GetScrollGeneration() const { return mScrollGeneration; }

  ScrollableLayerGuid::ViewID GetScrollId() const { return mScrollId; }

  const ScreenMargin& GetDisplayPortMargins() const {
    return mDisplayPortMargins;
  }

  uint32_t GetPresShellId() const { return mPresShellId; }

  const CSSRect& GetLayoutViewport() const { return mLayoutViewport; }

  const ScreenToLayerScale2D& GetExtraResolution() const {
    return mExtraResolution;
  }

  const TimeStamp& GetPaintRequestTime() const { return mPaintRequestTime; }

  bool IsScrollInfoLayer() const { return mIsScrollInfoLayer; }

 protected:
  void SetIsAnimationInProgress(bool aInProgress) {
    mIsAnimationInProgress = aInProgress;
  }

  void SetIsRootContent(bool aIsRootContent) {
    mIsRootContent = aIsRootContent;
  }

  void SetIsScrollInfoLayer(bool aIsScrollInfoLayer) {
    mIsScrollInfoLayer = aIsScrollInfoLayer;
  }

 private:
  // A ID assigned to each scrollable frame, unique within each LayersId..
  ScrollableLayerGuid::ViewID mScrollId;

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
  CSSPoint mScrollOffset;

  // The "user zoom". Content is painted by gecko at mCumulativeResolution *
  // mDevPixelsPerCSSPixel, but will be drawn to the screen at mZoom. In the
  // steady state, the two will be the same, but during an async zoom action the
  // two may diverge. This information is initialized in Gecko but updated in
  // the APZC.
  CSSToParentLayerScale2D mZoom;

  // The scroll generation counter used to acknowledge the scroll offset update.
  uint32_t mScrollGeneration;

  // A display port expressed as layer margins that apply to the rect of what
  // is drawn of the scrollable element.
  ScreenMargin mDisplayPortMargins;

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

  // The type of repaint request this represents.
  ScrollOffsetUpdateType mScrollUpdateType;

  // Whether or not this is the root scroll frame for the root content document.
  bool mIsRootContent : 1;

  // Whether or not we are in the middle of a scroll animation.
  bool mIsAnimationInProgress : 1;

  // True if this scroll frame is a scroll info layer. A scroll info layer is
  // not layerized and its content cannot be truly async-scrolled, but its
  // metrics are still sent to and updated by the compositor, with the updates
  // being reflected on the next paint rather than the next composite.
  bool mIsScrollInfoLayer : 1;
};

}  // namespace layers
}  // namespace mozilla

#endif /* GFX_REPAINTREQUEST_H */
