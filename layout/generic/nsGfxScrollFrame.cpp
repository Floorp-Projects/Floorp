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
#include "nsIScrollableView.h"
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
#include "nsIDOMHTMLDocument.h"
#include "nsEventDispatcher.h"
#include "nsContentUtils.h"
#include "nsLayoutUtils.h"
#ifdef ACCESSIBILITY
#include "nsIAccessibilityService.h"
#endif
#include "nsDisplayList.h"
#include "nsBidiUtils.h"

//----------------------------------------------------------------------

//----------nsHTMLScrollFrame-------------------------------------------

nsIFrame*
NS_NewHTMLScrollFrame(nsIPresShell* aPresShell, nsStyleContext* aContext, PRBool aIsRoot)
{
  return new (aPresShell) nsHTMLScrollFrame(aPresShell, aContext, aIsRoot);
}

nsHTMLScrollFrame::nsHTMLScrollFrame(nsIPresShell* aShell, nsStyleContext* aContext, PRBool aIsRoot)
  : nsHTMLContainerFrame(aContext),
    mInner(this, aIsRoot, PR_FALSE)
{
}

/**
* Get the view that we are scrolling within the scrolling view. 
* @result child view
*/
nsIFrame* nsHTMLScrollFrame::GetScrolledFrame() const
{
  return mInner.GetScrolledFrame();
}

nsIScrollableView* nsHTMLScrollFrame::GetScrollableView()
{
  return mInner.GetScrollableView();
}

nsPoint nsHTMLScrollFrame::GetScrollPosition() const
{
   nsIScrollableView* s = mInner.GetScrollableView();
   nsPoint scrollPosition;
   s->GetScrollPosition(scrollPosition.x, scrollPosition.y);
   return scrollPosition;
}

void nsHTMLScrollFrame::ScrollTo(nsPoint aScrollPosition, PRUint32 aFlags)
{
   nsIScrollableView* s = mInner.GetScrollableView();
   s->ScrollTo(aScrollPosition.x, aScrollPosition.y, aFlags);
}

nsGfxScrollFrameInner::ScrollbarStyles
nsHTMLScrollFrame::GetScrollbarStyles() const {
  return mInner.GetScrollbarStylesFromFrame();
}

nsMargin nsHTMLScrollFrame::GetDesiredScrollbarSizes(nsBoxLayoutState* aState) {
  return mInner.GetDesiredScrollbarSizes(aState);
}

void nsHTMLScrollFrame::SetScrollbarVisibility(PRBool aVerticalVisible, PRBool aHorizontalVisible)
{
  mInner.mNeverHasVerticalScrollbar = !aVerticalVisible;
  mInner.mNeverHasHorizontalScrollbar = !aHorizontalVisible;
}

nsIBox* nsHTMLScrollFrame::GetScrollbarBox(PRBool aVertical)
{
  return aVertical ? mInner.mVScrollbarBox : mInner.mHScrollbarBox;
}

nsresult
nsHTMLScrollFrame::CreateAnonymousContent(nsTArray<nsIContent*>& aElements)
{
  return mInner.CreateAnonymousContent(aElements);
}

void
nsHTMLScrollFrame::Destroy()
{
  mInner.mScrollEvent.Revoke();
  mInner.mAsyncScrollPortEvent.Revoke();
  nsIScrollableView *view = mInner.GetScrollableView();
  NS_ASSERTION(view, "unexpected null pointer");
  if (view)
    view->RemoveScrollPositionListener(&mInner);
  mInner.Destroy();
  nsHTMLContainerFrame::Destroy();
}

NS_IMETHODIMP
nsHTMLScrollFrame::
SetInitialChildList(nsIAtom*       aListName,
                    nsIFrame*      aChildList)
{
  nsresult rv = nsHTMLContainerFrame::SetInitialChildList(aListName, aChildList);
  mInner.CreateScrollableView();
  mInner.ReloadChildFrames();

  // listen for scroll events.
  mInner.GetScrollableView()->AddScrollPositionListener(&mInner);

  return rv;
}


NS_IMETHODIMP
nsHTMLScrollFrame::AppendFrames(nsIAtom*  aListName,
                                nsIFrame* aFrameList)
{
  NS_ASSERTION(!aListName, "Only main list supported");
  mFrames.AppendFrames(nsnull, aFrameList);
  mInner.ReloadChildFrames();
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLScrollFrame::InsertFrames(nsIAtom*  aListName,
                                nsIFrame* aPrevFrame,
                                nsIFrame* aFrameList)
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
  mFrames.RemoveFrame(aOldFrame);
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

/**
 HTML scrolling implementation

 We rely on the fact that if height is 'auto', changing the height of
 the element does not require reflowing the contents.

 All other things being equal, we prefer layouts with fewer scrollbars showing.
*/

struct ScrollReflowState {
  const nsHTMLReflowState& mReflowState;
  nsBoxLayoutState mBoxState;
  nsGfxScrollFrameInner::ScrollbarStyles mStyles;
  nsMargin mComputedBorder;

  // === Filled in when TryLayout succeeds ===
  // The area of the scrollport, in coordinates relative to the scrollframe
  nsRect mScrollPortRect;
  // The size of the inside-border area
  nsSize mInsideBorderSize;
  // Whether we decided to show the horizontal scrollbar
  PRPackedBool mShowHScrollbar;
  // Whether we decided to show the vertical scrollbar
  PRPackedBool mShowVScrollbar;

  ScrollReflowState(nsIScrollableFrame* aFrame,
                    const nsHTMLReflowState& aState) :
    mReflowState(aState),
    mBoxState(aState.frame->GetPresContext(), aState.rendContext),
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
  nscoord contentHeight = aState->mReflowState.mComputedHeight;
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
                             const nsHTMLReflowMetrics& aKidMetrics,
                             PRBool aAssumeVScroll, PRBool aAssumeHScroll,
                             PRBool aForce)
{
  if ((aState->mStyles.mVertical == NS_STYLE_OVERFLOW_HIDDEN && aAssumeVScroll) ||
      (aState->mStyles.mHorizontal == NS_STYLE_OVERFLOW_HIDDEN && aAssumeHScroll)) {
    NS_ASSERTION(!aForce, "Shouldn't be forcing a hidden scrollbar to show!");
    return PR_FALSE;
  }
  
  nsSize vScrollbarMinSize(0, 0);
  nsSize vScrollbarPrefSize(0, 0);
  if (mInner.mVScrollbarBox) {
    GetScrollbarMetrics(aState->mBoxState, mInner.mVScrollbarBox,
                        &vScrollbarMinSize,
                        aAssumeVScroll ? &vScrollbarPrefSize : nsnull, PR_TRUE);
  }
  nscoord vScrollbarDesiredWidth = aAssumeVScroll ? vScrollbarPrefSize.width : 0;
  nscoord vScrollbarDesiredHeight = aAssumeVScroll ? vScrollbarPrefSize.height : 0;

  nsSize hScrollbarMinSize(0, 0);
  nsSize hScrollbarPrefSize(0, 0);
  if (mInner.mHScrollbarBox) {
    GetScrollbarMetrics(aState->mBoxState, mInner.mHScrollbarBox,
                        &hScrollbarMinSize,
                        aAssumeHScroll ? &hScrollbarPrefSize : nsnull, PR_FALSE);
  }
  nscoord hScrollbarDesiredHeight = aAssumeHScroll ? hScrollbarPrefSize.height : 0;
  nscoord hScrollbarDesiredWidth = aAssumeHScroll ? hScrollbarPrefSize.width : 0;

  // First, compute our inside-border size and scrollport size
  // XXXldb Can we depend more on ComputeSize here?
  nsSize desiredInsideBorderSize;
  desiredInsideBorderSize.width = vScrollbarDesiredWidth +
    PR_MAX(aKidMetrics.width, hScrollbarDesiredWidth);
  desiredInsideBorderSize.height = hScrollbarDesiredHeight +
    PR_MAX(aKidMetrics.height, vScrollbarDesiredHeight);
  aState->mInsideBorderSize =
    ComputeInsideBorderSize(aState, desiredInsideBorderSize);
  nsSize scrollPortSize = nsSize(PR_MAX(0, aState->mInsideBorderSize.width - vScrollbarDesiredWidth),
                                 PR_MAX(0, aState->mInsideBorderSize.height - hScrollbarDesiredHeight));
                                                                                
  if (!aForce) {
    nsRect scrolledRect = mInner.GetScrolledRect(scrollPortSize);

    // If the style is HIDDEN then we already know that aAssumeHScroll is PR_FALSE
    if (aState->mStyles.mHorizontal != NS_STYLE_OVERFLOW_HIDDEN) {
      PRBool wantHScrollbar =
        aState->mStyles.mHorizontal == NS_STYLE_OVERFLOW_SCROLL ||
        scrolledRect.XMost() > scrollPortSize.width ||
        scrolledRect.x < 0;
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
        scrolledRect.YMost() > scrollPortSize.height ||
        scrolledRect.y < 0;
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
  aState->mScrollPortRect = nsRect(scrollPortOrigin, scrollPortSize);
  return PR_TRUE;
}

nsresult
nsHTMLScrollFrame::ReflowScrolledFrame(const ScrollReflowState& aState,
                                       PRBool aAssumeHScroll,
                                       PRBool aAssumeVScroll,
                                       nsHTMLReflowMetrics* aMetrics,
                                       PRBool aFirstPass)
{
  // these could be NS_UNCONSTRAINEDSIZE ... PR_MIN arithmetic should
  // be OK
  nscoord paddingLR = aState.mReflowState.mComputedPadding.LeftRight();

  nscoord availWidth = aState.mReflowState.ComputedWidth() + paddingLR;

  if (aAssumeVScroll) {
    nsSize vScrollbarPrefSize = 
      mInner.mVScrollbarBox->GetPrefSize(NS_CONST_CAST(nsBoxLayoutState&, 
                                                       aState.mBoxState));
    availWidth = PR_MAX(0, availWidth - vScrollbarPrefSize.width);
  }
  // pixel align the content
  nsPresContext* presContext = GetPresContext();
  nscoord twp = nsPresContext::CSSPixelsToAppUnits(1);
  availWidth -=  availWidth % twp;

  if (!aFirstPass)
    mInner.mScrolledFrame->AddStateBits(NS_FRAME_IS_DIRTY);

  // We're forcing the padding on our scrolled frame, so let it know what that
  // padding is.
  mInner.mScrolledFrame->
    SetProperty(nsGkAtoms::usedPaddingProperty,
                new nsMargin(aState.mReflowState.mComputedPadding),
                nsCSSOffsetState::DestroyMarginFunc);  
  
  // Pass PR_FALSE for aInit so we can pass in the correct padding
  nsHTMLReflowState kidReflowState(presContext, aState.mReflowState,
                                   mInner.mScrolledFrame,
                                   nsSize(availWidth, NS_UNCONSTRAINEDSIZE),
                                   -1, -1, PR_FALSE);
  kidReflowState.Init(presContext, -1, -1, nsnull,
                      &aState.mReflowState.mComputedPadding);
  kidReflowState.mFlags.mAssumingHScrollbar = aAssumeHScroll;
  kidReflowState.mFlags.mAssumingVScrollbar = aAssumeVScroll;

  nsReflowStatus status;
  nsresult rv = ReflowChild(mInner.mScrolledFrame, presContext, *aMetrics,
                            kidReflowState, 0, 0,
                            NS_FRAME_NO_MOVE_FRAME | NS_FRAME_NO_MOVE_VIEW, status);
  // Don't resize or position the view because we're going to resize
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
  // check NS_FRAME_OUTSIDE_CHILDREN because it could be set even though the
  // overflow area doesn't include the frame bounds.
  aMetrics->mOverflowArea.UnionRect(aMetrics->mOverflowArea,
                                    nsRect(0, 0, aMetrics->width, aMetrics->height));

  return rv;
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
  if (GetStateBits() & NS_FRAME_FIRST_REFLOW)
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

nsresult
nsHTMLScrollFrame::ReflowContents(ScrollReflowState* aState,
                                  const nsHTMLReflowMetrics& aDesiredSize)
{
  PRBool currentlyUsingVScrollbar = GuessVScrollbarNeeded(*aState);
  nsHTMLReflowMetrics kidDesiredSize(aDesiredSize.mFlags);
  nsresult rv = ReflowScrolledFrame(*aState, PR_FALSE, currentlyUsingVScrollbar,
                                    &kidDesiredSize, PR_TRUE);
  if (NS_FAILED(rv))
    return rv;
  PRBool didUseScrollbar = currentlyUsingVScrollbar;

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
  if (currentlyUsingVScrollbar &&
      aState->mStyles.mVertical != NS_STYLE_OVERFLOW_SCROLL &&
      aState->mStyles.mHorizontal != NS_STYLE_OVERFLOW_SCROLL) {
    nsSize insideBorderSize =
      ComputeInsideBorderSize(aState,
                              nsSize(kidDesiredSize.width, kidDesiredSize.height));
    nsRect scrolledRect = mInner.GetScrolledRect(insideBorderSize);
    if (nsRect(nsPoint(0, 0), insideBorderSize).Contains(scrolledRect)) {
      // Let's pretend we had no vertical scrollbar coming in here
      currentlyUsingVScrollbar = PR_FALSE;
      rv = ReflowScrolledFrame(*aState, PR_FALSE, currentlyUsingVScrollbar,
                               &kidDesiredSize, PR_FALSE);
      if (NS_FAILED(rv))
        return rv;
      didUseScrollbar = PR_FALSE;
    }
  }

  // First try a layout without a horizontal scrollbar, then with.
  if (TryLayout(aState, kidDesiredSize, didUseScrollbar, PR_FALSE, PR_FALSE))
    return NS_OK;
  // XXX Adding a horizontal scrollbar could cause absolute children positioned
  // relative to the bottom padding-edge to need to be reflowed. But we don't,
  // because that would be slow.
  if (TryLayout(aState, kidDesiredSize, didUseScrollbar, PR_TRUE, PR_FALSE))
    return NS_OK;

  PRBool canHaveVerticalScrollbar =
    aState->mStyles.mVertical != NS_STYLE_OVERFLOW_HIDDEN;
  // That didn't work. Try the other setting for the vertical scrollbar.
  // But don't try to show a scrollbar if we know there can't be one.
  if (currentlyUsingVScrollbar || canHaveVerticalScrollbar) {
    nsHTMLReflowMetrics kidRetrySize(aDesiredSize.mFlags);
    rv = ReflowScrolledFrame(*aState, PR_FALSE, !currentlyUsingVScrollbar,
                             &kidRetrySize, PR_FALSE);
    if (NS_FAILED(rv))
      return rv;
    didUseScrollbar = !currentlyUsingVScrollbar;
    // XXX Adding a horizontal scrollbar could cause absolute children positioned
    // relative to the bottom padding-edge to need to be reflowed. But we don't,
    // because that would be slow.
    if (TryLayout(aState, kidRetrySize, didUseScrollbar, PR_FALSE, PR_FALSE))
      return NS_OK;
    if (TryLayout(aState, kidRetrySize, didUseScrollbar, PR_TRUE, PR_FALSE))
      return NS_OK;

    NS_WARNING("Strange content ... we can't find logically consistent scrollbar settings");
  } else {
    NS_WARNING("Strange content ... we can't find logically consistent scrollbar settings");
  }

  // Fall back to no scrollbars --- even if NS_STYLE_OVERFLOW_SCROLL is
  // in effect. They might not fit anyway.
  if (didUseScrollbar) {
    rv = ReflowScrolledFrame(*aState, PR_FALSE, PR_FALSE, &kidDesiredSize, PR_FALSE);
    if (NS_FAILED(rv))
      return rv;
  }
  TryLayout(aState, kidDesiredSize, PR_FALSE, PR_FALSE, PR_TRUE);
  return NS_OK;
}

void
nsHTMLScrollFrame::PlaceScrollArea(const ScrollReflowState& aState)
{
  nsIView* scrollView = mInner.mScrollableView->View();
  nsIViewManager* vm = scrollView->GetViewManager();
  vm->MoveViewTo(scrollView, aState.mScrollPortRect.x, aState.mScrollPortRect.y);
  vm->ResizeView(scrollView, nsRect(nsPoint(0, 0), aState.mScrollPortRect.Size()),
                 PR_TRUE);

  nsIFrame *scrolledFrame = mInner.mScrolledFrame;
  nsIView *scrolledView = scrolledFrame->GetView();
  // Set the x,y of the scrolled frame to the correct value: the displacement
  // from its origin to the origin of this frame
  scrolledFrame->SetPosition(scrolledView->GetOffsetTo(GetView()));

  nsRect scrolledArea;
  scrolledArea.UnionRect(mInner.GetScrolledRect(aState.mScrollPortRect.Size()),
                         nsRect(nsPoint(0,0), aState.mScrollPortRect.Size()));

  // Store the new overflow area. Note that this changes where an outline
  // of the scrolled frame would be painted, but scrolled frames can't have
  // outlines (the outline would go on this scrollframe instead).
  // Using FinishAndStoreOverflow is needed so NS_FRAME_OUTSIDE_CHILDREN
  // gets set correctly.  It also messes with the overflow rect in the
  // -moz-hidden-unscrollable case, but scrolled frames can't have
  // 'overflow' either.
  // This needs to happen before SyncFrameViewAfterReflow so
  // NS_FRAME_OUTSIDE_CHILDREN is set.
  scrolledFrame->FinishAndStoreOverflow(&scrolledArea,
                                        scrolledFrame->GetSize());

  // Note that making the view *exactly* the size of the scrolled area
  // is critical, since the view scrolling code uses the size of the
  // scrolled view to clamp scroll requests.
  nsContainerFrame::SyncFrameViewAfterReflow(scrolledFrame->GetPresContext(),
                                             scrolledFrame,
                                             scrolledView,
                                             &scrolledArea,
                                             NS_FRAME_NO_MOVE_VIEW);

  mInner.PostOverflowEvent();
}

/* virtual */ nscoord
nsHTMLScrollFrame::GetMinWidth(nsIRenderingContext *aRenderingContext)
{
  nscoord result = 0;
  DISPLAY_MIN_WIDTH(this, result);
  return result;
}

/* virtual */ nscoord
nsHTMLScrollFrame::GetPrefWidth(nsIRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_PREF_WIDTH(this, result);
  // XXX Might this make us count padding/border/margin twice?
  result = nsLayoutUtils::IntrinsicForContainer(aRenderingContext,
                             mInner.mScrolledFrame, nsLayoutUtils::PREF_WIDTH);

  nsGfxScrollFrameInner::ScrollbarStyles ss = GetScrollbarStyles();
  if (ss.mVertical != NS_STYLE_OVERFLOW_HIDDEN && // ideal?
      mInner.mVScrollbarBox) {
    nsBoxLayoutState bls(GetPresContext(), aRenderingContext);
    nsSize vScrollbarPrefSize(0, 0);
    GetScrollbarMetrics(bls, mInner.mVScrollbarBox,
                        nsnull, &vScrollbarPrefSize, PR_TRUE);
    result += vScrollbarPrefSize.width;
  }

  return result;
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
      ((frame_) && \
       ((frame_)->GetStateBits() & \
        (NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN)) != 0)

    reflowContents = NEEDS_REFLOW(mInner.mScrolledFrame);
    reflowHScrollbar = NEEDS_REFLOW(mInner.mHScrollbarBox);
    reflowVScrollbar = NEEDS_REFLOW(mInner.mVScrollbarBox);
    reflowScrollCorner = NEEDS_REFLOW(mInner.mScrollCornerBox);

    #undef NEEDS_REFLOW
  }

  nsRect oldScrollAreaBounds = mInner.mScrollableView->View()->GetBounds();
  nsRect oldScrolledAreaBounds = mInner.mScrolledFrame->GetView()->GetBounds();
  state.mComputedBorder = aReflowState.mComputedBorderPadding -
    aReflowState.mComputedPadding;

  nsresult rv = ReflowContents(&state, aDesiredSize);
  if (NS_FAILED(rv))
    return rv;
  
  PlaceScrollArea(state);
  mInner.ScrollToRestoredPosition();

  PRBool didHaveHScrollbar = mInner.mHasHorizontalScrollbar;
  PRBool didHaveVScrollbar = mInner.mHasVerticalScrollbar;
  mInner.mHasHorizontalScrollbar = state.mShowHScrollbar;
  mInner.mHasVerticalScrollbar = state.mShowVScrollbar;
  nsRect newScrollAreaBounds = mInner.mScrollableView->View()->GetBounds();
  nsRect newScrolledAreaBounds = mInner.mScrolledFrame->GetView()->GetBounds();
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
                              oldScrollAreaBounds, state.mScrollPortRect);
    } else {
      mInner.mSkippedScrollbarLayout = PR_TRUE;
    }
  }

  aDesiredSize.width = state.mInsideBorderSize.width +
    state.mComputedBorder.LeftRight();
  aDesiredSize.height = state.mInsideBorderSize.height +
    state.mComputedBorder.TopBottom();

  aDesiredSize.mOverflowArea = nsRect(0, 0, aDesiredSize.width, aDesiredSize.height);
  FinishAndStoreOverflow(&aDesiredSize);

  if (!(GetStateBits() & NS_FRAME_FIRST_REFLOW) &&
      !mInner.mHadNonInitialReflow) {
    mInner.mHadNonInitialReflow = PR_TRUE;
    if (mInner.mIsRoot) {
      // For viewports, record whether we needed a vertical scrollbar
      // after the first non-initial reflow.
      mInner.SaveVScrollbarStateToGlobalHistory();
    }
  }

  aStatus = NS_FRAME_COMPLETE;
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return rv;
}

NS_IMETHODIMP_(nsrefcnt) 
nsHTMLScrollFrame::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt)
nsHTMLScrollFrame::Release(void)
{
    return NS_OK;
}

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
    return accService->CreateHTMLGenericAccessible(NS_STATIC_CAST(nsIFrame*, this), aAccessible);
  }

  return NS_ERROR_FAILURE;
}
#endif

void
nsHTMLScrollFrame::CurPosAttributeChanged(nsIContent* aChild,
                                          PRInt32 aModType)
{
  mInner.CurPosAttributeChanged(aChild);
}

nsresult 
nsHTMLScrollFrame::GetContentOf(nsIContent** aContent)
{
  *aContent = GetContent();
  NS_IF_ADDREF(*aContent);
  return NS_OK;
}

NS_INTERFACE_MAP_BEGIN(nsHTMLScrollFrame)
  NS_INTERFACE_MAP_ENTRY(nsIAnonymousContentCreator)
#ifdef NS_DEBUG
  NS_INTERFACE_MAP_ENTRY(nsIFrameDebug)
#endif
  NS_INTERFACE_MAP_ENTRY(nsIScrollableFrame)
  NS_INTERFACE_MAP_ENTRY(nsIScrollableViewProvider)
  NS_INTERFACE_MAP_ENTRY(nsIStatefulFrame)
NS_INTERFACE_MAP_END_INHERITING(nsHTMLContainerFrame)

//----------nsXULScrollFrame-------------------------------------------

nsIFrame*
NS_NewXULScrollFrame(nsIPresShell* aPresShell, nsStyleContext* aContext, PRBool aIsRoot)
{
  return new (aPresShell) nsXULScrollFrame(aPresShell, aContext, aIsRoot);
}

nsXULScrollFrame::nsXULScrollFrame(nsIPresShell* aShell, nsStyleContext* aContext, PRBool aIsRoot)
  : nsBoxFrame(aShell, aContext, aIsRoot),
    mInner(this, aIsRoot, PR_TRUE)
{
    SetLayoutManager(nsnull);
}

/**
* Get the view that we are scrolling within the scrolling view. 
* @result child view
*/
nsIFrame* nsXULScrollFrame::GetScrolledFrame() const
{
  return mInner.GetScrolledFrame();
}

nsIScrollableView* nsXULScrollFrame::GetScrollableView()
{
  return mInner.GetScrollableView();
}

nsPoint nsXULScrollFrame::GetScrollPosition() const
{
  nsIScrollableView* s = mInner.GetScrollableView();
  nsPoint scrollPosition;
  s->GetScrollPosition(scrollPosition.x, scrollPosition.y);
  return scrollPosition;
}

void nsXULScrollFrame::ScrollTo(nsPoint aScrollPosition, PRUint32 aFlags)
{
  nsIScrollableView* s = mInner.GetScrollableView();
  s->ScrollTo(aScrollPosition.x, aScrollPosition.y, aFlags);
}

nsGfxScrollFrameInner::ScrollbarStyles
nsXULScrollFrame::GetScrollbarStyles() const {
  return mInner.GetScrollbarStylesFromFrame();
}

nsMargin nsXULScrollFrame::GetDesiredScrollbarSizes(nsBoxLayoutState* aState) {
  return mInner.GetDesiredScrollbarSizes(aState);
}

nsMargin nsGfxScrollFrameInner::GetDesiredScrollbarSizes(nsBoxLayoutState* aState) {
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

void nsXULScrollFrame::SetScrollbarVisibility(PRBool aVerticalVisible, PRBool aHorizontalVisible)
{
  mInner.mNeverHasVerticalScrollbar = !aVerticalVisible;
  mInner.mNeverHasHorizontalScrollbar = !aHorizontalVisible;
}

nsIBox* nsXULScrollFrame::GetScrollbarBox(PRBool aVertical)
{
  return aVertical ? mInner.mVScrollbarBox : mInner.mHScrollbarBox;
}

nsresult
nsXULScrollFrame::CreateAnonymousContent(nsTArray<nsIContent*>& aElements)
{
  return mInner.CreateAnonymousContent(aElements);
}

void
nsXULScrollFrame::Destroy()
{
  mInner.mScrollEvent.Revoke();
  mInner.mAsyncScrollPortEvent.Revoke();
  nsIScrollableView *view = mInner.GetScrollableView();
  NS_ASSERTION(view, "unexpected null pointer");
  if (view)
    view->RemoveScrollPositionListener(&mInner);
  mInner.Destroy();
  nsBoxFrame::Destroy();
}

NS_IMETHODIMP
nsXULScrollFrame::SetInitialChildList(nsIAtom*        aListName,
                                      nsIFrame*       aChildList)
{
  nsresult rv = nsBoxFrame::SetInitialChildList(aListName, aChildList);

  mInner.CreateScrollableView();
  mInner.ReloadChildFrames();

  // listen for scroll events.
  mInner.GetScrollableView()->AddScrollPositionListener(&mInner);

  return rv;
}


NS_IMETHODIMP
nsXULScrollFrame::AppendFrames(nsIAtom*        aListName,
                               nsIFrame*       aFrameList)
{
  nsresult rv = nsBoxFrame::AppendFrames(aListName, aFrameList);
  mInner.ReloadChildFrames();
  return rv;
}

NS_IMETHODIMP
nsXULScrollFrame::InsertFrames(nsIAtom*        aListName,
                               nsIFrame*       aPrevFrame,
                               nsIFrame*       aFrameList)
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
  nsIBox::AddCSSMinSize(aState, this, min);
  return min;
}

nsSize
nsXULScrollFrame::GetMaxSize(nsBoxLayoutState& aState)
{
#ifdef DEBUG_LAYOUT
  PropagateDebug(aState);
#endif

  nsSize max(NS_INTRINSICSIZE, NS_INTRINSICSIZE);

  AddBorderAndPadding(max);
  nsIBox::AddCSSMaxSize(aState, this, max);
  return max;
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

NS_IMETHODIMP_(nsrefcnt) 
nsXULScrollFrame::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt)
nsXULScrollFrame::Release(void)
{
    return NS_OK;
}

PRBool
nsXULScrollFrame::IsFrameOfType(PRUint32 aFlags) const
{
  // Override nsBoxFrame.
  return !aFlags;
}

#ifdef NS_DEBUG
NS_IMETHODIMP
nsXULScrollFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("XULScroll"), aResult);
}
#endif

void nsXULScrollFrame::CurPosAttributeChanged(nsIContent* aChild, PRInt32 aModType)
{
  mInner.CurPosAttributeChanged(aChild);
}

NS_IMETHODIMP
nsXULScrollFrame::DoLayout(nsBoxLayoutState& aState)
{
  PRUint32 flags = aState.LayoutFlags();
  nsresult rv = Layout(aState);
  aState.SetLayoutFlags(flags);

  nsBox::DoLayout(aState);
  return rv;
}


nsresult 
nsXULScrollFrame::GetContentOf(nsIContent** aContent)
{
  *aContent = GetContent();
  NS_IF_ADDREF(*aContent);
  return NS_OK;
}

NS_INTERFACE_MAP_BEGIN(nsXULScrollFrame)
  NS_INTERFACE_MAP_ENTRY(nsIAnonymousContentCreator)
#ifdef NS_DEBUG
  NS_INTERFACE_MAP_ENTRY(nsIFrameDebug)
#endif
  NS_INTERFACE_MAP_ENTRY(nsIScrollableFrame)
  NS_INTERFACE_MAP_ENTRY(nsIScrollableViewProvider)
  NS_INTERFACE_MAP_ENTRY(nsIStatefulFrame)
NS_INTERFACE_MAP_END_INHERITING(nsBoxFrame)



//-------------------- Inner ----------------------

nsGfxScrollFrameInner::nsGfxScrollFrameInner(nsContainerFrame* aOuter,
                                             PRBool aIsRoot,
                                             PRBool aIsXUL)
  : mScrollableView(nsnull),
    mHScrollbarBox(nsnull),
    mVScrollbarBox(nsnull),
    mScrolledFrame(nsnull),
    mScrollCornerBox(nsnull),
    mOuter(aOuter),
    mRestoreRect(-1, -1, -1, -1),
    mLastPos(-1, -1),
    mNeverHasVerticalScrollbar(PR_FALSE),
    mNeverHasHorizontalScrollbar(PR_FALSE),
    mHasVerticalScrollbar(PR_FALSE), 
    mHasHorizontalScrollbar(PR_FALSE),
    mViewInitiatedScroll(PR_FALSE),
    mFrameInitiatedScroll(PR_FALSE),
    mDidHistoryRestore(PR_FALSE),
    mIsRoot(aIsRoot),
    mIsXUL(aIsXUL),
    mSupppressScrollbarUpdate(PR_FALSE),
    mSkippedScrollbarLayout(PR_FALSE),
    mDidLoadHistoryVScrollbarHint(PR_FALSE),
    mHistoryVScrollbarHint(PR_FALSE),
    mHadNonInitialReflow(PR_FALSE),
    mHorizontalOverflow(PR_FALSE),
    mVerticalOverflow(PR_FALSE)
{
}

nsGfxScrollFrameInner::~nsGfxScrollFrameInner()
{
}

NS_IMETHODIMP_(nsrefcnt) nsGfxScrollFrameInner::AddRef(void)
{
  return 2;
}

NS_IMETHODIMP_(nsrefcnt) nsGfxScrollFrameInner::Release(void)
{
  return 1;
}

NS_IMPL_QUERY_INTERFACE1(nsGfxScrollFrameInner, nsIScrollPositionListener)

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
    // can just pass aLists directly. We do need to replace aDirtyRect with
    // the scrolled area though, since callers may have restricted aDirtyRect
    // to our bounds.
    nsRect newDirty = GetScrolledRect(GetScrollPortSize()) +
        aBuilder->ToReferenceFrame(mScrolledFrame);
    return mOuter->BuildDisplayListForChild(aBuilder, mScrolledFrame, newDirty, aLists);
  }

  // Overflow clipping can never clip frames outside our subtree, so there
  // is no need to worry about whether we are a moving frame that might clip
  // non-moving frames.
  nsRect frameClip = mScrollableView->View()->GetBounds();
  nsRect dirtyRect;
  // Not all our descendants will be clipped by overflow clipping, but all
  // the ones that aren't clipped will be out of flow frames that have already
  // had dirty rects saved for them by their parent frames calling
  // MarkOutOfFlowChildrenForDisplayList, so it's safe to restrict our
  // dirty rect here.
  dirtyRect.IntersectRect(aDirtyRect, frameClip);
  
  nsDisplayListCollection set;
  rv = mOuter->BuildDisplayListForChild(aBuilder, mScrolledFrame, dirtyRect, set);
  NS_ENSURE_SUCCESS(rv, rv);
  nsRect clip = frameClip + aBuilder->ToReferenceFrame(mOuter);
  // mScrolledFrame may have given us a background, e.g., the scrolled canvas
  // frame below the viewport. If so, we want it to be clipped. We also want
  // to end up on our BorderBackground list.
  // If we are the viewport scrollframe, then clip all our descendants (to ensure
  // that fixed-pos elements get clipped by us).
  rv = mOuter->OverflowClip(aBuilder, set, aLists, clip, PR_TRUE, mIsRoot);
  NS_ENSURE_SUCCESS(rv, rv);

  // Now display the scrollbars and scrollcorner
  nsIFrame* kid = mOuter->GetFirstChild(nsnull);
  // Put each child's background directly onto the content list
  nsDisplayListSet scrollbarSet(aLists, aLists.Content());
  while (kid) {
    if (kid != mScrolledFrame) {
      rv = mOuter->BuildDisplayListForChild(aBuilder, kid, aDirtyRect, scrollbarSet,
                                            nsIFrame::DISPLAY_CHILD_FORCE_PSEUDO_STACKING_CONTEXT);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    kid = kid->GetNextSibling();
  }
  return NS_OK;
}

void
nsGfxScrollFrameInner::InvalidateInternal(const nsRect& aDamageRect,
                                          nscoord aX, nscoord aY, nsIFrame* aForChild,
                                          PRBool aImmediate)
{
  nsPoint pt = mOuter->GetPosition();

  if (aForChild == mScrolledFrame) {
    // restrict aDamageRect to the scrollable view's bounds
    nsRect r;
    if (r.IntersectRect(aDamageRect, mScrollableView->View()->GetBounds() - nsPoint(aX, aY))) {
      mOuter->GetParent()->
        InvalidateInternal(r, aX + pt.x, aY + pt.y, mOuter, aImmediate);
    }
    return;
  }
  
  mOuter->GetParent()->
    InvalidateInternal(aDamageRect, aX + pt.x, aY + pt.y, mOuter, aImmediate);
}

PRBool
nsGfxScrollFrameInner::NeedsClipWidget() const
{
  // Scrollports contained in form controls (e.g., listboxes) don't get
  // widgets.
  for (nsIFrame* parentFrame = mOuter; parentFrame;
       parentFrame = parentFrame->GetParent()) {
    nsIFormControlFrame* fcFrame;
    if ((NS_SUCCEEDED(parentFrame->QueryInterface(NS_GET_IID(nsIFormControlFrame), (void**)&fcFrame)))) {
      return PR_FALSE;
    }
  }

  // Scrollports that don't ever show associated scrollbars don't get
  // widgets, because they will seldom actually be scrolled.
  nsIScrollableFrame *scrollableFrame;
  CallQueryInterface(mOuter, &scrollableFrame);
  ScrollbarStyles scrollbars = scrollableFrame->GetScrollbarStyles();
  if ((scrollbars.mHorizontal == NS_STYLE_OVERFLOW_HIDDEN
       || scrollbars.mHorizontal == NS_STYLE_OVERFLOW_VISIBLE)
      && (scrollbars.mVertical == NS_STYLE_OVERFLOW_HIDDEN
          || scrollbars.mVertical == NS_STYLE_OVERFLOW_VISIBLE)) {
    return PR_FALSE;
  }
 
  return PR_TRUE;
}

void
nsGfxScrollFrameInner::CreateScrollableView()
{
  nsIView* outerView = mOuter->GetView();
  NS_ASSERTION(outerView, "scrollframes must have views");
  nsIViewManager* viewManager = outerView->GetViewManager();
  mScrollableView = viewManager->CreateScrollableView(mOuter->GetRect(), outerView);
  if (!mScrollableView)
    return;

  nsIView* view = mScrollableView->View();

  // Insert the view into the view hierarchy
  viewManager->InsertChild(outerView, view, nsnull, PR_TRUE);

  // Have the scrolling view create its internal widgets
  if (NeedsClipWidget()) {
    mScrollableView->CreateScrollControls(); 
  }
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

nsIView*
nsGfxScrollFrameInner::GetParentViewForChildFrame(nsIFrame* aFrame) const
{
  if (aFrame->GetContent() == mOuter->GetContent()) {
    NS_ASSERTION(mScrollableView, "Scrollable view should have been created by now");
    // scrolled frame, put it under our anonymous view
    return mScrollableView->View();
  }
  // scrollbars and stuff; put them under our regular view
  return mOuter->GetView();
}

nsGfxScrollFrameInner::ScrollbarStyles
nsGfxScrollFrameInner::GetScrollbarStylesFromFrame() const
{
  ScrollbarStyles result;

  nsPresContext* presContext = mOuter->GetPresContext();
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
  nsIScrollableView* scrollingView = GetScrollableView();
  if (!scrollingView) {
    return;
  }
  if (mRestoreRect.y == -1 || mLastPos.x == -1 || mLastPos.y == -1) {
    return;
  }
  // make sure our scroll position did not change for where we last put
  // it. if it does then the user must have moved it, and we no longer
  // need to restore.
  nscoord x = 0;
  nscoord y = 0;
  scrollingView->GetScrollPosition(x, y);

  // if we didn't move, we still need to restore
  if (x == mLastPos.x && y == mLastPos.y) {
    nsRect childRect(0, 0, 0, 0);
    nsIView* child = nsnull;
    nsresult rv = scrollingView->GetScrolledView(child);
    if (NS_SUCCEEDED(rv) && child)
      childRect = child->GetBounds();

    PRInt32 cx, cy, x, y;
    scrollingView->GetScrollPosition(cx,cy);

    x = (int)mRestoreRect.x;
    y = (int)mRestoreRect.y;

    // if our position is greater than the scroll position, scroll.
    // remember that we could be incrementally loading so we may enter
    // and scroll many times.
    if (y != cy || x != cx) {
      scrollingView->ScrollTo(x, y, 0);
      // scrollpostion goes from twips to pixels. this fixes any roundoff
      // problems.
      scrollingView->GetScrollPosition(mLastPos.x, mLastPos.y);
    } else {
      // if we reached the position then stop
      mRestoreRect.y = -1;
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
  mOuter->GetPresContext()->GetPresShell()->
    FlushPendingNotifications(Flush_OnlyReflow);
  if (mAsyncScrollPortEvent.IsPending()) {
    return NS_OK;
  }

  nsSize scrollportSize = GetScrollPortSize();
  nsSize childSize = GetScrolledRect(scrollportSize).Size();

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
                                     mOuter->GetPresContext(), &event);
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
  nsPresContext* presContext = mOuter->GetPresContext();
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

  nsIScrollableFrame *scrollable;
  CallQueryInterface(mOuter, &scrollable);

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
  nsCOMPtr<nsITextControlFrame> textFrame(do_QueryInterface(parent));
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
  rv = nodeInfoManager->GetNodeInfo(nsGkAtoms::scrollbar, nsnull,
                                    kNameSpaceID_XUL, getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  if (canHaveHorizontal) {
    rv = NS_NewElement(getter_AddRefs(mHScrollbarContent),
                       kNameSpaceID_XUL, nodeInfo);
    NS_ENSURE_SUCCESS(rv, rv);
    mHScrollbarContent->SetAttr(kNameSpaceID_None, nsGkAtoms::orient,
                                NS_LITERAL_STRING("horizontal"), PR_FALSE);
    if (!aElements.AppendElement(mHScrollbarContent))
      return NS_ERROR_OUT_OF_MEMORY;
  }

  if (canHaveVertical) {
    rv = NS_NewElement(getter_AddRefs(mVScrollbarContent),
                       kNameSpaceID_XUL, nodeInfo);
    NS_ENSURE_SUCCESS(rv, rv);
    mVScrollbarContent->SetAttr(kNameSpaceID_None, nsGkAtoms::orient,
                                NS_LITERAL_STRING("vertical"), PR_FALSE);
    if (!aElements.AppendElement(mVScrollbarContent))
      return NS_ERROR_OUT_OF_MEMORY;
  }

  if (canHaveHorizontal && canHaveVertical) {
    nodeInfoManager->GetNodeInfo(nsGkAtoms::scrollcorner, nsnull,
                                 kNameSpaceID_XUL, getter_AddRefs(nodeInfo));
    rv = NS_NewElement(getter_AddRefs(mScrollCornerContent),
                       kNameSpaceID_XUL, nodeInfo);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!aElements.AppendElement(mScrollCornerContent))
      return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

void
nsGfxScrollFrameInner::Destroy()
{
  // Unbind any content created in CreateAnonymousContent from the tree
  nsContentUtils::DestroyAnonymousContent(&mHScrollbarContent);
  nsContentUtils::DestroyAnonymousContent(&mVScrollbarContent);
  nsContentUtils::DestroyAnonymousContent(&mScrollCornerContent);
}

NS_IMETHODIMP
nsGfxScrollFrameInner::ScrollPositionWillChange(nsIScrollableView* aScrollable, nscoord aX, nscoord aY)
{
   // Do nothing.
   return NS_OK;
}

/**
 * Called when we want to update the scrollbar position, either because scrolling happened
 * or the user moved the scrollbar position and we need to undo that (e.g., when the user
 * clicks to scroll and we're using smooth scrolling, so we need to put the thumb back
 * to its initial position for the start of the smooth sequence).
 */
void
nsGfxScrollFrameInner::InternalScrollPositionDidChange(nscoord aX, nscoord aY)
{
  if (mVScrollbarBox)
    SetCoordAttribute(mVScrollbarBox, nsGkAtoms::curpos,
                      aY - GetScrolledRect(GetScrollPortSize()).y);
  
  if (mHScrollbarBox)
    SetCoordAttribute(mHScrollbarBox, nsGkAtoms::curpos,
                      aX - GetScrolledRect(GetScrollPortSize()).x);
}

/**
 * Called whenever actual scrolling happens for any reason.
 */
NS_IMETHODIMP
nsGfxScrollFrameInner::ScrollPositionDidChange(nsIScrollableView* aScrollable, nscoord aX, nscoord aY)
{
  NS_ASSERTION(!mViewInitiatedScroll, "Cannot reenter ScrollPositionDidChange");

  // Update frame position to match view offsets
  nsPoint childOffset = mScrolledFrame->GetView()->GetOffsetTo(mOuter->GetView());
  mScrolledFrame->SetPosition(childOffset);

  mViewInitiatedScroll = PR_TRUE;
  InternalScrollPositionDidChange(aX, aY);
  mViewInitiatedScroll = PR_FALSE;
  
  PostScrollEvent();
  
  return NS_OK;
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
  // In case 2), we don't need to scroll the view because the scrolling
  // has already happened. In case 3) we don't need to scroll because
  // we're just adjusting the scrollbars back to the correct setting
  // for the view.
  //
  // Cases 1) and 3) do not indicate that actual scrolling has happened. Only
  // case 2) indicates actual scrolling. Therefore we do not fire onscroll
  // here, but in ScrollPositionDidChange.
  // 
  // We used to detect this case implicitly because we'd compare the
  // scrollbar attributes with the view's current scroll position and
  // bail out if they were equal. But that approach is fragile; it can
  // fail when, for example, the view scrolls horizontally and
  // vertically simultaneously; we'll get here when only the vertical
  // attribute has been set, so the attributes and the view scroll
  // position don't yet agree, and we'd tell the view to scroll to the
  // new vertical position and the old horizontal position! Even worse
  // things could happen when smooth scrolling got involved ... crashes
  // and other terrors.
  if (mViewInitiatedScroll || mFrameInitiatedScroll) return;

  nsRect scrolledRect = GetScrolledRect(GetScrollPortSize());

  nscoord x = GetCoordAttribute(mHScrollbarBox, nsGkAtoms::curpos,
                                -scrolledRect.x) +
              scrolledRect.x;
  nscoord y = GetCoordAttribute(mVScrollbarBox, nsGkAtoms::curpos,
                                -scrolledRect.y) +
              scrolledRect.y;

  // Make sure the scrollbars indeed moved before firing the event.
  // I think it is OK to prevent the call to ScrollbarChanged()
  // if we didn't actually move. The following check is the first
  // thing ScrollbarChanged() does anyway, before deciding to move 
  // the scrollbars. 
  nscoord curPosX=0, curPosY=0;
  nsIScrollableView* s = GetScrollableView();
  if (s) {
    s->GetScrollPosition(curPosX, curPosY);
    if (x == curPosX && y == curPosY)
      return;

    PRBool isSmooth = aContent->HasAttr(kNameSpaceID_None, nsGkAtoms::smooth);
        
    if (isSmooth) {
      // Make sure an attribute-setting callback occurs even if the view
      // didn't actually move yet.  We need to make sure other listeners
      // see that the scroll position is not (yet) what they thought it
      // was.

      NS_ASSERTION(!mFrameInitiatedScroll, "Unexpected reentry");
      // Make sure we don't do anything in when the view calls us back
      // for this scroll operation.
      mFrameInitiatedScroll = PR_TRUE;
      InternalScrollPositionDidChange(curPosX, curPosY);
      mFrameInitiatedScroll = PR_FALSE;
    }
    ScrollbarChanged(mOuter->GetPresContext(), x, y, isSmooth ? NS_VMREFRESH_SMOOTHSCROLL : 0);
  }
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
  nsPresContext* prescontext = mOuter->GetPresContext();
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
  return mInner ? mInner->FireScrollPortEvent() : NS_OK;
}

PRBool
nsXULScrollFrame::AddHorizontalScrollbar(nsBoxLayoutState& aState,
                                         nsRect& aScrollAreaSize, PRBool aOnTop)
{
  if (!mInner.mHScrollbarBox)
    return PR_TRUE;

  return AddRemoveScrollbar(aState, aScrollAreaSize, aOnTop, PR_TRUE, PR_TRUE);
}

PRBool
nsXULScrollFrame::AddVerticalScrollbar(nsBoxLayoutState& aState,
                                       nsRect& aScrollAreaSize, PRBool aOnRight)
{
  if (!mInner.mVScrollbarBox)
    return PR_TRUE;

  return AddRemoveScrollbar(aState, aScrollAreaSize, aOnRight, PR_FALSE, PR_TRUE);
}

void
nsXULScrollFrame::RemoveHorizontalScrollbar(nsBoxLayoutState& aState,
                                            nsRect& aScrollAreaSize, PRBool aOnTop)
{
  // removing a scrollbar should always fit
#ifdef DEBUG
  PRBool result =
#endif
  AddRemoveScrollbar(aState, aScrollAreaSize, aOnTop, PR_TRUE, PR_FALSE);
  NS_ASSERTION(result, "Removing horizontal scrollbar failed to fit??");
}

void
nsXULScrollFrame::RemoveVerticalScrollbar(nsBoxLayoutState& aState,
                                          nsRect& aScrollAreaSize, PRBool aOnRight)
{
  // removing a scrollbar should always fit
#ifdef DEBUG
  PRBool result =
#endif
  AddRemoveScrollbar(aState, aScrollAreaSize, aOnRight, PR_FALSE, PR_FALSE);
  NS_ASSERTION(result, "Removing vertical scrollbar failed to fit??");
}

PRBool
nsXULScrollFrame::AddRemoveScrollbar(nsBoxLayoutState& aState, nsRect& aScrollAreaSize,
                                     PRBool aOnTop, PRBool aHorizontal, PRBool aAdd)
{
  if (aHorizontal) {
     if (mInner.mNeverHasHorizontalScrollbar || !mInner.mHScrollbarBox)
       return PR_FALSE;

     nsSize hSize = mInner.mHScrollbarBox->GetPrefSize(aState);
     nsBox::AddMargin(mInner.mHScrollbarBox, hSize);

     mInner.SetScrollbarVisibility(mInner.mHScrollbarBox, aAdd);

     PRBool hasHorizontalScrollbar;
     PRBool fit = AddRemoveScrollbar(hasHorizontalScrollbar, aScrollAreaSize.y, aScrollAreaSize.height, hSize.height, aOnTop, aAdd);
     mInner.mHasHorizontalScrollbar = hasHorizontalScrollbar;    // because mHasHorizontalScrollbar is a PRPackedBool
     if (!fit)
        mInner.SetScrollbarVisibility(mInner.mHScrollbarBox, !aAdd);

     return fit;
  } else {
     if (mInner.mNeverHasVerticalScrollbar || !mInner.mVScrollbarBox)
       return PR_FALSE;

     nsSize vSize = mInner.mVScrollbarBox->GetPrefSize(aState);
     nsBox::AddMargin(mInner.mVScrollbarBox, vSize);

     mInner.SetScrollbarVisibility(mInner.mVScrollbarBox, aAdd);

     PRBool hasVerticalScrollbar;
     PRBool fit = AddRemoveScrollbar(hasVerticalScrollbar, aScrollAreaSize.x, aScrollAreaSize.width, vSize.width, aOnTop, aAdd);
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
nsXULScrollFrame::LayoutScrollArea(nsBoxLayoutState& aState, const nsRect& aRect)
{
  nsIView* scrollView = mInner.mScrollableView->View();
  nsIViewManager* vm = scrollView->GetViewManager();
  vm->MoveViewTo(scrollView, aRect.x, aRect.y);
  vm->ResizeView(scrollView, nsRect(nsPoint(0, 0), aRect.Size()), PR_TRUE);

  PRUint32 oldflags = aState.LayoutFlags();
  nsPoint childOffset =
    mInner.mScrolledFrame->GetView()->GetOffsetTo(GetView());
  nsRect childRect = nsRect(childOffset, aRect.Size());

  PRInt32 flags = NS_FRAME_NO_MOVE_VIEW;

  nsSize minSize = mInner.mScrolledFrame->GetMinSize(aState);
  
  if (minSize.height > childRect.height)
    childRect.height = minSize.height;
  
  if (minSize.width > childRect.width)
    childRect.width = minSize.width;

  aState.SetLayoutFlags(flags);
  mInner.mScrolledFrame->SetBounds(aState, childRect);
  mInner.mScrolledFrame->Layout(aState);

  childRect = mInner.mScrolledFrame->GetRect();

  if (childRect.width < aRect.width || childRect.height < aRect.height)
  {
    childRect.width = PR_MAX(childRect.width, aRect.width);
    childRect.height = PR_MAX(childRect.height, aRect.height);

    // remove overflow area when we update the bounds,
    // because we've already accounted for it
    mInner.mScrolledFrame->SetBounds(aState, childRect, PR_TRUE);
  }

  aState.SetLayoutFlags(oldflags);

}

void nsGfxScrollFrameInner::PostOverflowEvent()
{
  if (mAsyncScrollPortEvent.IsPending())
    return;

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
    nsPresContext *presContext = mOuter->GetPresContext();
    nsIDocument *document = presContext->Document();
    nsIContent *root = document->GetRootContent();

    // But for HTML we want the body element.
    nsCOMPtr<nsIDOMHTMLDocument> htmlDoc = do_QueryInterface(document);
    if (htmlDoc && !document->IsCaseSensitive()) { // HTML, not XHTML
      nsCOMPtr<nsIDOMHTMLElement> body;
      htmlDoc->GetBody(getter_AddRefs(body));
      nsCOMPtr<nsIContent> bodyContent = do_QueryInterface(body);
      if (bodyContent)
        root = bodyContent; // we can trust the document to hold on to it
    }

    if (root) {
      nsIFrame *rootsFrame =
        presContext->PresShell()->GetPrimaryFrameFor(root);
      if (rootsFrame)
        frame = rootsFrame;
    }
  }

  return frame->GetStyleVisibility()->mDirection != NS_STYLE_DIRECTION_RTL;
}

PRBool
nsGfxScrollFrameInner::IsScrollbarOnRight() const
{
  nsPresContext *presContext = mOuter->GetPresContext();
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

  // the scroll area size starts off as big as our content area
  nsRect scrollAreaRect(clientRect);

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
     AddHorizontalScrollbar(aState, scrollAreaRect, scrollbarBottom);

  if (mInner.mHasVerticalScrollbar)
     AddVerticalScrollbar(aState, scrollAreaRect, scrollbarRight);
     
  nsRect oldScrollAreaBounds = mInner.mScrollableView->View()->GetBounds();

  // layout our the scroll area
  LayoutScrollArea(aState, scrollAreaRect);
  
  // now look at the content area and see if we need scrollbars or not
  PRBool needsLayout = PR_FALSE;

  // if we have 'auto' scrollbars look at the vertical case
  if (styles.mVertical != NS_STYLE_OVERFLOW_SCROLL) {
    // These are only good until the call to LayoutScrollArea.
    nsRect scrolledRect = mInner.GetScrolledRect(scrollAreaRect.Size());
    nsSize scrolledContentSize(scrolledRect.XMost(), scrolledRect.YMost());

    // There are two cases to consider
      if (scrolledContentSize.height <= scrollAreaRect.height
          || styles.mVertical != NS_STYLE_OVERFLOW_AUTO) {
        if (mInner.mHasVerticalScrollbar) {
          // We left room for the vertical scrollbar, but it's not needed;
          // remove it.
          RemoveVerticalScrollbar(aState, scrollAreaRect, scrollbarRight);
          needsLayout = PR_TRUE;
        }
      } else {
        if (!mInner.mHasVerticalScrollbar) {
          // We didn't leave room for the vertical scrollbar, but it turns
          // out we needed it
          if (AddVerticalScrollbar(aState, scrollAreaRect, scrollbarRight))
            needsLayout = PR_TRUE;
        }
    }

    // ok layout at the right size
    if (needsLayout) {
       nsBoxLayoutState resizeState(aState);
       LayoutScrollArea(resizeState, scrollAreaRect);
       needsLayout = PR_FALSE;
    }
  }


  // if scrollbars are auto look at the horizontal case
  if (styles.mHorizontal != NS_STYLE_OVERFLOW_SCROLL)
  {
    // These are only good until the call to LayoutScrollArea.
    nsRect scrolledRect = mInner.GetScrolledRect(scrollAreaRect.Size());
    nsSize scrolledContentSize(scrolledRect.XMost(), scrolledRect.YMost());

    // if the child is wider that the scroll area
    // and we don't have a scrollbar add one.
    if (scrolledContentSize.width > scrollAreaRect.width
        && styles.mHorizontal == NS_STYLE_OVERFLOW_AUTO) {

      if (!mInner.mHasHorizontalScrollbar) {
           // no scrollbar? 
          if (AddHorizontalScrollbar(aState, scrollAreaRect, scrollbarBottom))
             needsLayout = PR_TRUE;

           // if we added a horizonal scrollbar and we did not have a vertical
           // there is a chance that by adding the horizonal scrollbar we will
           // suddenly need a vertical scrollbar. Is a special case but its 
           // important.
           //if (!mHasVerticalScrollbar && scrolledContentSize.height > scrollAreaRect.height - sbSize.height)
           //  printf("****Gfx Scrollbar Special case hit!!*****\n");
           
      }
    } else {
        // if the area is smaller or equal to and we have a scrollbar then
        // remove it.
      if (mInner.mHasHorizontalScrollbar) {
        RemoveHorizontalScrollbar(aState, scrollAreaRect, scrollbarBottom);
        needsLayout = PR_TRUE;
      }
    }
  }

  // we only need to set the rect. The inner child stays the same size.
  if (needsLayout) {
     nsBoxLayoutState resizeState(aState);
     LayoutScrollArea(resizeState, scrollAreaRect);
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
    RemoveHorizontalScrollbar(aState, scrollAreaRect, scrollbarBottom);
    needsLayout = PR_TRUE;
  }
  // Now disable vertical scrollbar if necessary
  if (mInner.mHasVerticalScrollbar &&
      (vMinSize.height > clientRect.height - hMinSize.height
       || vMinSize.width > clientRect.width)) {
    RemoveVerticalScrollbar(aState, scrollAreaRect, scrollbarRight);
    needsLayout = PR_TRUE;
  }

  // we only need to set the rect. The inner child stays the same size.
  if (needsLayout) {
    nsBoxLayoutState resizeState(aState);
    mInner.mScrolledFrame->AddStateBits(NS_FRAME_IS_DIRTY);
    LayoutScrollArea(resizeState, scrollAreaRect);
  }

  if (!mInner.mSupppressScrollbarUpdate) { 
    mInner.LayoutScrollbars(aState, clientRect, oldScrollAreaBounds, scrollAreaRect);
  }
  mInner.ScrollToRestoredPosition();
  if (!(GetStateBits() & NS_FRAME_FIRST_REFLOW)) {
    mInner.mHadNonInitialReflow = PR_TRUE;
  }

  mInner.PostOverflowEvent();
  return NS_OK;
}

void
nsGfxScrollFrameInner::LayoutScrollbars(nsBoxLayoutState& aState,
                                        const nsRect& aContentArea,
                                        const nsRect& aOldScrollArea,
                                        const nsRect& aScrollArea)
{
  NS_ASSERTION(!mSupppressScrollbarUpdate,
               "This should have been suppressed");
    
  nsPresContext* presContext = aState.PresContext();
  const nsStyleFont* font = mOuter->GetStyleFont();
  const nsFont& f = font->mFont;
  nsCOMPtr<nsIFontMetrics> fm = presContext->GetMetricsFor(f);
  nscoord fontHeight = 1;
  NS_ASSERTION(fm,"FontMetrics is null assuming fontHeight == 1");
  if (fm)
    fm->GetHeight(fontHeight);

  nsRect scrolledContentRect = GetScrolledRect(aScrollArea.Size());

  nscoord minX = scrolledContentRect.x;
  nscoord maxX = scrolledContentRect.XMost() - aScrollArea.width;

  nscoord minY = scrolledContentRect.y;
  nscoord maxY = scrolledContentRect.YMost() - aScrollArea.height;

  nsIScrollableView* scrollable = GetScrollableView();
  scrollable->SetLineHeight(fontHeight);

  // Suppress handling of the curpos attribute changes we make here.
  PRBool oldFrameInitiatedScroll = mFrameInitiatedScroll;
  mFrameInitiatedScroll = PR_TRUE;

  if (mVScrollbarBox) {
    NS_PRECONDITION(mVScrollbarBox->IsBoxFrame(), "Must be a box frame!");
    nscoord curPosX, curPosY;
    scrollable->GetScrollPosition(curPosX, curPosY);
    // Scrollbars assume zero is the minimum position, so translate for them.
    SetCoordAttribute(mVScrollbarBox, nsGkAtoms::curpos, curPosY - minY);
    SetScrollbarEnabled(mVScrollbarBox, maxY - minY);
    SetCoordAttribute(mVScrollbarBox, nsGkAtoms::maxpos, maxY - minY);
    SetCoordAttribute(mVScrollbarBox, nsGkAtoms::pageincrement, nscoord(aScrollArea.height - fontHeight));
    SetCoordAttribute(mVScrollbarBox, nsGkAtoms::increment, fontHeight);

    nsRect vRect(aScrollArea);
    vRect.width = aContentArea.width - aScrollArea.width;
    vRect.x = IsScrollbarOnRight() ? aScrollArea.XMost() : aContentArea.x;
    nsMargin margin;
    mVScrollbarBox->GetMargin(margin);
    vRect.Deflate(margin);
    nsBoxFrame::LayoutChildAt(aState, mVScrollbarBox, vRect);
  }
    
  if (mHScrollbarBox) {
    NS_PRECONDITION(mHScrollbarBox->IsBoxFrame(), "Must be a box frame!");
    nscoord curPosX, curPosY;
    scrollable->GetScrollPosition(curPosX, curPosY);
    // Scrollbars assume zero is the minimum position, so translate for them.
    SetCoordAttribute(mHScrollbarBox, nsGkAtoms::curpos, curPosX - minX);
    SetScrollbarEnabled(mHScrollbarBox, maxX - minX);
    SetCoordAttribute(mHScrollbarBox, nsGkAtoms::maxpos, maxX - minX);
    SetCoordAttribute(mHScrollbarBox, nsGkAtoms::pageincrement, nscoord(float(aScrollArea.width)*0.8));
    SetCoordAttribute(mHScrollbarBox, nsGkAtoms::increment, nsPresContext::CSSPixelsToAppUnits(10));

    nsRect hRect(aScrollArea);
    hRect.height = aContentArea.height - aScrollArea.height;
    hRect.y = PR_TRUE ? aScrollArea.YMost() : aContentArea.y;
    nsMargin margin;
    mHScrollbarBox->GetMargin(margin);
    hRect.Deflate(margin);
    nsBoxFrame::LayoutChildAt(aState, mHScrollbarBox, hRect);
  }

  mFrameInitiatedScroll = oldFrameInitiatedScroll;
  // We used to rely on the curpos attribute changes above to scroll the
  // view.  However, for scrolling to the left of the viewport, we
  // rescale the curpos attribute, which means that operations like
  // resizing the window while it is scrolled all the way to the left
  // hold the curpos attribute constant at 0 while still requiring
  // scrolling.  So we suppress the effect of the changes above with
  // mFrameInitiatedScroll and call CurPosAttributeChanged here.
  // (It actually even works some of the time without this, thanks to
  // nsSliderFrame::AttributeChanged's handling of maxpos, but not when
  // we hide the scrollbar on a large size change, such as
  // maximization.)
  if (mHScrollbarBox || mVScrollbarBox)
    CurPosAttributeChanged(mVScrollbarBox ? mVScrollbarBox->GetContent()
                                          : mHScrollbarBox->GetContent());

  // place the scrollcorner
  if (mScrollCornerBox) {
    NS_PRECONDITION(mScrollCornerBox->IsBoxFrame(), "Must be a box frame!");
    nsRect r(0, 0, 0, 0);
    if (aContentArea.x != aScrollArea.x) {
      // scrollbar (if any) on left
      r.x = aContentArea.x;
      r.width = aScrollArea.x - aContentArea.x;
      NS_ASSERTION(r.width >= 0, "Scroll area should be inside client rect");
    } else {
      // scrollbar (if any) on right
      r.x = aScrollArea.XMost();
      r.width = aContentArea.XMost() - aScrollArea.XMost();
      NS_ASSERTION(r.width >= 0, "Scroll area should be inside client rect");
    }
    if (aContentArea.y != aScrollArea.y) {
      // scrollbar (if any) on top
      r.y = aContentArea.y;
      r.height = aScrollArea.y - aContentArea.y;
      NS_ASSERTION(r.height >= 0, "Scroll area should be inside client rect");
    } else {
      // scrollbar (if any) on bottom
      r.y = aScrollArea.YMost();
      r.height = aContentArea.YMost() - aScrollArea.YMost();
      NS_ASSERTION(r.height >= 0, "Scroll area should be inside client rect");
    }
    nsBoxFrame::LayoutChildAt(aState, mScrollCornerBox, r); 
  }

  // may need to update fixed position children of the viewport,
  // if the client area changed size because of an incremental
  // reflow of a descendant.  (If the outer frame is dirty, the fixed
  // children will be re-laid out anyway)
  if (aOldScrollArea.Size() != aScrollArea.Size() && 
      !(mOuter->GetStateBits() & NS_FRAME_IS_DIRTY) &&
      mIsRoot) {
    nsIFrame* parentFrame = mOuter->GetParent();
    for (nsIFrame *fixedChild =
           parentFrame->GetFirstChild(nsGkAtoms::fixedList);
         fixedChild; fixedChild = fixedChild->GetNextSibling()) {
      // force a reflow of the fixed child
      fixedChild->AddStateBits(NS_FRAME_HAS_DIRTY_CHILDREN);
      // XXX Will this work given where we currently are in reflow?
      mOuter->GetPresContext()->PresShell()->
        FrameNeedsReflow(fixedChild, nsIPresShell::eResize);
    }
  }
}

void
nsGfxScrollFrameInner::ScrollbarChanged(nsPresContext* aPresContext, nscoord aX, nscoord aY, PRUint32 aFlags)
{
  nsIScrollableView* scrollable = GetScrollableView();
  scrollable->ScrollTo(aX, aY, aFlags);
 // printf("scrolling to: %d, %d\n", aX, aY);
}

void
nsGfxScrollFrameInner::SetScrollbarEnabled(nsIBox* aBox, nscoord aMaxPos, PRBool aReflow)
{
  mOuter->GetPresContext()->PresShell()->PostAttributeChange(
    aBox->GetContent(),
    kNameSpaceID_None,
    nsGkAtoms::disabled,
    NS_LITERAL_STRING("true"),
    aReflow,
    aMaxPos ? eChangeType_Remove : eChangeType_Set);
}

/**
 * Returns whether it actually needed to change the attribute
 */
PRBool
nsGfxScrollFrameInner::SetCoordAttribute(nsIBox* aBox, nsIAtom* aAtom, nscoord aSize, PRBool aReflow)
{
  // convert to pixels
  aSize = nsPresContext::AppUnitsToIntCSSPixels(aSize);

  // only set the attribute if it changed.

  nsIContent *content = aBox->GetContent();

  nsAutoString newValue;
  newValue.AppendInt(aSize);

  if (content->AttrValueIs(kNameSpaceID_None, aAtom, newValue, eCaseMatters))
    return PR_FALSE;

  content->SetAttr(kNameSpaceID_None, aAtom, newValue, aReflow);
  return PR_TRUE;
}

nsRect
nsGfxScrollFrameInner::GetScrolledRect(const nsSize& aScrollPortSize) const
{
  nsRect result = mScrolledFrame->GetOverflowRect();
  nscoord x1 = result.x, x2 = result.XMost(),
          y1 = result.y, y2 = result.YMost();
  if (y1 < 0)
    y1 = 0;
  if (IsLTR() || mIsXUL) {
    if (x1 < 0)
      x1 = 0;
  } else {
    if (x2 > aScrollPortSize.width)
      x2 = aScrollPortSize.width;
  }

  return nsRect(x1, y1, x2 - x1, y2 - y1);
}

nsMargin
nsGfxScrollFrameInner::GetActualScrollbarSizes() const {
  nsMargin border;
  mOuter->GetBorder(border);
  nsRect r(nsPoint(0,0), mOuter->GetSize());
  r.Deflate(border);
  nsRect scrollArea = mScrollableView->View()->GetBounds();

  return nsMargin(scrollArea.x - r.x, scrollArea.y - r.y,
                  r.XMost() - scrollArea.XMost(),
                  r.YMost() - scrollArea.YMost());
}

void
nsGfxScrollFrameInner::SetScrollbarVisibility(nsIBox* aScrollbar, PRBool aVisible)
{
  if (!aScrollbar)
    return;

  nsIScrollbarFrame* scrollbar;
  CallQueryInterface(aScrollbar, &scrollbar);
  if (scrollbar) {
    // See if we have a mediator.
    nsIScrollbarMediator* mediator = scrollbar->GetScrollbarMediator();
    if (mediator) {
      // Inform the mediator of the visibility change.
      mediator->VisibilityChanged(scrollbar, aVisible);
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
  nsIPresShell* shell = aFrame->GetPresContext()->GetPresShell();
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

  nsIScrollbarMediator* mediator;
  CallQueryInterface(GetScrolledFrame(), &mediator);
  if (mediator) {
    // child handles its own scroll state, so don't bother saving state here
    return nsnull;
  }

  nsIScrollableView* scrollingView = GetScrollableView();
  PRInt32 x,y;
  scrollingView->GetScrollPosition(x,y);
  // Don't save scroll position if we are at (0,0)
  if (!x && !y) {
    return nsnull;
  }

  nsIView* child = nsnull;
  scrollingView->GetScrolledView(child);
  if (!child) {
    return nsnull;
  }

  nsRect childRect = child->GetBounds();
  childRect.x = x;
  childRect.y = y;
  nsAutoPtr<nsPresState> state;
  nsresult rv = NS_NewPresState(getter_Transfers(state));
  NS_ENSURE_SUCCESS(rv, nsnull);

  state->SetScrollState(childRect);

  return state.forget();
}

void
nsGfxScrollFrameInner::RestoreState(nsPresState* aState)
{
  mRestoreRect = aState->GetScrollState();
  mLastPos.x = -1;
  mLastPos.y = -1;
  mDidHistoryRestore = PR_TRUE;
  nsIScrollableView* scrollingView = GetScrollableView();
  if (scrollingView) {
    scrollingView->GetScrollPosition(mLastPos.x, mLastPos.y);
  } else {
    mLastPos = nsPoint(0, 0);
  }
}
