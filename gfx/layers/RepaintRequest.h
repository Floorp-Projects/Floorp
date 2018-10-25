/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_REPAINTREQUEST_H
#define GFX_REPAINTREQUEST_H

#include <stdint.h>                     // for uint8_t, uint32_t, uint64_t

#include "FrameMetrics.h"               // for FrameMetrics
#include "mozilla/DefineEnum.h"         // for MOZ_DEFINE_ENUM
#include "mozilla/gfx/BasePoint.h"      // for BasePoint
#include "mozilla/gfx/Rect.h"           // for RoundedIn
#include "mozilla/gfx/ScaleFactor.h"    // for ScaleFactor
#include "mozilla/TimeStamp.h"          // for TimeStamp
#include "Units.h"                      // for CSSRect, CSSPixel, etc

namespace IPC {
template <typename T> struct ParamTraits;
} // namespace IPC

namespace mozilla {
namespace layers {

struct RepaintRequest {
  friend struct IPC::ParamTraits<mozilla::layers::RepaintRequest>;
public:

  MOZ_DEFINE_ENUM_WITH_BASE_AT_CLASS_SCOPE(
    ScrollOffsetUpdateType, uint8_t, (
      eNone,             // The default; the scroll offset was not updated.
      eUserAction        // The scroll offset was updated by APZ.
  ));

  RepaintRequest()
    : mScrollId(FrameMetrics::NULL_SCROLL_ID)
    , mPresShellResolution(1)
    , mCompositionBounds(0, 0, 0, 0)
    , mCumulativeResolution()
    , mDevPixelsPerCSSPixel(1)
    , mScrollOffset(0, 0)
    , mZoom()
    , mScrollGeneration(0)
    , mDisplayPortMargins(0, 0, 0, 0)
    , mPresShellId(-1)
    , mViewport(0, 0, 0, 0)
    , mExtraResolution()
    , mPaintRequestTime()
    , mScrollUpdateType(eNone)
    , mIsRootContent(false)
    , mUseDisplayPortMargins(false)
    , mIsScrollInfoLayer(false)
  {
  }

  RepaintRequest(const FrameMetrics& aOther, const ScrollOffsetUpdateType aScrollUpdateType)
    : mScrollId(aOther.GetScrollId())
    , mPresShellResolution(aOther.GetPresShellResolution())
    , mCompositionBounds(aOther.GetCompositionBounds())
    , mCumulativeResolution(aOther.GetCumulativeResolution())
    , mDevPixelsPerCSSPixel(aOther.GetDevPixelsPerCSSPixel())
    , mScrollOffset(aOther.GetScrollOffset())
    , mZoom(aOther.GetZoom())
    , mScrollGeneration(aOther.GetScrollGeneration())
    , mDisplayPortMargins(aOther.GetDisplayPortMargins())
    , mPresShellId(aOther.GetPresShellId())
    , mViewport(aOther.GetViewport())
    , mExtraResolution(aOther.GetExtraResolution())
    , mPaintRequestTime(aOther.GetPaintRequestTime())
    , mScrollUpdateType(aScrollUpdateType)
    , mIsRootContent(aOther.IsRootContent())
    , mUseDisplayPortMargins(aOther.GetUseDisplayPortMargins())
    , mIsScrollInfoLayer(aOther.IsScrollInfoLayer())
  {
  }

  // Default copy ctor and operator= are fine

  bool operator==(const RepaintRequest& aOther) const
  {
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
           mViewport.IsEqualEdges(aOther.mViewport) &&
           mExtraResolution == aOther.mExtraResolution &&
           mPaintRequestTime == aOther.mPaintRequestTime &&
           mScrollUpdateType == aOther.mScrollUpdateType &&
           mIsRootContent == aOther.mIsRootContent &&
           mUseDisplayPortMargins == aOther.mUseDisplayPortMargins &&
           mIsScrollInfoLayer == aOther.mIsScrollInfoLayer;
  }

  bool operator!=(const RepaintRequest& aOther) const
  {
    return !operator==(aOther);
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

  CSSSize CalculateCompositedSizeInCssPixels() const
  {
    if (GetZoom() == CSSToParentLayerScale2D(0, 0)) {
      return CSSSize();  // avoid division by zero
    }
    return mCompositionBounds.Size() / GetZoom();
  }

  float GetPresShellResolution() const
  {
    return mPresShellResolution;
  }

  const ParentLayerRect& GetCompositionBounds() const
  {
    return mCompositionBounds;
  }

  const LayoutDeviceToLayerScale2D& GetCumulativeResolution() const
  {
    return mCumulativeResolution;
  }

  const CSSToLayoutDeviceScale& GetDevPixelsPerCSSPixel() const
  {
    return mDevPixelsPerCSSPixel;
  }

  bool IsRootContent() const
  {
    return mIsRootContent;
  }

  const CSSPoint& GetScrollOffset() const
  {
    return mScrollOffset;
  }

  const CSSToParentLayerScale2D& GetZoom() const
  {
    return mZoom;
  }

  ScrollOffsetUpdateType GetScrollUpdateType() const
  {
    return mScrollUpdateType;
  }

  bool GetScrollOffsetUpdated() const
  {
    return mScrollUpdateType != eNone;
  }

  uint32_t GetScrollGeneration() const
  {
    return mScrollGeneration;
  }

  FrameMetrics::ViewID GetScrollId() const
  {
    return mScrollId;
  }

  const ScreenMargin& GetDisplayPortMargins() const
  {
    return mDisplayPortMargins;
  }

  bool GetUseDisplayPortMargins() const
  {
    return mUseDisplayPortMargins;
  }

  uint32_t GetPresShellId() const
  {
    return mPresShellId;
  }

  const CSSRect& GetViewport() const
  {
    return mViewport;
  }

  const ScreenToLayerScale2D& GetExtraResolution() const
  {
    return mExtraResolution;
  }

  const TimeStamp& GetPaintRequestTime() const {
    return mPaintRequestTime;
  }

  bool IsScrollInfoLayer() const {
    return mIsScrollInfoLayer;
  }

protected:
  void SetIsRootContent(bool aIsRootContent)
  {
    mIsRootContent = aIsRootContent;
  }

  void SetUseDisplayPortMargins(bool aValue)
  {
    mUseDisplayPortMargins = aValue;
  }

  void SetIsScrollInfoLayer(bool aIsScrollInfoLayer)
  {
    mIsScrollInfoLayer = aIsScrollInfoLayer;
  }

private:
  // A unique ID assigned to each scrollable frame.
  FrameMetrics::ViewID mScrollId;

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

  // The cumulative resolution that the current frame has been painted at.
  // This is the product of the pres-shell resolutions of the document
  // containing this scroll frame and its ancestors, and any css-driven
  // resolution. This information is provided by Gecko at layout/paint time.
  // Note that this is allowed to have different x- and y-scales, but only
  // for subframes (mIsRoot = false). (The same applies to other scales that
  // "inherit" the 2D-ness of this one, such as mZoom.)
  LayoutDeviceToLayerScale2D mCumulativeResolution;

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

  // The type of repaint request this represents.
  ScrollOffsetUpdateType mScrollUpdateType;

  // Whether or not this is the root scroll frame for the root content document.
  bool mIsRootContent:1;

  // If this is true then we use the display port margins on this metrics,
  // otherwise use the display port rect.
  bool mUseDisplayPortMargins:1;

  // Whether or not this frame has a "scroll info layer" to capture events.
  bool mIsScrollInfoLayer:1;
};

} // namespace layers
} // namespace mozilla

#endif /* GFX_REPAINTREQUEST_H */
