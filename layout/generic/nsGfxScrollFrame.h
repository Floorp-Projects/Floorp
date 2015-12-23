/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

class nsPresContext;
class nsIPresShell;
class nsIContent;
class nsIAtom;
class nsPresState;
class nsIScrollPositionListener;
struct ScrollReflowState;

namespace mozilla {
namespace layers {
class Layer;
} // namespace layers
namespace layout {
class ScrollbarActivity;
} // namespace layout
} // namespace mozilla

namespace mozilla {

class ScrollFrameHelper : public nsIReflowCallback {
public:
  typedef nsIFrame::Sides Sides;
  typedef mozilla::CSSIntPoint CSSIntPoint;
  typedef mozilla::layout::ScrollbarActivity ScrollbarActivity;
  typedef mozilla::layers::FrameMetrics FrameMetrics;
  typedef mozilla::layers::Layer Layer;

  class AsyncScroll;
  class AsyncSmoothMSDScroll;

  ScrollFrameHelper(nsContainerFrame* aOuter, bool aIsRoot);
  ~ScrollFrameHelper();

  mozilla::ScrollbarStyles GetScrollbarStylesFromFrame() const;

  // If a child frame was added or removed on the scrollframe,
  // reload our child frame list.
  // We need this if a scrollbar frame is recreated.
  void ReloadChildFrames();

  nsresult CreateAnonymousContent(
    nsTArray<nsIAnonymousContentCreator::ContentInfo>& aElements);
  void AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements, uint32_t aFilter);
  nsresult FireScrollPortEvent();
  void PostOverflowEvent();
  void Destroy();

  void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                        const nsRect&           aDirtyRect,
                        const nsDisplayListSet& aLists);

  void AppendScrollPartsTo(nsDisplayListBuilder*   aBuilder,
                           const nsRect&           aDirtyRect,
                           const nsDisplayListSet& aLists,
                           bool                    aUsingDisplayPort,
                           bool                    aCreateLayer,
                           bool                    aPositioned);

  bool GetBorderRadii(const nsSize& aFrameSize, const nsSize& aBorderArea,
                      Sides aSkipSides, nscoord aRadii[8]) const;

  // nsIReflowCallback
  virtual bool ReflowFinished() override;
  virtual void ReflowCallbackCanceled() override;

  /**
   * @note This method might destroy the frame, pres shell and other objects.
   * Called when the 'curpos' attribute on one of the scrollbars changes.
   */
  void CurPosAttributeChanged(nsIContent* aChild);

  void PostScrollEvent();
  void FireScrollEvent();
  void PostScrolledAreaEvent();
  void FireScrolledAreaEvent();

  bool IsSmoothScrollingEnabled();

  class ScrollEvent : public nsARefreshObserver {
  public:
    NS_INLINE_DECL_REFCOUNTING(ScrollEvent, override)
    explicit ScrollEvent(ScrollFrameHelper *helper);
    void WillRefresh(mozilla::TimeStamp aTime) override;
  protected:
    virtual ~ScrollEvent();
  private:
    ScrollFrameHelper *mHelper;
    RefPtr<nsRefreshDriver> mDriver;
  };

  class AsyncScrollPortEvent : public nsRunnable {
  public:
    NS_DECL_NSIRUNNABLE
    explicit AsyncScrollPortEvent(ScrollFrameHelper *helper) : mHelper(helper) {}
    void Revoke() { mHelper = nullptr; }
  private:
    ScrollFrameHelper *mHelper;
  };

  class ScrolledAreaEvent : public nsRunnable {
  public:
    NS_DECL_NSIRUNNABLE
    explicit ScrolledAreaEvent(ScrollFrameHelper *helper) : mHelper(helper) {}
    void Revoke() { mHelper = nullptr; }
  private:
    ScrollFrameHelper *mHelper;
  };

  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void FinishReflowForScrollbar(nsIContent* aContent, nscoord aMinXY,
                                nscoord aMaxXY, nscoord aCurPosXY,
                                nscoord aPageIncrement,
                                nscoord aIncrement);
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void SetScrollbarEnabled(nsIContent* aContent, nscoord aMaxPos);
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void SetCoordAttribute(nsIContent* aContent, nsIAtom* aAtom, nscoord aSize);

  nscoord GetCoordAttribute(nsIFrame* aFrame, nsIAtom* aAtom, nscoord aDefaultValue,
                            nscoord* aRangeStart, nscoord* aRangeLength);

  /**
   * @note This method might destroy the frame, pres shell and other objects.
   * Update scrollbar curpos attributes to reflect current scroll position
   */
  void UpdateScrollbarPosition();

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
    pt.x = IsLTR() ?
      mScrollPort.x - mScrolledFrame->GetPosition().x :
      mScrollPort.XMost() - mScrolledFrame->GetRect().XMost();
    pt.y = mScrollPort.y - mScrolledFrame->GetPosition().y;
    return pt;
  }
  nsRect GetScrollRange() const;
  // Get the scroll range assuming the scrollport has size (aWidth, aHeight).
  nsRect GetScrollRange(nscoord aWidth, nscoord aHeight) const;
  nsSize GetScrollPositionClampingScrollPortSize() const;
  void FlingSnap(const mozilla::CSSPoint& aDestination);
  void ScrollSnap(nsIScrollableFrame::ScrollMode aMode = nsIScrollableFrame::SMOOTH_MSD);
  void ScrollSnap(const nsPoint &aDestination,
                  nsIScrollableFrame::ScrollMode aMode = nsIScrollableFrame::SMOOTH_MSD);

protected:
  nsRect GetScrollRangeForClamping() const;

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
  void ScrollTo(nsPoint aScrollPosition, nsIScrollableFrame::ScrollMode aMode,
                const nsRect* aRange = nullptr,
                nsIScrollbarMediator::ScrollSnapMode aSnap
                  = nsIScrollbarMediator::DISABLE_SNAP) {
    ScrollToWithOrigin(aScrollPosition, aMode, nsGkAtoms::other, aRange,
                       aSnap);
  }
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void ScrollToCSSPixels(const CSSIntPoint& aScrollPosition,
                         nsIScrollableFrame::ScrollMode aMode
                           = nsIScrollableFrame::INSTANT);
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void ScrollToCSSPixelsApproximate(const mozilla::CSSPoint& aScrollPosition,
                                    nsIAtom* aOrigin = nullptr);

  CSSIntPoint GetScrollPositionCSSPixels();
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void ScrollToImpl(nsPoint aScrollPosition, const nsRect& aRange, nsIAtom* aOrigin = nullptr);
  void ScrollVisual();
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void ScrollBy(nsIntPoint aDelta, nsIScrollableFrame::ScrollUnit aUnit,
                nsIScrollableFrame::ScrollMode aMode, nsIntPoint* aOverflow,
                nsIAtom* aOrigin = nullptr,
                nsIScrollableFrame::ScrollMomentum aMomentum = nsIScrollableFrame::NOT_MOMENTUM,
                nsIScrollbarMediator::ScrollSnapMode aSnap
                  = nsIScrollbarMediator::DISABLE_SNAP);
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void ScrollToRestoredPosition();
  /**
   * GetSnapPointForDestination determines which point to snap to after
   * scrolling. aStartPos gives the position before scrolling and aDestination
   * gives the position after scrolling, with no snapping. Behaviour is
   * dependent on the value of aUnit.
   * Returns true if a suitable snap point could be found and aDestination has
   * been updated to a valid snapping position.
   */
  bool GetSnapPointForDestination(nsIScrollableFrame::ScrollUnit aUnit,
                                  nsPoint aStartPos,
                                  nsPoint &aDestination);

  nsSize GetLineScrollAmount() const;
  nsSize GetPageScrollAmount() const;

  nsPresState* SaveState() const;
  void RestoreState(nsPresState* aState);

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
   * GetScrolledRectInternal is designed to encapsulate deciding which
   * directions of overflow should be reachable by scrolling and which
   * should not.  Callers should NOT depend on it having any particular
   * behavior (although nsXULScrollFrame currently does).
   * 
   * Currently it allows scrolling down and to the right for
   * nsHTMLScrollFrames with LTR directionality and for all
   * nsXULScrollFrames, and allows scrolling down and to the left for
   * nsHTMLScrollFrames with RTL directionality.
   */
  nsRect GetScrolledRectInternal(const nsRect& aScrolledOverflowArea,
                                 const nsSize& aScrollPortSize) const;

  uint32_t GetScrollbarVisibility() const {
    return (mHasVerticalScrollbar ? nsIScrollableFrame::VERTICAL : 0) |
           (mHasHorizontalScrollbar ? nsIScrollableFrame::HORIZONTAL : 0);
  }
  nsMargin GetActualScrollbarSizes() const;
  nsMargin GetDesiredScrollbarSizes(nsBoxLayoutState* aState);
  nscoord GetNondisappearingScrollbarWidth(nsBoxLayoutState* aState,
                                           mozilla::WritingMode aVerticalWM);
  bool IsLTR() const;
  bool IsScrollbarOnRight() const;
  bool IsScrollingActive(nsDisplayListBuilder* aBuilder) const;
  bool IsMaybeScrollingActive() const;
  bool IsProcessingAsyncScroll() const {
    return mAsyncScroll != nullptr || mAsyncSmoothMSDScroll != nullptr;
  }
  void ResetScrollPositionForLayerPixelAlignment()
  {
    mScrollPosForLayerPixelAlignment = GetScrollPosition();
  }

  bool UpdateOverflow();

  void UpdateSticky();

  void UpdatePrevScrolledRect();

  bool IsRectNearlyVisible(const nsRect& aRect) const;
  nsRect ExpandRectToNearlyVisible(const nsRect& aRect) const;

  // adjust the scrollbar rectangle aRect to account for any visible resizer.
  // aHasResizer specifies if there is a content resizer, however this method
  // will also check if a widget resizer is present as well.
  void AdjustScrollbarRectForResizer(nsIFrame* aFrame, nsPresContext* aPresContext,
                                     nsRect& aRect, bool aHasResizer, bool aVertical);
  // returns true if a resizer should be visible
  bool HasResizer() { return mResizerBox && !mCollapsedResizer; }
  void LayoutScrollbars(nsBoxLayoutState& aState,
                        const nsRect& aContentArea,
                        const nsRect& aOldScrollArea);

  bool IsIgnoringViewportClipping() const;

  void MarkScrollbarsDirtyForReflow() const;

  bool ShouldClampScrollPosition() const;

  bool IsAlwaysActive() const;
  void MarkRecentlyScrolled();
  void MarkNotRecentlyScrolled();
  nsExpirationState* GetExpirationState() { return &mActivityExpirationState; }

  void SetTransformingByAPZ(bool aTransforming) {
    mTransformingByAPZ = aTransforming;
    if (!mozilla::css::TextOverflow::HasClippedOverflow(mOuter)) {
      // If the block has some text-overflow stuff we should kick off a paint
      // because we have special behaviour for it when APZ scrolling is active.
      mOuter->SchedulePaint();
    }
    // Update windowed plugin visibility in response to apz scrolling events.
    NotifyPluginFrames(aTransforming ? BEGIN_APZ : END_APZ);
  }
  bool IsTransformingByAPZ() const {
    return mTransformingByAPZ;
  }
  void SetZoomableByAPZ(bool aZoomable);

  bool UsesContainerScrolling() const;

  bool DecideScrollableLayer(nsDisplayListBuilder* aBuilder,
                             nsRect* aDirtyRect,
                             bool aAllowCreateDisplayPort);

  void ScheduleSyntheticMouseMove();
  static void ScrollActivityCallback(nsITimer *aTimer, void* anInstance);

  void HandleScrollbarStyleSwitching();

  nsIAtom* LastScrollOrigin() const { return mLastScrollOrigin; }
  nsIAtom* LastSmoothScrollOrigin() const { return mLastSmoothScrollOrigin; }
  uint32_t CurrentScrollGeneration() const { return mScrollGeneration; }
  nsPoint LastScrollDestination() const { return mDestination; }
  void ResetScrollInfoIfGeneration(uint32_t aGeneration) {
    if (aGeneration == mScrollGeneration) {
      mLastScrollOrigin = nullptr;
      mLastSmoothScrollOrigin = nullptr;
    }
  }
  bool WantAsyncScroll() const;
  Maybe<mozilla::layers::FrameMetrics> ComputeFrameMetrics(
    Layer* aLayer, nsIFrame* aContainerReferenceFrame,
    const ContainerLayerParameters& aParameters,
    const mozilla::DisplayItemClip* aClip) const;

  // nsIScrollbarMediator
  void ScrollByPage(nsScrollbarFrame* aScrollbar, int32_t aDirection,
                    nsIScrollbarMediator::ScrollSnapMode aSnap
                      = nsIScrollbarMediator::DISABLE_SNAP);
  void ScrollByWhole(nsScrollbarFrame* aScrollbar, int32_t aDirection,
                     nsIScrollbarMediator::ScrollSnapMode aSnap
                       = nsIScrollbarMediator::DISABLE_SNAP);
  void ScrollByLine(nsScrollbarFrame* aScrollbar, int32_t aDirection,
                    nsIScrollbarMediator::ScrollSnapMode aSnap
                      = nsIScrollbarMediator::DISABLE_SNAP);
  void RepeatButtonScroll(nsScrollbarFrame* aScrollbar);
  void ThumbMoved(nsScrollbarFrame* aScrollbar,
                  nscoord aOldPos,
                  nscoord aNewPos);
  void ScrollbarReleased(nsScrollbarFrame* aScrollbar);
  void ScrollByUnit(nsScrollbarFrame* aScrollbar,
                    nsIScrollableFrame::ScrollMode aMode,
                    int32_t aDirection,
                    nsIScrollableFrame::ScrollUnit aUnit,
                    nsIScrollbarMediator::ScrollSnapMode aSnap
                      = nsIScrollbarMediator::DISABLE_SNAP);

  // owning references to the nsIAnonymousContentCreator-built content
  nsCOMPtr<nsIContent> mHScrollbarContent;
  nsCOMPtr<nsIContent> mVScrollbarContent;
  nsCOMPtr<nsIContent> mScrollCornerContent;
  nsCOMPtr<nsIContent> mResizerContent;

  RefPtr<ScrollEvent> mScrollEvent;
  nsRevocableEventPtr<AsyncScrollPortEvent> mAsyncScrollPortEvent;
  nsRevocableEventPtr<ScrolledAreaEvent> mScrolledAreaEvent;
  nsIFrame* mHScrollbarBox;
  nsIFrame* mVScrollbarBox;
  nsIFrame* mScrolledFrame;
  nsIFrame* mScrollCornerBox;
  nsIFrame* mResizerBox;
  nsContainerFrame* mOuter;
  RefPtr<AsyncScroll> mAsyncScroll;
  RefPtr<AsyncSmoothMSDScroll> mAsyncSmoothMSDScroll;
  RefPtr<ScrollbarActivity> mScrollbarActivity;
  nsTArray<nsIScrollPositionListener*> mListeners;
  nsIAtom* mLastScrollOrigin;
  nsIAtom* mLastSmoothScrollOrigin;
  Maybe<nsPoint> mApzSmoothScrollDestination;
  uint32_t mScrollGeneration;
  nsRect mScrollPort;
  // Where we're currently scrolling to, if we're scrolling asynchronously.
  // If we're not in the middle of an asynchronous scroll then this is
  // just the current scroll position. ScrollBy will choose its
  // destination based on this value.
  nsPoint mDestination;
  nsPoint mScrollPosAtLastPaint;

  // A goal position to try to scroll to as content loads. As long as mLastPos
  // matches the current logical scroll position, we try to scroll to mRestorePos
  // after every reflow --- because after each time content is loaded/added to the
  // scrollable element, there will be a reflow.
  nsPoint mRestorePos;
  // The last logical position we scrolled to while trying to restore mRestorePos, or
  // 0,0 when this is a new frame. Set to -1,-1 once we've scrolled for any reason
  // other than trying to restore mRestorePos.
  nsPoint mLastPos;

  nsExpirationState mActivityExpirationState;

  nsCOMPtr<nsITimer> mScrollActivityTimer;
  nsPoint mScrollPosForLayerPixelAlignment;

  // The scroll position where we last updated image visibility.
  nsPoint mLastUpdateImagesPos;

  nsRect mPrevScrolledRect;

  FrameMetrics::ViewID mScrollParentID;

  bool mNeverHasVerticalScrollbar:1;
  bool mNeverHasHorizontalScrollbar:1;
  bool mHasVerticalScrollbar:1;
  bool mHasHorizontalScrollbar:1;
  bool mFrameIsUpdatingScrollbar:1;
  bool mDidHistoryRestore:1;
  // Is this the scrollframe for the document's viewport?
  bool mIsRoot:1;
  // True if we should clip all descendants, false if we should only clip
  // descendants for which we are the containing block.
  bool mClipAllDescendants:1;
  // If true, don't try to layout the scrollbars in Reflow().  This can be
  // useful if multiple passes are involved, because we don't want to place the
  // scrollbars at the wrong size.
  bool mSupppressScrollbarUpdate:1;
  // If true, we skipped a scrollbar layout due to mSupppressScrollbarUpdate
  // being set at some point.  That means we should lay out scrollbars even if
  // it might not strictly be needed next time mSupppressScrollbarUpdate is
  // false.
  bool mSkippedScrollbarLayout:1;

  bool mHadNonInitialReflow:1;
  // State used only by PostScrollEvents so we know
  // which overflow states have changed.
  bool mHorizontalOverflow:1;
  bool mVerticalOverflow:1;
  bool mPostedReflowCallback:1;
  bool mMayHaveDirtyFixedChildren:1;
  // If true, need to actually update our scrollbar attributes in the
  // reflow callback.
  bool mUpdateScrollbarAttributes:1;
  // If true, we should be prepared to scroll using this scrollframe
  // by placing descendant content into its own layer(s)
  bool mHasBeenScrolledRecently:1;
  // If true, the resizer is collapsed and not displayed
  bool mCollapsedResizer:1;

  // If true, the scroll frame should always be active because we always build
  // a scrollable layer. Used for asynchronous scrolling.
  bool mWillBuildScrollableLayer:1;

  // Whether we are the root scroll frame that is used for containerful
  // scrolling with a display port. If true, the scrollable frame
  // shouldn't attach frame metrics to its layers because the container
  // will already have the necessary frame metrics.
  bool mIsScrollableLayerInRootContainer:1;

  // If true, add clipping in ScrollFrameHelper::ComputeFrameMetrics.
  bool mAddClipRectToLayer:1;

  // True if this frame has been scrolled at least once
  bool mHasBeenScrolled:1;

  // True if the events synthesized by OSX to produce momentum scrolling should
  // be ignored.  Reset when the next real, non-synthesized scroll event occurs.
  bool mIgnoreMomentumScroll:1;

  // True if the APZ is in the process of async-transforming this scrollframe,
  // (as best as we can tell on the main thread, anyway).
  bool mTransformingByAPZ:1;

  // True if the APZ is allowed to zoom this scrollframe.
  bool mZoomableByAPZ:1;

  mozilla::layout::ScrollVelocityQueue mVelocityQueue;

protected:
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void ScrollToWithOrigin(nsPoint aScrollPosition,
                          nsIScrollableFrame::ScrollMode aMode,
                          nsIAtom *aOrigin, // nullptr indicates "other" origin
                          const nsRect* aRange,
                          nsIScrollbarMediator::ScrollSnapMode aSnap
                            = nsIScrollbarMediator::DISABLE_SNAP);

  void CompleteAsyncScroll(const nsRect &aRange, nsIAtom* aOrigin = nullptr);

  /*
   * Helper that notifies plugins about async smooth scroll operations managed
   * by nsGfxScrollFrame.
   */
  enum AsyncScrollEventType { BEGIN_DOM, BEGIN_APZ, END_DOM, END_APZ };
  void NotifyPluginFrames(AsyncScrollEventType aEvent);
  AsyncScrollEventType mAsyncScrollEvent;

  static void EnsureImageVisPrefsCached();
  static bool sImageVisPrefsCached;
  // The number of scrollports wide/high to expand when looking for images.
  static uint32_t sHorzExpandScrollPort;
  static uint32_t sVertExpandScrollPort;
  // The fraction of the scrollport we allow to scroll by before we schedule
  // an update of image visibility.
  static int32_t sHorzScrollFraction;
  static int32_t sVertScrollFraction;
};

} // namespace mozilla

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
  friend nsHTMLScrollFrame* NS_NewHTMLScrollFrame(nsIPresShell* aPresShell,
                                                  nsStyleContext* aContext,
                                                  bool aIsRoot);

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  virtual mozilla::WritingMode GetWritingMode() const override
  {
    if (mHelper.mScrolledFrame) {
      return mHelper.mScrolledFrame->GetWritingMode();
    }
    return nsIFrame::GetWritingMode();
  }

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override {
    mHelper.BuildDisplayList(aBuilder, aDirtyRect, aLists);
  }

  bool TryLayout(ScrollReflowState* aState,
                   nsHTMLReflowMetrics* aKidMetrics,
                   bool aAssumeVScroll, bool aAssumeHScroll,
                   bool aForce);
  bool ScrolledContentDependsOnHeight(ScrollReflowState* aState);
  void ReflowScrolledFrame(ScrollReflowState* aState,
                           bool aAssumeHScroll,
                           bool aAssumeVScroll,
                           nsHTMLReflowMetrics* aMetrics,
                           bool aFirstPass);
  void ReflowContents(ScrollReflowState* aState,
                      const nsHTMLReflowMetrics& aDesiredSize);
  void PlaceScrollArea(const ScrollReflowState& aState,
                       const nsPoint& aScrollPosition);
  nscoord GetIntrinsicVScrollbarWidth(nsRenderingContext *aRenderingContext);

  virtual bool GetBorderRadii(const nsSize& aFrameSize, const nsSize& aBorderArea,
                              Sides aSkipSides, nscoord aRadii[8]) const override {
    return mHelper.GetBorderRadii(aFrameSize, aBorderArea, aSkipSides, aRadii);
  }

  virtual nscoord GetMinISize(nsRenderingContext *aRenderingContext) override;
  virtual nscoord GetPrefISize(nsRenderingContext *aRenderingContext) override;
  virtual nsresult GetPadding(nsMargin& aPadding) override;
  virtual bool IsCollapsed() override;
  
  virtual void Reflow(nsPresContext*           aPresContext,
                      nsHTMLReflowMetrics&     aDesiredSize,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&          aStatus) override;

  virtual bool UpdateOverflow() override {
    return mHelper.UpdateOverflow();
  }

  // Called to set the child frames. We typically have three: the scroll area,
  // the vertical scrollbar, and the horizontal scrollbar.
  virtual void SetInitialChildList(ChildListID     aListID,
                                   nsFrameList&    aChildList) override;
  virtual void AppendFrames(ChildListID     aListID,
                            nsFrameList&    aFrameList) override;
  virtual void InsertFrames(ChildListID     aListID,
                            nsIFrame*       aPrevFrame,
                            nsFrameList&    aFrameList) override;
  virtual void RemoveFrame(ChildListID     aListID,
                           nsIFrame*       aOldFrame) override;

  virtual void DestroyFrom(nsIFrame* aDestructRoot) override;

  virtual nsIScrollableFrame* GetScrollTargetFrame() override {
    return this;
  }

  virtual nsContainerFrame* GetContentInsertionFrame() override {
    return mHelper.GetScrolledFrame()->GetContentInsertionFrame();
  }

  virtual bool DoesClipChildren() override { return true; }
  virtual nsSplittableType GetSplittableType() const override;

  virtual nsPoint GetPositionOfChildIgnoringScrolling(nsIFrame* aChild) override
  { nsPoint pt = aChild->GetPosition();
    if (aChild == mHelper.GetScrolledFrame()) pt += GetScrollPosition();
    return pt;
  }

  // nsIAnonymousContentCreator
  virtual nsresult CreateAnonymousContent(nsTArray<ContentInfo>& aElements) override;
  virtual void AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                        uint32_t aFilter) override;

  // nsIScrollableFrame
  virtual nsIFrame* GetScrolledFrame() const override {
    return mHelper.GetScrolledFrame();
  }
  virtual mozilla::ScrollbarStyles GetScrollbarStyles() const override {
    return mHelper.GetScrollbarStylesFromFrame();
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
          nsRenderingContext* aRC) override {
    nsBoxLayoutState bls(aPresContext, aRC, 0);
    return GetDesiredScrollbarSizes(&bls);
  }
  virtual nscoord GetNondisappearingScrollbarWidth(nsPresContext* aPresContext,
          nsRenderingContext* aRC, mozilla::WritingMode aWM) override {
    nsBoxLayoutState bls(aPresContext, aRC, 0);
    return mHelper.GetNondisappearingScrollbarWidth(&bls, aWM);
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
  virtual nsRect GetScrollRange() const override {
    return mHelper.GetScrollRange();
  }
  virtual nsSize GetScrollPositionClampingScrollPortSize() const override {
    return mHelper.GetScrollPositionClampingScrollPortSize();
  }
  virtual nsSize GetLineScrollAmount() const override {
    return mHelper.GetLineScrollAmount();
  }
  virtual nsSize GetPageScrollAmount() const override {
    return mHelper.GetPageScrollAmount();
  }
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  virtual void ScrollTo(nsPoint aScrollPosition, ScrollMode aMode,
                        const nsRect* aRange = nullptr,
                        nsIScrollbarMediator::ScrollSnapMode aSnap
                          = nsIScrollbarMediator::DISABLE_SNAP)
                        override {
    mHelper.ScrollTo(aScrollPosition, aMode, aRange, aSnap);
  }
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  virtual void ScrollToCSSPixels(const CSSIntPoint& aScrollPosition,
                                 nsIScrollableFrame::ScrollMode aMode
                                   = nsIScrollableFrame::INSTANT) override {
    mHelper.ScrollToCSSPixels(aScrollPosition, aMode);
  }
  virtual void ScrollToCSSPixelsApproximate(const mozilla::CSSPoint& aScrollPosition,
                                            nsIAtom* aOrigin = nullptr) override {
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
                        nsIntPoint* aOverflow, nsIAtom* aOrigin = nullptr,
                        nsIScrollableFrame::ScrollMomentum aMomentum = nsIScrollableFrame::NOT_MOMENTUM,
                        nsIScrollbarMediator::ScrollSnapMode aSnap
                          = nsIScrollbarMediator::DISABLE_SNAP)
                        override {
    mHelper.ScrollBy(aDelta, aUnit, aMode, aOverflow, aOrigin, aMomentum, aSnap);
  }
  virtual void FlingSnap(const mozilla::CSSPoint& aDestination) override {
    mHelper.FlingSnap(aDestination);
  }
  virtual void ScrollSnap() override {
    mHelper.ScrollSnap();
  }
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  virtual void ScrollToRestoredPosition() override {
    mHelper.ScrollToRestoredPosition();
  }
  virtual void AddScrollPositionListener(nsIScrollPositionListener* aListener) override {
    mHelper.AddScrollPositionListener(aListener);
  }
  virtual void RemoveScrollPositionListener(nsIScrollPositionListener* aListener) override {
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
  virtual bool IsRectNearlyVisible(const nsRect& aRect) override {
    return mHelper.IsRectNearlyVisible(aRect);
  }
  virtual nsRect ExpandRectToNearlyVisible(const nsRect& aRect) const override {
    return mHelper.ExpandRectToNearlyVisible(aRect);
  }
  virtual nsIAtom* LastScrollOrigin() override {
    return mHelper.LastScrollOrigin();
  }
  virtual nsIAtom* LastSmoothScrollOrigin() override {
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
  virtual mozilla::Maybe<mozilla::layers::FrameMetrics> ComputeFrameMetrics(
    Layer* aLayer, nsIFrame* aContainerReferenceFrame,
    const ContainerLayerParameters& aParameters,
    const mozilla::DisplayItemClip* aClip) const override
  {
    return mHelper.ComputeFrameMetrics(aLayer, aContainerReferenceFrame, aParameters, aClip);
  }
  virtual bool IsIgnoringViewportClipping() const override {
    return mHelper.IsIgnoringViewportClipping();
  }
  virtual void MarkScrollbarsDirtyForReflow() const override {
    mHelper.MarkScrollbarsDirtyForReflow();
  }
  virtual bool UsesContainerScrolling() const override {
    return mHelper.UsesContainerScrolling();
  }
  virtual bool DecideScrollableLayer(nsDisplayListBuilder* aBuilder,
                                     nsRect* aDirtyRect,
                                     bool aAllowCreateDisplayPort) override {
    return mHelper.DecideScrollableLayer(aBuilder, aDirtyRect, aAllowCreateDisplayPort);
  }

  // nsIStatefulFrame
  NS_IMETHOD SaveState(nsPresState** aState) override {
    NS_ENSURE_ARG_POINTER(aState);
    *aState = mHelper.SaveState();
    return NS_OK;
  }
  NS_IMETHOD RestoreState(nsPresState* aState) override {
    NS_ENSURE_ARG_POINTER(aState);
    mHelper.RestoreState(aState);
    return NS_OK;
  }

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::scrollFrame
   */
  virtual nsIAtom* GetType() const override;

  // nsIScrollbarMediator
  virtual void ScrollByPage(nsScrollbarFrame* aScrollbar, int32_t aDirection,
                            nsIScrollbarMediator::ScrollSnapMode aSnap
                              = nsIScrollbarMediator::DISABLE_SNAP) override {
    mHelper.ScrollByPage(aScrollbar, aDirection, aSnap);
  }
  virtual void ScrollByWhole(nsScrollbarFrame* aScrollbar, int32_t aDirection,
                             nsIScrollbarMediator::ScrollSnapMode aSnap
                               = nsIScrollbarMediator::DISABLE_SNAP) override {
    mHelper.ScrollByWhole(aScrollbar, aDirection, aSnap);
  }
  virtual void ScrollByLine(nsScrollbarFrame* aScrollbar, int32_t aDirection,
                            nsIScrollbarMediator::ScrollSnapMode aSnap
                              = nsIScrollbarMediator::DISABLE_SNAP) override {
    mHelper.ScrollByLine(aScrollbar, aDirection, aSnap);
  }
  virtual void RepeatButtonScroll(nsScrollbarFrame* aScrollbar) override {
    mHelper.RepeatButtonScroll(aScrollbar);
  }
  virtual void ThumbMoved(nsScrollbarFrame* aScrollbar,
                          nscoord aOldPos,
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

  virtual void SetTransformingByAPZ(bool aTransforming) override {
    mHelper.SetTransformingByAPZ(aTransforming);
  }
  bool IsTransformingByAPZ() const override {
    return mHelper.IsTransformingByAPZ();
  }
  void SetZoomableByAPZ(bool aZoomable) override {
    mHelper.SetZoomableByAPZ(aZoomable);
  }
  
#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif

#ifdef ACCESSIBILITY
  virtual mozilla::a11y::AccType AccessibleType() override;
#endif

protected:
  nsHTMLScrollFrame(nsStyleContext* aContext, bool aIsRoot);
  void SetSuppressScrollbarUpdate(bool aSuppress) {
    mHelper.mSupppressScrollbarUpdate = aSuppress;
  }
  bool GuessHScrollbarNeeded(const ScrollReflowState& aState);
  bool GuessVScrollbarNeeded(const ScrollReflowState& aState);

  bool IsScrollbarUpdateSuppressed() const {
    return mHelper.mSupppressScrollbarUpdate;
  }

  // Return whether we're in an "initial" reflow.  Some reflows with
  // NS_FRAME_FIRST_REFLOW set are NOT "initial" as far as we're concerned.
  bool InInitialReflow() const;
  
  /**
   * Override this to return false if computed bsize/min-bsize/max-bsize
   * should NOT be propagated to child content.
   * nsListControlFrame uses this.
   */
  virtual bool ShouldPropagateComputedBSizeToScrolledContent() const { return true; }

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
                               public nsIStatefulFrame
{
public:
  typedef mozilla::ScrollFrameHelper ScrollFrameHelper;
  typedef mozilla::CSSIntPoint CSSIntPoint;

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  friend nsXULScrollFrame* NS_NewXULScrollFrame(nsIPresShell* aPresShell,
                                                nsStyleContext* aContext,
                                                bool aIsRoot,
                                                bool aClipAllDescendants);

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override {
    mHelper.BuildDisplayList(aBuilder, aDirtyRect, aLists);
  }

  // XXXldb Is this actually used?
#if 0
  virtual nscoord GetMinISize(nsRenderingContext *aRenderingContext) override;
#endif

  virtual bool UpdateOverflow() override {
    return mHelper.UpdateOverflow();
  }

  // Called to set the child frames. We typically have three: the scroll area,
  // the vertical scrollbar, and the horizontal scrollbar.
  virtual void SetInitialChildList(ChildListID     aListID,
                                   nsFrameList&    aChildList) override;
  virtual void AppendFrames(ChildListID     aListID,
                            nsFrameList&    aFrameList) override;
  virtual void InsertFrames(ChildListID     aListID,
                            nsIFrame*       aPrevFrame,
                            nsFrameList&    aFrameList) override;
  virtual void RemoveFrame(ChildListID     aListID,
                           nsIFrame*       aOldFrame) override;

  virtual void DestroyFrom(nsIFrame* aDestructRoot) override;


  virtual nsIScrollableFrame* GetScrollTargetFrame() override {
    return this;
  }

  virtual nsContainerFrame* GetContentInsertionFrame() override {
    return mHelper.GetScrolledFrame()->GetContentInsertionFrame();
  }

  virtual bool DoesClipChildren() override { return true; }
  virtual nsSplittableType GetSplittableType() const override;

  virtual nsPoint GetPositionOfChildIgnoringScrolling(nsIFrame* aChild) override
  { nsPoint pt = aChild->GetPosition();
    if (aChild == mHelper.GetScrolledFrame())
      pt += mHelper.GetLogicalScrollPosition();
    return pt;
  }

  // nsIAnonymousContentCreator
  virtual nsresult CreateAnonymousContent(nsTArray<ContentInfo>& aElements) override;
  virtual void AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                        uint32_t aFilter) override;

  virtual nsSize GetMinSize(nsBoxLayoutState& aBoxLayoutState) override;
  virtual nsSize GetPrefSize(nsBoxLayoutState& aBoxLayoutState) override;
  virtual nsSize GetMaxSize(nsBoxLayoutState& aBoxLayoutState) override;
  virtual nscoord GetBoxAscent(nsBoxLayoutState& aBoxLayoutState) override;

  NS_IMETHOD DoLayout(nsBoxLayoutState& aBoxLayoutState) override;
  virtual nsresult GetPadding(nsMargin& aPadding) override;

  virtual bool GetBorderRadii(const nsSize& aFrameSize, const nsSize& aBorderArea,
                              Sides aSkipSides, nscoord aRadii[8]) const override {
    return mHelper.GetBorderRadii(aFrameSize, aBorderArea, aSkipSides, aRadii);
  }

  nsresult Layout(nsBoxLayoutState& aState);
  void LayoutScrollArea(nsBoxLayoutState& aState, const nsPoint& aScrollPosition);

  static bool AddRemoveScrollbar(bool& aHasScrollbar, 
                                   nscoord& aXY, 
                                   nscoord& aSize, 
                                   nscoord aSbSize, 
                                   bool aOnRightOrBottom, 
                                   bool aAdd);
  
  bool AddRemoveScrollbar(nsBoxLayoutState& aState, 
                            bool aOnRightOrBottom, 
                            bool aHorizontal, 
                            bool aAdd);
  
  bool AddHorizontalScrollbar (nsBoxLayoutState& aState, bool aOnBottom);
  bool AddVerticalScrollbar   (nsBoxLayoutState& aState, bool aOnRight);
  void RemoveHorizontalScrollbar(nsBoxLayoutState& aState, bool aOnBottom);
  void RemoveVerticalScrollbar  (nsBoxLayoutState& aState, bool aOnRight);

  static void AdjustReflowStateForPrintPreview(nsBoxLayoutState& aState, bool& aSetBack);
  static void AdjustReflowStateBack(nsBoxLayoutState& aState, bool aSetBack);

  // nsIScrollableFrame
  virtual nsIFrame* GetScrolledFrame() const override {
    return mHelper.GetScrolledFrame();
  }
  virtual mozilla::ScrollbarStyles GetScrollbarStyles() const override {
    return mHelper.GetScrollbarStylesFromFrame();
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
          nsRenderingContext* aRC) override {
    nsBoxLayoutState bls(aPresContext, aRC, 0);
    return GetDesiredScrollbarSizes(&bls);
  }
  virtual nscoord GetNondisappearingScrollbarWidth(nsPresContext* aPresContext,
          nsRenderingContext* aRC, mozilla::WritingMode aWM) override {
    nsBoxLayoutState bls(aPresContext, aRC, 0);
    return mHelper.GetNondisappearingScrollbarWidth(&bls, aWM);
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
  virtual nsRect GetScrollRange() const override {
    return mHelper.GetScrollRange();
  }
  virtual nsSize GetScrollPositionClampingScrollPortSize() const override {
    return mHelper.GetScrollPositionClampingScrollPortSize();
  }
  virtual nsSize GetLineScrollAmount() const override {
    return mHelper.GetLineScrollAmount();
  }
  virtual nsSize GetPageScrollAmount() const override {
    return mHelper.GetPageScrollAmount();
  }
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  virtual void ScrollTo(nsPoint aScrollPosition, ScrollMode aMode,
                        const nsRect* aRange = nullptr,
                        ScrollSnapMode aSnap = nsIScrollbarMediator::DISABLE_SNAP)
                        override {
    mHelper.ScrollTo(aScrollPosition, aMode, aRange, aSnap);
  }
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  virtual void ScrollToCSSPixels(const CSSIntPoint& aScrollPosition,
                                 nsIScrollableFrame::ScrollMode aMode
                                   = nsIScrollableFrame::INSTANT) override {
    mHelper.ScrollToCSSPixels(aScrollPosition, aMode);
  }
  virtual void ScrollToCSSPixelsApproximate(const mozilla::CSSPoint& aScrollPosition,
                                            nsIAtom* aOrigin = nullptr) override {
    mHelper.ScrollToCSSPixelsApproximate(aScrollPosition, aOrigin);
  }
  virtual CSSIntPoint GetScrollPositionCSSPixels() override {
    return mHelper.GetScrollPositionCSSPixels();
  }
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  virtual void ScrollBy(nsIntPoint aDelta, ScrollUnit aUnit, ScrollMode aMode,
                        nsIntPoint* aOverflow, nsIAtom* aOrigin = nullptr,
                        nsIScrollableFrame::ScrollMomentum aMomentum = nsIScrollableFrame::NOT_MOMENTUM,
                        nsIScrollbarMediator::ScrollSnapMode aSnap
                          = nsIScrollbarMediator::DISABLE_SNAP)
                        override {
    mHelper.ScrollBy(aDelta, aUnit, aMode, aOverflow, aOrigin, aMomentum, aSnap);
  }
  virtual void FlingSnap(const mozilla::CSSPoint& aDestination) override {
    mHelper.FlingSnap(aDestination);
  }
  virtual void ScrollSnap() override {
    mHelper.ScrollSnap();
  }
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  virtual void ScrollToRestoredPosition() override {
    mHelper.ScrollToRestoredPosition();
  }
  virtual void AddScrollPositionListener(nsIScrollPositionListener* aListener) override {
    mHelper.AddScrollPositionListener(aListener);
  }
  virtual void RemoveScrollPositionListener(nsIScrollPositionListener* aListener) override {
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
  virtual bool IsRectNearlyVisible(const nsRect& aRect) override {
    return mHelper.IsRectNearlyVisible(aRect);
  }
  virtual nsRect ExpandRectToNearlyVisible(const nsRect& aRect) const override {
    return mHelper.ExpandRectToNearlyVisible(aRect);
  }
  virtual nsIAtom* LastScrollOrigin() override {
    return mHelper.LastScrollOrigin();
  }
  virtual nsIAtom* LastSmoothScrollOrigin() override {
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
  virtual mozilla::Maybe<mozilla::layers::FrameMetrics> ComputeFrameMetrics(
    Layer* aLayer, nsIFrame* aContainerReferenceFrame,
    const ContainerLayerParameters& aParameters,
    const mozilla::DisplayItemClip* aClip) const override
  {
    return mHelper.ComputeFrameMetrics(aLayer, aContainerReferenceFrame, aParameters, aClip);
  }
  virtual bool IsIgnoringViewportClipping() const override {
    return mHelper.IsIgnoringViewportClipping();
  }
  virtual void MarkScrollbarsDirtyForReflow() const override {
    mHelper.MarkScrollbarsDirtyForReflow();
  }

  // nsIStatefulFrame
  NS_IMETHOD SaveState(nsPresState** aState) override {
    NS_ENSURE_ARG_POINTER(aState);
    *aState = mHelper.SaveState();
    return NS_OK;
  }
  NS_IMETHOD RestoreState(nsPresState* aState) override {
    NS_ENSURE_ARG_POINTER(aState);
    mHelper.RestoreState(aState);
    return NS_OK;
  }

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::scrollFrame
   */
  virtual nsIAtom* GetType() const override;
  
  virtual bool IsFrameOfType(uint32_t aFlags) const override
  {
    // Override bogus IsFrameOfType in nsBoxFrame.
    if (aFlags & (nsIFrame::eReplacedContainsBlock | nsIFrame::eReplaced))
      return false;
    return nsBoxFrame::IsFrameOfType(aFlags);
  }

  virtual void ScrollByPage(nsScrollbarFrame* aScrollbar, int32_t aDirection,
                            nsIScrollbarMediator::ScrollSnapMode aSnap
                              = nsIScrollbarMediator::DISABLE_SNAP) override {
    mHelper.ScrollByPage(aScrollbar, aDirection, aSnap);
  }
  virtual void ScrollByWhole(nsScrollbarFrame* aScrollbar, int32_t aDirection,
                             nsIScrollbarMediator::ScrollSnapMode aSnap
                               = nsIScrollbarMediator::DISABLE_SNAP) override {
    mHelper.ScrollByWhole(aScrollbar, aDirection, aSnap);
  }
  virtual void ScrollByLine(nsScrollbarFrame* aScrollbar, int32_t aDirection,
                            nsIScrollbarMediator::ScrollSnapMode aSnap
                              = nsIScrollbarMediator::DISABLE_SNAP) override {
    mHelper.ScrollByLine(aScrollbar, aDirection, aSnap);
  }
  virtual void RepeatButtonScroll(nsScrollbarFrame* aScrollbar) override {
    mHelper.RepeatButtonScroll(aScrollbar);
  }
  virtual void ThumbMoved(nsScrollbarFrame* aScrollbar,
                          nscoord aOldPos,
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

  virtual void SetTransformingByAPZ(bool aTransforming) override {
    mHelper.SetTransformingByAPZ(aTransforming);
  }
  virtual bool UsesContainerScrolling() const override {
    return mHelper.UsesContainerScrolling();
  }
  bool IsTransformingByAPZ() const override {
    return mHelper.IsTransformingByAPZ();
  }
  void SetZoomableByAPZ(bool aZoomable) override {
    mHelper.SetZoomableByAPZ(aZoomable);
  }
  virtual bool DecideScrollableLayer(nsDisplayListBuilder* aBuilder,
                                     nsRect* aDirtyRect,
                                     bool aAllowCreateDisplayPort) override {
    return mHelper.DecideScrollableLayer(aBuilder, aDirtyRect, aAllowCreateDisplayPort);
  }


#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif

protected:
  nsXULScrollFrame(nsStyleContext* aContext, bool aIsRoot,
                   bool aClipAllDescendants);

  void ClampAndSetBounds(nsBoxLayoutState& aState, 
                         nsRect& aRect,
                         nsPoint aScrollPosition,
                         bool aRemoveOverflowAreas = false) {
    /* 
     * For RTL frames, restore the original scrolled position of the right
     * edge, then subtract the current width to find the physical position.
     */
    if (!mHelper.IsLTR()) {
      aRect.x = mHelper.mScrollPort.XMost() - aScrollPosition.x - aRect.width;
    }
    mHelper.mScrolledFrame->SetBounds(aState, aRect, aRemoveOverflowAreas);
  }

private:
  friend class mozilla::ScrollFrameHelper;
  ScrollFrameHelper mHelper;
};

#endif /* nsGfxScrollFrame_h___ */
