/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_FRAMEMETRICS_H
#define GFX_FRAMEMETRICS_H

#include <stdint.h>                     // for uint32_t, uint64_t
#include "Units.h"                      // for CSSRect, CSSPixel, etc
#include "mozilla/gfx/BasePoint.h"      // for BasePoint
#include "mozilla/gfx/Rect.h"           // for RoundedIn
#include "mozilla/gfx/ScaleFactor.h"    // for ScaleFactor
#include "mozilla/gfx/Logging.h"        // for Log
#include "gfxColor.h"
#include "nsString.h"

namespace IPC {
template <typename T> struct ParamTraits;
} // namespace IPC

namespace mozilla {
namespace layers {

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
  static const FrameMetrics sNullMetrics;   // We often need an empty metrics

  FrameMetrics()
    : mCompositionBounds(0, 0, 0, 0)
    , mDisplayPort(0, 0, 0, 0)
    , mCriticalDisplayPort(0, 0, 0, 0)
    , mScrollableRect(0, 0, 0, 0)
    , mPresShellResolution(1)
    , mCumulativeResolution(1)
    , mDevPixelsPerCSSPixel(1)
    , mMayHaveTouchListeners(false)
    , mMayHaveTouchCaret(false)
    , mIsRoot(false)
    , mHasScrollgrab(false)
    , mScrollId(NULL_SCROLL_ID)
    , mScrollParentId(NULL_SCROLL_ID)
    , mScrollOffset(0, 0)
    , mZoom(1)
    , mUpdateScrollOffset(false)
    , mScrollGeneration(0)
    , mDoSmoothScroll(false)
    , mSmoothScrollOffset(0, 0)
    , mRootCompositionSize(0, 0)
    , mDisplayPortMargins(0, 0, 0, 0)
    , mUseDisplayPortMargins(false)
    , mPresShellId(-1)
    , mViewport(0, 0, 0, 0)
    , mExtraResolution(1)
    , mBackgroundColor(0, 0, 0, 0)
    , mLineScrollAmount(0, 0)
  {
  }

  // Default copy ctor and operator= are fine

  bool operator==(const FrameMetrics& aOther) const
  {
    return mCompositionBounds.IsEqualEdges(aOther.mCompositionBounds) &&
           mRootCompositionSize == aOther.mRootCompositionSize &&
           mDisplayPort.IsEqualEdges(aOther.mDisplayPort) &&
           mDisplayPortMargins == aOther.mDisplayPortMargins &&
           mUseDisplayPortMargins == aOther.mUseDisplayPortMargins &&
           mCriticalDisplayPort.IsEqualEdges(aOther.mCriticalDisplayPort) &&
           mViewport.IsEqualEdges(aOther.mViewport) &&
           mScrollableRect.IsEqualEdges(aOther.mScrollableRect) &&
           mPresShellResolution == aOther.mPresShellResolution &&
           mCumulativeResolution == aOther.mCumulativeResolution &&
           mDevPixelsPerCSSPixel == aOther.mDevPixelsPerCSSPixel &&
           mMayHaveTouchListeners == aOther.mMayHaveTouchListeners &&
           mMayHaveTouchCaret == aOther.mMayHaveTouchCaret &&
           mPresShellId == aOther.mPresShellId &&
           mIsRoot == aOther.mIsRoot &&
           mScrollId == aOther.mScrollId &&
           mScrollParentId == aOther.mScrollParentId &&
           mScrollOffset == aOther.mScrollOffset &&
           mSmoothScrollOffset == aOther.mSmoothScrollOffset &&
           mHasScrollgrab == aOther.mHasScrollgrab &&
           mUpdateScrollOffset == aOther.mUpdateScrollOffset &&
           mExtraResolution == aOther.mExtraResolution &&
           mBackgroundColor == aOther.mBackgroundColor &&
           mDoSmoothScroll == aOther.mDoSmoothScroll &&
           mLineScrollAmount == aOther.mLineScrollAmount;
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

  CSSToScreenScale DisplayportPixelsPerCSSPixel() const
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

  CSSToLayerScale LayersPixelsPerCSSPixel() const
  {
    return mCumulativeResolution * mDevPixelsPerCSSPixel;
  }

  // Get the amount by which this frame has been zoomed since the last repaint.
  LayerToParentLayerScale GetAsyncZoom() const
  {
    return mZoom / LayersPixelsPerCSSPixel();
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

  // Return the scale factor needed to fit the viewport
  // into its composition bounds.
  CSSToParentLayerScale CalculateIntrinsicScale() const
  {
    return CSSToParentLayerScale(
        std::max(mCompositionBounds.width / mViewport.width,
                 mCompositionBounds.height / mViewport.height));
  }

  CSSSize CalculateCompositedSizeInCssPixels() const
  {
    return mCompositionBounds.Size() / GetZoom();
  }

  CSSRect CalculateCompositedRectInCssPixels() const
  {
    return mCompositionBounds / GetZoom();
  }

  CSSSize CalculateBoundedCompositedSizeInCssPixels() const
  {
    CSSSize size = CalculateCompositedSizeInCssPixels();
    size.width = std::min(size.width, mRootCompositionSize.width);
    size.height = std::min(size.height, mRootCompositionSize.height);
    return size;
  }

  void ScrollBy(const CSSPoint& aPoint)
  {
    mScrollOffset += aPoint;
  }

  void ZoomBy(float aFactor)
  {
    mZoom.scale *= aFactor;
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

  // Make a copy of this FrameMetrics object which does not have any pointers
  // to heap-allocated memory (i.e. is Plain Old Data, or 'POD'), and is
  // therefore safe to be placed into shared memory.
  FrameMetrics MakePODObject() const
  {
    FrameMetrics copy = *this;
    copy.mContentDescription.Truncate();
    return copy;
  }

  // ---------------------------------------------------------------------------
  // The following metrics are all in widget space/device pixels.
  //

  // This is the area within the widget that we're compositing to. It is in the
  // same coordinate space as the reference frame for the scrolled frame.
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
  ParentLayerRect mCompositionBounds;

  // ---------------------------------------------------------------------------
  // The following metrics are all in CSS pixels. They are not in any uniform
  // space, so each is explained separately.
  //

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

  // The pres-shell resolution that has been induced on the document containing
  // this scroll frame as a result of zooming this scroll frame (whether via
  // user action, or choosing an initial zoom level on page load). This can
  // only be different from 1.0 for frames that are zoomable, which currently
  // is just the root content document's root scroll frame (mIsRoot = true).
  // This is a plain float rather than a ScaleFactor because in and of itself
  // it does not convert between any coordinate spaces for which we have names.
  float mPresShellResolution;

public:
  void SetCumulativeResolution(const LayoutDeviceToLayerScale& aCumulativeResolution)
  {
    mCumulativeResolution = aCumulativeResolution;
  }

  LayoutDeviceToLayerScale GetCumulativeResolution() const
  {
    return mCumulativeResolution;
  }

  void SetDevPixelsPerCSSPixel(const CSSToLayoutDeviceScale& aDevPixelsPerCSSPixel)
  {
    mDevPixelsPerCSSPixel = aDevPixelsPerCSSPixel;
  }

  CSSToLayoutDeviceScale GetDevPixelsPerCSSPixel() const
  {
    return mDevPixelsPerCSSPixel;
  }

  void SetIsRoot(bool aIsRoot)
  {
    mIsRoot = aIsRoot;
  }

  bool GetIsRoot() const
  {
    return mIsRoot;
  }

  void SetHasScrollgrab(bool aHasScrollgrab)
  {
    mHasScrollgrab = aHasScrollgrab;
  }

  bool GetHasScrollgrab() const
  {
    return mHasScrollgrab;
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

  void SetZoom(const CSSToParentLayerScale& aZoom)
  {
    mZoom = aZoom;
  }

  CSSToParentLayerScale GetZoom() const
  {
    return mZoom;
  }

  void SetScrollOffsetUpdated(uint32_t aScrollGeneration)
  {
    mUpdateScrollOffset = true;
    mScrollGeneration = aScrollGeneration;
  }

  void SetSmoothScrollOffsetUpdated(int32_t aScrollGeneration)
  {
    mDoSmoothScroll = true;
    mScrollGeneration = aScrollGeneration;
  }

  bool GetScrollOffsetUpdated() const
  {
    return mUpdateScrollOffset;
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

  ViewID GetScrollParentId() const
  {
    return mScrollParentId;
  }

  void SetScrollParentId(ViewID aParentId)
  {
    mScrollParentId = aParentId;
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

  void SetUseDisplayPortMargins()
  {
    mUseDisplayPortMargins = true;
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

  void SetExtraResolution(const ScreenToLayerScale& aExtraResolution)
  {
    mExtraResolution = aExtraResolution;
  }

  ScreenToLayerScale GetExtraResolution() const
  {
    return mExtraResolution;
  }

  const gfxRGBA& GetBackgroundColor() const
  {
    return mBackgroundColor;
  }

  void SetBackgroundColor(const gfxRGBA& aBackgroundColor)
  {
    mBackgroundColor = aBackgroundColor;
  }

  const nsCString& GetContentDescription() const
  {
    return mContentDescription;
  }

  void SetContentDescription(const nsCString& aContentDescription)
  {
    mContentDescription = aContentDescription;
  }

  bool GetMayHaveTouchCaret() const
  {
    return mMayHaveTouchCaret;
  }

  void SetMayHaveTouchCaret(bool aMayHaveTouchCaret)
  {
    mMayHaveTouchCaret = aMayHaveTouchCaret;
  }

  bool GetMayHaveTouchListeners() const
  {
    return mMayHaveTouchListeners;
  }

  void SetMayHaveTouchListeners(bool aMayHaveTouchListeners)
  {
    mMayHaveTouchListeners = aMayHaveTouchListeners;
  }

  const LayoutDeviceIntSize& GetLineScrollAmount() const
  {
    return mLineScrollAmount;
  }

  void SetLineScrollAmount(const LayoutDeviceIntSize& size)
  {
    mLineScrollAmount = size;
  }

private:
  // The cumulative resolution that the current frame has been painted at.
  // This is the product of the pres-shell resolutions of the document
  // containing this scroll frame and its ancestors, and any css-driven
  // resolution. This information is provided by Gecko at layout/paint time.
  LayoutDeviceToLayerScale mCumulativeResolution;

  // New fields from now on should be made private and old fields should
  // be refactored to be private.

  // The conversion factor between CSS pixels and device pixels for this frame.
  // This can vary based on a variety of things, such as reflowing-zoom. The
  // conversion factor for device pixels to layers pixels is just the
  // resolution.
  CSSToLayoutDeviceScale mDevPixelsPerCSSPixel;

  // Whether or not this frame may have touch or scroll wheel listeners.
  bool mMayHaveTouchListeners;

  // Whether or not this frame may have a touch caret.
  bool mMayHaveTouchCaret;

  // Whether or not this is the root scroll frame for the root content document.
  bool mIsRoot;

  // Whether or not this frame is for an element marked 'scrollgrab'.
  bool mHasScrollgrab;

  // A unique ID assigned to each scrollable frame.
  ViewID mScrollId;

  // The ViewID of the scrollable frame to which overscroll should be handed off.
  ViewID mScrollParentId;

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
  CSSToParentLayerScale mZoom;

  // Whether mScrollOffset was updated by something other than the APZ code, and
  // if the APZC receiving this metrics should update its local copy.
  bool mUpdateScrollOffset;
  // The scroll generation counter used to acknowledge the scroll offset update.
  uint32_t mScrollGeneration;

  // When mDoSmoothScroll, the scroll offset should be animated to
  // smoothly transition to mScrollOffset rather than be updated instantly.
  bool mDoSmoothScroll;
  CSSPoint mSmoothScrollOffset;

  // The size of the root scrollable's composition bounds, but in local CSS pixels.
  CSSSize mRootCompositionSize;

  // A display port expressed as layer margins that apply to the rect of what
  // is drawn of the scrollable element.
  ScreenMargin mDisplayPortMargins;

  // If this is true then we use the display port margins on this metrics,
  // otherwise use the display port rect.
  bool mUseDisplayPortMargins;

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
  ScreenToLayerScale mExtraResolution;

  // The background color to use when overscrolling.
  gfxRGBA mBackgroundColor;

  // A description of the content element corresponding to this frame.
  // This is empty unless this is a scrollable layer and the
  // apz.printtree pref is turned on.
  nsCString mContentDescription;

  // The value of GetLineScrollAmount(), for scroll frames.
  LayoutDeviceIntSize mLineScrollAmount;
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

}
}

#endif /* GFX_FRAMEMETRICS_H */
