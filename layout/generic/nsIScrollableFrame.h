/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * interface that provides scroll APIs implemented by scrollable frames
 */

#ifndef nsIScrollFrame_h___
#define nsIScrollFrame_h___

#include "nsCoord.h"
#include "ScrollbarStyles.h"
#include "mozilla/gfx/Point.h"
#include "nsIScrollbarOwner.h"
#include "Units.h"

#define NS_DEFAULT_VERTICAL_SCROLL_DISTANCE   3
#define NS_DEFAULT_HORIZONTAL_SCROLL_DISTANCE 5

class nsBoxLayoutState;
class nsIScrollPositionListener;
class nsIFrame;
class nsPresContext;
class nsIContent;
class nsRenderingContext;
class nsIAtom;

/**
 * Interface for frames that are scrollable. This interface exposes
 * APIs for examining scroll state, observing changes to scroll state,
 * and triggering scrolling.
 */
class nsIScrollableFrame : public nsIScrollbarOwner {
public:
  typedef mozilla::CSSIntPoint CSSIntPoint;

  NS_DECL_QUERYFRAME_TARGET(nsIScrollableFrame)

  /**
   * Get the frame for the content that we are scrolling within
   * this scrollable frame.
   */
  virtual nsIFrame* GetScrolledFrame() const = 0;

  /**
   * Get the styles (NS_STYLE_OVERFLOW_SCROLL, NS_STYLE_OVERFLOW_HIDDEN,
   * or NS_STYLE_OVERFLOW_AUTO) governing the horizontal and vertical
   * scrollbars for this frame.
   */
  virtual mozilla::ScrollbarStyles GetScrollbarStyles() const = 0;

  enum { HORIZONTAL = 0x01, VERTICAL = 0x02 };
  /**
   * Return the scrollbars which are visible. It's OK to call this during reflow
   * of the scrolled contents, in which case it will reflect the current
   * assumptions about scrollbar visibility.
   */
  virtual uint32_t GetScrollbarVisibility() const = 0;
  /**
   * Returns the directions in which scrolling is perceived to be allowed.
   * A direction is perceived to be allowed if there is a visible scrollbar
   * for that direction or if the scroll range is at least one device pixel.
   */
  uint32_t GetPerceivedScrollingDirections() const;
  /**
   * Return the actual sizes of all possible scrollbars. Returns 0 for scrollbar
   * positions that don't have a scrollbar or where the scrollbar is not visible.
   * Do not call this while this frame's descendants are being reflowed, it won't be
   * accurate.
   */
  virtual nsMargin GetActualScrollbarSizes() const = 0;
  /**
   * Return the sizes of all scrollbars assuming that any scrollbars that could
   * be visible due to overflowing content, are. This can be called during reflow
   * of the scrolled contents.
   */
  virtual nsMargin GetDesiredScrollbarSizes(nsBoxLayoutState* aState) = 0;
  /**
   * Return the sizes of all scrollbars assuming that any scrollbars that could
   * be visible due to overflowing content, are. This can be called during reflow
   * of the scrolled contents.
   */
  virtual nsMargin GetDesiredScrollbarSizes(nsPresContext* aPresContext,
                                            nsRenderingContext* aRC) = 0;
  /**
   * Return the width for non-disappearing scrollbars.
   */
  virtual nscoord GetNondisappearingScrollbarWidth(nsPresContext* aPresContext,
                                                   nsRenderingContext* aRC) = 0;
  /**
   * GetScrolledRect is designed to encapsulate deciding which
   * directions of overflow should be reachable by scrolling and which
   * should not.  Callers should NOT depend on it having any particular
   * behavior (although nsXULScrollFrame currently does).
   *
   * This should only be called when the scrolled frame has been
   * reflowed with the scroll port size given in mScrollPort.
   *
   * Currently it allows scrolling down and to the right for
   * nsHTMLScrollFrames with LTR directionality and for all
   * nsXULScrollFrames, and allows scrolling down and to the left for
   * nsHTMLScrollFrames with RTL directionality.
   */
  virtual nsRect GetScrolledRect() const = 0;
  /**
   * Get the area of the scrollport relative to the origin of this frame's
   * border-box.
   * This is the area of this frame minus border and scrollbars.
   */
  virtual nsRect GetScrollPortRect() const = 0;
  /**
   * Get the offset of the scrollport origin relative to the scrolled
   * frame origin. Typically the position will be non-negative.
   * This will always be a multiple of device pixels.
   */
  virtual nsPoint GetScrollPosition() const = 0;
  /**
   * As GetScrollPosition(), but uses the top-right as origin for RTL frames. 
   */
  virtual nsPoint GetLogicalScrollPosition() const = 0;
  /**
   * Get the area that must contain the scroll position. Typically
   * (but not always, e.g. for RTL content) x and y will be 0, and
   * width or height will be nonzero if the content can be scrolled in
   * that direction. Since scroll positions must be a multiple of
   * device pixels, the range extrema will also be a multiple of
   * device pixels.
   */
  virtual nsRect GetScrollRange() const = 0;
  /**
   * Get the size of the scroll port to use when clamping the scroll
   * position.
   */
  virtual nsSize GetScrollPositionClampingScrollPortSize() const = 0;
  /**
   * Get the element resolution.
   */
  virtual gfxSize GetResolution() const = 0;
  /**
   * Set the element resolution.
   */
  virtual void SetResolution(const gfxSize& aResolution) = 0;
  /**
   * Return how much we would try to scroll by in each direction if
   * asked to scroll by one "line" vertically and horizontally.
   */
  virtual nsSize GetLineScrollAmount() const = 0;
  /**
   * Return how much we would try to scroll by in each direction if
   * asked to scroll by one "page" vertically and horizontally.
   */
  virtual nsSize GetPageScrollAmount() const = 0;

  /**
   * When a scroll operation is requested, we ask for instant, smooth or normal
   * scrolling. SMOOTH will only be smooth if smooth scrolling is actually
   * enabled. INSTANT is always synchronous, NORMAL can be asynchronous.
   * If an INSTANT request happens while a smooth or async scroll is already in
   * progress, the async scroll is interrupted and we instantly scroll to the
   * destination.
   */
  enum ScrollMode { INSTANT, SMOOTH, NORMAL };
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   * Clamps aScrollPosition to GetScrollRange and sets the scroll position
   * to that value.
   * @param aRange If non-null, specifies area which contains aScrollPosition
   * and can be used for choosing a performance-optimized scroll position.
   * Any point within this area can be chosen.
   * The choosen point will be as close as possible to aScrollPosition.
   */
  virtual void ScrollTo(nsPoint aScrollPosition, ScrollMode aMode,
                        const nsRect* aRange = nullptr) = 0;
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   * Scrolls to a particular position in integer CSS pixels.
   * Keeps the exact current horizontal or vertical position if the current
   * position, rounded to CSS pixels, matches aScrollPosition. If
   * aScrollPosition.x/y is different from the current CSS pixel position,
   * makes sure we only move in the direction given by the difference.
   * Ensures that GetScrollPositionCSSPixels (the scroll position after
   * rounding to CSS pixels) will be exactly aScrollPosition.
   * The scroll mode is INSTANT.
   */
  virtual void ScrollToCSSPixels(const CSSIntPoint& aScrollPosition) = 0;
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   * Scrolls to a particular position in float CSS pixels.
   * This does not guarantee that GetScrollPositionCSSPixels equals
   * aScrollPosition afterward. It tries to scroll as close to
   * aScrollPosition as possible while scrolling by an integer
   * number of layer pixels (so the operation is fast and looks clean).
   * The scroll mode is INSTANT.
   */
  virtual void ScrollToCSSPixelsApproximate(const mozilla::CSSPoint& aScrollPosition,
                                            nsIAtom *aOrigin = nullptr) = 0;

  /**
   * Returns the scroll position in integer CSS pixels, rounded to the nearest
   * pixel.
   */
  virtual CSSIntPoint GetScrollPositionCSSPixels() = 0;
  /**
   * When scrolling by a relative amount, we can choose various units.
   */
  enum ScrollUnit { DEVICE_PIXELS, LINES, PAGES, WHOLE };
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   * Modifies the current scroll position by aDelta units given by aUnit,
   * clamping it to GetScrollRange. If WHOLE is specified as the unit,
   * content is scrolled all the way in the direction(s) given by aDelta.
   * @param aOverflow if non-null, returns the amount that scrolling
   * was clamped by in each direction (how far we moved the scroll position
   * to bring it back into the legal range). This is never negative. The
   * values are in device pixels.
   */
  virtual void ScrollBy(nsIntPoint aDelta, ScrollUnit aUnit, ScrollMode aMode,
                        nsIntPoint* aOverflow = nullptr, nsIAtom *aOrigin = nullptr) = 0;
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   * This tells the scroll frame to try scrolling to the scroll
   * position that was restored from the history. This must be called
   * at least once after state has been restored. It is called by the
   * scrolled frame itself during reflow, but sometimes state can be
   * restored after reflows are done...
   * XXX should we take an aMode parameter here? Currently it's instant.
   */
  virtual void ScrollToRestoredPosition() = 0;

  /**
   * Add a scroll position listener. This listener must be removed
   * before it is destroyed.
   */
  virtual void AddScrollPositionListener(nsIScrollPositionListener* aListener) = 0;
  /**
   * Remove a scroll position listener.
   */
  virtual void RemoveScrollPositionListener(nsIScrollPositionListener* aListener) = 0;

  /**
   * Internal method used by scrollbars to notify their scrolling
   * container of changes.
   */
  virtual void CurPosAttributeChanged(nsIContent* aChild) = 0;

  /**
   * Allows the docshell to request that the scroll frame post an event
   * after being restored from history.
   */
  NS_IMETHOD PostScrolledAreaEventForCurrentArea() = 0;

  /**
   * Returns true if this scrollframe is being "actively scrolled".
   * This basically means that we should allocate resources in the
   * expectation that scrolling is going to happen.
   */
  virtual bool IsScrollingActive() = 0;
  /**
   * Returns true if the scrollframe is currently processing an async
   * or smooth scroll.
   */
  virtual bool IsProcessingAsyncScroll() = 0;
  /**
   * Call this when the layer(s) induced by active scrolling are being
   * completely redrawn.
   */
  virtual void ResetScrollPositionForLayerPixelAlignment() = 0;
  /**
   * Was the current presentation state for this frame restored from history?
   */
  virtual bool DidHistoryRestore() = 0;
  /**
   * Clear the flag so that DidHistoryRestore() returns false until the next
   * RestoreState call.
   * @see nsIStatefulFrame::RestoreState
   */
  virtual void ClearDidHistoryRestore() = 0;
  /**
   * Determine if the passed in rect is nearly visible according to the image
   * visibility heuristics for how close it is to the visible scrollport.
   */
  virtual bool IsRectNearlyVisible(const nsRect& aRect) = 0;
  /**
   * Returns the origin passed in to the last ScrollToImpl call that took
   * effect.
   */
  virtual nsIAtom* OriginOfLastScroll() = 0;
  /**
   * Returns the current generation counter for the scroll. This counter
   * increments every time the scroll position is set.
   */
  virtual uint32_t CurrentScrollGeneration() = 0;
  /**
   * Clears the "origin of last scroll" property stored in this frame, if
   * the generation counter passed in matches the current scroll generation
   * counter.
   */
  virtual void ResetOriginIfScrollAtGeneration(uint32_t aGeneration) = 0;
  /**
   * Determine whether it is desirable to be able to asynchronously scroll this
   * scroll frame.
   */
  virtual bool WantAsyncScroll() const = 0;
};

#endif
