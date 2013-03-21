/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object to wrap rendering objects that should be scrollable */

#include "nsCOMPtr.h"
#include "nsHTMLParts.h"
#include "nsPresContext.h"
#include "nsIServiceManager.h"
#include "nsView.h"
#include "nsIScrollable.h"
#include "nsViewManager.h"
#include "nsContainerFrame.h"
#include "nsGfxScrollFrame.h"
#include "nsGkAtoms.h"
#include "nsINameSpaceManager.h"
#include "nsContentList.h"
#include "nsIDocumentInlines.h"
#include "nsFontMetrics.h"
#include "nsIDocumentObserver.h"
#include "nsBoxLayoutState.h"
#include "nsINodeInfo.h"
#include "nsScrollbarFrame.h"
#include "nsIScrollbarMediator.h"
#include "nsITextControlFrame.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsNodeInfoManager.h"
#include "nsIURI.h"
#include "nsGUIEvent.h"
#include "nsContentCreatorFunctions.h"
#include "nsISupportsPrimitives.h"
#include "nsAutoPtr.h"
#include "nsPresState.h"
#include "nsDocShellCID.h"
#include "nsIHTMLDocument.h"
#include "nsEventDispatcher.h"
#include "nsContentUtils.h"
#include "nsLayoutUtils.h"
#include "nsBidiUtils.h"
#include "nsFrameManager.h"
#include "mozilla/Preferences.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/dom/Element.h"
#include "mozilla/StandardInteger.h"
#include "mozilla/Util.h"
#include "FrameLayerBuilder.h"
#include "nsSMILKeySpline.h"
#include "nsSubDocumentFrame.h"
#include "nsSVGOuterSVGFrame.h"
#include "mozilla/Attributes.h"
#include "ScrollbarActivity.h"
#include "nsRefreshDriver.h"
#include "nsContentList.h"
#include <algorithm>
#include <cstdlib> // for std::abs(int/long)
#include <cmath> // for std::abs(float/double)

using namespace mozilla;
using namespace mozilla::dom;

//----------------------------------------------------------------------

//----------nsHTMLScrollFrame-------------------------------------------

nsIFrame*
NS_NewHTMLScrollFrame(nsIPresShell* aPresShell, nsStyleContext* aContext, bool aIsRoot)
{
  return new (aPresShell) nsHTMLScrollFrame(aPresShell, aContext, aIsRoot);
}

NS_IMPL_FRAMEARENA_HELPERS(nsHTMLScrollFrame)

nsHTMLScrollFrame::nsHTMLScrollFrame(nsIPresShell* aShell, nsStyleContext* aContext, bool aIsRoot)
  : nsContainerFrame(aContext),
    mInner(this, aIsRoot)
{
}

nsresult
nsHTMLScrollFrame::CreateAnonymousContent(nsTArray<ContentInfo>& aElements)
{
  return mInner.CreateAnonymousContent(aElements);
}

void
nsHTMLScrollFrame::AppendAnonymousContentTo(nsBaseContentList& aElements,
                                            uint32_t aFilter)
{
  mInner.AppendAnonymousContentTo(aElements, aFilter);
}

void
nsHTMLScrollFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  DestroyAbsoluteFrames(aDestructRoot);
  mInner.Destroy();
  nsContainerFrame::DestroyFrom(aDestructRoot);
}

NS_IMETHODIMP
nsHTMLScrollFrame::SetInitialChildList(ChildListID  aListID,
                                       nsFrameList& aChildList)
{
  nsresult rv = nsContainerFrame::SetInitialChildList(aListID, aChildList);
  mInner.ReloadChildFrames();
  return rv;
}


NS_IMETHODIMP
nsHTMLScrollFrame::AppendFrames(ChildListID  aListID,
                                nsFrameList& aFrameList)
{
  NS_ASSERTION(aListID == kPrincipalList, "Only main list supported");
  mFrames.AppendFrames(nullptr, aFrameList);
  mInner.ReloadChildFrames();
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLScrollFrame::InsertFrames(ChildListID aListID,
                                nsIFrame* aPrevFrame,
                                nsFrameList& aFrameList)
{
  NS_ASSERTION(aListID == kPrincipalList, "Only main list supported");
  NS_ASSERTION(!aPrevFrame || aPrevFrame->GetParent() == this,
               "inserting after sibling frame with different parent");
  mFrames.InsertFrames(nullptr, aPrevFrame, aFrameList);
  mInner.ReloadChildFrames();
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLScrollFrame::RemoveFrame(ChildListID aListID,
                               nsIFrame* aOldFrame)
{
  NS_ASSERTION(aListID == kPrincipalList, "Only main list supported");
  mFrames.DestroyFrame(aOldFrame);
  mInner.ReloadChildFrames();
  return NS_OK;
}

nsSplittableType
nsHTMLScrollFrame::GetSplittableType() const
{
  return NS_FRAME_NOT_SPLITTABLE;
}

nsIAtom*
nsHTMLScrollFrame::GetType() const
{
  return nsGkAtoms::scrollFrame;
}

/**
 HTML scrolling implementation

 All other things being equal, we prefer layouts with fewer scrollbars showing.
*/

struct ScrollReflowState {
  const nsHTMLReflowState& mReflowState;
  nsBoxLayoutState mBoxState;
  nsGfxScrollFrameInner::ScrollbarStyles mStyles;
  nsMargin mComputedBorder;

  // === Filled in by ReflowScrolledFrame ===
  nsOverflowAreas mContentsOverflowAreas;
  bool mReflowedContentsWithHScrollbar;
  bool mReflowedContentsWithVScrollbar;

  // === Filled in when TryLayout succeeds ===
  // The size of the inside-border area
  nsSize mInsideBorderSize;
  // Whether we decided to show the horizontal scrollbar
  bool mShowHScrollbar;
  // Whether we decided to show the vertical scrollbar
  bool mShowVScrollbar;

  ScrollReflowState(nsIScrollableFrame* aFrame,
                    const nsHTMLReflowState& aState) :
    mReflowState(aState),
    // mBoxState is just used for scrollbars so we don't need to
    // worry about the reflow depth here
    mBoxState(aState.frame->PresContext(), aState.rendContext, 0),
    mStyles(aFrame->GetScrollbarStyles()) {
  }
};

// XXXldb Can this go away?
static nsSize ComputeInsideBorderSize(ScrollReflowState* aState,
                                      const nsSize& aDesiredInsideBorderSize)
{
  // aDesiredInsideBorderSize is the frame size; i.e., it includes
  // borders and padding (but the scrolled child doesn't have
  // borders). The scrolled child has the same padding as us.
  nscoord contentWidth = aState->mReflowState.ComputedWidth();
  if (contentWidth == NS_UNCONSTRAINEDSIZE) {
    contentWidth = aDesiredInsideBorderSize.width -
      aState->mReflowState.mComputedPadding.LeftRight();
  }
  nscoord contentHeight = aState->mReflowState.ComputedHeight();
  if (contentHeight == NS_UNCONSTRAINEDSIZE) {
    contentHeight = aDesiredInsideBorderSize.height -
      aState->mReflowState.mComputedPadding.TopBottom();
  }

  contentWidth  = aState->mReflowState.ApplyMinMaxWidth(contentWidth);
  contentHeight = aState->mReflowState.ApplyMinMaxHeight(contentHeight);
  return nsSize(contentWidth + aState->mReflowState.mComputedPadding.LeftRight(),
                contentHeight + aState->mReflowState.mComputedPadding.TopBottom());
}

static void
GetScrollbarMetrics(nsBoxLayoutState& aState, nsIFrame* aBox, nsSize* aMin,
                    nsSize* aPref, bool aVertical)
{
  NS_ASSERTION(aState.GetRenderingContext(),
               "Must have rendering context in layout state for size "
               "computations");

  if (aMin) {
    *aMin = aBox->GetMinSize(aState);
    nsBox::AddMargin(aBox, *aMin);
  }

  if (aPref) {
    *aPref = aBox->GetPrefSize(aState);
    nsBox::AddMargin(aBox, *aPref);
  }
}

/**
 * Assuming that we know the metrics for our wrapped frame and
 * whether the horizontal and/or vertical scrollbars are present,
 * compute the resulting layout and return true if the layout is
 * consistent. If the layout is consistent then we fill in the
 * computed fields of the ScrollReflowState.
 *
 * The layout is consistent when both scrollbars are showing if and only
 * if they should be showing. A horizontal scrollbar should be showing if all
 * following conditions are met:
 * 1) the style is not HIDDEN
 * 2) our inside-border height is at least the scrollbar height (i.e., the
 * scrollbar fits vertically)
 * 3) our scrollport width (the inside-border width minus the width allocated for a
 * vertical scrollbar, if showing) is at least the scrollbar's min-width
 * (i.e., the scrollbar fits horizontally)
 * 4) the style is SCROLL, or the kid's overflow-area XMost is
 * greater than the scrollport width
 *
 * @param aForce if true, then we just assume the layout is consistent.
 */
bool
nsHTMLScrollFrame::TryLayout(ScrollReflowState* aState,
                             nsHTMLReflowMetrics* aKidMetrics,
                             bool aAssumeHScroll, bool aAssumeVScroll,
                             bool aForce, nsresult* aResult)
{
  *aResult = NS_OK;

  if ((aState->mStyles.mVertical == NS_STYLE_OVERFLOW_HIDDEN && aAssumeVScroll) ||
      (aState->mStyles.mHorizontal == NS_STYLE_OVERFLOW_HIDDEN && aAssumeHScroll)) {
    NS_ASSERTION(!aForce, "Shouldn't be forcing a hidden scrollbar to show!");
    return false;
  }

  if (aAssumeVScroll != aState->mReflowedContentsWithVScrollbar ||
      (aAssumeHScroll != aState->mReflowedContentsWithHScrollbar &&
       ScrolledContentDependsOnHeight(aState))) {
    nsresult rv = ReflowScrolledFrame(aState, aAssumeHScroll, aAssumeVScroll,
                                      aKidMetrics, false);
    if (NS_FAILED(rv)) {
      *aResult = rv;
      return false;
    }
  }

  nsSize vScrollbarMinSize(0, 0);
  nsSize vScrollbarPrefSize(0, 0);
  if (mInner.mVScrollbarBox) {
    GetScrollbarMetrics(aState->mBoxState, mInner.mVScrollbarBox,
                        &vScrollbarMinSize,
                        aAssumeVScroll ? &vScrollbarPrefSize : nullptr, true);
  }
  nscoord vScrollbarDesiredWidth = aAssumeVScroll ? vScrollbarPrefSize.width : 0;
  nscoord vScrollbarMinHeight = aAssumeVScroll ? vScrollbarMinSize.height : 0;

  nsSize hScrollbarMinSize(0, 0);
  nsSize hScrollbarPrefSize(0, 0);
  if (mInner.mHScrollbarBox) {
    GetScrollbarMetrics(aState->mBoxState, mInner.mHScrollbarBox,
                        &hScrollbarMinSize,
                        aAssumeHScroll ? &hScrollbarPrefSize : nullptr, false);
  }
  nscoord hScrollbarDesiredHeight = aAssumeHScroll ? hScrollbarPrefSize.height : 0;
  nscoord hScrollbarMinWidth = aAssumeHScroll ? hScrollbarMinSize.width : 0;

  // First, compute our inside-border size and scrollport size
  // XXXldb Can we depend more on ComputeSize here?
  nsSize desiredInsideBorderSize;
  desiredInsideBorderSize.width = vScrollbarDesiredWidth +
    std::max(aKidMetrics->width, hScrollbarMinWidth);
  desiredInsideBorderSize.height = hScrollbarDesiredHeight +
    std::max(aKidMetrics->height, vScrollbarMinHeight);
  aState->mInsideBorderSize =
    ComputeInsideBorderSize(aState, desiredInsideBorderSize);
  nsSize scrollPortSize = nsSize(std::max(0, aState->mInsideBorderSize.width - vScrollbarDesiredWidth),
                                 std::max(0, aState->mInsideBorderSize.height - hScrollbarDesiredHeight));

  if (!aForce) {
    nsRect scrolledRect =
      mInner.GetScrolledRectInternal(aState->mContentsOverflowAreas.ScrollableOverflow(),
                                     scrollPortSize);
    nscoord oneDevPixel = aState->mBoxState.PresContext()->DevPixelsToAppUnits(1);

    // If the style is HIDDEN then we already know that aAssumeHScroll is false
    if (aState->mStyles.mHorizontal != NS_STYLE_OVERFLOW_HIDDEN) {
      bool wantHScrollbar =
        aState->mStyles.mHorizontal == NS_STYLE_OVERFLOW_SCROLL ||
        scrolledRect.XMost() >= scrollPortSize.width + oneDevPixel ||
        scrolledRect.x <= -oneDevPixel;
      if (scrollPortSize.width < hScrollbarMinSize.width)
        wantHScrollbar = false;
      if (wantHScrollbar != aAssumeHScroll)
        return false;
    }

    // If the style is HIDDEN then we already know that aAssumeVScroll is false
    if (aState->mStyles.mVertical != NS_STYLE_OVERFLOW_HIDDEN) {
      bool wantVScrollbar =
        aState->mStyles.mVertical == NS_STYLE_OVERFLOW_SCROLL ||
        scrolledRect.YMost() >= scrollPortSize.height + oneDevPixel ||
        scrolledRect.y <= -oneDevPixel;
      if (scrollPortSize.height < vScrollbarMinSize.height)
        wantVScrollbar = false;
      if (wantVScrollbar != aAssumeVScroll)
        return false;
    }
  }

  nscoord vScrollbarActualWidth = aState->mInsideBorderSize.width - scrollPortSize.width;

  aState->mShowHScrollbar = aAssumeHScroll;
  aState->mShowVScrollbar = aAssumeVScroll;
  nsPoint scrollPortOrigin(aState->mComputedBorder.left,
                           aState->mComputedBorder.top);
  if (!mInner.IsScrollbarOnRight()) {
    scrollPortOrigin.x += vScrollbarActualWidth;
  }
  mInner.mScrollPort = nsRect(scrollPortOrigin, scrollPortSize);
  return true;
}

bool
nsHTMLScrollFrame::ScrolledContentDependsOnHeight(ScrollReflowState* aState)
{
  // Return true if ReflowScrolledFrame is going to do something different
  // based on the presence of a horizontal scrollbar.
  return (mInner.mScrolledFrame->GetStateBits() & NS_FRAME_CONTAINS_RELATIVE_HEIGHT) ||
    aState->mReflowState.ComputedHeight() != NS_UNCONSTRAINEDSIZE ||
    aState->mReflowState.mComputedMinHeight > 0 ||
    aState->mReflowState.mComputedMaxHeight != NS_UNCONSTRAINEDSIZE;
}

nsresult
nsHTMLScrollFrame::ReflowScrolledFrame(ScrollReflowState* aState,
                                       bool aAssumeHScroll,
                                       bool aAssumeVScroll,
                                       nsHTMLReflowMetrics* aMetrics,
                                       bool aFirstPass)
{
  // these could be NS_UNCONSTRAINEDSIZE ... std::min arithmetic should
  // be OK
  nscoord paddingLR = aState->mReflowState.mComputedPadding.LeftRight();

  nscoord availWidth = aState->mReflowState.ComputedWidth() + paddingLR;

  nscoord computedHeight = aState->mReflowState.ComputedHeight();
  nscoord computedMinHeight = aState->mReflowState.mComputedMinHeight;
  nscoord computedMaxHeight = aState->mReflowState.mComputedMaxHeight;
  if (!ShouldPropagateComputedHeightToScrolledContent()) {
    computedHeight = NS_UNCONSTRAINEDSIZE;
    computedMinHeight = 0;
    computedMaxHeight = NS_UNCONSTRAINEDSIZE;
  }
  if (aAssumeHScroll) {
    nsSize hScrollbarPrefSize;
    GetScrollbarMetrics(aState->mBoxState, mInner.mHScrollbarBox,
                        nullptr, &hScrollbarPrefSize, false);
    if (computedHeight != NS_UNCONSTRAINEDSIZE)
      computedHeight = std::max(0, computedHeight - hScrollbarPrefSize.height);
    computedMinHeight = std::max(0, computedMinHeight - hScrollbarPrefSize.height);
    if (computedMaxHeight != NS_UNCONSTRAINEDSIZE)
      computedMaxHeight = std::max(0, computedMaxHeight - hScrollbarPrefSize.height);
  }

  if (aAssumeVScroll) {
    nsSize vScrollbarPrefSize;
    GetScrollbarMetrics(aState->mBoxState, mInner.mVScrollbarBox,
                        nullptr, &vScrollbarPrefSize, true);
    availWidth = std::max(0, availWidth - vScrollbarPrefSize.width);
  }

  nsPresContext* presContext = PresContext();

  // Pass false for aInit so we can pass in the correct padding.
  nsHTMLReflowState kidReflowState(presContext, aState->mReflowState,
                                   mInner.mScrolledFrame,
                                   nsSize(availWidth, NS_UNCONSTRAINEDSIZE),
                                   -1, -1, false);
  kidReflowState.Init(presContext, -1, -1, nullptr,
                      &aState->mReflowState.mComputedPadding);
  kidReflowState.mFlags.mAssumingHScrollbar = aAssumeHScroll;
  kidReflowState.mFlags.mAssumingVScrollbar = aAssumeVScroll;
  kidReflowState.SetComputedHeight(computedHeight);
  kidReflowState.mComputedMinHeight = computedMinHeight;
  kidReflowState.mComputedMaxHeight = computedMaxHeight;

  // Temporarily set mHasHorizontalScrollbar/mHasVerticalScrollbar to
  // reflect our assumptions while we reflow the child.
  bool didHaveHorizontalScrollbar = mInner.mHasHorizontalScrollbar;
  bool didHaveVerticalScrollbar = mInner.mHasVerticalScrollbar;
  mInner.mHasHorizontalScrollbar = aAssumeHScroll;
  mInner.mHasVerticalScrollbar = aAssumeVScroll;

  nsReflowStatus status;
  nsresult rv = ReflowChild(mInner.mScrolledFrame, presContext, *aMetrics,
                            kidReflowState, 0, 0,
                            NS_FRAME_NO_MOVE_FRAME, status);

  mInner.mHasHorizontalScrollbar = didHaveHorizontalScrollbar;
  mInner.mHasVerticalScrollbar = didHaveVerticalScrollbar;

  // Don't resize or position the view (if any) because we're going to resize
  // it to the correct size anyway in PlaceScrollArea. Allowing it to
  // resize here would size it to the natural height of the frame,
  // which will usually be different from the scrollport height;
  // invalidating the difference will cause unnecessary repainting.
  FinishReflowChild(mInner.mScrolledFrame, presContext,
                    &kidReflowState, *aMetrics, 0, 0,
                    NS_FRAME_NO_MOVE_FRAME | NS_FRAME_NO_SIZE_VIEW);

  // XXX Some frames (e.g., nsObjectFrame, nsFrameFrame, nsTextFrame) don't bother
  // setting their mOverflowArea. This is wrong because every frame should
  // always set mOverflowArea. In fact nsObjectFrame and nsFrameFrame don't
  // support the 'outline' property because of this. Rather than fix the world
  // right now, just fix up the overflow area if necessary. Note that we don't
  // check HasOverflowRect() because it could be set even though the
  // overflow area doesn't include the frame bounds.
  aMetrics->UnionOverflowAreasWithDesiredBounds();

  aState->mContentsOverflowAreas = aMetrics->mOverflowAreas;
  aState->mReflowedContentsWithHScrollbar = aAssumeHScroll;
  aState->mReflowedContentsWithVScrollbar = aAssumeVScroll;

  return rv;
}

bool
nsHTMLScrollFrame::GuessHScrollbarNeeded(const ScrollReflowState& aState)
{
  if (aState.mStyles.mHorizontal != NS_STYLE_OVERFLOW_AUTO)
    // no guessing required
    return aState.mStyles.mHorizontal == NS_STYLE_OVERFLOW_SCROLL;

  return mInner.mHasHorizontalScrollbar;
}

bool
nsHTMLScrollFrame::GuessVScrollbarNeeded(const ScrollReflowState& aState)
{
  if (aState.mStyles.mVertical != NS_STYLE_OVERFLOW_AUTO)
    // no guessing required
    return aState.mStyles.mVertical == NS_STYLE_OVERFLOW_SCROLL;

  // If we've had at least one non-initial reflow, then just assume
  // the state of the vertical scrollbar will be what we determined
  // last time.
  if (mInner.mHadNonInitialReflow) {
    return mInner.mHasVerticalScrollbar;
  }

  // If this is the initial reflow, guess false because usually
  // we have very little content by then.
  if (InInitialReflow())
    return false;

  if (mInner.mIsRoot) {
    nsIFrame *f = mInner.mScrolledFrame->GetFirstPrincipalChild();
    if (f && f->GetType() == nsGkAtoms::svgOuterSVGFrame &&
        static_cast<nsSVGOuterSVGFrame*>(f)->VerticalScrollbarNotNeeded()) {
      // Common SVG case - avoid a bad guess.
      return false;
    }
    // Assume that there will be a scrollbar; it seems to me
    // that 'most pages' do have a scrollbar, and anyway, it's cheaper
    // to do an extra reflow for the pages that *don't* need a
    // scrollbar (because on average they will have less content).
    return true;
  }

  // For non-viewports, just guess that we don't need a scrollbar.
  // XXX I wonder if statistically this is the right idea; I'm
  // basically guessing that there are a lot of overflow:auto DIVs
  // that get their intrinsic size and don't overflow
  return false;
}

bool
nsHTMLScrollFrame::InInitialReflow() const
{
  // We're in an initial reflow if NS_FRAME_FIRST_REFLOW is set, unless we're a
  // root scrollframe.  In that case we want to skip this clause altogether.
  // The guess here is that there are lots of overflow:auto divs out there that
  // end up auto-sizing so they don't overflow, and that the root basically
  // always needs a scrollbar if it did last time we loaded this page (good
  // assumption, because our initial reflow is no longer synchronous).
  return !mInner.mIsRoot && (GetStateBits() & NS_FRAME_FIRST_REFLOW);
}

nsresult
nsHTMLScrollFrame::ReflowContents(ScrollReflowState* aState,
                                  const nsHTMLReflowMetrics& aDesiredSize)
{
  nsHTMLReflowMetrics kidDesiredSize(aDesiredSize.mFlags);
  nsresult rv = ReflowScrolledFrame(aState, GuessHScrollbarNeeded(*aState),
      GuessVScrollbarNeeded(*aState), &kidDesiredSize, true);
  NS_ENSURE_SUCCESS(rv, rv);

  // There's an important special case ... if the child appears to fit
  // in the inside-border rect (but overflows the scrollport), we
  // should try laying it out without a vertical scrollbar. It will
  // usually fit because making the available-width wider will not
  // normally make the child taller. (The only situation I can think
  // of is when you have a line containing %-width inline replaced
  // elements whose percentages sum to more than 100%, so increasing
  // the available width makes the line break where it was fitting
  // before.) If we don't treat this case specially, then we will
  // decide that showing scrollbars is OK because the content
  // overflows when we're showing scrollbars and we won't try to
  // remove the vertical scrollbar.

  // Detecting when we enter this special case is important for when
  // people design layouts that exactly fit the container "most of the
  // time".

  // XXX Is this check really sufficient to catch all the incremental cases
  // where the ideal case doesn't have a scrollbar?
  if ((aState->mReflowedContentsWithHScrollbar || aState->mReflowedContentsWithVScrollbar) &&
      aState->mStyles.mVertical != NS_STYLE_OVERFLOW_SCROLL &&
      aState->mStyles.mHorizontal != NS_STYLE_OVERFLOW_SCROLL) {
    nsSize insideBorderSize =
      ComputeInsideBorderSize(aState,
                              nsSize(kidDesiredSize.width, kidDesiredSize.height));
    nsRect scrolledRect =
      mInner.GetScrolledRectInternal(kidDesiredSize.ScrollableOverflow(),
                                     insideBorderSize);
    if (nsRect(nsPoint(0, 0), insideBorderSize).Contains(scrolledRect)) {
      // Let's pretend we had no scrollbars coming in here
      rv = ReflowScrolledFrame(aState, false, false,
                               &kidDesiredSize, false);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // Try vertical scrollbar settings that leave the vertical scrollbar unchanged.
  // Do this first because changing the vertical scrollbar setting is expensive,
  // forcing a reflow always.

  // Try leaving the horizontal scrollbar unchanged first. This will be more
  // efficient.
  if (TryLayout(aState, &kidDesiredSize, aState->mReflowedContentsWithHScrollbar,
                aState->mReflowedContentsWithVScrollbar, false, &rv))
    return NS_OK;
  if (TryLayout(aState, &kidDesiredSize, !aState->mReflowedContentsWithHScrollbar,
                aState->mReflowedContentsWithVScrollbar, false, &rv))
    return NS_OK;

  // OK, now try toggling the vertical scrollbar. The performance advantage
  // of trying the status-quo horizontal scrollbar state
  // does not exist here (we'll have to reflow due to the vertical scrollbar
  // change), so always try no horizontal scrollbar first.
  bool newVScrollbarState = !aState->mReflowedContentsWithVScrollbar;
  if (TryLayout(aState, &kidDesiredSize, false, newVScrollbarState, false, &rv))
    return NS_OK;
  if (TryLayout(aState, &kidDesiredSize, true, newVScrollbarState, false, &rv))
    return NS_OK;

  // OK, we're out of ideas. Try again enabling whatever scrollbars we can
  // enable and force the layout to stick even if it's inconsistent.
  // This just happens sometimes.
  TryLayout(aState, &kidDesiredSize,
            aState->mStyles.mHorizontal != NS_STYLE_OVERFLOW_HIDDEN,
            aState->mStyles.mVertical != NS_STYLE_OVERFLOW_HIDDEN,
            true, &rv);
  return rv;
}

void
nsHTMLScrollFrame::PlaceScrollArea(const ScrollReflowState& aState,
                                   const nsPoint& aScrollPosition)
{
  nsIFrame *scrolledFrame = mInner.mScrolledFrame;
  // Set the x,y of the scrolled frame to the correct value
  scrolledFrame->SetPosition(mInner.mScrollPort.TopLeft() - aScrollPosition);

  nsRect scrolledArea;
  // Preserve the width or height of empty rects
  nsSize portSize = mInner.mScrollPort.Size();
  nsRect scrolledRect =
    mInner.GetScrolledRectInternal(aState.mContentsOverflowAreas.ScrollableOverflow(),
                                   portSize);
  scrolledArea.UnionRectEdges(scrolledRect,
                              nsRect(nsPoint(0,0), portSize));

  // Store the new overflow area. Note that this changes where an outline
  // of the scrolled frame would be painted, but scrolled frames can't have
  // outlines (the outline would go on this scrollframe instead).
  // Using FinishAndStoreOverflow is needed so the overflow rect
  // gets set correctly.  It also messes with the overflow rect in the
  // -moz-hidden-unscrollable case, but scrolled frames can't have
  // 'overflow' either.
  // This needs to happen before SyncFrameViewAfterReflow so
  // HasOverflowRect() will return the correct value.
  nsOverflowAreas overflow(scrolledArea, scrolledArea);
  scrolledFrame->FinishAndStoreOverflow(overflow,
                                        scrolledFrame->GetSize());

  // Note that making the view *exactly* the size of the scrolled area
  // is critical, since the view scrolling code uses the size of the
  // scrolled view to clamp scroll requests.
  // Normally the scrolledFrame won't have a view but in some cases it
  // might create its own.
  nsContainerFrame::SyncFrameViewAfterReflow(scrolledFrame->PresContext(),
                                             scrolledFrame,
                                             scrolledFrame->GetView(),
                                             scrolledArea,
                                             0);
}

nscoord
nsHTMLScrollFrame::GetIntrinsicVScrollbarWidth(nsRenderingContext *aRenderingContext)
{
  nsGfxScrollFrameInner::ScrollbarStyles ss = GetScrollbarStyles();
  if (ss.mVertical != NS_STYLE_OVERFLOW_SCROLL || !mInner.mVScrollbarBox)
    return 0;

  // Don't need to worry about reflow depth here since it's
  // just for scrollbars
  nsBoxLayoutState bls(PresContext(), aRenderingContext, 0);
  nsSize vScrollbarPrefSize(0, 0);
  GetScrollbarMetrics(bls, mInner.mVScrollbarBox,
                      nullptr, &vScrollbarPrefSize, true);
  return vScrollbarPrefSize.width;
}

/* virtual */ nscoord
nsHTMLScrollFrame::GetMinWidth(nsRenderingContext *aRenderingContext)
{
  nscoord result = mInner.mScrolledFrame->GetMinWidth(aRenderingContext);
  DISPLAY_MIN_WIDTH(this, result);
  return result + GetIntrinsicVScrollbarWidth(aRenderingContext);
}

/* virtual */ nscoord
nsHTMLScrollFrame::GetPrefWidth(nsRenderingContext *aRenderingContext)
{
  nscoord result = mInner.mScrolledFrame->GetPrefWidth(aRenderingContext);
  DISPLAY_PREF_WIDTH(this, result);
  return NSCoordSaturatingAdd(result, GetIntrinsicVScrollbarWidth(aRenderingContext));
}

NS_IMETHODIMP
nsHTMLScrollFrame::GetPadding(nsMargin& aMargin)
{
  // Our padding hangs out on the inside of the scrollframe, but XUL doesn't
  // reaize that.  If we're stuck inside a XUL box, we need to claim no
  // padding.
  // @see also nsXULScrollFrame::GetPadding.
  aMargin.SizeTo(0,0,0,0);
  return NS_OK;
}

bool
nsHTMLScrollFrame::IsCollapsed()
{
  // We're never collapsed in the box sense.
  return false;
}

// Return the <browser> if the scrollframe is for the root frame directly
// inside a <browser>.
static nsIContent*
GetBrowserRoot(nsIContent* aContent)
{
  if (aContent) {
    nsIDocument* doc = aContent->GetCurrentDoc();
    nsPIDOMWindow* win = doc->GetWindow();
    if (win) {
      nsCOMPtr<nsIContent> frameContent =
        do_QueryInterface(win->GetFrameElementInternal());
      if (frameContent &&
          frameContent->NodeInfo()->Equals(nsGkAtoms::browser, kNameSpaceID_XUL))
        return frameContent;
    }
  }

  return nullptr;
}


NS_IMETHODIMP
nsHTMLScrollFrame::Reflow(nsPresContext*           aPresContext,
                          nsHTMLReflowMetrics&     aDesiredSize,
                          const nsHTMLReflowState& aReflowState,
                          nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsHTMLScrollFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  ScrollReflowState state(this, aReflowState);
  // sanity check: ensure that if we have no scrollbar, we treat it
  // as hidden.
  if (!mInner.mVScrollbarBox || mInner.mNeverHasVerticalScrollbar)
    state.mStyles.mVertical = NS_STYLE_OVERFLOW_HIDDEN;
  if (!mInner.mHScrollbarBox || mInner.mNeverHasHorizontalScrollbar)
    state.mStyles.mHorizontal = NS_STYLE_OVERFLOW_HIDDEN;

  //------------ Handle Incremental Reflow -----------------
  bool reflowHScrollbar = true;
  bool reflowVScrollbar = true;
  bool reflowScrollCorner = true;
  if (!aReflowState.ShouldReflowAllKids()) {
    #define NEEDS_REFLOW(frame_) \
      ((frame_) && NS_SUBTREE_DIRTY(frame_))

    reflowHScrollbar = NEEDS_REFLOW(mInner.mHScrollbarBox);
    reflowVScrollbar = NEEDS_REFLOW(mInner.mVScrollbarBox);
    reflowScrollCorner = NEEDS_REFLOW(mInner.mScrollCornerBox) ||
                         NEEDS_REFLOW(mInner.mResizerBox);

    #undef NEEDS_REFLOW
  }

  if (mInner.mIsRoot) {
    mInner.mCollapsedResizer = true;

    nsIContent* browserRoot = GetBrowserRoot(mContent);
    if (browserRoot) {
      bool showResizer = browserRoot->HasAttr(kNameSpaceID_None, nsGkAtoms::showresizer);
      reflowScrollCorner = showResizer == mInner.mCollapsedResizer;
      mInner.mCollapsedResizer = !showResizer;
    }
  }

  nsRect oldScrollAreaBounds = mInner.mScrollPort;
  nsRect oldScrolledAreaBounds =
    mInner.mScrolledFrame->GetScrollableOverflowRectRelativeToParent();
  // Adjust to a multiple of device pixels to restore the invariant that
  // oldScrollPosition is a multiple of device pixels. This could have been
  // thrown out by a zoom change.
  nsIntPoint ptDevPx;
  nsPoint oldScrollPosition = mInner.GetScrollPosition();

  state.mComputedBorder = aReflowState.mComputedBorderPadding -
    aReflowState.mComputedPadding;

  nsresult rv = ReflowContents(&state, aDesiredSize);
  if (NS_FAILED(rv))
    return rv;

  // Restore the old scroll position, for now, even if that's not valid anymore
  // because we changed size. We'll fix it up in a post-reflow callback, because
  // our current size may only be temporary (e.g. we're compute XUL desired sizes).
  PlaceScrollArea(state, oldScrollPosition);
  if (!mInner.mPostedReflowCallback) {
    // Make sure we'll try scrolling to restored position
    PresContext()->PresShell()->PostReflowCallback(&mInner);
    mInner.mPostedReflowCallback = true;
  }

  bool didHaveHScrollbar = mInner.mHasHorizontalScrollbar;
  bool didHaveVScrollbar = mInner.mHasVerticalScrollbar;
  mInner.mHasHorizontalScrollbar = state.mShowHScrollbar;
  mInner.mHasVerticalScrollbar = state.mShowVScrollbar;
  nsRect newScrollAreaBounds = mInner.mScrollPort;
  nsRect newScrolledAreaBounds =
    mInner.mScrolledFrame->GetScrollableOverflowRectRelativeToParent();
  if (mInner.mSkippedScrollbarLayout ||
      reflowHScrollbar || reflowVScrollbar || reflowScrollCorner ||
      (GetStateBits() & NS_FRAME_IS_DIRTY) ||
      didHaveHScrollbar != state.mShowHScrollbar ||
      didHaveVScrollbar != state.mShowVScrollbar ||
      !oldScrollAreaBounds.IsEqualEdges(newScrollAreaBounds) ||
      !oldScrolledAreaBounds.IsEqualEdges(newScrolledAreaBounds)) {
    if (!mInner.mSupppressScrollbarUpdate) {
      mInner.mSkippedScrollbarLayout = false;
      mInner.SetScrollbarVisibility(mInner.mHScrollbarBox, state.mShowHScrollbar);
      mInner.SetScrollbarVisibility(mInner.mVScrollbarBox, state.mShowVScrollbar);
      // place and reflow scrollbars
      nsRect insideBorderArea =
        nsRect(nsPoint(state.mComputedBorder.left, state.mComputedBorder.top),
               state.mInsideBorderSize);
      mInner.LayoutScrollbars(state.mBoxState, insideBorderArea,
                              oldScrollAreaBounds);
    } else {
      mInner.mSkippedScrollbarLayout = true;
    }
  }

  aDesiredSize.width = state.mInsideBorderSize.width +
    state.mComputedBorder.LeftRight();
  aDesiredSize.height = state.mInsideBorderSize.height +
    state.mComputedBorder.TopBottom();

  aDesiredSize.SetOverflowAreasToDesiredBounds();
  if (mInner.IsIgnoringViewportClipping()) {
    aDesiredSize.mOverflowAreas.UnionWith(
      state.mContentsOverflowAreas + mInner.mScrolledFrame->GetPosition());
  }

  FinishReflowWithAbsoluteFrames(aPresContext, aDesiredSize, aReflowState, aStatus);

  if (!InInitialReflow() && !mInner.mHadNonInitialReflow) {
    mInner.mHadNonInitialReflow = true;
  }

  if (mInner.mIsRoot && !oldScrolledAreaBounds.IsEqualEdges(newScrolledAreaBounds)) {
    mInner.PostScrolledAreaEvent();
  }

  aStatus = NS_FRAME_COMPLETE;
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  mInner.PostOverflowEvent();
  return rv;
}


////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG
NS_IMETHODIMP
nsHTMLScrollFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("HTMLScroll"), aResult);
}
#endif

#ifdef ACCESSIBILITY
a11y::AccType
nsHTMLScrollFrame::AccessibleType()
{
  // Create an accessible regardless of focusable state because the state can be
  // changed during frame life cycle without any notifications to accessibility.
  if (mContent->IsRootOfNativeAnonymousSubtree() ||
      GetScrollbarStyles() == nsIScrollableFrame::
        ScrollbarStyles(NS_STYLE_OVERFLOW_HIDDEN, NS_STYLE_OVERFLOW_HIDDEN) ) {
    return a11y::eNoType;
  }

  return a11y::eHyperTextType;
}
#endif

NS_QUERYFRAME_HEAD(nsHTMLScrollFrame)
  NS_QUERYFRAME_ENTRY(nsIAnonymousContentCreator)
  NS_QUERYFRAME_ENTRY(nsIScrollableFrame)
  NS_QUERYFRAME_ENTRY(nsIStatefulFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

//----------nsXULScrollFrame-------------------------------------------

nsIFrame*
NS_NewXULScrollFrame(nsIPresShell* aPresShell, nsStyleContext* aContext, bool aIsRoot)
{
  return new (aPresShell) nsXULScrollFrame(aPresShell, aContext, aIsRoot);
}

NS_IMPL_FRAMEARENA_HELPERS(nsXULScrollFrame)

nsXULScrollFrame::nsXULScrollFrame(nsIPresShell* aShell, nsStyleContext* aContext, bool aIsRoot)
  : nsBoxFrame(aShell, aContext, aIsRoot),
    mInner(this, aIsRoot)
{
  SetLayoutManager(nullptr);
}

nsMargin
nsGfxScrollFrameInner::GetDesiredScrollbarSizes(nsBoxLayoutState* aState)
{
  NS_ASSERTION(aState && aState->GetRenderingContext(),
               "Must have rendering context in layout state for size "
               "computations");

  nsMargin result(0, 0, 0, 0);

  if (mVScrollbarBox) {
    nsSize size = mVScrollbarBox->GetPrefSize(*aState);
    nsBox::AddMargin(mVScrollbarBox, size);
    if (IsScrollbarOnRight())
      result.left = size.width;
    else
      result.right = size.width;
  }

  if (mHScrollbarBox) {
    nsSize size = mHScrollbarBox->GetPrefSize(*aState);
    nsBox::AddMargin(mHScrollbarBox, size);
    // We don't currently support any scripts that would require a scrollbar
    // at the top. (Are there any?)
    result.bottom = size.height;
  }

  return result;
}

nsresult
nsXULScrollFrame::CreateAnonymousContent(nsTArray<ContentInfo>& aElements)
{
  return mInner.CreateAnonymousContent(aElements);
}

void
nsXULScrollFrame::AppendAnonymousContentTo(nsBaseContentList& aElements,
                                           uint32_t aFilter)
{
  mInner.AppendAnonymousContentTo(aElements, aFilter);
}

void
nsXULScrollFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  mInner.Destroy();
  nsBoxFrame::DestroyFrom(aDestructRoot);
}

NS_IMETHODIMP
nsXULScrollFrame::SetInitialChildList(ChildListID     aListID,
                                      nsFrameList&    aChildList)
{
  nsresult rv = nsBoxFrame::SetInitialChildList(aListID, aChildList);
  mInner.ReloadChildFrames();
  return rv;
}


NS_IMETHODIMP
nsXULScrollFrame::AppendFrames(ChildListID     aListID,
                               nsFrameList&    aFrameList)
{
  nsresult rv = nsBoxFrame::AppendFrames(aListID, aFrameList);
  mInner.ReloadChildFrames();
  return rv;
}

NS_IMETHODIMP
nsXULScrollFrame::InsertFrames(ChildListID     aListID,
                               nsIFrame*       aPrevFrame,
                               nsFrameList&    aFrameList)
{
  nsresult rv = nsBoxFrame::InsertFrames(aListID, aPrevFrame, aFrameList);
  mInner.ReloadChildFrames();
  return rv;
}

NS_IMETHODIMP
nsXULScrollFrame::RemoveFrame(ChildListID     aListID,
                              nsIFrame*       aOldFrame)
{
  nsresult rv = nsBoxFrame::RemoveFrame(aListID, aOldFrame);
  mInner.ReloadChildFrames();
  return rv;
}

nsSplittableType
nsXULScrollFrame::GetSplittableType() const
{
  return NS_FRAME_NOT_SPLITTABLE;
}

NS_IMETHODIMP
nsXULScrollFrame::GetPadding(nsMargin& aMargin)
{
  aMargin.SizeTo(0,0,0,0);
  return NS_OK;
}

nsIAtom*
nsXULScrollFrame::GetType() const
{
  return nsGkAtoms::scrollFrame;
}

nscoord
nsXULScrollFrame::GetBoxAscent(nsBoxLayoutState& aState)
{
  if (!mInner.mScrolledFrame)
    return 0;

  nscoord ascent = mInner.mScrolledFrame->GetBoxAscent(aState);
  nsMargin m(0,0,0,0);
  GetBorderAndPadding(m);
  ascent += m.top;
  GetMargin(m);
  ascent += m.top;

  return ascent;
}

nsSize
nsXULScrollFrame::GetPrefSize(nsBoxLayoutState& aState)
{
#ifdef DEBUG_LAYOUT
  PropagateDebug(aState);
#endif

  nsSize pref = mInner.mScrolledFrame->GetPrefSize(aState);

  nsGfxScrollFrameInner::ScrollbarStyles styles = GetScrollbarStyles();

  // scrolled frames don't have their own margins

  if (mInner.mVScrollbarBox &&
      styles.mVertical == NS_STYLE_OVERFLOW_SCROLL) {
    nsSize vSize = mInner.mVScrollbarBox->GetPrefSize(aState);
    nsBox::AddMargin(mInner.mVScrollbarBox, vSize);
    pref.width += vSize.width;
  }

  if (mInner.mHScrollbarBox &&
      styles.mHorizontal == NS_STYLE_OVERFLOW_SCROLL) {
    nsSize hSize = mInner.mHScrollbarBox->GetPrefSize(aState);
    nsBox::AddMargin(mInner.mHScrollbarBox, hSize);
    pref.height += hSize.height;
  }

  AddBorderAndPadding(pref);
  bool widthSet, heightSet;
  nsIFrame::AddCSSPrefSize(this, pref, widthSet, heightSet);
  return pref;
}

nsSize
nsXULScrollFrame::GetMinSize(nsBoxLayoutState& aState)
{
#ifdef DEBUG_LAYOUT
  PropagateDebug(aState);
#endif

  nsSize min = mInner.mScrolledFrame->GetMinSizeForScrollArea(aState);

  nsGfxScrollFrameInner::ScrollbarStyles styles = GetScrollbarStyles();

  if (mInner.mVScrollbarBox &&
      styles.mVertical == NS_STYLE_OVERFLOW_SCROLL) {
     nsSize vSize = mInner.mVScrollbarBox->GetMinSize(aState);
     AddMargin(mInner.mVScrollbarBox, vSize);
     min.width += vSize.width;
     if (min.height < vSize.height)
        min.height = vSize.height;
  }

  if (mInner.mHScrollbarBox &&
      styles.mHorizontal == NS_STYLE_OVERFLOW_SCROLL) {
     nsSize hSize = mInner.mHScrollbarBox->GetMinSize(aState);
     AddMargin(mInner.mHScrollbarBox, hSize);
     min.height += hSize.height;
     if (min.width < hSize.width)
        min.width = hSize.width;
  }

  AddBorderAndPadding(min);
  bool widthSet, heightSet;
  nsIFrame::AddCSSMinSize(aState, this, min, widthSet, heightSet);
  return min;
}

nsSize
nsXULScrollFrame::GetMaxSize(nsBoxLayoutState& aState)
{
#ifdef DEBUG_LAYOUT
  PropagateDebug(aState);
#endif

  nsSize maxSize(NS_INTRINSICSIZE, NS_INTRINSICSIZE);

  AddBorderAndPadding(maxSize);
  bool widthSet, heightSet;
  nsIFrame::AddCSSMaxSize(this, maxSize, widthSet, heightSet);
  return maxSize;
}

#ifdef DEBUG
NS_IMETHODIMP
nsXULScrollFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("XULScroll"), aResult);
}
#endif

NS_IMETHODIMP
nsXULScrollFrame::DoLayout(nsBoxLayoutState& aState)
{
  uint32_t flags = aState.LayoutFlags();
  nsresult rv = Layout(aState);
  aState.SetLayoutFlags(flags);

  nsBox::DoLayout(aState);
  return rv;
}

NS_QUERYFRAME_HEAD(nsXULScrollFrame)
  NS_QUERYFRAME_ENTRY(nsIAnonymousContentCreator)
  NS_QUERYFRAME_ENTRY(nsIScrollableFrame)
  NS_QUERYFRAME_ENTRY(nsIStatefulFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsBoxFrame)

//-------------------- Inner ----------------------

#define SMOOTH_SCROLL_PREF_NAME "general.smoothScroll"

const double kCurrentVelocityWeighting = 0.25;
const double kStopDecelerationWeighting = 0.4;

// AsyncScroll has ref counting.
class nsGfxScrollFrameInner::AsyncScroll MOZ_FINAL : public nsARefreshObserver {
public:
  typedef mozilla::TimeStamp TimeStamp;
  typedef mozilla::TimeDuration TimeDuration;

  AsyncScroll()
    : mIsFirstIteration(true)
    , mCallee(nullptr)
  {}

  ~AsyncScroll() {
    RemoveObserver();
  }

  nsPoint PositionAt(TimeStamp aTime);
  nsSize VelocityAt(TimeStamp aTime); // In nscoords per second

  void InitSmoothScroll(TimeStamp aTime, nsPoint aCurrentPos,
                        nsSize aCurrentVelocity, nsPoint aDestination,
                        nsIAtom *aOrigin, const nsRect& aRange);
  void Init(const nsRect& aRange) {
    mRange = aRange;
  }

  bool IsFinished(TimeStamp aTime) {
    return aTime > mStartTime + mDuration; // XXX or if we've hit the wall
  }

  TimeStamp mStartTime;

  // mPrevStartTime holds previous 3 timestamps for intervals averaging (to
  // reduce duration fluctuations). When AsyncScroll is constructed and no
  // previous timestamps are available (indicated with mIsFirstIteration),
  // initialize mPrevStartTime using imaginary previous timestamps with maximum
  // relevant intervals between them.
  TimeStamp mPrevStartTime[3];
  bool mIsFirstIteration;

  // Cached Preferences values to avoid re-reading them when extending an existing
  // animation for the same event origin (can be as frequent as every 10(!)ms for
  // a quick roll of the mouse wheel).
  // These values are minimum and maximum animation duration per event origin,
  // and a global ratio which defines how longer is the animation's duration
  // compared to the average recent events intervals (such that for a relatively
  // consistent events rate, the next event arrives before current animation ends)
  nsCOMPtr<nsIAtom> mOrigin;
  int32_t mOriginMinMS;
  int32_t mOriginMaxMS;
  double  mIntervalRatio;

  TimeDuration mDuration;
  nsPoint mStartPos;
  nsPoint mDestination;
  // Allowed destination positions around mDestination
  nsRect mRange;
  nsSMILKeySpline mTimingFunctionX;
  nsSMILKeySpline mTimingFunctionY;
  bool mIsSmoothScroll;

protected:
  double ProgressAt(TimeStamp aTime) {
    return clamped((aTime - mStartTime) / mDuration, 0.0, 1.0);
  }

  nscoord VelocityComponent(double aTimeProgress,
                            nsSMILKeySpline& aTimingFunction,
                            nscoord aStart, nscoord aDestination);

  // Initializes the timing function in such a way that the current velocity is
  // preserved.
  void InitTimingFunction(nsSMILKeySpline& aTimingFunction,
                          nscoord aCurrentPos, nscoord aCurrentVelocity,
                          nscoord aDestination);

  void InitDuration(nsIAtom *aOrigin);

// The next section is observer/callback management
// Bodies of WillRefresh and RefreshDriver contain nsGfxScrollFrameInner specific code.
public:
  NS_INLINE_DECL_REFCOUNTING(AsyncScroll)

  /*
   * Set a refresh observer for smooth scroll iterations (and start observing).
   * Should be used at most once during the lifetime of this object.
   * Return value: true on success, false otherwise.
   */
  bool SetRefreshObserver(nsGfxScrollFrameInner *aCallee) {
    NS_ASSERTION(aCallee && !mCallee, "AsyncScroll::SetRefreshObserver - Invalid usage.");

    if (!RefreshDriver(aCallee)->AddRefreshObserver(this, Flush_Style)) {
      return false;
    }

    mCallee = aCallee;
    return true;
  }

  virtual void WillRefresh(mozilla::TimeStamp aTime) {
    // The callback may release "this".
    // We don't access members after returning, so no need for KungFuDeathGrip.
    nsGfxScrollFrameInner::AsyncScrollCallback(mCallee, aTime);
  }

private:
  nsGfxScrollFrameInner *mCallee;

  nsRefreshDriver* RefreshDriver(nsGfxScrollFrameInner* aCallee) {
    return aCallee->mOuter->PresContext()->RefreshDriver();
  }

  /*
   * The refresh driver doesn't hold a reference to its observers,
   *   so releasing this object can (and is) used to remove the observer on DTOR.
   * Currently, this object is released once the scrolling ends.
   */
  void RemoveObserver() {
    if (mCallee) {
      RefreshDriver(mCallee)->RemoveRefreshObserver(this, Flush_Style);
    }
  }
};

nsPoint
nsGfxScrollFrameInner::AsyncScroll::PositionAt(TimeStamp aTime) {
  double progressX = mTimingFunctionX.GetSplineValue(ProgressAt(aTime));
  double progressY = mTimingFunctionY.GetSplineValue(ProgressAt(aTime));
  return nsPoint(NSToCoordRound((1 - progressX) * mStartPos.x + progressX * mDestination.x),
                 NSToCoordRound((1 - progressY) * mStartPos.y + progressY * mDestination.y));
}

nsSize
nsGfxScrollFrameInner::AsyncScroll::VelocityAt(TimeStamp aTime) {
  double timeProgress = ProgressAt(aTime);
  return nsSize(VelocityComponent(timeProgress, mTimingFunctionX,
                                  mStartPos.x, mDestination.x),
                VelocityComponent(timeProgress, mTimingFunctionY,
                                  mStartPos.y, mDestination.y));
}

/*
 * Calculate/update mDuration, possibly dynamically according to events rate and event origin.
 * (also maintain previous timestamps - which are only used here).
 */
void
nsGfxScrollFrameInner::AsyncScroll::InitDuration(nsIAtom *aOrigin) {
  if (!aOrigin){
    aOrigin = nsGkAtoms::other;
  }

  // Read preferences only on first iteration or for a different event origin.
  if (mIsFirstIteration || aOrigin != mOrigin) {
    mOrigin = aOrigin;
    mOriginMinMS = mOriginMaxMS = 0;
    bool isOriginSmoothnessEnabled = false;
    mIntervalRatio = 1;

    // Default values for all preferences are defined in all.js
    static const int32_t kDefaultMinMS = 150, kDefaultMaxMS = 150;
    static const bool kDefaultIsSmoothEnabled = true;

    nsAutoCString originName;
    aOrigin->ToUTF8String(originName);
    nsAutoCString prefBase = NS_LITERAL_CSTRING("general.smoothScroll.") + originName;

    isOriginSmoothnessEnabled = Preferences::GetBool(prefBase.get(), kDefaultIsSmoothEnabled);
    if (isOriginSmoothnessEnabled) {
      nsAutoCString prefMin = prefBase + NS_LITERAL_CSTRING(".durationMinMS");
      nsAutoCString prefMax = prefBase + NS_LITERAL_CSTRING(".durationMaxMS");
      mOriginMinMS = Preferences::GetInt(prefMin.get(), kDefaultMinMS);
      mOriginMaxMS = Preferences::GetInt(prefMax.get(), kDefaultMaxMS);

      static const int32_t kSmoothScrollMaxAllowedAnimationDurationMS = 10000;
      mOriginMaxMS = clamped(mOriginMaxMS, 0, kSmoothScrollMaxAllowedAnimationDurationMS);
      mOriginMinMS = clamped(mOriginMinMS, 0, mOriginMaxMS);
    }

    // Keep the animation duration longer than the average event intervals
    //   (to "connect" consecutive scroll animations before the scroll comes to a stop).
    static const double kDefaultDurationToIntervalRatio = 2; // Duplicated at all.js
    mIntervalRatio = Preferences::GetInt("general.smoothScroll.durationToIntervalRatio",
                                         kDefaultDurationToIntervalRatio * 100) / 100.0;

    // Duration should be at least as long as the intervals -> ratio is at least 1
    mIntervalRatio = std::max(1.0, mIntervalRatio);

    if (mIsFirstIteration) {
      // Starting a new scroll (i.e. not when extending an existing scroll animation),
      //   create imaginary prev timestamps with maximum relevant intervals between them.
      mIsFirstIteration = false;

      // Longest relevant interval (which results in maximum duration)
      TimeDuration maxDelta = TimeDuration::FromMilliseconds(mOriginMaxMS / mIntervalRatio);
      mPrevStartTime[0] = mStartTime         - maxDelta;
      mPrevStartTime[1] = mPrevStartTime[0]  - maxDelta;
      mPrevStartTime[2] = mPrevStartTime[1]  - maxDelta;
    }
  }

  // Average last 3 delta durations (rounding errors up to 2ms are negligible for us)
  int32_t eventsDeltaMs = (mStartTime - mPrevStartTime[2]).ToMilliseconds() / 3;
  mPrevStartTime[2] = mPrevStartTime[1];
  mPrevStartTime[1] = mPrevStartTime[0];
  mPrevStartTime[0] = mStartTime;

  // Modulate duration according to events rate (quicker events -> shorter durations).
  // The desired effect is to use longer duration when scrolling slowly, such that
  // it's easier to follow, but reduce the duration to make it feel more snappy when
  // scrolling quickly. To reduce fluctuations of the duration, we average event
  // intervals using the recent 4 timestamps (now + three prev -> 3 intervals).
  int32_t durationMS = clamped<int32_t>(eventsDeltaMs * mIntervalRatio, mOriginMinMS, mOriginMaxMS);

  mDuration = TimeDuration::FromMilliseconds(durationMS);
}

void
nsGfxScrollFrameInner::AsyncScroll::InitSmoothScroll(TimeStamp aTime,
                                                     nsPoint aCurrentPos,
                                                     nsSize aCurrentVelocity,
                                                     nsPoint aDestination,
                                                     nsIAtom *aOrigin,
                                                     const nsRect& aRange) {
  mStartTime = aTime;
  mStartPos = aCurrentPos;
  mDestination = aDestination;
  mRange = aRange;
  InitDuration(aOrigin);
  InitTimingFunction(mTimingFunctionX, mStartPos.x, aCurrentVelocity.width, aDestination.x);
  InitTimingFunction(mTimingFunctionY, mStartPos.y, aCurrentVelocity.height, aDestination.y);
}


nscoord
nsGfxScrollFrameInner::AsyncScroll::VelocityComponent(double aTimeProgress,
                                                      nsSMILKeySpline& aTimingFunction,
                                                      nscoord aStart,
                                                      nscoord aDestination)
{
  double dt, dxy;
  aTimingFunction.GetSplineDerivativeValues(aTimeProgress, dt, dxy);
  if (dt == 0)
    return dxy >= 0 ? nscoord_MAX : nscoord_MIN;

  const TimeDuration oneSecond = TimeDuration::FromSeconds(1);
  double slope = dxy / dt;
  return NSToCoordRound(slope * (aDestination - aStart) / (mDuration / oneSecond));
}

void
nsGfxScrollFrameInner::AsyncScroll::InitTimingFunction(nsSMILKeySpline& aTimingFunction,
                                                       nscoord aCurrentPos,
                                                       nscoord aCurrentVelocity,
                                                       nscoord aDestination)
{
  if (aDestination == aCurrentPos || kCurrentVelocityWeighting == 0) {
    aTimingFunction.Init(0, 0, 1 - kStopDecelerationWeighting, 1);
    return;
  }

  const TimeDuration oneSecond = TimeDuration::FromSeconds(1);
  double slope = aCurrentVelocity * (mDuration / oneSecond) / (aDestination - aCurrentPos);
  double normalization = sqrt(1.0 + slope * slope);
  double dt = 1.0 / normalization * kCurrentVelocityWeighting;
  double dxy = slope / normalization * kCurrentVelocityWeighting;
  aTimingFunction.Init(dt, dxy, 1 - kStopDecelerationWeighting, 1);
}

static bool
IsSmoothScrollingEnabled()
{
  return Preferences::GetBool(SMOOTH_SCROLL_PREF_NAME, false);
}

class ScrollFrameActivityTracker MOZ_FINAL : public nsExpirationTracker<nsGfxScrollFrameInner,4> {
public:
  // Wait for 3-4s between scrolls before we remove our layers.
  // That's 4 generations of 1s each.
  enum { TIMEOUT_MS = 1000 };
  ScrollFrameActivityTracker()
    : nsExpirationTracker<nsGfxScrollFrameInner,4>(TIMEOUT_MS) {}
  ~ScrollFrameActivityTracker() {
    AgeAllGenerations();
  }

  virtual void NotifyExpired(nsGfxScrollFrameInner *aObject) {
    RemoveObject(aObject);
    aObject->MarkInactive();
  }
};

static ScrollFrameActivityTracker *gScrollFrameActivityTracker = nullptr;

nsGfxScrollFrameInner::nsGfxScrollFrameInner(nsContainerFrame* aOuter,
                                             bool aIsRoot)
  : mHScrollbarBox(nullptr)
  , mVScrollbarBox(nullptr)
  , mScrolledFrame(nullptr)
  , mScrollCornerBox(nullptr)
  , mResizerBox(nullptr)
  , mOuter(aOuter)
  , mAsyncScroll(nullptr)
  , mDestination(0, 0)
  , mScrollPosAtLastPaint(0, 0)
  , mRestorePos(-1, -1)
  , mLastPos(-1, -1)
  , mScrollPosForLayerPixelAlignment(-1, -1)
  , mLastUpdateImagesPos(-1, -1)
  , mNeverHasVerticalScrollbar(false)
  , mNeverHasHorizontalScrollbar(false)
  , mHasVerticalScrollbar(false)
  , mHasHorizontalScrollbar(false)
  , mFrameIsUpdatingScrollbar(false)
  , mDidHistoryRestore(false)
  , mIsRoot(aIsRoot)
  , mSupppressScrollbarUpdate(false)
  , mSkippedScrollbarLayout(false)
  , mHadNonInitialReflow(false)
  , mHorizontalOverflow(false)
  , mVerticalOverflow(false)
  , mPostedReflowCallback(false)
  , mMayHaveDirtyFixedChildren(false)
  , mUpdateScrollbarAttributes(false)
  , mCollapsedResizer(false)
  , mShouldBuildLayer(false)
  , mHasBeenScrolled(false)
{
  mScrollingActive = IsAlwaysActive();

  if (LookAndFeel::GetInt(LookAndFeel::eIntID_ShowHideScrollbars) != 0) {
    mScrollbarActivity = new ScrollbarActivity(do_QueryFrame(aOuter));
  }
}

nsGfxScrollFrameInner::~nsGfxScrollFrameInner()
{
  if (mActivityExpirationState.IsTracked()) {
    gScrollFrameActivityTracker->RemoveObject(this);
  }
  if (gScrollFrameActivityTracker &&
      gScrollFrameActivityTracker->IsEmpty()) {
    delete gScrollFrameActivityTracker;
    gScrollFrameActivityTracker = nullptr;
  }

  if (mScrollActivityTimer) {
    mScrollActivityTimer->Cancel();
    mScrollActivityTimer = nullptr;
  }
}

/*
 * Callback function from AsyncScroll, used in nsGfxScrollFrameInner::ScrollTo
 */
void
nsGfxScrollFrameInner::AsyncScrollCallback(void* anInstance, mozilla::TimeStamp aTime)
{
  nsGfxScrollFrameInner* self = static_cast<nsGfxScrollFrameInner*>(anInstance);
  if (!self || !self->mAsyncScroll)
    return;

  nsRect range = self->mAsyncScroll->mRange;
  if (self->mAsyncScroll->mIsSmoothScroll) {
    if (!self->mAsyncScroll->IsFinished(aTime)) {
      nsPoint destination = self->mAsyncScroll->PositionAt(aTime);
      nsPoint start = self->mAsyncScroll->mStartPos;
      // Allow this scroll operation to land on any pixel boundary in the
      // right direction (as well as anywhere in the final allowed range,
      // since we don't want intermediate steps to be more constrained than the
      // final step!).
      static const int veryLargeDistance = nscoord_MAX/4;
      nsRect unlimitedRange(0, 0, veryLargeDistance, veryLargeDistance);
      if (destination.x < start.x) {
        unlimitedRange.x = -veryLargeDistance;
      } else if (destination.x == start.x) {
        unlimitedRange.width = 0;
      }
      if (destination.y < start.y) {
        unlimitedRange.y = -veryLargeDistance;
      } else if (destination.y == start.y) {
        unlimitedRange.height = 0;
      }
      self->ScrollToImpl(destination,
                         (unlimitedRange + destination).UnionEdges(range));
      return;
    }
  }

  // Apply desired destination range since this is the last step of scrolling.
  self->mAsyncScroll = nullptr;
  self->ScrollToImpl(self->mDestination, range);
  // We are done scrolling, set our destination to wherever we actually ended
  // up scrolling to.
  self->mDestination = self->GetScrollPosition();
}

void
nsGfxScrollFrameInner::ScrollToCSSPixels(nsIntPoint aScrollPosition)
{
  nsPoint current = GetScrollPosition();
  nsIntPoint currentCSSPixels = GetScrollPositionCSSPixels();
  nsPoint pt(nsPresContext::CSSPixelsToAppUnits(aScrollPosition.x),
             nsPresContext::CSSPixelsToAppUnits(aScrollPosition.y));
  nscoord halfPixel = nsPresContext::CSSPixelsToAppUnits(0.5f);
  nsRect range(pt.x - halfPixel, pt.y - halfPixel, 2*halfPixel - 1, 2*halfPixel - 1);
  // XXX I don't think the following blocks are needed anymore, now that
  // ScrollToImpl simply tries to scroll an integer number of layer
  // pixels from the current position
  if (currentCSSPixels.x == aScrollPosition.x) {
    pt.x = current.x;
    range.x = pt.x;
    range.width = 0;
  }
  if (currentCSSPixels.y == aScrollPosition.y) {
    pt.y = current.y;
    range.y = pt.y;
    range.height = 0;
  }
  ScrollTo(pt, nsIScrollableFrame::INSTANT, &range);
}

void
nsGfxScrollFrameInner::ScrollToCSSPixelsApproximate(const Point& aScrollPosition)
{
  nsPoint pt(nsPresContext::CSSPixelsToAppUnits(aScrollPosition.x),
             nsPresContext::CSSPixelsToAppUnits(aScrollPosition.y));
  nscoord halfRange = nsPresContext::CSSPixelsToAppUnits(1000);
  nsRect range(pt.x - halfRange, pt.y - halfRange, 2*halfRange - 1, 2*halfRange - 1);
  ScrollTo(pt, nsIScrollableFrame::INSTANT, &range);
}

nsIntPoint
nsGfxScrollFrameInner::GetScrollPositionCSSPixels()
{
  nsPoint pt = GetScrollPosition();
  return nsIntPoint(nsPresContext::AppUnitsToIntCSSPixels(pt.x),
                    nsPresContext::AppUnitsToIntCSSPixels(pt.y));
}

/*
 * this method wraps calls to ScrollToImpl(), either in one shot or incrementally,
 *  based on the setting of the smoothness scroll pref
 */
void
nsGfxScrollFrameInner::ScrollToWithOrigin(nsPoint aScrollPosition,
                                          nsIScrollableFrame::ScrollMode aMode,
                                          nsIAtom *aOrigin,
                                          const nsRect* aRange)
{
  nsRect scrollRange = GetScrollRangeForClamping();
  mDestination = scrollRange.ClampPoint(aScrollPosition);

  nsRect range = aRange ? *aRange : nsRect(aScrollPosition, nsSize(0, 0));

  if (aMode == nsIScrollableFrame::INSTANT) {
    // Asynchronous scrolling is not allowed, so we'll kill any existing
    // async-scrolling process and do an instant scroll.
    mAsyncScroll = nullptr;
    ScrollToImpl(mDestination, range);
    // We are done scrolling, set our destination to wherever we actually ended
    // up scrolling to.
    mDestination = GetScrollPosition();
    return;
  }

  TimeStamp now = TimeStamp::Now();
  nsPoint currentPosition = GetScrollPosition();
  nsSize currentVelocity(0, 0);
  bool isSmoothScroll = (aMode == nsIScrollableFrame::SMOOTH) &&
                          IsSmoothScrollingEnabled();

  if (mAsyncScroll) {
    if (mAsyncScroll->mIsSmoothScroll) {
      currentPosition = mAsyncScroll->PositionAt(now);
      currentVelocity = mAsyncScroll->VelocityAt(now);
    }
  } else {
    mAsyncScroll = new AsyncScroll;
    if (!mAsyncScroll->SetRefreshObserver(this)) {
      mAsyncScroll = nullptr;
      // Observer setup failed. Scroll the normal way.
      ScrollToImpl(mDestination, range);
      // We are done scrolling, set our destination to wherever we actually
      // ended up scrolling to.
      mDestination = GetScrollPosition();
      return;
    }
  }

  mAsyncScroll->mIsSmoothScroll = isSmoothScroll;

  if (isSmoothScroll) {
    mAsyncScroll->InitSmoothScroll(now, currentPosition, currentVelocity,
                                   mDestination, aOrigin, range);
  } else {
    mAsyncScroll->Init(range);
  }
}

// We can't use nsContainerFrame::PositionChildViews here because
// we don't want to invalidate views that have moved.
static void AdjustViews(nsIFrame* aFrame)
{
  nsView* view = aFrame->GetView();
  if (view) {
    nsPoint pt;
    aFrame->GetParent()->GetClosestView(&pt);
    pt += aFrame->GetPosition();
    view->SetPosition(pt.x, pt.y);

    return;
  }

  if (!(aFrame->GetStateBits() & NS_FRAME_HAS_CHILD_WITH_VIEW)) {
    return;
  }

  // Call AdjustViews recursively for all child frames except the popup list as
  // the views for popups are not scrolled.
  nsIFrame::ChildListIterator lists(aFrame);
  for (; !lists.IsDone(); lists.Next()) {
    if (lists.CurrentID() == nsIFrame::kPopupList) {
      continue;
    }
    nsFrameList::Enumerator childFrames(lists.CurrentList());
    for (; !childFrames.AtEnd(); childFrames.Next()) {
      AdjustViews(childFrames.get());
    }
  }
}

static bool
CanScrollWithBlitting(nsIFrame* aFrame)
{
  if (aFrame->GetStateBits() & NS_SCROLLFRAME_INVALIDATE_CONTENTS_ON_SCROLL)
    return false;

  for (nsIFrame* f = aFrame; f;
       f = nsLayoutUtils::GetCrossDocParentFrame(f)) {
    if (nsSVGIntegrationUtils::UsingEffectsForFrame(f) ||
        f->IsFrameOfType(nsIFrame::eSVG) ||
        f->GetStateBits() & NS_FRAME_NO_COMPONENT_ALPHA) {
      return false;
    }
    if (nsLayoutUtils::IsPopup(f))
      break;
  }
  return true;
}

bool nsGfxScrollFrameInner::IsIgnoringViewportClipping() const
{
  if (!mIsRoot)
    return false;
  nsSubDocumentFrame* subdocFrame = static_cast<nsSubDocumentFrame*>
    (nsLayoutUtils::GetCrossDocParentFrame(mOuter->PresContext()->PresShell()->GetRootFrame()));
  return subdocFrame && !subdocFrame->ShouldClipSubdocument();
}

bool nsGfxScrollFrameInner::ShouldClampScrollPosition() const
{
  if (!mIsRoot)
    return true;
  nsSubDocumentFrame* subdocFrame = static_cast<nsSubDocumentFrame*>
    (nsLayoutUtils::GetCrossDocParentFrame(mOuter->PresContext()->PresShell()->GetRootFrame()));
  return !subdocFrame || subdocFrame->ShouldClampScrollPosition();
}

bool nsGfxScrollFrameInner::IsAlwaysActive() const
{
  if (nsDisplayItem::ForceActiveLayers()) {
    return true;
  }

  // Unless this is the root scrollframe for a non-chrome document
  // which is the direct child of a chrome document, we default to not
  // being "active".
  if (!(mIsRoot && mOuter->PresContext()->IsRootContentDocument())) {
     return false;
  }

  // If we have scrolled before, then we should stay active.
  if (mHasBeenScrolled) {
    return true;
  }

  // If we're overflow:hidden, then start as inactive until
  // we get scrolled manually.
  ScrollbarStyles styles = GetScrollbarStylesFromFrame();
  return (styles.mHorizontal != NS_STYLE_OVERFLOW_HIDDEN &&
          styles.mVertical != NS_STYLE_OVERFLOW_HIDDEN);
}

void nsGfxScrollFrameInner::MarkInactive()
{
  if (IsAlwaysActive() || !mScrollingActive)
    return;

  mScrollingActive = false;
  mOuter->InvalidateFrameSubtree();
}

void nsGfxScrollFrameInner::MarkActive()
{
  mScrollingActive = true;
  if (IsAlwaysActive())
    return;

  if (mActivityExpirationState.IsTracked()) {
    gScrollFrameActivityTracker->MarkUsed(this);
  } else {
    if (!gScrollFrameActivityTracker) {
      gScrollFrameActivityTracker = new ScrollFrameActivityTracker();
    }
    gScrollFrameActivityTracker->AddObject(this);
  }
}

void nsGfxScrollFrameInner::ScrollVisual(nsPoint aOldScrolledFramePos)
{
  // Mark this frame as having been scrolled. If this is the root
  // scroll frame of a content document, then IsAlwaysActive()
  // will return true from now on and MarkInactive() won't
  // have any effect.
  mHasBeenScrolled = true;

  AdjustViews(mScrolledFrame);
  // We need to call this after fixing up the view positions
  // to be consistent with the frame hierarchy.
  bool canScrollWithBlitting = CanScrollWithBlitting(mOuter);
  mOuter->RemoveStateBits(NS_SCROLLFRAME_INVALIDATE_CONTENTS_ON_SCROLL);
  if (IsScrollingActive()) {
    if (!canScrollWithBlitting) {
      MarkInactive();
    }
  }
  if (canScrollWithBlitting) {
    MarkActive();
  }

  mOuter->SchedulePaint();
}

/**
 * Return an appunit value close to aDesired and between aLower and aUpper
 * such that (aDesired - aCurrent)*aRes/aAppUnitsPerPixel is an integer (or
 * as close as we can get modulo rounding to appunits). If that
 * can't be done, just returns aDesired.
 */
static nscoord
AlignWithLayerPixels(nscoord aDesired, nscoord aLower,
                     nscoord aUpper, nscoord aAppUnitsPerPixel,
                     double aRes, nscoord aCurrent)
{
  double currentLayerVal = (aRes*aCurrent)/aAppUnitsPerPixel;
  double desiredLayerVal = (aRes*aDesired)/aAppUnitsPerPixel;
  double delta = desiredLayerVal - currentLayerVal;
  double nearestVal = NS_round(delta) + currentLayerVal;

  // Convert back from ThebesLayer space to appunits relative to the top-left
  // of the scrolled frame.
  nscoord nearestAppUnitVal =
    NSToCoordRoundWithClamp(nearestVal*aAppUnitsPerPixel/aRes);

  // Check if nearest layer pixel result fit into allowed and scroll range
  if (nearestAppUnitVal >= aLower && nearestAppUnitVal <= aUpper) {
    return nearestAppUnitVal;
  } else if (nearestVal != desiredLayerVal) {
    // Check if opposite pixel boundary fit into scroll range
    double oppositeVal = nearestVal + ((nearestVal < desiredLayerVal) ? 1 : -1);
    nscoord oppositeAppUnitVal =
      NSToCoordRoundWithClamp(oppositeVal*aAppUnitsPerPixel/aRes);
    if (oppositeAppUnitVal >= aLower && oppositeAppUnitVal <= aUpper) {
      return oppositeAppUnitVal;
    }
  }
  return aDesired;
}

/**
 * Clamp desired scroll position aPt to aBounds and then snap
 * it to the same layer pixel edges as aCurrent, keeping it within aRange
 * during snapping. aCurrent is the current scroll position.
 */
static nsPoint
ClampAndAlignWithLayerPixels(const nsPoint& aPt,
                             const nsRect& aBounds,
                             const nsRect& aRange,
                             const nsPoint& aCurrent,
                             nscoord aAppUnitsPerPixel,
                             const gfxSize& aScale)
{
  nsPoint pt = aBounds.ClampPoint(aPt);
  // Intersect scroll range with allowed range, by clamping the corners
  // of aRange to be within bounds
  nsPoint rangeTopLeft = aBounds.ClampPoint(aRange.TopLeft());
  nsPoint rangeBottomRight = aBounds.ClampPoint(aRange.BottomRight());

  return nsPoint(AlignWithLayerPixels(pt.x, rangeTopLeft.x, rangeBottomRight.x,
                                      aAppUnitsPerPixel, aScale.width, aCurrent.x),
                 AlignWithLayerPixels(pt.y, rangeTopLeft.y, rangeBottomRight.y,
                                      aAppUnitsPerPixel, aScale.height, aCurrent.y));
}

/* static */ void
nsGfxScrollFrameInner::ScrollActivityCallback(nsITimer *aTimer, void* anInstance)
{
  nsGfxScrollFrameInner* self = static_cast<nsGfxScrollFrameInner*>(anInstance);

  // Fire the synth mouse move.
  self->mScrollActivityTimer->Cancel();
  self->mScrollActivityTimer = nullptr;
  self->mOuter->PresContext()->PresShell()->SynthesizeMouseMove(true);
}


void
nsGfxScrollFrameInner::ScheduleSyntheticMouseMove()
{
  if (!mScrollActivityTimer) {
    mScrollActivityTimer = do_CreateInstance("@mozilla.org/timer;1");
    if (!mScrollActivityTimer)
      return;
  }

  mScrollActivityTimer->InitWithFuncCallback(
        ScrollActivityCallback, this, 100, nsITimer::TYPE_ONE_SHOT);
}

void
nsGfxScrollFrameInner::ScrollToImpl(nsPoint aPt, const nsRect& aRange)
{
  nsPresContext* presContext = mOuter->PresContext();
  nscoord appUnitsPerDevPixel = presContext->AppUnitsPerDevPixel();
  // 'scale' is our estimate of the scale factor that will be applied
  // when rendering the scrolled content to its own ThebesLayer.
  gfxSize scale = FrameLayerBuilder::GetThebesLayerScaleForFrame(mScrolledFrame);
  nsPoint curPos = GetScrollPosition();
  nsPoint alignWithPos = mScrollPosForLayerPixelAlignment == nsPoint(-1,-1)
      ? curPos : mScrollPosForLayerPixelAlignment;
  // Try to align aPt with curPos so they have an integer number of layer
  // pixels between them. This gives us the best chance of scrolling without
  // having to invalidate due to changes in subpixel rendering.
  // Note that when we actually draw into a ThebesLayer, the coordinates
  // that get mapped onto the layer buffer pixels are from the display list,
  // which are relative to the display root frame's top-left increasing down,
  // whereas here our coordinates are scroll positions which increase upward
  // and are relative to the scrollport top-left. This difference doesn't actually
  // matter since all we are about is that there be an integer number of
  // layer pixels between pt and curPos.
  nsPoint pt =
    ClampAndAlignWithLayerPixels(aPt,
                                 GetScrollRangeForClamping(),
                                 aRange,
                                 alignWithPos,
                                 appUnitsPerDevPixel,
                                 scale);
  if (pt == curPos) {
    return;
  }

  bool needImageVisibilityUpdate = (mLastUpdateImagesPos == nsPoint(-1,-1));

  static bool sImageVisPrefsCached = false;
  // The fraction of the scrollport we allow to scroll by before we schedule
  // an update of image visibility.
  static int32_t sHorzScrollFraction = 2;
  static int32_t sVertScrollFraction = 2;
  if (!sImageVisPrefsCached) {
    Preferences::AddIntVarCache(&sHorzScrollFraction,
      "layout.imagevisibility.amountscrollbeforeupdatehorizontal", 2);
    Preferences::AddIntVarCache(&sVertScrollFraction,
      "layout.imagevisibility.amountscrollbeforeupdatevertical", 2);
    sImageVisPrefsCached = true;
  }

  nsPoint dist(std::abs(pt.x - mLastUpdateImagesPos.x),
               std::abs(pt.y - mLastUpdateImagesPos.y));
  nscoord horzAllowance = std::max(mScrollPort.width / std::max(sHorzScrollFraction, 1),
                                   nsPresContext::AppUnitsPerCSSPixel());
  nscoord vertAllowance = std::max(mScrollPort.height / std::max(sVertScrollFraction, 1),
                                   nsPresContext::AppUnitsPerCSSPixel());
  if (dist.x >= horzAllowance || dist.y >= vertAllowance) {
    needImageVisibilityUpdate = true;
  }

  if (needImageVisibilityUpdate) {
    presContext->PresShell()->ScheduleImageVisibilityUpdate();
  }

  // notify the listeners.
  for (uint32_t i = 0; i < mListeners.Length(); i++) {
    mListeners[i]->ScrollPositionWillChange(pt.x, pt.y);
  }

  nsPoint oldScrollFramePos = mScrolledFrame->GetPosition();
  // Update frame position for scrolling
  mScrolledFrame->SetPosition(mScrollPort.TopLeft() - pt);

  // We pass in the amount to move visually
  ScrollVisual(oldScrollFramePos);

  ScheduleSyntheticMouseMove();
  UpdateScrollbarPosition();
  PostScrollEvent();

  // notify the listeners.
  for (uint32_t i = 0; i < mListeners.Length(); i++) {
    mListeners[i]->ScrollPositionDidChange(pt.x, pt.y);
  }
}

static void
AppendToTop(nsDisplayListBuilder* aBuilder, nsDisplayList* aDest,
            nsDisplayList* aSource, nsIFrame* aSourceFrame, bool aOwnLayer)
{
  if (aSource->IsEmpty())
    return;
  if (aOwnLayer) {
    aDest->AppendNewToTop(
        new (aBuilder) nsDisplayOwnLayer(aBuilder, aSourceFrame, aSource));
  } else {
    aDest->AppendToTop(aSource);
  }
}

void
nsGfxScrollFrameInner::AppendScrollPartsTo(nsDisplayListBuilder*   aBuilder,
                                           const nsRect&           aDirtyRect,
                                           const nsDisplayListSet& aLists,
                                           bool&                   aCreateLayer,
                                           bool                    aPositioned)
{
  for (nsIFrame* kid = mOuter->GetFirstPrincipalChild(); kid; kid = kid->GetNextSibling()) {
    if (kid == mScrolledFrame ||
        (kid->IsPositioned() != aPositioned))
      continue;

    nsDisplayListCollection partList;
    mOuter->BuildDisplayListForChild(aBuilder, kid, aDirtyRect, partList,
                                     nsIFrame::DISPLAY_CHILD_FORCE_STACKING_CONTEXT);

    // Don't append textarea resizers to the positioned descendants because
    // we don't want them to float on top of overlapping elements.
    bool appendToPositioned = aPositioned && !(kid == mResizerBox && !mIsRoot);

    nsDisplayList* dest = appendToPositioned ?
      aLists.PositionedDescendants() : aLists.BorderBackground();

    // DISPLAY_CHILD_FORCE_STACKING_CONTEXT put everything into
    // partList.PositionedDescendants().
    ::AppendToTop(aBuilder, dest,
                  partList.PositionedDescendants(), kid,
                  aCreateLayer);
  }
}

bool
nsGfxScrollFrameInner::ShouldBuildLayer() const
{
  return mShouldBuildLayer;
}

class ScrollLayerWrapper : public nsDisplayWrapper
{
public:
  ScrollLayerWrapper(nsIFrame* aScrollFrame, nsIFrame* aScrolledFrame)
    : mCount(0)
    , mProps(aScrolledFrame->Properties())
    , mScrollFrame(aScrollFrame)
    , mScrolledFrame(aScrolledFrame)
  {
    SetCount(0);
  }

  virtual nsDisplayItem* WrapList(nsDisplayListBuilder* aBuilder,
                                  nsIFrame* aFrame,
                                  nsDisplayList* aList) {
    SetCount(++mCount);
    return new (aBuilder) nsDisplayScrollLayer(aBuilder, aList, mScrolledFrame, mScrolledFrame, mScrollFrame);
  }

  virtual nsDisplayItem* WrapItem(nsDisplayListBuilder* aBuilder,
                                  nsDisplayItem* aItem) {

    SetCount(++mCount);
    return new (aBuilder) nsDisplayScrollLayer(aBuilder, aItem, aItem->GetUnderlyingFrame(), mScrolledFrame, mScrollFrame);
  }

protected:
  void SetCount(intptr_t aCount) {
    mProps.Set(nsIFrame::ScrollLayerCount(), reinterpret_cast<void*>(aCount));
  }

  intptr_t mCount;
  FrameProperties mProps;
  nsIFrame* mScrollFrame;
  nsIFrame* mScrolledFrame;
};

void
nsGfxScrollFrameInner::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                        const nsRect&           aDirtyRect,
                                        const nsDisplayListSet& aLists)
{
  if (aBuilder->IsForImageVisibility()) {
    mLastUpdateImagesPos = GetScrollPosition();
  }

  mOuter->DisplayBorderBackgroundOutline(aBuilder, aLists);

  if (aBuilder->IsPaintingToWindow()) {
    mScrollPosAtLastPaint = GetScrollPosition();
    if (IsScrollingActive() && !CanScrollWithBlitting(mOuter)) {
      MarkInactive();
    }
    if (IsScrollingActive()) {
      if (mScrollPosForLayerPixelAlignment == nsPoint(-1,-1)) {
        mScrollPosForLayerPixelAlignment = mScrollPosAtLastPaint;
      }
    } else {
      mScrollPosForLayerPixelAlignment = nsPoint(-1,-1);
    }
  }

  if (aBuilder->GetIgnoreScrollFrame() == mOuter || IsIgnoringViewportClipping()) {
    // Don't clip the scrolled child, and don't paint scrollbars/scrollcorner.
    // The scrolled frame shouldn't have its own background/border, so we
    // can just pass aLists directly.
    mOuter->BuildDisplayListForChild(aBuilder, mScrolledFrame,
                                     aDirtyRect, aLists);
    return;
  }

  // We put scrollbars in their own layers when this is the root scroll
  // frame and we are a toplevel content document. In this situation, the
  // scrollbar(s) would normally be assigned their own layer anyway, since
  // they're not scrolled with the rest of the document. But when both
  // scrollbars are visible, the layer's visible rectangle would be the size
  // of the viewport, so most layer implementations would create a layer buffer
  // that's much larger than necessary. Creating independent layers for each
  // scrollbar works around the problem.
  bool createLayersForScrollbars = mIsRoot &&
    mOuter->PresContext()->IsRootContentDocument();

  // Now display the scrollbars and scrollcorner. These parts are drawn
  // in the border-background layer, on top of our own background and
  // borders and underneath borders and backgrounds of later elements
  // in the tree.
  // Note that this does not apply for overlay scrollbars; those are drawn
  // in the positioned-elements layer on top of everything else by the call
  // to AppendScrollPartsTo(..., true) further down.
  AppendScrollPartsTo(aBuilder, aDirtyRect, aLists, createLayersForScrollbars,
                      false);

  // Overflow clipping can never clip frames outside our subtree, so there
  // is no need to worry about whether we are a moving frame that might clip
  // non-moving frames.
  nsRect dirtyRect;
  // Not all our descendants will be clipped by overflow clipping, but all
  // the ones that aren't clipped will be out of flow frames that have already
  // had dirty rects saved for them by their parent frames calling
  // MarkOutOfFlowChildrenForDisplayList, so it's safe to restrict our
  // dirty rect here.
  dirtyRect.IntersectRect(aDirtyRect, mScrollPort);

  // Override the dirty rectangle if the displayport has been set.
  nsRect displayPort;
  bool usingDisplayport =
    nsLayoutUtils::GetDisplayPort(mOuter->GetContent(), &displayPort) &&
    !aBuilder->IsForEventDelivery();
  if (usingDisplayport) {
    dirtyRect = displayPort;
  }

  if (aBuilder->IsForImageVisibility()) {
    static bool sImageVisPrefsCached = false;
    // The number of scrollports wide/high to expand when looking for images.
    static uint32_t sHorzExpandScrollPort = 0;
    static uint32_t sVertExpandScrollPort = 1;
    if (!sImageVisPrefsCached) {
      Preferences::AddUintVarCache(&sHorzExpandScrollPort,
        "layout.imagevisibility.numscrollportwidths", (uint32_t)0);
      Preferences::AddUintVarCache(&sVertExpandScrollPort,
        "layout.imagevisibility.numscrollportheights", 1);
      sImageVisPrefsCached = true;
    }

    // We expand the dirty rect to catch images just outside of the scroll port.
    // We could be smarter and not expand the dirty rect in a direction in which
    // we are not able to scroll.
    nsRect dirtyRectBefore = dirtyRect;

    nsPoint vertShift = nsPoint(0, sVertExpandScrollPort * dirtyRectBefore.height);
    dirtyRect = dirtyRect.Union(dirtyRectBefore - vertShift);
    dirtyRect = dirtyRect.Union(dirtyRectBefore + vertShift);

    nsPoint horzShift = nsPoint(sHorzExpandScrollPort * dirtyRectBefore.width, 0);
    dirtyRect = dirtyRect.Union(dirtyRectBefore - horzShift);
    dirtyRect = dirtyRect.Union(dirtyRectBefore + horzShift);
  }

  nsDisplayListCollection set;
  {
    DisplayListClipState::AutoSaveRestore clipState(aBuilder);

    if (usingDisplayport) {
      nsRect clip = displayPort + aBuilder->ToReferenceFrame(mOuter);
      if (mIsRoot) {
        clipState.ClipContentDescendants(clip);
      } else {
        clipState.ClipContainingBlockDescendants(clip, nullptr);
      }
    } else {
      nsRect clip = mScrollPort + aBuilder->ToReferenceFrame(mOuter);
      // Our override of GetBorderRadii ensures we never have a radius at
      // the corners where we have a scrollbar.
      if (mIsRoot) {
#ifdef DEBUG
        nscoord radii[8];
#endif
        NS_ASSERTION(!mOuter->GetPaddingBoxBorderRadii(radii),
                     "Roots with radii not supported");
        clipState.ClipContentDescendants(clip);
      } else {
        nscoord radii[8];
        bool haveRadii = mOuter->GetPaddingBoxBorderRadii(radii);
        clipState.ClipContainingBlockDescendants(clip, haveRadii ? radii : nullptr);
      }
    }

    mOuter->BuildDisplayListForChild(aBuilder, mScrolledFrame, dirtyRect, aLists);
  }

  // Since making new layers is expensive, only use nsDisplayScrollLayer
  // if the area is scrollable and we're the content process.
  // When a displayport is being used, force building of a layer so that
  // CompositorParent can always find the scrollable layer for the root content
  // document.
  if (usingDisplayport) {
    mShouldBuildLayer = true;
  } else {
    nsRect scrollRange = GetScrollRange();
    ScrollbarStyles styles = GetScrollbarStylesFromFrame();
    mShouldBuildLayer =
       ((styles.mHorizontal != NS_STYLE_OVERFLOW_HIDDEN && mHScrollbarBox) ||
        (styles.mVertical   != NS_STYLE_OVERFLOW_HIDDEN && mVScrollbarBox)) &&
       (XRE_GetProcessType() == GeckoProcessType_Content &&
        (scrollRange.width > 0 || scrollRange.height > 0) &&
        (!mIsRoot || !mOuter->PresContext()->IsRootContentDocument()));
  }

  if (ShouldBuildLayer()) {
    // ScrollLayerWrapper must always be created because it initializes the
    // scroll layer count. The display lists depend on this.
    ScrollLayerWrapper wrapper(mOuter, mScrolledFrame);

    if (usingDisplayport) {
      // Once a displayport is set, assume that scrolling needs to be fast
      // so create a layer with all the content inside. The compositor
      // process will be able to scroll the content asynchronously.
      wrapper.WrapListsInPlace(aBuilder, mOuter, aLists);
    }

    // In case we are not using displayport or the nsDisplayScrollLayers are
    // flattened during visibility computation, we still need to export the
    // metadata about this scroll box to the compositor process.
    nsDisplayScrollInfoLayer* layerItem = new (aBuilder) nsDisplayScrollInfoLayer(
      aBuilder, mScrolledFrame, mOuter);
    aLists.BorderBackground()->AppendNewToBottom(layerItem);
  }

  // Now display overlay scrollbars and the resizer, if we have one.
  AppendScrollPartsTo(aBuilder, aDirtyRect, aLists, createLayersForScrollbars,
                      true);
}

static void HandleScrollPref(nsIScrollable *aScrollable, int32_t aOrientation,
                             uint8_t& aValue)
{
  int32_t pref;
  aScrollable->GetDefaultScrollbarPreferences(aOrientation, &pref);
  switch (pref) {
    case nsIScrollable::Scrollbar_Auto:
      // leave |aValue| untouched
      break;
    case nsIScrollable::Scrollbar_Never:
      aValue = NS_STYLE_OVERFLOW_HIDDEN;
      break;
    case nsIScrollable::Scrollbar_Always:
      aValue = NS_STYLE_OVERFLOW_SCROLL;
      break;
  }
}

nsGfxScrollFrameInner::ScrollbarStyles
nsGfxScrollFrameInner::GetScrollbarStylesFromFrame() const
{
  nsPresContext* presContext = mOuter->PresContext();
  if (!presContext->IsDynamic() &&
      !(mIsRoot && presContext->HasPaginatedScrolling())) {
    return ScrollbarStyles(NS_STYLE_OVERFLOW_HIDDEN, NS_STYLE_OVERFLOW_HIDDEN);
  }

  if (!mIsRoot) {
    const nsStyleDisplay* disp = mOuter->StyleDisplay();
    return ScrollbarStyles(disp->mOverflowX, disp->mOverflowY);
  }

  ScrollbarStyles result = presContext->GetViewportOverflowOverride();
  nsCOMPtr<nsISupports> container = presContext->GetContainer();
  nsCOMPtr<nsIScrollable> scrollable = do_QueryInterface(container);
  if (scrollable) {
    HandleScrollPref(scrollable, nsIScrollable::ScrollOrientation_X,
                     result.mHorizontal);
    HandleScrollPref(scrollable, nsIScrollable::ScrollOrientation_Y,
                     result.mVertical);
  }
  return result;
}

nsRect
nsGfxScrollFrameInner::GetScrollRange() const
{
  return GetScrollRange(mScrollPort.width, mScrollPort.height);
}

nsRect
nsGfxScrollFrameInner::GetScrollRange(nscoord aWidth, nscoord aHeight) const
{
  nsRect range = GetScrolledRect();
  range.width -= aWidth;
  range.height -= aHeight;
  return range;
}

nsRect
nsGfxScrollFrameInner::GetScrollRangeForClamping() const
{
  if (!ShouldClampScrollPosition()) {
    return nsRect(nscoord_MIN/2, nscoord_MIN/2,
                  nscoord_MAX - nscoord_MIN/2, nscoord_MAX - nscoord_MIN/2);
  }
  nsSize scrollPortSize = GetScrollPositionClampingScrollPortSize();
  return GetScrollRange(scrollPortSize.width, scrollPortSize.height);
}

nsSize
nsGfxScrollFrameInner::GetScrollPositionClampingScrollPortSize() const
{
  nsIPresShell* presShell = mOuter->PresContext()->PresShell();
  if (mIsRoot && presShell->IsScrollPositionClampingScrollPortSizeSet()) {
    return presShell->GetScrollPositionClampingScrollPortSize();
  }
  return mScrollPort.Size();
}

static void
AdjustForWholeDelta(int32_t aDelta, nscoord* aCoord)
{
  if (aDelta < 0) {
    *aCoord = nscoord_MIN;
  } else if (aDelta > 0) {
    *aCoord = nscoord_MAX;
  }
}

/**
 * Calculate lower/upper scrollBy range in given direction.
 * @param aDelta specifies scrollBy direction, if 0 then range will be 0 size
 * @param aPos desired destination in AppUnits
 * @param aNeg/PosTolerance defines relative range distance
 *   below and above of aPos point
 * @param aMultiplier used for conversion of tolerance into appUnis
 */
static void
CalcRangeForScrollBy(int32_t aDelta, nscoord aPos,
                     float aNegTolerance,
                     float aPosTolerance,
                     nscoord aMultiplier,
                     nscoord* aLower, nscoord* aUpper)
{
  if (!aDelta) {
    *aLower = *aUpper = aPos;
    return;
  }
  *aLower = aPos - NSToCoordRound(aMultiplier * (aDelta > 0 ? aNegTolerance : aPosTolerance));
  *aUpper = aPos + NSToCoordRound(aMultiplier * (aDelta > 0 ? aPosTolerance : aNegTolerance));
}

void
nsGfxScrollFrameInner::ScrollBy(nsIntPoint aDelta,
                                nsIScrollableFrame::ScrollUnit aUnit,
                                nsIScrollableFrame::ScrollMode aMode,
                                nsIntPoint* aOverflow,
                                nsIAtom *aOrigin)
{
  nsSize deltaMultiplier;
  float negativeTolerance;
  float positiveTolerance;
  if (!aOrigin){
    aOrigin = nsGkAtoms::other;
  }
  bool isGenericOrigin = (aOrigin == nsGkAtoms::other);
  switch (aUnit) {
  case nsIScrollableFrame::DEVICE_PIXELS: {
    nscoord appUnitsPerDevPixel =
      mOuter->PresContext()->AppUnitsPerDevPixel();
    deltaMultiplier = nsSize(appUnitsPerDevPixel, appUnitsPerDevPixel);
    if (isGenericOrigin){
      aOrigin = nsGkAtoms::pixels;
    }
    negativeTolerance = positiveTolerance = 0.5;
    break;
  }
  case nsIScrollableFrame::LINES: {
    deltaMultiplier = GetLineScrollAmount();
    if (isGenericOrigin){
      aOrigin = nsGkAtoms::lines;
    }
    negativeTolerance = positiveTolerance = 0.1;
    break;
  }
  case nsIScrollableFrame::PAGES: {
    deltaMultiplier = GetPageScrollAmount();
    if (isGenericOrigin){
      aOrigin = nsGkAtoms::pages;
    }
    negativeTolerance = 0.05;
    positiveTolerance = 0;
    break;
  }
  case nsIScrollableFrame::WHOLE: {
    nsPoint pos = GetScrollPosition();
    AdjustForWholeDelta(aDelta.x, &pos.x);
    AdjustForWholeDelta(aDelta.y, &pos.y);
    ScrollTo(pos, aMode);
    if (aOverflow) {
      *aOverflow = nsIntPoint(0, 0);
    }
    return;
  }
  default:
    NS_ERROR("Invalid scroll mode");
    return;
  }

  nsPoint newPos = mDestination +
    nsPoint(aDelta.x*deltaMultiplier.width, aDelta.y*deltaMultiplier.height);
  // Calculate desired range values.
  nscoord rangeLowerX, rangeUpperX, rangeLowerY, rangeUpperY;
  CalcRangeForScrollBy(aDelta.x, newPos.x, negativeTolerance, positiveTolerance,
                       deltaMultiplier.width, &rangeLowerX, &rangeUpperX);
  CalcRangeForScrollBy(aDelta.y, newPos.y, negativeTolerance, positiveTolerance,
                       deltaMultiplier.height, &rangeLowerY, &rangeUpperY);
  nsRect range(rangeLowerX,
               rangeLowerY,
               rangeUpperX - rangeLowerX,
               rangeUpperY - rangeLowerY);
  ScrollToWithOrigin(newPos, aMode, aOrigin, &range);

  if (aOverflow) {
    nsPoint clampAmount = newPos - mDestination;
    float appUnitsPerDevPixel = mOuter->PresContext()->AppUnitsPerDevPixel();
    *aOverflow = nsIntPoint(
        NSAppUnitsToIntPixels(clampAmount.x, appUnitsPerDevPixel),
        NSAppUnitsToIntPixels(clampAmount.y, appUnitsPerDevPixel));
  }
}

nsSize
nsGfxScrollFrameInner::GetLineScrollAmount() const
{
  nsRefPtr<nsFontMetrics> fm;
  nsLayoutUtils::GetFontMetricsForFrame(mOuter, getter_AddRefs(fm),
    nsLayoutUtils::FontSizeInflationFor(mOuter));
  NS_ASSERTION(fm, "FontMetrics is null, assuming fontHeight == 1 appunit");
  static nscoord sMinLineScrollAmountInPixels = -1;
  if (sMinLineScrollAmountInPixels < 0) {
    Preferences::AddIntVarCache(&sMinLineScrollAmountInPixels,
                                "mousewheel.min_line_scroll_amount", 1);
  }
  int32_t appUnitsPerDevPixel = mOuter->PresContext()->AppUnitsPerDevPixel();
  nscoord minScrollAmountInAppUnits =
    std::max(1, sMinLineScrollAmountInPixels) * appUnitsPerDevPixel;
  nscoord horizontalAmount = fm ? fm->AveCharWidth() : 0;
  nscoord verticalAmount = fm ? fm->MaxHeight() : 0;
  return nsSize(std::max(horizontalAmount, minScrollAmountInAppUnits),
                std::max(verticalAmount, minScrollAmountInAppUnits));
}

/**
 * Compute the scrollport size excluding any fixed-pos headers and
 * footers. A header or footer is an box that spans that entire width
 * of the viewport and touches the top (or bottom, respectively) of the
 * viewport. We also want to consider fixed elements that stack or overlap
 * to effectively create a larger header or footer. Headers and footers that
 * cover more than a third of the the viewport are ignored since they
 * probably aren't true headers and footers and we don't want to restrict
 * scrolling too much in such cases. This is a bit conservative --- some
 * pages use elements as headers or footers that don't span the entire width
 * of the viewport --- but it should be a good start.
 */
struct TopAndBottom
{
  TopAndBottom(nscoord aTop, nscoord aBottom) : top(aTop), bottom(aBottom) {}

  nscoord top, bottom;
};
struct TopComparator
{
  bool Equals(const TopAndBottom& A, const TopAndBottom& B) const {
    return A.top == B.top;
  }
  bool LessThan(const TopAndBottom& A, const TopAndBottom& B) const {
    return A.top < B.top;
  }
};
struct ReverseBottomComparator
{
  bool Equals(const TopAndBottom& A, const TopAndBottom& B) const {
    return A.bottom == B.bottom;
  }
  bool LessThan(const TopAndBottom& A, const TopAndBottom& B) const {
    return A.bottom > B.bottom;
  }
};
static nsSize
GetScrollPortSizeExcludingHeadersAndFooters(nsIFrame* aViewportFrame,
                                            const nsRect& aScrollPort)
{
  nsTArray<TopAndBottom> list;
  nsFrameList fixedFrames = aViewportFrame->GetChildList(nsIFrame::kFixedList);
  for (nsFrameList::Enumerator iterator(fixedFrames); !iterator.AtEnd();
       iterator.Next()) {
    nsIFrame* f = iterator.get();
    nsRect r = f->GetRect().Intersect(aScrollPort);
    if (r.x == 0 && r.width == aScrollPort.width &&
        r.height <= aScrollPort.height/3) {
      list.AppendElement(TopAndBottom(r.y, r.YMost()));
    }
  }

  list.Sort(TopComparator());
  nscoord headerBottom = 0;
  for (uint32_t i = 0; i < list.Length(); ++i) {
    if (list[i].top <= headerBottom) {
      headerBottom = std::max(headerBottom, list[i].bottom);
    }
  }

  list.Sort(ReverseBottomComparator());
  nscoord footerTop = aScrollPort.height;
  for (uint32_t i = 0; i < list.Length(); ++i) {
    if (list[i].bottom >= footerTop) {
      footerTop = std::min(footerTop, list[i].top);
    }
  }

  headerBottom = std::min(aScrollPort.height/3, headerBottom);
  footerTop = std::max(aScrollPort.height - aScrollPort.height/3, footerTop);

  return nsSize(aScrollPort.width, footerTop - headerBottom);
}

nsSize
nsGfxScrollFrameInner::GetPageScrollAmount() const
{
  nsSize lineScrollAmount = GetLineScrollAmount();
  nsSize effectiveScrollPortSize;
  if (mIsRoot) {
    // Reduce effective scrollport height by the height of any fixed-pos
    // headers or footers
    nsIFrame* root = mOuter->PresContext()->PresShell()->GetRootFrame();
    effectiveScrollPortSize =
      GetScrollPortSizeExcludingHeadersAndFooters(root, mScrollPort);
  } else {
    effectiveScrollPortSize = mScrollPort.Size();
  }
  // The page increment is the size of the page, minus the smaller of
  // 10% of the size or 2 lines.
  return nsSize(
    effectiveScrollPortSize.width -
      std::min(effectiveScrollPortSize.width/10, 2*lineScrollAmount.width),
    effectiveScrollPortSize.height -
      std::min(effectiveScrollPortSize.height/10, 2*lineScrollAmount.height));
}

  /**
   * this code is resposible for restoring the scroll position back to some
   * saved position. if the user has not moved the scroll position manually
   * we keep scrolling down until we get to our original position. keep in
   * mind that content could incrementally be coming in. we only want to stop
   * when we reach our new position.
   */
void
nsGfxScrollFrameInner::ScrollToRestoredPosition()
{
  if (mRestorePos.y == -1 || mLastPos.x == -1 || mLastPos.y == -1) {
    return;
  }
  // make sure our scroll position did not change for where we last put
  // it. if it does then the user must have moved it, and we no longer
  // need to restore.
  //
  // In the RTL case, we check whether the scroll position changed using the
  // logical scroll position, but we scroll to the physical scroll position in
  // all cases

  // if we didn't move, we still need to restore
  if (GetLogicalScrollPosition() == mLastPos) {
    // if our desired position is different to the scroll position, scroll.
    // remember that we could be incrementally loading so we may enter
    // and scroll many times.
    if (mRestorePos != mLastPos /* GetLogicalScrollPosition() */) {
      nsPoint scrollToPos = mRestorePos;
      if (!IsLTR())
        // convert from logical to physical scroll position
        scrollToPos.x = mScrollPort.x -
          (mScrollPort.XMost() - scrollToPos.x - mScrolledFrame->GetRect().width);
      ScrollTo(scrollToPos, nsIScrollableFrame::INSTANT);
      // Re-get the scroll position, it might not be exactly equal to
      // mRestorePos due to rounding and clamping.
      mLastPos = GetLogicalScrollPosition();
    } else {
      // if we reached the position then stop
      mRestorePos.y = -1;
      mLastPos.x = -1;
      mLastPos.y = -1;
    }
  } else {
    // user moved the position, so we won't need to restore
    mLastPos.x = -1;
    mLastPos.y = -1;
  }
}

nsresult
nsGfxScrollFrameInner::FireScrollPortEvent()
{
  mAsyncScrollPortEvent.Forget();

  // Keep this in sync with PostOverflowEvent().
  nsSize scrollportSize = mScrollPort.Size();
  nsSize childSize = GetScrolledRect().Size();

  bool newVerticalOverflow = childSize.height > scrollportSize.height;
  bool vertChanged = mVerticalOverflow != newVerticalOverflow;

  bool newHorizontalOverflow = childSize.width > scrollportSize.width;
  bool horizChanged = mHorizontalOverflow != newHorizontalOverflow;

  if (!vertChanged && !horizChanged) {
    return NS_OK;
  }

  // If both either overflowed or underflowed then we dispatch only one
  // DOM event.
  bool both = vertChanged && horizChanged &&
                newVerticalOverflow == newHorizontalOverflow;
  nsScrollPortEvent::orientType orient;
  if (both) {
    orient = nsScrollPortEvent::both;
    mHorizontalOverflow = newHorizontalOverflow;
    mVerticalOverflow = newVerticalOverflow;
  }
  else if (vertChanged) {
    orient = nsScrollPortEvent::vertical;
    mVerticalOverflow = newVerticalOverflow;
    if (horizChanged) {
      // We need to dispatch a separate horizontal DOM event. Do that the next
      // time around since dispatching the vertical DOM event might destroy
      // the frame.
      PostOverflowEvent();
    }
  }
  else {
    orient = nsScrollPortEvent::horizontal;
    mHorizontalOverflow = newHorizontalOverflow;
  }

  nsScrollPortEvent event(true,
                          (orient == nsScrollPortEvent::horizontal ?
                           mHorizontalOverflow : mVerticalOverflow) ?
                            NS_SCROLLPORT_OVERFLOW : NS_SCROLLPORT_UNDERFLOW,
                          nullptr);
  event.orient = orient;
  return nsEventDispatcher::Dispatch(mOuter->GetContent(),
                                     mOuter->PresContext(), &event);
}

void
nsGfxScrollFrameInner::ReloadChildFrames()
{
  mScrolledFrame = nullptr;
  mHScrollbarBox = nullptr;
  mVScrollbarBox = nullptr;
  mScrollCornerBox = nullptr;
  mResizerBox = nullptr;

  nsIFrame* frame = mOuter->GetFirstPrincipalChild();
  while (frame) {
    nsIContent* content = frame->GetContent();
    if (content == mOuter->GetContent()) {
      NS_ASSERTION(!mScrolledFrame, "Already found the scrolled frame");
      mScrolledFrame = frame;
    } else {
      nsAutoString value;
      content->GetAttr(kNameSpaceID_None, nsGkAtoms::orient, value);
      if (!value.IsEmpty()) {
        // probably a scrollbar then
        if (value.LowerCaseEqualsLiteral("horizontal")) {
          NS_ASSERTION(!mHScrollbarBox, "Found multiple horizontal scrollbars?");
          mHScrollbarBox = frame;
        } else {
          NS_ASSERTION(!mVScrollbarBox, "Found multiple vertical scrollbars?");
          mVScrollbarBox = frame;
        }
      } else if (content->Tag() == nsGkAtoms::resizer) {
        NS_ASSERTION(!mResizerBox, "Found multiple resizers");
        mResizerBox = frame;
      } else if (content->Tag() == nsGkAtoms::scrollcorner) {
        // probably a scrollcorner
        NS_ASSERTION(!mScrollCornerBox, "Found multiple scrollcorners");
        mScrollCornerBox = frame;
      }
    }

    frame = frame->GetNextSibling();
  }
}

nsresult
nsGfxScrollFrameInner::CreateAnonymousContent(
  nsTArray<nsIAnonymousContentCreator::ContentInfo>& aElements)
{
  nsPresContext* presContext = mOuter->PresContext();
  nsIFrame* parent = mOuter->GetParent();

  // Don't create scrollbars if we're an SVG document being used as an image,
  // or if we're printing/print previewing.
  // (In the printing case, we allow scrollbars if this is the child of the
  // viewport & paginated scrolling is enabled, because then we must be the
  // scroll frame for the print preview window, & that does need scrollbars.)
  if (presContext->Document()->IsBeingUsedAsImage() ||
      (!presContext->IsDynamic() &&
       !(mIsRoot && presContext->HasPaginatedScrolling()))) {
    mNeverHasVerticalScrollbar = mNeverHasHorizontalScrollbar = true;
    return NS_OK;
  }

  // Check if the frame is resizable.
  int8_t resizeStyle = mOuter->StyleDisplay()->mResize;
  bool isResizable = resizeStyle != NS_STYLE_RESIZE_NONE;

  nsIScrollableFrame *scrollable = do_QueryFrame(mOuter);

  // If we're the scrollframe for the root, then we want to construct
  // our scrollbar frames no matter what.  That way later dynamic
  // changes to propagated overflow styles will show or hide
  // scrollbars on the viewport without requiring frame reconstruction
  // of the viewport (good!).
  bool canHaveHorizontal;
  bool canHaveVertical;
  if (!mIsRoot) {
    ScrollbarStyles styles = scrollable->GetScrollbarStyles();
    canHaveHorizontal = styles.mHorizontal != NS_STYLE_OVERFLOW_HIDDEN;
    canHaveVertical = styles.mVertical != NS_STYLE_OVERFLOW_HIDDEN;
    if (!canHaveHorizontal && !canHaveVertical && !isResizable) {
      // Nothing to do.
      return NS_OK;
    }
  } else {
    canHaveHorizontal = true;
    canHaveVertical = true;
  }

  // The anonymous <div> used by <inputs> never gets scrollbars.
  nsITextControlFrame* textFrame = do_QueryFrame(parent);
  if (textFrame) {
    // Make sure we are not a text area.
    nsCOMPtr<nsIDOMHTMLTextAreaElement> textAreaElement(do_QueryInterface(parent->GetContent()));
    if (!textAreaElement) {
      mNeverHasVerticalScrollbar = mNeverHasHorizontalScrollbar = true;
      return NS_OK;
    }
  }

  nsNodeInfoManager *nodeInfoManager =
    presContext->Document()->NodeInfoManager();
  nsCOMPtr<nsINodeInfo> nodeInfo;
  nodeInfo = nodeInfoManager->GetNodeInfo(nsGkAtoms::scrollbar, nullptr,
                                          kNameSpaceID_XUL,
                                          nsIDOMNode::ELEMENT_NODE);
  NS_ENSURE_TRUE(nodeInfo, NS_ERROR_OUT_OF_MEMORY);

  if (canHaveHorizontal) {
    nsCOMPtr<nsINodeInfo> ni = nodeInfo;
    NS_TrustedNewXULElement(getter_AddRefs(mHScrollbarContent), ni.forget());
    mHScrollbarContent->SetAttr(kNameSpaceID_None, nsGkAtoms::orient,
                                NS_LITERAL_STRING("horizontal"), false);
    mHScrollbarContent->SetAttr(kNameSpaceID_None, nsGkAtoms::clickthrough,
                                NS_LITERAL_STRING("always"), false);
    if (!aElements.AppendElement(mHScrollbarContent))
      return NS_ERROR_OUT_OF_MEMORY;
  }

  if (canHaveVertical) {
    nsCOMPtr<nsINodeInfo> ni = nodeInfo;
    NS_TrustedNewXULElement(getter_AddRefs(mVScrollbarContent), ni.forget());
    mVScrollbarContent->SetAttr(kNameSpaceID_None, nsGkAtoms::orient,
                                NS_LITERAL_STRING("vertical"), false);
    mVScrollbarContent->SetAttr(kNameSpaceID_None, nsGkAtoms::clickthrough,
                                NS_LITERAL_STRING("always"), false);
    if (!aElements.AppendElement(mVScrollbarContent))
      return NS_ERROR_OUT_OF_MEMORY;
  }

  if (isResizable) {
    nsCOMPtr<nsINodeInfo> nodeInfo;
    nodeInfo = nodeInfoManager->GetNodeInfo(nsGkAtoms::resizer, nullptr,
                                            kNameSpaceID_XUL,
                                            nsIDOMNode::ELEMENT_NODE);
    NS_ENSURE_TRUE(nodeInfo, NS_ERROR_OUT_OF_MEMORY);

    NS_TrustedNewXULElement(getter_AddRefs(mResizerContent), nodeInfo.forget());

    nsAutoString dir;
    switch (resizeStyle) {
      case NS_STYLE_RESIZE_HORIZONTAL:
        if (IsScrollbarOnRight()) {
          dir.AssignLiteral("right");
        }
        else {
          dir.AssignLiteral("left");
        }
        break;
      case NS_STYLE_RESIZE_VERTICAL:
        dir.AssignLiteral("bottom");
        break;
      case NS_STYLE_RESIZE_BOTH:
        dir.AssignLiteral("bottomend");
        break;
      default:
        NS_WARNING("only resizable types should have resizers");
    }
    mResizerContent->SetAttr(kNameSpaceID_None, nsGkAtoms::dir, dir, false);

    if (mIsRoot) {
      nsIContent* browserRoot = GetBrowserRoot(mOuter->GetContent());
      mCollapsedResizer = !(browserRoot &&
                            browserRoot->HasAttr(kNameSpaceID_None, nsGkAtoms::showresizer));
    }
    else {
      mResizerContent->SetAttr(kNameSpaceID_None, nsGkAtoms::element,
                                    NS_LITERAL_STRING("_parent"), false);
    }

    mResizerContent->SetAttr(kNameSpaceID_None, nsGkAtoms::clickthrough,
                                  NS_LITERAL_STRING("always"), false);

    if (!aElements.AppendElement(mResizerContent))
      return NS_ERROR_OUT_OF_MEMORY;
  }

  if (canHaveHorizontal && canHaveVertical) {
    nodeInfo = nodeInfoManager->GetNodeInfo(nsGkAtoms::scrollcorner, nullptr,
                                            kNameSpaceID_XUL,
                                            nsIDOMNode::ELEMENT_NODE);
    NS_TrustedNewXULElement(getter_AddRefs(mScrollCornerContent), nodeInfo.forget());
    if (!aElements.AppendElement(mScrollCornerContent))
      return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

void
nsGfxScrollFrameInner::AppendAnonymousContentTo(nsBaseContentList& aElements,
                                                uint32_t aFilter)
{
  aElements.MaybeAppendElement(mHScrollbarContent);
  aElements.MaybeAppendElement(mVScrollbarContent);
  aElements.MaybeAppendElement(mScrollCornerContent);
  aElements.MaybeAppendElement(mResizerContent);
}

void
nsGfxScrollFrameInner::Destroy()
{
  if (mScrollbarActivity) {
    mScrollbarActivity = nullptr;
  }

  // Unbind any content created in CreateAnonymousContent from the tree
  nsContentUtils::DestroyAnonymousContent(&mHScrollbarContent);
  nsContentUtils::DestroyAnonymousContent(&mVScrollbarContent);
  nsContentUtils::DestroyAnonymousContent(&mScrollCornerContent);
  nsContentUtils::DestroyAnonymousContent(&mResizerContent);

  if (mPostedReflowCallback) {
    mOuter->PresContext()->PresShell()->CancelReflowCallback(this);
    mPostedReflowCallback = false;
  }
}

/**
 * Called when we want to update the scrollbar position, either because scrolling happened
 * or the user moved the scrollbar position and we need to undo that (e.g., when the user
 * clicks to scroll and we're using smooth scrolling, so we need to put the thumb back
 * to its initial position for the start of the smooth sequence).
 */
void
nsGfxScrollFrameInner::UpdateScrollbarPosition()
{
  mFrameIsUpdatingScrollbar = true;

  nsPoint pt = GetScrollPosition();
  if (mVScrollbarBox) {
    SetCoordAttribute(mVScrollbarBox->GetContent(), nsGkAtoms::curpos,
                      pt.y - GetScrolledRect().y);
  }
  if (mHScrollbarBox) {
    SetCoordAttribute(mHScrollbarBox->GetContent(), nsGkAtoms::curpos,
                      pt.x - GetScrolledRect().x);
  }

  mFrameIsUpdatingScrollbar = false;
}

void nsGfxScrollFrameInner::CurPosAttributeChanged(nsIContent* aContent)
{
  NS_ASSERTION(aContent, "aContent must not be null");
  NS_ASSERTION((mHScrollbarBox && mHScrollbarBox->GetContent() == aContent) ||
               (mVScrollbarBox && mVScrollbarBox->GetContent() == aContent),
               "unexpected child");

  if (mScrollbarActivity) {
    mScrollbarActivity->ActivityOccurred();
  }

  // Attribute changes on the scrollbars happen in one of three ways:
  // 1) The scrollbar changed the attribute in response to some user event
  // 2) We changed the attribute in response to a ScrollPositionDidChange
  // callback from the scrolling view
  // 3) We changed the attribute to adjust the scrollbars for the start
  // of a smooth scroll operation
  //
  // In cases 2 and 3 we do not need to scroll because we're just
  // updating our scrollbar.
  if (mFrameIsUpdatingScrollbar)
    return;

  nsRect scrolledRect = GetScrolledRect();

  nsPoint current = GetScrollPosition() - scrolledRect.TopLeft();
  nsPoint dest;
  nsRect allowedRange;
  dest.x = GetCoordAttribute(mHScrollbarBox, nsGkAtoms::curpos, current.x,
                             &allowedRange.x, &allowedRange.width);
  dest.y = GetCoordAttribute(mVScrollbarBox, nsGkAtoms::curpos, current.y,
                             &allowedRange.y, &allowedRange.height);
  current += scrolledRect.TopLeft();
  dest += scrolledRect.TopLeft();
  allowedRange += scrolledRect.TopLeft();

  // Don't try to scroll if we're already at an acceptable place.
  // Don't call Contains here since Contains returns false when the point is
  // on the bottom or right edge of the rectangle.
  if (allowedRange.ClampPoint(current) == current) {
    return;
  }

  bool isSmooth = aContent->HasAttr(kNameSpaceID_None, nsGkAtoms::smooth);
  if (isSmooth) {
    // Make sure an attribute-setting callback occurs even if the view
    // didn't actually move yet.  We need to make sure other listeners
    // see that the scroll position is not (yet) what they thought it
    // was.
    UpdateScrollbarPosition();
  }
  ScrollToWithOrigin(dest,
                     isSmooth ? nsIScrollableFrame::SMOOTH : nsIScrollableFrame::INSTANT,
                     nsGkAtoms::scrollbars, &allowedRange);
}

/* ============= Scroll events ========== */

NS_IMETHODIMP
nsGfxScrollFrameInner::ScrollEvent::Run()
{
  if (mInner)
    mInner->FireScrollEvent();
  return NS_OK;
}

void
nsGfxScrollFrameInner::FireScrollEvent()
{
  mScrollEvent.Forget();

  nsScrollbarEvent event(true, NS_SCROLL_EVENT, nullptr);
  nsEventStatus status = nsEventStatus_eIgnore;
  nsIContent* content = mOuter->GetContent();
  nsPresContext* prescontext = mOuter->PresContext();
  // Fire viewport scroll events at the document (where they
  // will bubble to the window)
  if (mIsRoot) {
    nsIDocument* doc = content->GetCurrentDoc();
    if (doc) {
      nsEventDispatcher::Dispatch(doc, prescontext, &event, nullptr,  &status);
    }
  } else {
    // scroll events fired at elements don't bubble (although scroll events
    // fired at documents do, to the window)
    event.mFlags.mBubbles = false;
    nsEventDispatcher::Dispatch(content, prescontext, &event, nullptr, &status);
  }
}

void
nsGfxScrollFrameInner::PostScrollEvent()
{
  if (mScrollEvent.IsPending())
    return;

  nsRootPresContext* rpc = mOuter->PresContext()->GetRootPresContext();
  if (!rpc)
    return;
  mScrollEvent = new ScrollEvent(this);
  rpc->AddWillPaintObserver(mScrollEvent.get());
}

NS_IMETHODIMP
nsGfxScrollFrameInner::AsyncScrollPortEvent::Run()
{
  if (mInner) {
    mInner->mOuter->PresContext()->GetPresShell()->
      FlushPendingNotifications(Flush_InterruptibleLayout);
  }
  return mInner ? mInner->FireScrollPortEvent() : NS_OK;
}

bool
nsXULScrollFrame::AddHorizontalScrollbar(nsBoxLayoutState& aState, bool aOnBottom)
{
  if (!mInner.mHScrollbarBox)
    return true;

  return AddRemoveScrollbar(aState, aOnBottom, true, true);
}

bool
nsXULScrollFrame::AddVerticalScrollbar(nsBoxLayoutState& aState, bool aOnRight)
{
  if (!mInner.mVScrollbarBox)
    return true;

  return AddRemoveScrollbar(aState, aOnRight, false, true);
}

void
nsXULScrollFrame::RemoveHorizontalScrollbar(nsBoxLayoutState& aState, bool aOnBottom)
{
  // removing a scrollbar should always fit
#ifdef DEBUG
  bool result =
#endif
  AddRemoveScrollbar(aState, aOnBottom, true, false);
  NS_ASSERTION(result, "Removing horizontal scrollbar failed to fit??");
}

void
nsXULScrollFrame::RemoveVerticalScrollbar(nsBoxLayoutState& aState, bool aOnRight)
{
  // removing a scrollbar should always fit
#ifdef DEBUG
  bool result =
#endif
  AddRemoveScrollbar(aState, aOnRight, false, false);
  NS_ASSERTION(result, "Removing vertical scrollbar failed to fit??");
}

bool
nsXULScrollFrame::AddRemoveScrollbar(nsBoxLayoutState& aState,
                                     bool aOnRightOrBottom, bool aHorizontal, bool aAdd)
{
  if (aHorizontal) {
     if (mInner.mNeverHasHorizontalScrollbar || !mInner.mHScrollbarBox)
       return false;

     nsSize hSize = mInner.mHScrollbarBox->GetPrefSize(aState);
     nsBox::AddMargin(mInner.mHScrollbarBox, hSize);

     mInner.SetScrollbarVisibility(mInner.mHScrollbarBox, aAdd);

     bool hasHorizontalScrollbar;
     bool fit = AddRemoveScrollbar(hasHorizontalScrollbar,
                                     mInner.mScrollPort.y,
                                     mInner.mScrollPort.height,
                                     hSize.height, aOnRightOrBottom, aAdd);
     mInner.mHasHorizontalScrollbar = hasHorizontalScrollbar;    // because mHasHorizontalScrollbar is a bool
     if (!fit)
        mInner.SetScrollbarVisibility(mInner.mHScrollbarBox, !aAdd);

     return fit;
  } else {
     if (mInner.mNeverHasVerticalScrollbar || !mInner.mVScrollbarBox)
       return false;

     nsSize vSize = mInner.mVScrollbarBox->GetPrefSize(aState);
     nsBox::AddMargin(mInner.mVScrollbarBox, vSize);

     mInner.SetScrollbarVisibility(mInner.mVScrollbarBox, aAdd);

     bool hasVerticalScrollbar;
     bool fit = AddRemoveScrollbar(hasVerticalScrollbar,
                                     mInner.mScrollPort.x,
                                     mInner.mScrollPort.width,
                                     vSize.width, aOnRightOrBottom, aAdd);
     mInner.mHasVerticalScrollbar = hasVerticalScrollbar;    // because mHasVerticalScrollbar is a bool
     if (!fit)
        mInner.SetScrollbarVisibility(mInner.mVScrollbarBox, !aAdd);

     return fit;
  }
}

bool
nsXULScrollFrame::AddRemoveScrollbar(bool& aHasScrollbar, nscoord& aXY,
                                     nscoord& aSize, nscoord aSbSize,
                                     bool aOnRightOrBottom, bool aAdd)
{
   nscoord size = aSize;
   nscoord xy = aXY;

   if (size != NS_INTRINSICSIZE) {
     if (aAdd) {
        size -= aSbSize;
        if (!aOnRightOrBottom && size >= 0)
          xy += aSbSize;
     } else {
        size += aSbSize;
        if (!aOnRightOrBottom)
          xy -= aSbSize;
     }
   }

   // not enough room? Yes? Return true.
   if (size >= 0) {
       aHasScrollbar = aAdd;
       aSize = size;
       aXY = xy;
       return true;
   }

   aHasScrollbar = false;
   return false;
}

void
nsXULScrollFrame::LayoutScrollArea(nsBoxLayoutState& aState,
                                   const nsPoint& aScrollPosition)
{
  uint32_t oldflags = aState.LayoutFlags();
  nsRect childRect = nsRect(mInner.mScrollPort.TopLeft() - aScrollPosition,
                            mInner.mScrollPort.Size());
  int32_t flags = NS_FRAME_NO_MOVE_VIEW;

  nsSize minSize = mInner.mScrolledFrame->GetMinSize(aState);

  if (minSize.height > childRect.height)
    childRect.height = minSize.height;

  if (minSize.width > childRect.width)
    childRect.width = minSize.width;

  aState.SetLayoutFlags(flags);
  ClampAndSetBounds(aState, childRect, aScrollPosition);
  mInner.mScrolledFrame->Layout(aState);

  childRect = mInner.mScrolledFrame->GetRect();

  if (childRect.width < mInner.mScrollPort.width ||
      childRect.height < mInner.mScrollPort.height)
  {
    childRect.width = std::max(childRect.width, mInner.mScrollPort.width);
    childRect.height = std::max(childRect.height, mInner.mScrollPort.height);

    // remove overflow areas when we update the bounds,
    // because we've already accounted for it
    // REVIEW: Have we accounted for both?
    ClampAndSetBounds(aState, childRect, aScrollPosition, true);
  }

  aState.SetLayoutFlags(oldflags);

}

void nsGfxScrollFrameInner::PostOverflowEvent()
{
  if (mAsyncScrollPortEvent.IsPending())
    return;

  // Keep this in sync with FireScrollPortEvent().
  nsSize scrollportSize = mScrollPort.Size();
  nsSize childSize = GetScrolledRect().Size();

  bool newVerticalOverflow = childSize.height > scrollportSize.height;
  bool vertChanged = mVerticalOverflow != newVerticalOverflow;

  bool newHorizontalOverflow = childSize.width > scrollportSize.width;
  bool horizChanged = mHorizontalOverflow != newHorizontalOverflow;

  if (!vertChanged && !horizChanged) {
    return;
  }

  nsRootPresContext* rpc = mOuter->PresContext()->GetRootPresContext();
  if (!rpc)
    return;

  mAsyncScrollPortEvent = new AsyncScrollPortEvent(this);
  rpc->AddWillPaintObserver(mAsyncScrollPortEvent.get());
}

bool
nsGfxScrollFrameInner::IsLTR() const
{
  //TODO make bidi code set these from preferences

  nsIFrame *frame = mOuter;
  // XXX This is a bit on the slow side.
  if (mIsRoot) {
    // If we're the root scrollframe, we need the root element's style data.
    nsPresContext *presContext = mOuter->PresContext();
    nsIDocument *document = presContext->Document();
    Element *root = document->GetRootElement();

    // But for HTML and XHTML we want the body element.
    nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(document);
    if (htmlDoc) {
      Element *bodyElement = document->GetBodyElement();
      if (bodyElement)
        root = bodyElement; // we can trust the document to hold on to it
    }

    if (root) {
      nsIFrame *rootsFrame = root->GetPrimaryFrame();
      if (rootsFrame)
        frame = rootsFrame;
    }
  }

  return frame->StyleVisibility()->mDirection != NS_STYLE_DIRECTION_RTL;
}

bool
nsGfxScrollFrameInner::IsScrollbarOnRight() const
{
  nsPresContext *presContext = mOuter->PresContext();

  // The position of the scrollbar in top-level windows depends on the pref
  // layout.scrollbar.side. For non-top-level elements, it depends only on the
  // directionaliy of the element (equivalent to a value of "1" for the pref).
  if (!mIsRoot)
    return IsLTR();
  switch (presContext->GetCachedIntPref(kPresContext_ScrollbarSide)) {
    default:
    case 0: // UI directionality
      return presContext->GetCachedIntPref(kPresContext_BidiDirection)
             == IBMBIDI_TEXTDIRECTION_LTR;
    case 1: // Document / content directionality
      return IsLTR();
    case 2: // Always right
      return true;
    case 3: // Always left
      return false;
  }
}

/**
 * Reflow the scroll area if it needs it and return its size. Also determine if the reflow will
 * cause any of the scrollbars to need to be reflowed.
 */
nsresult
nsXULScrollFrame::Layout(nsBoxLayoutState& aState)
{
  bool scrollbarRight = mInner.IsScrollbarOnRight();
  bool scrollbarBottom = true;

  // get the content rect
  nsRect clientRect(0,0,0,0);
  GetClientRect(clientRect);

  nsRect oldScrollAreaBounds = mInner.mScrollPort;
  nsPoint oldScrollPosition = mInner.GetLogicalScrollPosition();

  // the scroll area size starts off as big as our content area
  mInner.mScrollPort = clientRect;

  /**************
   Our basic strategy here is to first try laying out the content with
   the scrollbars in their current state. We're hoping that that will
   just "work"; the content will overflow wherever there's a scrollbar
   already visible. If that does work, then there's no need to lay out
   the scrollarea. Otherwise we fix up the scrollbars; first we add a
   vertical one to scroll the content if necessary, or remove it if
   it's not needed. Then we reflow the content if the scrollbar
   changed.  Then we add a horizontal scrollbar if necessary (or
   remove if not needed), and if that changed, we reflow the content
   again. At this point, any scrollbars that are needed to scroll the
   content have been added.

   In the second phase we check to see if any scrollbars are too small
   to display, and if so, we remove them. We check the horizontal
   scrollbar first; removing it might make room for the vertical
   scrollbar, and if we have room for just one scrollbar we'll save
   the vertical one.

   Finally we position and size the scrollbars and scrollcorner (the
   square that is needed in the corner of the window when two
   scrollbars are visible), and reflow any fixed position views
   (if we're the viewport and we added or removed a scrollbar).
   **************/

  ScrollbarStyles styles = GetScrollbarStyles();

  // Look at our style do we always have vertical or horizontal scrollbars?
  if (styles.mHorizontal == NS_STYLE_OVERFLOW_SCROLL)
     mInner.mHasHorizontalScrollbar = true;
  if (styles.mVertical == NS_STYLE_OVERFLOW_SCROLL)
     mInner.mHasVerticalScrollbar = true;

  if (mInner.mHasHorizontalScrollbar)
     AddHorizontalScrollbar(aState, scrollbarBottom);

  if (mInner.mHasVerticalScrollbar)
     AddVerticalScrollbar(aState, scrollbarRight);

  // layout our the scroll area
  LayoutScrollArea(aState, oldScrollPosition);

  // now look at the content area and see if we need scrollbars or not
  bool needsLayout = false;

  // if we have 'auto' scrollbars look at the vertical case
  if (styles.mVertical != NS_STYLE_OVERFLOW_SCROLL) {
    // These are only good until the call to LayoutScrollArea.
    nsRect scrolledRect = mInner.GetScrolledRect();

    // There are two cases to consider
      if (scrolledRect.height <= mInner.mScrollPort.height
          || styles.mVertical != NS_STYLE_OVERFLOW_AUTO) {
        if (mInner.mHasVerticalScrollbar) {
          // We left room for the vertical scrollbar, but it's not needed;
          // remove it.
          RemoveVerticalScrollbar(aState, scrollbarRight);
          needsLayout = true;
        }
      } else {
        if (!mInner.mHasVerticalScrollbar) {
          // We didn't leave room for the vertical scrollbar, but it turns
          // out we needed it
          if (AddVerticalScrollbar(aState, scrollbarRight))
            needsLayout = true;
        }
    }

    // ok layout at the right size
    if (needsLayout) {
       nsBoxLayoutState resizeState(aState);
       LayoutScrollArea(resizeState, oldScrollPosition);
       needsLayout = false;
    }
  }


  // if scrollbars are auto look at the horizontal case
  if (styles.mHorizontal != NS_STYLE_OVERFLOW_SCROLL)
  {
    // These are only good until the call to LayoutScrollArea.
    nsRect scrolledRect = mInner.GetScrolledRect();

    // if the child is wider that the scroll area
    // and we don't have a scrollbar add one.
    if ((scrolledRect.width > mInner.mScrollPort.width)
        && styles.mHorizontal == NS_STYLE_OVERFLOW_AUTO) {

      if (!mInner.mHasHorizontalScrollbar) {
           // no scrollbar?
          if (AddHorizontalScrollbar(aState, scrollbarBottom))
             needsLayout = true;

           // if we added a horizontal scrollbar and we did not have a vertical
           // there is a chance that by adding the horizontal scrollbar we will
           // suddenly need a vertical scrollbar. Is a special case but its
           // important.
           //if (!mHasVerticalScrollbar && scrolledRect.height > scrollAreaRect.height - sbSize.height)
           //  printf("****Gfx Scrollbar Special case hit!!*****\n");

      }
    } else {
        // if the area is smaller or equal to and we have a scrollbar then
        // remove it.
      if (mInner.mHasHorizontalScrollbar) {
        RemoveHorizontalScrollbar(aState, scrollbarBottom);
        needsLayout = true;
      }
    }
  }

  // we only need to set the rect. The inner child stays the same size.
  if (needsLayout) {
     nsBoxLayoutState resizeState(aState);
     LayoutScrollArea(resizeState, oldScrollPosition);
     needsLayout = false;
  }

  // get the preferred size of the scrollbars
  nsSize hMinSize(0, 0);
  if (mInner.mHScrollbarBox && mInner.mHasHorizontalScrollbar) {
    GetScrollbarMetrics(aState, mInner.mHScrollbarBox, &hMinSize, nullptr, false);
  }
  nsSize vMinSize(0, 0);
  if (mInner.mVScrollbarBox && mInner.mHasVerticalScrollbar) {
    GetScrollbarMetrics(aState, mInner.mVScrollbarBox, &vMinSize, nullptr, true);
  }

  // Disable scrollbars that are too small
  // Disable horizontal scrollbar first. If we have to disable only one
  // scrollbar, we'd rather keep the vertical scrollbar.
  // Note that we always give horizontal scrollbars their preferred height,
  // never their min-height. So check that there's room for the preferred height.
  if (mInner.mHasHorizontalScrollbar &&
      (hMinSize.width > clientRect.width - vMinSize.width
       || hMinSize.height > clientRect.height)) {
    RemoveHorizontalScrollbar(aState, scrollbarBottom);
    needsLayout = true;
  }
  // Now disable vertical scrollbar if necessary
  if (mInner.mHasVerticalScrollbar &&
      (vMinSize.height > clientRect.height - hMinSize.height
       || vMinSize.width > clientRect.width)) {
    RemoveVerticalScrollbar(aState, scrollbarRight);
    needsLayout = true;
  }

  // we only need to set the rect. The inner child stays the same size.
  if (needsLayout) {
    nsBoxLayoutState resizeState(aState);
    LayoutScrollArea(resizeState, oldScrollPosition);
  }

  if (!mInner.mSupppressScrollbarUpdate) {
    mInner.LayoutScrollbars(aState, clientRect, oldScrollAreaBounds);
  }
  if (!mInner.mPostedReflowCallback) {
    // Make sure we'll try scrolling to restored position
    PresContext()->PresShell()->PostReflowCallback(&mInner);
    mInner.mPostedReflowCallback = true;
  }
  if (!(GetStateBits() & NS_FRAME_FIRST_REFLOW)) {
    mInner.mHadNonInitialReflow = true;
  }

  // Set up overflow areas for block frames for the benefit of
  // text-overflow.
  nsIFrame* f = mInner.mScrolledFrame->GetContentInsertionFrame();
  if (nsLayoutUtils::GetAsBlock(f)) {
    nsRect origRect = f->GetRect();
    nsRect clippedRect = origRect;
    clippedRect.MoveBy(mInner.mScrollPort.TopLeft());
    clippedRect.IntersectRect(clippedRect, mInner.mScrollPort);
    nsOverflowAreas overflow = f->GetOverflowAreas();
    f->FinishAndStoreOverflow(overflow, clippedRect.Size());
    clippedRect.MoveTo(origRect.TopLeft());
    f->SetRect(clippedRect);
  }

  mInner.PostOverflowEvent();
  return NS_OK;
}

void
nsGfxScrollFrameInner::FinishReflowForScrollbar(nsIContent* aContent,
                                                nscoord aMinXY, nscoord aMaxXY,
                                                nscoord aCurPosXY,
                                                nscoord aPageIncrement,
                                                nscoord aIncrement)
{
  // Scrollbars assume zero is the minimum position, so translate for them.
  SetCoordAttribute(aContent, nsGkAtoms::curpos, aCurPosXY - aMinXY);
  SetScrollbarEnabled(aContent, aMaxXY - aMinXY);
  SetCoordAttribute(aContent, nsGkAtoms::maxpos, aMaxXY - aMinXY);
  SetCoordAttribute(aContent, nsGkAtoms::pageincrement, aPageIncrement);
  SetCoordAttribute(aContent, nsGkAtoms::increment, aIncrement);
}

bool
nsGfxScrollFrameInner::ReflowFinished()
{
  nsAutoScriptBlocker scriptBlocker;
  mPostedReflowCallback = false;

  ScrollToRestoredPosition();

  // Clamp current scroll position to new bounds. Normally this won't
  // do anything.
  nsPoint currentScrollPos = GetScrollPosition();
  ScrollToImpl(currentScrollPos, nsRect(currentScrollPos, nsSize(0, 0)));
  if (!mAsyncScroll) {
    // We need to have mDestination track the current scroll position,
    // in case it falls outside the new reflow area. mDestination is used
    // by ScrollBy as its starting position.
    mDestination = GetScrollPosition();
  }

  if (NS_SUBTREE_DIRTY(mOuter) || !mUpdateScrollbarAttributes)
    return false;

  mUpdateScrollbarAttributes = false;

  // Update scrollbar attributes.
  nsPresContext* presContext = mOuter->PresContext();

  if (mMayHaveDirtyFixedChildren) {
    mMayHaveDirtyFixedChildren = false;
    nsIFrame* parentFrame = mOuter->GetParent();
    for (nsIFrame* fixedChild =
           parentFrame->GetFirstChild(nsIFrame::kFixedList);
         fixedChild; fixedChild = fixedChild->GetNextSibling()) {
      // force a reflow of the fixed child
      presContext->PresShell()->
        FrameNeedsReflow(fixedChild, nsIPresShell::eResize,
                         NS_FRAME_HAS_DIRTY_CHILDREN);
    }
  }

  nsRect scrolledContentRect = GetScrolledRect();
  nscoord minX = scrolledContentRect.x;
  nscoord maxX = scrolledContentRect.XMost() - mScrollPort.width;
  nscoord minY = scrolledContentRect.y;
  nscoord maxY = scrolledContentRect.YMost() - mScrollPort.height;

  // Suppress handling of the curpos attribute changes we make here.
  NS_ASSERTION(!mFrameIsUpdatingScrollbar, "We shouldn't be reentering here");
  mFrameIsUpdatingScrollbar = true;

  nsCOMPtr<nsIContent> vScroll =
    mVScrollbarBox ? mVScrollbarBox->GetContent() : nullptr;
  nsCOMPtr<nsIContent> hScroll =
    mHScrollbarBox ? mHScrollbarBox->GetContent() : nullptr;

  // Note, in some cases mOuter may get deleted while finishing reflow
  // for scrollbars.
  if (vScroll || hScroll) {
    nsWeakFrame weakFrame(mOuter);
    nsPoint scrollPos = GetScrollPosition();
    nsSize lineScrollAmount = GetLineScrollAmount();
    if (vScroll) {
      const double kScrollMultiplier =
        Preferences::GetInt("toolkit.scrollbox.verticalScrollDistance",
                            NS_DEFAULT_VERTICAL_SCROLL_DISTANCE);
      nscoord increment = lineScrollAmount.height * kScrollMultiplier;
      // We normally use (scrollArea.height - increment) for height
      // of page scrolling.  However, it is too small when
      // increment is very large. (If increment is larger than
      // scrollArea.height, direction of scrolling will be opposite).
      // To avoid it, we use (float(scrollArea.height) * 0.8) as
      // lower bound value of height of page scrolling. (bug 383267)
      // XXX shouldn't we use GetPageScrollAmount here?
      nscoord pageincrement = nscoord(mScrollPort.height - increment);
      nscoord pageincrementMin = nscoord(float(mScrollPort.height) * 0.8);
      FinishReflowForScrollbar(vScroll, minY, maxY, scrollPos.y,
                               std::max(pageincrement, pageincrementMin),
                               increment);
    }
    if (hScroll) {
      const double kScrollMultiplier =
        Preferences::GetInt("toolkit.scrollbox.horizontalScrollDistance",
                            NS_DEFAULT_HORIZONTAL_SCROLL_DISTANCE);
      nscoord increment = lineScrollAmount.width * kScrollMultiplier;
      FinishReflowForScrollbar(hScroll, minX, maxX, scrollPos.x,
                               nscoord(float(mScrollPort.width) * 0.8),
                               increment);
    }
    NS_ENSURE_TRUE(weakFrame.IsAlive(), false);
  }

  mFrameIsUpdatingScrollbar = false;
  // We used to rely on the curpos attribute changes above to scroll the
  // view.  However, for scrolling to the left of the viewport, we
  // rescale the curpos attribute, which means that operations like
  // resizing the window while it is scrolled all the way to the left
  // hold the curpos attribute constant at 0 while still requiring
  // scrolling.  So we suppress the effect of the changes above with
  // mFrameIsUpdatingScrollbar and call CurPosAttributeChanged here.
  // (It actually even works some of the time without this, thanks to
  // nsSliderFrame::AttributeChanged's handling of maxpos, but not when
  // we hide the scrollbar on a large size change, such as
  // maximization.)
  if (!mHScrollbarBox && !mVScrollbarBox)
    return false;
  CurPosAttributeChanged(mVScrollbarBox ? mVScrollbarBox->GetContent()
                                        : mHScrollbarBox->GetContent());
  return true;
}

void
nsGfxScrollFrameInner::ReflowCallbackCanceled()
{
  mPostedReflowCallback = false;
}

bool
nsGfxScrollFrameInner::UpdateOverflow()
{
  nsIScrollableFrame* sf = do_QueryFrame(mOuter);
  ScrollbarStyles ss = sf->GetScrollbarStyles();

  if (ss.mVertical != NS_STYLE_OVERFLOW_HIDDEN ||
      ss.mHorizontal != NS_STYLE_OVERFLOW_HIDDEN ||
      GetScrollPosition() != nsPoint()) {
    // If there are scrollbars, or we're not at the beginning of the pane,
    // the scroll position may change. In this case, mark the frame as
    // needing reflow.
    mOuter->PresContext()->PresShell()->FrameNeedsReflow(
      mOuter, nsIPresShell::eResize, NS_FRAME_IS_DIRTY);
  }

  // Scroll frames never have overflow area because they always clip their
  // children, so return false.
  return false;
}

void
nsGfxScrollFrameInner::AdjustScrollbarRectForResizer(
                         nsIFrame* aFrame, nsPresContext* aPresContext,
                         nsRect& aRect, bool aHasResizer, bool aVertical)
{
  if ((aVertical ? aRect.width : aRect.height) == 0)
    return;

  // if a content resizer is present, use its size. Otherwise, check if the
  // widget has a resizer.
  nsRect resizerRect;
  if (aHasResizer) {
    resizerRect = mResizerBox->GetRect();
  }
  else {
    nsPoint offset;
    nsIWidget* widget = aFrame->GetNearestWidget(offset);
    nsIntRect widgetRect;
    if (!widget || !widget->ShowsResizeIndicator(&widgetRect))
      return;

    resizerRect = nsRect(aPresContext->DevPixelsToAppUnits(widgetRect.x) - offset.x,
                         aPresContext->DevPixelsToAppUnits(widgetRect.y) - offset.y,
                         aPresContext->DevPixelsToAppUnits(widgetRect.width),
                         aPresContext->DevPixelsToAppUnits(widgetRect.height));
  }

  if (!resizerRect.Contains(aRect.BottomRight() - nsPoint(1, 1)))
    return;

  if (aVertical)
    aRect.height = std::max(0, resizerRect.y - aRect.y);
  else
    aRect.width = std::max(0, resizerRect.x - aRect.x);
}

void
nsGfxScrollFrameInner::LayoutScrollbars(nsBoxLayoutState& aState,
                                        const nsRect& aContentArea,
                                        const nsRect& aOldScrollArea)
{
  NS_ASSERTION(!mSupppressScrollbarUpdate,
               "This should have been suppressed");

  bool hasResizer = HasResizer();
  bool scrollbarOnLeft = !IsScrollbarOnRight();

  // place the scrollcorner
  if (mScrollCornerBox || mResizerBox) {
    NS_PRECONDITION(!mScrollCornerBox || mScrollCornerBox->IsBoxFrame(), "Must be a box frame!");

    nsRect r(0, 0, 0, 0);
    if (aContentArea.x != mScrollPort.x || scrollbarOnLeft) {
      // scrollbar (if any) on left
      r.x = aContentArea.x;
      r.width = mScrollPort.x - aContentArea.x;
      NS_ASSERTION(r.width >= 0, "Scroll area should be inside client rect");
    } else {
      // scrollbar (if any) on right
      r.width = aContentArea.XMost() - mScrollPort.XMost();
      r.x = aContentArea.XMost() - r.width;
      NS_ASSERTION(r.width >= 0, "Scroll area should be inside client rect");
    }
    if (aContentArea.y != mScrollPort.y) {
      NS_ERROR("top scrollbars not supported");
    } else {
      // scrollbar (if any) on bottom
      r.height = aContentArea.YMost() - mScrollPort.YMost();
      r.y = aContentArea.YMost() - r.height;
      NS_ASSERTION(r.height >= 0, "Scroll area should be inside client rect");
    }

    if (mScrollCornerBox) {
      nsBoxFrame::LayoutChildAt(aState, mScrollCornerBox, r);
    }

    if (hasResizer) {
      // if a resizer is present, get its size. Assume a default size of 15 pixels.
      nsSize resizerSize;
      nscoord defaultSize = nsPresContext::CSSPixelsToAppUnits(15);
      resizerSize.width = mVScrollbarBox ?
        mVScrollbarBox->GetPrefSize(aState).width : defaultSize;
      if (resizerSize.width > r.width) {
        r.width = resizerSize.width;
        if (aContentArea.x == mScrollPort.x && !scrollbarOnLeft)
          r.x = aContentArea.XMost() - r.width;
      }

      resizerSize.height = mHScrollbarBox ?
        mHScrollbarBox->GetPrefSize(aState).height : defaultSize;
      if (resizerSize.height > r.height) {
        r.height = resizerSize.height;
        if (aContentArea.y == mScrollPort.y)
          r.y = aContentArea.YMost() - r.height;
      }

      nsBoxFrame::LayoutChildAt(aState, mResizerBox, r);
    }
    else if (mResizerBox) {
      // otherwise lay out the resizer with an empty rectangle
      nsBoxFrame::LayoutChildAt(aState, mResizerBox, nsRect());
    }
  }

  nsPresContext* presContext = mScrolledFrame->PresContext();
  if (mVScrollbarBox) {
    NS_PRECONDITION(mVScrollbarBox->IsBoxFrame(), "Must be a box frame!");
    nsRect vRect(mScrollPort);
    vRect.width = aContentArea.width - mScrollPort.width;
    vRect.x = scrollbarOnLeft ? aContentArea.x : mScrollPort.XMost();
    if (mHasVerticalScrollbar) {
      nsMargin margin;
      mVScrollbarBox->GetMargin(margin);
      vRect.Deflate(margin);
    }
    AdjustScrollbarRectForResizer(mOuter, presContext, vRect, hasResizer, true);
    nsBoxFrame::LayoutChildAt(aState, mVScrollbarBox, vRect);
  }

  if (mHScrollbarBox) {
    NS_PRECONDITION(mHScrollbarBox->IsBoxFrame(), "Must be a box frame!");
    nsRect hRect(mScrollPort);
    hRect.height = aContentArea.height - mScrollPort.height;
    hRect.y = true ? mScrollPort.YMost() : aContentArea.y;
    if (mHasHorizontalScrollbar) {
      nsMargin margin;
      mHScrollbarBox->GetMargin(margin);
      hRect.Deflate(margin);
    }
    AdjustScrollbarRectForResizer(mOuter, presContext, hRect, hasResizer, false);
    nsBoxFrame::LayoutChildAt(aState, mHScrollbarBox, hRect);
  }

  // may need to update fixed position children of the viewport,
  // if the client area changed size because of an incremental
  // reflow of a descendant.  (If the outer frame is dirty, the fixed
  // children will be re-laid out anyway)
  if (aOldScrollArea.Size() != mScrollPort.Size() &&
      !(mOuter->GetStateBits() & NS_FRAME_IS_DIRTY) &&
      mIsRoot) {
    mMayHaveDirtyFixedChildren = true;
  }

  // post reflow callback to modify scrollbar attributes
  mUpdateScrollbarAttributes = true;
  if (!mPostedReflowCallback) {
    aState.PresContext()->PresShell()->PostReflowCallback(this);
    mPostedReflowCallback = true;
  }
}

void
nsGfxScrollFrameInner::SetScrollbarEnabled(nsIContent* aContent, nscoord aMaxPos)
{
  if (aMaxPos) {
    aContent->UnsetAttr(kNameSpaceID_None, nsGkAtoms::disabled, true);
  } else {
    aContent->SetAttr(kNameSpaceID_None, nsGkAtoms::disabled,
                      NS_LITERAL_STRING("true"), true);
  }
}

void
nsGfxScrollFrameInner::SetCoordAttribute(nsIContent* aContent, nsIAtom* aAtom,
                                         nscoord aSize)
{
  // convert to pixels
  aSize = nsPresContext::AppUnitsToIntCSSPixels(aSize);

  // only set the attribute if it changed.

  nsAutoString newValue;
  newValue.AppendInt(aSize);

  if (aContent->AttrValueIs(kNameSpaceID_None, aAtom, newValue, eCaseMatters))
    return;

  aContent->SetAttr(kNameSpaceID_None, aAtom, newValue, true);

  if (mScrollbarActivity) {
    mScrollbarActivity->ActivityOccurred();
  }
}

static void
ReduceRadii(nscoord aXBorder, nscoord aYBorder,
            nscoord& aXRadius, nscoord& aYRadius)
{
  // In order to ensure that the inside edge of the border has no
  // curvature, we need at least one of its radii to be zero.
  if (aXRadius <= aXBorder || aYRadius <= aYBorder)
    return;

  // For any corner where we reduce the radii, preserve the corner's shape.
  double ratio = std::max(double(aXBorder) / aXRadius,
                        double(aYBorder) / aYRadius);
  aXRadius *= ratio;
  aYRadius *= ratio;
}

/**
 * Implement an override for nsIFrame::GetBorderRadii to ensure that
 * the clipping region for the border radius does not clip the scrollbars.
 *
 * In other words, we require that the border radius be reduced until the
 * inner border radius at the inner edge of the border is 0 wherever we
 * have scrollbars.
 */
bool
nsGfxScrollFrameInner::GetBorderRadii(nscoord aRadii[8]) const
{
  if (!mOuter->nsContainerFrame::GetBorderRadii(aRadii))
    return false;

  // Since we can use GetActualScrollbarSizes (rather than
  // GetDesiredScrollbarSizes) since this doesn't affect reflow, we
  // probably should.
  nsMargin sb = GetActualScrollbarSizes();
  nsMargin border = mOuter->GetUsedBorder();

  if (sb.left > 0 || sb.top > 0) {
    ReduceRadii(border.left, border.top,
                aRadii[NS_CORNER_TOP_LEFT_X],
                aRadii[NS_CORNER_TOP_LEFT_Y]);
  }

  if (sb.top > 0 || sb.right > 0) {
    ReduceRadii(border.right, border.top,
                aRadii[NS_CORNER_TOP_RIGHT_X],
                aRadii[NS_CORNER_TOP_RIGHT_Y]);
  }

  if (sb.right > 0 || sb.bottom > 0) {
    ReduceRadii(border.right, border.bottom,
                aRadii[NS_CORNER_BOTTOM_RIGHT_X],
                aRadii[NS_CORNER_BOTTOM_RIGHT_Y]);
  }

  if (sb.bottom > 0 || sb.left > 0) {
    ReduceRadii(border.left, border.bottom,
                aRadii[NS_CORNER_BOTTOM_LEFT_X],
                aRadii[NS_CORNER_BOTTOM_LEFT_Y]);
  }

  return true;
}

nsRect
nsGfxScrollFrameInner::GetScrolledRect() const
{
  nsRect result =
    GetScrolledRectInternal(mScrolledFrame->GetScrollableOverflowRect(),
                            mScrollPort.Size());

  NS_ASSERTION(result.width >= mScrollPort.width,
               "Scrolled rect smaller than scrollport?");
  NS_ASSERTION(result.height >= mScrollPort.height,
               "Scrolled rect smaller than scrollport?");
  return result;
}

nsRect
nsGfxScrollFrameInner::GetScrolledRectInternal(const nsRect& aScrolledFrameOverflowArea,
                                               const nsSize& aScrollPortSize) const
{
  return nsLayoutUtils::GetScrolledRect(mScrolledFrame,
      aScrolledFrameOverflowArea, aScrollPortSize,
      IsLTR() ? NS_STYLE_DIRECTION_LTR : NS_STYLE_DIRECTION_RTL);
}

nsMargin
nsGfxScrollFrameInner::GetActualScrollbarSizes() const
{
  nsRect r = mOuter->GetPaddingRect() - mOuter->GetPosition();

  return nsMargin(mScrollPort.y - r.y,
                  r.XMost() - mScrollPort.XMost(),
                  r.YMost() - mScrollPort.YMost(),
                  mScrollPort.x - r.x);
}

void
nsGfxScrollFrameInner::SetScrollbarVisibility(nsIFrame* aScrollbar, bool aVisible)
{
  nsScrollbarFrame* scrollbar = do_QueryFrame(aScrollbar);
  if (scrollbar) {
    // See if we have a mediator.
    nsIScrollbarMediator* mediator = scrollbar->GetScrollbarMediator();
    if (mediator) {
      // Inform the mediator of the visibility change.
      mediator->VisibilityChanged(aVisible);
    }
  }
}

nscoord
nsGfxScrollFrameInner::GetCoordAttribute(nsIFrame* aBox, nsIAtom* aAtom,
                                         nscoord aDefaultValue,
                                         nscoord* aRangeStart,
                                         nscoord* aRangeLength)
{
  if (aBox) {
    nsIContent* content = aBox->GetContent();

    nsAutoString value;
    content->GetAttr(kNameSpaceID_None, aAtom, value);
    if (!value.IsEmpty())
    {
      nsresult error;
      // convert it to appunits
      nscoord result = nsPresContext::CSSPixelsToAppUnits(value.ToInteger(&error));
      nscoord halfPixel = nsPresContext::CSSPixelsToAppUnits(0.5f);
      // Any nscoord value that would round to the attribute value when converted
      // to CSS pixels is allowed.
      *aRangeStart = result - halfPixel;
      *aRangeLength = halfPixel*2 - 1;
      return result;
    }
  }

  // Only this exact default value is allowed.
  *aRangeStart = aDefaultValue;
  *aRangeLength = 0;
  return aDefaultValue;
}

nsPresState*
nsGfxScrollFrameInner::SaveState()
{
  nsIScrollbarMediator* mediator = do_QueryFrame(GetScrolledFrame());
  if (mediator) {
    // child handles its own scroll state, so don't bother saving state here
    return nullptr;
  }

  nsPresState* state = new nsPresState();
  // Save mRestorePos instead of our actual current scroll position, if it's
  // valid and we haven't moved since the last update of mLastPos (same check
  // that ScrollToRestoredPosition uses). This ensures if a reframe occurs
  // while we're in the process of loading content to scroll to a restored
  // position, we'll keep trying after the reframe.
  nsPoint pt = GetLogicalScrollPosition();
  if (mRestorePos.y != -1 && pt == mLastPos) {
    pt = mRestorePos;
  }
  state->SetScrollState(pt);
  return state;
}

void
nsGfxScrollFrameInner::RestoreState(nsPresState* aState)
{
  mRestorePos = aState->GetScrollState();
  mDidHistoryRestore = true;
  mLastPos = mScrolledFrame ? GetLogicalScrollPosition() : nsPoint(0,0);
}

void
nsGfxScrollFrameInner::PostScrolledAreaEvent()
{
  if (mScrolledAreaEvent.IsPending()) {
    return;
  }
  mScrolledAreaEvent = new ScrolledAreaEvent(this);
  nsContentUtils::AddScriptRunner(mScrolledAreaEvent.get());
}

////////////////////////////////////////////////////////////////////////////////
// ScrolledArea change event dispatch

NS_IMETHODIMP
nsGfxScrollFrameInner::ScrolledAreaEvent::Run()
{
  if (mInner) {
    mInner->FireScrolledAreaEvent();
  }
  return NS_OK;
}

void
nsGfxScrollFrameInner::FireScrolledAreaEvent()
{
  mScrolledAreaEvent.Forget();

  nsScrollAreaEvent event(true, NS_SCROLLEDAREACHANGED, nullptr);
  nsPresContext *prescontext = mOuter->PresContext();
  nsIContent* content = mOuter->GetContent();

  event.mArea = mScrolledFrame->GetScrollableOverflowRectRelativeToParent();

  nsIDocument *doc = content->GetCurrentDoc();
  if (doc) {
    nsEventDispatcher::Dispatch(doc, prescontext, &event, nullptr);
  }
}

uint32_t
nsIScrollableFrame::GetPerceivedScrollingDirections() const
{
  nscoord oneDevPixel = GetScrolledFrame()->PresContext()->AppUnitsPerDevPixel();
  uint32_t directions = GetScrollbarVisibility();
  nsRect scrollRange = GetScrollRange();
  if (scrollRange.width >= oneDevPixel) {
    directions |= HORIZONTAL;
  }
  if (scrollRange.height >= oneDevPixel) {
    directions |= VERTICAL;
  }
  return directions;
}
