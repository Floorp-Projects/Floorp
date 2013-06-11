/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_FRAMEMETRICS_H
#define GFX_FRAMEMETRICS_H

#include "gfxPoint.h"
#include "gfxTypes.h"
#include "nsRect.h"
#include "mozilla/gfx/Rect.h"
#include "Units.h"

namespace mozilla {
namespace layers {

/**
 * The viewport and displayport metrics for the painted frame at the
 * time of a layer-tree transaction.  These metrics are especially
 * useful for shadow layers, because the metrics values are updated
 * atomically with new pixels.
 */
struct FrameMetrics {
public:
  // We use IDs to identify frames across processes.
  typedef uint64_t ViewID;
  static const ViewID NULL_SCROLL_ID;   // This container layer does not scroll.
  static const ViewID ROOT_SCROLL_ID;   // This is the root scroll frame.
  static const ViewID START_SCROLL_ID;  // This is the ID that scrolling subframes
                                        // will begin at.

  FrameMetrics()
    : mCompositionBounds(0, 0, 0, 0)
    , mDisplayPort(0, 0, 0, 0)
    , mCriticalDisplayPort(0, 0, 0, 0)
    , mViewport(0, 0, 0, 0)
    , mScrollOffset(0, 0)
    , mScrollId(NULL_SCROLL_ID)
    , mScrollableRect(0, 0, 0, 0)
    , mResolution(1, 1)
    , mZoom(1, 1)
    , mDevPixelsPerCSSPixel(1)
    , mMayHaveTouchListeners(false)
    , mPresShellId(-1)
  {}

  // Default copy ctor and operator= are fine

  bool operator==(const FrameMetrics& aOther) const
  {
    return mCompositionBounds.IsEqualEdges(aOther.mCompositionBounds) &&
           mDisplayPort.IsEqualEdges(aOther.mDisplayPort) &&
           mCriticalDisplayPort.IsEqualEdges(aOther.mCriticalDisplayPort) &&
           mViewport.IsEqualEdges(aOther.mViewport) &&
           mScrollOffset == aOther.mScrollOffset &&
           mScrollId == aOther.mScrollId &&
           mScrollableRect.IsEqualEdges(aOther.mScrollableRect) &&
           mResolution == aOther.mResolution &&
           mDevPixelsPerCSSPixel == aOther.mDevPixelsPerCSSPixel &&
           mMayHaveTouchListeners == aOther.mMayHaveTouchListeners &&
           mPresShellId == aOther.mPresShellId;
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
    return mScrollId == ROOT_SCROLL_ID;
  }

  bool IsScrollable() const
  {
    return mScrollId != NULL_SCROLL_ID;
  }

  gfxSize LayersPixelsPerCSSPixel() const
  {
    return mResolution * mDevPixelsPerCSSPixel;
  }

  gfxPoint GetScrollOffsetInLayerPixels() const
  {
    return gfxPoint(
      static_cast<gfx::Float>(
        mScrollOffset.x * LayersPixelsPerCSSPixel().width),
      static_cast<gfx::Float>(
        mScrollOffset.y * LayersPixelsPerCSSPixel().height));
  }

  // ---------------------------------------------------------------------------
  // The following metrics are all in widget space/device pixels.
  //

  // This is the area within the widget that we're compositing to, which means
  // that it is the visible region of this frame. It is not relative to
  // anything.
  // So { 0, 0, [compositeArea.width], [compositeArea.height] }.
  //
  // This is useful because, on mobile, the viewport and composition dimensions
  // are not always the same. In this case, we calculate the displayport using
  // an area bigger than the region we're compositing to. If we used the
  // viewport dimensions to calculate the displayport, we'd run into situations
  // where we're prerendering the wrong regions and the content may be clipped,
  // or too much of it prerendered. If the displayport is the same as the
  // viewport, there is no need for this and we can just use the viewport
  // instead.
  //
  // This is only valid on the root layer. Nested iframes do not need this
  // metric as they do not have a displayport set. See bug 775452.
  LayerIntRect mCompositionBounds;

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
  //   width = window.innerWidth + 100, height = window.innerHeight + 100 }
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
  //   width = mCompositionBounds.x / mResolution.width,
  //   height = mCompositionBounds.y / mResolution.height }
  // Be within |mScrollableRect|.
  //
  // This is valid for any layer, but is always relative to this frame and
  // not any parents, regardless of parent transforms.
  CSSPoint mScrollOffset;

  // A unique ID assigned to each scrollable frame (unless this is
  // ROOT_SCROLL_ID, in which case it is not unique).
  ViewID mScrollId;

  // The scrollable bounds of a frame. This is determined by reflow.
  // For the top-level |window|,
  // { x = window.scrollX, y = window.scrollY, // could be 0, 0
  //   width = window.innerWidth, height = window.innerHeight }
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

  // The resolution, along both axes, that the current frame has been painted
  // at.
  //
  // Every time this frame is composited and the compositor samples its
  // transform, this metric is used to create a transform which is
  // post-multiplied into the parent's transform. Since this only happens when
  // we walk the layer tree, the resulting transform isn't stored here. Thus the
  // resolution of parent layers is opaque to this metric.
  gfxSize mResolution;

  // The resolution-independent "user zoom".  For example, if a page
  // configures the viewport to a zoom value of 2x, then this member
  // will always be 2.0 no matter what the viewport or composition
  // bounds.
  //
  // In the steady state (no animations), and ignoring DPI, then the
  // following is usually true
  //
  //  intrinsicScale = (mCompositionBounds / mViewport)
  //  mResolution = mZoom * intrinsicScale
  //
  // When this is not true, we're probably asynchronously sampling a
  // zoom animation for content.
  gfxSize mZoom;

  // The conversion factor between CSS pixels and device pixels for this frame.
  // This can vary based on a variety of things, such as reflowing-zoom. The
  // conversion factor for device pixels to layers pixels is just the
  // resolution.
  float mDevPixelsPerCSSPixel;

  // Whether or not this frame may have touch listeners.
  bool mMayHaveTouchListeners;

  uint32_t mPresShellId;
};

}
}

#endif /* GFX_FRAMEMETRICS_H */
