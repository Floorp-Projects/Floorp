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
#include "nsFrameReflowState.h"
#include "nsLineLayout.h"
#include "nsHTMLIIDs.h"
#include "nsISpaceManager.h"
#include "nsIFontMetrics.h"
#include "nsIPresContext.h"
#include "nsIStyleContext.h"
#include "nsHTMLContainerFrame.h"

nsBlockReflowContext::nsBlockReflowContext(nsIPresContext& aPresContext,
                                           nsLineLayout& aLineLayout,
                                           nsFrameReflowState& aParentRS)
  : mPresContext(aPresContext),
    mLineLayout(aLineLayout),
    mOuterReflowState(aParentRS),
    mMetrics(aParentRS.mComputeMaxElementSize ? &mMaxElementSize : nsnull)
{
  mRunInFrame = nsnull;
  mCompactMarginWidth = 0;
  mSpacing = nsnull;
}

void
nsBlockReflowContext::ComputeMarginsFor(nsIPresContext& aPresContext,
                                        nsIFrame* aFrame,
                                        const nsStyleSpacing* aSpacing,
                                        const nsFrameReflowState& aParentRS,
                                        nsMargin& aResult)
{
  // XXX pass in mSpacing to ComputeMarginFor
  nsHTMLReflowState::ComputeMarginFor(aFrame, &aParentRS, aResult);

  // Compute auto top/bottom margin values
  nsStyleUnit topUnit = aSpacing->mMargin.GetTopUnit();
  nsStyleUnit bottomUnit = aSpacing->mMargin.GetBottomUnit();
  nscoord fontHeight = 0;
  if ((eStyleUnit_Auto == topUnit) || (eStyleUnit_Auto == bottomUnit)) {
    // XXX Use the font for the frame, not the default font???
    const nsFont& defaultFont = aPresContext.GetDefaultFont();
    nsIFontMetrics* fm = aPresContext.GetMetricsFor(defaultFont);
    fm->GetHeight(fontHeight);
    NS_RELEASE(fm);
  }

  // For auto margins use the font height computed above
  if (eStyleUnit_Auto == topUnit) {
    aResult.top = fontHeight;
  }
  if (eStyleUnit_Auto == bottomUnit) {
    aResult.bottom = fontHeight;
  }
}

nsresult
nsBlockReflowContext::ReflowBlock(nsIFrame* aFrame,
                                  const nsRect& aSpace,
                                  PRBool aIsAdjacentWithTop,
                                  nsReflowStatus& aFrameReflowStatus)
{
  nsresult rv = NS_OK;
  mFrame = aFrame;
  mSpace = aSpace;

  // Compute the margins (including auto margins)
  aFrame->GetStyleData(eStyleStruct_Spacing, (const nsStyleStruct*&)mSpacing);
  ComputeMarginsFor(mPresContext, aFrame, mSpacing, mOuterReflowState,
                    mMargin);

  // Compute x/y coordinate where reflow will begin. Use the rules
  // from 10.3.3 to determine what to apply. At this point in the
  // reflow auto left/right margins will have a zero value.
  nscoord x = aSpace.x + mMargin.left;
  nscoord y = aSpace.y;
  mX = x;
  mY = y;
  nsSize availSize(aSpace.width, aSpace.height);
  if (NS_UNCONSTRAINEDSIZE != aSpace.width) {
    availSize.width -= mMargin.left + mMargin.right;
  }
  if (NS_UNCONSTRAINEDSIZE != aSpace.height) {
    availSize.height -= mMargin.top + mMargin.bottom;
  }

  // Get reflow reason set correctly. It's possible that a child was
  // created and then it was decided that it could not be reflowed
  // (for example, a block frame that isn't at the start of a
  // line). In this case the reason will be wrong so we need to check
  // the frame state.
  nsReflowReason reason = eReflowReason_Resize;
  nsFrameState state;
  aFrame->GetFrameState(state);
  if (NS_FRAME_FIRST_REFLOW & state) {
    reason = eReflowReason_Initial;
  }
  else if (mOuterReflowState.mNextRCFrame == aFrame) {
    reason = eReflowReason_Incremental;
    // Make sure we only incrementally reflow once
    // XXX caller should do this, yes?
    mOuterReflowState.mNextRCFrame = nsnull;
  }

  // Setup reflow state for reflowing the frame
  nsHTMLReflowState reflowState(mPresContext, aFrame, mOuterReflowState,
                                availSize);
  reflowState.lineLayout = nsnull;
  reflowState.mRunInFrame = mRunInFrame;
  reflowState.mCompactMarginWidth = mCompactMarginWidth;
  reflowState.reason = reason;
  if (!aIsAdjacentWithTop) {
    reflowState.isTopOfPage = PR_FALSE;  // make sure this is cleared
  }
  mLineLayout.SetUnderstandsWhiteSpace(PR_FALSE);

  // Let frame know that we are reflowing it
  nsIHTMLReflow* htmlReflow;
  rv = aFrame->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow);
  if (NS_FAILED(rv)) {
    return rv;
  }
  htmlReflow->WillReflow(mPresContext);

  // Adjust spacemanager coordinate system for the frame. The
  // spacemanager coordinates are <b>inside</b> the callers
  // border+padding, but the x/y coordinates are not (recall that
  // frame coordinates are relative to the parents origin and that the
  // parents border/padding is <b>inside</b> the parent
  // frame. Therefore we have to subtract out the parents
  // border+padding before translating.
  nscoord tx = x - mOuterReflowState.mBorderPadding.left;
  nscoord ty = y - mOuterReflowState.mBorderPadding.top;
  aFrame->MoveTo(x, y);
  mOuterReflowState.spaceManager->Translate(tx, ty);
  rv = htmlReflow->Reflow(mPresContext, mMetrics, reflowState,
                          aFrameReflowStatus);
  mOuterReflowState.spaceManager->Translate(-tx, -ty);

  aFrame->GetFrameState(state);
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
      aFrame->GetNextInFlow(kidNextInFlow);
      if (nsnull != kidNextInFlow) {
        // Remove all of the childs next-in-flows. Make sure that we ask
        // the right parent to do the removal (it's possible that the
        // parent is not this because we are executing pullup code)
/* XXX promote DeleteChildsNextInFlow to nsIFrame to elminate this cast */
        nsHTMLContainerFrame* parent;
        aFrame->GetParent((nsIFrame*&) parent);
        parent->DeleteChildsNextInFlow(mPresContext, aFrame);
      }
    }
  }

  return rv;
}

/**
 * The CSS2 spec, section 8.3.1 defines margin collapsing to apply to
 * vertical margins of block boxes. And the spec also indicates that
 * this should be done for two or more adjacent vertical margins. It
 * also indicates that the margins collapse between boxes that are
 * next to each other or nested.
 */
void
nsBlockReflowContext::CollapseMargins(const nsMargin& aMargin,
                                      nscoord aCarriedOutTopMargin,
                                      nscoord aCarriedOutBottomMargin,
                                      nscoord aFrameHeight,
                                      nscoord aPrevBottomMargin,
                                      nscoord& aTopMarginResult,
                                      nscoord& aBottomMarginResult)
{
  // Compute the collapsed top margin value. The top margin value is a
  // 3 way margin collapse. First we collapse the carried out top
  // margin with the block frames top margin (this is a CSS2 "nested"
  // collapse). Then we collapse that value with the previous bottom
  // margin (because the collapsed margin is adjacent to the previous
  // bottom margin).
  nscoord carriedTopMargin = aCarriedOutTopMargin;
  nscoord topMargin = aMargin.top;
  topMargin = MaxMargin(topMargin, carriedTopMargin);
  topMargin = MaxMargin(topMargin, aPrevBottomMargin);

  // Compute the collapsed bottom margin value. The bottom margin
  // value is a 2 way margin collapse. Collapse the carried out bottom
  // margin with the block frames bottom margin (this is a CSS2
  // "nested" collapse).
  nscoord carriedBottomMargin = aCarriedOutBottomMargin;
  nscoord bottomMargin = aMargin.bottom;
  bottomMargin = MaxMargin(bottomMargin, carriedBottomMargin);

  // If the block line is empty then we collapse the top and bottom
  // margin values (because the margins are adjacent).
  if (0 == aFrameHeight) {
    nscoord collapsedMargin = MaxMargin(topMargin, bottomMargin);
    topMargin = 0;
    bottomMargin = collapsedMargin;
  }

  aTopMarginResult = topMargin;
  aBottomMarginResult = bottomMargin;
}

/**
 * Attempt to place the block frame within the available space.  If
 * it fits, apply horizontal positioning (CSS 10.3.3), collapse
 * margins (CSS2 8.3.1). Also apply relative positioning.
 */
PRBool
nsBlockReflowContext::PlaceBlock(PRBool aForceFit, PRBool aApplyTopMargin,
                                 nscoord aPrevBottomMargin,
                                 nsRect& aInFlowBounds,
                                 nsRect& aCombinedRect)
{
  // Compute collapsed margin value
  CollapseMargins(mMargin, mMetrics.mCarriedOutTopMargin,
                  mMetrics.mCarriedOutBottomMargin,
                  mMetrics.height, aPrevBottomMargin,
                  mTopMargin, mBottomMargin);

  // See if the block will fit in the available space
  PRBool fits;
  if (0 == mMetrics.height) {
    // Empty blocks do not have anything special done to them and they
    // always fit.
    nsRect r(mX, mY, 0, 0);
    mFrame->SetRect(r);
    aInFlowBounds = r;
    aCombinedRect = r;
    fits = PR_TRUE;
  }
  else {
    nscoord x = mX;
    nscoord y = mY;

    // Apply top margin unless it's going to be carried out.
    if (aApplyTopMargin) {
      y += mTopMargin;
    }

    // Position block horizontally according to CSS2 10.3.3
    if (NS_UNCONSTRAINEDSIZE != mSpace.width) {
      // XXX left/right block margins should be applied right here.
      // Because of the issue with floaters and block-left/right
      // margins this code is not yet complete. This code may need to
      // move to before reflowing the frame instead of after (because
      // of floater positioning).
      nscoord remainder = mSpace.XMost() -
        (x + mMetrics.width + mMargin.right);
      if (remainder >= 0) {
        // The block frame didn't use all of the available space. Apply
        // auto margins.
        nsStyleUnit leftUnit = mSpacing->mMargin.GetLeftUnit();
        nsStyleUnit rightUnit = mSpacing->mMargin.GetRightUnit();
        if (eStyleUnit_Auto == leftUnit) {
          if (eStyleUnit_Auto == rightUnit) {
            // When both margins are auto, we center the block
            x += remainder / 2;
          }
          else {
            x += remainder;
          }
        }
        else if (eStyleUnit_Auto != rightUnit) {
          // When neither margin is auto then text-align applies
          const nsStyleText* styleText = mOuterReflowState.mStyleText;
          switch (styleText->mTextAlign) {
          case NS_STYLE_TEXT_ALIGN_DEFAULT:
          case NS_STYLE_TEXT_ALIGN_JUSTIFY:
            if (NS_STYLE_DIRECTION_RTL == mOuterReflowState.mDirection) {
              // When given a default alignment, and a right-to-left
              // direction, right align the frame.
              x += remainder;
            }
            break;
          case NS_STYLE_TEXT_ALIGN_RIGHT:
            x += remainder;
            break;
          case NS_STYLE_TEXT_ALIGN_CENTER:
            x += remainder / 2;
            break;
          }
        }
      }
      else {
        // XXX Handle over-constrained case by forcing the appropriate
        // margin to be auto
      }
    }

    // See if the frame fit. If its the first frame then it always
    // fits.
    if (aForceFit || (y + mMetrics.height <= mSpace.YMost())) {
      fits = PR_TRUE;

      // Update the in-flow bounding box's bounds. Include the margins.
      aInFlowBounds.SetRect(x, y,
                            mMetrics.width + mMargin.right,
                            mMetrics.height);

      // Apply CSS relative positioning to update x,y coordinates
      const nsStylePosition* stylePos;
      mFrame->GetStyleData(eStyleStruct_Position,
                           (const nsStyleStruct*&)stylePos);
      if (NS_STYLE_POSITION_RELATIVE == stylePos->mPosition) {
        ComputeRelativePosition(mFrame, stylePos, x, y);
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
      if (mOuterReflowState.mComputeMaxElementSize) {
        nsSize* m = mMetrics.maxElementSize;
        m->width += mMargin.left + mMargin.right;
        m->height += mTopMargin + mBottomMargin;
      }
    }
    else {
      fits = PR_FALSE;
    }
  }

  return fits;
}

void
nsBlockReflowContext::ComputeRelativePosition(nsIFrame* aFrame,
                                              const nsStylePosition* aStylePos,
                                              nscoord& aX, nscoord& aY)
{
  nsStyleCoord coord;
  
  nscoord dx = 0;
  switch (aStylePos->mOffset.GetLeftUnit()) {
  case eStyleUnit_Percent:
    printf("XXX: not yet implemented: % relative position\n");
  case eStyleUnit_Auto:
    break;
  case eStyleUnit_Coord:
    dx = aStylePos->mOffset.GetLeft(coord).GetCoordValue();
    break;
  }
  aX += dx;

  nscoord dy = 0;
  switch (aStylePos->mOffset.GetTopUnit()) {
  case eStyleUnit_Percent:
    printf("XXX: not yet implemented: % relative position\n");
  case eStyleUnit_Auto:
    break;
  case eStyleUnit_Coord:
    dy = aStylePos->mOffset.GetTop(coord).GetCoordValue();
    break;
  }
  aY += dy;
}
