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
#include "nsRefreshDriver.h"
#include "nsExpirationTracker.h"
#include "TextOverflow.h"
#include "ScrollVelocityQueue.h"
#include "mozilla/PresState.h"
#include "mozilla/layout/ScrollAnchorContainer.h"

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

  class AsyncScroll;
  class AsyncSmoothMSDScroll;

  ScrollFrameHelper(nsContainerFrame* aOuter, bool aIsRoot);
  ~ScrollFrameHelper();

  mozilla::ScrollStyles GetScrollStylesFromFrame() const;

  // If a child frame was added or removed on the scrollframe,
  // reload our child frame list.
  // We need this if a scrollbar frame is recreated.
  void ReloadChildFrames();

  nsresult CreateAnonymousContent(
      nsTArray<nsIAnonymousContentCreator::ContentInfo>& aElements);
  void AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                uint32_t aFilter);
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
  void MaybeAddTopLayerItems(nsDisplayListBuilder* aBuilder,
                             const nsDisplayListSet& aLists);

  void AppendScrollPartsTo(nsDisplayListBuilder* aBuilder,
                           const nsDisplayListSet& aLists, bool aCreateLayer,
                           bool aPositioned);

  bool GetBorderRadii(const nsSize& aFrameSize, const nsSize& aBorderArea,
                      Sides aSkipSides, nscoord aRadii[8]) const;

  // nsIReflowCallback
  virtual bool ReflowFinished() override;
  virtual void ReflowCallbackCanceled() override;

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
  void FinishReflowForScrollbar(mozilla::dom::Element* aElement, nscoord aMinXY,
                                nscoord aMaxXY, nscoord aCurPosXY,
                                nscoord aPageIncrement, nscoord aIncrement);
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void SetScrollbarEnabled(mozilla::dom::Element* aElement, nscoord aMaxPos);
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void SetCoordAttribute(mozilla::dom::Element* aElement, nsAtom* aAtom,
                         nscoord aSize);

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
  nsPoint GetApzScrollPosition() const { return mApzScrollPos; }
  nsRect GetLayoutScrollRange() const;
  // Get the scroll range assuming the viewport has size (aWidth, aHeight).
  nsRect GetScrollRange(nscoord aWidth, nscoord aHeight) const;
  nsSize GetVisualViewportSize() const;
  nsPoint GetVisualViewportOffset() const;

  /**
   * Return the 'optimal viewing region' [1] as a rect suitable for use by
   * scroll anchoring. This rect is in the same coordinate space as
   * 'GetScrollPortRect'.
   *
   * [1] https://drafts.csswg.org/css-scroll-snap-1/#optimal-viewing-region
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

 protected:
  nsRect GetVisualScrollRange() const;

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
                nsAtom* aOrigin = nullptr, const nsRect* aRange = nullptr,
                nsIScrollbarMediator::ScrollSnapMode aSnap =
                    nsIScrollbarMediator::DISABLE_SNAP);
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void ScrollToCSSPixels(const CSSIntPoint& aScrollPosition,
                         ScrollMode aMode = ScrollMode::Instant,
                         nsIScrollbarMediator::ScrollSnapMode aSnap =
                             nsIScrollbarMediator::DEFAULT,
                         nsAtom* aOrigin = nullptr);
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void ScrollToCSSPixelsApproximate(const mozilla::CSSPoint& aScrollPosition,
                                    nsAtom* aOrigin = nullptr);

  CSSIntPoint GetScrollPositionCSSPixels();
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void ScrollToImpl(nsPoint aScrollPosition, const nsRect& aRange,
                    nsAtom* aOrigin = nullptr);
  void ScrollVisual();
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void ScrollBy(nsIntPoint aDelta, nsIScrollableFrame::ScrollUnit aUnit,
                ScrollMode aMode, nsIntPoint* aOverflow,
                nsAtom* aOrigin = nullptr,
                nsIScrollableFrame::ScrollMomentum aMomentum =
                    nsIScrollableFrame::NOT_MOMENTUM,
                nsIScrollbarMediator::ScrollSnapMode aSnap =
                    nsIScrollbarMediator::DISABLE_SNAP);
  void ScrollByCSSPixels(const CSSIntPoint& aDelta,
                         ScrollMode aMode = ScrollMode::Instant,
                         nsAtom* aOrigin = nullptr,
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
  bool GetSnapPointForDestination(nsIScrollableFrame::ScrollUnit aUnit,
                                  nsPoint aStartPos, nsPoint& aDestination);

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

  uint32_t GetScrollbarVisibility() const {
    return (mHasVerticalScrollbar ? nsIScrollableFrame::VERTICAL : 0) |
           (mHasHorizontalScrollbar ? nsIScrollableFrame::HORIZONTAL : 0);
  }
  nsMargin GetActualScrollbarSizes() const;
  nsMargin GetDesiredScrollbarSizes(nsBoxLayoutState* aState);
  nscoord GetNondisappearingScrollbarWidth(nsBoxLayoutState* aState,
                                           mozilla::WritingMode aVerticalWM);
  bool IsPhysicalLTR() const {
    WritingMode wm = GetFrameForDir()->GetWritingMode();
    return wm.IsVertical() ? wm.IsVerticalLR() : wm.IsBidiLTR();
  }
  bool IsBidiLTR() const {
    nsIFrame* frame = GetFrameForDir();
    return frame->StyleVisibility()->mDirection == NS_STYLE_DIRECTION_LTR;
  }

 private:
  nsIFrame* GetFrameForDir() const;  // helper for Is{Physical,Bidi}LTR to find
                                     // the frame whose directionality we use
  // helper to find the frame that style data for this scrollable frame is
  // stored.
  //
  // NOTE: Use GetFrameForDir() if you want to know `writing-mode` or `dir`
  // properties. Use GetScrollStylesFromFrame() if you want to know `overflow`
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
  bool IsScrollingActive(nsDisplayListBuilder* aBuilder) const;
  bool IsMaybeAsynchronouslyScrolled() const {
    // If this is true, then we'll build an ASR, and that's what we want
    // to know I think.
    return mWillBuildScrollableLayer;
  }
  bool IsMaybeScrollingActive() const;
  bool IsProcessingAsyncScroll() const {
    return mAsyncScroll != nullptr || mAsyncSmoothMSDScroll != nullptr;
  }
  void ResetScrollPositionForLayerPixelAlignment() {
    mScrollPosForLayerPixelAlignment = GetScrollPosition();
  }

  bool ComputeCustomOverflow(nsOverflowAreas& aOverflowAreas);

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
  bool HasResizer() { return mResizerBox && !mCollapsedResizer; }
  void LayoutScrollbars(nsBoxLayoutState& aState, const nsRect& aContentArea,
                        const nsRect& aOldScrollArea);

  void MarkScrollbarsDirtyForReflow() const;

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

  bool UsesContainerScrolling() const;

  // In the case where |aDestination| is given, elements which are entirely out
  // of view when the scroll position is moved to |aDestination| are not going
  // to be used for snap positions.
  ScrollSnapInfo GetScrollSnapInfo(
      const mozilla::Maybe<nsPoint>& aDestination) const;

  bool DecideScrollableLayer(nsDisplayListBuilder* aBuilder,
                             nsRect* aVisibleRect, nsRect* aDirtyRect,
                             bool aSetBase,
                             bool* aDirtyRectHasBeenOverriden = nullptr);
  void NotifyApzTransaction() {
    mAllowScrollOriginDowngrade = true;
    mApzScrollPos = GetScrollPosition();
  }
  void NotifyApproximateFrameVisibilityUpdate(bool aIgnoreDisplayPort);
  bool GetDisplayPortAtLastApproximateFrameVisibilityUpdate(
      nsRect* aDisplayPort);

  bool AllowDisplayPortExpiration();
  void TriggerDisplayPortExpiration();
  void ResetDisplayPortExpiryTimer();

  void ScheduleSyntheticMouseMove();
  static void ScrollActivityCallback(nsITimer* aTimer, void* anInstance);

  void HandleScrollbarStyleSwitching();

  nsAtom* LastScrollOrigin() const { return mLastScrollOrigin; }
  nsAtom* LastSmoothScrollOrigin() const { return mLastSmoothScrollOrigin; }
  uint32_t CurrentScrollGeneration() const { return mScrollGeneration; }
  nsPoint LastScrollDestination() const { return mDestination; }
  void ResetScrollInfoIfGeneration(uint32_t aGeneration) {
    if (aGeneration == mScrollGeneration) {
      mLastScrollOrigin = nullptr;
      mLastSmoothScrollOrigin = nullptr;
    }
  }
  bool WantAsyncScroll() const;
  Maybe<mozilla::layers::ScrollMetadata> ComputeScrollMetadata(
      LayerManager* aLayerManager, const nsIFrame* aContainerReferenceFrame,
      const Maybe<ContainerLayerParameters>& aParameters,
      const mozilla::DisplayItemClip* aClip) const;
  void ClipLayerToDisplayPort(
      Layer* aLayer, const mozilla::DisplayItemClip* aClip,
      const ContainerLayerParameters& aParameters) const;

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
  void RepeatButtonScroll(nsScrollbarFrame* aScrollbar);
  void ThumbMoved(nsScrollbarFrame* aScrollbar, nscoord aOldPos,
                  nscoord aNewPos);
  void ScrollbarReleased(nsScrollbarFrame* aScrollbar);
  void ScrollByUnit(nsScrollbarFrame* aScrollbar, ScrollMode aMode,
                    int32_t aDirection, nsIScrollableFrame::ScrollUnit aUnit,
                    nsIScrollbarMediator::ScrollSnapMode aSnap =
                        nsIScrollbarMediator::DISABLE_SNAP);
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
  nsSize TrueOuterSize() const;

  already_AddRefed<Element> MakeScrollbar(dom::NodeInfo* aNodeInfo,
                                          bool aVertical,
                                          AnonymousContentKey& aKey);

  // owning references to the nsIAnonymousContentCreator-built content
  nsCOMPtr<mozilla::dom::Element> mHScrollbarContent;
  nsCOMPtr<mozilla::dom::Element> mVScrollbarContent;
  nsCOMPtr<mozilla::dom::Element> mScrollCornerContent;
  nsCOMPtr<mozilla::dom::Element> mResizerContent;

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
  nsAtom* mLastScrollOrigin;
  nsAtom* mLastSmoothScrollOrigin;
  Maybe<nsPoint> mApzSmoothScrollDestination;
  uint32_t mScrollGeneration;
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
  // If true, the resizer is collapsed and not displayed
  bool mCollapsedResizer : 1;

  // If true, the scroll frame should always be active because we always build
  // a scrollable layer. Used for asynchronous scrolling.
  bool mWillBuildScrollableLayer : 1;

  // If true, the scroll frame is an ancestor of other scrolling frames, so
  // we shouldn't expire the displayport on this scrollframe unless those
  // descendant scrollframes also have their displayports removed.
  bool mIsScrollParent : 1;

  // Whether we are the root scroll frame that is used for containerful
  // scrolling with a display port. If true, the scrollable frame
  // shouldn't attach frame metrics to its layers because the container
  // will already have the necessary frame metrics.
  bool mIsScrollableLayerInRootContainer : 1;

  // If true, add clipping in ScrollFrameHelper::ClipLayerToDisplayPort.
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

  mozilla::layout::ScrollVelocityQueue mVelocityQueue;

 protected:
  class AutoScrollbarRepaintSuppression;
  friend class AutoScrollbarRepaintSuppression;
  class AutoScrollbarRepaintSuppression {
   public:
    AutoScrollbarRepaintSuppression(ScrollFrameHelper* aHelper, bool aSuppress)
        : mHelper(aHelper),
          mOldSuppressValue(aHelper->mSuppressScrollbarRepaints) {
      mHelper->mSuppressScrollbarRepaints = aSuppress;
    }

    ~AutoScrollbarRepaintSuppression() {
      mHelper->mSuppressScrollbarRepaints = mOldSuppressValue;
    }

   private:
    ScrollFrameHelper* mHelper;
    bool mOldSuppressValue;
  };

  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void ScrollToWithOrigin(nsPoint aScrollPosition, ScrollMode aMode,
                          nsAtom* aOrigin,  // nullptr indicates "other" origin
                          const nsRect* aRange,
                          nsIScrollbarMediator::ScrollSnapMode aSnap =
                              nsIScrollbarMediator::DISABLE_SNAP);

  void CompleteAsyncScroll(const nsRect& aRange, nsAtom* aOrigin = nullptr);

  bool HasPluginFrames();
  bool HasPerspective() const { return mOuter->ChildrenHavePerspective(); }
  bool HasBgAttachmentLocal() const;
  uint8_t GetScrolledFrameDir() const;

  bool IsForTextControlWithNoScrollbars() const;

  // Ask APZ to smooth scroll to |aDestination|.
  // This method does not clamp the destination; callers should clamp it to
  // either the layout or the visual scroll range (APZ will happily smooth
  // scroll to either).
  void ApzSmoothScrollTo(const nsPoint& aDestination, nsAtom* aOrigin);

  // Removes any RefreshDriver observers we might have registered.
  void RemoveObservers();

  static void EnsureFrameVisPrefsCached();
  static bool sFrameVisPrefsCached;
  // The number of scrollports wide/high to expand when tracking frame
  // visibility.
  static uint32_t sHorzExpandScrollPort;
  static uint32_t sVertExpandScrollPort;
};

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

  virtual void BuildDisplayList(nsDisplayListBuilder* aBuilder,
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

  virtual bool GetBorderRadii(const nsSize& aFrameSize,
                              const nsSize& aBorderArea, Sides aSkipSides,
                              nscoord aRadii[8]) const override {
    return mHelper.GetBorderRadii(aFrameSize, aBorderArea, aSkipSides, aRadii);
  }

  virtual nscoord GetMinISize(gfxContext* aRenderingContext) override;
  virtual nscoord GetPrefISize(gfxContext* aRenderingContext) override;
  virtual nsresult GetXULPadding(nsMargin& aPadding) override;
  virtual bool IsXULCollapsed() override;

  virtual void Reflow(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus& aStatus) override;
  virtual void DidReflow(nsPresContext* aPresContext,
                         const ReflowInput* aReflowInput) override;

  virtual bool ComputeCustomOverflow(nsOverflowAreas& aOverflowAreas) override {
    return mHelper.ComputeCustomOverflow(aOverflowAreas);
  }

  nscoord GetLogicalBaseline(mozilla::WritingMode aWritingMode) const override;

  bool GetVerticalAlignBaseline(mozilla::WritingMode aWM,
                                nscoord* aBaseline) const override {
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
  virtual void SetInitialChildList(ChildListID aListID,
                                   nsFrameList& aChildList) override;
  virtual void AppendFrames(ChildListID aListID,
                            nsFrameList& aFrameList) override;
  virtual void InsertFrames(ChildListID aListID, nsIFrame* aPrevFrame,
                            const nsLineList::iterator* aPrevFrameLine,
                            nsFrameList& aFrameList) override;
  virtual void RemoveFrame(ChildListID aListID, nsIFrame* aOldFrame) override;

  virtual void DestroyFrom(nsIFrame* aDestructRoot,
                           PostDestroyData& aPostDestroyData) override;

  virtual nsIScrollableFrame* GetScrollTargetFrame() override { return this; }

  virtual nsContainerFrame* GetContentInsertionFrame() override {
    return mHelper.GetScrolledFrame()->GetContentInsertionFrame();
  }

  virtual bool DoesClipChildren() override { return true; }

  nsPoint GetPositionOfChildIgnoringScrolling(const nsIFrame* aChild) override {
    nsPoint pt = aChild->GetPosition();
    if (aChild == mHelper.GetScrolledFrame()) pt += GetScrollPosition();
    return pt;
  }

  // nsIAnonymousContentCreator
  virtual nsresult CreateAnonymousContent(
      nsTArray<ContentInfo>& aElements) override;
  virtual void AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                        uint32_t aFilter) override;

  // nsIScrollableFrame
  virtual nsIFrame* GetScrolledFrame() const override {
    return mHelper.GetScrolledFrame();
  }
  virtual mozilla::ScrollStyles GetScrollStyles() const override {
    return mHelper.GetScrollStylesFromFrame();
  }
  virtual uint32_t GetScrollbarVisibility() const override {
    return mHelper.GetScrollbarVisibility();
  }
  virtual nsMargin GetActualScrollbarSizes() const override {
    return mHelper.GetActualScrollbarSizes();
  }
  virtual nsMargin GetDesiredScrollbarSizes(nsBoxLayoutState* aState) override {
    return mHelper.GetDesiredScrollbarSizes(aState);
  }
  virtual nsMargin GetDesiredScrollbarSizes(nsPresContext* aPresContext,
                                            gfxContext* aRC) override {
    nsBoxLayoutState bls(aPresContext, aRC, 0);
    return GetDesiredScrollbarSizes(&bls);
  }
  virtual nscoord GetNondisappearingScrollbarWidth(
      nsPresContext* aPresContext, gfxContext* aRC,
      mozilla::WritingMode aWM) override {
    nsBoxLayoutState bls(aPresContext, aRC, 0);
    return mHelper.GetNondisappearingScrollbarWidth(&bls, aWM);
  }
  virtual nsSize GetLayoutSize() const override {
    return mHelper.GetLayoutSize();
  }
  virtual nsRect GetScrolledRect() const override {
    return mHelper.GetScrolledRect();
  }
  virtual nsRect GetScrollPortRect() const override {
    return mHelper.GetScrollPortRect();
  }
  virtual nsPoint GetScrollPosition() const override {
    return mHelper.GetScrollPosition();
  }
  virtual nsPoint GetLogicalScrollPosition() const override {
    return mHelper.GetLogicalScrollPosition();
  }
  virtual nsPoint GetApzScrollPosition() const override {
    return mHelper.GetApzScrollPosition();
  }
  virtual nsRect GetScrollRange() const override {
    return mHelper.GetLayoutScrollRange();
  }
  virtual nsSize GetVisualViewportSize() const override {
    return mHelper.GetVisualViewportSize();
  }
  virtual nsPoint GetVisualViewportOffset() const override {
    return mHelper.GetVisualViewportOffset();
  }
  virtual nsSize GetLineScrollAmount() const override {
    return mHelper.GetLineScrollAmount();
  }
  virtual nsSize GetPageScrollAmount() const override {
    return mHelper.GetPageScrollAmount();
  }
  virtual nsMargin GetScrollPadding() const override {
    return mHelper.GetScrollPadding();
  }
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  virtual void ScrollTo(nsPoint aScrollPosition, ScrollMode aMode,
                        const nsRect* aRange = nullptr,
                        nsIScrollbarMediator::ScrollSnapMode aSnap =
                            nsIScrollbarMediator::DISABLE_SNAP) override {
    mHelper.ScrollTo(aScrollPosition, aMode, nsGkAtoms::other, aRange, aSnap);
  }
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  virtual void ScrollToCSSPixels(const CSSIntPoint& aScrollPosition,
                                 ScrollMode aMode = ScrollMode::Instant,
                                 nsIScrollbarMediator::ScrollSnapMode aSnap =
                                     nsIScrollbarMediator::DEFAULT,
                                 nsAtom* aOrigin = nullptr) override {
    mHelper.ScrollToCSSPixels(aScrollPosition, aMode, aSnap, aOrigin);
  }
  virtual void ScrollToCSSPixelsApproximate(
      const mozilla::CSSPoint& aScrollPosition,
      nsAtom* aOrigin = nullptr) override {
    mHelper.ScrollToCSSPixelsApproximate(aScrollPosition, aOrigin);
  }
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  virtual CSSIntPoint GetScrollPositionCSSPixels() override {
    return mHelper.GetScrollPositionCSSPixels();
  }
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  virtual void ScrollBy(nsIntPoint aDelta, ScrollUnit aUnit, ScrollMode aMode,
                        nsIntPoint* aOverflow, nsAtom* aOrigin = nullptr,
                        nsIScrollableFrame::ScrollMomentum aMomentum =
                            nsIScrollableFrame::NOT_MOMENTUM,
                        nsIScrollbarMediator::ScrollSnapMode aSnap =
                            nsIScrollbarMediator::DISABLE_SNAP) override {
    mHelper.ScrollBy(aDelta, aUnit, aMode, aOverflow, aOrigin, aMomentum,
                     aSnap);
  }
  virtual void ScrollByCSSPixels(const CSSIntPoint& aDelta,
                                 ScrollMode aMode = ScrollMode::Instant,
                                 nsAtom* aOrigin = nullptr,
                                 nsIScrollbarMediator::ScrollSnapMode aSnap =
                                     nsIScrollbarMediator::DEFAULT) override {
    mHelper.ScrollByCSSPixels(aDelta, aMode, aOrigin, aSnap);
  }
  virtual void ScrollSnap() override { mHelper.ScrollSnap(); }
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  virtual void ScrollToRestoredPosition() override {
    mHelper.ScrollToRestoredPosition();
  }
  virtual void AddScrollPositionListener(
      nsIScrollPositionListener* aListener) override {
    mHelper.AddScrollPositionListener(aListener);
  }
  virtual void RemoveScrollPositionListener(
      nsIScrollPositionListener* aListener) override {
    mHelper.RemoveScrollPositionListener(aListener);
  }
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  virtual void CurPosAttributeChanged(nsIContent* aChild) override {
    mHelper.CurPosAttributeChanged(aChild);
  }
  NS_IMETHOD PostScrolledAreaEventForCurrentArea() override {
    mHelper.PostScrolledAreaEvent();
    return NS_OK;
  }
  virtual bool IsScrollingActive(nsDisplayListBuilder* aBuilder) override {
    return mHelper.IsScrollingActive(aBuilder);
  }
  virtual bool IsMaybeScrollingActive() const override {
    return mHelper.IsMaybeScrollingActive();
  }
  virtual bool IsMaybeAsynchronouslyScrolled() override {
    return mHelper.IsMaybeAsynchronouslyScrolled();
  }
  virtual bool IsProcessingAsyncScroll() override {
    return mHelper.IsProcessingAsyncScroll();
  }
  virtual void ResetScrollPositionForLayerPixelAlignment() override {
    mHelper.ResetScrollPositionForLayerPixelAlignment();
  }
  virtual bool DidHistoryRestore() const override {
    return mHelper.mDidHistoryRestore;
  }
  virtual void ClearDidHistoryRestore() override {
    mHelper.mDidHistoryRestore = false;
  }
  virtual void MarkEverScrolled() override { mHelper.MarkEverScrolled(); }
  virtual bool IsRectNearlyVisible(const nsRect& aRect) override {
    return mHelper.IsRectNearlyVisible(aRect);
  }
  virtual nsRect ExpandRectToNearlyVisible(const nsRect& aRect) const override {
    return mHelper.ExpandRectToNearlyVisible(aRect);
  }
  virtual nsAtom* LastScrollOrigin() override {
    return mHelper.LastScrollOrigin();
  }
  virtual nsAtom* LastSmoothScrollOrigin() override {
    return mHelper.LastSmoothScrollOrigin();
  }
  virtual uint32_t CurrentScrollGeneration() override {
    return mHelper.CurrentScrollGeneration();
  }
  virtual nsPoint LastScrollDestination() override {
    return mHelper.LastScrollDestination();
  }
  virtual void ResetScrollInfoIfGeneration(uint32_t aGeneration) override {
    mHelper.ResetScrollInfoIfGeneration(aGeneration);
  }
  virtual bool WantAsyncScroll() const override {
    return mHelper.WantAsyncScroll();
  }
  virtual mozilla::Maybe<mozilla::layers::ScrollMetadata> ComputeScrollMetadata(
      LayerManager* aLayerManager, const nsIFrame* aContainerReferenceFrame,
      const Maybe<ContainerLayerParameters>& aParameters,
      const mozilla::DisplayItemClip* aClip) const override {
    return mHelper.ComputeScrollMetadata(
        aLayerManager, aContainerReferenceFrame, aParameters, aClip);
  }
  virtual void ClipLayerToDisplayPort(
      Layer* aLayer, const mozilla::DisplayItemClip* aClip,
      const ContainerLayerParameters& aParameters) const override {
    mHelper.ClipLayerToDisplayPort(aLayer, aClip, aParameters);
  }
  virtual void MarkScrollbarsDirtyForReflow() const override {
    mHelper.MarkScrollbarsDirtyForReflow();
  }
  virtual bool UsesContainerScrolling() const override {
    return mHelper.UsesContainerScrolling();
  }
  virtual bool DecideScrollableLayer(nsDisplayListBuilder* aBuilder,
                                     nsRect* aVisibleRect, nsRect* aDirtyRect,
                                     bool aSetBase) override {
    return mHelper.DecideScrollableLayer(aBuilder, aVisibleRect, aDirtyRect,
                                         aSetBase);
  }
  virtual void NotifyApzTransaction() override {
    mHelper.NotifyApzTransaction();
  }
  virtual void NotifyApproximateFrameVisibilityUpdate(
      bool aIgnoreDisplayPort) override {
    mHelper.NotifyApproximateFrameVisibilityUpdate(aIgnoreDisplayPort);
  }
  virtual bool GetDisplayPortAtLastApproximateFrameVisibilityUpdate(
      nsRect* aDisplayPort) override {
    return mHelper.GetDisplayPortAtLastApproximateFrameVisibilityUpdate(
        aDisplayPort);
  }
  void TriggerDisplayPortExpiration() override {
    mHelper.TriggerDisplayPortExpiration();
  }

  // nsIStatefulFrame
  mozilla::UniquePtr<mozilla::PresState> SaveState() override {
    return mHelper.SaveState();
  }
  NS_IMETHOD RestoreState(mozilla::PresState* aState) override {
    NS_ENSURE_ARG_POINTER(aState);
    mHelper.RestoreState(aState);
    return NS_OK;
  }

  // nsIScrollbarMediator
  virtual void ScrollByPage(nsScrollbarFrame* aScrollbar, int32_t aDirection,
                            nsIScrollbarMediator::ScrollSnapMode aSnap =
                                nsIScrollbarMediator::DISABLE_SNAP) override {
    mHelper.ScrollByPage(aScrollbar, aDirection, aSnap);
  }
  virtual void ScrollByWhole(nsScrollbarFrame* aScrollbar, int32_t aDirection,
                             nsIScrollbarMediator::ScrollSnapMode aSnap =
                                 nsIScrollbarMediator::DISABLE_SNAP) override {
    mHelper.ScrollByWhole(aScrollbar, aDirection, aSnap);
  }
  virtual void ScrollByLine(nsScrollbarFrame* aScrollbar, int32_t aDirection,
                            nsIScrollbarMediator::ScrollSnapMode aSnap =
                                nsIScrollbarMediator::DISABLE_SNAP) override {
    mHelper.ScrollByLine(aScrollbar, aDirection, aSnap);
  }
  virtual void RepeatButtonScroll(nsScrollbarFrame* aScrollbar) override {
    mHelper.RepeatButtonScroll(aScrollbar);
  }
  virtual void ThumbMoved(nsScrollbarFrame* aScrollbar, nscoord aOldPos,
                          nscoord aNewPos) override {
    mHelper.ThumbMoved(aScrollbar, aOldPos, aNewPos);
  }
  virtual void ScrollbarReleased(nsScrollbarFrame* aScrollbar) override {
    mHelper.ScrollbarReleased(aScrollbar);
  }
  virtual void VisibilityChanged(bool aVisible) override {}
  virtual nsIFrame* GetScrollbarBox(bool aVertical) override {
    return mHelper.GetScrollbarBox(aVertical);
  }
  virtual void ScrollbarActivityStarted() const override;
  virtual void ScrollbarActivityStopped() const override;

  virtual bool IsScrollbarOnRight() const override {
    return mHelper.IsScrollbarOnRight();
  }

  virtual bool ShouldSuppressScrollbarRepaints() const override {
    return mHelper.ShouldSuppressScrollbarRepaints();
  }

  virtual void SetTransformingByAPZ(bool aTransforming) override {
    mHelper.SetTransformingByAPZ(aTransforming);
  }
  bool IsTransformingByAPZ() const override {
    return mHelper.IsTransformingByAPZ();
  }
  void SetScrollableByAPZ(bool aScrollable) override {
    mHelper.SetScrollableByAPZ(aScrollable);
  }
  void SetZoomableByAPZ(bool aZoomable) override {
    mHelper.SetZoomableByAPZ(aZoomable);
  }
  void SetHasOutOfFlowContentInsideFilter() override {
    mHelper.SetHasOutOfFlowContentInsideFilter();
  }

  ScrollSnapInfo GetScrollSnapInfo() const override {
    return mHelper.GetScrollSnapInfo(Nothing());
  }

  virtual bool DragScroll(mozilla::WidgetEvent* aEvent) override {
    return mHelper.DragScroll(aEvent);
  }

  virtual void AsyncScrollbarDragInitiated(
      uint64_t aDragBlockId,
      mozilla::layers::ScrollDirection aDirection) override {
    return mHelper.AsyncScrollbarDragInitiated(aDragBlockId, aDirection);
  }

  virtual void AsyncScrollbarDragRejected() override {
    return mHelper.AsyncScrollbarDragRejected();
  }

  virtual bool IsRootScrollFrameOfDocument() const override {
    return mHelper.IsRootScrollFrameOfDocument();
  }

  virtual const ScrollAnchorContainer* Anchor() const override {
    return &mHelper.mAnchor;
  }

  virtual ScrollAnchorContainer* Anchor() override { return &mHelper.mAnchor; }

  // Return the scrolled frame.
  void AppendDirectlyOwnedAnonBoxes(nsTArray<OwnedAnonBox>& aResult) override {
    aResult.AppendElement(OwnedAnonBox(mHelper.GetScrolledFrame()));
  }

  bool SmoothScrollVisual(const nsPoint& aVisualViewportOffset,
                          mozilla::layers::FrameMetrics::ScrollOffsetUpdateType
                              aUpdateType) override {
    return mHelper.SmoothScrollVisual(aVisualViewportOffset, aUpdateType);
  }

  bool IsSmoothScroll(mozilla::dom::ScrollBehavior aBehavior) const override {
    return mHelper.IsSmoothScroll(aBehavior);
  }

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif

#ifdef ACCESSIBILITY
  virtual mozilla::a11y::AccType AccessibleType() override;
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

  virtual void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                const nsDisplayListSet& aLists) override {
    mHelper.BuildDisplayList(aBuilder, aLists);
  }

  // XXXldb Is this actually used?
#if 0
  virtual nscoord GetMinISize(gfxContext *aRenderingContext) override;
#endif

  virtual bool ComputeCustomOverflow(nsOverflowAreas& aOverflowAreas) override {
    return mHelper.ComputeCustomOverflow(aOverflowAreas);
  }

  bool GetVerticalAlignBaseline(mozilla::WritingMode aWM,
                                nscoord* aBaseline) const override {
    *aBaseline = GetLogicalBaseline(aWM);
    return true;
  }

  // Called to set the child frames. We typically have three: the scroll area,
  // the vertical scrollbar, and the horizontal scrollbar.
  virtual void SetInitialChildList(ChildListID aListID,
                                   nsFrameList& aChildList) override;
  virtual void AppendFrames(ChildListID aListID,
                            nsFrameList& aFrameList) override;
  virtual void InsertFrames(ChildListID aListID, nsIFrame* aPrevFrame,
                            const nsLineList::iterator* aPrevFrameLine,
                            nsFrameList& aFrameList) override;
  virtual void RemoveFrame(ChildListID aListID, nsIFrame* aOldFrame) override;

  virtual void DestroyFrom(nsIFrame* aDestructRoot,
                           PostDestroyData& aPostDestroyData) override;

  virtual nsIScrollableFrame* GetScrollTargetFrame() override { return this; }

  virtual nsContainerFrame* GetContentInsertionFrame() override {
    return mHelper.GetScrolledFrame()->GetContentInsertionFrame();
  }

  virtual bool DoesClipChildren() override { return true; }

  nsPoint GetPositionOfChildIgnoringScrolling(const nsIFrame* aChild) override {
    nsPoint pt = aChild->GetPosition();
    if (aChild == mHelper.GetScrolledFrame())
      pt += mHelper.GetLogicalScrollPosition();
    return pt;
  }

  // nsIAnonymousContentCreator
  virtual nsresult CreateAnonymousContent(
      nsTArray<ContentInfo>& aElements) override;
  virtual void AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                        uint32_t aFilter) override;

  virtual nsSize GetXULMinSize(nsBoxLayoutState& aBoxLayoutState) override;
  virtual nsSize GetXULPrefSize(nsBoxLayoutState& aBoxLayoutState) override;
  virtual nsSize GetXULMaxSize(nsBoxLayoutState& aBoxLayoutState) override;
  virtual nscoord GetXULBoxAscent(nsBoxLayoutState& aBoxLayoutState) override;

  NS_IMETHOD DoXULLayout(nsBoxLayoutState& aBoxLayoutState) override;
  virtual nsresult GetXULPadding(nsMargin& aPadding) override;

  virtual bool GetBorderRadii(const nsSize& aFrameSize,
                              const nsSize& aBorderArea, Sides aSkipSides,
                              nscoord aRadii[8]) const override {
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
  virtual nsIFrame* GetScrolledFrame() const override {
    return mHelper.GetScrolledFrame();
  }
  virtual mozilla::ScrollStyles GetScrollStyles() const override {
    return mHelper.GetScrollStylesFromFrame();
  }
  virtual uint32_t GetScrollbarVisibility() const override {
    return mHelper.GetScrollbarVisibility();
  }
  virtual nsMargin GetActualScrollbarSizes() const override {
    return mHelper.GetActualScrollbarSizes();
  }
  virtual nsMargin GetDesiredScrollbarSizes(nsBoxLayoutState* aState) override {
    return mHelper.GetDesiredScrollbarSizes(aState);
  }
  virtual nsMargin GetDesiredScrollbarSizes(nsPresContext* aPresContext,
                                            gfxContext* aRC) override {
    nsBoxLayoutState bls(aPresContext, aRC, 0);
    return GetDesiredScrollbarSizes(&bls);
  }
  virtual nscoord GetNondisappearingScrollbarWidth(
      nsPresContext* aPresContext, gfxContext* aRC,
      mozilla::WritingMode aWM) override {
    nsBoxLayoutState bls(aPresContext, aRC, 0);
    return mHelper.GetNondisappearingScrollbarWidth(&bls, aWM);
  }
  virtual nsSize GetLayoutSize() const override {
    return mHelper.GetLayoutSize();
  }
  virtual nsRect GetScrolledRect() const override {
    return mHelper.GetScrolledRect();
  }
  virtual nsRect GetScrollPortRect() const override {
    return mHelper.GetScrollPortRect();
  }
  virtual nsPoint GetScrollPosition() const override {
    return mHelper.GetScrollPosition();
  }
  virtual nsPoint GetLogicalScrollPosition() const override {
    return mHelper.GetLogicalScrollPosition();
  }
  virtual nsPoint GetApzScrollPosition() const override {
    return mHelper.GetApzScrollPosition();
  }
  virtual nsRect GetScrollRange() const override {
    return mHelper.GetLayoutScrollRange();
  }
  virtual nsSize GetVisualViewportSize() const override {
    return mHelper.GetVisualViewportSize();
  }
  virtual nsPoint GetVisualViewportOffset() const override {
    return mHelper.GetVisualViewportOffset();
  }
  virtual nsSize GetLineScrollAmount() const override {
    return mHelper.GetLineScrollAmount();
  }
  virtual nsSize GetPageScrollAmount() const override {
    return mHelper.GetPageScrollAmount();
  }
  virtual nsMargin GetScrollPadding() const override {
    return mHelper.GetScrollPadding();
  }
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  virtual void ScrollTo(
      nsPoint aScrollPosition, ScrollMode aMode, const nsRect* aRange = nullptr,
      ScrollSnapMode aSnap = nsIScrollbarMediator::DISABLE_SNAP) override {
    mHelper.ScrollTo(aScrollPosition, aMode, nsGkAtoms::other, aRange, aSnap);
  }
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  virtual void ScrollToCSSPixels(const CSSIntPoint& aScrollPosition,
                                 ScrollMode aMode = ScrollMode::Instant,
                                 nsIScrollbarMediator::ScrollSnapMode aSnap =
                                     nsIScrollbarMediator::DISABLE_SNAP,
                                 nsAtom* aOrigin = nullptr) override {
    mHelper.ScrollToCSSPixels(aScrollPosition, aMode, aSnap, aOrigin);
  }
  virtual void ScrollToCSSPixelsApproximate(
      const mozilla::CSSPoint& aScrollPosition,
      nsAtom* aOrigin = nullptr) override {
    mHelper.ScrollToCSSPixelsApproximate(aScrollPosition, aOrigin);
  }
  virtual CSSIntPoint GetScrollPositionCSSPixels() override {
    return mHelper.GetScrollPositionCSSPixels();
  }
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  virtual void ScrollBy(nsIntPoint aDelta, ScrollUnit aUnit, ScrollMode aMode,
                        nsIntPoint* aOverflow, nsAtom* aOrigin = nullptr,
                        nsIScrollableFrame::ScrollMomentum aMomentum =
                            nsIScrollableFrame::NOT_MOMENTUM,
                        nsIScrollbarMediator::ScrollSnapMode aSnap =
                            nsIScrollbarMediator::DISABLE_SNAP) override {
    mHelper.ScrollBy(aDelta, aUnit, aMode, aOverflow, aOrigin, aMomentum,
                     aSnap);
  }
  virtual void ScrollByCSSPixels(const CSSIntPoint& aDelta,
                                 ScrollMode aMode = ScrollMode::Instant,
                                 nsAtom* aOrigin = nullptr,
                                 nsIScrollbarMediator::ScrollSnapMode aSnap =
                                     nsIScrollbarMediator::DEFAULT) override {
    mHelper.ScrollByCSSPixels(aDelta, aMode, aOrigin, aSnap);
  }
  virtual void ScrollSnap() override { mHelper.ScrollSnap(); }
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  virtual void ScrollToRestoredPosition() override {
    mHelper.ScrollToRestoredPosition();
  }
  virtual void AddScrollPositionListener(
      nsIScrollPositionListener* aListener) override {
    mHelper.AddScrollPositionListener(aListener);
  }
  virtual void RemoveScrollPositionListener(
      nsIScrollPositionListener* aListener) override {
    mHelper.RemoveScrollPositionListener(aListener);
  }
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  virtual void CurPosAttributeChanged(nsIContent* aChild) override {
    mHelper.CurPosAttributeChanged(aChild);
  }
  NS_IMETHOD PostScrolledAreaEventForCurrentArea() override {
    mHelper.PostScrolledAreaEvent();
    return NS_OK;
  }
  virtual bool IsScrollingActive(nsDisplayListBuilder* aBuilder) override {
    return mHelper.IsScrollingActive(aBuilder);
  }
  virtual bool IsMaybeScrollingActive() const override {
    return mHelper.IsMaybeScrollingActive();
  }
  virtual bool IsMaybeAsynchronouslyScrolled() override {
    return mHelper.IsMaybeAsynchronouslyScrolled();
  }
  virtual bool IsProcessingAsyncScroll() override {
    return mHelper.IsProcessingAsyncScroll();
  }
  virtual void ResetScrollPositionForLayerPixelAlignment() override {
    mHelper.ResetScrollPositionForLayerPixelAlignment();
  }
  virtual bool DidHistoryRestore() const override {
    return mHelper.mDidHistoryRestore;
  }
  virtual void ClearDidHistoryRestore() override {
    mHelper.mDidHistoryRestore = false;
  }
  virtual void MarkEverScrolled() override { mHelper.MarkEverScrolled(); }
  virtual bool IsRectNearlyVisible(const nsRect& aRect) override {
    return mHelper.IsRectNearlyVisible(aRect);
  }
  virtual nsRect ExpandRectToNearlyVisible(const nsRect& aRect) const override {
    return mHelper.ExpandRectToNearlyVisible(aRect);
  }
  virtual nsAtom* LastScrollOrigin() override {
    return mHelper.LastScrollOrigin();
  }
  virtual nsAtom* LastSmoothScrollOrigin() override {
    return mHelper.LastSmoothScrollOrigin();
  }
  virtual uint32_t CurrentScrollGeneration() override {
    return mHelper.CurrentScrollGeneration();
  }
  virtual nsPoint LastScrollDestination() override {
    return mHelper.LastScrollDestination();
  }
  virtual void ResetScrollInfoIfGeneration(uint32_t aGeneration) override {
    mHelper.ResetScrollInfoIfGeneration(aGeneration);
  }
  virtual bool WantAsyncScroll() const override {
    return mHelper.WantAsyncScroll();
  }
  virtual mozilla::Maybe<mozilla::layers::ScrollMetadata> ComputeScrollMetadata(
      LayerManager* aLayerManager, const nsIFrame* aContainerReferenceFrame,
      const Maybe<ContainerLayerParameters>& aParameters,
      const mozilla::DisplayItemClip* aClip) const override {
    return mHelper.ComputeScrollMetadata(
        aLayerManager, aContainerReferenceFrame, aParameters, aClip);
  }
  virtual void ClipLayerToDisplayPort(
      Layer* aLayer, const mozilla::DisplayItemClip* aClip,
      const ContainerLayerParameters& aParameters) const override {
    mHelper.ClipLayerToDisplayPort(aLayer, aClip, aParameters);
  }
  virtual void MarkScrollbarsDirtyForReflow() const override {
    mHelper.MarkScrollbarsDirtyForReflow();
  }

  // nsIStatefulFrame
  mozilla::UniquePtr<mozilla::PresState> SaveState() override {
    return mHelper.SaveState();
  }
  NS_IMETHOD RestoreState(mozilla::PresState* aState) override {
    NS_ENSURE_ARG_POINTER(aState);
    mHelper.RestoreState(aState);
    return NS_OK;
  }

  virtual bool IsFrameOfType(uint32_t aFlags) const override {
    // Override bogus IsFrameOfType in nsBoxFrame.
    if (aFlags & (nsIFrame::eReplacedContainsBlock | nsIFrame::eReplaced))
      return false;
    return nsBoxFrame::IsFrameOfType(aFlags);
  }

  virtual void ScrollByPage(nsScrollbarFrame* aScrollbar, int32_t aDirection,
                            nsIScrollbarMediator::ScrollSnapMode aSnap =
                                nsIScrollbarMediator::DISABLE_SNAP) override {
    mHelper.ScrollByPage(aScrollbar, aDirection, aSnap);
  }
  virtual void ScrollByWhole(nsScrollbarFrame* aScrollbar, int32_t aDirection,
                             nsIScrollbarMediator::ScrollSnapMode aSnap =
                                 nsIScrollbarMediator::DISABLE_SNAP) override {
    mHelper.ScrollByWhole(aScrollbar, aDirection, aSnap);
  }
  virtual void ScrollByLine(nsScrollbarFrame* aScrollbar, int32_t aDirection,
                            nsIScrollbarMediator::ScrollSnapMode aSnap =
                                nsIScrollbarMediator::DISABLE_SNAP) override {
    mHelper.ScrollByLine(aScrollbar, aDirection, aSnap);
  }
  virtual void RepeatButtonScroll(nsScrollbarFrame* aScrollbar) override {
    mHelper.RepeatButtonScroll(aScrollbar);
  }
  virtual void ThumbMoved(nsScrollbarFrame* aScrollbar, nscoord aOldPos,
                          nscoord aNewPos) override {
    mHelper.ThumbMoved(aScrollbar, aOldPos, aNewPos);
  }
  virtual void ScrollbarReleased(nsScrollbarFrame* aScrollbar) override {
    mHelper.ScrollbarReleased(aScrollbar);
  }
  virtual void VisibilityChanged(bool aVisible) override {}
  virtual nsIFrame* GetScrollbarBox(bool aVertical) override {
    return mHelper.GetScrollbarBox(aVertical);
  }

  virtual void ScrollbarActivityStarted() const override;
  virtual void ScrollbarActivityStopped() const override;

  virtual bool IsScrollbarOnRight() const override {
    return mHelper.IsScrollbarOnRight();
  }

  virtual bool ShouldSuppressScrollbarRepaints() const override {
    return mHelper.ShouldSuppressScrollbarRepaints();
  }

  virtual void SetTransformingByAPZ(bool aTransforming) override {
    mHelper.SetTransformingByAPZ(aTransforming);
  }
  virtual bool UsesContainerScrolling() const override {
    return mHelper.UsesContainerScrolling();
  }
  bool IsTransformingByAPZ() const override {
    return mHelper.IsTransformingByAPZ();
  }
  void SetScrollableByAPZ(bool aScrollable) override {
    mHelper.SetScrollableByAPZ(aScrollable);
  }
  void SetZoomableByAPZ(bool aZoomable) override {
    mHelper.SetZoomableByAPZ(aZoomable);
  }
  void SetHasOutOfFlowContentInsideFilter() override {
    mHelper.SetHasOutOfFlowContentInsideFilter();
  }
  virtual bool DecideScrollableLayer(nsDisplayListBuilder* aBuilder,
                                     nsRect* aVisibleRect, nsRect* aDirtyRect,
                                     bool aSetBase) override {
    return mHelper.DecideScrollableLayer(aBuilder, aVisibleRect, aDirtyRect,
                                         aSetBase);
  }
  virtual void NotifyApzTransaction() override {
    mHelper.NotifyApzTransaction();
  }
  virtual void NotifyApproximateFrameVisibilityUpdate(
      bool aIgnoreDisplayPort) override {
    mHelper.NotifyApproximateFrameVisibilityUpdate(aIgnoreDisplayPort);
  }
  virtual bool GetDisplayPortAtLastApproximateFrameVisibilityUpdate(
      nsRect* aDisplayPort) override {
    return mHelper.GetDisplayPortAtLastApproximateFrameVisibilityUpdate(
        aDisplayPort);
  }
  void TriggerDisplayPortExpiration() override {
    mHelper.TriggerDisplayPortExpiration();
  }

  ScrollSnapInfo GetScrollSnapInfo() const override {
    return mHelper.GetScrollSnapInfo(Nothing());
  }

  virtual bool DragScroll(mozilla::WidgetEvent* aEvent) override {
    return mHelper.DragScroll(aEvent);
  }

  virtual void AsyncScrollbarDragInitiated(
      uint64_t aDragBlockId,
      mozilla::layers::ScrollDirection aDirection) override {
    return mHelper.AsyncScrollbarDragInitiated(aDragBlockId, aDirection);
  }

  virtual void AsyncScrollbarDragRejected() override {
    return mHelper.AsyncScrollbarDragRejected();
  }

  virtual bool IsRootScrollFrameOfDocument() const override {
    return mHelper.IsRootScrollFrameOfDocument();
  }

  virtual const ScrollAnchorContainer* Anchor() const override {
    return &mHelper.mAnchor;
  }

  virtual ScrollAnchorContainer* Anchor() override { return &mHelper.mAnchor; }

  // Return the scrolled frame.
  void AppendDirectlyOwnedAnonBoxes(nsTArray<OwnedAnonBox>& aResult) override {
    aResult.AppendElement(OwnedAnonBox(mHelper.GetScrolledFrame()));
  }

  bool SmoothScrollVisual(const nsPoint& aVisualViewportOffset,
                          mozilla::layers::FrameMetrics::ScrollOffsetUpdateType
                              aUpdateType) override {
    return mHelper.SmoothScrollVisual(aVisualViewportOffset, aUpdateType);
  }

  bool IsSmoothScroll(mozilla::dom::ScrollBehavior aBehavior) const override {
    return mHelper.IsSmoothScroll(aBehavior);
  }

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
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
