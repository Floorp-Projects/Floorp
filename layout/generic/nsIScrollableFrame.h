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
#include "DisplayItemClip.h"
#include "ScrollbarStyles.h"
#include "mozilla/Maybe.h"
#include "mozilla/gfx/Point.h"
#include "nsIScrollbarMediator.h"
#include "Units.h"
#include "FrameMetrics.h"

#define NS_DEFAULT_VERTICAL_SCROLL_DISTANCE   3
#define NS_DEFAULT_HORIZONTAL_SCROLL_DISTANCE 5

class nsBoxLayoutState;
class nsIScrollPositionListener;
class nsIFrame;
class nsPresContext;
class nsIContent;
class nsRenderingContext;
class nsIAtom;
class nsDisplayListBuilder;

namespace mozilla {
struct ContainerLayerParameters;
namespace layers {
class Layer;
} // namespace layers
} // namespace mozilla

/**
 * Interface for frames that are scrollable. This interface exposes
 * APIs for examining scroll state, observing changes to scroll state,
 * and triggering scrolling.
 */
class nsIScrollableFrame : public nsIScrollbarMediator {
public:
  typedef mozilla::CSSIntPoint CSSIntPoint;
  typedef mozilla::ContainerLayerParameters ContainerLayerParameters;
  typedef mozilla::layers::FrameMetrics FrameMetrics;

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

  virtual bool IsScrollFrameWithSnapping() const = 0;

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
  virtual nscoord
  GetNondisappearingScrollbarWidth(nsPresContext* aPresContext,
                                   nsRenderingContext* aRC,
                                   mozilla::WritingMode aWM) = 0;
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
   * When a scroll operation is requested, we ask for instant, smooth,
   * smooth msd, or normal scrolling.
   *
   * SMOOTH scrolls have a symmetrical acceleration and deceleration curve
   * modeled with a set of splines that guarantee that the destination will be 
   * reached over a fixed time interval.  SMOOTH will only be smooth if smooth
   * scrolling is actually enabled.  This behavior is utilized by keyboard and
   * mouse wheel scrolling events.
   *
   * SMOOTH_MSD implements a physically based model that approximates the
   * behavior of a mass-spring-damper system.  SMOOTH_MSD scrolls have a
   * non-symmetrical acceleration and deceleration curve, can potentially
   * overshoot the destination on intermediate frames, and complete over a
   * variable time interval.  SMOOTH_MSD will only be smooth if cssom-view
   * smooth-scrolling is enabled.
   *
   * INSTANT is always synchronous, NORMAL can be asynchronous.
   *
   * If an INSTANT scroll request happens while a SMOOTH or async scroll is
   * already in progress, the async scroll is interrupted and we instantly
   * scroll to the destination.
   *
   * If an INSTANT or SMOOTH scroll request happens while a SMOOTH_MSD scroll
   * is already in progress, the SMOOTH_MSD scroll is interrupted without
   * first scrolling to the destination.
   */
  enum ScrollMode { INSTANT, SMOOTH, SMOOTH_MSD, NORMAL };
  /**
   * Some platforms (OSX) may generate additional scrolling events even
   * after the user has stopped scrolling, simulating a momentum scrolling
   * effect resulting from fling gestures.
   * SYNTHESIZED_MOMENTUM_EVENT indicates that the scrolling is being requested
   * by such a synthesized event and may be ignored if another scroll has
   * been started since the last actual user input.
   */
  enum ScrollMomentum { NOT_MOMENTUM, SYNTHESIZED_MOMENTUM_EVENT };
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
                        const nsRect* aRange = nullptr,
                        nsIScrollbarMediator::ScrollSnapMode aSnap
                          = nsIScrollbarMediator::DISABLE_SNAP) = 0;
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   * Scrolls to a particular position in integer CSS pixels.
   * Keeps the exact current horizontal or vertical position if the current
   * position, rounded to CSS pixels, matches aScrollPosition. If
   * aScrollPosition.x/y is different from the current CSS pixel position,
   * makes sure we only move in the direction given by the difference.
   *
   * When aMode is SMOOTH, INSTANT, or NORMAL, GetScrollPositionCSSPixels (the
   * scroll position after rounding to CSS pixels) will be exactly
   * aScrollPosition at the end of the scroll animation.
   *
   * When aMode is SMOOTH_MSD, intermediate animation frames may be outside the
   * range and / or moving in any direction; GetScrollPositionCSSPixels will be
   * exactly aScrollPosition at the end of the scroll animation unless the
   * SMOOTH_MSD animation is interrupted.
   */
  virtual void ScrollToCSSPixels(const CSSIntPoint& aScrollPosition,
                                 nsIScrollableFrame::ScrollMode aMode
                                   = nsIScrollableFrame::INSTANT) = 0;
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   * Scrolls to a particular position in float CSS pixels.
   * This does not guarantee that GetScrollPositionCSSPixels equals
   * aScrollPosition afterward. It tries to scroll as close to
   * aScrollPosition as possible while scrolling by an integer
   * number of layer pixels (so the operation is fast and looks clean).
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
                        nsIntPoint* aOverflow = nullptr,
                        nsIAtom* aOrigin = nullptr,
                        ScrollMomentum aMomentum = NOT_MOMENTUM,
                        nsIScrollbarMediator::ScrollSnapMode aSnap
                          = nsIScrollbarMediator::DISABLE_SNAP) = 0;

  /**
   * Perform scroll snapping, possibly resulting in a smooth scroll to
   * maintain the scroll snap position constraints.  A predicted landing
   * position determined by the APZC is used to select the best matching
   * snap point, allowing touchscreen fling gestures to navigate between
   * snap points.
   * @param aDestination The desired landing position of the fling, which
   * is used to select the best matching snap point.
   */
  virtual void FlingSnap(const mozilla::CSSPoint& aDestination) = 0;
  /**
   * Perform scroll snapping, possibly resulting in a smooth scroll to
   * maintain the scroll snap position constraints.  Velocity sampled from
   * main thread scrolling is used to determine best matching snap point
   * when called after a fling gesture on a trackpad or mouse wheel.
   */
  virtual void ScrollSnap() = 0;

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
  virtual bool IsScrollingActive(nsDisplayListBuilder* aBuilder) = 0;
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
  virtual bool DidHistoryRestore() const = 0;
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
  * Expand the given rect taking into account which directions we can scroll
  * and how far we want to expand for image visibility purposes.
  */
  virtual nsRect ExpandRectToNearlyVisible(const nsRect& aRect) const = 0;
  /**
   * Returns the origin that triggered the last instant scroll. Will equal
   * nsGkAtoms::apz when the compositor's replica frame metrics includes the
   * latest instant scroll.
   */
  virtual nsIAtom* LastScrollOrigin() = 0;
  /**
   * Returns the origin that triggered the last smooth scroll.
   * Will equal nsGkAtoms::apz when the compositor's replica frame
   * metrics includes the latest smooth scroll.  The compositor will always
   * perform an instant scroll prior to instantiating any smooth scrolls
   * if LastScrollOrigin and LastSmoothScrollOrigin indicate that
   * an instant scroll and a smooth scroll have occurred since the last
   * replication of the frame metrics.
   *
   * This is set to nullptr to when the compositor thread acknowledges that
   * the smooth scroll has been started.  If the smooth scroll has been stomped
   * by an instant scroll before the smooth scroll could be started by the
   * compositor, this is set to nullptr to clear the smooth scroll.
   */
  virtual nsIAtom* LastSmoothScrollOrigin() = 0;
  /**
   * Returns the current generation counter for the scroll. This counter
   * increments every time the scroll position is set.
   */
  virtual uint32_t CurrentScrollGeneration() = 0;
  /**
   * LastScrollDestination returns the destination of the most recently
   * requested smooth scroll animation.
   */
  virtual nsPoint LastScrollDestination() = 0;
  /**
   * Clears the "origin of last scroll" property stored in this frame, if
   * the generation counter passed in matches the current scroll generation
   * counter.
   */
  virtual void ResetScrollInfoIfGeneration(uint32_t aGeneration) = 0;
  /**
   * Determine whether it is desirable to be able to asynchronously scroll this
   * scroll frame.
   */
  virtual bool WantAsyncScroll() const = 0;
  /**
   * aLayer's animated geometry root is this frame. If there needs to be a
   * FrameMetrics contributed by this frame, append it to aOutput.
   */
  virtual mozilla::Maybe<mozilla::layers::FrameMetrics> ComputeFrameMetrics(
    mozilla::layers::Layer* aLayer,
    nsIFrame* aContainerReferenceFrame,
    const ContainerLayerParameters& aParameters,
    const mozilla::DisplayItemClip* aClip) const = 0;

  /**
   * If this scroll frame is ignoring viewporting clipping
   */
  virtual bool IsIgnoringViewportClipping() const = 0;

  /**
   * Mark the scrollbar frames for reflow.
   */
  virtual void MarkScrollbarsDirtyForReflow() const = 0;

  virtual void SetTransformingByAPZ(bool aTransforming) = 0;
  virtual bool IsTransformingByAPZ() const = 0;

  /**
   * Notify this scroll frame that it can be scrolled by APZ. In particular,
   * this is called *after* the APZ code has created an APZC for this scroll
   * frame and verified that it is not a scrollinfo layer. Therefore, setting an
   * async transform on it is actually user visible.
   */
  virtual void SetScrollableByAPZ(bool aScrollable) = 0;

  /**
   * Notify this scroll frame that it can be zoomed by APZ.
   */
  virtual void SetZoomableByAPZ(bool aZoomable) = 0;

  /**
   * Whether or not this frame uses containerful scrolling.
   */
  virtual bool UsesContainerScrolling() const = 0;

  /**
   * Determine if we should build a scrollable layer for this scroll frame and
   * return the result. It will also record this result on the scroll frame.
   * Pass the dirty rect in aDirtyRect. On return it will be set to the
   * displayport if there is one (ie the dirty rect that should be used).
   * This function may create a display port where one did not exist before if
   * aAllowCreateDisplayPort is true. It is only allowed to be false if there
   * has been a call with it set to true before on the same paint.
   */
  virtual bool DecideScrollableLayer(nsDisplayListBuilder* aBuilder,
                                     nsRect* aDirtyRect,
                                     bool aAllowCreateDisplayPort) = 0;

  /**
   * Notification that this scroll frame is getting its image visibility updated.
   */
  virtual void NotifyImageVisibilityUpdate() = 0;

  /**
   * Returns true if this scroll frame had a display port at the last image
   * visibility update and fills in aDisplayPort with that displayport. Returns
   * false otherwise, and doesn't touch aDisplayPort.
   */
  virtual bool GetDisplayPortAtLastImageVisibilityUpdate(nsRect* aDisplayPort) = 0;

  /**
   * This is called when a descendant scrollframe's has its displayport expired.
   * This function will check to see if this scrollframe may safely expire its
   * own displayport and schedule a timer to do that if it is safe.
   */
  virtual void TriggerDisplayPortExpiration() = 0;
};

#endif
