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
#include "nsIScrollableFrame.h"
#include "nsIScrollbarMediator.h"
#include "nsIStatefulFrame.h"
#include "nsThreadUtils.h"
#include "nsIReflowCallback.h"
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
class AutoContainsBlendModeCapturer;

namespace mozilla {
class PresShell;
enum class StyleScrollbarWidth : uint8_t;
struct ScrollReflowInput;
struct StyleScrollSnapAlign;
namespace layers {
class Layer;
class WebRenderLayerManager;
}  // namespace layers
namespace layout {
class ScrollbarActivity;
}  // namespace layout

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
                          public nsIReflowCallback,
                          public nsIStatefulFrame {
 public:
  using Sides = nsIFrame::Sides;
  using ScrollbarActivity = mozilla::layout::ScrollbarActivity;
  using ScrollSnapFlags = mozilla::ScrollSnapFlags;
  using ScrollTriggeredByScript = mozilla::ScrollTriggeredByScript;
  using ScrollSnapTargetIds = mozilla::ScrollSnapTargetIds;
  using FrameMetrics = mozilla::layers::FrameMetrics;
  using ScrollableLayerGuid = mozilla::layers::ScrollableLayerGuid;
  using ScrollSnapInfo = mozilla::ScrollSnapInfo;
  using WebRenderLayerManager = mozilla::layers::WebRenderLayerManager;
  using APZScrollAnimationType = mozilla::APZScrollAnimationType;
  using ScrollDirections = mozilla::layers::ScrollDirections;
  using ScrollDirection = mozilla::layers::ScrollDirection;
  using Element = mozilla::dom::Element;
  using SnapTargetSet = nsTHashSet<RefPtr<nsIContent>>;
  using InScrollingGesture = nsIScrollableFrame::InScrollingGesture;

  using CSSIntPoint = mozilla::CSSIntPoint;
  using CSSPoint = mozilla::CSSPoint;
  using ScrollReflowInput = mozilla::ScrollReflowInput;
  using ScrollAnchorContainer = mozilla::layout::ScrollAnchorContainer;
  friend nsHTMLScrollFrame* NS_NewHTMLScrollFrame(
      mozilla::PresShell* aPresShell, ComputedStyle* aStyle, bool aIsRoot);
  friend class mozilla::layout::ScrollAnchorContainer;

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsHTMLScrollFrame)

  void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                        const nsDisplayListSet& aLists) override;

  bool TryLayout(ScrollReflowInput& aState, ReflowOutput* aKidMetrics,
                 bool aAssumeHScroll, bool aAssumeVScroll, bool aForce);

  // Return true if ReflowScrolledFrame is going to do something different based
  // on the presence of a horizontal scrollbar in a horizontal writing mode or a
  // vertical scrollbar in a vertical writing mode.
  bool ScrolledContentDependsOnBSize(const ScrollReflowInput& aState) const;

  void ReflowScrolledFrame(ScrollReflowInput& aState, bool aAssumeHScroll,
                           bool aAssumeVScroll, ReflowOutput* aMetrics);
  void ReflowContents(ScrollReflowInput& aState,
                      const ReflowOutput& aDesiredSize);
  void PlaceScrollArea(ScrollReflowInput& aState,
                       const nsPoint& aScrollPosition);

  // Return the sum of inline-size of the scrollbar gutters (if any) at the
  // inline-start and inline-end edges of the scroll frame (for a potential
  // scrollbar that scrolls in the block axis).
  nscoord IntrinsicScrollbarGutterSizeAtInlineEdges();

  bool GetBorderRadii(const nsSize& aFrameSize, const nsSize& aBorderArea,
                      Sides aSkipSides, nscoord aRadii[8]) const final;

  nscoord GetMinISize(gfxContext* aRenderingContext) override;
  nscoord GetPrefISize(gfxContext* aRenderingContext) override;

  void Reflow(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
              const ReflowInput& aReflowInput,
              nsReflowStatus& aStatus) override;
  void DidReflow(nsPresContext* aPresContext,
                 const ReflowInput* aReflowInput) override;

  bool ComputeCustomOverflow(mozilla::OverflowAreas& aOverflowAreas) final;

  BaselineSharingGroup GetDefaultBaselineSharingGroup() const override;
  nscoord SynthesizeFallbackBaseline(
      mozilla::WritingMode aWM,
      BaselineSharingGroup aBaselineGroup) const override;
  Maybe<nscoord> GetNaturalBaselineBOffset(
      mozilla::WritingMode aWM, BaselineSharingGroup aBaselineGroup,
      BaselineExportContext aExportContext) const override;

  // Recomputes the scrollable overflow area we store in the helper to take
  // children that are affected by perpsective set on the outer frame and scroll
  // at different rates.
  void AdjustForPerspective(nsRect& aScrollableOverflow);

  // Called to set the child frames. We typically have three: the scroll area,
  // the vertical scrollbar, and the horizontal scrollbar.
  void SetInitialChildList(ChildListID aListID,
                           nsFrameList&& aChildList) override;
  void AppendFrames(ChildListID aListID, nsFrameList&& aFrameList) final;
  void InsertFrames(ChildListID aListID, nsIFrame* aPrevFrame,
                    const nsLineList::iterator* aPrevFrameLine,
                    nsFrameList&& aFrameList) final;
  void RemoveFrame(DestroyContext&, ChildListID, nsIFrame*) final;

  void DidSetComputedStyle(ComputedStyle* aOldComputedStyle) final;

  void Destroy(DestroyContext&) override;

  nsIScrollableFrame* GetScrollTargetFrame() const final {
    return const_cast<nsHTMLScrollFrame*>(this);
  }

  nsContainerFrame* GetContentInsertionFrame() override {
    return GetScrolledFrame()->GetContentInsertionFrame();
  }

  nsPoint GetPositionOfChildIgnoringScrolling(const nsIFrame* aChild) final {
    nsPoint pt = aChild->GetPosition();
    if (aChild == GetScrolledFrame()) {
      pt += GetScrollPosition();
    }
    return pt;
  }

  // nsIAnonymousContentCreator
  nsresult CreateAnonymousContent(nsTArray<ContentInfo>&) final;
  void AppendAnonymousContentTo(nsTArray<nsIContent*>&, uint32_t aFilter) final;

  // nsIScrollableFrame
  nsIFrame* GetScrolledFrame() const final { return mScrolledFrame; }
  mozilla::ScrollStyles GetScrollStyles() const final;
  bool IsForTextControlWithNoScrollbars() const final;
  bool HasAllNeededScrollbars() const final {
    return GetCurrentAnonymousContent().contains(GetNeededAnonymousContent());
  }

  mozilla::layers::OverscrollBehaviorInfo GetOverscrollBehaviorInfo()
      const final;
  ScrollDirections GetAvailableScrollingDirectionsForUserInputEvents()
      const final;
  ScrollDirections GetScrollbarVisibility() const final {
    ScrollDirections result;
    if (mHasHorizontalScrollbar) {
      result += ScrollDirection::eHorizontal;
    }
    if (mHasVerticalScrollbar) {
      result += ScrollDirection::eVertical;
    }
    return result;
  }
  nsMargin GetActualScrollbarSizes(
      nsIScrollableFrame::ScrollbarSizesOptions aOptions =
          nsIScrollableFrame::ScrollbarSizesOptions::NONE) const final;
  nsMargin GetDesiredScrollbarSizes() const final;
  static nscoord GetNonOverlayScrollbarSize(const nsPresContext*,
                                            mozilla::StyleScrollbarWidth);
  nsSize GetLayoutSize() const final {
    if (mIsUsingMinimumScaleSize) {
      return mICBSize;
    }
    return mScrollPort.Size();
  }
  nsRect GetScrolledRect() const final;
  nsRect GetScrollPortRect() const final { return mScrollPort; }
  nsPoint GetScrollPosition() const final {
    return mScrollPort.TopLeft() - mScrolledFrame->GetPosition();
  }
  nsPoint GetLogicalScrollPosition() const final {
    nsPoint pt;
    pt.x = IsPhysicalLTR()
               ? mScrollPort.x - mScrolledFrame->GetPosition().x
               : mScrollPort.XMost() - mScrolledFrame->GetRect().XMost();
    pt.y = mScrollPort.y - mScrolledFrame->GetPosition().y;
    return pt;
  }

  nsRect GetScrollRange() const final { return GetLayoutScrollRange(); }
  nsSize GetVisualViewportSize() const final;
  nsPoint GetVisualViewportOffset() const final;
  bool SetVisualViewportOffset(const nsPoint& aOffset, bool aRepaint) final;
  nsRect GetVisualScrollRange() const final;
  nsRect GetScrollRangeForUserInputEvents() const final;
  nsSize GetLineScrollAmount() const final;
  nsSize GetPageScrollAmount() const final;
  nsMargin GetScrollPadding() const final;
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void ScrollTo(nsPoint aScrollPosition, ScrollMode aMode,
                const nsRect* aRange = nullptr,
                ScrollSnapFlags aSnapFlags = ScrollSnapFlags::Disabled,
                ScrollTriggeredByScript aTriggeredByScript =
                    ScrollTriggeredByScript::No) final {
    return ScrollToInternal(aScrollPosition, aMode, ScrollOrigin::Other, aRange,
                            aSnapFlags, aTriggeredByScript);
  }
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void ScrollToCSSPixels(const CSSIntPoint& aScrollPosition,
                         ScrollMode aMode = ScrollMode::Instant) final;
  void ScrollToCSSPixelsForApz(const mozilla::CSSPoint& aScrollPosition,
                               ScrollSnapTargetIds&& aLastSnapTargetIds) final;
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  CSSIntPoint GetRoundedScrollPositionCSSPixels() final;

  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void ScrollBy(nsIntPoint aDelta, mozilla::ScrollUnit aUnit, ScrollMode aMode,
                nsIntPoint* aOverflow,
                ScrollOrigin aOrigin = ScrollOrigin::NotSpecified,
                nsIScrollableFrame::ScrollMomentum aMomentum =
                    nsIScrollableFrame::NOT_MOMENTUM,
                ScrollSnapFlags aSnapFlags = ScrollSnapFlags::Disabled) final;
  void ScrollByCSSPixels(const CSSIntPoint& aDelta,
                         ScrollMode aMode = ScrollMode::Instant) final {
    return ScrollByCSSPixelsInternal(aDelta, aMode);
  }
  void ScrollByCSSPixelsInternal(
      const CSSIntPoint& aDelta, ScrollMode aMode = ScrollMode::Instant,
      // This ScrollByCSSPixels is mainly used for Element.scrollBy and
      // Window.scrollBy. An exception is the transmogrification of
      // ScrollToCSSPixels.
      ScrollSnapFlags aSnapFlags = ScrollSnapFlags::IntendedDirection |
                                   ScrollSnapFlags::IntendedEndPosition);

  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void ScrollToRestoredPosition() final;
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void CurPosAttributeChanged(nsIContent* aChild) final {
    return CurPosAttributeChangedInternal(aChild);
  }
  NS_IMETHOD PostScrolledAreaEventForCurrentArea() final {
    PostScrolledAreaEvent();
    return NS_OK;
  }
  bool IsScrollingActive() const final;
  bool IsMaybeAsynchronouslyScrolled() const final {
    // If this is true, then we'll build an ASR, and that's what we want
    // to know I think.
    return mWillBuildScrollableLayer;
  }

  bool DidHistoryRestore() const final { return mDidHistoryRestore; }
  void ClearDidHistoryRestore() final { mDidHistoryRestore = false; }
  void MarkEverScrolled() final;
  bool IsRectNearlyVisible(const nsRect& aRect) const final;
  nsRect ExpandRectToNearlyVisible(const nsRect& aRect) const final;
  ScrollOrigin LastScrollOrigin() const final { return mLastScrollOrigin; }
  using AnimationState = nsIScrollableFrame::AnimationState;
  mozilla::EnumSet<AnimationState> ScrollAnimationState() const final;
  mozilla::MainThreadScrollGeneration CurrentScrollGeneration() const final {
    return mScrollGeneration;
  }
  mozilla::APZScrollGeneration ScrollGenerationOnApz() const final {
    return mScrollGenerationOnApz;
  }
  nsPoint LastScrollDestination() final { return mDestination; }
  nsTArray<mozilla::ScrollPositionUpdate> GetScrollUpdates() const final;
  bool HasScrollUpdates() const final { return !mScrollUpdates.IsEmpty(); }
  void ResetScrollInfoIfNeeded(
      const mozilla::MainThreadScrollGeneration& aGeneration,
      const mozilla::APZScrollGeneration& aGenerationOnApz,
      mozilla::APZScrollAnimationType aAPZScrollAnimationType,
      InScrollingGesture aInScrollingGesture) final;
  bool WantAsyncScroll() const final;
  mozilla::Maybe<mozilla::layers::ScrollMetadata> ComputeScrollMetadata(
      mozilla::layers::WebRenderLayerManager* aLayerManager,
      const nsIFrame* aItemFrame,
      const nsPoint& aOffsetToReferenceFrame) const final;
  void MarkScrollbarsDirtyForReflow() const final;
  void InvalidateScrollbars() const final;

  /**
   * @note This method might destroy the frame, pres shell and other objects.
   * Update scrollbar curpos attributes to reflect current scroll position
   */
  void UpdateScrollbarPosition() final;
  bool DecideScrollableLayer(nsDisplayListBuilder* aBuilder,
                             nsRect* aVisibleRect, nsRect* aDirtyRect,
                             bool aSetBase) final {
    return DecideScrollableLayer(aBuilder, aVisibleRect, aDirtyRect, aSetBase,
                                 nullptr);
  }
  void NotifyApzTransaction() final;
  void NotifyApproximateFrameVisibilityUpdate(bool aIgnoreDisplayPort) final;
  bool GetDisplayPortAtLastApproximateFrameVisibilityUpdate(
      nsRect* aDisplayPort) final;
  void TriggerDisplayPortExpiration() final;

  // nsIStatefulFrame
  mozilla::UniquePtr<mozilla::PresState> SaveState() final;
  NS_IMETHOD RestoreState(mozilla::PresState* aState) final;

  // nsIScrollbarMediator
  void ScrollByPage(
      nsScrollbarFrame* aScrollbar, int32_t aDirection,
      ScrollSnapFlags aSnapFlags = ScrollSnapFlags::Disabled) final;
  void ScrollByWhole(
      nsScrollbarFrame* aScrollbar, int32_t aDirection,
      ScrollSnapFlags aSnapFlags = ScrollSnapFlags::Disabled) final;
  void ScrollByLine(nsScrollbarFrame* aScrollbar, int32_t aDirection,
                    ScrollSnapFlags = ScrollSnapFlags::Disabled) final;
  void ScrollByUnit(nsScrollbarFrame* aScrollbar, ScrollMode aMode,
                    int32_t aDirection, mozilla::ScrollUnit aUnit,
                    ScrollSnapFlags = ScrollSnapFlags::Disabled) final;
  void RepeatButtonScroll(nsScrollbarFrame* aScrollbar) final;
  void ThumbMoved(nsScrollbarFrame* aScrollbar, nscoord aOldPos,
                  nscoord aNewPos) final;
  void ScrollbarReleased(nsScrollbarFrame* aScrollbar) final;
  void VisibilityChanged(bool aVisible) final {}
  nsScrollbarFrame* GetScrollbarBox(bool aVertical) final {
    return aVertical ? mVScrollbarBox : mHScrollbarBox;
  }

  void ScrollbarActivityStarted() const final;
  void ScrollbarActivityStopped() const final;

  bool IsScrollbarOnRight() const final;

  bool ShouldSuppressScrollbarRepaints() const final {
    return mSuppressScrollbarRepaints;
  }
  void SetTransformingByAPZ(bool aTransforming) final;
  bool IsTransformingByAPZ() const final { return mTransformingByAPZ; }
  void SetScrollableByAPZ(bool aScrollable) final;
  void SetZoomableByAPZ(bool aZoomable) final;
  void SetHasOutOfFlowContentInsideFilter() final;
  ScrollSnapInfo GetScrollSnapInfo() final;

  void TryResnap() final;
  void PostPendingResnapIfNeeded(const nsIFrame* aFrame) final;
  void PostPendingResnap() final;
  using PhysicalScrollSnapAlign = nsIScrollableFrame::PhysicalScrollSnapAlign;
  PhysicalScrollSnapAlign GetScrollSnapAlignFor(
      const nsIFrame* aFrame) const final;

  bool DragScroll(mozilla::WidgetEvent* aEvent) final;

  void AsyncScrollbarDragInitiated(
      uint64_t aDragBlockId, mozilla::layers::ScrollDirection aDirection) final;

  void AsyncScrollbarDragRejected() final;

  bool IsRootScrollFrameOfDocument() const final { return mIsRoot; }

  Maybe<uint32_t> IsFirstScrollableFrameSequenceNumber() const final {
    return mIsFirstScrollableFrameSequenceNumber;
  }

  void SetIsFirstScrollableFrameSequenceNumber(Maybe<uint32_t> aValue) final {
    mIsFirstScrollableFrameSequenceNumber = aValue;
  }

  const ScrollAnchorContainer* Anchor() const final { return &mAnchor; }
  ScrollAnchorContainer* Anchor() final { return &mAnchor; }

  // Return the scrolled frame.
  void AppendDirectlyOwnedAnonBoxes(nsTArray<OwnedAnonBox>& aResult) final {
    aResult.AppendElement(OwnedAnonBox(GetScrolledFrame()));
  }

  bool SmoothScrollVisual(
      const nsPoint& aVisualViewportOffset,
      mozilla::layers::FrameMetrics::ScrollOffsetUpdateType aUpdateType) final;

  bool IsSmoothScroll(mozilla::dom::ScrollBehavior aBehavior) const final;

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
  ~nsHTMLScrollFrame();
  void SetSuppressScrollbarUpdate(bool aSuppress) {
    mSuppressScrollbarUpdate = aSuppress;
  }
  bool GuessHScrollbarNeeded(const ScrollReflowInput& aState);
  bool GuessVScrollbarNeeded(const ScrollReflowInput& aState);

  bool IsScrollbarUpdateSuppressed() const { return mSuppressScrollbarUpdate; }

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
  class AsyncScroll;
  class AsyncSmoothMSDScroll;

  enum class AnonymousContentType {
    VerticalScrollbar,
    HorizontalScrollbar,
    Resizer,
  };
  mozilla::EnumSet<AnonymousContentType> GetNeededAnonymousContent() const;
  mozilla::EnumSet<AnonymousContentType> GetCurrentAnonymousContent() const;

  // If a child frame was added or removed on the scrollframe,
  // reload our child frame list.
  // We need this if a scrollbar frame is recreated.
  void ReloadChildFrames();

 public:
  enum class OverflowState : uint32_t {
    None = 0,
    Vertical = (1 << 0),
    Horizontal = (1 << 1),
  };

 protected:
  OverflowState GetOverflowState() const;

  MOZ_CAN_RUN_SCRIPT nsresult FireScrollPortEvent();
  void PostScrollEndEvent();
  MOZ_CAN_RUN_SCRIPT void FireScrollEndEvent();
  void PostOverflowEvent();

  // Add display items for the top-layer (which includes things like
  // the fullscreen element, its backdrop, and text selection carets)
  // to |aLists|.
  // This is a no-op for scroll frames other than the viewport's
  // root scroll frame.
  // This should be called with an nsDisplayListSet that will be
  // wrapped in the async zoom container, if we're building one.
  // It should not be called with an ASR setter on the stack, as the
  // top-layer items handle setting up their own ASRs.
  void MaybeCreateTopLayerAndWrapRootItems(
      nsDisplayListBuilder*, mozilla::nsDisplayListCollection&,
      bool aCreateAsyncZoom,
      AutoContainsBlendModeCapturer* aAsyncZoomBlendCapture,
      const nsRect& aAsyncZoomClipRect, nscoord* aRadii);

  void AppendScrollPartsTo(nsDisplayListBuilder* aBuilder,
                           const nsDisplayListSet& aLists, bool aCreateLayer,
                           bool aPositioned);

  // nsIReflowCallback
  bool ReflowFinished() final;
  void ReflowCallbackCanceled() final;

  /**
   * @note This method might destroy the frame, pres shell and other objects.
   * Called when the 'curpos' attribute on one of the scrollbars changes.
   */
  void CurPosAttributeChangedInternal(nsIContent*, bool aDoScroll = true);

  void PostScrollEvent(bool aDelayed = false);
  MOZ_CAN_RUN_SCRIPT void FireScrollEvent();
  void PostScrolledAreaEvent();
  MOZ_CAN_RUN_SCRIPT void FireScrolledAreaEvent();

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

  nsRect GetLayoutScrollRange() const;
  // Get the scroll range assuming the viewport has size (aWidth, aHeight).
  nsRect GetScrollRange(nscoord aWidth, nscoord aHeight) const;

  const nsRect& ScrollPort() const { return mScrollPort; }
  void SetScrollPort(const nsRect& aNewScrollPort) {
    if (!mScrollPort.IsEqualEdges(aNewScrollPort)) {
      mMayScheduleScrollAnimations = true;
    }
    mScrollPort = aNewScrollPort;
  }

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
  void ScrollSnap() final { return ScrollSnap(ScrollMode::SmoothMsd); }
  void ScrollSnap(ScrollMode aMode);
  void ScrollSnap(const nsPoint& aDestination,
                  ScrollMode aMode = ScrollMode::SmoothMsd);

  bool HasPendingScrollRestoration() const {
    return mRestorePos != nsPoint(-1, -1);
  }

  bool IsProcessingScrollEvent() const { return mProcessingScrollEvent; }

 public:
  static void AsyncScrollCallback(nsHTMLScrollFrame* aInstance,
                                  mozilla::TimeStamp aTime);
  static void AsyncSmoothMSDScrollCallback(nsHTMLScrollFrame* aInstance,
                                           mozilla::TimeDuration aDeltaTime);
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   * aRange is the range of allowable scroll positions around the desired
   * aScrollPosition. Null means only aScrollPosition is allowed.
   * This is a closed-ended range --- aRange.XMost()/aRange.YMost() are allowed.
   */
  void ScrollToInternal(
      nsPoint aScrollPosition, ScrollMode aMode,
      ScrollOrigin aOrigin = ScrollOrigin::NotSpecified,
      const nsRect* aRange = nullptr,
      ScrollSnapFlags aSnapFlags = ScrollSnapFlags::Disabled,
      ScrollTriggeredByScript aTriggeredByScript = ScrollTriggeredByScript::No);
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void ScrollToImpl(
      nsPoint aPt, const nsRect& aRange,
      ScrollOrigin aOrigin = ScrollOrigin::NotSpecified,
      ScrollTriggeredByScript aTriggeredByScript = ScrollTriggeredByScript::No);
  void ScrollVisual();

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
  Maybe<mozilla::SnapDestination> GetSnapPointForDestination(
      mozilla::ScrollUnit aUnit, ScrollSnapFlags aFlags,
      const nsPoint& aStartPos, const nsPoint& aDestination);

  Maybe<mozilla::SnapDestination> GetSnapPointForResnap();
  bool NeedsResnap();

  void SetLastSnapTargetIds(mozilla::UniquePtr<ScrollSnapTargetIds> aId);
  void AddScrollPositionListener(nsIScrollPositionListener* aListener) final {
    mListeners.AppendElement(aListener);
  }
  void RemoveScrollPositionListener(
      nsIScrollPositionListener* aListener) final {
    mListeners.RemoveElement(aListener);
  }

  static void SetScrollbarVisibility(nsIFrame* aScrollbar, bool aVisible);

  /**
   * GetUnsnappedScrolledRectInternal is designed to encapsulate deciding which
   * directions of overflow should be reachable by scrolling and which
   * should not.  Callers should NOT depend on it having any particular
   * behavior.
   *
   * Currently it allows scrolling down and to the right for
   * nsHTMLScrollFrames with LTR directionality, and allows scrolling down and
   * to the left for nsHTMLScrollFrames with RTL directionality.
   */
  nsRect GetUnsnappedScrolledRectInternal(const nsRect& aScrolledOverflowArea,
                                          const nsSize& aScrollPortSize) const;

  bool IsPhysicalLTR() const { return GetWritingMode().IsPhysicalLTR(); }
  bool IsBidiLTR() const { return GetWritingMode().IsBidiLTR(); }

 private:
  // NOTE: Use GetScrollStylesFromFrame() if you want to know `overflow`
  // and `overflow-behavior` properties.
  nsIFrame* GetFrameForStyle() const;

  // Compute all scroll snap related information and store eash snap target
  // element in |mSnapTargets|.
  ScrollSnapInfo ComputeScrollSnapInfo();

  bool NeedsScrollSnap() const;

  // Returns the snapport size of this scroll container.
  // https://drafts.csswg.org/css-scroll-snap/#scroll-snapport
  nsSize GetSnapportSize() const;

  // Schedule the scroll-driven animations.
  void ScheduleScrollAnimations();
  void TryScheduleScrollAnimations() {
    if (!mMayScheduleScrollAnimations) {
      return;
    }
    ScheduleScrollAnimations();
    mMayScheduleScrollAnimations = false;
  }

  CSSPoint GetScrollPositionCSSPixels() const {
    return CSSPoint::FromAppUnits(GetScrollPosition());
  }

 public:
  void UpdateSticky();

  void UpdatePrevScrolledRect();

  // adjust the scrollbar rectangle aRect to account for any visible resizer.
  // aHasResizer specifies if there is a content resizer, however this method
  // will also check if a widget resizer is present as well.
  void AdjustScrollbarRectForResizer(
      nsIFrame* aFrame, nsPresContext* aPresContext, nsRect& aRect,
      bool aHasResizer, mozilla::layers::ScrollDirection aDirection);
  void LayoutScrollbars(ScrollReflowInput& aState,
                        const nsRect& aInsideBorderArea,
                        const nsRect& aOldScrollPort);

  void LayoutScrollbarPartAtRect(const ScrollReflowInput&,
                                 ReflowInput& aKidReflowInput, const nsRect&);

  bool IsAlwaysActive() const;
  void MarkRecentlyScrolled();
  void MarkNotRecentlyScrolled();
  nsExpirationState* GetExpirationState() { return &mActivityExpirationState; }

  bool UsesOverlayScrollbars() const;
  bool IsLastSnappedTarget(const nsIFrame* aFrame) const;

  static bool ShouldActivateAllScrollFrames();
  nsRect RestrictToRootDisplayPort(const nsRect& aDisplayportBase);
  bool DecideScrollableLayer(nsDisplayListBuilder* aBuilder,
                             nsRect* aVisibleRect, nsRect* aDirtyRect,
                             bool aSetBase,
                             bool* aDirtyRectHasBeenOverriden = nullptr);
  bool AllowDisplayPortExpiration();
  void ResetDisplayPortExpiryTimer();

  void ScheduleSyntheticMouseMove();
  static void ScrollActivityCallback(nsITimer* aTimer, void* anInstance);

  void HandleScrollbarStyleSwitching();

  bool IsApzAnimationInProgress() const {
    return mCurrentAPZScrollAnimationType != APZScrollAnimationType::No;
  }
  nsPoint LastScrollDestination() const { return mDestination; }

  bool IsLastScrollUpdateAnimating() const;
  bool IsLastScrollUpdateTriggeredByScriptAnimating() const;

  // Update minimum-scale size.  The minimum-scale size will be set/used only
  // if there is overflow-x:hidden region.
  void UpdateMinimumScaleSize(const nsRect& aScrollableOverflow,
                              const nsSize& aICBSize);

  // Return the scroll frame's "true outer size".
  // This is GetSize(), except when we've been sized to reflect a virtual
  // (layout) viewport in which case this returns the outer size used to size
  // the physical (visual) viewport.
  nsSize TrueOuterSize(nsDisplayListBuilder* aBuilder) const;

  already_AddRefed<Element> MakeScrollbar(mozilla::dom::NodeInfo* aNodeInfo,
                                          bool aVertical,
                                          mozilla::AnonymousContentKey& aKey);

  void AppendScrollUpdate(const mozilla::ScrollPositionUpdate& aUpdate);

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
  nsScrollbarFrame* mHScrollbarBox;
  nsScrollbarFrame* mVScrollbarBox;
  nsIFrame* mScrolledFrame;
  nsIFrame* mScrollCornerBox;
  nsIFrame* mResizerBox;
  const nsIFrame* mReferenceFrameDuringPainting;
  RefPtr<AsyncScroll> mAsyncScroll;
  RefPtr<AsyncSmoothMSDScroll> mAsyncSmoothMSDScroll;
  RefPtr<ScrollbarActivity> mScrollbarActivity;
  nsTArray<nsIScrollPositionListener*> mListeners;
  ScrollOrigin mLastScrollOrigin;
  Maybe<nsPoint> mApzSmoothScrollDestination;
  mozilla::MainThreadScrollGeneration mScrollGeneration;
  mozilla::APZScrollGeneration mScrollGenerationOnApz;

  nsTArray<mozilla::ScrollPositionUpdate> mScrollUpdates;

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

  // The scroll position where we last updated frame visibility.
  nsPoint mLastUpdateFramesPos;
  nsRect mDisplayPortAtLastFrameUpdate;

  nsRect mPrevScrolledRect;

  ScrollableLayerGuid::ViewID mScrollParentID;

  // Timer to remove the displayport some time after scrolling has stopped
  nsCOMPtr<nsITimer> mDisplayPortExpiryTimer;

  ScrollAnchorContainer mAnchor;

  // We keep holding a strong reference for each snap target element until the
  // next snapping happens so that it avoids using the same nsIContent* pointer
  // for newly created contents in this scroll container. Otherwise we will try
  // to match different nsIContent(s) generated at the same address during
  // re-snapping.
  SnapTargetSet mSnapTargets;

  // Representing there's an APZ animation is in progress and what caused the
  // animation. Note that this is only set when repainted via APZ, which means
  // that there may be a request for an APZ animation in flight for example,
  // while this is still `No`. In order to answer "is an APZ animation in the
  // process of starting or in progress" you need to check mScrollUpdates,
  // mApzAnimationRequested, and this type.
  APZScrollAnimationType mCurrentAPZScrollAnimationType;

  // The paint sequence number if the scroll frame is the first scrollable frame
  // encountered.
  Maybe<uint32_t> mIsFirstScrollableFrameSequenceNumber;

  // Representing whether the APZC corresponding to this frame is now in the
  // middle of handling a gesture (e.g. a pan gesture).
  InScrollingGesture mInScrollingGesture : 1;

  bool mAllowScrollOriginDowngrade : 1;
  bool mHadDisplayPortAtLastFrameUpdate : 1;

  // True if the most recent reflow of the scroll container frame has
  // the vertical scrollbar shown.
  bool mHasVerticalScrollbar : 1;
  // True if the most recent reflow of the scroll container frame has the
  // horizontal scrollbar shown.
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

  // This is true from the time a scroll animation is requested of APZ to the
  // time that APZ responds with an up-to-date repaint request. More precisely,
  // this is flipped to true if a repaint request is dispatched to APZ where
  // the most recent scroll request is a smooth scroll, and it is cleared when
  // mApzAnimationInProgress is updated.
  bool mApzAnimationRequested : 1;

  // Similar to above mApzAnimationRequested but the request came from script,
  // e.g., scrollBy().
  bool mApzAnimationTriggeredByScriptRequested : 1;

  // Whether we need to reclamp the visual viewport offset in ReflowFinished.
  bool mReclampVVOffsetInReflowFinished : 1;

  // Whether we need to schedule the scroll-driven animations.
  bool mMayScheduleScrollAnimations : 1;

#ifdef MOZ_WIDGET_ANDROID
  // True if this scrollable frame was vertically overflowed on the last reflow.
  bool mHasVerticalOverflowForDynamicToolbar : 1;
#endif

  mozilla::layout::ScrollVelocityQueue mVelocityQueue;

 protected:
  class AutoScrollbarRepaintSuppression;
  friend class AutoScrollbarRepaintSuppression;
  class AutoScrollbarRepaintSuppression {
   public:
    AutoScrollbarRepaintSuppression(nsHTMLScrollFrame* aFrame,
                                    AutoWeakFrame& aWeakOuter, bool aSuppress)
        : mFrame(aFrame),
          mWeakOuter(aWeakOuter),
          mOldSuppressValue(aFrame->mSuppressScrollbarRepaints) {
      mFrame->mSuppressScrollbarRepaints = aSuppress;
    }

    ~AutoScrollbarRepaintSuppression() {
      if (mWeakOuter.IsAlive()) {
        mFrame->mSuppressScrollbarRepaints = mOldSuppressValue;
      }
    }

   private:
    nsHTMLScrollFrame* mFrame;
    AutoWeakFrame& mWeakOuter;
    bool mOldSuppressValue;
  };

  struct ScrollOperationParams {
    ScrollOperationParams(const ScrollOperationParams&) = delete;
    ScrollOperationParams(ScrollMode aMode, ScrollOrigin aOrigin)
        : mMode(aMode), mOrigin(aOrigin) {}
    ScrollOperationParams(ScrollMode aMode, ScrollOrigin aOrigin,
                          ScrollSnapTargetIds&& aSnapTargetIds)
        : ScrollOperationParams(aMode, aOrigin) {
      mTargetIds = std::move(aSnapTargetIds);
    }
    ScrollOperationParams(ScrollMode aMode, ScrollOrigin aOrigin,
                          ScrollSnapFlags aSnapFlags,
                          ScrollTriggeredByScript aTriggeredByScript)
        : ScrollOperationParams(aMode, aOrigin) {
      mSnapFlags = aSnapFlags;
      mTriggeredByScript = aTriggeredByScript;
    }

    ScrollMode mMode;
    ScrollOrigin mOrigin;
    ScrollSnapFlags mSnapFlags = ScrollSnapFlags::Disabled;
    ScrollTriggeredByScript mTriggeredByScript = ScrollTriggeredByScript::No;
    ScrollSnapTargetIds mTargetIds;

    bool IsInstant() const { return mMode == ScrollMode::Instant; }
    bool IsSmoothMsd() const { return mMode == ScrollMode::SmoothMsd; }
    bool IsSmooth() const { return mMode == ScrollMode::Smooth; }
    bool IsScrollSnapDisabled() const {
      return mSnapFlags == ScrollSnapFlags::Disabled;
    }
  };

  /**
   * @note This method might destroy the frame, pres shell and other objects.
   *
   * A caller can ask this ScrollToWithOrigin() function to perform snapping by
   * passing in aParams.mSnapFlags != ScrollSnapFlags::Disabled. Alternatively,
   * a caller may want to do its own snapping, in which case it should pass
   * ScrollSnapFlags::Disabled and populate aParams.mTargetIds based on the
   * result of the snapping.
   */
  void ScrollToWithOrigin(nsPoint aScrollPosition, const nsRect* aRange,
                          ScrollOperationParams&& aParams);

  void CompleteAsyncScroll(
      const nsPoint& aScrollPosition, const nsRect& aRange,
      mozilla::UniquePtr<ScrollSnapTargetIds> aSnapTargetIds,
      ScrollOrigin aOrigin = ScrollOrigin::NotSpecified);

  bool HasPerspective() const { return ChildrenHavePerspective(); }
  bool HasBgAttachmentLocal() const;
  mozilla::StyleDirection GetScrolledFrameDir() const;

  // Ask APZ to smooth scroll to |aDestination|.
  // This method does not clamp the destination; callers should clamp it to
  // either the layout or the visual scroll range (APZ will happily smooth
  // scroll to either).
  void ApzSmoothScrollTo(
      const nsPoint& aDestination, ScrollMode, ScrollOrigin,
      mozilla::ScrollTriggeredByScript,
      mozilla::UniquePtr<ScrollSnapTargetIds> aSnapTargetIds);

  // Check whether APZ can scroll in the provided directions, keeping in mind
  // that APZ currently cannot scroll along axes which are overflow:hidden.
  bool CanApzScrollInTheseDirections(
      mozilla::layers::ScrollDirections aDirections);

  // Removes any RefreshDriver observers we might have registered.
  void RemoveObservers();

 private:
  // NOTE: On mobile this value might be factoring into overflow:hidden region
  // in the case of the top level document.
  nsRect mScrollPort;
  mozilla::UniquePtr<ScrollSnapTargetIds> mLastSnapTargetIds;
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(nsHTMLScrollFrame::OverflowState)

#endif /* nsGfxScrollFrame_h___ */
