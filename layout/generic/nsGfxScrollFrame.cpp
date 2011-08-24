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
 *   Mats Palmgren <matspal@gmail.com>
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
#include "nsFontMetrics.h"
#include "nsIDocumentObserver.h"
#include "nsIDocument.h"
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
#ifdef ACCESSIBILITY
#include "nsAccessibilityService.h"
#endif
#include "nsBidiUtils.h"
#include "nsFrameManager.h"
#include "mozilla/Preferences.h"
#include "nsILookAndFeel.h"
#include "mozilla/dom/Element.h"
#include "FrameLayerBuilder.h"
#include "nsSMILKeySpline.h"

using namespace mozilla;
using namespace mozilla::dom;

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
                                            PRUint32 aFilter)
{
  mInner.AppendAnonymousContentTo(aElements, aFilter);
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
      nsRect damage = aDamageRect + nsPoint(aX, aY);
      // This is the damage rect that we're going to pass up to our parent.
      nsRect parentDamage;
      // If we're using a displayport, we might be displaying an area
      // different than our scroll port and the damage needs to be
      // clipped to that instead.
      nsRect displayport;
      PRBool usingDisplayport = nsLayoutUtils::GetDisplayPort(GetContent(),
                                                              &displayport);
      if (usingDisplayport) {
        parentDamage.IntersectRect(damage, displayport);
      } else {
        parentDamage.IntersectRect(damage, mInner.mScrollPort);
      }

      if (IsScrollingActive()) {
        // This is the damage rect that we're going to pass up and
        // only request invalidation of ThebesLayers for.
        // damage is now in our coordinate system, which means it was
        // translated using the current scroll position. Adjust it to
        // reflect the scroll position at last paint, since that's what
        // the ThebesLayers are currently set up for.
        // This should not be clipped to the scrollport since ThebesLayers
        // can contain content outside the scrollport that may need to be
        // invalidated.
        nsRect thebesLayerDamage = damage + GetScrollPosition() - mInner.mScrollPosAtLastPaint;
        if (parentDamage.IsEqualInterior(thebesLayerDamage)) {
          // This single call will take care of both rects
          nsHTMLContainerFrame::InvalidateInternal(parentDamage, 0, 0, aForChild, aFlags);
        } else {
          // Invalidate rects separately
          if (!(aFlags & INVALIDATE_NO_THEBES_LAYERS)) {
            nsHTMLContainerFrame::InvalidateInternal(thebesLayerDamage, 0, 0, aForChild,
                                                     aFlags | INVALIDATE_ONLY_THEBES_LAYERS);
          }
          if (!(aFlags & INVALIDATE_ONLY_THEBES_LAYERS) && !parentDamage.IsEmpty()) {
            nsHTMLContainerFrame::InvalidateInternal(parentDamage, 0, 0, aForChild,
                                                     aFlags | INVALIDATE_NO_THEBES_LAYERS);
          }
        }
      } else {
        if (!parentDamage.IsEmpty()) {
          nsHTMLContainerFrame::InvalidateInternal(parentDamage, 0, 0, aForChild, aFlags);
        }
      }

      if (mInner.mIsRoot && !parentDamage.IsEqualInterior(damage)) {
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

  nsPresContext* presContext = PresContext();

  // Pass PR_FALSE for aInit so we can pass in the correct padding.
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
  aMetrics->UnionOverflowAreasWithDesiredBounds();

  aState->mContentsOverflowArea = aMetrics->ScrollableOverflow();
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
    // Assume that there will be a scrollbar; it seems to me
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
      mInner.GetScrolledRectInternal(kidDesiredSize.ScrollableOverflow(),
                                     insideBorderSize);
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
                      nsnull, &vScrollbarPrefSize, PR_TRUE);
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

PRBool
nsHTMLScrollFrame::IsCollapsed(nsBoxLayoutState& aBoxLayoutState)
{
  // We're never collapsed in the box sense.
  return PR_FALSE;
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

  return nsnull;
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
    reflowScrollCorner = NEEDS_REFLOW(mInner.mScrollCornerBox) ||
                         NEEDS_REFLOW(mInner.mResizerBox);

    #undef NEEDS_REFLOW
  }

  if (mInner.mIsRoot) {
    mInner.mCollapsedResizer = PR_TRUE;

    nsIContent* browserRoot = GetBrowserRoot(mContent);
    if (browserRoot) {
      PRBool showResizer = browserRoot->HasAttr(kNameSpaceID_None, nsGkAtoms::showresizer);
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
    mInner.mPostedReflowCallback = PR_TRUE;
  }

  PRBool didHaveHScrollbar = mInner.mHasHorizontalScrollbar;
  PRBool didHaveVScrollbar = mInner.mHasVerticalScrollbar;
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

  aDesiredSize.SetOverflowAreasToDesiredBounds();

  CheckInvalidateSizeChange(aDesiredSize);

  FinishAndStoreOverflow(&aDesiredSize);

  if (!InInitialReflow() && !mInner.mHadNonInitialReflow) {
    mInner.mHadNonInitialReflow = PR_TRUE;
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

#ifdef NS_DEBUG
NS_IMETHODIMP
nsHTMLScrollFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("HTMLScroll"), aResult);
}
#endif

#ifdef ACCESSIBILITY
already_AddRefed<nsAccessible>
nsHTMLScrollFrame::CreateAccessible()
{
  if (!IsFocusable()) {
    return nsnull;
  }
  // Focusable via CSS, so needs to be in accessibility hierarchy
  nsAccessibilityService* accService = nsIPresShell::AccService();
  if (accService) {
    return accService->CreateHyperTextAccessible(mContent,
                                                 PresContext()->PresShell());
  }

  return nsnull;
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
    mInner(this, aIsRoot)
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
                                           PRUint32 aFilter)
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
    nsRect damage = aDamageRect + nsPoint(aX, aY);
    // This is the damage rect that we're going to pass up to our parent.
    nsRect parentDamage;
    // If we're using a displayport, we might be displaying an area
    // different than our scroll port and the damage needs to be
    // clipped to that instead.
    nsRect displayport;
    PRBool usingDisplayport = nsLayoutUtils::GetDisplayPort(GetContent(),
                                                            &displayport);
    if (usingDisplayport) {
      parentDamage.IntersectRect(damage, displayport);
    } else {
      parentDamage.IntersectRect(damage, mInner.mScrollPort);
    }

    if (IsScrollingActive()) {
      // This is the damage rect that we're going to pass up and
      // only request invalidation of ThebesLayers for.
      // damage is now in our coordinate system, which means it was
      // translated using the current scroll position. Adjust it to
      // reflect the scroll position at last paint, since that's what
      // the ThebesLayers are currently set up for.
      // This should not be clipped to the scrollport since ThebesLayers
      // can contain content outside the scrollport that may need to be
      // invalidated.
      nsRect thebesLayerDamage = damage + GetScrollPosition() - mInner.mScrollPosAtLastPaint;
      if (parentDamage.IsEqualInterior(thebesLayerDamage)) {
        // This single call will take care of both rects
        nsBoxFrame::InvalidateInternal(parentDamage, 0, 0, aForChild, aFlags);
      } else {
        // Invalidate rects separately
        if (!(aFlags & INVALIDATE_NO_THEBES_LAYERS)) {
          nsBoxFrame::InvalidateInternal(thebesLayerDamage, 0, 0, aForChild,
                                         aFlags | INVALIDATE_ONLY_THEBES_LAYERS);
        }
        if (!(aFlags & INVALIDATE_ONLY_THEBES_LAYERS) && !parentDamage.IsEmpty()) {
          nsBoxFrame::InvalidateInternal(parentDamage, 0, 0, aForChild,
                                         aFlags | INVALIDATE_NO_THEBES_LAYERS);
        }
      }
    } else {
      if (!parentDamage.IsEmpty()) {
        nsBoxFrame::InvalidateInternal(parentDamage, 0, 0, aForChild, aFlags);
      }
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
  PRBool widthSet, heightSet;
  nsIBox::AddCSSPrefSize(this, pref, widthSet, heightSet);
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
  PRBool widthSet, heightSet;
  nsIBox::AddCSSMinSize(aState, this, min, widthSet, heightSet);
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
  PRBool widthSet, heightSet;
  nsIBox::AddCSSMaxSize(this, maxSize, widthSet, heightSet);
  return maxSize;
}

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

#define SMOOTH_SCROLL_PREF_NAME "general.smoothScroll"

const double kCurrentVelocityWeighting = 0.25;
const double kStopDecelerationWeighting = 0.4;
const double kSmoothScrollAnimationDuration = 150; // milliseconds

class nsGfxScrollFrameInner::AsyncScroll {
public:
  typedef mozilla::TimeStamp TimeStamp;
  typedef mozilla::TimeDuration TimeDuration;

  AsyncScroll() {}
  ~AsyncScroll() {
    if (mScrollTimer) mScrollTimer->Cancel();
  }

  nsPoint PositionAt(TimeStamp aTime);
  nsSize VelocityAt(TimeStamp aTime); // In nscoords per second

  void InitSmoothScroll(TimeStamp aTime, nsPoint aCurrentPos,
                        nsSize aCurrentVelocity, nsPoint aDestination);

  PRBool IsFinished(TimeStamp aTime) {
    return aTime > mStartTime + mDuration; // XXX or if we've hit the wall
  }

  nsCOMPtr<nsITimer> mScrollTimer;
  TimeStamp mStartTime;
  TimeDuration mDuration;
  nsPoint mStartPos;
  nsPoint mDestination;
  nsSMILKeySpline mTimingFunctionX;
  nsSMILKeySpline mTimingFunctionY;
  PRPackedBool mIsSmoothScroll;

protected:
  double ProgressAt(TimeStamp aTime) {
    return NS_MIN(1.0, NS_MAX(0.0, (aTime - mStartTime) / mDuration));
  }

  nscoord VelocityComponent(double aTimeProgress,
                            nsSMILKeySpline& aTimingFunction,
                            nscoord aStart, nscoord aDestination);

  // Initializes the timing function in such a way that the current velocity is
  // preserved.
  void InitTimingFunction(nsSMILKeySpline& aTimingFunction,
                          nscoord aCurrentPos, nscoord aCurrentVelocity,
                          nscoord aDestination);
};

nsPoint
nsGfxScrollFrameInner::AsyncScroll::PositionAt(TimeStamp aTime) {
  double progressX = mTimingFunctionX.GetSplineValue(ProgressAt(aTime));
  double progressY = mTimingFunctionY.GetSplineValue(ProgressAt(aTime));
  return nsPoint((1 - progressX) * mStartPos.x + progressX * mDestination.x,
                 (1 - progressY) * mStartPos.y + progressY * mDestination.y);
}

nsSize
nsGfxScrollFrameInner::AsyncScroll::VelocityAt(TimeStamp aTime) {
  double timeProgress = ProgressAt(aTime);
  return nsSize(VelocityComponent(timeProgress, mTimingFunctionX,
                                  mStartPos.x, mDestination.x),
                VelocityComponent(timeProgress, mTimingFunctionY,
                                  mStartPos.y, mDestination.y));
}

void
nsGfxScrollFrameInner::AsyncScroll::InitSmoothScroll(TimeStamp aTime,
                                                     nsPoint aCurrentPos,
                                                     nsSize aCurrentVelocity,
                                                     nsPoint aDestination) {
  mStartTime = aTime;
  mStartPos = aCurrentPos;
  mDestination = aDestination;
  mDuration = TimeDuration::FromMilliseconds(kSmoothScrollAnimationDuration);
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
  return (slope * (aDestination - aStart) / (mDuration / oneSecond));
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

static PRBool
IsSmoothScrollingEnabled()
{
  return Preferences::GetBool(SMOOTH_SCROLL_PREF_NAME, PR_FALSE);
}

class ScrollFrameActivityTracker : public nsExpirationTracker<nsGfxScrollFrameInner,4> {
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

static ScrollFrameActivityTracker *gScrollFrameActivityTracker = nsnull;

nsGfxScrollFrameInner::nsGfxScrollFrameInner(nsContainerFrame* aOuter,
                                             PRBool aIsRoot)
  : mHScrollbarBox(nsnull)
  , mVScrollbarBox(nsnull)
  , mScrolledFrame(nsnull)
  , mScrollCornerBox(nsnull)
  , mResizerBox(nsnull)
  , mOuter(aOuter)
  , mAsyncScroll(nsnull)
  , mDestination(0, 0)
  , mScrollPosAtLastPaint(0, 0)
  , mRestorePos(-1, -1)
  , mLastPos(-1, -1)
  , mNeverHasVerticalScrollbar(PR_FALSE)
  , mNeverHasHorizontalScrollbar(PR_FALSE)
  , mHasVerticalScrollbar(PR_FALSE)
  , mHasHorizontalScrollbar(PR_FALSE)
  , mFrameIsUpdatingScrollbar(PR_FALSE)
  , mDidHistoryRestore(PR_FALSE)
  , mIsRoot(aIsRoot)
  , mSupppressScrollbarUpdate(PR_FALSE)
  , mSkippedScrollbarLayout(PR_FALSE)
  , mHadNonInitialReflow(PR_FALSE)
  , mHorizontalOverflow(PR_FALSE)
  , mVerticalOverflow(PR_FALSE)
  , mPostedReflowCallback(PR_FALSE)
  , mMayHaveDirtyFixedChildren(PR_FALSE)
  , mUpdateScrollbarAttributes(PR_FALSE)
  , mCollapsedResizer(PR_FALSE)
  , mShouldBuildLayer(PR_FALSE)
{
  // lookup if we're allowed to overlap the content from the look&feel object
  PRInt32 canOverlap;
  nsPresContext* presContext = mOuter->PresContext();
  presContext->LookAndFeel()->
    GetMetric(nsILookAndFeel::eMetric_ScrollbarsCanOverlapContent, canOverlap);
  mScrollbarsCanOverlapContent = canOverlap;
  mScrollingActive = IsAlwaysActive();
}

nsGfxScrollFrameInner::~nsGfxScrollFrameInner()
{
  if (mActivityExpirationState.IsTracked()) {
    gScrollFrameActivityTracker->RemoveObject(this);
  }
  if (gScrollFrameActivityTracker &&
      gScrollFrameActivityTracker->IsEmpty()) {
    delete gScrollFrameActivityTracker;
    gScrollFrameActivityTracker = nsnull;
  }
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
    TimeStamp now = TimeStamp::Now();
    nsPoint destination = self->mAsyncScroll->PositionAt(now);
    if (self->mAsyncScroll->IsFinished(now)) {
      delete self->mAsyncScroll;
      self->mAsyncScroll = nsnull;
    }

    self->ScrollToImpl(destination);
  } else {
    delete self->mAsyncScroll;
    self->mAsyncScroll = nsnull;

    self->ScrollToImpl(self->mDestination);
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

  TimeStamp now = TimeStamp::Now();
  nsPoint currentPosition = GetScrollPosition();
  nsSize currentVelocity(0, 0);
  PRBool isSmoothScroll = (aMode == nsIScrollableFrame::SMOOTH) &&
                          IsSmoothScrollingEnabled();

  if (mAsyncScroll) {
    if (mAsyncScroll->mIsSmoothScroll) {
      currentPosition = mAsyncScroll->PositionAt(now);
      currentVelocity = mAsyncScroll->VelocityAt(now);
    }
  } else {
    mAsyncScroll = new AsyncScroll;
    mAsyncScroll->mScrollTimer = do_CreateInstance("@mozilla.org/timer;1");
    if (!mAsyncScroll->mScrollTimer) {
      delete mAsyncScroll;
      mAsyncScroll = nsnull;
      // allocation failed. Scroll the normal way.
      ScrollToImpl(mDestination);
      return;
    }
    if (isSmoothScroll) {
      mAsyncScroll->mScrollTimer->InitWithFuncCallback(
        AsyncScrollCallback, this, 1000 / 60,
        nsITimer::TYPE_REPEATING_SLACK);
    } else {
      mAsyncScroll->mScrollTimer->InitWithFuncCallback(
        AsyncScrollCallback, this, 0, nsITimer::TYPE_ONE_SHOT);
    }
  }

  mAsyncScroll->mIsSmoothScroll = isSmoothScroll;

  if (isSmoothScroll) {
    mAsyncScroll->InitSmoothScroll(now, currentPosition, currentVelocity,
                                   aScrollPosition);
  }
}

// We can't use nsContainerFrame::PositionChildViews here because
// we don't want to invalidate views that have moved.
static void AdjustViews(nsIFrame* aFrame)
{
  nsIView* view = aFrame->GetView();
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

static PRBool
CanScrollWithBlitting(nsIFrame* aFrame)
{
  for (nsIFrame* f = aFrame; f;
       f = nsLayoutUtils::GetCrossDocParentFrame(f)) {
    if (nsSVGIntegrationUtils::UsingEffectsForFrame(f) ||
        f->IsFrameOfType(nsIFrame::eSVG)) {
      return PR_FALSE;
    }
    nsIScrollableFrame* sf = do_QueryFrame(f);
    if (sf && nsLayoutUtils::HasNonZeroCorner(f->GetStyleBorder()->mBorderRadius))
      return PR_FALSE;
    if (nsLayoutUtils::IsPopup(f))
      break;
  }
  return PR_TRUE;
}

static void
InvalidateFixedBackgroundFramesFromList(nsDisplayListBuilder* aBuilder,
                                        nsIFrame* aMovingFrame,
                                        const nsDisplayList& aList)
{
  for (nsDisplayItem* item = aList.GetBottom(); item; item = item->GetAbove()) {
    nsDisplayList* sublist = item->GetList();
    if (sublist) {
      InvalidateFixedBackgroundFramesFromList(aBuilder, aMovingFrame, *sublist);
      continue;
    }
    nsIFrame* f = item->GetUnderlyingFrame();
    if (f &&
        item->IsVaryingRelativeToMovingFrame(aBuilder, aMovingFrame)) {
      if (FrameLayerBuilder::NeedToInvalidateFixedDisplayItem(aBuilder, item)) {
        // FrameLayerBuilder does not take care of scrolling this one
        f->Invalidate(item->GetVisibleRect() - item->ToReferenceFrame());
      }
    }
  }
}

static void
InvalidateFixedBackgroundFrames(nsIFrame* aRootFrame,
                                nsIFrame* aMovingFrame,
                                const nsRect& aUpdateRect)
{
  if (!aMovingFrame->PresContext()->MayHaveFixedBackgroundFrames())
    return;

  NS_ASSERTION(aRootFrame != aMovingFrame,
               "The root frame shouldn't be the one that's moving, that makes no sense");

  // Build the 'after' display list over the whole area of interest.
  nsDisplayListBuilder builder(aRootFrame, nsDisplayListBuilder::OTHER, PR_TRUE);
  builder.EnterPresShell(aRootFrame, aUpdateRect);
  nsDisplayList list;
  nsresult rv =
    aRootFrame->BuildDisplayListForStackingContext(&builder, aUpdateRect, &list);
  builder.LeavePresShell(aRootFrame, aUpdateRect);
  if (NS_FAILED(rv))
    return;

  nsRegion visibleRegion(aUpdateRect);
  list.ComputeVisibilityForRoot(&builder, &visibleRegion);

  InvalidateFixedBackgroundFramesFromList(&builder, aMovingFrame, list);
  list.DeleteAll();
}

PRBool nsGfxScrollFrameInner::IsAlwaysActive() const
{
  // The root scrollframe for a non-chrome document which is the direct
  // child of a chrome document is always treated as "active".
  // XXX maybe we should extend this so that IFRAMEs which are fill the
  // entire viewport (like GMail!) are always active
  return mIsRoot && mOuter->PresContext()->IsRootContentDocument();
}

void nsGfxScrollFrameInner::MarkInactive()
{
  if (IsAlwaysActive() || !mScrollingActive)
    return;

  mScrollingActive = PR_FALSE;
  mOuter->InvalidateFrameSubtree();
}

void nsGfxScrollFrameInner::MarkActive()
{
  if (IsAlwaysActive())
    return;

  mScrollingActive = PR_TRUE;
  if (mActivityExpirationState.IsTracked()) {
    gScrollFrameActivityTracker->MarkUsed(this);
  } else {
    if (!gScrollFrameActivityTracker) {
      gScrollFrameActivityTracker = new ScrollFrameActivityTracker();
    }
    gScrollFrameActivityTracker->AddObject(this);
  }
}

void nsGfxScrollFrameInner::ScrollVisual()
{
  nsRootPresContext* rootPresContext = mOuter->PresContext()->GetRootPresContext();
  if (!rootPresContext) {
    return;
  }

  rootPresContext->RequestUpdatePluginGeometry(mOuter);

  AdjustViews(mScrolledFrame);
  // We need to call this after fixing up the view positions
  // to be consistent with the frame hierarchy.
  PRUint32 flags = nsIFrame::INVALIDATE_REASON_SCROLL_REPAINT;
  PRBool canScrollWithBlitting = CanScrollWithBlitting(mOuter);
  if (IsScrollingActive()) {
    if (!canScrollWithBlitting) {
      MarkInactive();
    } else {
      flags |= nsIFrame::INVALIDATE_NO_THEBES_LAYERS;
    }
  }
  if (canScrollWithBlitting) {
    MarkActive();
  }

  nsRect invalidateRect, displayport;
  invalidateRect =
    (nsLayoutUtils::GetDisplayPort(mOuter->GetContent(), &displayport)) ?
    displayport : mScrollPort;

  mOuter->InvalidateWithFlags(invalidateRect, flags);

  if (flags & nsIFrame::INVALIDATE_NO_THEBES_LAYERS) {
    nsIFrame* displayRoot = nsLayoutUtils::GetDisplayRootFrame(mOuter);
    nsRect update =
      GetScrollPortRect() + mOuter->GetOffsetToCrossDoc(displayRoot);
    update = update.ConvertAppUnitsRoundOut(
      mOuter->PresContext()->AppUnitsPerDevPixel(),
      displayRoot->PresContext()->AppUnitsPerDevPixel());
    InvalidateFixedBackgroundFrames(displayRoot, mScrolledFrame, update);
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
  for (PRUint32 i = 0; i < mListeners.Length(); i++) {
    mListeners[i]->ScrollPositionWillChange(pt.x, pt.y);
  }
  
  // Update frame position for scrolling
  mScrolledFrame->SetPosition(mScrollPort.TopLeft() - pt);

  // We pass in the amount to move visually
  ScrollVisual();

  presContext->PresShell()->SynthesizeMouseMove(PR_TRUE);
  UpdateScrollbarPosition();
  PostScrollEvent();

  // notify the listeners.
  for (PRUint32 i = 0; i < mListeners.Length(); i++) {
    mListeners[i]->ScrollPositionDidChange(pt.x, pt.y);
  }
}

static void
AppendToTop(nsDisplayListBuilder* aBuilder, nsDisplayList* aDest,
            nsDisplayList* aSource, nsIFrame* aSourceFrame, PRBool aOwnLayer)
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

nsresult
nsGfxScrollFrameInner::AppendScrollPartsTo(nsDisplayListBuilder*          aBuilder,
                                           const nsRect&                  aDirtyRect,
                                           const nsDisplayListSet&        aLists,
                                           const nsDisplayListCollection& aDest,
                                           PRBool&                        aCreateLayer)
{
  nsresult rv = NS_OK;
  PRBool hasResizer = HasResizer();
  for (nsIFrame* kid = mOuter->GetFirstChild(nsnull); kid; kid = kid->GetNextSibling()) {
    if (kid != mScrolledFrame) {
      if (kid == mResizerBox && hasResizer) {
        // skip the resizer as this will be drawn later on top of the scrolled content
        continue;
      }
      rv = mOuter->BuildDisplayListForChild(aBuilder, kid, aDirtyRect, aDest,
                                            nsIFrame::DISPLAY_CHILD_FORCE_STACKING_CONTEXT);
      NS_ENSURE_SUCCESS(rv, rv);
      // DISPLAY_CHILD_FORCE_STACKING_CONTEXT put everything into the
      // PositionedDescendants list.
      ::AppendToTop(aBuilder, aLists.BorderBackground(),
                    aDest.PositionedDescendants(), kid,
                    aCreateLayer);
    }
  }
  return rv;
}

PRBool
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
  void SetCount(PRWord aCount) {
    mProps.Set(nsIFrame::ScrollLayerCount(), reinterpret_cast<void*>(aCount));
  }

  PRWord mCount;
  FrameProperties mProps;
  nsIFrame* mScrollFrame;
  nsIFrame* mScrolledFrame;
};

nsresult
nsGfxScrollFrameInner::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                        const nsRect&           aDirtyRect,
                                        const nsDisplayListSet& aLists)
{
  nsresult rv = mOuter->DisplayBorderBackgroundOutline(aBuilder, aLists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aBuilder->IsPaintingToWindow()) {
    mScrollPosAtLastPaint = GetScrollPosition();
    if (IsScrollingActive() && !CanScrollWithBlitting(mOuter)) {
      MarkInactive();
    }
  }

  if (aBuilder->GetIgnoreScrollFrame() == mOuter) {
    // Don't clip the scrolled child, and don't paint scrollbars/scrollcorner.
    // The scrolled frame shouldn't have its own background/border, so we
    // can just pass aLists directly.
    return mOuter->BuildDisplayListForChild(aBuilder, mScrolledFrame,
                                            aDirtyRect, aLists);
  }

  // We put scrollbars in their own layers when this is the root scroll
  // frame and we are a toplevel content document. In this situation, the
  // scrollbar(s) would normally be assigned their own layer anyway, since
  // they're not scrolled with the rest of the document. But when both
  // scrollbars are visible, the layer's visible rectangle would be the size
  // of the viewport, so most layer implementations would create a layer buffer
  // that's much larger than necessary. Creating independent layers for each
  // scrollbar works around the problem.
  PRBool createLayersForScrollbars = mIsRoot &&
    mOuter->PresContext()->IsRootContentDocument();

  nsDisplayListCollection scrollParts;
  if (!mScrollbarsCanOverlapContent) {
    // Now display the scrollbars and scrollcorner. These parts are drawn
    // in the border-background layer, on top of our own background and
    // borders and underneath borders and backgrounds of later elements
    // in the tree.
    AppendScrollPartsTo(aBuilder, aDirtyRect, aLists,
                        scrollParts, createLayersForScrollbars);
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

  // Override the dirty rectangle if the displayport has been set.
  PRBool usingDisplayport =
    nsLayoutUtils::GetDisplayPort(mOuter->GetContent(), &dirtyRect);

  nsDisplayListCollection set;
  rv = mOuter->BuildDisplayListForChild(aBuilder, mScrolledFrame, dirtyRect, set);
  NS_ENSURE_SUCCESS(rv, rv);

  // Since making new layers is expensive, only use nsDisplayScrollLayer
  // if the area is scrollable.
  nsRect scrollRange = GetScrollRange();
  ScrollbarStyles styles = GetScrollbarStylesFromFrame();
  mShouldBuildLayer =
     (XRE_GetProcessType() == GeckoProcessType_Content &&
     (styles.mHorizontal != NS_STYLE_OVERFLOW_HIDDEN ||
      styles.mVertical != NS_STYLE_OVERFLOW_HIDDEN) &&
     (scrollRange.width > 0 ||
      scrollRange.height > 0) &&
     (!mIsRoot || !mOuter->PresContext()->IsRootContentDocument()));

  if (ShouldBuildLayer()) {
    // ScrollLayerWrapper must always be created because it initializes the
    // scroll layer count. The display lists depend on this.
    ScrollLayerWrapper wrapper(mOuter, mScrolledFrame);

    if (usingDisplayport) {
      // Once a displayport is set, assume that scrolling needs to be fast
      // so create a layer with all the content inside. The compositor
      // process will be able to scroll the content asynchronously.
      wrapper.WrapListsInPlace(aBuilder, mOuter, set);
    }

    // In case we are not using displayport or the nsDisplayScrollLayers are
    // flattened during visibility computation, we still need to export the
    // metadata about this scroll box to the compositor process.
    nsDisplayScrollInfoLayer* layerItem = new (aBuilder) nsDisplayScrollInfoLayer(
      aBuilder, mScrolledFrame, mOuter);
    set.BorderBackground()->AppendNewToBottom(layerItem);
  }

  nsRect clip;
  clip = mScrollPort + aBuilder->ToReferenceFrame(mOuter);

  nscoord radii[8];
  // Our override of GetBorderRadii ensures we never have a radius at
  // the corners where we have a scrollbar.
  mOuter->GetPaddingBoxBorderRadii(radii);

  // mScrolledFrame may have given us a background, e.g., the scrolled canvas
  // frame below the viewport. If so, we want it to be clipped. We also want
  // to end up on our BorderBackground list.
  // If we are the viewport scrollframe, then clip all our descendants (to ensure
  // that fixed-pos elements get clipped by us).
  rv = mOuter->OverflowClip(aBuilder, set, aLists, clip, radii,
                            PR_TRUE, mIsRoot);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mScrollbarsCanOverlapContent) {
    AppendScrollPartsTo(aBuilder, aDirtyRect, aLists,
                       scrollParts, createLayersForScrollbars);
  }

  if (HasResizer()) {
    rv = mOuter->BuildDisplayListForChild(aBuilder, mResizerBox, aDirtyRect, scrollParts,
                                          nsIFrame::DISPLAY_CHILD_FORCE_STACKING_CONTEXT);
    NS_ENSURE_SUCCESS(rv, rv);
    // DISPLAY_CHILD_FORCE_STACKING_CONTEXT puts everything into the
    // PositionedDescendants list.
    // The resizer is positioned and has maximum z-index; we put it in
    // PositionedDescendants() for the root frame to ensure that it appears
    // above all content, bug 631337.
    ::AppendToTop(aBuilder,
                  mIsRoot ? aLists.PositionedDescendants() : aLists.Content(),
                  scrollParts.PositionedDescendants(), mResizerBox,
                  createLayersForScrollbars);
  }

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
      // XXX EVIL COMPILER BUG BE CAREFUL WHEN CHANGING
      //     There is a bug in the Android compiler :(
      //     It seems that the compiler optimizes something out and uses
      //     a bad value for result if we don't directly return here.
      return result;
    }
  } else {
    const nsStyleDisplay *disp = mOuter->GetStyleDisplay();
    result.mHorizontal = disp->mOverflowX;
    result.mVertical = disp->mOverflowY;
  }

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
        NSAppUnitsToIntPixels(NS_ABS(clampAmount.x), appUnitsPerDevPixel),
        NSAppUnitsToIntPixels(NS_ABS(clampAmount.y), appUnitsPerDevPixel));
  }
}

nsSize
nsGfxScrollFrameInner::GetLineScrollAmount() const
{
  nsRefPtr<nsFontMetrics> fm;
  nsLayoutUtils::GetFontMetricsForFrame(mOuter, getter_AddRefs(fm));
  NS_ASSERTION(fm, "FontMetrics is null, assuming fontHeight == 1 appunit");
  nscoord fontHeight = 1;
  if (fm) {
    fontHeight = fm->MaxHeight();
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
  mResizerBox = nsnull;

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
      } else if (content->Tag() == nsGkAtoms::resizer) {
        NS_ASSERTION(!mResizerBox, "Found multiple resizers");
        mResizerBox = frame;
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
    mNeverHasVerticalScrollbar = mNeverHasHorizontalScrollbar = PR_TRUE;
    return NS_OK;
  }

  // Check if the frame is resizable.
  PRInt8 resizeStyle = mOuter->GetStyleDisplay()->mResize;
  PRBool isResizable = resizeStyle != NS_STYLE_RESIZE_NONE;

  nsIScrollableFrame *scrollable = do_QueryFrame(mOuter);

  // If we're the scrollframe for the root, then we want to construct
  // our scrollbar frames no matter what.  That way later dynamic
  // changes to propagated overflow styles will show or hide
  // scrollbars on the viewport without requiring frame reconstruction
  // of the viewport (good!).
  PRBool canHaveHorizontal;
  PRBool canHaveVertical;
  if (!mIsRoot) {
    ScrollbarStyles styles = scrollable->GetScrollbarStyles();
    canHaveHorizontal = styles.mHorizontal != NS_STYLE_OVERFLOW_HIDDEN;
    canHaveVertical = styles.mVertical != NS_STYLE_OVERFLOW_HIDDEN;
    if (!canHaveHorizontal && !canHaveVertical && !isResizable) {
      // Nothing to do.
      return NS_OK;
    }
  } else {
    canHaveHorizontal = PR_TRUE;
    canHaveVertical = PR_TRUE;
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

  nsNodeInfoManager *nodeInfoManager =
    presContext->Document()->NodeInfoManager();
  nsCOMPtr<nsINodeInfo> nodeInfo;
  nodeInfo = nodeInfoManager->GetNodeInfo(nsGkAtoms::scrollbar, nsnull,
                                          kNameSpaceID_XUL,
                                          nsIDOMNode::ELEMENT_NODE);
  NS_ENSURE_TRUE(nodeInfo, NS_ERROR_OUT_OF_MEMORY);

  if (canHaveHorizontal) {
    nsCOMPtr<nsINodeInfo> ni = nodeInfo;
    NS_TrustedNewXULElement(getter_AddRefs(mHScrollbarContent), ni.forget());
    mHScrollbarContent->SetAttr(kNameSpaceID_None, nsGkAtoms::orient,
                                NS_LITERAL_STRING("horizontal"), PR_FALSE);
    mHScrollbarContent->SetAttr(kNameSpaceID_None, nsGkAtoms::clickthrough,
                                NS_LITERAL_STRING("always"), PR_FALSE);
    if (!aElements.AppendElement(mHScrollbarContent))
      return NS_ERROR_OUT_OF_MEMORY;
  }

  if (canHaveVertical) {
    nsCOMPtr<nsINodeInfo> ni = nodeInfo;
    NS_TrustedNewXULElement(getter_AddRefs(mVScrollbarContent), ni.forget());
    mVScrollbarContent->SetAttr(kNameSpaceID_None, nsGkAtoms::orient,
                                NS_LITERAL_STRING("vertical"), PR_FALSE);
    mVScrollbarContent->SetAttr(kNameSpaceID_None, nsGkAtoms::clickthrough,
                                NS_LITERAL_STRING("always"), PR_FALSE);
    if (!aElements.AppendElement(mVScrollbarContent))
      return NS_ERROR_OUT_OF_MEMORY;
  }

  if (isResizable) {
    nsCOMPtr<nsINodeInfo> nodeInfo;
    nodeInfo = nodeInfoManager->GetNodeInfo(nsGkAtoms::resizer, nsnull,
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
    mResizerContent->SetAttr(kNameSpaceID_None, nsGkAtoms::dir, dir, PR_FALSE);

    if (mIsRoot) {
      nsIContent* browserRoot = GetBrowserRoot(mOuter->GetContent());
      mCollapsedResizer = !(browserRoot &&
                            browserRoot->HasAttr(kNameSpaceID_None, nsGkAtoms::showresizer));
    }
    else {
      mResizerContent->SetAttr(kNameSpaceID_None, nsGkAtoms::element,
                                    NS_LITERAL_STRING("_parent"), PR_FALSE);
    }

    mResizerContent->SetAttr(kNameSpaceID_None, nsGkAtoms::clickthrough,
                                  NS_LITERAL_STRING("always"), PR_FALSE);

    if (!aElements.AppendElement(mResizerContent))
      return NS_ERROR_OUT_OF_MEMORY;
  }

  if (canHaveHorizontal && canHaveVertical) {
    nodeInfo = nodeInfoManager->GetNodeInfo(nsGkAtoms::scrollcorner, nsnull,
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
                                                PRUint32 aFilter)
{
  aElements.MaybeAppendElement(mHScrollbarContent);
  aElements.MaybeAppendElement(mVScrollbarContent);
  aElements.MaybeAppendElement(mScrollCornerContent);
  aElements.MaybeAppendElement(mResizerContent);
}

void
nsGfxScrollFrameInner::Destroy()
{
  // Unbind any content created in CreateAnonymousContent from the tree
  nsContentUtils::DestroyAnonymousContent(&mHScrollbarContent);
  nsContentUtils::DestroyAnonymousContent(&mVScrollbarContent);
  nsContentUtils::DestroyAnonymousContent(&mScrollCornerContent);
  nsContentUtils::DestroyAnonymousContent(&mResizerContent);

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

  nsPoint current = GetScrollPosition() - scrolledRect.TopLeft();
  nsPoint dest;
  dest.x = GetCoordAttribute(mHScrollbarBox, nsGkAtoms::curpos, current.x) +
           scrolledRect.x;
  dest.y = GetCoordAttribute(mVScrollbarBox, nsGkAtoms::curpos, current.y) +
           scrolledRect.y;

  // If we have an async scroll pending don't stomp on that by calling ScrollTo.
  if (mAsyncScroll && dest == GetScrollPosition()) {
    return;
  }

  PRBool isSmooth = aContent->HasAttr(kNameSpaceID_None, nsGkAtoms::smooth);
  if (isSmooth) {
    // Make sure an attribute-setting callback occurs even if the view
    // didn't actually move yet.  We need to make sure other listeners
    // see that the scroll position is not (yet) what they thought it
    // was.
    UpdateScrollbarPosition();
  }
  ScrollTo(dest,
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
nsXULScrollFrame::AddHorizontalScrollbar(nsBoxLayoutState& aState, PRBool aOnBottom)
{
  if (!mInner.mHScrollbarBox)
    return PR_TRUE;

  return AddRemoveScrollbar(aState, aOnBottom, PR_TRUE, PR_TRUE);
}

PRBool
nsXULScrollFrame::AddVerticalScrollbar(nsBoxLayoutState& aState, PRBool aOnRight)
{
  if (!mInner.mVScrollbarBox)
    return PR_TRUE;

  return AddRemoveScrollbar(aState, aOnRight, PR_FALSE, PR_TRUE);
}

void
nsXULScrollFrame::RemoveHorizontalScrollbar(nsBoxLayoutState& aState, PRBool aOnBottom)
{
  // removing a scrollbar should always fit
#ifdef DEBUG
  PRBool result =
#endif
  AddRemoveScrollbar(aState, aOnBottom, PR_TRUE, PR_FALSE);
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
                                     PRBool aOnRightOrBottom, PRBool aHorizontal, PRBool aAdd)
{
  if (aHorizontal) {
     if (mInner.mNeverHasHorizontalScrollbar || !mInner.mHScrollbarBox)
       return PR_FALSE;

     nsSize hSize = mInner.mHScrollbarBox->GetPrefSize(aState);
     nsBox::AddMargin(mInner.mHScrollbarBox, hSize);

     mInner.SetScrollbarVisibility(mInner.mHScrollbarBox, aAdd);

     PRBool hasHorizontalScrollbar;
     PRBool fit = AddRemoveScrollbar(hasHorizontalScrollbar,
                                     mInner.mScrollPort.y,
                                     mInner.mScrollPort.height,
                                     hSize.height, aOnRightOrBottom, aAdd);
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
     PRBool fit = AddRemoveScrollbar(hasVerticalScrollbar,
                                     mInner.mScrollPort.x,
                                     mInner.mScrollPort.width,
                                     vSize.width, aOnRightOrBottom, aAdd);
     mInner.mHasVerticalScrollbar = hasVerticalScrollbar;    // because mHasVerticalScrollbar is a PRPackedBool
     if (!fit)
        mInner.SetScrollbarVisibility(mInner.mVScrollbarBox, !aAdd);

     return fit;
  }
}

PRBool
nsXULScrollFrame::AddRemoveScrollbar(PRBool& aHasScrollbar, nscoord& aXY,
                                     nscoord& aSize, nscoord aSbSize,
                                     PRBool aOnRightOrBottom, PRBool aAdd)
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
  nsRect originalVisOverflow = mInner.mScrolledFrame->GetVisualOverflowRect();

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
    childRect.width = NS_MAX(childRect.width, mInner.mScrollPort.width);
    childRect.height = NS_MAX(childRect.height, mInner.mScrollPort.height);

    // remove overflow areas when we update the bounds,
    // because we've already accounted for it
    // REVIEW: Have we accounted for both?
    ClampAndSetBounds(aState, childRect, aScrollPosition, PR_TRUE);
  }

  nsRect finalRect = mInner.mScrolledFrame->GetRect();
  nsRect finalVisOverflow = mInner.mScrolledFrame->GetVisualOverflowRect();
  // The position of the scrolled frame shouldn't change, but if it does or
  // the position of the overflow rect changes just invalidate both the old
  // and new overflow rect.
  if (originalRect.TopLeft() != finalRect.TopLeft() ||
      originalVisOverflow.TopLeft() != finalVisOverflow.TopLeft())
  {
    // The old overflow rect needs to be adjusted if the frame's position
    // changed.
    mInner.mScrolledFrame->Invalidate(
      originalVisOverflow + originalRect.TopLeft() - finalRect.TopLeft());
    mInner.mScrolledFrame->Invalidate(finalVisOverflow);
  } else if (!originalVisOverflow.IsEqualInterior(finalVisOverflow)) {
    // If the overflow rect changed then invalidate the difference between the
    // old and new overflow rects.
    mInner.mScrolledFrame->CheckInvalidateSizeChange(
      originalRect, originalVisOverflow, finalRect.Size());
    mInner.mScrolledFrame->InvalidateRectDifference(
      originalVisOverflow, finalVisOverflow);
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

  return frame->GetStyleVisibility()->mDirection != NS_STYLE_DIRECTION_RTL;
}

PRBool
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

    // There are two cases to consider
      if (scrolledRect.height <= mInner.mScrollPort.height
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

    // if the child is wider that the scroll area
    // and we don't have a scrollbar add one.
    if ((scrolledRect.width > mInner.mScrollPort.width)
        && styles.mHorizontal == NS_STYLE_OVERFLOW_AUTO) {

      if (!mInner.mHasHorizontalScrollbar) {
           // no scrollbar? 
          if (AddHorizontalScrollbar(aState, scrollbarBottom))
             needsLayout = PR_TRUE;

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

  // Clamp scroll position
  ScrollToImpl(GetScrollPosition());

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
                                nsIFrame* aBox, const nsRect& aRect,
                                PRBool aScrollbarIsBeingHidden)
{
  // When a child box changes shape of position, the parent
  // is responsible for invalidation; the overflow rect must be invalidated
  // to make sure to catch any overflow.
  // We invalidate the parent (i.e. the scrollframe) directly, because
  // invalidates coming from scrollbars are suppressed by nsHTMLScrollFrame when
  // mHasVScrollbar/mHasHScrollbar is false, and this is called after those
  // flags have been set ... if a scrollbar is being hidden, we still need
  // to invalidate the scrollbar area here.
  // But we also need to invalidate the scrollbar itself in case it has
  // its own layer; we need to ensure that layer is updated.
  PRBool rectChanged = !aBox->GetRect().IsEqualInterior(aRect);
  if (rectChanged) {
    if (aScrollbarIsBeingHidden) {
      aBox->GetParent()->Invalidate(aBox->GetVisualOverflowRect() +
                                    aBox->GetPosition());
    } else {
      aBox->InvalidateFrameSubtree();
    }
  }
  nsBoxFrame::LayoutChildAt(aState, aBox, aRect);
  if (rectChanged) {
    if (aScrollbarIsBeingHidden) {
      aBox->GetParent()->Invalidate(aBox->GetVisualOverflowRect() +
                                    aBox->GetPosition());
    } else {
      aBox->InvalidateFrameSubtree();
    }
  }
}

void
nsGfxScrollFrameInner::AdjustScrollbarRectForResizer(
                         nsIFrame* aFrame, nsPresContext* aPresContext,
                         nsRect& aRect, PRBool aHasResizer, PRBool aVertical)
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

  PRBool hasResizer = HasResizer();
  PRBool scrollbarOnLeft = !IsScrollbarOnRight();

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
      LayoutAndInvalidate(aState, mScrollCornerBox, r, PR_FALSE);
    }

    if (hasResizer) {
      // if a resizer is present, get its size. Assume a default size of 15 pixels.
      nsSize resizerSize;
      nscoord defaultSize = nsPresContext::CSSPixelsToAppUnits(15);
      resizerSize.width =
        mVScrollbarBox ? mVScrollbarBox->GetMinSize(aState).width : defaultSize;
      if (resizerSize.width > r.width) {
        r.width = resizerSize.width;
        if (aContentArea.x == mScrollPort.x && !scrollbarOnLeft)
          r.x = aContentArea.XMost() - r.width;
      }

      resizerSize.height =
        mHScrollbarBox ? mHScrollbarBox->GetMinSize(aState).height : defaultSize;
      if (resizerSize.height > r.height) {
        r.height = resizerSize.height;
        if (aContentArea.y == mScrollPort.y)
          r.y = aContentArea.YMost() - r.height;
      }

      LayoutAndInvalidate(aState, mResizerBox, r, PR_FALSE);
    }
    else if (mResizerBox) {
      // otherwise lay out the resizer with an empty rectangle
      LayoutAndInvalidate(aState, mResizerBox, nsRect(), PR_FALSE);
    }
  }

  nsPresContext* presContext = mScrolledFrame->PresContext();
  if (mVScrollbarBox) {
    NS_PRECONDITION(mVScrollbarBox->IsBoxFrame(), "Must be a box frame!");
    nsRect vRect(mScrollPort);
    vRect.width = aContentArea.width - mScrollPort.width;
    vRect.x = scrollbarOnLeft ? aContentArea.x : mScrollPort.XMost();
    nsMargin margin;
    mVScrollbarBox->GetMargin(margin);
    vRect.Deflate(margin);
    AdjustScrollbarRectForResizer(mOuter, presContext, vRect, hasResizer, PR_TRUE);
    LayoutAndInvalidate(aState, mVScrollbarBox, vRect, !mHasVerticalScrollbar);
  }

  if (mHScrollbarBox) {
    NS_PRECONDITION(mHScrollbarBox->IsBoxFrame(), "Must be a box frame!");
    nsRect hRect(mScrollPort);
    hRect.height = aContentArea.height - mScrollPort.height;
    hRect.y = PR_TRUE ? mScrollPort.YMost() : aContentArea.y;
    nsMargin margin;
    mHScrollbarBox->GetMargin(margin);
    hRect.Deflate(margin);
    AdjustScrollbarRectForResizer(mOuter, presContext, hRect, hasResizer, PR_FALSE);
    LayoutAndInvalidate(aState, mHScrollbarBox, hRect, !mHasHorizontalScrollbar);
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

static void
ReduceRadii(nscoord aXBorder, nscoord aYBorder,
            nscoord& aXRadius, nscoord& aYRadius)
{
  // In order to ensure that the inside edge of the border has no
  // curvature, we need at least one of its radii to be zero.
  if (aXRadius <= aXBorder || aYRadius <= aYBorder)
    return;

  // For any corner where we reduce the radii, preserve the corner's shape.
  double ratio = NS_MAX(double(aXBorder) / aXRadius,
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
PRBool
nsGfxScrollFrameInner::GetBorderRadii(nscoord aRadii[8]) const
{
  if (!mOuter->nsContainerFrame::GetBorderRadii(aRadii))
    return PR_FALSE;

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

  return PR_TRUE;
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
  nscoord x1 = aScrolledFrameOverflowArea.x,
          x2 = aScrolledFrameOverflowArea.XMost(),
          y1 = aScrolledFrameOverflowArea.y,
          y2 = aScrolledFrameOverflowArea.YMost();
  if (y1 < 0)
    y1 = 0;
  if (IsLTR()) {
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
  nsRect r = mOuter->GetPaddingRect() - mOuter->GetPosition();

  return nsMargin(mScrollPort.x - r.x, mScrollPort.y - r.y,
                  r.XMost() - mScrollPort.XMost(),
                  r.YMost() - mScrollPort.YMost());
}

void
nsGfxScrollFrameInner::SetScrollbarVisibility(nsIBox* aScrollbar, PRBool aVisible)
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

  nsPoint scrollPos = GetLogicalScrollPosition();
  // Don't save scroll position if we are at (0,0)
  if (scrollPos == nsPoint(0,0)) {
    return nsnull;
  }

  nsPresState* state = new nsPresState();

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

  nsScrollAreaEvent event(PR_TRUE, NS_SCROLLEDAREACHANGED, nsnull);
  nsPresContext *prescontext = mOuter->PresContext();
  nsIContent* content = mOuter->GetContent();

  event.mArea = mScrolledFrame->GetScrollableOverflowRectRelativeToParent();

  nsIDocument *doc = content->GetCurrentDoc();
  if (doc) {
    nsEventDispatcher::Dispatch(doc, prescontext, &event, nsnull);
  }
}
