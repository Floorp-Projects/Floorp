/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_FRAMEMETRICS_H
#define GFX_FRAMEMETRICS_H

#include <stdint.h>                     // for uint32_t, uint64_t
#include <string>                       // for std::string
#include "Units.h"                      // for CSSRect, CSSPixel, etc
#include "mozilla/gfx/BasePoint.h"      // for BasePoint
#include "mozilla/gfx/Rect.h"           // for RoundedIn
#include "mozilla/gfx/ScaleFactor.h"    // for ScaleFactor
#include "mozilla/gfx/Logging.h"        // for Log

namespace IPC {
template <typename T> struct ParamTraits;
} // namespace IPC

namespace mozilla {
namespace layers {

// The layer coordinates of the parent layer. Like the layer coordinates
// of the current layer (LayerPixel) but doesn't include the current layer's
// resolution.
struct ParentLayerPixel {};

typedef gfx::ScaleFactor<LayoutDevicePixel, ParentLayerPixel> LayoutDeviceToParentLayerScale;
typedef gfx::ScaleFactor<ParentLayerPixel, LayerPixel> ParentLayerToLayerScale;

typedef gfx::ScaleFactor<ParentLayerPixel, ScreenPixel> ParentLayerToScreenScale;

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

  FrameMetrics()
    : mCompositionBounds(0, 0, 0, 0)
    , mDisplayPort(0, 0, 0, 0)
    , mCriticalDisplayPort(0, 0, 0, 0)
    , mViewport(0, 0, 0, 0)
    , mScrollOffset(0, 0)
    , mScrollId(NULL_SCROLL_ID)
    , mScrollableRect(0, 0, 0, 0)
    , mResolution(1)
    , mCumulativeResolution(1)
    , mZoom(1)
    , mDevPixelsPerCSSPixel(1)
    , mPresShellId(-1)
    , mMayHaveTouchListeners(false)
    , mIsRoot(false)
    , mHasScrollgrab(false)
    , mDisableScrollingX(false)
    , mDisableScrollingY(false)
    , mUpdateScrollOffset(false)
    , mScrollGeneration(0)
  {}

  // Default copy ctor and operator= are fine

  bool operator==(const FrameMetrics& aOther) const
  {
    // mContentDescription is not compared on purpose as it's only used
    // for debugging.
    return mCompositionBounds.IsEqualEdges(aOther.mCompositionBounds) &&
           mDisplayPort.IsEqualEdges(aOther.mDisplayPort) &&
           mCriticalDisplayPort.IsEqualEdges(aOther.mCriticalDisplayPort) &&
           mViewport.IsEqualEdges(aOther.mViewport) &&
           mScrollOffset == aOther.mScrollOffset &&
           mScrollId == aOther.mScrollId &&
           mScrollableRect.IsEqualEdges(aOther.mScrollableRect) &&
           mResolution == aOther.mResolution &&
           mCumulativeResolution == aOther.mCumulativeResolution &&
           mDevPixelsPerCSSPixel == aOther.mDevPixelsPerCSSPixel &&
           mMayHaveTouchListeners == aOther.mMayHaveTouchListeners &&
           mPresShellId == aOther.mPresShellId &&
           mIsRoot == aOther.mIsRoot &&
           mHasScrollgrab == aOther.mHasScrollgrab &&
           mDisableScrollingX == aOther.mDisableScrollingX &&
           mDisableScrollingY == aOther.mDisableScrollingY &&
           mUpdateScrollOffset == aOther.mUpdateScrollOffset;
  }
  bool operator!=(const FrameMetrics& aOther) const
  {
    return !operator==(aOther);
  }

  bool IsDefault() const
  {
    FrameMetrics def;

    def.mPresShellId = mPresShellId;
    return (def == *this);
  }

  bool IsRootScrollable() const
  {
    return mIsRoot;
  }

  bool IsScrollable() const
  {
    return mScrollId != NULL_SCROLL_ID;
  }

  CSSToLayerScale LayersPixelsPerCSSPixel() const
  {
    return mCumulativeResolution * mDevPixelsPerCSSPixel;
  }

  LayerPoint GetScrollOffsetInLayerPixels() const
  {
    return mScrollOffset * LayersPixelsPerCSSPixel();
  }

  LayoutDeviceToParentLayerScale GetParentResolution() const
  {
    return mCumulativeResolution / mResolution;
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
    CSSRect compBounds = CalculateCompositedRectInCssPixels();
    if (scrollableRect.width < compBounds.width) {
      scrollableRect.x = std::max(0.f,
                                  scrollableRect.x - (compBounds.width - scrollableRect.width));
      scrollableRect.width = compBounds.width;
    }

    if (scrollableRect.height < compBounds.height) {
      scrollableRect.y = std::max(0.f,
                                  scrollableRect.y - (compBounds.height - scrollableRect.height));
      scrollableRect.height = compBounds.height;
    }

    return scrollableRect;
  }

  /**
   * Return the scale factor needed to fit the viewport
   * into its composition bounds.
   */
  CSSToScreenScale CalculateIntrinsicScale() const
  {
    return CSSToScreenScale(float(mCompositionBounds.width) / float(mViewport.width));
  }

  CSSRect CalculateCompositedRectInCssPixels() const
  {
    return CSSRect(gfx::RoundedIn(mCompositionBounds / mZoom));
  }

  // ---------------------------------------------------------------------------
  // The following metrics are all in widget space/device pixels.
  //

  // This is the area within the widget that we're compositing to. It is relative
  // to the layer tree origin.
  //
  // This is useful because, on mobile, the viewport and composition dimensions
  // are not always the same. In this case, we calculate the displayport using
  // an area bigger than the region we're compositing to. If we used the
  // viewport dimensions to calculate the displayport, we'd run into situations
  // where we're prerendering the wrong regions and the content may be clipped,
  // or too much of it prerendered. If the composition dimensions are the same as the
  // viewport dimensions, there is no need for this and we can just use the viewport
  // instead.
  //
  // This value is valid for nested scrollable layers as well, and is still
  // relative to the layer tree origin. This value is provided by Gecko at
  // layout/paint time.
  ScreenIntRect mCompositionBounds;

  // ---------------------------------------------------------------------------
  // The following metrics are all in CSS pixels. They are not in any uniform
  // space, so each is explained separately.
  //

  // The area of a frame's contents that has been painted, relative to the
  // viewport. It is in the same coordinate space as |mViewport|. For example,
  // if it is at 0,0, then it's at the same place at the viewport, which is at
  // the top-left in the layer, and at the same place as the scroll offset of
  // the document.
  //
  // Note that this is structured in such a way that it doesn't depend on the
  // method layout uses to scroll content.
  //
  // May be larger or smaller than |mScrollableRect|.
  //
  // To pre-render a margin of 100 CSS pixels around the window,
  // { x = -100, y = - 100,
  //   width = window.innerWidth + 200, height = window.innerHeight + 200 }
  //
  // This is only valid on the root layer. Nested iframes do not have a
  // displayport set on them. See bug 775452.
  CSSRect mDisplayPort;

  // If non-empty, the area of a frame's contents that is considered critical
  // to paint. Area outside of this area (i.e. area inside mDisplayPort, but
  // outside of mCriticalDisplayPort) is considered low-priority, and may be
  // painted with lower precision, or not painted at all.
  //
  // The same restrictions for mDisplayPort apply here.
  CSSRect mCriticalDisplayPort;

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

  // A unique ID assigned to each scrollable frame.
  ViewID mScrollId;

  // The scrollable bounds of a frame. This is determined by reflow.
  // Ordinarily the x and y will be 0 and the width and height will be the
  // size of the element being scrolled. However for RTL pages or elements
  // the x value may be negative.
  //
  // This is relative to the document. It is in the same coordinate space as
  // |mScrollOffset|, but a different coordinate space than |mViewport| and
  // |mDisplayPort|. Note also that this coordinate system is understood by
  // window.scrollTo().
  //
  // This is valid on any layer unless it has no content.
  CSSRect mScrollableRect;

  // ---------------------------------------------------------------------------
  // The following metrics are dimensionless.
  //

  // The incremental resolution that the current frame has been painted at
  // relative to the parent frame's resolution. This information is provided
  // by Gecko at layout/paint time.
  ParentLayerToLayerScale mResolution;

  // The cumulative resolution that the current frame has been painted at.
  // This is the product of our mResolution and the mResolutions of our parent frames.
  // This information is provided by Gecko at layout/paint time.
  LayoutDeviceToLayerScale mCumulativeResolution;

  // The "user zoom". Content is painted by gecko at mResolution * mDevPixelsPerCSSPixel,
  // but will be drawn to the screen at mZoom. In the steady state, the
  // two will be the same, but during an async zoom action the two may
  // diverge. This information is initialized in Gecko but updated in the APZC.
  CSSToScreenScale mZoom;

  // The conversion factor between CSS pixels and device pixels for this frame.
  // This can vary based on a variety of things, such as reflowing-zoom. The
  // conversion factor for device pixels to layers pixels is just the
  // resolution.
  CSSToLayoutDeviceScale mDevPixelsPerCSSPixel;

  uint32_t mPresShellId;

  // Whether or not this frame may have touch listeners.
  bool mMayHaveTouchListeners;

  // Whether or not this is the root scroll frame for the root content document.
  bool mIsRoot;

  // Whether or not this frame is for an element marked 'scrollgrab'.
  bool mHasScrollgrab;

public:
  bool GetDisableScrollingX() const
  {
    return mDisableScrollingX;
  }

  void SetDisableScrollingX(bool aDisableScrollingX)
  {
    mDisableScrollingX = aDisableScrollingX;
  }

  bool GetDisableScrollingY() const
  {
    return mDisableScrollingY;
  }

  void SetDisableScrollingY(bool aDisableScrollingY)
  {
    mDisableScrollingY = aDisableScrollingY;
  }

  void SetScrollOffsetUpdated(uint32_t aScrollGeneration)
  {
    mUpdateScrollOffset = true;
    mScrollGeneration = aScrollGeneration;
  }

  bool GetScrollOffsetUpdated() const
  {
    return mUpdateScrollOffset;
  }

  uint32_t GetScrollGeneration() const
  {
    return mScrollGeneration;
  }

  const std::string& GetContentDescription() const
  {
    return mContentDescription;
  }

  void SetContentDescription(const std::string& aContentDescription)
  {
    mContentDescription = aContentDescription;
  }

private:
  // New fields from now on should be made private and old fields should
  // be refactored to be private.

  // Allow disabling scrolling in individual axis to support
  // |overflow: hidden|.
  bool mDisableScrollingX;
  bool mDisableScrollingY;

  // Whether mScrollOffset was updated by something other than the APZ code, and
  // if the APZC receiving this metrics should update its local copy.
  bool mUpdateScrollOffset;
  // The scroll generation counter used to acknowledge the scroll offset update.
  uint32_t mScrollGeneration;

  // A description of the content element corresponding to this frame.
  // This is empty unless the apz.printtree pref is turned on.
  std::string mContentDescription;
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
    MOZ_COUNT_CTOR(ScrollableLayerGuid);
  }

  ScrollableLayerGuid(uint64_t aLayersId, uint32_t aPresShellId,
                      FrameMetrics::ViewID aScrollId)
    : mLayersId(aLayersId)
    , mPresShellId(aPresShellId)
    , mScrollId(aScrollId)
  {
    MOZ_COUNT_CTOR(ScrollableLayerGuid);
  }

  ScrollableLayerGuid(uint64_t aLayersId, const FrameMetrics& aMetrics)
    : mLayersId(aLayersId)
    , mPresShellId(aMetrics.mPresShellId)
    , mScrollId(aMetrics.mScrollId)
  {
    MOZ_COUNT_CTOR(ScrollableLayerGuid);
  }

  ~ScrollableLayerGuid()
  {
    MOZ_COUNT_DTOR(ScrollableLayerGuid);
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
};

template <int LogLevel>
gfx::Log<LogLevel>& operator<<(gfx::Log<LogLevel>& log, const ScrollableLayerGuid& aGuid) {
  return log << '(' << aGuid.mLayersId << ',' << aGuid.mPresShellId << ',' << aGuid.mScrollId << ')';
}

struct ZoomConstraints {
  bool mAllowZoom;
  bool mAllowDoubleTapZoom;
  CSSToScreenScale mMinZoom;
  CSSToScreenScale mMaxZoom;

  ZoomConstraints()
    : mAllowZoom(true)
    , mAllowDoubleTapZoom(true)
  {
    MOZ_COUNT_CTOR(ZoomConstraints);
  }

  ZoomConstraints(bool aAllowZoom,
                  bool aAllowDoubleTapZoom,
                  const CSSToScreenScale& aMinZoom,
                  const CSSToScreenScale& aMaxZoom)
    : mAllowZoom(aAllowZoom)
    , mAllowDoubleTapZoom(aAllowDoubleTapZoom)
    , mMinZoom(aMinZoom)
    , mMaxZoom(aMaxZoom)
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

}
}

#endif /* GFX_FRAMEMETRICS_H */
