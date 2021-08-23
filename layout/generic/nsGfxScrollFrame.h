/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object to wrap rendering objects that should be scrollable */

#ifndef nsGfxScrollFrame_h___
#define nsGfxScrollFrame_h___

#include "mozilla/Attributes.h"
#include "nsContainerFrame.h"
#include "nsIAnonymousContentCreator.h"
#include "nsBoxFrame.h"
#include "nsIScrollableFrame.h"
#include "nsIScrollbarMediator.h"
#include "nsIStatefulFrame.h"
#include "nsThreadUtils.h"
#include "nsIReflowCallback.h"
#include "nsBoxLayoutState.h"
#include "nsQueryFrame.h"
#include "nsExpirationTracker.h"
#include "TextOverflow.h"
#include "ScrollVelocityQueue.h"
#include "mozilla/ScrollTypes.h"
#include "mozilla/PresState.h"
#include "mozilla/layout/ScrollAnchorContainer.h"
#include "mozilla/TypedEnumBits.h"

class nsPresContext;
class nsIContent;
class nsAtom;
class nsIScrollPositionListener;

namespace mozilla {
class PresShell;
struct ScrollReflowInput;
namespace layers {
class Layer;
class LayerManager;
}  // namespace layers
namespace layout {
class ScrollbarActivity;
}  // namespace layout

class ScrollFrameHelper : public nsIReflowCallback {
 public:
  typedef nsIFrame::Sides Sides;
  typedef mozilla::CSSIntPoint CSSIntPoint;
  typedef mozilla::layout::ScrollbarActivity ScrollbarActivity;
  typedef mozilla::layers::FrameMetrics FrameMetrics;
  typedef mozilla::layers::ScrollableLayerGuid ScrollableLayerGuid;
  typedef mozilla::layers::ScrollSnapInfo ScrollSnapInfo;
  typedef mozilla::layers::Layer Layer;
  typedef mozilla::layers::LayerManager LayerManager;
  typedef mozilla::layout::ScrollAnchorContainer ScrollAnchorContainer;
  using Element = mozilla::dom::Element;

  class AsyncScroll;
  class AsyncSmoothMSDScroll;

  ScrollFrameHelper(nsContainerFrame* aOuter, bool aIsRoot);
  ~ScrollFrameHelper();

  mozilla::ScrollStyles GetScrollStylesFromFrame() const;
  mozilla::layers::OverscrollBehaviorInfo GetOverscrollBehaviorInfo() const;

  bool IsForTextControlWithNoScrollbars() const;

  // If a child frame was added or removed on the scrollframe,
  // reload our child frame list.
  // We need this if a scrollbar frame is recreated.
  void ReloadChildFrames();

  nsresult CreateAnonymousContent(
      nsTArray<nsIAnonymousContentCreator::ContentInfo>& aElements);
  void AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                uint32_t aFilter);

  enum class OverflowState : uint32_t {
    None = 0,
    Vertical = (1 << 0),
    Horizontal = (1 << 1),
  };

  OverflowState GetOverflowState() const;

  nsresult FireScrollPortEvent();
  void PostScrollEndEvent();
  void FireScrollEndEvent();
  void PostOverflowEvent();
  using PostDestroyData = nsIFrame::PostDestroyData;
  void Destroy(PostDestroyData& aPostDestroyData);

  void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                        const nsDisplayListSet& aLists);

  // Add display items for the top-layer (which includes things like
  // the fullscreen element, its backdrop, and text selection carets)
  // to |aLists|.
  // This is a no-op for scroll frames other than the viewport's
  // root scroll frame.
  // This should be called with an nsDisplayListSet that will be
  // wrapped in the async zoom container, if we're building one.
  // It should not be called with an ASR setter on the stack, as the
  // top-layer items handle setting up their own ASRs.
  nsDisplayWrapList* MaybeCreateTopLayerItems(nsDisplayListBuilder* aBuilder,
                                              bool* aIsOpaque);

  void AppendScrollPartsTo(nsDisplayListBuilder* aBuilder,
                           const nsDisplayListSet& aLists, bool aCreateLayer,
                           bool aPositioned);

  bool GetBorderRadii(const nsSize& aFrameSize, const nsSize& aBorderArea,
                      Sides aSkipSides, nscoord aRadii[8]) const;

  // nsIReflowCallback
  bool ReflowFinished() final;
  void ReflowCallbackCanceled() final;

  /**
   * @note This method might destroy the frame, pres shell and other objects.
   * Called when the 'curpos' attribute on one of the scrollbars changes.
   */
  void CurPosAttributeChanged(nsIContent* aChild, bool aDoScroll = true);

  void PostScrollEvent(bool aDelayed = false);
  void FireScrollEvent();
  void PostScrolledAreaEvent();
  void FireScrolledAreaEvent();

  bool IsSmoothScrollingEnabled();

  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void FinishReflowForScrollbar(Element* aElement, nscoord aMinXY,
                                nscoord aMaxXY, nscoord aCurPosXY,
                                nscoord aPageIncrement, nscoord aIncrement);
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void SetScrollbarEnabled(Element* aElement, nscoord aMaxPos);
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void SetCoordAttribute(Element* aElement, nsAtom* aAtom, nscoord aSize);

  nscoord GetCoordAttribute(nsIFrame* aFrame, nsAtom* aAtom,
                            nscoord aDefaultValue, nscoord* aRangeStart,
                            nscoord* aRangeLength);

  /**
   * @note This method might destroy the frame, pres shell and other objects.
   * Update scrollbar curpos attributes to reflect current scroll position
   */
  void UpdateScrollbarPosition();

  nsSize GetLayoutSize() const {
    if (mIsUsingMinimumScaleSize) {
      return mICBSize;
    }
    return mScrollPort.Size();
  }
  nsRect GetScrollPortRect() const { return mScrollPort; }
  nsPoint GetScrollPosition() const {
    return mScrollPort.TopLeft() - mScrolledFrame->GetPosition();
  }
  /**
   * For LTR frames, the logical scroll position is the offset of the top left
   * corner of the frame from the top left corner of the scroll port (same as
   * GetScrollPosition).
   * For RTL frames, it is the offset of the top right corner of the frame from
   * the top right corner of the scroll port
   */
  nsPoint GetLogicalScrollPosition() const {
    nsPoint pt;
    pt.x = IsPhysicalLTR()
               ? mScrollPort.x - mScrolledFrame->GetPosition().x
               : mScrollPort.XMost() - mScrolledFrame->GetRect().XMost();
    pt.y = mScrollPort.y - mScrolledFrame->GetPosition().y;
    return pt;
  }
  nsRect GetLayoutScrollRange() const;
  // Get the scroll range assuming the viewport has size (aWidth, aHeight).
  nsRect GetScrollRange(nscoord aWidth, nscoord aHeight) const;
  nsSize GetVisualViewportSize() const;
  nsPoint GetVisualViewportOffset() const;
  bool SetVisualViewportOffset(const nsPoint& aOffset, bool aRepaint);
  nsRect GetVisualScrollRange() const;
  nsRect GetScrollRangeForUserInputEvents() const;

  /**
   * Return the 'optimal viewing region' as a rect suitable for use by
   * scroll anchoring. This rect is in the same coordinate space as
   * 'GetScrollPortRect', and accounts for 'scroll-padding' as defined by:
   *
   * https://drafts.csswg.org/css-scroll-snap-1/#optimal-viewing-region
   */
  nsRect GetVisualOptimalViewingRect() const;

  /**
   * For LTR frames, this is the same as GetVisualViewportOffset().
   * For RTL frames, we take the offset from the top right corner of the frame
   * to the top right corner of the visual viewport.
   */
  nsPoint GetLogicalVisualViewportOffset() const {
    nsPoint pt = GetVisualViewportOffset();
    if (!IsPhysicalLTR()) {
      pt.x += GetVisualViewportSize().width - mScrolledFrame->GetRect().width;
    }
    return pt;
  }
  void ScrollSnap(ScrollMode aMode = ScrollMode::SmoothMsd);
  void ScrollSnap(const nsPoint& aDestination,
                  ScrollMode aMode = ScrollMode::SmoothMsd);

  bool HasPendingScrollRestoration() const {
    return mRestorePos != nsPoint(-1, -1);
  }

  bool IsProcessingScrollEvent() const { return mProcessingScrollEvent; }

 public:
  static void AsyncScrollCallback(ScrollFrameHelper* aInstance,
                                  mozilla::TimeStamp aTime);
  static void AsyncSmoothMSDScrollCallback(ScrollFrameHelper* aInstance,
                                           mozilla::TimeDuration aDeltaTime);
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   * aRange is the range of allowable scroll positions around the desired
   * aScrollPosition. Null means only aScrollPosition is allowed.
   * This is a closed-ended range --- aRange.XMost()/aRange.YMost() are allowed.
   */
  void ScrollTo(nsPoint aScrollPosition, ScrollMode aMode,
                ScrollOrigin aOrigin = ScrollOrigin::NotSpecified,
                const nsRect* aRange = nullptr,
                nsIScrollbarMediator::ScrollSnapMode aSnap =
                    nsIScrollbarMediator::DISABLE_SNAP);
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void ScrollToCSSPixels(const CSSIntPoint& aScrollPosition,
                         ScrollMode aMode = ScrollMode::Instant,
                         nsIScrollbarMediator::ScrollSnapMode aSnap =
                             nsIScrollbarMediator::DEFAULT,
                         ScrollOrigin aOrigin = ScrollOrigin::NotSpecified);
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void ScrollToCSSPixelsApproximate(
      const mozilla::CSSPoint& aScrollPosition,
      ScrollOrigin aOrigin = ScrollOrigin::NotSpecified);

  CSSIntPoint GetScrollPositionCSSPixels();
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void ScrollToImpl(nsPoint aScrollPosition, const nsRect& aRange,
                    ScrollOrigin aOrigin = ScrollOrigin::NotSpecified);
  void ScrollVisual();
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void ScrollBy(nsIntPoint aDelta, mozilla::ScrollUnit aUnit, ScrollMode aMode,
                nsIntPoint* aOverflow,
                ScrollOrigin aOrigin = ScrollOrigin::NotSpecified,
                nsIScrollableFrame::ScrollMomentum aMomentum =
                    nsIScrollableFrame::NOT_MOMENTUM,
                nsIScrollbarMediator::ScrollSnapMode aSnap =
                    nsIScrollbarMediator::DISABLE_SNAP);
  void ScrollByCSSPixels(const CSSIntPoint& aDelta,
                         ScrollMode aMode = ScrollMode::Instant,
                         ScrollOrigin aOrigin = ScrollOrigin::NotSpecified,
                         nsIScrollbarMediator::ScrollSnapMode aSnap =
                             nsIScrollbarMediator::DEFAULT);
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void ScrollToRestoredPosition();

  enum class LoadingState { Loading, Stopped, Loaded };

  LoadingState GetPageLoadingState();

  /**
   * GetSnapPointForDestination determines which point to snap to after
   * scrolling. aStartPos gives the position before scrolling and aDestination
   * gives the position after scrolling, with no snapping. Behaviour is
   * dependent on the value of aUnit.
   * Returns true if a suitable snap point could be found and aDestination has
   * been updated to a valid snapping position.
   */
  bool GetSnapPointForDestination(mozilla::ScrollUnit aUnit,
                                  const nsPoint& aStartPos,
                                  nsPoint& aDestination);

  nsMargin GetScrollPadding() const;

  nsSize GetLineScrollAmount() const;
  nsSize GetPageScrollAmount() const;

  mozilla::UniquePtr<mozilla::PresState> SaveState() const;
  void RestoreState(mozilla::PresState* aState);

  nsIFrame* GetScrolledFrame() const { return mScrolledFrame; }
  nsIFrame* GetScrollbarBox(bool aVertical) const {
    return aVertical ? mVScrollbarBox : mHScrollbarBox;
  }

  void AddScrollPositionListener(nsIScrollPositionListener* aListener) {
    mListeners.AppendElement(aListener);
  }
  void RemoveScrollPositionListener(nsIScrollPositionListener* aListener) {
    mListeners.RemoveElement(aListener);
  }

  static void SetScrollbarVisibility(nsIFrame* aScrollbar, bool aVisible);

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
  nsRect GetScrolledRect() const;

  /**
   * GetUnsnappedScrolledRectInternal is designed to encapsulate deciding which
   * directions of overflow should be reachable by scrolling and which
   * should not.  Callers should NOT depend on it having any particular
   * behavior (although nsXULScrollFrame currently does).
   *
   * Currently it allows scrolling down and to the right for
   * nsHTMLScrollFrames with LTR directionality and for all
   * nsXULScrollFrames, and allows scrolling down and to the left for
   * nsHTMLScrollFrames with RTL directionality.
   */
  nsRect GetUnsnappedScrolledRectInternal(const nsRect& aScrolledOverflowArea,
                                          const nsSize& aScrollPortSize) const;

  layers::ScrollDirections GetAvailableScrollingDirectionsForUserInputEvents()
      const;

  layers::ScrollDirections GetScrollbarVisibility() const {
    layers::ScrollDirections result;
    if (mHasHorizontalScrollbar) {
      result += layers::ScrollDirection::eHorizontal;
    }
    if (mHasVerticalScrollbar) {
      result += layers::ScrollDirection::eVertical;
    }
    return result;
  }
  nsMargin GetActualScrollbarSizes(
      nsIScrollableFrame::ScrollbarSizesOptions aOptions =
          nsIScrollableFrame::ScrollbarSizesOptions::NONE) const;
  nsMargin GetDesiredScrollbarSizes(nsBoxLayoutState* aState);
  nscoord GetNondisappearingScrollbarWidth(nsBoxLayoutState* aState,
                                           mozilla::WritingMode aVerticalWM);
  bool IsPhysicalLTR() const {
    return mOuter->GetWritingMode().IsPhysicalLTR();
  }
  bool IsBidiLTR() const { return mOuter->GetWritingMode().IsBidiLTR(); }

 private:
  // NOTE: Use GetScrollStylesFromFrame() if you want to know `overflow`
  // and `overflow-behavior` properties.
  nsIFrame* GetFrameForStyle() const;

  // This is the for the old unspecced scroll snap implementation.
  ScrollSnapInfo ComputeOldScrollSnapInfo() const;
  // This is the for the scroll snap v1 implementation.
  ScrollSnapInfo ComputeScrollSnapInfo(
      const Maybe<nsPoint>& aDestination) const;

  bool NeedsScrollSnap() const;

 public:
  bool IsScrollbarOnRight() const;
  bool IsScrollingActive() const;
  bool IsScrollingActiveNotMinimalDisplayPort() const;
  bool IsMaybeAsynchronouslyScrolled() const {
    // If this is true, then we'll build an ASR, and that's what we want
    // to know I think.
    return mWillBuildScrollableLayer;
  }

  void ResetScrollPositionForLayerPixelAlignment() {
    mScrollPosForLayerPixelAlignment = GetScrollPosition();
  }

  bool ComputeCustomOverflow(mozilla::OverflowAreas& aOverflowAreas);

  void UpdateSticky();

  void UpdatePrevScrolledRect();

  bool IsRectNearlyVisible(const nsRect& aRect) const;
  nsRect ExpandRectToNearlyVisible(const nsRect& aRect) const;

  // adjust the scrollbar rectangle aRect to account for any visible resizer.
  // aHasResizer specifies if there is a content resizer, however this method
  // will also check if a widget resizer is present as well.
  void AdjustScrollbarRectForResizer(
      nsIFrame* aFrame, nsPresContext* aPresContext, nsRect& aRect,
      bool aHasResizer, mozilla::layers::ScrollDirection aDirection);
  // returns true if a resizer should be visible
  bool HasResizer() { return mResizerBox; }
  void LayoutScrollbars(nsBoxLayoutState& aState, const nsRect& aContentArea,
                        const nsRect& aOldScrollArea);

  void MarkScrollbarsDirtyForReflow() const;
  void InvalidateVerticalScrollbar() const;

  bool IsAlwaysActive() const;
  void MarkEverScrolled();
  void MarkRecentlyScrolled();
  void MarkNotRecentlyScrolled();
  nsExpirationState* GetExpirationState() { return &mActivityExpirationState; }

  void SetTransformingByAPZ(bool aTransforming) {
    if (mTransformingByAPZ && !aTransforming) {
      PostScrollEndEvent();
    }
    mTransformingByAPZ = aTransforming;
    if (!mozilla::css::TextOverflow::HasClippedTextOverflow(mOuter) ||
        mozilla::css::TextOverflow::HasBlockEllipsis(mScrolledFrame)) {
      // If the block has some overflow marker stuff we should kick off a paint
      // because we have special behaviour for it when APZ scrolling is active.
      mOuter->SchedulePaint();
    }
  }
  bool IsTransformingByAPZ() const { return mTransformingByAPZ; }
  void SetScrollableByAPZ(bool aScrollable);
  void SetZoomableByAPZ(bool aZoomable);
  void SetHasOutOfFlowContentInsideFilter();

  bool UsesOverlayScrollbars() const;

  // In the case where |aDestination| is given, elements which are entirely out
  // of view when the scroll position is moved to |aDestination| are not going
  // to be used for snap positions.
  ScrollSnapInfo GetScrollSnapInfo(
      const mozilla::Maybe<nsPoint>& aDestination) const;

  static bool ShouldActivateAllScrollFrames();
  nsRect RestrictToRootDisplayPort(const nsRect& aDisplayportBase);
  bool DecideScrollableLayer(nsDisplayListBuilder* aBuilder,
                             nsRect* aVisibleRect, nsRect* aDirtyRect,
                             bool aSetBase,
                             bool* aDirtyRectHasBeenOverriden = nullptr);
  void NotifyApzTransaction();
  void NotifyApproximateFrameVisibilityUpdate(bool aIgnoreDisplayPort);
  bool GetDisplayPortAtLastApproximateFrameVisibilityUpdate(
      nsRect* aDisplayPort);

  bool AllowDisplayPortExpiration();
  void TriggerDisplayPortExpiration();
  void ResetDisplayPortExpiryTimer();

  void ScheduleSyntheticMouseMove();
  static void ScrollActivityCallback(nsITimer* aTimer, void* anInstance);

  void HandleScrollbarStyleSwitching();

  ScrollOrigin LastScrollOrigin() const { return mLastScrollOrigin; }
  bool IsApzAnimationInProgress() const { return mApzAnimationInProgress; }
  ScrollGeneration CurrentScrollGeneration() const { return mScrollGeneration; }
  nsPoint LastScrollDestination() const { return mDestination; }
  nsTArray<ScrollPositionUpdate> GetScrollUpdates() const;
  bool HasScrollUpdates() const { return !mScrollUpdates.IsEmpty(); }

  bool IsLastScrollUpdateAnimating() const;
  using IncludeApzAnimation = nsIScrollableFrame::IncludeApzAnimation;
  bool IsScrollAnimating(IncludeApzAnimation = IncludeApzAnimation::Yes) const;

  void ResetScrollInfoIfNeeded(const ScrollGeneration& aGeneration,
                               bool aApzAnimationInProgress);
  bool WantAsyncScroll() const;
  Maybe<mozilla::layers::ScrollMetadata> ComputeScrollMetadata(
      LayerManager* aLayerManager, const nsIFrame* aContainerReferenceFrame,
      const mozilla::DisplayItemClip* aClip) const;
  // nsIScrollbarMediator
  void ScrollByPage(nsScrollbarFrame* aScrollbar, int32_t aDirection,
                    nsIScrollbarMediator::ScrollSnapMode aSnap =
                        nsIScrollbarMediator::DISABLE_SNAP);
  void ScrollByWhole(nsScrollbarFrame* aScrollbar, int32_t aDirection,
                     nsIScrollbarMediator::ScrollSnapMode aSnap =
                         nsIScrollbarMediator::DISABLE_SNAP);
  void ScrollByLine(nsScrollbarFrame* aScrollbar, int32_t aDirection,
                    nsIScrollbarMediator::ScrollSnapMode aSnap =
                        nsIScrollbarMediator::DISABLE_SNAP);
  void ScrollByUnit(nsScrollbarFrame* aScrollbar, ScrollMode aMode,
                    int32_t aDirection, ScrollUnit aUnit,
                    nsIScrollbarMediator::ScrollSnapMode aSnap =
                        nsIScrollbarMediator::DISABLE_SNAP);
  void RepeatButtonScroll(nsScrollbarFrame* aScrollbar);
  void ThumbMoved(nsScrollbarFrame* aScrollbar, nscoord aOldPos,
                  nscoord aNewPos);
  void ScrollbarReleased(nsScrollbarFrame* aScrollbar);
  bool ShouldSuppressScrollbarRepaints() const {
    return mSuppressScrollbarRepaints;
  }

  bool DragScroll(WidgetEvent* aEvent);

  void AsyncScrollbarDragInitiated(uint64_t aDragBlockId,
                                   mozilla::layers::ScrollDirection aDirection);
  void AsyncScrollbarDragRejected();

  bool IsRootScrollFrameOfDocument() const { return mIsRoot; }

  bool SmoothScrollVisual(
      const nsPoint& aVisualViewportOffset,
      mozilla::layers::FrameMetrics::ScrollOffsetUpdateType aUpdateType);

  bool IsSmoothScroll(mozilla::dom::ScrollBehavior aBehavior) const;

  // Update minimum-scale size.  The minimum-scale size will be set/used only
  // if there is overflow-x:hidden region.
  void UpdateMinimumScaleSize(const nsRect& aScrollableOverflow,
                              const nsSize& aICBSize);

  // Return the scroll frame's "true outer size".
  // This is mOuter->GetSize(), except when mOuter has been sized to reflect
  // a virtual (layout) viewport in which case this returns the outer size
  // used to size the physical (visual) viewport.
  nsSize TrueOuterSize(nsDisplayListBuilder* aBuilder) const;

  already_AddRefed<Element> MakeScrollbar(dom::NodeInfo* aNodeInfo,
                                          bool aVertical,
                                          AnonymousContentKey& aKey);

  void AppendScrollUpdate(const ScrollPositionUpdate& aUpdate);

  // owning references to the nsIAnonymousContentCreator-built content
  nsCOMPtr<Element> mHScrollbarContent;
  nsCOMPtr<Element> mVScrollbarContent;
  nsCOMPtr<Element> mScrollCornerContent;
  nsCOMPtr<Element> mResizerContent;

  class ScrollEvent;
  class ScrollEndEvent;
  class AsyncScrollPortEvent;
  class ScrolledAreaEvent;

  RefPtr<ScrollEvent> mScrollEvent;
  RefPtr<ScrollEndEvent> mScrollEndEvent;
  nsRevocableEventPtr<AsyncScrollPortEvent> mAsyncScrollPortEvent;
  nsRevocableEventPtr<ScrolledAreaEvent> mScrolledAreaEvent;
  nsIFrame* mHScrollbarBox;
  nsIFrame* mVScrollbarBox;
  nsIFrame* mScrolledFrame;
  nsIFrame* mScrollCornerBox;
  nsIFrame* mResizerBox;
  nsContainerFrame* mOuter;
  const nsIFrame* mReferenceFrameDuringPainting;
  RefPtr<AsyncScroll> mAsyncScroll;
  RefPtr<AsyncSmoothMSDScroll> mAsyncSmoothMSDScroll;
  RefPtr<ScrollbarActivity> mScrollbarActivity;
  nsTArray<nsIScrollPositionListener*> mListeners;
  ScrollOrigin mLastScrollOrigin;
  Maybe<nsPoint> mApzSmoothScrollDestination;
  ScrollGeneration mScrollGeneration;

  nsTArray<ScrollPositionUpdate> mScrollUpdates;

  // NOTE: On mobile this value might be factoring into overflow:hidden region
  // in the case of the top level document.
  nsRect mScrollPort;
  nsSize mMinimumScaleSize;

  // Stores the ICB size for the root document if this frame is using the
  // minimum scale size for |mScrollPort|.
  nsSize mICBSize;

  // Where we're currently scrolling to, if we're scrolling asynchronously.
  // If we're not in the middle of an asynchronous scroll then this is
  // just the current scroll position. ScrollBy will choose its
  // destination based on this value.
  nsPoint mDestination;

  // A goal position to try to scroll to as content loads. As long as mLastPos
  // matches the current logical scroll position, we try to scroll to
  // mRestorePos after every reflow --- because after each time content is
  // loaded/added to the scrollable element, there will be a reflow.
  // Note that for frames where layout and visual viewport aren't one and the
  // same thing, this scroll position will be the logical scroll position of
  // the *visual* viewport, as its position will be more relevant to the user.
  nsPoint mRestorePos;
  // The last logical position we scrolled to while trying to restore
  // mRestorePos, or 0,0 when this is a new frame. Set to -1,-1 once we've
  // scrolled for any reason other than trying to restore mRestorePos.
  // Just as with mRestorePos, this position will be the logical position of
  // the *visual* viewport where available.
  nsPoint mLastPos;

  // The latest scroll position we've sent or received from APZ. This
  // represents the main thread's best knowledge of the APZ scroll position,
  // and is used to calculate relative scroll offset updates.
  nsPoint mApzScrollPos;

  nsExpirationState mActivityExpirationState;

  nsCOMPtr<nsITimer> mScrollActivityTimer;
  nsPoint mScrollPosForLayerPixelAlignment;

  // The scroll position where we last updated frame visibility.
  nsPoint mLastUpdateFramesPos;
  nsRect mDisplayPortAtLastFrameUpdate;

  nsRect mPrevScrolledRect;

  ScrollableLayerGuid::ViewID mScrollParentID;

  // Timer to remove the displayport some time after scrolling has stopped
  nsCOMPtr<nsITimer> mDisplayPortExpiryTimer;

  ScrollAnchorContainer mAnchor;

  bool mAllowScrollOriginDowngrade : 1;
  bool mHadDisplayPortAtLastFrameUpdate : 1;
  bool mNeverHasVerticalScrollbar : 1;
  bool mNeverHasHorizontalScrollbar : 1;
  bool mHasVerticalScrollbar : 1;
  bool mHasHorizontalScrollbar : 1;
  // If mHas(Vertical|Horizontal)Scrollbar is true then
  // mOnlyNeed(V|H)ScrollbarToScrollVVInsideLV indicates if the only reason we
  // need that scrollbar is to scroll the visual viewport inside the layout
  // viewport. These scrollbars are special in that even if they are layout
  // scrollbars they do not take up any layout space.
  bool mOnlyNeedVScrollbarToScrollVVInsideLV : 1;
  bool mOnlyNeedHScrollbarToScrollVVInsideLV : 1;
  bool mFrameIsUpdatingScrollbar : 1;
  bool mDidHistoryRestore : 1;
  // Is this the scrollframe for the document's viewport?
  bool mIsRoot : 1;
  // True if we should clip all descendants, false if we should only clip
  // descendants for which we are the containing block.
  bool mClipAllDescendants : 1;
  // If true, don't try to layout the scrollbars in Reflow().  This can be
  // useful if multiple passes are involved, because we don't want to place the
  // scrollbars at the wrong size.
  bool mSuppressScrollbarUpdate : 1;
  // If true, we skipped a scrollbar layout due to mSuppressScrollbarUpdate
  // being set at some point.  That means we should lay out scrollbars even if
  // it might not strictly be needed next time mSuppressScrollbarUpdate is
  // false.
  bool mSkippedScrollbarLayout : 1;

  bool mHadNonInitialReflow : 1;
  // Initially true; first call to ReflowFinished() sets it to false.
  bool mFirstReflow : 1;
  // State used only by PostScrollEvents so we know
  // which overflow states have changed.
  bool mHorizontalOverflow : 1;
  bool mVerticalOverflow : 1;
  bool mPostedReflowCallback : 1;
  bool mMayHaveDirtyFixedChildren : 1;
  // If true, need to actually update our scrollbar attributes in the
  // reflow callback.
  bool mUpdateScrollbarAttributes : 1;
  // If true, we should be prepared to scroll using this scrollframe
  // by placing descendant content into its own layer(s)
  bool mHasBeenScrolledRecently : 1;

  // If true, the scroll frame should always be active because we always build
  // a scrollable layer. Used for asynchronous scrolling.
  bool mWillBuildScrollableLayer : 1;

  // If true, the scroll frame is an ancestor of other "active" scrolling
  // frames, where "active" means has a non-minimal display port if
  // ShouldActivateAllScrollFrames is true, or has a display port if
  // ShouldActivateAllScrollFrames is false. And this means that we shouldn't
  // expire the display port (if ShouldActivateAllScrollFrames is true then
  // expiring a display port means making it minimal, otherwise it means
  // removing the display port). If those descendant scrollframes have their
  // display ports removed or made minimal, then we expire our display port.
  bool mIsParentToActiveScrollFrames : 1;

  // If true, add clipping in ScrollFrameHelper::ClipLayerToDisplayPort.
  // XXX this flag needs some auditing and better documentation, bug 1646686.
  bool mAddClipRectToLayer : 1;

  // True if this frame has been scrolled at least once
  bool mHasBeenScrolled : 1;

  // True if the events synthesized by OSX to produce momentum scrolling should
  // be ignored.  Reset when the next real, non-synthesized scroll event occurs.
  bool mIgnoreMomentumScroll : 1;

  // True if the APZ is in the process of async-transforming this scrollframe,
  // (as best as we can tell on the main thread, anyway).
  bool mTransformingByAPZ : 1;

  // True if APZ can scroll this frame asynchronously (i.e. it has an APZC
  // set up for this frame and it's not a scrollinfo layer).
  bool mScrollableByAPZ : 1;

  // True if the APZ is allowed to zoom this scrollframe.
  bool mZoomableByAPZ : 1;

  // True if the scroll frame contains out-of-flow content and is inside
  // a CSS filter.
  bool mHasOutOfFlowContentInsideFilter : 1;

  // True if we don't want the scrollbar to repaint itself right now.
  bool mSuppressScrollbarRepaints : 1;

  // True if we are using the minimum scale size instead of ICB for scroll port.
  bool mIsUsingMinimumScaleSize : 1;

  // True if the minimum scale size has been changed since the last reflow.
  bool mMinimumScaleSizeChanged : 1;

  // True if we're processing an scroll event.
  bool mProcessingScrollEvent : 1;

  // Whether an APZ animation is in progress. Note that this is only set to true
  // when repainted via APZ, which means that there may be a request for an APZ
  // animation in flight for example, while this is still false. In order to
  // answer "is an APZ animation in the process of starting or in progress" you
  // need to check mScrollUpdates, mApzAnimationRequested, and this bit.
  bool mApzAnimationInProgress : 1;
  // This is true from the time a scroll animation is requested of APZ to the
  // time that APZ responds with an up-to-date repaint request. More precisely,
  // this is flipped to true if a repaint request is dispatched to APZ where
  // the most recent scroll request is a smooth scroll, and it is cleared when
  // mApzAnimationInProgress is updated.
  bool mApzAnimationRequested : 1;

  // Whether we need to reclamp the visual viewport offset in ReflowFinished.
  bool mReclampVVOffsetInReflowFinished : 1;

  mozilla::layout::ScrollVelocityQueue mVelocityQueue;

 protected:
  class AutoScrollbarRepaintSuppression;
  friend class AutoScrollbarRepaintSuppression;
  class AutoScrollbarRepaintSuppression {
   public:
    AutoScrollbarRepaintSuppression(ScrollFrameHelper* aHelper,
                                    AutoWeakFrame& aWeakOuter, bool aSuppress)
        : mHelper(aHelper),
          mWeakOuter(aWeakOuter),
          mOldSuppressValue(aHelper->mSuppressScrollbarRepaints) {
      mHelper->mSuppressScrollbarRepaints = aSuppress;
    }

    ~AutoScrollbarRepaintSuppression() {
      if (mWeakOuter.IsAlive()) {
        mHelper->mSuppressScrollbarRepaints = mOldSuppressValue;
      }
    }

   private:
    ScrollFrameHelper* mHelper;
    AutoWeakFrame& mWeakOuter;
    bool mOldSuppressValue;
  };

  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void ScrollToWithOrigin(nsPoint aScrollPosition, ScrollMode aMode,
                          ScrollOrigin aOrigin, const nsRect* aRange,
                          nsIScrollbarMediator::ScrollSnapMode aSnap =
                              nsIScrollbarMediator::DISABLE_SNAP);

  void CompleteAsyncScroll(const nsRect& aRange,
                           ScrollOrigin aOrigin = ScrollOrigin::NotSpecified);

  bool HasPerspective() const { return mOuter->ChildrenHavePerspective(); }
  bool HasBgAttachmentLocal() const;
  mozilla::StyleDirection GetScrolledFrameDir() const;

  // Ask APZ to smooth scroll to |aDestination|.
  // This method does not clamp the destination; callers should clamp it to
  // either the layout or the visual scroll range (APZ will happily smooth
  // scroll to either).
  void ApzSmoothScrollTo(const nsPoint& aDestination, ScrollOrigin aOrigin);

  // Removes any RefreshDriver observers we might have registered.
  void RemoveObservers();
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(ScrollFrameHelper::OverflowState)

}  // namespace mozilla

/**
 * The scroll frame creates and manages the scrolling view
 *
 * It only supports having a single child frame that typically is an area
 * frame, but doesn't have to be. The child frame must have a view, though
 *
 * Scroll frames don't support incremental changes, i.e. you can't replace
 * or remove the scrolled frame
 */
class nsHTMLScrollFrame : public nsContainerFrame,
                          public nsIScrollableFrame,
                          public nsIAnonymousContentCreator,
                          public nsIStatefulFrame {
 public:
  typedef mozilla::ScrollFrameHelper ScrollFrameHelper;
  typedef mozilla::CSSIntPoint CSSIntPoint;
  typedef mozilla::ScrollReflowInput ScrollReflowInput;
  typedef mozilla::layout::ScrollAnchorContainer ScrollAnchorContainer;
  friend nsHTMLScrollFrame* NS_NewHTMLScrollFrame(
      mozilla::PresShell* aPresShell, ComputedStyle* aStyle, bool aIsRoot);

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsHTMLScrollFrame)

  void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                        const nsDisplayListSet& aLists) override {
    mHelper.BuildDisplayList(aBuilder, aLists);
  }

  bool TryLayout(ScrollReflowInput* aState, ReflowOutput* aKidMetrics,
                 bool aAssumeVScroll, bool aAssumeHScroll, bool aForce);
  bool ScrolledContentDependsOnHeight(ScrollReflowInput* aState);
  void ReflowScrolledFrame(ScrollReflowInput* aState, bool aAssumeHScroll,
                           bool aAssumeVScroll, ReflowOutput* aMetrics);
  void ReflowContents(ScrollReflowInput* aState,
                      const ReflowOutput& aDesiredSize);
  void PlaceScrollArea(ScrollReflowInput& aState,
                       const nsPoint& aScrollPosition);
  nscoord GetIntrinsicVScrollbarWidth(gfxContext* aRenderingContext);

  bool GetBorderRadii(const nsSize& aFrameSize, const nsSize& aBorderArea,
                      Sides aSkipSides, nscoord aRadii[8]) const final {
    return mHelper.GetBorderRadii(aFrameSize, aBorderArea, aSkipSides, aRadii);
  }

  nscoord GetMinISize(gfxContext* aRenderingContext) override;
  nscoord GetPrefISize(gfxContext* aRenderingContext) override;
  nsresult GetXULPadding(nsMargin& aPadding) final;
  bool IsXULCollapsed() final;

  void Reflow(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
              const ReflowInput& aReflowInput,
              nsReflowStatus& aStatus) override;
  void DidReflow(nsPresContext* aPresContext,
                 const ReflowInput* aReflowInput) override;

  bool ComputeCustomOverflow(mozilla::OverflowAreas& aOverflowAreas) final {
    return mHelper.ComputeCustomOverflow(aOverflowAreas);
  }

  nscoord GetLogicalBaseline(mozilla::WritingMode aWritingMode) const final;

  bool GetVerticalAlignBaseline(mozilla::WritingMode aWM,
                                nscoord* aBaseline) const final {
    NS_ASSERTION(!aWM.IsOrthogonalTo(GetWritingMode()),
                 "You should only call this on frames with a WM that's "
                 "parallel to aWM");
    *aBaseline = GetLogicalBaseline(aWM);
    return true;
  }

  // Recomputes the scrollable overflow area we store in the helper to take
  // children that are affected by perpsective set on the outer frame and scroll
  // at different rates.
  void AdjustForPerspective(nsRect& aScrollableOverflow);

  // Called to set the child frames. We typically have three: the scroll area,
  // the vertical scrollbar, and the horizontal scrollbar.
  void SetInitialChildList(ChildListID aListID,
                           nsFrameList& aChildList) override;
  void AppendFrames(ChildListID aListID, nsFrameList& aFrameList) final;
  void InsertFrames(ChildListID aListID, nsIFrame* aPrevFrame,
                    const nsLineList::iterator* aPrevFrameLine,
                    nsFrameList& aFrameList) final;
  void RemoveFrame(ChildListID aListID, nsIFrame* aOldFrame) final;

  void DestroyFrom(nsIFrame* aDestructRoot, PostDestroyData&) override;

  nsIScrollableFrame* GetScrollTargetFrame() const final {
    return const_cast<nsHTMLScrollFrame*>(this);
  }

  nsContainerFrame* GetContentInsertionFrame() override {
    return mHelper.GetScrolledFrame()->GetContentInsertionFrame();
  }

  bool DoesClipChildrenInBothAxes() final { return true; }

  nsPoint GetPositionOfChildIgnoringScrolling(const nsIFrame* aChild) final {
    nsPoint pt = aChild->GetPosition();
    if (aChild == mHelper.GetScrolledFrame()) pt += GetScrollPosition();
    return pt;
  }

  // nsIAnonymousContentCreator
  nsresult CreateAnonymousContent(nsTArray<ContentInfo>&) final;
  void AppendAnonymousContentTo(nsTArray<nsIContent*>&, uint32_t aFilter) final;

  // nsIScrollableFrame
  nsIFrame* GetScrolledFrame() const final {
    return mHelper.GetScrolledFrame();
  }
  mozilla::ScrollStyles GetScrollStyles() const override {
    return mHelper.GetScrollStylesFromFrame();
  }
  bool IsForTextControlWithNoScrollbars() const final {
    return mHelper.IsForTextControlWithNoScrollbars();
  }
  mozilla::layers::OverscrollBehaviorInfo GetOverscrollBehaviorInfo()
      const final {
    return mHelper.GetOverscrollBehaviorInfo();
  }
  mozilla::layers::ScrollDirections
  GetAvailableScrollingDirectionsForUserInputEvents() const final {
    return mHelper.GetAvailableScrollingDirectionsForUserInputEvents();
  }
  mozilla::layers::ScrollDirections GetScrollbarVisibility() const final {
    return mHelper.GetScrollbarVisibility();
  }
  nsMargin GetActualScrollbarSizes(
      nsIScrollableFrame::ScrollbarSizesOptions aOptions =
          nsIScrollableFrame::ScrollbarSizesOptions::NONE) const final {
    return mHelper.GetActualScrollbarSizes(aOptions);
  }
  nsMargin GetDesiredScrollbarSizes(nsBoxLayoutState* aState) final {
    return mHelper.GetDesiredScrollbarSizes(aState);
  }
  nsMargin GetDesiredScrollbarSizes(nsPresContext* aPresContext,
                                    gfxContext* aRC) final {
    nsBoxLayoutState bls(aPresContext, aRC, 0);
    return GetDesiredScrollbarSizes(&bls);
  }
  nscoord GetNondisappearingScrollbarWidth(nsPresContext* aPresContext,
                                           gfxContext* aRC,
                                           mozilla::WritingMode aWM) final {
    nsBoxLayoutState bls(aPresContext, aRC, 0);
    return mHelper.GetNondisappearingScrollbarWidth(&bls, aWM);
  }
  nsSize GetLayoutSize() const final { return mHelper.GetLayoutSize(); }
  nsRect GetScrolledRect() const final { return mHelper.GetScrolledRect(); }
  nsRect GetScrollPortRect() const final { return mHelper.GetScrollPortRect(); }
  nsPoint GetScrollPosition() const final {
    return mHelper.GetScrollPosition();
  }
  nsPoint GetLogicalScrollPosition() const final {
    return mHelper.GetLogicalScrollPosition();
  }
  nsRect GetScrollRange() const final { return mHelper.GetLayoutScrollRange(); }
  nsSize GetVisualViewportSize() const final {
    return mHelper.GetVisualViewportSize();
  }
  nsPoint GetVisualViewportOffset() const final {
    return mHelper.GetVisualViewportOffset();
  }
  bool SetVisualViewportOffset(const nsPoint& aOffset, bool aRepaint) final {
    return mHelper.SetVisualViewportOffset(aOffset, aRepaint);
  }
  nsRect GetVisualScrollRange() const final {
    return mHelper.GetVisualScrollRange();
  }
  nsRect GetScrollRangeForUserInputEvents() const final {
    return mHelper.GetScrollRangeForUserInputEvents();
  }
  nsSize GetLineScrollAmount() const final {
    return mHelper.GetLineScrollAmount();
  }
  nsSize GetPageScrollAmount() const final {
    return mHelper.GetPageScrollAmount();
  }
  nsMargin GetScrollPadding() const final { return mHelper.GetScrollPadding(); }
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void ScrollTo(nsPoint aScrollPosition, ScrollMode aMode,
                const nsRect* aRange = nullptr,
                nsIScrollbarMediator::ScrollSnapMode aSnap =
                    nsIScrollbarMediator::DISABLE_SNAP) final {
    mHelper.ScrollTo(aScrollPosition, aMode, ScrollOrigin::Other, aRange,
                     aSnap);
  }
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void ScrollToCSSPixels(
      const CSSIntPoint& aScrollPosition,
      ScrollMode aMode = ScrollMode::Instant,
      nsIScrollbarMediator::ScrollSnapMode aSnap =
          nsIScrollbarMediator::DEFAULT,
      ScrollOrigin aOrigin = ScrollOrigin::NotSpecified) final {
    mHelper.ScrollToCSSPixels(aScrollPosition, aMode, aSnap, aOrigin);
  }
  void ScrollToCSSPixelsApproximate(
      const mozilla::CSSPoint& aScrollPosition,
      ScrollOrigin aOrigin = ScrollOrigin::NotSpecified) final {
    mHelper.ScrollToCSSPixelsApproximate(aScrollPosition, aOrigin);
  }
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  CSSIntPoint GetScrollPositionCSSPixels() final {
    return mHelper.GetScrollPositionCSSPixels();
  }
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void ScrollBy(nsIntPoint aDelta, mozilla::ScrollUnit aUnit, ScrollMode aMode,
                nsIntPoint* aOverflow,
                ScrollOrigin aOrigin = ScrollOrigin::NotSpecified,
                nsIScrollableFrame::ScrollMomentum aMomentum =
                    nsIScrollableFrame::NOT_MOMENTUM,
                nsIScrollbarMediator::ScrollSnapMode aSnap =
                    nsIScrollbarMediator::DISABLE_SNAP) final {
    mHelper.ScrollBy(aDelta, aUnit, aMode, aOverflow, aOrigin, aMomentum,
                     aSnap);
  }
  void ScrollByCSSPixels(const CSSIntPoint& aDelta,
                         ScrollMode aMode = ScrollMode::Instant,
                         ScrollOrigin aOrigin = ScrollOrigin::NotSpecified,
                         nsIScrollbarMediator::ScrollSnapMode aSnap =
                             nsIScrollbarMediator::DEFAULT) final {
    mHelper.ScrollByCSSPixels(aDelta, aMode, aOrigin, aSnap);
  }
  void ScrollSnap() final { mHelper.ScrollSnap(); }
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void ScrollToRestoredPosition() final { mHelper.ScrollToRestoredPosition(); }
  void AddScrollPositionListener(nsIScrollPositionListener* aListener) final {
    mHelper.AddScrollPositionListener(aListener);
  }
  void RemoveScrollPositionListener(
      nsIScrollPositionListener* aListener) final {
    mHelper.RemoveScrollPositionListener(aListener);
  }
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void CurPosAttributeChanged(nsIContent* aChild) final {
    mHelper.CurPosAttributeChanged(aChild);
  }
  NS_IMETHOD PostScrolledAreaEventForCurrentArea() final {
    mHelper.PostScrolledAreaEvent();
    return NS_OK;
  }
  bool IsScrollingActive() final { return mHelper.IsScrollingActive(); }
  bool IsScrollingActiveNotMinimalDisplayPort() final {
    return mHelper.IsScrollingActiveNotMinimalDisplayPort();
  }
  bool IsMaybeAsynchronouslyScrolled() final {
    return mHelper.IsMaybeAsynchronouslyScrolled();
  }
  void ResetScrollPositionForLayerPixelAlignment() final {
    mHelper.ResetScrollPositionForLayerPixelAlignment();
  }
  bool DidHistoryRestore() const final { return mHelper.mDidHistoryRestore; }
  void ClearDidHistoryRestore() final { mHelper.mDidHistoryRestore = false; }
  void MarkEverScrolled() final { mHelper.MarkEverScrolled(); }
  bool IsRectNearlyVisible(const nsRect& aRect) final {
    return mHelper.IsRectNearlyVisible(aRect);
  }
  nsRect ExpandRectToNearlyVisible(const nsRect& aRect) const final {
    return mHelper.ExpandRectToNearlyVisible(aRect);
  }
  ScrollOrigin LastScrollOrigin() final { return mHelper.LastScrollOrigin(); }
  bool IsScrollAnimating(IncludeApzAnimation aIncludeApz) final {
    return mHelper.IsScrollAnimating(aIncludeApz);
  }
  mozilla::ScrollGeneration CurrentScrollGeneration() const final {
    return mHelper.CurrentScrollGeneration();
  }
  nsPoint LastScrollDestination() final {
    return mHelper.LastScrollDestination();
  }
  nsTArray<mozilla::ScrollPositionUpdate> GetScrollUpdates() const final {
    return mHelper.GetScrollUpdates();
  }
  bool HasScrollUpdates() const final { return mHelper.HasScrollUpdates(); }
  void ResetScrollInfoIfNeeded(const mozilla::ScrollGeneration& aGeneration,
                               bool aApzAnimationInProgress) final {
    mHelper.ResetScrollInfoIfNeeded(aGeneration, aApzAnimationInProgress);
  }
  bool WantAsyncScroll() const final { return mHelper.WantAsyncScroll(); }
  mozilla::Maybe<mozilla::layers::ScrollMetadata> ComputeScrollMetadata(
      LayerManager* aLayerManager, const nsIFrame* aContainerReferenceFrame,
      const mozilla::DisplayItemClip* aClip) const final {
    return mHelper.ComputeScrollMetadata(aLayerManager,
                                         aContainerReferenceFrame, aClip);
  }
  void MarkScrollbarsDirtyForReflow() const final {
    mHelper.MarkScrollbarsDirtyForReflow();
  }
  void InvalidateVerticalScrollbar() const final {
    mHelper.InvalidateVerticalScrollbar();
  }

  void UpdateScrollbarPosition() final { mHelper.UpdateScrollbarPosition(); }
  bool DecideScrollableLayer(nsDisplayListBuilder* aBuilder,
                             nsRect* aVisibleRect, nsRect* aDirtyRect,
                             bool aSetBase) final {
    return mHelper.DecideScrollableLayer(aBuilder, aVisibleRect, aDirtyRect,
                                         aSetBase);
  }
  void NotifyApzTransaction() final { mHelper.NotifyApzTransaction(); }
  void NotifyApproximateFrameVisibilityUpdate(bool aIgnoreDisplayPort) final {
    mHelper.NotifyApproximateFrameVisibilityUpdate(aIgnoreDisplayPort);
  }
  bool GetDisplayPortAtLastApproximateFrameVisibilityUpdate(
      nsRect* aDisplayPort) final {
    return mHelper.GetDisplayPortAtLastApproximateFrameVisibilityUpdate(
        aDisplayPort);
  }
  void TriggerDisplayPortExpiration() final {
    mHelper.TriggerDisplayPortExpiration();
  }

  // nsIStatefulFrame
  mozilla::UniquePtr<mozilla::PresState> SaveState() final {
    return mHelper.SaveState();
  }
  NS_IMETHOD RestoreState(mozilla::PresState* aState) final {
    NS_ENSURE_ARG_POINTER(aState);
    mHelper.RestoreState(aState);
    return NS_OK;
  }

  // nsIScrollbarMediator
  void ScrollByPage(nsScrollbarFrame* aScrollbar, int32_t aDirection,
                    nsIScrollbarMediator::ScrollSnapMode aSnap =
                        nsIScrollbarMediator::DISABLE_SNAP) final {
    mHelper.ScrollByPage(aScrollbar, aDirection, aSnap);
  }
  void ScrollByWhole(nsScrollbarFrame* aScrollbar, int32_t aDirection,
                     nsIScrollbarMediator::ScrollSnapMode aSnap =
                         nsIScrollbarMediator::DISABLE_SNAP) final {
    mHelper.ScrollByWhole(aScrollbar, aDirection, aSnap);
  }
  void ScrollByLine(nsScrollbarFrame* aScrollbar, int32_t aDirection,
                    nsIScrollbarMediator::ScrollSnapMode aSnap =
                        nsIScrollbarMediator::DISABLE_SNAP) final {
    mHelper.ScrollByLine(aScrollbar, aDirection, aSnap);
  }
  void ScrollByUnit(nsScrollbarFrame* aScrollbar, ScrollMode aMode,
                    int32_t aDirection, mozilla::ScrollUnit aUnit,
                    ScrollSnapMode aSnap = DISABLE_SNAP) final {
    mHelper.ScrollByUnit(aScrollbar, aMode, aDirection, aUnit, aSnap);
  }
  void RepeatButtonScroll(nsScrollbarFrame* aScrollbar) final {
    mHelper.RepeatButtonScroll(aScrollbar);
  }
  void ThumbMoved(nsScrollbarFrame* aScrollbar, nscoord aOldPos,
                  nscoord aNewPos) final {
    mHelper.ThumbMoved(aScrollbar, aOldPos, aNewPos);
  }
  void ScrollbarReleased(nsScrollbarFrame* aScrollbar) final {
    mHelper.ScrollbarReleased(aScrollbar);
  }
  void VisibilityChanged(bool aVisible) final {}
  nsIFrame* GetScrollbarBox(bool aVertical) final {
    return mHelper.GetScrollbarBox(aVertical);
  }
  void ScrollbarActivityStarted() const final;
  void ScrollbarActivityStopped() const final;

  bool IsScrollbarOnRight() const final { return mHelper.IsScrollbarOnRight(); }

  bool ShouldSuppressScrollbarRepaints() const final {
    return mHelper.ShouldSuppressScrollbarRepaints();
  }

  void SetTransformingByAPZ(bool aTransforming) final {
    mHelper.SetTransformingByAPZ(aTransforming);
  }
  bool IsTransformingByAPZ() const final {
    return mHelper.IsTransformingByAPZ();
  }
  void SetScrollableByAPZ(bool aScrollable) final {
    mHelper.SetScrollableByAPZ(aScrollable);
  }
  void SetZoomableByAPZ(bool aZoomable) final {
    mHelper.SetZoomableByAPZ(aZoomable);
  }
  void SetHasOutOfFlowContentInsideFilter() final {
    mHelper.SetHasOutOfFlowContentInsideFilter();
  }

  ScrollSnapInfo GetScrollSnapInfo() const final {
    return mHelper.GetScrollSnapInfo(Nothing());
  }

  bool DragScroll(mozilla::WidgetEvent* aEvent) final {
    return mHelper.DragScroll(aEvent);
  }

  void AsyncScrollbarDragInitiated(
      uint64_t aDragBlockId,
      mozilla::layers::ScrollDirection aDirection) final {
    return mHelper.AsyncScrollbarDragInitiated(aDragBlockId, aDirection);
  }

  void AsyncScrollbarDragRejected() final {
    return mHelper.AsyncScrollbarDragRejected();
  }

  bool IsRootScrollFrameOfDocument() const final {
    return mHelper.IsRootScrollFrameOfDocument();
  }

  const ScrollAnchorContainer* Anchor() const final { return &mHelper.mAnchor; }

  ScrollAnchorContainer* Anchor() final { return &mHelper.mAnchor; }

  // Return the scrolled frame.
  void AppendDirectlyOwnedAnonBoxes(nsTArray<OwnedAnonBox>& aResult) final {
    aResult.AppendElement(OwnedAnonBox(mHelper.GetScrolledFrame()));
  }

  bool SmoothScrollVisual(
      const nsPoint& aVisualViewportOffset,
      mozilla::layers::FrameMetrics::ScrollOffsetUpdateType aUpdateType) final {
    return mHelper.SmoothScrollVisual(aVisualViewportOffset, aUpdateType);
  }

  bool IsSmoothScroll(mozilla::dom::ScrollBehavior aBehavior) const final {
    return mHelper.IsSmoothScroll(aBehavior);
  }

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override;
#endif

#ifdef ACCESSIBILITY
  mozilla::a11y::AccType AccessibleType() override;
#endif

 protected:
  nsHTMLScrollFrame(ComputedStyle* aStyle, nsPresContext* aPresContext,
                    bool aIsRoot)
      : nsHTMLScrollFrame(aStyle, aPresContext, kClassID, aIsRoot) {}

  nsHTMLScrollFrame(ComputedStyle* aStyle, nsPresContext* aPresContext,
                    nsIFrame::ClassID aID, bool aIsRoot);
  void SetSuppressScrollbarUpdate(bool aSuppress) {
    mHelper.mSuppressScrollbarUpdate = aSuppress;
  }
  bool GuessHScrollbarNeeded(const ScrollReflowInput& aState);
  bool GuessVScrollbarNeeded(const ScrollReflowInput& aState);

  bool IsScrollbarUpdateSuppressed() const {
    return mHelper.mSuppressScrollbarUpdate;
  }

  // Return whether we're in an "initial" reflow.  Some reflows with
  // NS_FRAME_FIRST_REFLOW set are NOT "initial" as far as we're concerned.
  bool InInitialReflow() const;

  /**
   * Override this to return false if computed bsize/min-bsize/max-bsize
   * should NOT be propagated to child content.
   * nsListControlFrame uses this.
   */
  virtual bool ShouldPropagateComputedBSizeToScrolledContent() const {
    return true;
  }

 private:
  friend class mozilla::ScrollFrameHelper;
  ScrollFrameHelper mHelper;
};

/**
 * The scroll frame creates and manages the scrolling view
 *
 * It only supports having a single child frame that typically is an area
 * frame, but doesn't have to be. The child frame must have a view, though
 *
 * Scroll frames don't support incremental changes, i.e. you can't replace
 * or remove the scrolled frame
 */
class nsXULScrollFrame final : public nsBoxFrame,
                               public nsIScrollableFrame,
                               public nsIAnonymousContentCreator,
                               public nsIStatefulFrame {
 public:
  typedef mozilla::ScrollFrameHelper ScrollFrameHelper;
  typedef mozilla::CSSIntPoint CSSIntPoint;
  typedef mozilla::layout::ScrollAnchorContainer ScrollAnchorContainer;

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsXULScrollFrame)

  friend nsXULScrollFrame* NS_NewXULScrollFrame(mozilla::PresShell* aPresShell,
                                                ComputedStyle* aStyle,
                                                bool aIsRoot,
                                                bool aClipAllDescendants);

  void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                        const nsDisplayListSet& aLists) final {
    mHelper.BuildDisplayList(aBuilder, aLists);
  }

  // XXXldb Is this actually used?
#if 0
  nscoord GetMinISize(gfxContext *aRenderingContext) final;
#endif

  bool ComputeCustomOverflow(mozilla::OverflowAreas& aOverflowAreas) final {
    return mHelper.ComputeCustomOverflow(aOverflowAreas);
  }

  bool GetVerticalAlignBaseline(mozilla::WritingMode aWM,
                                nscoord* aBaseline) const final {
    *aBaseline = GetLogicalBaseline(aWM);
    return true;
  }

  // Called to set the child frames. We typically have three: the scroll area,
  // the vertical scrollbar, and the horizontal scrollbar.
  void SetInitialChildList(ChildListID aListID, nsFrameList& aChildList) final;
  void AppendFrames(ChildListID aListID, nsFrameList& aFrameList) final;
  void InsertFrames(ChildListID aListID, nsIFrame* aPrevFrame,
                    const nsLineList::iterator* aPrevFrameLine,
                    nsFrameList& aFrameList) final;
  void RemoveFrame(ChildListID aListID, nsIFrame* aOldFrame) final;

  void DestroyFrom(nsIFrame* aDestructRoot,
                   PostDestroyData& aPostDestroyData) final;

  nsIScrollableFrame* GetScrollTargetFrame() const final {
    return const_cast<nsXULScrollFrame*>(this);
  }

  nsContainerFrame* GetContentInsertionFrame() final {
    return mHelper.GetScrolledFrame()->GetContentInsertionFrame();
  }

  bool DoesClipChildrenInBothAxes() final { return true; }

  nsPoint GetPositionOfChildIgnoringScrolling(const nsIFrame* aChild) final {
    nsPoint pt = aChild->GetPosition();
    if (aChild == mHelper.GetScrolledFrame())
      pt += mHelper.GetLogicalScrollPosition();
    return pt;
  }

  // nsIAnonymousContentCreator
  nsresult CreateAnonymousContent(nsTArray<ContentInfo>& aElements) final;
  void AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                uint32_t aFilter) final;

  nsSize GetXULMinSize(nsBoxLayoutState& aBoxLayoutState) final;
  nsSize GetXULPrefSize(nsBoxLayoutState& aBoxLayoutState) final;
  nsSize GetXULMaxSize(nsBoxLayoutState& aBoxLayoutState) final;
  nscoord GetXULBoxAscent(nsBoxLayoutState& aBoxLayoutState) final;

  NS_IMETHOD DoXULLayout(nsBoxLayoutState& aBoxLayoutState) final;
  nsresult GetXULPadding(nsMargin& aPadding) final;

  bool GetBorderRadii(const nsSize& aFrameSize, const nsSize& aBorderArea,
                      Sides aSkipSides, nscoord aRadii[8]) const final {
    return mHelper.GetBorderRadii(aFrameSize, aBorderArea, aSkipSides, aRadii);
  }

  nsresult XULLayout(nsBoxLayoutState& aState);
  void LayoutScrollArea(nsBoxLayoutState& aState,
                        const nsPoint& aScrollPosition);

  static bool AddRemoveScrollbar(bool& aHasScrollbar, nscoord& aXY,
                                 nscoord& aSize, nscoord aSbSize,
                                 bool aOnRightOrBottom, bool aAdd);

  bool AddRemoveScrollbar(nsBoxLayoutState& aState, bool aOnRightOrBottom,
                          bool aHorizontal, bool aAdd);

  bool AddHorizontalScrollbar(nsBoxLayoutState& aState, bool aOnBottom);
  bool AddVerticalScrollbar(nsBoxLayoutState& aState, bool aOnRight);
  void RemoveHorizontalScrollbar(nsBoxLayoutState& aState, bool aOnBottom);
  void RemoveVerticalScrollbar(nsBoxLayoutState& aState, bool aOnRight);

  static void AdjustReflowInputForPrintPreview(nsBoxLayoutState& aState,
                                               bool& aSetBack);
  static void AdjustReflowInputBack(nsBoxLayoutState& aState, bool aSetBack);

  // nsIScrollableFrame
  nsIFrame* GetScrolledFrame() const final {
    return mHelper.GetScrolledFrame();
  }
  mozilla::ScrollStyles GetScrollStyles() const final {
    return mHelper.GetScrollStylesFromFrame();
  }
  bool IsForTextControlWithNoScrollbars() const final {
    return mHelper.IsForTextControlWithNoScrollbars();
  }
  mozilla::layers::OverscrollBehaviorInfo GetOverscrollBehaviorInfo()
      const final {
    return mHelper.GetOverscrollBehaviorInfo();
  }
  mozilla::layers::ScrollDirections
  GetAvailableScrollingDirectionsForUserInputEvents() const final {
    return mHelper.GetAvailableScrollingDirectionsForUserInputEvents();
  }
  mozilla::layers::ScrollDirections GetScrollbarVisibility() const final {
    return mHelper.GetScrollbarVisibility();
  }
  nsMargin GetActualScrollbarSizes(
      nsIScrollableFrame::ScrollbarSizesOptions aOptions =
          nsIScrollableFrame::ScrollbarSizesOptions::NONE) const final {
    return mHelper.GetActualScrollbarSizes(aOptions);
  }
  nsMargin GetDesiredScrollbarSizes(nsBoxLayoutState* aState) final {
    return mHelper.GetDesiredScrollbarSizes(aState);
  }
  nsMargin GetDesiredScrollbarSizes(nsPresContext* aPresContext,
                                    gfxContext* aRC) final {
    nsBoxLayoutState bls(aPresContext, aRC, 0);
    return GetDesiredScrollbarSizes(&bls);
  }
  nscoord GetNondisappearingScrollbarWidth(nsPresContext* aPresContext,
                                           gfxContext* aRC,
                                           mozilla::WritingMode aWM) final {
    nsBoxLayoutState bls(aPresContext, aRC, 0);
    return mHelper.GetNondisappearingScrollbarWidth(&bls, aWM);
  }
  nsSize GetLayoutSize() const final { return mHelper.GetLayoutSize(); }
  nsRect GetScrolledRect() const final { return mHelper.GetScrolledRect(); }
  nsRect GetScrollPortRect() const final { return mHelper.GetScrollPortRect(); }
  nsPoint GetScrollPosition() const final {
    return mHelper.GetScrollPosition();
  }
  nsPoint GetLogicalScrollPosition() const final {
    return mHelper.GetLogicalScrollPosition();
  }
  nsRect GetScrollRange() const final { return mHelper.GetLayoutScrollRange(); }
  nsSize GetVisualViewportSize() const final {
    return mHelper.GetVisualViewportSize();
  }
  nsPoint GetVisualViewportOffset() const final {
    return mHelper.GetVisualViewportOffset();
  }
  bool SetVisualViewportOffset(const nsPoint& aOffset, bool aRepaint) final {
    return mHelper.SetVisualViewportOffset(aOffset, aRepaint);
  }
  nsRect GetVisualScrollRange() const final {
    return mHelper.GetVisualScrollRange();
  }
  nsRect GetScrollRangeForUserInputEvents() const final {
    return mHelper.GetScrollRangeForUserInputEvents();
  }
  nsSize GetLineScrollAmount() const final {
    return mHelper.GetLineScrollAmount();
  }
  nsSize GetPageScrollAmount() const final {
    return mHelper.GetPageScrollAmount();
  }
  nsMargin GetScrollPadding() const final { return mHelper.GetScrollPadding(); }
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void ScrollTo(
      nsPoint aScrollPosition, ScrollMode aMode, const nsRect* aRange = nullptr,
      ScrollSnapMode aSnap = nsIScrollbarMediator::DISABLE_SNAP) final {
    mHelper.ScrollTo(aScrollPosition, aMode, ScrollOrigin::Other, aRange,
                     aSnap);
  }
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void ScrollToCSSPixels(
      const CSSIntPoint& aScrollPosition,
      ScrollMode aMode = ScrollMode::Instant,
      nsIScrollbarMediator::ScrollSnapMode aSnap =
          nsIScrollbarMediator::DISABLE_SNAP,
      ScrollOrigin aOrigin = ScrollOrigin::NotSpecified) final {
    mHelper.ScrollToCSSPixels(aScrollPosition, aMode, aSnap, aOrigin);
  }
  void ScrollToCSSPixelsApproximate(
      const mozilla::CSSPoint& aScrollPosition,
      ScrollOrigin aOrigin = ScrollOrigin::NotSpecified) final {
    mHelper.ScrollToCSSPixelsApproximate(aScrollPosition, aOrigin);
  }
  CSSIntPoint GetScrollPositionCSSPixels() final {
    return mHelper.GetScrollPositionCSSPixels();
  }
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void ScrollBy(nsIntPoint aDelta, mozilla::ScrollUnit aUnit, ScrollMode aMode,
                nsIntPoint* aOverflow,
                ScrollOrigin aOrigin = ScrollOrigin::NotSpecified,
                nsIScrollableFrame::ScrollMomentum aMomentum =
                    nsIScrollableFrame::NOT_MOMENTUM,
                nsIScrollbarMediator::ScrollSnapMode aSnap =
                    nsIScrollbarMediator::DISABLE_SNAP) final {
    mHelper.ScrollBy(aDelta, aUnit, aMode, aOverflow, aOrigin, aMomentum,
                     aSnap);
  }
  void ScrollByCSSPixels(const CSSIntPoint& aDelta,
                         ScrollMode aMode = ScrollMode::Instant,
                         ScrollOrigin aOrigin = ScrollOrigin::NotSpecified,
                         nsIScrollbarMediator::ScrollSnapMode aSnap =
                             nsIScrollbarMediator::DEFAULT) final {
    mHelper.ScrollByCSSPixels(aDelta, aMode, aOrigin, aSnap);
  }
  void ScrollSnap() final { mHelper.ScrollSnap(); }
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void ScrollToRestoredPosition() final { mHelper.ScrollToRestoredPosition(); }
  void AddScrollPositionListener(nsIScrollPositionListener* aListener) final {
    mHelper.AddScrollPositionListener(aListener);
  }
  void RemoveScrollPositionListener(
      nsIScrollPositionListener* aListener) final {
    mHelper.RemoveScrollPositionListener(aListener);
  }
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void CurPosAttributeChanged(nsIContent* aChild) final {
    mHelper.CurPosAttributeChanged(aChild);
  }
  NS_IMETHOD PostScrolledAreaEventForCurrentArea() final {
    mHelper.PostScrolledAreaEvent();
    return NS_OK;
  }
  bool IsScrollingActive() final { return mHelper.IsScrollingActive(); }
  bool IsScrollingActiveNotMinimalDisplayPort() final {
    return mHelper.IsScrollingActiveNotMinimalDisplayPort();
  }
  bool IsMaybeAsynchronouslyScrolled() final {
    return mHelper.IsMaybeAsynchronouslyScrolled();
  }
  void ResetScrollPositionForLayerPixelAlignment() final {
    mHelper.ResetScrollPositionForLayerPixelAlignment();
  }
  bool DidHistoryRestore() const final { return mHelper.mDidHistoryRestore; }
  void ClearDidHistoryRestore() final { mHelper.mDidHistoryRestore = false; }
  void MarkEverScrolled() final { mHelper.MarkEverScrolled(); }
  bool IsRectNearlyVisible(const nsRect& aRect) final {
    return mHelper.IsRectNearlyVisible(aRect);
  }
  nsRect ExpandRectToNearlyVisible(const nsRect& aRect) const final {
    return mHelper.ExpandRectToNearlyVisible(aRect);
  }
  ScrollOrigin LastScrollOrigin() final { return mHelper.LastScrollOrigin(); }
  bool IsScrollAnimating(IncludeApzAnimation aIncludeApz) final {
    return mHelper.IsScrollAnimating(aIncludeApz);
  }
  mozilla::ScrollGeneration CurrentScrollGeneration() const final {
    return mHelper.CurrentScrollGeneration();
  }
  nsPoint LastScrollDestination() final {
    return mHelper.LastScrollDestination();
  }
  nsTArray<mozilla::ScrollPositionUpdate> GetScrollUpdates() const final {
    return mHelper.GetScrollUpdates();
  }
  bool HasScrollUpdates() const final { return mHelper.HasScrollUpdates(); }
  void ResetScrollInfoIfNeeded(const mozilla::ScrollGeneration& aGeneration,
                               bool aApzAnimationInProgress) final {
    mHelper.ResetScrollInfoIfNeeded(aGeneration, aApzAnimationInProgress);
  }
  bool WantAsyncScroll() const final { return mHelper.WantAsyncScroll(); }
  mozilla::Maybe<mozilla::layers::ScrollMetadata> ComputeScrollMetadata(
      LayerManager* aLayerManager, const nsIFrame* aContainerReferenceFrame,
      const mozilla::DisplayItemClip* aClip) const final {
    return mHelper.ComputeScrollMetadata(aLayerManager,
                                         aContainerReferenceFrame, aClip);
  }
  void MarkScrollbarsDirtyForReflow() const final {
    mHelper.MarkScrollbarsDirtyForReflow();
  }
  void InvalidateVerticalScrollbar() const final {
    mHelper.InvalidateVerticalScrollbar();
  }
  void UpdateScrollbarPosition() final { mHelper.UpdateScrollbarPosition(); }

  // nsIStatefulFrame
  mozilla::UniquePtr<mozilla::PresState> SaveState() final {
    return mHelper.SaveState();
  }
  NS_IMETHOD RestoreState(mozilla::PresState* aState) final {
    NS_ENSURE_ARG_POINTER(aState);
    mHelper.RestoreState(aState);
    return NS_OK;
  }

  bool IsFrameOfType(uint32_t aFlags) const final {
    // Override bogus IsFrameOfType in nsBoxFrame.
    if (aFlags & (nsIFrame::eReplacedContainsBlock | nsIFrame::eReplaced))
      return false;
    return nsBoxFrame::IsFrameOfType(aFlags);
  }

  void ScrollByPage(nsScrollbarFrame* aScrollbar, int32_t aDirection,
                    nsIScrollbarMediator::ScrollSnapMode aSnap =
                        nsIScrollbarMediator::DISABLE_SNAP) final {
    mHelper.ScrollByPage(aScrollbar, aDirection, aSnap);
  }
  void ScrollByWhole(nsScrollbarFrame* aScrollbar, int32_t aDirection,
                     nsIScrollbarMediator::ScrollSnapMode aSnap =
                         nsIScrollbarMediator::DISABLE_SNAP) final {
    mHelper.ScrollByWhole(aScrollbar, aDirection, aSnap);
  }
  void ScrollByLine(nsScrollbarFrame* aScrollbar, int32_t aDirection,
                    nsIScrollbarMediator::ScrollSnapMode aSnap =
                        nsIScrollbarMediator::DISABLE_SNAP) final {
    mHelper.ScrollByLine(aScrollbar, aDirection, aSnap);
  }
  void ScrollByUnit(nsScrollbarFrame* aScrollbar, ScrollMode aMode,
                    int32_t aDirection, mozilla::ScrollUnit aUnit,
                    ScrollSnapMode aSnap = DISABLE_SNAP) final {
    mHelper.ScrollByUnit(aScrollbar, aMode, aDirection, aUnit, aSnap);
  }
  void RepeatButtonScroll(nsScrollbarFrame* aScrollbar) final {
    mHelper.RepeatButtonScroll(aScrollbar);
  }
  void ThumbMoved(nsScrollbarFrame* aScrollbar, nscoord aOldPos,
                  nscoord aNewPos) final {
    mHelper.ThumbMoved(aScrollbar, aOldPos, aNewPos);
  }
  void ScrollbarReleased(nsScrollbarFrame* aScrollbar) final {
    mHelper.ScrollbarReleased(aScrollbar);
  }
  void VisibilityChanged(bool aVisible) final {}
  nsIFrame* GetScrollbarBox(bool aVertical) final {
    return mHelper.GetScrollbarBox(aVertical);
  }

  void ScrollbarActivityStarted() const final;
  void ScrollbarActivityStopped() const final;

  bool IsScrollbarOnRight() const final { return mHelper.IsScrollbarOnRight(); }

  bool ShouldSuppressScrollbarRepaints() const final {
    return mHelper.ShouldSuppressScrollbarRepaints();
  }

  void SetTransformingByAPZ(bool aTransforming) final {
    mHelper.SetTransformingByAPZ(aTransforming);
  }
  bool IsTransformingByAPZ() const final {
    return mHelper.IsTransformingByAPZ();
  }
  void SetScrollableByAPZ(bool aScrollable) final {
    mHelper.SetScrollableByAPZ(aScrollable);
  }
  void SetZoomableByAPZ(bool aZoomable) final {
    mHelper.SetZoomableByAPZ(aZoomable);
  }
  void SetHasOutOfFlowContentInsideFilter() final {
    mHelper.SetHasOutOfFlowContentInsideFilter();
  }
  bool DecideScrollableLayer(nsDisplayListBuilder* aBuilder,
                             nsRect* aVisibleRect, nsRect* aDirtyRect,
                             bool aSetBase) final {
    return mHelper.DecideScrollableLayer(aBuilder, aVisibleRect, aDirtyRect,
                                         aSetBase);
  }
  void NotifyApzTransaction() final { mHelper.NotifyApzTransaction(); }
  void NotifyApproximateFrameVisibilityUpdate(bool aIgnoreDisplayPort) final {
    mHelper.NotifyApproximateFrameVisibilityUpdate(aIgnoreDisplayPort);
  }
  bool GetDisplayPortAtLastApproximateFrameVisibilityUpdate(
      nsRect* aDisplayPort) final {
    return mHelper.GetDisplayPortAtLastApproximateFrameVisibilityUpdate(
        aDisplayPort);
  }
  void TriggerDisplayPortExpiration() final {
    mHelper.TriggerDisplayPortExpiration();
  }

  ScrollSnapInfo GetScrollSnapInfo() const final {
    return mHelper.GetScrollSnapInfo(Nothing());
  }

  bool DragScroll(mozilla::WidgetEvent* aEvent) final {
    return mHelper.DragScroll(aEvent);
  }

  void AsyncScrollbarDragInitiated(
      uint64_t aDragBlockId,
      mozilla::layers::ScrollDirection aDirection) final {
    return mHelper.AsyncScrollbarDragInitiated(aDragBlockId, aDirection);
  }

  void AsyncScrollbarDragRejected() final {
    return mHelper.AsyncScrollbarDragRejected();
  }

  bool IsRootScrollFrameOfDocument() const final {
    return mHelper.IsRootScrollFrameOfDocument();
  }

  const ScrollAnchorContainer* Anchor() const final { return &mHelper.mAnchor; }

  ScrollAnchorContainer* Anchor() final { return &mHelper.mAnchor; }

  // Return the scrolled frame.
  void AppendDirectlyOwnedAnonBoxes(nsTArray<OwnedAnonBox>& aResult) final {
    aResult.AppendElement(OwnedAnonBox(mHelper.GetScrolledFrame()));
  }

  bool SmoothScrollVisual(
      const nsPoint& aVisualViewportOffset,
      mozilla::layers::FrameMetrics::ScrollOffsetUpdateType aUpdateType) final {
    return mHelper.SmoothScrollVisual(aVisualViewportOffset, aUpdateType);
  }

  bool IsSmoothScroll(mozilla::dom::ScrollBehavior aBehavior) const final {
    return mHelper.IsSmoothScroll(aBehavior);
  }

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const final;
#endif

 protected:
  nsXULScrollFrame(ComputedStyle*, nsPresContext*, bool aIsRoot,
                   bool aClipAllDescendants);

  void ClampAndSetBounds(nsBoxLayoutState& aState, nsRect& aRect,
                         nsPoint aScrollPosition,
                         bool aRemoveOverflowAreas = false) {
    /*
     * For RTL frames, restore the original scrolled position of the right
     * edge, then subtract the current width to find the physical position.
     */
    if (!mHelper.IsPhysicalLTR()) {
      aRect.x = mHelper.mScrollPort.XMost() - aScrollPosition.x - aRect.width;
    }
    mHelper.mScrolledFrame->SetXULBounds(aState, aRect, aRemoveOverflowAreas);
  }

 private:
  friend class mozilla::ScrollFrameHelper;
  ScrollFrameHelper mHelper;
};

#endif /* nsGfxScrollFrame_h___ */
