/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#include "nsBlockReflowContext.h"
#include "nsLineLayout.h"
#include "nsHTMLIIDs.h"
#include "nsISpaceManager.h"
#include "nsIFontMetrics.h"
#include "nsIPresContext.h"
#include "nsIContent.h"
#include "nsIStyleContext.h"
#include "nsHTMLContainerFrame.h"
#include "nsBlockFrame.h"
#include "nsIDOMHTMLParagraphElement.h"

#ifdef NS_DEBUG
#undef  NOISY_MAX_ELEMENT_SIZE
#undef   REALLY_NOISY_MAX_ELEMENT_SIZE
#undef  NOISY_VERTICAL_MARGINS
#else
#undef  NOISY_MAX_ELEMENT_SIZE
#undef   REALLY_NOISY_MAX_ELEMENT_SIZE
#undef  NOISY_VERTICAL_MARGINS
#endif

nsBlockReflowContext::nsBlockReflowContext(nsIPresContext* aPresContext,
                                           const nsHTMLReflowState& aParentRS,
                                           PRBool aComputeMaxElementSize)
  : mPresContext(aPresContext),
    mOuterReflowState(aParentRS),
    mMetrics(aComputeMaxElementSize ? &mMaxElementSize : nsnull),
    mMaxElementSize(0, 0)
{
  mStyleSpacing = nsnull;
}

PRBool
nsBlockReflowContext::IsHTMLParagraph(nsIFrame* aFrame)
{
  PRBool result = PR_FALSE;
  nsIContent* content;
  nsresult rv = aFrame->GetContent(&content);
  if (NS_SUCCEEDED(rv) && (nsnull != content)) {
    static NS_DEFINE_IID(kIDOMHTMLParagraphElementIID, NS_IDOMHTMLPARAGRAPHELEMENT_IID);
    nsIDOMHTMLParagraphElement* p;
    nsresult rv = content->QueryInterface(kIDOMHTMLParagraphElementIID,
                                          (void**) &p);
    if (NS_SUCCEEDED(rv) && p) {
      result = PR_TRUE;
      NS_RELEASE(p);
    }
    NS_RELEASE(content);
  }
  return result;
}

nscoord
nsBlockReflowContext::ComputeCollapsedTopMargin(nsIPresContext* aPresContext,
                                                nsHTMLReflowState& aRS)
{
  // Get aFrame's top margin
  nscoord topMargin = aRS.computedMargin.top;

  // Calculate aFrame's generational top-margin from its child
  // blocks. Note that if aFrame has a non-zero top-border or
  // top-padding then this step is skipped because it will be a margin
  // root.
  nscoord generationalTopMargin = 0;
  if (0 == aRS.mComputedBorderPadding.top) {
    nsBlockFrame* bf;
    if (NS_SUCCEEDED(aRS.frame->QueryInterface(kBlockFrameCID, (void**)&bf))) {
      // Ask the block frame for the top block child that we should
      // try to collapse the top margin with.
      nsIFrame* childFrame = bf->GetTopBlockChild();
      if (nsnull != childFrame) {

        // Here is where we recurse. Now that we have determined that a
        // generational collapse is required we need to compute the
        // child blocks margin and so in so that we can look into
        // it. For its margins to be computed we need to have a reflow
        // state for it.
        nsSize availSpace(aRS.computedWidth, aRS.computedHeight);
        nsHTMLReflowState reflowState(*aPresContext, aRS, childFrame,
                                      availSpace);
        generationalTopMargin =
          ComputeCollapsedTopMargin(aPresContext, reflowState);
      }
    }
  }

  // Now compute the collapsed top-margin value. At this point we have
  // the child frames effective top margin value.
  nscoord collapsedTopMargin = MaxMargin(topMargin, generationalTopMargin);

#ifdef NOISY_VERTICAL_MARGINS
  nsFrame::ListTag(stdout, aRS.frame);
  printf(": topMargin=%d generationalTopMargin=%d => %d\n",
         topMargin, generationalTopMargin, collapsedTopMargin);
#endif

  return collapsedTopMargin;
}

nsresult
nsBlockReflowContext::ReflowBlock(nsIFrame* aFrame,
                                  const nsRect& aSpace,
                                  PRBool aApplyTopMargin,
                                  nscoord aPrevBottomMargin,
                                  PRBool aIsAdjacentWithTop,
                                  nsMargin& aComputedOffsets,
                                  nsReflowStatus& aFrameReflowStatus)
{
  nsresult rv = NS_OK;
  mFrame = aFrame;
  mSpace = aSpace;

  // Get reflow reason set correctly. It's possible that a child was
  // created and then it was decided that it could not be reflowed
  // (for example, a block frame that isn't at the start of a
  // line). In this case the reason will be wrong so we need to check
  // the frame state.
  nsReflowReason reason = eReflowReason_Resize;
  nsFrameState state;
  aFrame->GetFrameState(&state);
  if (NS_FRAME_FIRST_REFLOW & state) {
    reason = eReflowReason_Initial;
  }
  else if (mNextRCFrame == aFrame) {
    reason = eReflowReason_Incremental;
    // Make sure we only incrementally reflow once
    mNextRCFrame = nsnull;
  }

  // Setup reflow state for reflowing the frame
  // XXX subtract out vertical margin?
  nsSize availSpace(aSpace.width, aSpace.height);
  nsHTMLReflowState reflowState(*mPresContext, mOuterReflowState, aFrame,
                                availSpace, reason);
  aComputedOffsets = reflowState.computedOffsets;
  reflowState.lineLayout = nsnull;
  if (!aIsAdjacentWithTop) {
    reflowState.isTopOfPage = PR_FALSE;  // make sure this is cleared
  }
  mIsTable = NS_STYLE_DISPLAY_TABLE == reflowState.mStyleDisplay->mDisplay;

  nscoord topMargin = 0;
  if (aApplyTopMargin) {
    // Compute the childs collapsed top margin (its margin collpased
    // with its first childs top-margin -- recursively).
#ifdef NOISY_VERTICAL_MARGINS
    nsFrame::ListTag(stdout, mOuterReflowState.frame);
    printf(": prevBottomMargin=%d\n", aPrevBottomMargin);
#endif
    topMargin = ComputeCollapsedTopMargin(mPresContext, reflowState);

    // Collapse that value with the previous bottom margin to perform
    // the sibling to sibling collaspe.
    topMargin = MaxMargin(topMargin, aPrevBottomMargin);

    // Adjust the available height if its constrained so that the
    // child frame doesn't think it can reflow into its margin area.
    // XXX write me
#if 0
    availSpace.y += topMargin;
    if (NS_UNCONSTRAINEDSIZE != availHeight) {
      availSpace.height -= topMargin;
    }
#endif
  }
  mTopMargin = topMargin;

  // Compute x/y coordinate where reflow will begin. Use the rules
  // from 10.3.3 to determine what to apply. At this point in the
  // reflow auto left/right margins will have a zero value.
  mMargin = reflowState.computedMargin;
  mStyleSpacing = reflowState.mStyleSpacing;
  nscoord x = aSpace.x + mMargin.left;
  nscoord y = aSpace.y + topMargin;
  mX = x;
  mY = y;

  // Let frame know that we are reflowing it
  nsIHTMLReflow* htmlReflow;
  rv = aFrame->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow);
  if (NS_FAILED(rv)) {
    return rv;
  }
  htmlReflow->WillReflow(*mPresContext);

#ifdef DEBUG
  mMetrics.width = nscoord(0xdeadbeef);
  mMetrics.height = nscoord(0xdeadbeef);
  mMetrics.ascent = nscoord(0xdeadbeef);
  mMetrics.descent = nscoord(0xdeadbeef);
  if (nsnull != mMetrics.maxElementSize) {
    mMetrics.maxElementSize->width = nscoord(0xdeadbeef);
    mMetrics.maxElementSize->height = nscoord(0xdeadbeef);
  }
#endif

  // Adjust spacemanager coordinate system for the frame. The
  // spacemanager coordinates are <b>inside</b> the callers
  // border+padding, but the x/y coordinates are not (recall that
  // frame coordinates are relative to the parents origin and that the
  // parents border/padding is <b>inside</b> the parent
  // frame. Therefore we have to subtract out the parents
  // border+padding before translating.
  nscoord tx = x - mOuterReflowState.mComputedBorderPadding.left;
  nscoord ty = y - mOuterReflowState.mComputedBorderPadding.top;
  mOuterReflowState.spaceManager->Translate(tx, ty);
  rv = htmlReflow->Reflow(*mPresContext, mMetrics, reflowState,
                          aFrameReflowStatus);
  mOuterReflowState.spaceManager->Translate(-tx, -ty);

#ifdef DEBUG
  if (!NS_INLINE_IS_BREAK_BEFORE(aFrameReflowStatus)) {
    if (CRAZY_WIDTH(mMetrics.width) || CRAZY_HEIGHT(mMetrics.height)) {
      printf("nsBlockReflowContext: ");
      nsFrame::ListTag(stdout, aFrame);
      printf(" metrics=%d,%d!\n", mMetrics.width, mMetrics.height);
    }
    if ((nsnull != mMetrics.maxElementSize) &&
        ((nscoord(0xdeadbeef) == mMetrics.maxElementSize->width) ||
         (nscoord(0xdeadbeef) == mMetrics.maxElementSize->height))) {
      printf("nsBlockReflowContext: ");
      nsFrame::ListTag(stdout, aFrame);
      printf(" didn't set max-element-size!\n");
      mMetrics.maxElementSize->width = 0;
      mMetrics.maxElementSize->height = 0;
    }
#ifdef REALLY_NOISY_MAX_ELEMENT_SIZE
    // Note: there are common reflow situations where this *correctly*
    // occurs; so only enable this debug noise when you really need to
    // analyze in detail.
    if ((nsnull != mMetrics.maxElementSize) &&
        ((mMetrics.maxElementSize->width > mMetrics.width) ||
         (mMetrics.maxElementSize->height > mMetrics.height))) {
      printf("nsBlockReflowContext: ");
      nsFrame::ListTag(stdout, aFrame);
      printf(": WARNING: maxElementSize=%d,%d > metrics=%d,%d\n",
             mMetrics.maxElementSize->width,
             mMetrics.maxElementSize->height,
             mMetrics.width, mMetrics.height);
    }
#endif
    if ((mMetrics.width == nscoord(0xdeadbeef)) ||
        (mMetrics.height == nscoord(0xdeadbeef)) ||
        (mMetrics.ascent == nscoord(0xdeadbeef)) ||
        (mMetrics.descent == nscoord(0xdeadbeef))) {
      printf("nsBlockReflowContext: ");
      nsFrame::ListTag(stdout, aFrame);
      printf(" didn't set whad %d,%d,%d,%d!\n",
             mMetrics.width, mMetrics.height,
             mMetrics.ascent, mMetrics.descent);
    }
  }
#endif
#ifdef NOISY_MAX_ELEMENT_SIZE
  if (!NS_INLINE_IS_BREAK_BEFORE(aFrameReflowStatus)) {
    if (nsnull != mMetrics.maxElementSize) {
      printf("  ");
      nsFrame::ListTag(stdout, aFrame);
      printf(": maxElementSize=%d,%d wh=%d,%d\n",
             mMetrics.maxElementSize->width,
             mMetrics.maxElementSize->height,
             mMetrics.width, mMetrics.height);
    }
  }
#endif

  aFrame->GetFrameState(&state);
  if (0 == (NS_FRAME_OUTSIDE_CHILDREN & state)) {
    // Provide combined area for child that doesn't have any
    mMetrics.mCombinedArea.x = 0;
    mMetrics.mCombinedArea.y = 0;
    mMetrics.mCombinedArea.width = mMetrics.width;
    mMetrics.mCombinedArea.height = mMetrics.height;
  }

  // Now that frame has been reflowed at least one time make sure that
  // the NS_FRAME_FIRST_REFLOW bit is cleared so that never give it an
  // initial reflow reason again.
  if (eReflowReason_Initial == reason) {
    aFrame->SetFrameState(state & ~NS_FRAME_FIRST_REFLOW);
  }

  if (!NS_INLINE_IS_BREAK_BEFORE(aFrameReflowStatus)) {
    // If frame is complete and has a next-in-flow, we need to delete
    // them now. Do not do this when a break-before is signaled because
    // the frame is going to get reflowed again (and may end up wanting
    // a next-in-flow where it ends up).
    if (NS_FRAME_IS_COMPLETE(aFrameReflowStatus)) {
      nsIFrame* kidNextInFlow;
      aFrame->GetNextInFlow(&kidNextInFlow);
      if (nsnull != kidNextInFlow) {
        // Remove all of the childs next-in-flows. Make sure that we ask
        // the right parent to do the removal (it's possible that the
        // parent is not this because we are executing pullup code)
/* XXX promote DeleteChildsNextInFlow to nsIFrame to elminate this cast */
        nsHTMLContainerFrame* parent;
        aFrame->GetParent((nsIFrame**)&parent);
        parent->DeleteChildsNextInFlow(*mPresContext, aFrame);
      }
    }
  }

  return rv;
}

/**
 * Attempt to place the block frame within the available space.  If
 * it fits, apply horizontal positioning (CSS 10.3.3), collapse
 * margins (CSS2 8.3.1). Also apply relative positioning.
 */
PRBool
nsBlockReflowContext::PlaceBlock(PRBool aForceFit,
                                 const nsMargin& aComputedOffsets,
                                 nscoord* aBottomMarginResult,
                                 nsRect& aInFlowBounds,
                                 nsRect& aCombinedRect)
{
  // Compute collapsed bottom margin value
  nscoord collapsedBottomMargin = MaxMargin(mMetrics.mCarriedOutBottomMargin,
                                            mMargin.bottom);
  *aBottomMarginResult = collapsedBottomMargin;

  // See if the block will fit in the available space
  PRBool fits = PR_TRUE;
  nscoord x = mX;
  nscoord y = mY;
  if (0 == mMetrics.height) {
    // For empty blocks we revert the y coordinate back so that the
    // top margin is no longer applied.
    y = mSpace.y;
    if (IsHTMLParagraph(mFrame)) {
      // Special "feature" for HTML compatability - empty paragraphs
      // collapse into nothingness, including their margins.
      *aBottomMarginResult = 0;
    }
    else {
      // Collapse the bottom margin with the top margin that was already
      // applied.
      nscoord newBottomMargin = MaxMargin(collapsedBottomMargin, mTopMargin);
      *aBottomMarginResult = newBottomMargin;
    }

    // Empty blocks do not have anything special done to them and they
    // always fit.
    nsRect r(x, y, 0, 0);
    mFrame->SetRect(r);
    aInFlowBounds = r;

    // Retain combined area information in case we contain a floater
    // and nothing else.
    aCombinedRect = mMetrics.mCombinedArea;
    aCombinedRect.x += x;
    aCombinedRect.y += y;
  }
  else {
    // See if the frame fit. If its the first frame then it always
    // fits.
    if (aForceFit || (y + mMetrics.height <= mSpace.YMost())) {
      // Get style unit associated with the left and right margins
      nsStyleUnit leftUnit = mStyleSpacing->mMargin.GetLeftUnit();
      if (eStyleUnit_Inherit == leftUnit) {
        leftUnit = GetRealMarginLeftUnit();
      }
      nsStyleUnit rightUnit = mStyleSpacing->mMargin.GetRightUnit();
      if (eStyleUnit_Inherit == rightUnit) {
        rightUnit = GetRealMarginRightUnit();
      }

      // Apply post-reflow horizontal alignment. When a block element
      // doesn't use it all of the available width then we need to
      // align it using the text-align property. Note that
      // block-non-replaced elements will always take up the available
      // width (counting the margins!) so we shouldn't be handling
      // them here.
      if (NS_UNCONSTRAINEDSIZE != mSpace.width) {
        nscoord remainder = mSpace.XMost() -
          (x + mMetrics.width + mMargin.right);
        if (remainder > 0) {
          // The block frame didn't use all of the available
          // space. Synthesize margins for its horizontal placement.
          if (eStyleUnit_Auto == leftUnit) {
            if (eStyleUnit_Auto == rightUnit) {
              // When both margins are auto, we center the block
              x += remainder / 2;
            }
            else {
              // When the left margin is auto we right align the block
              x += remainder;
            }
          }
          else if (eStyleUnit_Auto != rightUnit) {
            PRBool doCSS = PR_TRUE;
            if (mIsTable) {
              const nsStyleText* styleText;
              mOuterReflowState.frame->GetStyleData(eStyleStruct_Text,
                                           (const nsStyleStruct*&)styleText);
              // This is a navigator compatability case: tables are
              // affected by the text alignment of the containing
              // block. CSS doesn't do this, so we use special
              // text-align attribute values to signal these
              // compatability cases.
              switch (styleText->mTextAlign) {
                case NS_STYLE_TEXT_ALIGN_MOZ_RIGHT:
                  x += remainder;
                  doCSS = PR_FALSE;
                  break;
                case NS_STYLE_TEXT_ALIGN_MOZ_CENTER:
                  x += remainder / 2;
                  doCSS = PR_FALSE;
                  break;
              }
            }
            if (doCSS) {
              // When neither margin is auto then the block is said to
              // be over constrained, Depending on the direction, choose
              // which margin to treat as auto.
              PRUint8 direction = mOuterReflowState.mStyleDisplay->mDirection;
              if (NS_STYLE_DIRECTION_RTL == direction) {
                // The left margin becomes auto
                x += remainder;
              }
              else {
                // The right margin becomes auto which is a no-op
              }
            }
          }
        }
      }

      // Update the in-flow bounding box's bounds. Include the margins.
      aInFlowBounds.SetRect(x, y,
                            mMetrics.width + mMargin.right,
                            mMetrics.height);


      // Apply CSS relative positioning to update x,y coordinates
      const nsStylePosition* stylePos;
      mFrame->GetStyleData(eStyleStruct_Position,
                           (const nsStyleStruct*&)stylePos);
      if (NS_STYLE_POSITION_RELATIVE == stylePos->mPosition) {
        x += aComputedOffsets.left;
        y += aComputedOffsets.top;
      }

      // Compute combined-rect in callers coordinate system. The value
      // returned in the reflow metrics is relative to the child
      // frame.
      aCombinedRect.x = mMetrics.mCombinedArea.x + x;
      aCombinedRect.y = mMetrics.mCombinedArea.y + y;
      aCombinedRect.width = mMetrics.mCombinedArea.width;
      aCombinedRect.height = mMetrics.mCombinedArea.height;

      // Now place the frame
      mFrame->SetRect(nsRect(x, y, mMetrics.width, mMetrics.height));

      // If the block frame ended up moving then we need to slide
      // anything inside of it that impacts the space manager
      // (otherwise the impacted space in the space manager will be
      // out of sync with where the frames really are).
      nscoord dx = x - mX;
      nscoord dy = y - mY;
      if ((0 != dx) || (0 != dy)) {
        nsIHTMLReflow* htmlReflow;
        nsresult rv;
        rv = mFrame->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow);
        if (NS_SUCCEEDED(rv)) {
          // If the child has any floaters that impact the space manager,
          // slide them now
          htmlReflow->MoveInSpaceManager(mPresContext,
                                         mOuterReflowState.spaceManager,
                                         dx, dy);
        }
      }

      // Adjust the max-element-size in the metrics to take into
      // account the margins around the block element. Note that we
      // use the collapsed top and bottom margin values.
      if (nsnull != mMetrics.maxElementSize) {
        nsSize* m = mMetrics.maxElementSize;
        // Do not allow auto margins to impact the max-element size
        // since they are springy and don't really count!
        if (eStyleUnit_Auto != leftUnit) {
          m->width += mMargin.left;
        }
        if (eStyleUnit_Auto != rightUnit) {
          m->width += mMargin.right;
        }

#if XXX_fix_me
        // Margin height should affect the max-element height (since
        // auto top/bottom margins are always zero)
        m->height += mTopMargin + mBottomMargin;
#endif
      }
    }
    else {
      fits = PR_FALSE;
    }
  }

  return fits;
}

// If we have an inherited margin its possible that its auto all the
// way up to the top of the tree. If that is the case, we need to know
// it.
nsStyleUnit
nsBlockReflowContext::GetRealMarginLeftUnit()
{
  nsStyleUnit unit = eStyleUnit_Inherit;
  nsIStyleContext* sc;
  mFrame->GetStyleContext(&sc);
  while ((nsnull != sc) && (eStyleUnit_Inherit == unit)) {
    // Get parent style context
    nsIStyleContext* psc;
    psc = sc->GetParent();
    NS_RELEASE(sc);
    sc = psc;
    if (nsnull != sc) {
      const nsStyleSpacing* spacing = (const nsStyleSpacing*)
        sc->GetStyleData(eStyleStruct_Spacing);
      unit = spacing->mMargin.GetLeftUnit();
    }
  }
  NS_IF_RELEASE(sc);
  return unit;
}

// If we have an inherited margin its possible that its auto all the
// way up to the top of the tree. If that is the case, we need to know
// it.
nsStyleUnit
nsBlockReflowContext::GetRealMarginRightUnit()
{
  nsStyleUnit unit = eStyleUnit_Inherit;
  nsIStyleContext* sc;
  mFrame->GetStyleContext(&sc);
  while ((nsnull != sc) && (eStyleUnit_Inherit == unit)) {
    // Get parent style context
    nsIStyleContext* psc;
    psc = sc->GetParent();
    NS_RELEASE(sc);
    sc = psc;
    if (nsnull != sc) {
      const nsStyleSpacing* spacing = (const nsStyleSpacing*)
        sc->GetStyleData(eStyleStruct_Spacing);
      unit = spacing->mMargin.GetRightUnit();
    }
  }
  NS_IF_RELEASE(sc);
  return unit;
}
