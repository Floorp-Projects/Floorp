/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * interface that provides scroll APIs implemented by scrollable frames
 */

#ifndef nsIScrollFrame_h___
#define nsIScrollFrame_h___

#include "nsCoord.h"
#include "mozilla/dom/WindowBinding.h"  // for mozilla::dom::ScrollBehavior
#include "mozilla/Maybe.h"
#include "mozilla/ScrollOrigin.h"
#include "mozilla/ScrollPositionUpdate.h"
#include "mozilla/ScrollStyles.h"
#include "mozilla/ScrollTypes.h"
#include "mozilla/gfx/Point.h"
#include "nsIScrollbarMediator.h"
#include "Units.h"
#include "FrameMetrics.h"

#define NS_DEFAULT_VERTICAL_SCROLL_DISTANCE 3
#define NS_DEFAULT_HORIZONTAL_SCROLL_DISTANCE 5

class gfxContext;
class nsBoxLayoutState;
class nsIScrollPositionListener;
class nsIFrame;
class nsPresContext;
class nsIContent;

namespace mozilla {
class DisplayItemClip;
class nsDisplayListBuilder;
enum class StyleScrollSnapAlignKeyword : uint8_t;

namespace layers {
struct ScrollMetadata;
class Layer;
class WebRenderLayerManager;
}  // namespace layers
namespace layout {
class ScrollAnchorContainer;
}  // namespace layout
}  // namespace mozilla

/**
 * Interface for frames that are scrollable. This interface exposes
 * APIs for examining scroll state, observing changes to scroll state,
 * and triggering scrolling.
 */
class nsIScrollableFrame : public nsIScrollbarMediator {
 public:
  using CSSIntPoint = mozilla::CSSIntPoint;
  using ScrollSnapInfo = mozilla::layers::ScrollSnapInfo;
  using ScrollAnchorContainer = mozilla::layout::ScrollAnchorContainer;
  using ScrollMode = mozilla::ScrollMode;
  using ScrollOrigin = mozilla::ScrollOrigin;
  using PhysicalScrollSnapAlign =
      std::pair<mozilla::StyleScrollSnapAlignKeyword,
                mozilla::StyleScrollSnapAlignKeyword>;

  NS_DECL_QUERYFRAME_TARGET(nsIScrollableFrame)

  /**
   * Get the frame for the content that we are scrolling within
   * this scrollable frame.
   */
  virtual nsIFrame* GetScrolledFrame() const = 0;

  /**
   * Get the overflow styles (StyleOverflow::Scroll, StyleOverflow::Hidden, or
   * StyleOverflow::Auto) governing the horizontal and vertical scrollbars for
   * this frame.
   *
   * This is special because they can be propagated from the <body> element,
   * unlike other styles.
   */
  virtual mozilla::ScrollStyles GetScrollStyles() const = 0;

  /**
   * Returns whether this scroll frame is for a text control element with no
   * scrollbars (for <input>, basically).
   */
  virtual bool IsForTextControlWithNoScrollbars() const = 0;

  /**
   * Returns whether we already have anonymous content nodes for all our needed
   * scrollbar parts (or a superset thereof).
   */
  virtual bool HasAllNeededScrollbars() const = 0;

  /**
   * Get the overscroll-behavior styles.
   */
  virtual mozilla::layers::OverscrollBehaviorInfo GetOverscrollBehaviorInfo()
      const = 0;

  /**
   * Return the scrollbars which are visible. It's OK to call this during reflow
   * of the scrolled contents, in which case it will reflect the current
   * assumptions about scrollbar visibility.
   */
  virtual mozilla::layers::ScrollDirections GetScrollbarVisibility() const = 0;
  /**
   * Returns the directions in which scrolling is allowed (if the scroll range
   * is at least one device pixel in that direction).
   */
  mozilla::layers::ScrollDirections GetAvailableScrollingDirections() const;
  /**
   * Returns the directions in which scrolling is allowed when taking into
   * account the visual viewport size and overflow hidden. (An (apz) zoomed in
   * overflow hidden scrollframe is actually user scrollable.)
   */
  virtual mozilla::layers::ScrollDirections
  GetAvailableScrollingDirectionsForUserInputEvents() const = 0;
  /**
   * Return the actual sizes of all possible scrollbars. Returns 0 for scrollbar
   * positions that don't have a scrollbar or where the scrollbar is not
   * visible. Do not call this while this frame's descendants are being
   * reflowed, it won't be accurate.
   * INCLUDE_VISUAL_VIEWPORT_SCROLLBARS means we include the size of layout
   * scrollbars that are only visible to scroll the visual viewport inside the
   * layout viewport (ie the layout viewport cannot be scrolled) even though
   * there is no layout space set aside for these scrollbars.
   */
  enum class ScrollbarSizesOptions { NONE, INCLUDE_VISUAL_VIEWPORT_SCROLLBARS };
  virtual nsMargin GetActualScrollbarSizes(
      ScrollbarSizesOptions aOptions = ScrollbarSizesOptions::NONE) const = 0;
  /**
   * Return the sizes of all scrollbars assuming that any scrollbars that could
   * be visible due to overflowing content, are. This can be called during
   * reflow of the scrolled contents.
   */
  virtual nsMargin GetDesiredScrollbarSizes(nsBoxLayoutState* aState) = 0;
  /**
   * Return the sizes of all scrollbars assuming that any scrollbars that could
   * be visible due to overflowing content, are. This can be called during
   * reflow of the scrolled contents.
   */
  virtual nsMargin GetDesiredScrollbarSizes(nsPresContext* aPresContext,
                                            gfxContext* aRC) = 0;
  /**
   * Return the width for non-disappearing scrollbars.
   */
  static nscoord GetNondisappearingScrollbarWidth(nsPresContext*,
                                                  mozilla::WritingMode);
  /**
   * Get the layout size of this frame.
   * Note that this is a value which is not expanded by the minimum scale size.
   * For scroll frames other than the root content document's scroll frame, this
   * value will be the same as GetScrollPortRect().Size().
   *
   * This value is used for Element.clientWidth and clientHeight.
   */
  virtual nsSize GetLayoutSize() const = 0;
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
   * Get the size of the view port to use when clamping the scroll
   * position.
   */
  virtual nsSize GetVisualViewportSize() const = 0;
  /**
   * Returns the offset of the visual viewport relative to
   * the origin of the scrolled content. Note that only the RCD-RSF
   * has a distinct visual viewport; for other scroll frames, the
   * visual viewport always coincides with the layout viewport, and
   * consequently the offset this function returns is equal to
   * GetScrollPosition().
   */
  virtual nsPoint GetVisualViewportOffset() const = 0;
  /**
   * Set the visual viewport offset associated with a root scroll frame. This is
   * only valid when called on a root scroll frame and will assert otherwise.
   * aRepaint indicates if we need to ask for a main thread paint if this
   * changes scrollbar positions or not. For example, if the compositor has
   * already put the scrollbars at this position then they don't need to move so
   * we can skip the repaint. Returns true if the offset changed and the scroll
   * frame is still alive after this call.
   */
  virtual bool SetVisualViewportOffset(const nsPoint& aOffset,
                                       bool aRepaint) = 0;
  /**
   * Get the area that must contain the visual viewport offset.
   */
  virtual nsRect GetVisualScrollRange() const = 0;
  /**
   * Like GetVisualScrollRange but also takes into account overflow: hidden.
   */
  virtual nsRect GetScrollRangeForUserInputEvents() const = 0;
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
   * Return scroll-padding value of this frame.
   */
  virtual nsMargin GetScrollPadding() const = 0;
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
  virtual void ScrollTo(
      nsPoint aScrollPosition, ScrollMode aMode, const nsRect* aRange = nullptr,
      mozilla::ScrollSnapFlags aSnapFlags = mozilla::ScrollSnapFlags::Disabled,
      mozilla::ScrollTriggeredByScript aTriggeredByScript =
          mozilla::ScrollTriggeredByScript::No) = 0;
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
                                 ScrollMode aMode = ScrollMode::Instant) = 0;
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   * Scrolls to a particular position in float CSS pixels.
   * This does not guarantee that GetScrollPositionCSSPixels equals
   * aScrollPosition afterward. It tries to scroll as close to
   * aScrollPosition as possible while scrolling by an integer
   * number of layer pixels (so the operation is fast and looks clean).
   */
  virtual void ScrollToCSSPixelsForApz(
      const mozilla::CSSPoint& aScrollPosition,
      mozilla::ScrollSnapTargetIds&& aLastSnapTargetIds) = 0;

  /**
   * Returns the scroll position in integer CSS pixels, rounded to the nearest
   * pixel.
   */
  virtual CSSIntPoint GetScrollPositionCSSPixels() = 0;
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
  virtual void ScrollBy(nsIntPoint aDelta, mozilla::ScrollUnit aUnit,
                        ScrollMode aMode, nsIntPoint* aOverflow = nullptr,
                        ScrollOrigin aOrigin = ScrollOrigin::NotSpecified,
                        ScrollMomentum aMomentum = NOT_MOMENTUM,
                        mozilla::ScrollSnapFlags aSnapFlags =
                            mozilla::ScrollSnapFlags::Disabled) = 0;

  virtual void ScrollByCSSPixels(const CSSIntPoint& aDelta,
                                 ScrollMode aMode = ScrollMode::Instant) = 0;

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
  virtual void AddScrollPositionListener(
      nsIScrollPositionListener* aListener) = 0;
  /**
   * Remove a scroll position listener.
   */
  virtual void RemoveScrollPositionListener(
      nsIScrollPositionListener* aListener) = 0;

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
   * Returns true if this scroll frame might be scrolled
   * asynchronously by the compositor.
   */
  virtual bool IsMaybeAsynchronouslyScrolled() = 0;

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
   * Mark the frame as having been scrolled at least once, so that it remains
   * active and we can also start storing its scroll position when saving state.
   */
  virtual void MarkEverScrolled() = 0;
  /**
   * Determine if the passed in rect is nearly visible according to the frame
   * visibility heuristics for how close it is to the visible scrollport.
   */
  virtual bool IsRectNearlyVisible(const nsRect& aRect) = 0;
  /**
   * Expand the given rect taking into account which directions we can scroll
   * and how far we want to expand for frame visibility purposes.
   */
  virtual nsRect ExpandRectToNearlyVisible(const nsRect& aRect) const = 0;
  /**
   * Returns the origin that triggered the last instant scroll. Will equal
   * ScrollOrigin::Apz when the compositor's replica frame metrics includes the
   * latest instant scroll.
   */
  virtual ScrollOrigin LastScrollOrigin() = 0;

  /**
   * Gets the async scroll animation state of this scroll frame.
   *
   * There are five possible kinds that can overlap.
   * MainThread means async scroll animated by the main thread.
   * APZ scroll animations that are requested from the main thread go through
   * three states: 1) pending, when the main thread has recorded that it wants
   * apz to do a scroll animation, 2) requested, when the main thread has sent
   * the request to the compositor (but it hasn't necessarily arrived yet), and
   * 3) in progress, after apz has responded to the main thread that it got the
   * request.
   * TriggeredByScript means that the async scroll animation was triggered by
   * script, e.g. Element.scrollTo().
   */
  enum class AnimationState {
    MainThread,        // mAsyncScroll || mAsyncSmoothMSDScroll
    APZPending,        // mScrollUpdates.LastElement() is Smooth or SmoothMsd
    APZRequested,      // mApzAnimationRequested
    APZInProgress,     // mCurrentAPZScrollAnimationType !=
                       // APZScrollAniationType::No
    TriggeredByScript  // The animation was triggered with
                       // ScrollTriggeredByScript::Yes
  };
  virtual mozilla::EnumSet<AnimationState> ScrollAnimationState() const = 0;

  /**
   * Returns the current generation counter for the scrollframe. This counter
   * increments every time the scroll position is set.
   */
  virtual mozilla::MainThreadScrollGeneration CurrentScrollGeneration()
      const = 0;
  /**
   * The APZ scroll generation associated with the last APZ scroll offset for
   * which we processed a repaint request.
   */
  virtual mozilla::APZScrollGeneration ScrollGenerationOnApz() const = 0;
  /**
   * LastScrollDestination returns the destination of the most recently
   * requested smooth scroll animation.
   */
  virtual nsPoint LastScrollDestination() = 0;
  /**
   * Returns the list of scroll position updates since the last call to
   * NotifyApzTransaction().
   */
  virtual nsTArray<mozilla::ScrollPositionUpdate> GetScrollUpdates() const = 0;
  /**
   * Returns true if the scroll frame has any scroll position updates since
   * the last call to NotifyApzTransaction().
   */
  virtual bool HasScrollUpdates() const = 0;

  enum class InScrollingGesture : bool { No, Yes };
  /**
   * Clears the "origin of last scroll" property stored in this frame, if
   * the generation counter passed in matches the current scroll generation
   * counter, and clears the "origin of last smooth scroll" property if the
   * generation counter matches. It also resets whether there's an ongoing apz
   * animation.
   */
  virtual void ResetScrollInfoIfNeeded(
      const mozilla::MainThreadScrollGeneration& aGeneration,
      const mozilla::APZScrollGeneration& aGenerationOnApz,
      mozilla::APZScrollAnimationType aAPZScrollAnimationType,
      InScrollingGesture aInScrollingGesture) = 0;
  /**
   * Determine whether it is desirable to be able to asynchronously scroll this
   * scroll frame.
   */
  virtual bool WantAsyncScroll() const = 0;
  /**
   * Returns the ScrollMetadata contributed by this frame, if there is one.
   */
  virtual mozilla::Maybe<mozilla::layers::ScrollMetadata> ComputeScrollMetadata(
      mozilla::layers::WebRenderLayerManager* aLayerManager,
      const nsIFrame* aItemFrame,
      const nsPoint& aOffsetToReferenceFrame) const = 0;

  /**
   * Mark the scrollbar frames for reflow.
   */
  virtual void MarkScrollbarsDirtyForReflow() const = 0;

  /**
   * Invalidate the scrollbar after the marks have been changed.
   */
  virtual void InvalidateScrollbars() const = 0;

  virtual void UpdateScrollbarPosition() = 0;

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
   * Mark this scroll frame as having out-of-flow content inside a CSS filter.
   * Such content will move incorrectly during async-scrolling; to mitigate
   * this, paint skipping is disabled for such scroll frames.
   */
  virtual void SetHasOutOfFlowContentInsideFilter() = 0;

  /**
   * Determine if we should build a scrollable layer for this scroll frame and
   * return the result. It will also record this result on the scroll frame.
   * Pass the visible rect in aVisibleRect. On return it will be set to the
   * displayport if there is one.
   * Pass the dirty rect in aDirtyRect. On return it will be set to the
   * dirty rect inside the displayport (ie the dirty rect that should be used).
   * This function will set the display port base rect if aSetBase is true.
   * aSetBase is only allowed to be false if there has been a call with it
   * set to true before on the same paint.
   */
  virtual bool DecideScrollableLayer(mozilla::nsDisplayListBuilder* aBuilder,
                                     nsRect* aVisibleRect, nsRect* aDirtyRect,
                                     bool aSetBase) = 0;

  /**
   * Notify the scrollframe that the current scroll offset and origin have been
   * sent over in a layers transaction.
   *
   * This sets a flag on the scrollframe that indicates subsequent changes
   * to the scroll position by "weaker" origins are permitted to overwrite the
   * the scroll origin. Scroll origins that
   * nsLayoutUtils::CanScrollOriginClobberApz returns false for are considered
   * "weaker" than scroll origins for which that function returns true.
   *
   * This function must be called for a scrollframe after all calls to
   * ComputeScrollMetadata in a layers transaction have been completed.
   *
   */
  virtual void NotifyApzTransaction() = 0;

  /**
   * Notification that this scroll frame is getting its frame visibility
   * updated. aIgnoreDisplayPort indicates that the display port was ignored
   * (because there was no suitable base rect)
   */
  virtual void NotifyApproximateFrameVisibilityUpdate(
      bool aIgnoreDisplayPort) = 0;

  /**
   * Returns true if this scroll frame had a display port at the last frame
   * visibility update and fills in aDisplayPort with that displayport. Returns
   * false otherwise, and doesn't touch aDisplayPort.
   */
  virtual bool GetDisplayPortAtLastApproximateFrameVisibilityUpdate(
      nsRect* aDisplayPort) = 0;

  /**
   * This is called when a descendant scrollframe's has its displayport expired.
   * This function will check to see if this scrollframe may safely expire its
   * own displayport and schedule a timer to do that if it is safe.
   */
  virtual void TriggerDisplayPortExpiration() = 0;

  /**
   * Returns information required to determine where to snap to after a scroll.
   */
  virtual ScrollSnapInfo GetScrollSnapInfo() = 0;

  virtual void TryResnap() = 0;
  /**
   * Post a pending re-snap request if the given |aFrame| is one of the snap
   * points on the last scroll operation.
   */
  virtual void PostPendingResnapIfNeeded(const nsIFrame* aFrame) = 0;
  virtual void PostPendingResnap() = 0;

  /**
   * Returns a pair of the scroll-snap-align property value both on X and Y axes
   * for the given |aFrame| considering the scroll-snap-type of this scroll
   * container. For example, if the scroll-snap-type is `none`, the pair of
   * scroll-snap-align is also `none none`.
   */
  virtual PhysicalScrollSnapAlign GetScrollSnapAlignFor(
      const nsIFrame* aFrame) const = 0;

  /**
   * Given the drag event aEvent, determine whether the mouse is near the edge
   * of the scrollable area, and scroll the view in the direction of that edge
   * if so. If scrolling occurred, true is returned. When false is returned, the
   * caller should look for an ancestor to scroll.
   */
  virtual bool DragScroll(mozilla::WidgetEvent* aEvent) = 0;

  virtual void AsyncScrollbarDragInitiated(
      uint64_t aDragBlockId, mozilla::layers::ScrollDirection aDirection) = 0;
  virtual void AsyncScrollbarDragRejected() = 0;

  /**
   * Returns whether this scroll frame is the root scroll frame of the document
   * that it is in. Note that some documents don't have root scroll frames at
   * all (ie XUL documents) even though they may contain other scroll frames.
   */
  virtual bool IsRootScrollFrameOfDocument() const = 0;

  /**
   * Returns the scroll anchor associated with this scrollable frame. This is
   * never null.
   */
  virtual const ScrollAnchorContainer* Anchor() const = 0;
  virtual ScrollAnchorContainer* Anchor() = 0;

  virtual bool SmoothScrollVisual(
      const nsPoint& aVisualViewportOffset,
      mozilla::layers::FrameMetrics::ScrollOffsetUpdateType aUpdateType) = 0;

  /**
   * Returns true if this scroll frame should perform smooth scroll with the
   * given |aBehavior|.
   */
  virtual bool IsSmoothScroll(mozilla::dom::ScrollBehavior aBehavior =
                                  mozilla::dom::ScrollBehavior::Auto) const = 0;
};

#endif
