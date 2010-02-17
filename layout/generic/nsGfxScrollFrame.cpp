/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Mats Palmgren <mats.palmgren@bredband.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* rendering object to wrap rendering objects that should be scrollable */

#include "nsCOMPtr.h"
#include "nsHTMLParts.h"
#include "nsPresContext.h"
#include "nsIServiceManager.h"
#include "nsIView.h"
#include "nsIScrollable.h"
#include "nsIViewManager.h"
#include "nsHTMLContainerFrame.h"
#include "nsGfxScrollFrame.h"
#include "nsGkAtoms.h"
#include "nsINameSpaceManager.h"
#include "nsIDocument.h"
#include "nsIFontMetrics.h"
#include "nsIDocumentObserver.h"
#include "nsIDocument.h"
#include "nsBoxLayoutState.h"
#include "nsINodeInfo.h"
#include "nsIScrollbarFrame.h"
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
#include "nsIGlobalHistory3.h"
#include "nsDocShellCID.h"
#include "nsIHTMLDocument.h"
#include "nsEventDispatcher.h"
#include "nsContentUtils.h"
#include "nsLayoutUtils.h"
#ifdef ACCESSIBILITY
#include "nsIAccessibilityService.h"
#endif
#include "nsDisplayList.h"
#include "nsBidiUtils.h"
#include "nsFrameManager.h"
#include "nsIPrefService.h"

//----------------------------------------------------------------------

//----------nsHTMLScrollFrame-------------------------------------------

nsIFrame*
NS_NewHTMLScrollFrame(nsIPresShell* aPresShell, nsStyleContext* aContext, PRBool aIsRoot)
{
  return new (aPresShell) nsHTMLScrollFrame(aPresShell, aContext, aIsRoot);
}

NS_IMPL_FRAMEARENA_HELPERS(nsHTMLScrollFrame)

nsHTMLScrollFrame::nsHTMLScrollFrame(nsIPresShell* aShell, nsStyleContext* aContext, PRBool aIsRoot)
  : nsHTMLContainerFrame(aContext),
    mInner(this, aIsRoot, PR_FALSE)
{
}

nsresult
nsHTMLScrollFrame::CreateAnonymousContent(nsTArray<nsIContent*>& aElements)
{
  return mInner.CreateAnonymousContent(aElements);
}

void
nsHTMLScrollFrame::GetAnonymousContent(nsBaseContentList& aElements)
{
  mInner.GetAnonymousContent(aElements);
}

void
nsHTMLScrollFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  mInner.Destroy();
  nsHTMLContainerFrame::DestroyFrom(aDestructRoot);
}

NS_IMETHODIMP
nsHTMLScrollFrame::SetInitialChildList(nsIAtom*     aListName,
                                       nsFrameList& aChildList)
{
  nsresult rv = nsHTMLContainerFrame::SetInitialChildList(aListName, aChildList);
  mInner.ReloadChildFrames();
  return rv;
}


NS_IMETHODIMP
nsHTMLScrollFrame::AppendFrames(nsIAtom*  aListName,
                                nsFrameList& aFrameList)
{
  NS_ASSERTION(!aListName, "Only main list supported");
  mFrames.AppendFrames(nsnull, aFrameList);
  mInner.ReloadChildFrames();
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLScrollFrame::InsertFrames(nsIAtom*  aListName,
                                nsIFrame* aPrevFrame,
                                nsFrameList& aFrameList)
{
  NS_ASSERTION(!aListName, "Only main list supported");
  NS_ASSERTION(!aPrevFrame || aPrevFrame->GetParent() == this,
               "inserting after sibling frame with different parent");
  mFrames.InsertFrames(nsnull, aPrevFrame, aFrameList);
  mInner.ReloadChildFrames();
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLScrollFrame::RemoveFrame(nsIAtom*  aListName,
                               nsIFrame* aOldFrame)
{
  NS_ASSERTION(!aListName, "Only main list supported");
  mFrames.DestroyFrame(aOldFrame);
  mInner.ReloadChildFrames();
  return NS_OK;
}

nsSplittableType
nsHTMLScrollFrame::GetSplittableType() const
{
  return NS_FRAME_NOT_SPLITTABLE;
}

PRIntn
nsHTMLScrollFrame::GetSkipSides() const
{
  return 0;
}

nsIAtom*
nsHTMLScrollFrame::GetType() const
{
  return nsGkAtoms::scrollFrame; 
}

void
nsHTMLScrollFrame::InvalidateInternal(const nsRect& aDamageRect,
                                      nscoord aX, nscoord aY, nsIFrame* aForChild,
                                      PRUint32 aFlags)
{
  if (aForChild) {
    if (aForChild == mInner.mScrolledFrame) {
      // restrict aDamageRect to the scrollable view's bounds
      nsRect damage = aDamageRect + nsPoint(aX, aY);
      nsRect r;
      if (r.IntersectRect(damage, mInner.mScrollPort)) {
        nsHTMLContainerFrame::InvalidateInternal(r, 0, 0, aForChild, aFlags);
      }
      if (mInner.mIsRoot && r != damage) {
        // Make sure we notify our prescontext about invalidations outside
        // viewport clipping.
        // This is important for things that are snapshotting the viewport,
        // possibly outside the scrolled bounds.
        // We don't need to propagate this any further up, though. Anyone who
        // cares about scrolled-out-of-view invalidates had better be listening
        // to our window directly.
        PresContext()->NotifyInvalidation(damage, aFlags);
      }
      return;
    } else if (aForChild == mInner.mHScrollbarBox) {
      if (!mInner.mHasHorizontalScrollbar) {
        // Our scrollbars may send up invalidations even when they're collapsed,
        // because we just size a collapsed scrollbar to empty and some
        // descendants may be non-empty. Suppress that invalidation here.
        return;
      }
    } else if (aForChild == mInner.mVScrollbarBox) {
      if (!mInner.mHasVerticalScrollbar) {
        // Our scrollbars may send up invalidations even when they're collapsed,
        // because we just size a collapsed scrollbar to empty and some
        // descendants may be non-empty. Suppress that invalidation here.
        return;
      }
    }
  }
 
  nsHTMLContainerFrame::InvalidateInternal(aDamageRect, aX, aY, aForChild, aFlags);
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
  nsRect mContentsOverflowArea;
  PRPackedBool mReflowedContentsWithHScrollbar;
  PRPackedBool mReflowedContentsWithVScrollbar;

  // === Filled in when TryLayout succeeds ===
  // The size of the inside-border area
  nsSize mInsideBorderSize;
  // Whether we decided to show the horizontal scrollbar
  PRPackedBool mShowHScrollbar;
  // Whether we decided to show the vertical scrollbar
  PRPackedBool mShowVScrollbar;

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

  aState->mReflowState.ApplyMinMaxConstraints(&contentWidth, &contentHeight);
  return nsSize(contentWidth + aState->mReflowState.mComputedPadding.LeftRight(),
                contentHeight + aState->mReflowState.mComputedPadding.TopBottom());
}

static void
GetScrollbarMetrics(nsBoxLayoutState& aState, nsIBox* aBox, nsSize* aMin,
                    nsSize* aPref, PRBool aVertical)
{
  NS_ASSERTION(aState.GetRenderingContext(),
               "Must have rendering context in layout state for size "
               "computations");
  
  if (aMin) {
    *aMin = aBox->GetMinSize(aState);
  }
 
  if (aPref) {
    *aPref = aBox->GetPrefSize(aState);
  }
}

/**
 * Assuming that we know the metrics for our wrapped frame and
 * whether the horizontal and/or vertical scrollbars are present,
 * compute the resulting layout and return PR_TRUE if the layout is
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
 * @param aForce if PR_TRUE, then we just assume the layout is consistent.
 */
PRBool
nsHTMLScrollFrame::TryLayout(ScrollReflowState* aState,
                             nsHTMLReflowMetrics* aKidMetrics,
                             PRBool aAssumeHScroll, PRBool aAssumeVScroll,
                             PRBool aForce, nsresult* aResult)
{
  *aResult = NS_OK;

  if ((aState->mStyles.mVertical == NS_STYLE_OVERFLOW_HIDDEN && aAssumeVScroll) ||
      (aState->mStyles.mHorizontal == NS_STYLE_OVERFLOW_HIDDEN && aAssumeHScroll)) {
    NS_ASSERTION(!aForce, "Shouldn't be forcing a hidden scrollbar to show!");
    return PR_FALSE;
  }

  if (aAssumeVScroll != aState->mReflowedContentsWithVScrollbar ||
      (aAssumeHScroll != aState->mReflowedContentsWithHScrollbar &&
       ScrolledContentDependsOnHeight(aState))) {
    nsresult rv = ReflowScrolledFrame(aState, aAssumeHScroll, aAssumeVScroll,
                                      aKidMetrics, PR_FALSE);
    if (NS_FAILED(rv)) {
      *aResult = rv;
      return PR_FALSE;
    }
  }

  nsSize vScrollbarMinSize(0, 0);
  nsSize vScrollbarPrefSize(0, 0);
  if (mInner.mVScrollbarBox) {
    GetScrollbarMetrics(aState->mBoxState, mInner.mVScrollbarBox,
                        &vScrollbarMinSize,
                        aAssumeVScroll ? &vScrollbarPrefSize : nsnull, PR_TRUE);
  }
  nscoord vScrollbarDesiredWidth = aAssumeVScroll ? vScrollbarPrefSize.width : 0;
  nscoord vScrollbarMinHeight = aAssumeVScroll ? vScrollbarMinSize.height : 0;

  nsSize hScrollbarMinSize(0, 0);
  nsSize hScrollbarPrefSize(0, 0);
  if (mInner.mHScrollbarBox) {
    GetScrollbarMetrics(aState->mBoxState, mInner.mHScrollbarBox,
                        &hScrollbarMinSize,
                        aAssumeHScroll ? &hScrollbarPrefSize : nsnull, PR_FALSE);
  }
  nscoord hScrollbarDesiredHeight = aAssumeHScroll ? hScrollbarPrefSize.height : 0;
  nscoord hScrollbarMinWidth = aAssumeHScroll ? hScrollbarMinSize.width : 0;

  // First, compute our inside-border size and scrollport size
  // XXXldb Can we depend more on ComputeSize here?
  nsSize desiredInsideBorderSize;
  desiredInsideBorderSize.width = vScrollbarDesiredWidth +
    NS_MAX(aKidMetrics->width, hScrollbarMinWidth);
  desiredInsideBorderSize.height = hScrollbarDesiredHeight +
    NS_MAX(aKidMetrics->height, vScrollbarMinHeight);
  aState->mInsideBorderSize =
    ComputeInsideBorderSize(aState, desiredInsideBorderSize);
  nsSize scrollPortSize = nsSize(NS_MAX(0, aState->mInsideBorderSize.width - vScrollbarDesiredWidth),
                                 NS_MAX(0, aState->mInsideBorderSize.height - hScrollbarDesiredHeight));
                                                                                
  if (!aForce) {
    nsRect scrolledRect =
      mInner.GetScrolledRectInternal(aState->mContentsOverflowArea, scrollPortSize);
    nscoord oneDevPixel = aState->mBoxState.PresContext()->DevPixelsToAppUnits(1);

    // If the style is HIDDEN then we already know that aAssumeHScroll is PR_FALSE
    if (aState->mStyles.mHorizontal != NS_STYLE_OVERFLOW_HIDDEN) {
      PRBool wantHScrollbar =
        aState->mStyles.mHorizontal == NS_STYLE_OVERFLOW_SCROLL ||
        scrolledRect.XMost() >= scrollPortSize.width + oneDevPixel ||
        scrolledRect.x <= -oneDevPixel;
      if (aState->mInsideBorderSize.height < hScrollbarMinSize.height ||
          scrollPortSize.width < hScrollbarMinSize.width)
        wantHScrollbar = PR_FALSE;
      if (wantHScrollbar != aAssumeHScroll)
        return PR_FALSE;
    }

    // If the style is HIDDEN then we already know that aAssumeVScroll is PR_FALSE
    if (aState->mStyles.mVertical != NS_STYLE_OVERFLOW_HIDDEN) {
      PRBool wantVScrollbar =
        aState->mStyles.mVertical == NS_STYLE_OVERFLOW_SCROLL ||
        scrolledRect.YMost() >= scrollPortSize.height + oneDevPixel ||
        scrolledRect.y <= -oneDevPixel;
      if (aState->mInsideBorderSize.width < vScrollbarMinSize.width ||
          scrollPortSize.height < vScrollbarMinSize.height)
        wantVScrollbar = PR_FALSE;
      if (wantVScrollbar != aAssumeVScroll)
        return PR_FALSE;
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
  return PR_TRUE;
}

PRBool
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
                                       PRBool aAssumeHScroll,
                                       PRBool aAssumeVScroll,
                                       nsHTMLReflowMetrics* aMetrics,
                                       PRBool aFirstPass)
{
  // these could be NS_UNCONSTRAINEDSIZE ... NS_MIN arithmetic should
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
    nsSize hScrollbarPrefSize = 
      mInner.mHScrollbarBox->GetPrefSize(const_cast<nsBoxLayoutState&>(aState->mBoxState));
    if (computedHeight != NS_UNCONSTRAINEDSIZE)
      computedHeight = NS_MAX(0, computedHeight - hScrollbarPrefSize.height);
    computedMinHeight = NS_MAX(0, computedMinHeight - hScrollbarPrefSize.height);
    if (computedMaxHeight != NS_UNCONSTRAINEDSIZE)
      computedMaxHeight = NS_MAX(0, computedMaxHeight - hScrollbarPrefSize.height);
  }

  if (aAssumeVScroll) {
    nsSize vScrollbarPrefSize = 
      mInner.mVScrollbarBox->GetPrefSize(const_cast<nsBoxLayoutState&>(aState->mBoxState));
    availWidth = NS_MAX(0, availWidth - vScrollbarPrefSize.width);
  }

  // We're forcing the padding on our scrolled frame, so let it know what that
  // padding is.
  mInner.mScrolledFrame->
    SetProperty(nsGkAtoms::usedPaddingProperty,
                new nsMargin(aState->mReflowState.mComputedPadding),
                nsCSSOffsetState::DestroyMarginFunc);  
  
  nsPresContext* presContext = PresContext();
  // Pass PR_FALSE for aInit so we can pass in the correct padding
  nsHTMLReflowState kidReflowState(presContext, aState->mReflowState,
                                   mInner.mScrolledFrame,
                                   nsSize(availWidth, NS_UNCONSTRAINEDSIZE),
                                   -1, -1, PR_FALSE);
  kidReflowState.Init(presContext, -1, -1, nsnull,
                      &aState->mReflowState.mComputedPadding);
  kidReflowState.mFlags.mAssumingHScrollbar = aAssumeHScroll;
  kidReflowState.mFlags.mAssumingVScrollbar = aAssumeVScroll;
  kidReflowState.SetComputedHeight(computedHeight);
  kidReflowState.mComputedMinHeight = computedMinHeight;
  kidReflowState.mComputedMaxHeight = computedMaxHeight;

  // Temporarily set mHasHorizontalScrollbar/mHasVerticalScrollbar to
  // reflect our assumptions while we reflow the child.
  PRBool didHaveHorizontalScrollbar = mInner.mHasHorizontalScrollbar;
  PRBool didHaveVerticalScrollbar = mInner.mHasVerticalScrollbar;
  mInner.mHasHorizontalScrollbar = aAssumeHScroll;
  mInner.mHasVerticalScrollbar = aAssumeVScroll;

  nsReflowStatus status;
  nsresult rv = ReflowChild(mInner.mScrolledFrame, presContext, *aMetrics,
                            kidReflowState, 0, 0,
                            NS_FRAME_NO_MOVE_FRAME | NS_FRAME_NO_MOVE_VIEW, status);

  mInner.mHasHorizontalScrollbar = didHaveHorizontalScrollbar;
  mInner.mHasVerticalScrollbar = didHaveVerticalScrollbar;

  // Don't resize or position the view (if any) because we're going to resize
  // it to the correct size anyway in PlaceScrollArea. Allowing it to
  // resize here would size it to the natural height of the frame,
  // which will usually be different from the scrollport height;
  // invalidating the difference will cause unnecessary repainting.
  FinishReflowChild(mInner.mScrolledFrame, presContext,
                    &kidReflowState, *aMetrics, 0, 0,
                    NS_FRAME_NO_MOVE_FRAME | NS_FRAME_NO_MOVE_VIEW | NS_FRAME_NO_SIZE_VIEW);

  // XXX Some frames (e.g., nsObjectFrame, nsFrameFrame, nsTextFrame) don't bother
  // setting their mOverflowArea. This is wrong because every frame should
  // always set mOverflowArea. In fact nsObjectFrame and nsFrameFrame don't
  // support the 'outline' property because of this. Rather than fix the world
  // right now, just fix up the overflow area if necessary. Note that we don't
  // check HasOverflowRect() because it could be set even though the
  // overflow area doesn't include the frame bounds.
  aMetrics->mOverflowArea.UnionRect(aMetrics->mOverflowArea,
                                    nsRect(0, 0, aMetrics->width, aMetrics->height));

  aState->mContentsOverflowArea = aMetrics->mOverflowArea;
  aState->mReflowedContentsWithHScrollbar = aAssumeHScroll;
  aState->mReflowedContentsWithVScrollbar = aAssumeVScroll;
  
  return rv;
}

PRBool
nsHTMLScrollFrame::GuessHScrollbarNeeded(const ScrollReflowState& aState)
{
  if (aState.mStyles.mHorizontal != NS_STYLE_OVERFLOW_AUTO)
    // no guessing required
    return aState.mStyles.mHorizontal == NS_STYLE_OVERFLOW_SCROLL;

  return mInner.mHasHorizontalScrollbar;
}

PRBool
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

  // If this is the initial reflow, guess PR_FALSE because usually
  // we have very little content by then.
  if (InInitialReflow())
    return PR_FALSE;

  if (mInner.mIsRoot) {
    // For viewports, try getting a hint from global history
    // as to whether we had a vertical scrollbar last time.
    PRBool hint;
    nsresult rv = mInner.GetVScrollbarHintFromGlobalHistory(&hint);
    if (NS_SUCCEEDED(rv))
      return hint;
    // No hint. Assume that there will be a scrollbar; it seems to me
    // that 'most pages' do have a scrollbar, and anyway, it's cheaper
    // to do an extra reflow for the pages that *don't* need a
    // scrollbar (because on average they will have less content).
    return PR_TRUE;
  }

  // For non-viewports, just guess that we don't need a scrollbar.
  // XXX I wonder if statistically this is the right idea; I'm
  // basically guessing that there are a lot of overflow:auto DIVs
  // that get their intrinsic size and don't overflow
  return PR_FALSE;
}

PRBool
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
      GuessVScrollbarNeeded(*aState), &kidDesiredSize, PR_TRUE);
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
      mInner.GetScrolledRectInternal(kidDesiredSize.mOverflowArea, insideBorderSize);
    if (nsRect(nsPoint(0, 0), insideBorderSize).Contains(scrolledRect)) {
      // Let's pretend we had no scrollbars coming in here
      rv = ReflowScrolledFrame(aState, PR_FALSE, PR_FALSE,
                               &kidDesiredSize, PR_FALSE);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // Try vertical scrollbar settings that leave the vertical scrollbar unchanged.
  // Do this first because changing the vertical scrollbar setting is expensive,
  // forcing a reflow always.

  // Try leaving the horizontal scrollbar unchanged first. This will be more
  // efficient.
  if (TryLayout(aState, &kidDesiredSize, aState->mReflowedContentsWithHScrollbar,
                aState->mReflowedContentsWithVScrollbar, PR_FALSE, &rv))
    return NS_OK;
  if (TryLayout(aState, &kidDesiredSize, !aState->mReflowedContentsWithHScrollbar,
                aState->mReflowedContentsWithVScrollbar, PR_FALSE, &rv))
    return NS_OK;

  // OK, now try toggling the vertical scrollbar. The performance advantage
  // of trying the status-quo horizontal scrollbar state
  // does not exist here (we'll have to reflow due to the vertical scrollbar
  // change), so always try no horizontal scrollbar first.
  PRBool newVScrollbarState = !aState->mReflowedContentsWithVScrollbar;
  if (TryLayout(aState, &kidDesiredSize, PR_FALSE, newVScrollbarState, PR_FALSE, &rv))
    return NS_OK;
  if (TryLayout(aState, &kidDesiredSize, PR_TRUE, newVScrollbarState, PR_FALSE, &rv))
    return NS_OK;

  // OK, we're out of ideas. Try again enabling whatever scrollbars we can
  // enable and force the layout to stick even if it's inconsistent.
  // This just happens sometimes.
  TryLayout(aState, &kidDesiredSize,
            aState->mStyles.mHorizontal != NS_STYLE_OVERFLOW_HIDDEN,
            aState->mStyles.mVertical != NS_STYLE_OVERFLOW_HIDDEN,
            PR_TRUE, &rv);
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
  nsRect scrolledRect = mInner.GetScrolledRectInternal(aState.mContentsOverflowArea, portSize);
  scrolledArea.UnionRectIncludeEmpty(scrolledRect,
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
  scrolledFrame->FinishAndStoreOverflow(&scrolledArea,
                                        scrolledFrame->GetSize());

  // Note that making the view *exactly* the size of the scrolled area
  // is critical, since the view scrolling code uses the size of the
  // scrolled view to clamp scroll requests.
  // Normally the scrolledFrame won't have a view but in some cases it
  // might create its own.
  nsContainerFrame::SyncFrameViewAfterReflow(scrolledFrame->PresContext(),
                                             scrolledFrame,
                                             scrolledFrame->GetView(),
                                             &scrolledArea,
                                             0);
}

nscoord
nsHTMLScrollFrame::GetIntrinsicVScrollbarWidth(nsIRenderingContext *aRenderingContext)
{
  nsGfxScrollFrameInner::ScrollbarStyles ss = GetScrollbarStyles();
  if (ss.mVertical != NS_STYLE_OVERFLOW_SCROLL || !mInner.mVScrollbarBox)
    return 0;

  // Don't need to worry about reflow depth here since it's
  // just for scrollbars
  nsBoxLayoutState bls(PresContext(), aRenderingContext, 0);
  nsSize vScrollbarPrefSize(0, 0);
  GetScrollbarMetrics(bls, mInner.mVScrollbarBox,
                      nsnull, &vScrollbarPrefSize, PR_TRUE);
  return vScrollbarPrefSize.width;
}

/* virtual */ nscoord
nsHTMLScrollFrame::GetMinWidth(nsIRenderingContext *aRenderingContext)
{
  nscoord result = mInner.mScrolledFrame->GetMinWidth(aRenderingContext);
  DISPLAY_MIN_WIDTH(this, result);
  return result + GetIntrinsicVScrollbarWidth(aRenderingContext);
}

/* virtual */ nscoord
nsHTMLScrollFrame::GetPrefWidth(nsIRenderingContext *aRenderingContext)
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

PRBool
nsHTMLScrollFrame::IsCollapsed(nsBoxLayoutState& aBoxLayoutState)
{
  // We're never collapsed in the box sense.
  return PR_FALSE;
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
  PRBool reflowContents = PR_TRUE; // XXX Ignored
  PRBool reflowHScrollbar = PR_TRUE;
  PRBool reflowVScrollbar = PR_TRUE;
  PRBool reflowScrollCorner = PR_TRUE;
  if (!aReflowState.ShouldReflowAllKids()) {
    #define NEEDS_REFLOW(frame_) \
      ((frame_) && NS_SUBTREE_DIRTY(frame_))

    reflowContents = NEEDS_REFLOW(mInner.mScrolledFrame);
    reflowHScrollbar = NEEDS_REFLOW(mInner.mHScrollbarBox);
    reflowVScrollbar = NEEDS_REFLOW(mInner.mVScrollbarBox);
    reflowScrollCorner = NEEDS_REFLOW(mInner.mScrollCornerBox);

    #undef NEEDS_REFLOW
  }

  nsRect oldScrollAreaBounds = mInner.mScrollPort;
  nsRect oldScrolledAreaBounds =
    mInner.mScrolledFrame->GetOverflowRectRelativeToParent();
  // Adjust to a multiple of device pixels to restore the invariant that
  // oldScrollPosition is a multiple of device pixels. This could have been
  // thrown out by a zoom change.
  nsIntPoint ptDevPx;
  nsPoint oldScrollPosition =
    mInner.ClampAndRestrictToDevPixels(mInner.GetScrollPosition(), &ptDevPx);
  
  state.mComputedBorder = aReflowState.mComputedBorderPadding -
    aReflowState.mComputedPadding;

  nsresult rv = ReflowContents(&state, aDesiredSize);
  if (NS_FAILED(rv))
    return rv;
  
  PlaceScrollArea(state, oldScrollPosition);
  if (!mInner.mPostedReflowCallback) {
    // Make sure we'll try scrolling to restored position
    PresContext()->PresShell()->PostReflowCallback(&mInner);
    mInner.mPostedReflowCallback = PR_TRUE;
  }

  PRBool didHaveHScrollbar = mInner.mHasHorizontalScrollbar;
  PRBool didHaveVScrollbar = mInner.mHasVerticalScrollbar;
  mInner.mHasHorizontalScrollbar = state.mShowHScrollbar;
  mInner.mHasVerticalScrollbar = state.mShowVScrollbar;
  nsRect newScrollAreaBounds = mInner.mScrollPort;
  nsRect newScrolledAreaBounds =
    mInner.mScrolledFrame->GetOverflowRectRelativeToParent();
  if (mInner.mSkippedScrollbarLayout ||
      reflowHScrollbar || reflowVScrollbar || reflowScrollCorner ||
      (GetStateBits() & NS_FRAME_IS_DIRTY) ||
      didHaveHScrollbar != state.mShowHScrollbar ||
      didHaveVScrollbar != state.mShowVScrollbar ||
      oldScrollAreaBounds != newScrollAreaBounds ||
      oldScrolledAreaBounds != newScrolledAreaBounds) {
    if (!mInner.mSupppressScrollbarUpdate) {
      mInner.mSkippedScrollbarLayout = PR_FALSE;
      mInner.SetScrollbarVisibility(mInner.mHScrollbarBox, state.mShowHScrollbar);
      mInner.SetScrollbarVisibility(mInner.mVScrollbarBox, state.mShowVScrollbar);
      // place and reflow scrollbars
      nsRect insideBorderArea =
        nsRect(nsPoint(state.mComputedBorder.left, state.mComputedBorder.top),
               state.mInsideBorderSize);
      mInner.LayoutScrollbars(state.mBoxState, insideBorderArea,
                              oldScrollAreaBounds);
    } else {
      mInner.mSkippedScrollbarLayout = PR_TRUE;
    }
  }

  aDesiredSize.width = state.mInsideBorderSize.width +
    state.mComputedBorder.LeftRight();
  aDesiredSize.height = state.mInsideBorderSize.height +
    state.mComputedBorder.TopBottom();

  aDesiredSize.mOverflowArea = nsRect(0, 0, aDesiredSize.width, aDesiredSize.height);

  CheckInvalidateSizeChange(aDesiredSize);

  FinishAndStoreOverflow(&aDesiredSize);

  if (!InInitialReflow() && !mInner.mHadNonInitialReflow) {
    mInner.mHadNonInitialReflow = PR_TRUE;
    if (mInner.mIsRoot) {
      // For viewports, record whether we needed a vertical scrollbar
      // after the first non-initial reflow.
      mInner.SaveVScrollbarStateToGlobalHistory();
    }
  }

  if (mInner.mIsRoot && oldScrolledAreaBounds != newScrolledAreaBounds) {
    mInner.PostScrolledAreaEvent();
  }

  aStatus = NS_FRAME_COMPLETE;
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  mInner.PostOverflowEvent();
  return rv;
}


////////////////////////////////////////////////////////////////////////////////

#ifdef NS_DEBUG
NS_IMETHODIMP
nsHTMLScrollFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("HTMLScroll"), aResult);
}
#endif

#ifdef ACCESSIBILITY
NS_IMETHODIMP nsHTMLScrollFrame::GetAccessible(nsIAccessible** aAccessible)
{
  *aAccessible = nsnull;
  if (!IsFocusable()) {
    return NS_OK;
  }
  // Focusable via CSS, so needs to be in accessibility hierarchy
  nsCOMPtr<nsIAccessibilityService> accService = do_GetService("@mozilla.org/accessibilityService;1");

  if (accService) {
    return accService->CreateHTMLGenericAccessible(static_cast<nsIFrame*>(this), aAccessible);
  }

  return NS_ERROR_FAILURE;
}
#endif

NS_QUERYFRAME_HEAD(nsHTMLScrollFrame)
  NS_QUERYFRAME_ENTRY(nsIAnonymousContentCreator)
  NS_QUERYFRAME_ENTRY(nsIScrollableFrame)
  NS_QUERYFRAME_ENTRY(nsIStatefulFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsHTMLContainerFrame)

//----------nsXULScrollFrame-------------------------------------------

nsIFrame*
NS_NewXULScrollFrame(nsIPresShell* aPresShell, nsStyleContext* aContext, PRBool aIsRoot)
{
  return new (aPresShell) nsXULScrollFrame(aPresShell, aContext, aIsRoot);
}

NS_IMPL_FRAMEARENA_HELPERS(nsXULScrollFrame)

nsXULScrollFrame::nsXULScrollFrame(nsIPresShell* aShell, nsStyleContext* aContext, PRBool aIsRoot)
  : nsBoxFrame(aShell, aContext, aIsRoot),
    mInner(this, aIsRoot, PR_TRUE)
{
    SetLayoutManager(nsnull);
}

nsMargin nsGfxScrollFrameInner::GetDesiredScrollbarSizes(nsBoxLayoutState* aState) {
  NS_ASSERTION(aState && aState->GetRenderingContext(),
               "Must have rendering context in layout state for size "
               "computations");
  
  nsMargin result(0, 0, 0, 0);

  if (mVScrollbarBox) {
    nsSize size = mVScrollbarBox->GetPrefSize(*aState);
    if (IsScrollbarOnRight())
      result.left = size.width;
    else
      result.right = size.width;
  }

  if (mHScrollbarBox) {
    nsSize size = mHScrollbarBox->GetPrefSize(*aState);
    // We don't currently support any scripts that would require a scrollbar
    // at the top. (Are there any?)
    result.bottom = size.height;
  }

  return result;
}

nsresult
nsXULScrollFrame::CreateAnonymousContent(nsTArray<nsIContent*>& aElements)
{
  return mInner.CreateAnonymousContent(aElements);
}

void
nsXULScrollFrame::GetAnonymousContent(nsBaseContentList& aElements)
{
  mInner.GetAnonymousContent(aElements);
}

void
nsXULScrollFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  mInner.Destroy();
  nsBoxFrame::DestroyFrom(aDestructRoot);
}

NS_IMETHODIMP
nsXULScrollFrame::SetInitialChildList(nsIAtom*        aListName,
                                      nsFrameList&    aChildList)
{
  nsresult rv = nsBoxFrame::SetInitialChildList(aListName, aChildList);
  mInner.ReloadChildFrames();
  return rv;
}


NS_IMETHODIMP
nsXULScrollFrame::AppendFrames(nsIAtom*        aListName,
                               nsFrameList&    aFrameList)
{
  nsresult rv = nsBoxFrame::AppendFrames(aListName, aFrameList);
  mInner.ReloadChildFrames();
  return rv;
}

NS_IMETHODIMP
nsXULScrollFrame::InsertFrames(nsIAtom*        aListName,
                               nsIFrame*       aPrevFrame,
                               nsFrameList&    aFrameList)
{
  nsresult rv = nsBoxFrame::InsertFrames(aListName, aPrevFrame, aFrameList);
  mInner.ReloadChildFrames();
  return rv;
}

NS_IMETHODIMP
nsXULScrollFrame::RemoveFrame(nsIAtom*        aListName,
                              nsIFrame*       aOldFrame)
{
  nsresult rv = nsBoxFrame::RemoveFrame(aListName, aOldFrame);
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

PRIntn
nsXULScrollFrame::GetSkipSides() const
{
  return 0;
}

nsIAtom*
nsXULScrollFrame::GetType() const
{
  return nsGkAtoms::scrollFrame; 
}

void
nsXULScrollFrame::InvalidateInternal(const nsRect& aDamageRect,
                                     nscoord aX, nscoord aY, nsIFrame* aForChild,
                                     PRUint32 aFlags)
{
  if (aForChild == mInner.mScrolledFrame) {
    // restrict aDamageRect to the scrollable view's bounds
    nsRect r;
    if (r.IntersectRect(aDamageRect + nsPoint(aX, aY), mInner.mScrollPort)) {
      nsBoxFrame::InvalidateInternal(r, 0, 0, aForChild, aFlags);
    }
    return;
  }
  
  nsBoxFrame::InvalidateInternal(aDamageRect, aX, aY, aForChild, aFlags);
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
    pref.width += vSize.width;
  }
   
  if (mInner.mHScrollbarBox &&
      styles.mHorizontal == NS_STYLE_OVERFLOW_SCROLL) {
    nsSize hSize = mInner.mHScrollbarBox->GetPrefSize(aState);
    pref.height += hSize.height;
  }

  AddBorderAndPadding(pref);
  nsIBox::AddCSSPrefSize(aState, this, pref);
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
     min.width += vSize.width;
     if (min.height < vSize.height)
        min.height = vSize.height;
  }
        
  if (mInner.mHScrollbarBox &&
      styles.mHorizontal == NS_STYLE_OVERFLOW_SCROLL) {
     nsSize hSize = mInner.mHScrollbarBox->GetMinSize(aState);
     min.height += hSize.height;
     if (min.width < hSize.width)
        min.width = hSize.width;
  }

  AddBorderAndPadding(min);
  nsIBox::AddCSSMinSize(aState, this, min);
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
  nsIBox::AddCSSMaxSize(aState, this, maxSize);
  return maxSize;
}

#if 0 // XXXldb I don't think this is even needed
/* virtual */ nscoord
nsXULScrollFrame::GetMinWidth(nsIRenderingContext *aRenderingContext)
{
  nsStyleUnit widthUnit = GetStylePosition()->mWidth.GetUnit();
  if (widthUnit == eStyleUnit_Percent || widthUnit == eStyleUnit_Auto) {
    nsMargin border = aReflowState.mComputedBorderPadding;
    aDesiredSize.mMaxElementWidth = border.right + border.left;
    mMaxElementWidth = aDesiredSize.mMaxElementWidth;
  } else {
    NS_NOTYETIMPLEMENTED("Use the info from the scrolled frame");
#if 0
    // if not set then use the cached size. If set then set it.
    if (aDesiredSize.mMaxElementWidth == -1)
      aDesiredSize.mMaxElementWidth = mMaxElementWidth;
    else
      mMaxElementWidth = aDesiredSize.mMaxElementWidth;
#endif
  }
  return 0;
}
#endif

#ifdef NS_DEBUG
NS_IMETHODIMP
nsXULScrollFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("XULScroll"), aResult);
}
#endif

NS_IMETHODIMP
nsXULScrollFrame::DoLayout(nsBoxLayoutState& aState)
{
  PRUint32 flags = aState.LayoutFlags();
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

#define SMOOTH_SCROLL_MSECS_PER_FRAME 10
#define SMOOTH_SCROLL_FRAMES    10

#define SMOOTH_SCROLL_PREF_NAME "general.smoothScroll"

class nsGfxScrollFrameInner::AsyncScroll {
public:
  AsyncScroll() {}
  ~AsyncScroll() {
    if (mScrollTimer) mScrollTimer->Cancel();
  }

  nsCOMPtr<nsITimer> mScrollTimer;
  PRInt32 mVelocities[SMOOTH_SCROLL_FRAMES*2];
  PRInt32 mFrameIndex;
  PRPackedBool mIsSmoothScroll;
};

static void ComputeVelocities(PRInt32 aCurVelocity, nscoord aCurPos, nscoord aDstPos,
                              PRInt32* aVelocities, PRInt32 aP2A)
{
  // scrolling always works in units of whole pixels. So compute velocities
  // in pixels and then scale them up. This ensures, for example, that
  // a 1-pixel scroll isn't broken into N frames of 1/N pixels each, each
  // frame increment being rounded to 0 whole pixels.
  aCurPos = NSAppUnitsToIntPixels(aCurPos, aP2A);
  aDstPos = NSAppUnitsToIntPixels(aDstPos, aP2A);

  PRInt32 i;
  PRInt32 direction = (aCurPos < aDstPos ? 1 : -1);
  PRInt32 absDelta = (aDstPos - aCurPos)*direction;
  PRInt32 baseVelocity = absDelta/SMOOTH_SCROLL_FRAMES;

  for (i = 0; i < SMOOTH_SCROLL_FRAMES; i++) {
    aVelocities[i*2] = baseVelocity;
  }
  nscoord total = baseVelocity*SMOOTH_SCROLL_FRAMES;
  for (i = 0; i < SMOOTH_SCROLL_FRAMES; i++) {
    if (total < absDelta) {
      aVelocities[i*2]++;
      total++;
    }
  }
  NS_ASSERTION(total == absDelta, "Invalid velocity sum");

  PRInt32 scale = NSIntPixelsToAppUnits(direction, aP2A);
  for (i = 0; i < SMOOTH_SCROLL_FRAMES; i++) {
    aVelocities[i*2] *= scale;
  }
}

static PRBool
IsSmoothScrollingEnabled()
{
  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs) {
    PRBool enabled;
    nsresult rv = prefs->GetBoolPref(SMOOTH_SCROLL_PREF_NAME, &enabled);
    if (NS_SUCCEEDED(rv)) {
      return enabled;
    }
  }
  return PR_FALSE;
}

nsGfxScrollFrameInner::nsGfxScrollFrameInner(nsContainerFrame* aOuter,
                                             PRBool aIsRoot,
                                             PRBool aIsXUL)
  : mHScrollbarBox(nsnull),
    mVScrollbarBox(nsnull),
    mScrolledFrame(nsnull),
    mScrollCornerBox(nsnull),
    mOuter(aOuter),
    mAsyncScroll(nsnull),
    mDestination(0, 0),
    mRestorePos(-1, -1),
    mLastPos(-1, -1),
    mNeverHasVerticalScrollbar(PR_FALSE),
    mNeverHasHorizontalScrollbar(PR_FALSE),
    mHasVerticalScrollbar(PR_FALSE), 
    mHasHorizontalScrollbar(PR_FALSE),
    mFrameIsUpdatingScrollbar(PR_FALSE),
    mDidHistoryRestore(PR_FALSE),
    mIsRoot(aIsRoot),
    mIsXUL(aIsXUL),
    mSupppressScrollbarUpdate(PR_FALSE),
    mSkippedScrollbarLayout(PR_FALSE),
    mDidLoadHistoryVScrollbarHint(PR_FALSE),
    mHistoryVScrollbarHint(PR_FALSE),
    mHadNonInitialReflow(PR_FALSE),
    mHorizontalOverflow(PR_FALSE),
    mVerticalOverflow(PR_FALSE),
    mPostedReflowCallback(PR_FALSE),
    mMayHaveDirtyFixedChildren(PR_FALSE),
    mUpdateScrollbarAttributes(PR_FALSE)
{
}

nsGfxScrollFrameInner::~nsGfxScrollFrameInner()
{
  delete mAsyncScroll;
}

static nscoord
Clamp(nscoord aLower, nscoord aVal, nscoord aUpper)
{
  if (aVal < aLower)
    return aLower;
  if (aVal > aUpper)
    return aUpper;
  return aVal;
}

nsPoint
nsGfxScrollFrameInner::ClampScrollPosition(const nsPoint& aPt) const
{
  nsRect range = GetScrollRange();
  return nsPoint(Clamp(range.x, aPt.x, range.XMost()),
                 Clamp(range.y, aPt.y, range.YMost()));
}

/*
 * Callback function from timer used in nsGfxScrollFrameInner::ScrollTo
 */
void
nsGfxScrollFrameInner::AsyncScrollCallback(nsITimer *aTimer, void* anInstance)
{
  nsGfxScrollFrameInner* self = static_cast<nsGfxScrollFrameInner*>(anInstance);
  if (!self || !self->mAsyncScroll)
    return;

  if (self->mAsyncScroll->mIsSmoothScroll) {
    // XXX this is crappy, the scroll position needs to be based on the
    // current time
    NS_ASSERTION(self->mAsyncScroll->mFrameIndex < SMOOTH_SCROLL_FRAMES,
                 "Past last frame?");
    nscoord* velocities =
      &self->mAsyncScroll->mVelocities[self->mAsyncScroll->mFrameIndex*2];
    nsPoint destination =
      self->GetScrollPosition() + nsPoint(velocities[0], velocities[1]);        

    self->mAsyncScroll->mFrameIndex++;
    if (self->mAsyncScroll->mFrameIndex >= SMOOTH_SCROLL_FRAMES) {
      delete self->mAsyncScroll;
      self->mAsyncScroll = nsnull;
    }

    self->ScrollToImpl(destination);
    // 'self' may be a dangling pointer here since ScrollToImpl may have destroyed it
  } else {
    delete self->mAsyncScroll;
    self->mAsyncScroll = nsnull;

    self->ScrollToImpl(self->mDestination);
    // 'self' may be a dangling pointer here since ScrollToImpl may have destroyed it
  }
}

/*
 * this method wraps calls to ScrollToImpl(), either in one shot or incrementally,
 *  based on the setting of the smooth scroll pref
 */
void
nsGfxScrollFrameInner::ScrollTo(nsPoint aScrollPosition,
                                nsIScrollableFrame::ScrollMode aMode)
{
  mDestination = ClampScrollPosition(aScrollPosition);

  if (aMode == nsIScrollableFrame::INSTANT) {
    // Asynchronous scrolling is not allowed, so we'll kill any existing
    // async-scrolling process and do an instant scroll
    delete mAsyncScroll;
    mAsyncScroll = nsnull;
    ScrollToImpl(mDestination);
    return;
  }

  PRInt32 currentVelocityX = 0;
  PRInt32 currentVelocityY = 0;
  PRBool isSmoothScroll = IsSmoothScrollingEnabled();

  if (mAsyncScroll) {
    if (mAsyncScroll->mIsSmoothScroll) {
      currentVelocityX = mAsyncScroll->mVelocities[mAsyncScroll->mFrameIndex*2];
      currentVelocityY = mAsyncScroll->mVelocities[mAsyncScroll->mFrameIndex*2 + 1];
    }
  } else {
    mAsyncScroll = new AsyncScroll;
    if (mAsyncScroll) {
      mAsyncScroll->mScrollTimer = do_CreateInstance("@mozilla.org/timer;1");
      if (!mAsyncScroll->mScrollTimer) {
        delete mAsyncScroll;
        mAsyncScroll = nsnull;
      }
    }
    if (!mAsyncScroll) {
      // some allocation failed. Scroll the normal way.
      ScrollToImpl(mDestination);
      return;
    }
    if (isSmoothScroll) {
      mAsyncScroll->mScrollTimer->InitWithFuncCallback(
        AsyncScrollCallback, this, SMOOTH_SCROLL_MSECS_PER_FRAME,
        nsITimer::TYPE_REPEATING_PRECISE);
    } else {
      mAsyncScroll->mScrollTimer->InitWithFuncCallback(
        AsyncScrollCallback, this, 0, nsITimer::TYPE_ONE_SHOT);
    }
  }

  mAsyncScroll->mFrameIndex = 0;
  mAsyncScroll->mIsSmoothScroll = isSmoothScroll;

  if (isSmoothScroll) {
    PRInt32 p2a = mOuter->PresContext()->AppUnitsPerDevPixel();

    // compute velocity vectors
    nsPoint currentPos = GetScrollPosition();
    ComputeVelocities(currentVelocityX, currentPos.x, mDestination.x,
                      mAsyncScroll->mVelocities, p2a);
    ComputeVelocities(currentVelocityY, currentPos.y, mDestination.y,
                      mAsyncScroll->mVelocities + 1, p2a);
  }
}

static void InvalidateWidgets(nsIView* aView)
{
  if (aView->HasWidget()) {
    nsIWidget* widget = aView->GetWidget();
    nsWindowType type;
    widget->GetWindowType(type);
    if (type != eWindowType_popup) {
      // Force the widget and everything in it to repaint. We can't
      // just use Invalidate because the widget might have child
      // widgets and they wouldn't get updated. We can't call
      // UpdateView(aView) because the area to be repainted might be
      // outside aView's clipped bounds. This isn't the greatest way
      // to achieve this, perhaps, but it works.
      widget->Show(PR_FALSE);
      widget->Show(PR_TRUE);
    }
    return;
  }

  for (nsIView* v = aView->GetFirstChild(); v; v = v->GetNextSibling()) {
    InvalidateWidgets(v);
  }
}

// We can't use nsContainerFrame::PositionChildViews here because
// we don't want to invalidate views that have moved.
// aInvalidateWidgets is set to true if we should invalidate the area
// covered by every widget in the subtree.
static void AdjustViewsAndWidgets(nsIFrame* aFrame,
                                  PRBool aInvalidateWidgets)
{
  nsIView* view = aFrame->GetView();
  if (view) {
    nsPoint pt;
    aFrame->GetParent()->GetClosestView(&pt);
    pt += aFrame->GetPosition();
    view->SetPosition(pt.x, pt.y);

    if (aInvalidateWidgets) {
      InvalidateWidgets(view);
    }
    return;
  }

  if (!(aFrame->GetStateBits() & NS_FRAME_HAS_CHILD_WITH_VIEW)) {
    return;
  }

  nsIAtom* childListName = nsnull;
  PRInt32  childListIndex = 0;
  do {
    // Recursively walk aFrame's child frames
    nsIFrame* childFrame = aFrame->GetFirstChild(childListName);
    while (childFrame) {
      AdjustViewsAndWidgets(childFrame, aInvalidateWidgets);

      // Get the next sibling child frame
      childFrame = childFrame->GetNextSibling();
    }

    // also process the additional child lists, but skip the popup list as the
    // views for popups are not scrolled.
    do {
      childListName = aFrame->GetAdditionalChildListName(childListIndex++);
    } while (childListName == nsGkAtoms::popupList);
  } while (childListName);
}

/**
 * Given aBlitRegion in appunits, create and return an nsRegion in
 * device pixels that represents the device pixels whose centers are
 * contained in aBlitRegion. Whatever appunit area was removed from
 * aBlitRegion in that process is added to aRepaintRegion. An appunits
 * version of the result is placed in aAppunitsBlitRegion.
 * 
 * We convert the blit region to pixels this way because in general
 * frames touch the pixels whose centers are contained in the
 * (possibly not pixel-aligned) frame bounds.
 */
static void
ConvertBlitRegionToPixelRects(const nsRegion& aBlitRegion,
                              nscoord aAppUnitsPerPixel,
                              nsTArray<nsIntRect>* aPixelRects,
                              nsRegion* aRepaintRegion,
                              nsRegion* aAppunitsBlitRegion)
{
  const nsRect* r;

  aPixelRects->Clear();
  aAppunitsBlitRegion->SetEmpty();
  // The rectangles in aBlitRegion don't overlap so converting them via
  // ToNearestPixels also produces a sequence of non-overlapping rectangles
  for (nsRegionRectIterator iter(aBlitRegion); (r = iter.Next());) {
    nsIntRect pixRect = r->ToNearestPixels(aAppUnitsPerPixel);
    aPixelRects->AppendElement(pixRect);
    aAppunitsBlitRegion->Or(*aAppunitsBlitRegion,
                            pixRect.ToAppUnits(aAppUnitsPerPixel));
  }

  nsRegion repaint;
  repaint.Sub(aBlitRegion, *aAppunitsBlitRegion);
  aRepaintRegion->Or(*aRepaintRegion, repaint);
}

/**
 * An nsTArray comparator that lets us sort nsIntRects by their right edge.
 */
class RightEdgeComparator {
public:
  /** @return True if the elements are equals; false otherwise. */
  PRBool Equals(const nsIntRect& aA, const nsIntRect& aB) const
  {
    return aA.XMost() == aB.XMost();
  }
  /** @return True if (a < b); false otherwise. */
  PRBool LessThan(const nsIntRect& aA, const nsIntRect& aB) const
  {
    return aA.XMost() < aB.XMost();
  }
};

// If aPixDelta has a negative component, flip aRect across the
// axis in that direction. We do this so we can assume all scrolling is
// down and to the right to simplify SortBlitRectsForCopy
static nsIntRect
FlipRect(const nsIntRect& aRect, nsIntPoint aPixDelta)
{
  nsIntRect r = aRect;
  if (aPixDelta.x < 0) {
    r.x = -r.XMost();
  }
  if (aPixDelta.y < 0) {
    r.y = -r.YMost();
  }
  return r;
}

// Sort aRects so that moving rectangle aRects[i] - aPixDelta to aRects[i]
// will not cause the rectangle to overlap any rectangles that haven't
// moved yet.
// See http://weblogs.mozillazine.org/roc/archives/2009/08/homework_answer.html
static void
SortBlitRectsForCopy(nsIntPoint aPixDelta, nsTArray<nsIntRect>* aRects)
{
  nsTArray<nsIntRect> rects;

  for (PRUint32 i = 0; i < aRects->Length(); ++i) {
    nsIntRect* r = &aRects->ElementAt(i);
    nsIntRect rect =
      FlipRect(nsIntRect(r->x, r->y, r->width, r->height), aPixDelta);
    rects.AppendElement(rect);
  }
  rects.Sort(RightEdgeComparator());

  aRects->Clear();
  // This could probably be improved a bit for some worst-case scenarios.
  // But in common cases this should be very fast, and we shouldn't
  // make it more complex unless we really need to.
  while (!rects.IsEmpty()) {
    PRInt32 i = rects.Length() - 1;
    PRBool overlappedBelow;
    do {
      overlappedBelow = PR_FALSE;
      const nsIntRect& rectI = rects[i];
      // see if any rectangle < i overlaps rectI horizontally and is below
      // rectI
      for (PRInt32 j = i - 1; j >= 0; --j) {
        if (rects[j].XMost() <= rectI.x) {
          // No rectangle with index <= j can overlap rectI horizontally
          break;
        }
        // Rectangle j overlaps rectI horizontally.
        if (rects[j].y >= rectI.y) {
          // Rectangle j is below rectangle i. This is the rightmost such
          // rectangle, so set i to this rectangle and continue.
          i = j;
          overlappedBelow = PR_TRUE;
          break;
        }
      }
    } while (overlappedBelow); 

    // Rectangle i has no rectangles to the right or below.
    // Flip it back before saving the result.
    aRects->AppendElement(FlipRect(rects[i], aPixDelta));
    rects.RemoveElementAt(i);
  }
}

static PRBool
CanScrollWithBlitting(nsIFrame* aFrame)
{
  for (nsIFrame* f = aFrame; f; f = nsLayoutUtils::GetCrossDocParentFrame(f)) {
    if (f->GetStyleDisplay()->HasTransform()) {
      return PR_FALSE;
    }
#ifdef MOZ_SVG
    if (nsSVGIntegrationUtils::UsingEffectsForFrame(f) ||
        f->IsFrameOfType(nsIFrame::eSVG)) {
      return PR_FALSE;
    }
#endif
  }
  return PR_TRUE;
}

void nsGfxScrollFrameInner::ScrollVisual(nsIntPoint aPixDelta)
{
  nsRootPresContext* rootPresContext =
    mOuter->PresContext()->GetRootPresContext();
  if (!rootPresContext) {
    return;
  }

  nsPoint offsetToView;
  nsPoint offsetToWidget;
  nsIWidget* nearestWidget =
    mOuter->GetClosestView(&offsetToView)->GetNearestWidget(&offsetToWidget);
  nsPoint nearestWidgetOffset = offsetToView + offsetToWidget;

  nsTArray<nsIWidget::Configuration> configurations;
  // Only update plugin configurations if we're going to scroll the
  // root widget. Otherwise we must be in a popup or some other situation
  // where we don't actually support windows plugins.
  if (rootPresContext->FrameManager()->GetRootFrame()->GetWindow() == nearestWidget) {
    rootPresContext->GetPluginGeometryUpdates(mOuter, &configurations);
  }

  if (!nearestWidget ||
      nearestWidget->GetTransparencyMode() == eTransparencyTransparent ||
      !CanScrollWithBlitting(mOuter)) {
    // Just invalidate the frame and adjust child widgets
    // Recall that our widget's origin is at our bounds' top-left
    if (nearestWidget) {
      nearestWidget->ConfigureChildren(configurations);
    }
    AdjustViewsAndWidgets(mScrolledFrame, PR_FALSE);
    // We need to call this after fixing up the widget and view positions
    // to be consistent with the view and frame hierarchy.
    mOuter->InvalidateWithFlags(mScrollPort,
                                nsIFrame::INVALIDATE_REASON_SCROLL_REPAINT);
  } else {
    nsIFrame* displayRoot = nsLayoutUtils::GetDisplayRootFrame(mOuter);
    nsRegion blitRegion;
    nsRegion repaintRegion;
    nsPresContext* presContext = mOuter->PresContext();
    nsPoint delta(presContext->DevPixelsToAppUnits(aPixDelta.x),
                  presContext->DevPixelsToAppUnits(aPixDelta.y));
    nsPoint offsetToDisplayRoot = mOuter->GetOffsetTo(displayRoot);
    nscoord appUnitsPerDevPixel = presContext->AppUnitsPerDevPixel();
    nsRect scrollPort =
      (mScrollPort + offsetToDisplayRoot).ToNearestPixels(appUnitsPerDevPixel).
      ToAppUnits(appUnitsPerDevPixel);
    nsresult rv =
      nsLayoutUtils::ComputeRepaintRegionForCopy(displayRoot, mScrolledFrame,
                                                 delta, scrollPort,
                                                 &blitRegion,
                                                 &repaintRegion);
    if (NS_FAILED(rv)) {
      nearestWidget->ConfigureChildren(configurations);
      AdjustViewsAndWidgets(mScrolledFrame, PR_FALSE);
      mOuter->InvalidateWithFlags(mScrollPort,
                                  nsIFrame::INVALIDATE_REASON_SCROLL_REPAINT);
      return;
    }

    blitRegion.MoveBy(nearestWidgetOffset - offsetToDisplayRoot);
    repaintRegion.MoveBy(nearestWidgetOffset - offsetToDisplayRoot);

    // We're going to bit-blit.  Let the viewmanager know so it can
    // adjust dirty regions appropriately.
    nsIView* view = displayRoot->GetView();
    NS_ASSERTION(view, "Display root has no view?");
    nsIViewManager* vm = view->GetViewManager();
    vm->WillBitBlit(view, mScrollPort + offsetToDisplayRoot, -delta);

    // innerPixRegion is in device pixels
    nsTArray<nsIntRect> blitRects;
    nsRegion blitRectsRegion;
    ConvertBlitRegionToPixelRects(blitRegion,
                                  appUnitsPerDevPixel,
                                  &blitRects, &repaintRegion,
                                  &blitRectsRegion);
    SortBlitRectsForCopy(aPixDelta, &blitRects);

    nearestWidget->Scroll(aPixDelta, blitRects, configurations);
    AdjustViewsAndWidgets(mScrolledFrame, PR_TRUE);
    repaintRegion.MoveBy(-nearestWidgetOffset + offsetToDisplayRoot);

    {
      // Block script execution. This suppresses event dispatching in
      // PresShell::HandleEvent. We need to do this because Windows
      // is evil and can dispatch WM_MOUSEACTIVATE messages during
      // our call to ::UpdateWindow (in the presence of out-of-process
      // plugins, it seems). We are not able to handle event dispatch
      // here.
      // No script runners should be added as we paint!
      nsContentUtils::AddScriptBlockerAndPreventAddingRunners();
      vm->UpdateViewAfterScroll(view, repaintRegion);
      nsContentUtils::RemoveScriptBlocker();
      // Nothing should run here on removing the blocker, since we
      // prevented the addition of any script runners.
    }

    nsIFrame* presContextRootFrame = presContext->FrameManager()->GetRootFrame();
    if (nearestWidget == presContextRootFrame->GetWindow()) {
      nsPoint offsetToPresContext = mOuter->GetOffsetTo(presContextRootFrame);
      blitRectsRegion.MoveBy(-nearestWidgetOffset + offsetToPresContext);
      repaintRegion.MoveBy(-offsetToDisplayRoot + offsetToPresContext);
      presContext->NotifyInvalidateForScrolling(blitRectsRegion, repaintRegion);
    }
  }
}

static PRInt32
ClampInt(nscoord aLower, nscoord aVal, nscoord aUpper, nscoord aAppUnitsPerPixel)
{
  PRInt32 low = NSToIntCeil(float(aLower)/aAppUnitsPerPixel);
  PRInt32 high = NSToIntFloor(float(aUpper)/aAppUnitsPerPixel);
  PRInt32 v = NSToIntRound(float(aVal)/aAppUnitsPerPixel);
  NS_ASSERTION(low <= high, "No integers in range; 0 is supposed to be in range");
  if (v < low)
    return low;
  if (v > high)
    return high;
  return v;
}

nsPoint
nsGfxScrollFrameInner::ClampAndRestrictToDevPixels(const nsPoint& aPt,
                                                   nsIntPoint* aPtDevPx) const
{
  nsPresContext* presContext = mOuter->PresContext();
  nscoord appUnitsPerDevPixel = presContext->AppUnitsPerDevPixel();
  // Convert to device pixels so we scroll to an integer offset of device
  // pixels. But we also need to make sure that our position remains
  // inside the allowed region.
  nsRect scrollRange = GetScrollRange();
  *aPtDevPx = nsIntPoint(ClampInt(scrollRange.x, aPt.x, scrollRange.XMost(), appUnitsPerDevPixel),
                         ClampInt(scrollRange.y, aPt.y, scrollRange.YMost(), appUnitsPerDevPixel));
  return nsPoint(NSIntPixelsToAppUnits(aPtDevPx->x, appUnitsPerDevPixel),
                 NSIntPixelsToAppUnits(aPtDevPx->y, appUnitsPerDevPixel));
}

void
nsGfxScrollFrameInner::ScrollToImpl(nsPoint aPt)
{
  nsPresContext* presContext = mOuter->PresContext();
  nscoord appUnitsPerDevPixel = presContext->AppUnitsPerDevPixel();
  nsIntPoint ptDevPx;
  nsPoint pt = ClampAndRestrictToDevPixels(aPt, &ptDevPx);

  nsPoint curPos = GetScrollPosition();
  if (pt == curPos) {
    return;
  }
  nsIntPoint curPosDevPx(NSAppUnitsToIntPixels(curPos.x, appUnitsPerDevPixel),
                         NSAppUnitsToIntPixels(curPos.y, appUnitsPerDevPixel));
  // We maintain the invariant that the scroll position is a multiple of device
  // pixels.
  NS_ASSERTION(curPosDevPx.x*appUnitsPerDevPixel == curPos.x,
               "curPos.x not a multiple of device pixels");
  NS_ASSERTION(curPosDevPx.y*appUnitsPerDevPixel == curPos.y,
               "curPos.y not a multiple of device pixels");

  // notify the listeners.
  for (PRInt32 i = 0; i < mListeners.Length(); i++) {
    mListeners[i]->ScrollPositionWillChange(pt.x, pt.y);
  }
  
  // Update frame position for scrolling
  mScrolledFrame->SetPosition(mScrollPort.TopLeft() - pt);

  // We pass in the amount to move visually
  ScrollVisual(curPosDevPx - ptDevPx);

  presContext->PresShell()->GetViewManager()->SynthesizeMouseMove(PR_TRUE);
  UpdateScrollbarPosition();
  PostScrollEvent();

  // notify the listeners.
  for (PRInt32 i = 0; i < mListeners.Length(); i++) {
    mListeners[i]->ScrollPositionDidChange(pt.x, pt.y);
  }
}

nsresult
nsGfxScrollFrameInner::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                        const nsRect&           aDirtyRect,
                                        const nsDisplayListSet& aLists)
{
  nsresult rv = mOuter->DisplayBorderBackgroundOutline(aBuilder, aLists);
  NS_ENSURE_SUCCESS(rv, rv);
  
  if (aBuilder->GetIgnoreScrollFrame() == mOuter) {
    // Don't clip the scrolled child, and don't paint scrollbars/scrollcorner.
    // The scrolled frame shouldn't have its own background/border, so we
    // can just pass aLists directly.
    return mOuter->BuildDisplayListForChild(aBuilder, mScrolledFrame,
                                            aDirtyRect, aLists);
  }

  // Now display the scrollbars and scrollcorner. These parts are drawn
  // in the border-background layer, on top of our own background and
  // borders and underneath borders and backgrounds of later elements
  // in the tree.
  nsIFrame* kid = mOuter->GetFirstChild(nsnull);
  while (kid) {
    if (kid != mScrolledFrame) {
      rv = mOuter->BuildDisplayListForChild(aBuilder, kid, aDirtyRect, aLists);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    kid = kid->GetNextSibling();
  }

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
  
  nsDisplayListCollection set;
  rv = mOuter->BuildDisplayListForChild(aBuilder, mScrolledFrame, dirtyRect, set);
  NS_ENSURE_SUCCESS(rv, rv);
  nsRect clip = mScrollPort + aBuilder->ToReferenceFrame(mOuter);
  // mScrolledFrame may have given us a background, e.g., the scrolled canvas
  // frame below the viewport. If so, we want it to be clipped. We also want
  // to end up on our BorderBackground list.
  // If we are the viewport scrollframe, then clip all our descendants (to ensure
  // that fixed-pos elements get clipped by us).
  rv = mOuter->OverflowClip(aBuilder, set, aLists, clip, PR_TRUE, mIsRoot);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

static void HandleScrollPref(nsIScrollable *aScrollable, PRInt32 aOrientation,
                             PRUint8& aValue)
{
  PRInt32 pref;
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
  ScrollbarStyles result;

  nsPresContext* presContext = mOuter->PresContext();
  if (!presContext->IsDynamic() &&
      !(mIsRoot && presContext->HasPaginatedScrolling())) {
    return ScrollbarStyles(NS_STYLE_OVERFLOW_HIDDEN, NS_STYLE_OVERFLOW_HIDDEN);
  }

  if (mIsRoot) {
    result = presContext->GetViewportOverflowOverride();

    nsCOMPtr<nsISupports> container = presContext->GetContainer();
    nsCOMPtr<nsIScrollable> scrollable = do_QueryInterface(container);
    if (scrollable) {
      HandleScrollPref(scrollable, nsIScrollable::ScrollOrientation_X,
                       result.mHorizontal);
      HandleScrollPref(scrollable, nsIScrollable::ScrollOrientation_Y,
                       result.mVertical);
    }
  } else {
    const nsStyleDisplay *disp = mOuter->GetStyleDisplay();
    result.mHorizontal = disp->mOverflowX;
    result.mVertical = disp->mOverflowY;
  }

  NS_ASSERTION(result.mHorizontal != NS_STYLE_OVERFLOW_VISIBLE &&
               result.mHorizontal != NS_STYLE_OVERFLOW_CLIP &&
               result.mVertical != NS_STYLE_OVERFLOW_VISIBLE &&
               result.mVertical != NS_STYLE_OVERFLOW_CLIP,
               "scrollbars should not have been created");
  return result;
}

static nscoord
AlignToDevPixelRoundingToZero(nscoord aVal, PRInt32 aAppUnitsPerDevPixel)
{
  return (aVal/aAppUnitsPerDevPixel)*aAppUnitsPerDevPixel;
}

nsRect
nsGfxScrollFrameInner::GetScrollRange() const
{
  nsRect range = GetScrolledRect();
  range.width -= mScrollPort.width;
  range.height -= mScrollPort.height;

  nsPresContext* presContext = mOuter->PresContext();
  PRInt32 appUnitsPerDevPixel = presContext->AppUnitsPerDevPixel();
  range.width =
    AlignToDevPixelRoundingToZero(range.XMost(), appUnitsPerDevPixel) - range.x;
  range.height =
    AlignToDevPixelRoundingToZero(range.YMost(), appUnitsPerDevPixel) - range.y;
  range.x = AlignToDevPixelRoundingToZero(range.x, appUnitsPerDevPixel);
  range.y = AlignToDevPixelRoundingToZero(range.y, appUnitsPerDevPixel);
  return range;
}

static void
AdjustForWholeDelta(PRInt32 aDelta, nscoord* aCoord)
{
  if (aDelta < 0) {
    *aCoord = nscoord_MIN;
  } else if (aDelta > 0) {
    *aCoord = nscoord_MAX;
  }
}

void
nsGfxScrollFrameInner::ScrollBy(nsIntPoint aDelta,
                                nsIScrollableFrame::ScrollUnit aUnit,
                                nsIScrollableFrame::ScrollMode aMode,
                                nsIntPoint* aOverflow)
{
  nsSize deltaMultiplier;
  switch (aUnit) {
  case nsIScrollableFrame::DEVICE_PIXELS: {
    nscoord appUnitsPerDevPixel =
      mOuter->PresContext()->AppUnitsPerDevPixel();
    deltaMultiplier = nsSize(appUnitsPerDevPixel, appUnitsPerDevPixel);
    break;
  }
  case nsIScrollableFrame::LINES: {
    deltaMultiplier = GetLineScrollAmount();
    break;
  }
  case nsIScrollableFrame::PAGES: {
    deltaMultiplier = GetPageScrollAmount();
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
  ScrollTo(newPos, aMode);

  if (aOverflow) {
    nsPoint clampAmount = mDestination - newPos;
    float appUnitsPerDevPixel = mOuter->PresContext()->AppUnitsPerDevPixel();
    *aOverflow = nsIntPoint(
        NSAppUnitsToIntPixels(PR_ABS(clampAmount.x), appUnitsPerDevPixel),
        NSAppUnitsToIntPixels(PR_ABS(clampAmount.y), appUnitsPerDevPixel));
  }
}

nsSize
nsGfxScrollFrameInner::GetLineScrollAmount() const
{
  const nsStyleFont* font = mOuter->GetStyleFont();
  const nsFont& f = font->mFont;
  nsCOMPtr<nsIFontMetrics> fm = mOuter->PresContext()->GetMetricsFor(f);
  NS_ASSERTION(fm, "FontMetrics is null, assuming fontHeight == 1 appunit");
  nscoord fontHeight = 1;
  if (fm) {
    fm->GetHeight(fontHeight);
  }

  return nsSize(fontHeight, fontHeight);
}

nsSize
nsGfxScrollFrameInner::GetPageScrollAmount() const
{
  nsSize lineScrollAmount = GetLineScrollAmount();
  // The page increment is the size of the page, minus the smaller of
  // 10% of the size or 2 lines.
  return nsSize(
    mScrollPort.width - NS_MIN(mScrollPort.width/10, 2*lineScrollAmount.width),
    mScrollPort.height - NS_MIN(mScrollPort.height/10, 2*lineScrollAmount.height));
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
  nsPoint scrollPos = GetScrollPosition();

  // if we didn't move, we still need to restore
  if (scrollPos == mLastPos) {
    // if our desired position is different to the scroll position, scroll.
    // remember that we could be incrementally loading so we may enter
    // and scroll many times.
    if (mRestorePos != scrollPos) {
      ScrollTo(mRestorePos, nsIScrollableFrame::INSTANT);
      // Re-get the scroll position, it might not be exactly equal to
      // mRestorePos due to rounding and clamping.
      mLastPos = GetScrollPosition();
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

  PRBool newVerticalOverflow = childSize.height > scrollportSize.height;
  PRBool vertChanged = mVerticalOverflow != newVerticalOverflow;

  PRBool newHorizontalOverflow = childSize.width > scrollportSize.width;
  PRBool horizChanged = mHorizontalOverflow != newHorizontalOverflow;

  if (!vertChanged && !horizChanged) {
    return NS_OK;
  }

  // If both either overflowed or underflowed then we dispatch only one
  // DOM event.
  PRBool both = vertChanged && horizChanged &&
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

  nsScrollPortEvent event(PR_TRUE,
                          (orient == nsScrollPortEvent::horizontal ?
                           mHorizontalOverflow : mVerticalOverflow) ?
                            NS_SCROLLPORT_OVERFLOW : NS_SCROLLPORT_UNDERFLOW,
                          nsnull);
  event.orient = orient;
  return nsEventDispatcher::Dispatch(mOuter->GetContent(),
                                     mOuter->PresContext(), &event);
}

void
nsGfxScrollFrameInner::ReloadChildFrames()
{
  mScrolledFrame = nsnull;
  mHScrollbarBox = nsnull;
  mVScrollbarBox = nsnull;
  mScrollCornerBox = nsnull;

  nsIFrame* frame = mOuter->GetFirstChild(nsnull);
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
      } else {
        // probably a scrollcorner
        NS_ASSERTION(!mScrollCornerBox, "Found multiple scrollcorners");
        mScrollCornerBox = frame;
      }
    }

    frame = frame->GetNextSibling();
  }
}
  
nsresult
nsGfxScrollFrameInner::CreateAnonymousContent(nsTArray<nsIContent*>& aElements)
{
  nsPresContext* presContext = mOuter->PresContext();
  nsIFrame* parent = mOuter->GetParent();

  // Don't create scrollbars if we're printing/print previewing
  // Get rid of this code when printing moves to its own presentation
  if (!presContext->IsDynamic()) {
    // allow scrollbars if this is the child of the viewport, because
    // we must be the scrollbars for the print preview window
    if (!(mIsRoot && presContext->HasPaginatedScrolling())) {
      mNeverHasVerticalScrollbar = mNeverHasHorizontalScrollbar = PR_TRUE;
      return NS_OK;
    }
  }

  nsIScrollableFrame *scrollable = do_QueryFrame(mOuter);

  // At this stage in frame construction, the document element and/or
  // BODY overflow styles have not yet been propagated to the
  // viewport. So GetScrollbarStylesFromFrame called here will only
  // take into account the scrollbar preferences set on the docshell.
  // Thus if no scrollbar preferences are set on the docshell, we will
  // always create scrollbars, which means later dynamic changes to
  // propagated overflow styles will show or hide scrollbars on the
  // viewport without requiring frame reconstruction of the viewport
  // (good!).

  // XXX On the other hand, if scrolling="no" is set on the container
  // we won't create scrollbars here so no scrollbars will ever be
  // created even if the container's scrolling attribute is later
  // changed. However, this has never been supported.
  ScrollbarStyles styles = scrollable->GetScrollbarStyles();
  PRBool canHaveHorizontal = styles.mHorizontal != NS_STYLE_OVERFLOW_HIDDEN;
  PRBool canHaveVertical = styles.mVertical != NS_STYLE_OVERFLOW_HIDDEN;
  if (!canHaveHorizontal && !canHaveVertical) {
    // Nothing to do.
    return NS_OK;
  }

  // The anonymous <div> used by <inputs> never gets scrollbars.
  nsITextControlFrame* textFrame = do_QueryFrame(parent);
  if (textFrame) {
    // Make sure we are not a text area.
    nsCOMPtr<nsIDOMHTMLTextAreaElement> textAreaElement(do_QueryInterface(parent->GetContent()));
    if (!textAreaElement) {
      mNeverHasVerticalScrollbar = mNeverHasHorizontalScrollbar = PR_TRUE;
      return NS_OK;
    }
  }

  nsresult rv;

  nsNodeInfoManager *nodeInfoManager =
    presContext->Document()->NodeInfoManager();
  nsCOMPtr<nsINodeInfo> nodeInfo;
  nodeInfo = nodeInfoManager->GetNodeInfo(nsGkAtoms::scrollbar, nsnull,
                                          kNameSpaceID_XUL);
  NS_ENSURE_TRUE(nodeInfo, NS_ERROR_OUT_OF_MEMORY);

  if (canHaveHorizontal) {
    rv = NS_NewElement(getter_AddRefs(mHScrollbarContent),
                       kNameSpaceID_XUL, nodeInfo, PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);
    mHScrollbarContent->SetAttr(kNameSpaceID_None, nsGkAtoms::orient,
                                NS_LITERAL_STRING("horizontal"), PR_FALSE);
    if (!aElements.AppendElement(mHScrollbarContent))
      return NS_ERROR_OUT_OF_MEMORY;
  }

  if (canHaveVertical) {
    rv = NS_NewElement(getter_AddRefs(mVScrollbarContent),
                       kNameSpaceID_XUL, nodeInfo, PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);
    mVScrollbarContent->SetAttr(kNameSpaceID_None, nsGkAtoms::orient,
                                NS_LITERAL_STRING("vertical"), PR_FALSE);
    if (!aElements.AppendElement(mVScrollbarContent))
      return NS_ERROR_OUT_OF_MEMORY;
  }

  if (canHaveHorizontal && canHaveVertical) {
    nodeInfo = nodeInfoManager->GetNodeInfo(nsGkAtoms::scrollcorner, nsnull,
                                            kNameSpaceID_XUL);
    rv = NS_NewElement(getter_AddRefs(mScrollCornerContent),
                       kNameSpaceID_XUL, nodeInfo, PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!aElements.AppendElement(mScrollCornerContent))
      return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

void
nsGfxScrollFrameInner::GetAnonymousContent(nsBaseContentList& aElements)
{
  aElements.MaybeAppendElement(mHScrollbarContent);
  aElements.MaybeAppendElement(mVScrollbarContent);
  aElements.MaybeAppendElement(mScrollCornerContent);
}

void
nsGfxScrollFrameInner::Destroy()
{
  // Unbind any content created in CreateAnonymousContent from the tree
  nsContentUtils::DestroyAnonymousContent(&mHScrollbarContent);
  nsContentUtils::DestroyAnonymousContent(&mVScrollbarContent);
  nsContentUtils::DestroyAnonymousContent(&mScrollCornerContent);

  if (mPostedReflowCallback) {
    mOuter->PresContext()->PresShell()->CancelReflowCallback(this);
    mPostedReflowCallback = PR_FALSE;
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
  mFrameIsUpdatingScrollbar = PR_TRUE;

  nsPoint pt = GetScrollPosition();
  if (mVScrollbarBox) {
    SetCoordAttribute(mVScrollbarBox->GetContent(), nsGkAtoms::curpos,
                      pt.y - GetScrolledRect().y);
  }
  if (mHScrollbarBox) {
    SetCoordAttribute(mHScrollbarBox->GetContent(), nsGkAtoms::curpos,
                      pt.x - GetScrolledRect().x);
  }

  mFrameIsUpdatingScrollbar = PR_FALSE;
}

void nsGfxScrollFrameInner::CurPosAttributeChanged(nsIContent* aContent)
{
  NS_ASSERTION(aContent, "aContent must not be null");
  NS_ASSERTION((mHScrollbarBox && mHScrollbarBox->GetContent() == aContent) ||
               (mVScrollbarBox && mVScrollbarBox->GetContent() == aContent),
               "unexpected child");

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

  nscoord x = GetCoordAttribute(mHScrollbarBox, nsGkAtoms::curpos,
                                -scrolledRect.x) +
              scrolledRect.x;
  nscoord y = GetCoordAttribute(mVScrollbarBox, nsGkAtoms::curpos,
                                -scrolledRect.y) +
              scrolledRect.y;

  PRBool isSmooth = aContent->HasAttr(kNameSpaceID_None, nsGkAtoms::smooth);
  if (isSmooth) {
    // Make sure an attribute-setting callback occurs even if the view
    // didn't actually move yet.  We need to make sure other listeners
    // see that the scroll position is not (yet) what they thought it
    // was.
    UpdateScrollbarPosition();
  }
  ScrollTo(nsPoint(x, y),
           isSmooth ? nsIScrollableFrame::SMOOTH : nsIScrollableFrame::INSTANT);
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

  nsScrollbarEvent event(PR_TRUE, NS_SCROLL_EVENT, nsnull);
  nsEventStatus status = nsEventStatus_eIgnore;
  nsIContent* content = mOuter->GetContent();
  nsPresContext* prescontext = mOuter->PresContext();
  // Fire viewport scroll events at the document (where they
  // will bubble to the window)
  if (mIsRoot) {
    nsIDocument* doc = content->GetCurrentDoc();
    if (doc) {
      nsEventDispatcher::Dispatch(doc, prescontext, &event, nsnull,  &status);
    }
  } else {
    // scroll events fired at elements don't bubble (although scroll events
    // fired at documents do, to the window)
    event.flags |= NS_EVENT_FLAG_CANT_BUBBLE;
    nsEventDispatcher::Dispatch(content, prescontext, &event, nsnull, &status);
  }
}

void
nsGfxScrollFrameInner::PostScrollEvent()
{
  if (mScrollEvent.IsPending())
    return;

  nsRefPtr<ScrollEvent> ev = new ScrollEvent(this);
  if (NS_FAILED(NS_DispatchToCurrentThread(ev))) {
    NS_WARNING("failed to dispatch ScrollEvent");
  } else {
    mScrollEvent = ev;
  }
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

PRBool
nsXULScrollFrame::AddHorizontalScrollbar(nsBoxLayoutState& aState, PRBool aOnTop)
{
  if (!mInner.mHScrollbarBox)
    return PR_TRUE;

  return AddRemoveScrollbar(aState, aOnTop, PR_TRUE, PR_TRUE);
}

PRBool
nsXULScrollFrame::AddVerticalScrollbar(nsBoxLayoutState& aState, PRBool aOnRight)
{
  if (!mInner.mVScrollbarBox)
    return PR_TRUE;

  return AddRemoveScrollbar(aState, aOnRight, PR_FALSE, PR_TRUE);
}

void
nsXULScrollFrame::RemoveHorizontalScrollbar(nsBoxLayoutState& aState, PRBool aOnTop)
{
  // removing a scrollbar should always fit
#ifdef DEBUG
  PRBool result =
#endif
  AddRemoveScrollbar(aState, aOnTop, PR_TRUE, PR_FALSE);
  NS_ASSERTION(result, "Removing horizontal scrollbar failed to fit??");
}

void
nsXULScrollFrame::RemoveVerticalScrollbar(nsBoxLayoutState& aState, PRBool aOnRight)
{
  // removing a scrollbar should always fit
#ifdef DEBUG
  PRBool result =
#endif
  AddRemoveScrollbar(aState, aOnRight, PR_FALSE, PR_FALSE);
  NS_ASSERTION(result, "Removing vertical scrollbar failed to fit??");
}

PRBool
nsXULScrollFrame::AddRemoveScrollbar(nsBoxLayoutState& aState,
                                     PRBool aOnTop, PRBool aHorizontal, PRBool aAdd)
{
  if (aHorizontal) {
     if (mInner.mNeverHasHorizontalScrollbar || !mInner.mHScrollbarBox)
       return PR_FALSE;

     nsSize hSize = mInner.mHScrollbarBox->GetPrefSize(aState);

     mInner.SetScrollbarVisibility(mInner.mHScrollbarBox, aAdd);

     PRBool hasHorizontalScrollbar;
     PRBool fit = AddRemoveScrollbar(hasHorizontalScrollbar,
                                     mInner.mScrollPort.y,
                                     mInner.mScrollPort.height,
                                     hSize.height, aOnTop, aAdd);
     mInner.mHasHorizontalScrollbar = hasHorizontalScrollbar;    // because mHasHorizontalScrollbar is a PRPackedBool
     if (!fit)
        mInner.SetScrollbarVisibility(mInner.mHScrollbarBox, !aAdd);

     return fit;
  } else {
     if (mInner.mNeverHasVerticalScrollbar || !mInner.mVScrollbarBox)
       return PR_FALSE;

     nsSize vSize = mInner.mVScrollbarBox->GetPrefSize(aState);

     mInner.SetScrollbarVisibility(mInner.mVScrollbarBox, aAdd);

     PRBool hasVerticalScrollbar;
     PRBool fit = AddRemoveScrollbar(hasVerticalScrollbar,
                                     mInner.mScrollPort.x,
                                     mInner.mScrollPort.width,
                                     vSize.width, aOnTop, aAdd);
     mInner.mHasVerticalScrollbar = hasVerticalScrollbar;    // because mHasVerticalScrollbar is a PRPackedBool
     if (!fit)
        mInner.SetScrollbarVisibility(mInner.mVScrollbarBox, !aAdd);

     return fit;
  }
}

PRBool
nsXULScrollFrame::AddRemoveScrollbar(PRBool& aHasScrollbar, nscoord& aXY,
                                     nscoord& aSize, nscoord aSbSize,
                                     PRBool aRightOrBottom, PRBool aAdd)
{ 
   nscoord size = aSize;
   nscoord xy = aXY;

   if (size != NS_INTRINSICSIZE) {
     if (aAdd) {
        size -= aSbSize;
        if (!aRightOrBottom && size >= 0)
          xy += aSbSize;
     } else {
        size += aSbSize;
        if (!aRightOrBottom)
          xy -= aSbSize;
     }
   }

   // not enough room? Yes? Return true.
   if (size >= 0) {
       aHasScrollbar = aAdd;
       aSize = size;
       aXY = xy;
       return PR_TRUE;
   }

   aHasScrollbar = PR_FALSE;
   return PR_FALSE;
}

void
nsXULScrollFrame::LayoutScrollArea(nsBoxLayoutState& aState,
                                   const nsPoint& aScrollPosition)
{
  PRUint32 oldflags = aState.LayoutFlags();
  nsRect childRect = nsRect(mInner.mScrollPort.TopLeft() - aScrollPosition,
                            mInner.mScrollPort.Size());
  PRInt32 flags = NS_FRAME_NO_MOVE_VIEW;

  nsRect originalRect = mInner.mScrolledFrame->GetRect();
  nsRect originalOverflow = mInner.mScrolledFrame->GetOverflowRect();

  nsSize minSize = mInner.mScrolledFrame->GetMinSize(aState);
  
  if (minSize.height > childRect.height)
    childRect.height = minSize.height;
  
  if (minSize.width > childRect.width)
    childRect.width = minSize.width;

  aState.SetLayoutFlags(flags);
  mInner.mScrolledFrame->SetBounds(aState, childRect);
  mInner.mScrolledFrame->Layout(aState);

  childRect = mInner.mScrolledFrame->GetRect();

  if (childRect.width < mInner.mScrollPort.width ||
      childRect.height < mInner.mScrollPort.height)
  {
    childRect.width = NS_MAX(childRect.width, mInner.mScrollPort.width);
    childRect.height = NS_MAX(childRect.height, mInner.mScrollPort.height);

    // remove overflow area when we update the bounds,
    // because we've already accounted for it
    mInner.mScrolledFrame->SetBounds(aState, childRect);
    mInner.mScrolledFrame->ClearOverflowRect();
  }

  nsRect finalRect = mInner.mScrolledFrame->GetRect();
  nsRect finalOverflow = mInner.mScrolledFrame->GetOverflowRect();
  // The position of the scrolled frame shouldn't change, but if it does or
  // the position of the overflow rect changes just invalidate both the old
  // and new overflow rect.
  if (originalRect.TopLeft() != finalRect.TopLeft() ||
      originalOverflow.TopLeft() != finalOverflow.TopLeft())
  {
    // The old overflow rect needs to be adjusted if the frame's position
    // changed.
    mInner.mScrolledFrame->Invalidate(
      originalOverflow + originalRect.TopLeft() - finalRect.TopLeft());
    mInner.mScrolledFrame->Invalidate(finalOverflow);
  } else if (!originalOverflow.IsExactEqual(finalOverflow)) {
    // If the overflow rect changed then invalidate the difference between the
    // old and new overflow rects.
    mInner.mScrolledFrame->CheckInvalidateSizeChange(
      originalRect, originalOverflow, finalRect.Size());
    mInner.mScrolledFrame->InvalidateRectDifference(
      originalOverflow, finalOverflow);
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

  PRBool newVerticalOverflow = childSize.height > scrollportSize.height;
  PRBool vertChanged = mVerticalOverflow != newVerticalOverflow;

  PRBool newHorizontalOverflow = childSize.width > scrollportSize.width;
  PRBool horizChanged = mHorizontalOverflow != newHorizontalOverflow;

  if (!vertChanged && !horizChanged) {
    return;
  }

  nsRefPtr<AsyncScrollPortEvent> ev = new AsyncScrollPortEvent(this);
  if (NS_SUCCEEDED(NS_DispatchToCurrentThread(ev)))
    mAsyncScrollPortEvent = ev;
}

PRBool
nsGfxScrollFrameInner::IsLTR() const
{
  //TODO make bidi code set these from preferences

  nsIFrame *frame = mOuter;
  // XXX This is a bit on the slow side.
  if (mIsRoot) {
    // If we're the root scrollframe, we need the root element's style data.
    nsPresContext *presContext = mOuter->PresContext();
    nsIDocument *document = presContext->Document();
    nsIContent *root = document->GetRootContent();

    // But for HTML we want the body element.
    nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(document);
    if (htmlDoc) {
      nsIContent *bodyContent = htmlDoc->GetBodyContentExternal();
      if (bodyContent)
        root = bodyContent; // we can trust the document to hold on to it
    }

    if (root) {
      nsIFrame *rootsFrame = root->GetPrimaryFrame();
      if (rootsFrame)
        frame = rootsFrame;
    }
  }

  return frame->GetStyleVisibility()->mDirection != NS_STYLE_DIRECTION_RTL;
}

PRBool
nsGfxScrollFrameInner::IsScrollbarOnRight() const
{
  nsPresContext *presContext = mOuter->PresContext();
  switch (presContext->GetCachedIntPref(kPresContext_ScrollbarSide)) {
    default:
    case 0: // UI directionality
      return presContext->GetCachedIntPref(kPresContext_BidiDirection)
             == IBMBIDI_TEXTDIRECTION_LTR;
    case 1: // Document / content directionality
      return IsLTR();
    case 2: // Always right
      return PR_TRUE;
    case 3: // Always left
      return PR_FALSE;
  }
}

/**
 * Reflow the scroll area if it needs it and return its size. Also determine if the reflow will
 * cause any of the scrollbars to need to be reflowed.
 */
nsresult
nsXULScrollFrame::Layout(nsBoxLayoutState& aState)
{
  PRBool scrollbarRight = mInner.IsScrollbarOnRight();
  PRBool scrollbarBottom = PR_TRUE;

  // get the content rect
  nsRect clientRect(0,0,0,0);
  GetClientRect(clientRect);

  nsRect oldScrollAreaBounds = mInner.mScrollPort;
  nsPoint oldScrollPosition = mInner.GetScrollPosition();

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
     mInner.mHasHorizontalScrollbar = PR_TRUE;
  if (styles.mVertical == NS_STYLE_OVERFLOW_SCROLL)
     mInner.mHasVerticalScrollbar = PR_TRUE;

  if (mInner.mHasHorizontalScrollbar)
     AddHorizontalScrollbar(aState, scrollbarBottom);

  if (mInner.mHasVerticalScrollbar)
     AddVerticalScrollbar(aState, scrollbarRight);
     
  // layout our the scroll area
  LayoutScrollArea(aState, oldScrollPosition);
  
  // now look at the content area and see if we need scrollbars or not
  PRBool needsLayout = PR_FALSE;

  // if we have 'auto' scrollbars look at the vertical case
  if (styles.mVertical != NS_STYLE_OVERFLOW_SCROLL) {
    // These are only good until the call to LayoutScrollArea.
    nsRect scrolledRect = mInner.GetScrolledRect();
    nsSize scrolledContentSize(scrolledRect.XMost(), scrolledRect.YMost());

    // There are two cases to consider
      if (scrolledContentSize.height <= mInner.mScrollPort.height
          || styles.mVertical != NS_STYLE_OVERFLOW_AUTO) {
        if (mInner.mHasVerticalScrollbar) {
          // We left room for the vertical scrollbar, but it's not needed;
          // remove it.
          RemoveVerticalScrollbar(aState, scrollbarRight);
          needsLayout = PR_TRUE;
        }
      } else {
        if (!mInner.mHasVerticalScrollbar) {
          // We didn't leave room for the vertical scrollbar, but it turns
          // out we needed it
          if (AddVerticalScrollbar(aState, scrollbarRight))
            needsLayout = PR_TRUE;
        }
    }

    // ok layout at the right size
    if (needsLayout) {
       nsBoxLayoutState resizeState(aState);
       LayoutScrollArea(resizeState, oldScrollPosition);
       needsLayout = PR_FALSE;
    }
  }


  // if scrollbars are auto look at the horizontal case
  if (styles.mHorizontal != NS_STYLE_OVERFLOW_SCROLL)
  {
    // These are only good until the call to LayoutScrollArea.
    nsRect scrolledRect = mInner.GetScrolledRect();
    nsSize scrolledContentSize(scrolledRect.XMost(), scrolledRect.YMost());

    // if the child is wider that the scroll area
    // and we don't have a scrollbar add one.
    if (scrolledContentSize.width > mInner.mScrollPort.width
        && styles.mHorizontal == NS_STYLE_OVERFLOW_AUTO) {

      if (!mInner.mHasHorizontalScrollbar) {
           // no scrollbar? 
          if (AddHorizontalScrollbar(aState, scrollbarBottom))
             needsLayout = PR_TRUE;

           // if we added a horizontal scrollbar and we did not have a vertical
           // there is a chance that by adding the horizontal scrollbar we will
           // suddenly need a vertical scrollbar. Is a special case but its 
           // important.
           //if (!mHasVerticalScrollbar && scrolledContentSize.height > scrollAreaRect.height - sbSize.height)
           //  printf("****Gfx Scrollbar Special case hit!!*****\n");
           
      }
    } else {
        // if the area is smaller or equal to and we have a scrollbar then
        // remove it.
      if (mInner.mHasHorizontalScrollbar) {
        RemoveHorizontalScrollbar(aState, scrollbarBottom);
        needsLayout = PR_TRUE;
      }
    }
  }

  // we only need to set the rect. The inner child stays the same size.
  if (needsLayout) {
     nsBoxLayoutState resizeState(aState);
     LayoutScrollArea(resizeState, oldScrollPosition);
     needsLayout = PR_FALSE;
  }
    
  // get the preferred size of the scrollbars
  nsSize hMinSize(0, 0);
  if (mInner.mHScrollbarBox && mInner.mHasHorizontalScrollbar) {
    GetScrollbarMetrics(aState, mInner.mHScrollbarBox, &hMinSize, nsnull, PR_FALSE);
  }
  nsSize vMinSize(0, 0);
  if (mInner.mVScrollbarBox && mInner.mHasVerticalScrollbar) {
    GetScrollbarMetrics(aState, mInner.mVScrollbarBox, &vMinSize, nsnull, PR_TRUE);
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
    needsLayout = PR_TRUE;
  }
  // Now disable vertical scrollbar if necessary
  if (mInner.mHasVerticalScrollbar &&
      (vMinSize.height > clientRect.height - hMinSize.height
       || vMinSize.width > clientRect.width)) {
    RemoveVerticalScrollbar(aState, scrollbarRight);
    needsLayout = PR_TRUE;
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
    mInner.mPostedReflowCallback = PR_TRUE;
  }
  if (!(GetStateBits() & NS_FRAME_FIRST_REFLOW)) {
    mInner.mHadNonInitialReflow = PR_TRUE;
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

PRBool
nsGfxScrollFrameInner::ReflowFinished()
{
  mPostedReflowCallback = PR_FALSE;

  ScrollToRestoredPosition();

  if (NS_SUBTREE_DIRTY(mOuter) || !mUpdateScrollbarAttributes)
    return PR_FALSE;

  mUpdateScrollbarAttributes = PR_FALSE;

  // Update scrollbar attributes.
  nsPresContext* presContext = mOuter->PresContext();

  if (mMayHaveDirtyFixedChildren) {
    mMayHaveDirtyFixedChildren = PR_FALSE;
    nsIFrame* parentFrame = mOuter->GetParent();
    for (nsIFrame* fixedChild =
           parentFrame->GetFirstChild(nsGkAtoms::fixedList);
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
  mFrameIsUpdatingScrollbar = PR_TRUE;

  nsCOMPtr<nsIContent> vScroll =
    mVScrollbarBox ? mVScrollbarBox->GetContent() : nsnull;
  nsCOMPtr<nsIContent> hScroll =
    mHScrollbarBox ? mHScrollbarBox->GetContent() : nsnull;

  // Note, in some cases mOuter may get deleted while finishing reflow
  // for scrollbars.
  if (vScroll || hScroll) {
    nsWeakFrame weakFrame(mOuter);
    nsPoint scrollPos = GetScrollPosition();
    // XXX shouldn't we use GetPageScrollAmount/GetLineScrollAmount here?
    if (vScroll) {
      nscoord fontHeight = GetLineScrollAmount().height;
      // We normally use (scrollArea.height - fontHeight) for height
      // of page scrolling.  However, it is too small when
      // fontHeight is very large. (If fontHeight is larger than
      // scrollArea.height, direction of scrolling will be opposite).
      // To avoid it, we use (float(scrollArea.height) * 0.8) as
      // lower bound value of height of page scrolling. (bug 383267)
      nscoord pageincrement = nscoord(mScrollPort.height - fontHeight);
      nscoord pageincrementMin = nscoord(float(mScrollPort.height) * 0.8);
      FinishReflowForScrollbar(vScroll, minY, maxY, scrollPos.y,
                               NS_MAX(pageincrement,pageincrementMin),
                               fontHeight);
    }
    if (hScroll) {
      FinishReflowForScrollbar(hScroll, minX, maxX, scrollPos.x,
                               nscoord(float(mScrollPort.width) * 0.8),
                               nsPresContext::CSSPixelsToAppUnits(10));
    }
    NS_ENSURE_TRUE(weakFrame.IsAlive(), PR_FALSE);
  }

  mFrameIsUpdatingScrollbar = PR_FALSE;
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
    return PR_FALSE;
  CurPosAttributeChanged(mVScrollbarBox ? mVScrollbarBox->GetContent()
                                        : mHScrollbarBox->GetContent());
  return PR_TRUE;
}

void
nsGfxScrollFrameInner::ReflowCallbackCanceled()
{
  mPostedReflowCallback = PR_FALSE;
}

static void LayoutAndInvalidate(nsBoxLayoutState& aState,
                                nsIFrame* aBox, const nsRect& aRect)
{
  // When a child box changes shape of position, the parent
  // is responsible for invalidation; the overflow rect must be invalidated
  // to make sure to catch any overflow.
  // We invalidate the parent (i.e. the scrollframe) directly, because
  // invalidates coming from scrollbars are suppressed by nsHTMLScrollFrame when
  // mHasVScrollbar/mHasHScrollbar is false, and this is called after those
  // flags have been set ... if a scrollbar is being hidden, we still need
  // to invalidate the scrollbar area here.
  PRBool rectChanged = aBox->GetRect() != aRect;
  if (rectChanged) {
    aBox->GetParent()->Invalidate(aBox->GetOverflowRect() + aBox->GetPosition());
  }
  nsBoxFrame::LayoutChildAt(aState, aBox, aRect);
  if (rectChanged) {
    aBox->GetParent()->Invalidate(aBox->GetOverflowRect() + aBox->GetPosition());
  }
}

static void AdjustScrollbarRect(nsIFrame* aFrame, nsPresContext* aPresContext,
                                nsRect& aRect, PRBool aVertical)
{
  if ((aVertical ? aRect.width : aRect.height) == 0)
    return;

  nsPoint offsetToView;
  nsPoint offsetToWidget;
  nsIWidget* widget =
    aFrame->GetClosestView(&offsetToView)->GetNearestWidget(&offsetToWidget);
  nsPoint offset = offsetToView + offsetToWidget;
  nsIntRect widgetRect;
  if (!widget || !widget->ShowsResizeIndicator(&widgetRect))
    return;

  nsRect resizerRect =
      nsRect(aPresContext->DevPixelsToAppUnits(widgetRect.x) - offset.x,
             aPresContext->DevPixelsToAppUnits(widgetRect.y) - offset.y,
             aPresContext->DevPixelsToAppUnits(widgetRect.width),
             aPresContext->DevPixelsToAppUnits(widgetRect.height));

  if (!resizerRect.Contains(aRect.BottomRight() - nsPoint(1, 1)))
    return;

  if (aVertical)
    aRect.height = NS_MAX(0, resizerRect.y - aRect.y);
  else
    aRect.width = NS_MAX(0, resizerRect.x - aRect.x);
}

void
nsGfxScrollFrameInner::LayoutScrollbars(nsBoxLayoutState& aState,
                                        const nsRect& aContentArea,
                                        const nsRect& aOldScrollArea)
{
  NS_ASSERTION(!mSupppressScrollbarUpdate,
               "This should have been suppressed");

  nsPresContext* presContext = mScrolledFrame->PresContext();
  if (mVScrollbarBox) {
    NS_PRECONDITION(mVScrollbarBox->IsBoxFrame(), "Must be a box frame!");
    nsRect vRect(mScrollPort);
    vRect.width = aContentArea.width - mScrollPort.width;
    vRect.x = IsScrollbarOnRight() ? mScrollPort.XMost() : aContentArea.x;
#ifdef DEBUG
    nsMargin margin;
    mVScrollbarBox->GetMargin(margin);
    NS_ASSERTION(margin == nsMargin(0,0,0,0), "Scrollbar margin not supported");
#endif
    AdjustScrollbarRect(mOuter, presContext, vRect, PR_TRUE);
    LayoutAndInvalidate(aState, mVScrollbarBox, vRect);
  }

  if (mHScrollbarBox) {
    NS_PRECONDITION(mHScrollbarBox->IsBoxFrame(), "Must be a box frame!");
    nsRect hRect(mScrollPort);
    hRect.height = aContentArea.height - mScrollPort.height;
    hRect.y = PR_TRUE ? mScrollPort.YMost() : aContentArea.y;
#ifdef DEBUG
    nsMargin margin;
    mHScrollbarBox->GetMargin(margin);
    NS_ASSERTION(margin == nsMargin(0,0,0,0), "Scrollbar margin not supported");
#endif
    AdjustScrollbarRect(mOuter, presContext, hRect, PR_FALSE);
    LayoutAndInvalidate(aState, mHScrollbarBox, hRect);
  }

  // place the scrollcorner
  if (mScrollCornerBox) {
    NS_PRECONDITION(mScrollCornerBox->IsBoxFrame(), "Must be a box frame!");
    nsRect r(0, 0, 0, 0);
    if (aContentArea.x != mScrollPort.x) {
      // scrollbar (if any) on left
      r.x = aContentArea.x;
      r.width = mScrollPort.x - aContentArea.x;
      NS_ASSERTION(r.width >= 0, "Scroll area should be inside client rect");
    } else {
      // scrollbar (if any) on right
      r.x = mScrollPort.XMost();
      r.width = aContentArea.XMost() - mScrollPort.XMost();
      NS_ASSERTION(r.width >= 0, "Scroll area should be inside client rect");
    }
    if (aContentArea.y != mScrollPort.y) {
      // scrollbar (if any) on top
      r.y = aContentArea.y;
      r.height = mScrollPort.y - aContentArea.y;
      NS_ASSERTION(r.height >= 0, "Scroll area should be inside client rect");
    } else {
      // scrollbar (if any) on bottom
      r.y = mScrollPort.YMost();
      r.height = aContentArea.YMost() - mScrollPort.YMost();
      NS_ASSERTION(r.height >= 0, "Scroll area should be inside client rect");
    }
    LayoutAndInvalidate(aState, mScrollCornerBox, r);
  }

  // may need to update fixed position children of the viewport,
  // if the client area changed size because of an incremental
  // reflow of a descendant.  (If the outer frame is dirty, the fixed
  // children will be re-laid out anyway)
  if (aOldScrollArea.Size() != mScrollPort.Size() && 
      !(mOuter->GetStateBits() & NS_FRAME_IS_DIRTY) &&
      mIsRoot) {
    mMayHaveDirtyFixedChildren = PR_TRUE;
  }
  
  // post reflow callback to modify scrollbar attributes
  mUpdateScrollbarAttributes = PR_TRUE;
  if (!mPostedReflowCallback) {
    aState.PresContext()->PresShell()->PostReflowCallback(this);
    mPostedReflowCallback = PR_TRUE;
  }
}

void
nsGfxScrollFrameInner::SetScrollbarEnabled(nsIContent* aContent, nscoord aMaxPos)
{
  if (aMaxPos) {
    aContent->UnsetAttr(kNameSpaceID_None, nsGkAtoms::disabled, PR_TRUE);
  } else {
    aContent->SetAttr(kNameSpaceID_None, nsGkAtoms::disabled,
                      NS_LITERAL_STRING("true"), PR_TRUE);
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

  aContent->SetAttr(kNameSpaceID_None, aAtom, newValue, PR_TRUE);
}

nsRect
nsGfxScrollFrameInner::GetScrolledRect() const
{
  nsRect result =
    GetScrolledRectInternal(mScrolledFrame->GetOverflowRect(),
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
  nscoord x1 = aScrolledFrameOverflowArea.x,
          x2 = aScrolledFrameOverflowArea.XMost(),
          y1 = aScrolledFrameOverflowArea.y,
          y2 = aScrolledFrameOverflowArea.YMost();
  if (y1 < 0)
    y1 = 0;
  if (IsLTR() || mIsXUL) {
    if (x1 < 0)
      x1 = 0;
  } else {
    if (x2 > aScrollPortSize.width)
      x2 = aScrollPortSize.width;
    // When the scrolled frame chooses a size larger than its available width (because
    // its padding alone is larger than the available width), we need to keep the
    // start-edge of the scroll frame anchored to the start-edge of the scrollport. 
    // When the scrolled frame is RTL, this means moving it in our left-based
    // coordinate system, so we need to compensate for its extra width here by
    // effectively repositioning the frame.
    nscoord extraWidth = NS_MAX(0, mScrolledFrame->GetSize().width - aScrollPortSize.width);
    x2 += extraWidth;
  }
  return nsRect(x1, y1, x2 - x1, y2 - y1);
}

nsMargin
nsGfxScrollFrameInner::GetActualScrollbarSizes() const
{
  nsMargin result(0, 0, 0, 0);
  if (mVScrollbarBox) {
    if (IsScrollbarOnRight()) {
      result.right = mVScrollbarBox->GetRect().width;
    } else {
      result.left = mVScrollbarBox->GetRect().width;
    }
  }
  if (mHScrollbarBox) {
    result.bottom = mHScrollbarBox->GetRect().height;
  }
  return result;
}

void
nsGfxScrollFrameInner::SetScrollbarVisibility(nsIBox* aScrollbar, PRBool aVisible)
{
  if (!aScrollbar)
    return;

  nsIScrollbarFrame* scrollbar = do_QueryFrame(aScrollbar);
  if (scrollbar) {
    // See if we have a mediator.
    nsIScrollbarMediator* mediator = scrollbar->GetScrollbarMediator();
    if (mediator) {
      // Inform the mediator of the visibility change.
      mediator->VisibilityChanged(aVisible);
    }
  }
}

PRInt32
nsGfxScrollFrameInner::GetCoordAttribute(nsIBox* aBox, nsIAtom* atom, PRInt32 defaultValue)
{
  if (aBox) {
    nsIContent* content = aBox->GetContent();

    nsAutoString value;
    content->GetAttr(kNameSpaceID_None, atom, value);
    if (!value.IsEmpty())
    {
      PRInt32 error;

      // convert it to an integer
      defaultValue = nsPresContext::CSSPixelsToAppUnits(value.ToInteger(&error));
    }
  }

  return defaultValue;
}

static nsIURI* GetDocURI(nsIFrame* aFrame)
{
  nsIPresShell* shell = aFrame->PresContext()->GetPresShell();
  if (!shell)
    return nsnull;
  nsIDocument* doc = shell->GetDocument();
  if (!doc)
    return nsnull;
  return doc->GetDocumentURI();
}

void
nsGfxScrollFrameInner::SaveVScrollbarStateToGlobalHistory()
{
  NS_ASSERTION(mIsRoot, "Only use this on viewports");

  // If the hint is the same as the one we loaded, don't bother
  // saving it
  if (mDidLoadHistoryVScrollbarHint &&
      (mHistoryVScrollbarHint == mHasVerticalScrollbar))
    return;

  nsIURI* uri = GetDocURI(mOuter);
  if (!uri)
    return;

  nsCOMPtr<nsIGlobalHistory3> history(do_GetService(NS_GLOBALHISTORY2_CONTRACTID));
  if (!history)
    return;
  
  PRUint32 flags = 0;
  if (mHasVerticalScrollbar) {
    flags |= NS_GECKO_FLAG_NEEDS_VERTICAL_SCROLLBAR;
  }
  history->SetURIGeckoFlags(uri, flags);
  // if it fails, we don't care
}

nsresult
nsGfxScrollFrameInner::GetVScrollbarHintFromGlobalHistory(PRBool* aVScrollbarNeeded)
{
  NS_ASSERTION(mIsRoot, "Only use this on viewports");
  NS_ASSERTION(!mDidLoadHistoryVScrollbarHint,
               "Should only load a hint once, it can be expensive");

  nsIURI* uri = GetDocURI(mOuter);
  if (!uri)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIGlobalHistory3> history(do_GetService(NS_GLOBALHISTORY2_CONTRACTID));
  if (!history)
    return NS_ERROR_FAILURE;
  
  PRUint32 flags;
  nsresult rv = history->GetURIGeckoFlags(uri, &flags);
  if (NS_FAILED(rv))
    return rv;

  *aVScrollbarNeeded = (flags & NS_GECKO_FLAG_NEEDS_VERTICAL_SCROLLBAR) != 0;
  mDidLoadHistoryVScrollbarHint = PR_TRUE;
  mHistoryVScrollbarHint = *aVScrollbarNeeded;
  return NS_OK;
}

nsPresState*
nsGfxScrollFrameInner::SaveState(nsIStatefulFrame::SpecialStateID aStateID)
{
  // Don't save "normal" state for the root scrollframe; that's
  // handled via the eDocumentScrollState state id
  if (mIsRoot && aStateID == nsIStatefulFrame::eNoID) {
    return nsnull;
  }

  nsIScrollbarMediator* mediator = do_QueryFrame(GetScrolledFrame());
  if (mediator) {
    // child handles its own scroll state, so don't bother saving state here
    return nsnull;
  }

  nsPoint scrollPos = GetScrollPosition();
  // Don't save scroll position if we are at (0,0)
  if (scrollPos == nsPoint(0,0)) {
    return nsnull;
  }

  nsPresState* state = new nsPresState();
  if (!state) {
    return nsnull;
  }
 
  state->SetScrollState(scrollPos);

  return state;
}

void
nsGfxScrollFrameInner::RestoreState(nsPresState* aState)
{
  mRestorePos = aState->GetScrollState();
  mLastPos.x = -1;
  mLastPos.y = -1;
  mDidHistoryRestore = PR_TRUE;
  mLastPos = mScrolledFrame ? GetScrollPosition() : nsPoint(0,0);
}

void
nsGfxScrollFrameInner::PostScrolledAreaEvent()
{
  if (mScrolledAreaEvent.IsPending()) {
    return;
  }
  mScrolledAreaEvent = new ScrolledAreaEvent(this);
  NS_DispatchToCurrentThread(mScrolledAreaEvent.get());
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

  nsScrollAreaEvent event(PR_TRUE, NS_SCROLLEDAREACHANGED, nsnull);
  nsPresContext *prescontext = mOuter->PresContext();
  nsIContent* content = mOuter->GetContent();

  event.mArea = mScrolledFrame->GetOverflowRectRelativeToParent();

  nsIDocument *doc = content->GetCurrentDoc();
  if (doc) {
    nsEventDispatcher::Dispatch(doc, prescontext, &event, nsnull);
  }
}
