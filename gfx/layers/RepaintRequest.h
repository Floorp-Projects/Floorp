/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_REPAINTREQUEST_H
#define GFX_REPAINTREQUEST_H

#include <iosfwd>
#include <stdint.h>  // for uint8_t, uint32_t, uint64_t

#include "FrameMetrics.h"                // for FrameMetrics
#include "mozilla/DefineEnum.h"          // for MOZ_DEFINE_ENUM
#include "mozilla/gfx/BasePoint.h"       // for BasePoint
#include "mozilla/gfx/Rect.h"            // for RoundedIn
#include "mozilla/gfx/ScaleFactor.h"     // for ScaleFactor
#include "mozilla/ScrollSnapTargetId.h"  // for ScrollSnapTargetIds
#include "mozilla/TimeStamp.h"           // for TimeStamp
#include "Units.h"                       // for CSSRect, CSSPixel, etc
#include "UnitTransforms.h"              // for ViewAs

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
        mDisplayPortMargins(0, 0, 0, 0),
        mPresShellId(-1),
        mLayoutViewport(0, 0, 0, 0),
        mTransformToAncestorScale(),
        mPaintRequestTime(),
        mScrollUpdateType(eNone),
        mScrollAnimationType(APZScrollAnimationType::No),
        mIsRootContent(false),
        mIsScrollInfoLayer(false),
        mIsInScrollingGesture(false) {}

  RepaintRequest(const FrameMetrics& aOther,
                 const ScreenMargin& aDisplayportMargins,
                 const ScrollOffsetUpdateType aScrollUpdateType,
                 APZScrollAnimationType aScrollAnimationType,
                 const APZScrollGeneration& aScrollGenerationOnApz,
                 const ScrollSnapTargetIds& aLastSnapTargetIds,
                 bool aIsInScrollingGesture)
      : mScrollId(aOther.GetScrollId()),
        mPresShellResolution(aOther.GetPresShellResolution()),
        mCompositionBounds(aOther.GetCompositionBounds()),
        mCumulativeResolution(aOther.GetCumulativeResolution()),
        mDevPixelsPerCSSPixel(aOther.GetDevPixelsPerCSSPixel()),
        mScrollOffset(aOther.GetVisualScrollOffset()),
        mZoom(aOther.GetZoom()),
        mScrollGeneration(aOther.GetScrollGeneration()),
        mScrollGenerationOnApz(aScrollGenerationOnApz),
        mDisplayPortMargins(aDisplayportMargins),
        mPresShellId(aOther.GetPresShellId()),
        mLayoutViewport(aOther.GetLayoutViewport()),
        mTransformToAncestorScale(aOther.GetTransformToAncestorScale()),
        mPaintRequestTime(aOther.GetPaintRequestTime()),
        mScrollUpdateType(aScrollUpdateType),
        mScrollAnimationType(aScrollAnimationType),
        mLastSnapTargetIds(aLastSnapTargetIds),
        mIsRootContent(aOther.IsRootContent()),
        mIsScrollInfoLayer(aOther.IsScrollInfoLayer()),
        mIsInScrollingGesture(aIsInScrollingGesture) {}

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
           mTransformToAncestorScale == aOther.mTransformToAncestorScale &&
           mPaintRequestTime == aOther.mPaintRequestTime &&
           mScrollUpdateType == aOther.mScrollUpdateType &&
           mScrollAnimationType == aOther.mScrollAnimationType &&
           mLastSnapTargetIds == aOther.mLastSnapTargetIds &&
           mIsRootContent == aOther.mIsRootContent &&
           mIsScrollInfoLayer == aOther.mIsScrollInfoLayer &&
           mIsInScrollingGesture == aOther.mIsInScrollingGesture;
  }

  bool operator!=(const RepaintRequest& aOther) const {
    return !operator==(aOther);
  }

  friend std::ostream& operator<<(std::ostream& aOut,
                                  const RepaintRequest& aRequest);

  CSSToScreenScale2D DisplayportPixelsPerCSSPixel() const {
    // Refer to FrameMetrics::DisplayportPixelsPerCSSPixel() for explanation.
    return mZoom * mTransformToAncestorScale;
  }

  CSSToLayerScale LayersPixelsPerCSSPixel() const {
    return mDevPixelsPerCSSPixel * mCumulativeResolution;
  }

  // Get the amount by which this frame has been zoomed since the last repaint.
  LayerToParentLayerScale GetAsyncZoom() const {
    return mZoom / LayersPixelsPerCSSPixel();
  }

  CSSSize CalculateCompositedSizeInCssPixels() const {
    if (GetZoom() == CSSToParentLayerScale(0)) {
      return CSSSize();  // avoid division by zero
    }
    return mCompositionBounds.Size() / GetZoom();
  }

  float GetPresShellResolution() const { return mPresShellResolution; }

  const ParentLayerRect& GetCompositionBounds() const {
    return mCompositionBounds;
  }

  const LayoutDeviceToLayerScale& GetCumulativeResolution() const {
    return mCumulativeResolution;
  }

  const CSSToLayoutDeviceScale& GetDevPixelsPerCSSPixel() const {
    return mDevPixelsPerCSSPixel;
  }

  bool IsAnimationInProgress() const {
    return mScrollAnimationType != APZScrollAnimationType::No;
  }

  bool IsRootContent() const { return mIsRootContent; }

  CSSPoint GetLayoutScrollOffset() const { return mLayoutViewport.TopLeft(); }

  const CSSPoint& GetVisualScrollOffset() const { return mScrollOffset; }

  const CSSToParentLayerScale& GetZoom() const { return mZoom; }

  ScrollOffsetUpdateType GetScrollUpdateType() const {
    return mScrollUpdateType;
  }

  bool GetScrollOffsetUpdated() const { return mScrollUpdateType != eNone; }

  MainThreadScrollGeneration GetScrollGeneration() const {
    return mScrollGeneration;
  }

  APZScrollGeneration GetScrollGenerationOnApz() const {
    return mScrollGenerationOnApz;
  }

  ScrollableLayerGuid::ViewID GetScrollId() const { return mScrollId; }

  const ScreenMargin& GetDisplayPortMargins() const {
    return mDisplayPortMargins;
  }

  uint32_t GetPresShellId() const { return mPresShellId; }

  const CSSRect& GetLayoutViewport() const { return mLayoutViewport; }

  const ParentLayerToScreenScale2D& GetTransformToAncestorScale() const {
    return mTransformToAncestorScale;
  }

  const TimeStamp& GetPaintRequestTime() const { return mPaintRequestTime; }

  bool IsScrollInfoLayer() const { return mIsScrollInfoLayer; }

  bool IsInScrollingGesture() const { return mIsInScrollingGesture; }

  APZScrollAnimationType GetScrollAnimationType() const {
    return mScrollAnimationType;
  }

  const ScrollSnapTargetIds& GetLastSnapTargetIds() const {
    return mLastSnapTargetIds;
  }

 protected:
  void SetIsRootContent(bool aIsRootContent) {
    mIsRootContent = aIsRootContent;
  }

  void SetIsScrollInfoLayer(bool aIsScrollInfoLayer) {
    mIsScrollInfoLayer = aIsScrollInfoLayer;
  }

  void SetIsInScrollingGesture(bool aIsInScrollingGesture) {
    mIsInScrollingGesture = aIsInScrollingGesture;
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

  // See FrameMetrics::mCumulativeResolution for description.
  LayoutDeviceToLayerScale mCumulativeResolution;

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
  CSSToParentLayerScale mZoom;

  // The scroll generation counter used to acknowledge the scroll offset update
  // on the main-thread.
  MainThreadScrollGeneration mScrollGeneration;

  // The scroll generation counter stored in each SampledAPZState and the
  // scrollable frame on the main-thread and used to compare with each other
  // in the WebRender renderer thread to tell which sampled scroll offset
  // matches the scroll offset used on the main-thread.
  APZScrollGeneration mScrollGenerationOnApz;

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

  // See FrameMetrics::mTransformToAncestorScale for description.
  ParentLayerToScreenScale2D mTransformToAncestorScale;

  // The time at which the APZC last requested a repaint for this scroll frame.
  TimeStamp mPaintRequestTime;

  // The type of repaint request this represents.
  ScrollOffsetUpdateType mScrollUpdateType;

  APZScrollAnimationType mScrollAnimationType;

  ScrollSnapTargetIds mLastSnapTargetIds;

  // Whether or not this is the root scroll frame for the root content document.
  bool mIsRootContent : 1;

  // True if this scroll frame is a scroll info layer. A scroll info layer is
  // not layerized and its content cannot be truly async-scrolled, but its
  // metrics are still sent to and updated by the compositor, with the updates
  // being reflected on the next paint rather than the next composite.
  bool mIsScrollInfoLayer : 1;

  // Whether the APZC is in the middle of processing a gesture.
  bool mIsInScrollingGesture : 1;
};

}  // namespace layers
}  // namespace mozilla

#endif /* GFX_REPAINTREQUEST_H */
