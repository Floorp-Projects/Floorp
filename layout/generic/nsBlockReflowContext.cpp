/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsBlockReflowContext.h"
#include "nsLineLayout.h"
#include "nsHTMLIIDs.h"
#include "nsISpaceManager.h"
#include "nsIFontMetrics.h"
#include "nsIPresContext.h"
#include "nsIContent.h"
#include "nsIStyleContext.h"
#include "nsIReflowCommand.h"
#include "nsHTMLContainerFrame.h"
#include "nsBlockFrame.h"
#include "nsIDOMHTMLParagraphElement.h"
#include "nsCOMPtr.h"

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
                                           PRBool aComputeMaxElementSize,
                                           PRBool aComputeMaximumWidth)
  : mPresContext(aPresContext),
    mOuterReflowState(aParentRS),
    mMetrics(aComputeMaxElementSize ? &mMaxElementSize : nsnull),
    mMaxElementSize(0, 0),
    mComputeMaximumWidth(aComputeMaximumWidth)
{
  mStyleSpacing = nsnull;
}

PRBool
nsBlockReflowContext::IsHTMLParagraph(nsIFrame* aFrame)
{
  PRBool result = PR_FALSE;
  nsCOMPtr<nsIContent> content;
  nsresult rv = aFrame->GetContent(getter_AddRefs(content));
  if (NS_SUCCEEDED(rv) && content) {
    nsCOMPtr<nsIDOMHTMLParagraphElement> p(do_QueryInterface(content));
    if (p) {
      result = PR_TRUE;
    }
  }
  return result;
}

nscoord
nsBlockReflowContext::ComputeCollapsedTopMargin(nsIPresContext* aPresContext,
                                                nsHTMLReflowState& aRS)
{
  // Get aFrame's top margin
  nscoord topMargin = aRS.mComputedMargin.top;

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
        nsSize availSpace(aRS.mComputedWidth, aRS.mComputedHeight);
        nsHTMLReflowState reflowState(aPresContext, aRS, childFrame,
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

struct nsBlockHorizontalAlign {
  nscoord mXOffset;  // left edge
  nscoord mLeftMargin;
  nscoord mRightMargin;
};

// Given the width of the block frame and a suggested x-offset calculate
// the actual x-offset taking into account horizontal alignment. Also returns
// the actual left and right margin
void
nsBlockReflowContext::AlignBlockHorizontally(nscoord                 aWidth,
                                             nsBlockHorizontalAlign &aAlign)
{
  // Initialize OUT parameters
  aAlign.mLeftMargin = mMargin.left;
  aAlign.mRightMargin = mMargin.right;

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
  // align it using the text-align property.
  if (NS_UNCONSTRAINEDSIZE != mSpace.width) {
    // It is possible that the object reflowed was given a
    // constrained width and ended up picking a different width
    // (e.g. a table width a set width that ended up larger
    // because its contents required it). When this happens we
    // need to recompute auto margins because the reflow state's
    // computations are no longer valid.
    if (aWidth != mComputedWidth) {
      if (eStyleUnit_Auto == leftUnit) {
        aAlign.mXOffset = 0;
        aAlign.mLeftMargin = 0;
      }
      if (eStyleUnit_Auto == rightUnit) {
        aAlign.mRightMargin = 0;
      }
    }

    // Compute how much remaining space there is, and in special
    // cases apply it (normally we should get zero here because of
    // the logic in nsHTMLReflowState).
    nscoord remainingSpace = mSpace.XMost() - (aAlign.mXOffset + aWidth +
                             aAlign.mRightMargin);
    if (remainingSpace > 0) {
      // The block/table frame didn't use all of the available
      // space. Synthesize margins for its horizontal placement.
      if (eStyleUnit_Auto == leftUnit) {
        if (eStyleUnit_Auto == rightUnit) {
          // When both margins are auto, we center the block
          aAlign.mXOffset += remainingSpace / 2;
        }
        else {
          // When the left margin is auto we right align the block
          aAlign.mXOffset += remainingSpace;
        }
      }
      else if (eStyleUnit_Auto != rightUnit) {
        // The block/table doesn't have auto margins.
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
            case NS_STYLE_TEXT_ALIGN_RIGHT:
              aAlign.mXOffset += remainingSpace;
              doCSS = PR_FALSE;
              break;
            case NS_STYLE_TEXT_ALIGN_MOZ_CENTER:
            case NS_STYLE_TEXT_ALIGN_CENTER:
              aAlign.mXOffset += remainingSpace / 2;
              doCSS = PR_FALSE;
              break;
          }
        }
        if (doCSS) {
// XXX It's not clear we can ever get here because for normal blocks,
// their size will be well defined by the nsHTMLReflowState logic
// (maybe width=0 cases get here?)
          // When neither margin is auto then the block is said to
          // be over constrained, Depending on the direction, choose
          // which margin to treat as auto.
          PRUint8 direction = mOuterReflowState.mStyleDisplay->mDirection;
          if (NS_STYLE_DIRECTION_RTL == direction) {
            // The left margin becomes auto
            aAlign.mXOffset += remainingSpace;
          }
          else {
            // The right margin becomes auto which is a no-op
          }
        }
      }
    }
  }
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

    // If we should compute the maximum width, then let the block know
    if (mComputeMaximumWidth) {
      mMetrics.mFlags |= NS_REFLOW_CALC_MAX_WIDTH;
    }
  }
  else if (mOuterReflowState.reason == eReflowReason_StyleChange) {
    reason = eReflowReason_StyleChange;
  }
  else if (mOuterReflowState.reason == eReflowReason_Dirty) {
    if (state & NS_FRAME_IS_DIRTY)
      reason = eReflowReason_Dirty;
  }
  else {
    if (mOuterReflowState.reason == eReflowReason_Incremental) {
      // If the incremental reflow command is a StyleChanged reflow
      // and it's target is the current block, then make sure we send
      // StyleChange reflow reasons down to all the children so that
      // they don't over-optimize their reflow.
      nsIReflowCommand* rc = mOuterReflowState.reflowCommand;
      if (rc) {
        nsIReflowCommand::ReflowType type;
        rc->GetType(type);
        if (type == nsIReflowCommand::StyleChanged) {
          nsIFrame* target;
          rc->GetTarget(target);
          if (target == mOuterReflowState.frame) {
            reason = eReflowReason_StyleChange;
          }
        }
        else if (type == nsIReflowCommand::ReflowDirty &&
                 (state & NS_FRAME_IS_DIRTY)) {
          reason = eReflowReason_Dirty;
        }
      }
    }
  }

  // Setup reflow state for reflowing the frame
  // XXX subtract out vertical margin?
  nsSize availSpace(aSpace.width, aSpace.height);
  nsHTMLReflowState reflowState(mPresContext, mOuterReflowState, aFrame,
                                availSpace, reason);
  aComputedOffsets = reflowState.mComputedOffsets;
  reflowState.mLineLayout = nsnull;
  if (!aIsAdjacentWithTop) {
    reflowState.isTopOfPage = PR_FALSE;  // make sure this is cleared
  }
  mIsTable = NS_STYLE_DISPLAY_TABLE == reflowState.mStyleDisplay->mDisplay;
  mComputedWidth = reflowState.mComputedWidth;

  nscoord topMargin = 0;
  if (aApplyTopMargin) {
    // Compute the childs collapsed top margin (its margin collpased
    // with its first childs top-margin -- recursively).
    topMargin = ComputeCollapsedTopMargin(mPresContext, reflowState);

#ifdef NOISY_VERTICAL_MARGINS
    nsFrame::ListTag(stdout, mOuterReflowState.frame);
    printf(": reflowing ");
    nsFrame::ListTag(stdout, aFrame);
    printf(" prevBottomMargin=%d, collapsedTopMargin=%d => %d\n",
           aPrevBottomMargin, topMargin,
           MaxMargin(topMargin, aPrevBottomMargin));
#endif

    // Collapse that value with the previous bottom margin to perform
    // the sibling to sibling collaspe.
    topMargin = MaxMargin(topMargin, aPrevBottomMargin);

    // Adjust the available height if its constrained so that the
    // child frame doesn't think it can reflow into its margin area.
    if (aApplyTopMargin && (NS_UNCONSTRAINEDSIZE != reflowState.availableHeight)) {
      reflowState.availableHeight -= topMargin;
    }
  }
  mTopMargin = topMargin;

  // Compute x/y coordinate where reflow will begin. Use the rules
  // from 10.3.3 to determine what to apply. At this point in the
  // reflow auto left/right margins will have a zero value.
  mMargin = reflowState.mComputedMargin;
  mStyleSpacing = reflowState.mStyleSpacing;
  nscoord x;
  nscoord y = aSpace.y + topMargin;

  // If it's a right floated element, then calculate the x-offset
  // differently
  if (NS_STYLE_FLOAT_RIGHT == reflowState.mStyleDisplay->mFloats) {
    nscoord frameWidth;
     
    if (NS_UNCONSTRAINEDSIZE == reflowState.mComputedWidth) {
      nsSize  frameSize;

      // Use the current frame width
      aFrame->GetSize(frameSize);
      frameWidth = frameSize.width;

    } else {
      frameWidth = reflowState.mComputedWidth +
                   reflowState.mComputedBorderPadding.left +
                   reflowState.mComputedBorderPadding.right;
    }

    // if this is an unconstrained width reflow, then just place the floater at the left margin
    if (NS_UNCONSTRAINEDSIZE == aSpace.width)
      x = aSpace.x;
    else
      x = aSpace.XMost() - mMargin.right - frameWidth;

  } else {
    x = aSpace.x + mMargin.left;
  }
  mX = x;
  mY = y;

  // If it's an auto-width table, then it doesn't behave like other blocks
  if (mIsTable && !reflowState.mStyleDisplay->IsFloating()) {
    // If this isn't the table's initial reflow, then use its existing
    // width to determine where it will be placed horizontally
    if (reflowState.reason != eReflowReason_Initial) {
      nsBlockHorizontalAlign  align;
      nsSize                  size;

      aFrame->GetSize(size);
      align.mXOffset = x;
      AlignBlockHorizontally(size.width, align);
      // Don't reset "mX". because PlaceBlock() will recompute the
      // x-offset and expects "mX" to be at the left margin edge
      x = align.mXOffset;
    }
  }

  // If the element is relatively positioned, then adjust x and y accordingly
  if (NS_STYLE_POSITION_RELATIVE == reflowState.mStylePosition->mPosition) {
    x += reflowState.mComputedOffsets.left;
    y += reflowState.mComputedOffsets.top;
  }

  // Let frame know that we are reflowing it
  aFrame->WillReflow(mPresContext);

  // Position it and its view (if it has one)
  // Note: Use "x" and "y" and not "mX" and "mY" because they more accurately
  // represents where we think the block will be placed
  aFrame->MoveTo(mPresContext, x, y);
  nsIView*  view;
  aFrame->GetView(mPresContext, &view);
  if (view) {
    nsContainerFrame::PositionFrameView(mPresContext, aFrame, view);
  }

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
  mOuterReflowState.mSpaceManager->Translate(tx, ty);

  // See if this is the child's initial reflow and we are supposed to
  // compute our maximum width
  if (mComputeMaximumWidth && (eReflowReason_Initial == reason)) {
    nscoord oldAvailableWidth = reflowState.availableWidth;
    nscoord oldComputedWidth = reflowState.mComputedWidth;

    reflowState.availableWidth = NS_UNCONSTRAINEDSIZE;
    reflowState.mComputedWidth = NS_UNCONSTRAINEDSIZE;
    rv = aFrame->Reflow(mPresContext, mMetrics, reflowState,
                        aFrameReflowStatus);

    // Update the reflow metrics with the maximum width
    mMetrics.mMaximumWidth = mMetrics.width;

    // The second reflow is just as a resize reflow with the constrained
    // width
    reflowState.availableWidth = oldAvailableWidth;
    reflowState.mComputedWidth = oldComputedWidth;
    reason = eReflowReason_Resize;
  }
  rv = aFrame->Reflow(mPresContext, mMetrics, reflowState,
                      aFrameReflowStatus);
  mOuterReflowState.mSpaceManager->Translate(-tx, -ty);

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
    // Provide overflow area for child that doesn't have any
    mMetrics.mOverflowArea.x = 0;
    mMetrics.mOverflowArea.y = 0;
    mMetrics.mOverflowArea.width = mMetrics.width;
    mMetrics.mOverflowArea.height = mMetrics.height;
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
        parent->DeleteChildsNextInFlow(mPresContext, aFrame);
      }
    }
  }

  // If the block is shrink wrapping its width, then see if we have percentage
  // based margins. If so, we can calculate them now that we know the shrink
  // wrap width
  if (NS_SHRINKWRAPWIDTH == reflowState.mComputedWidth) {
    nscoord boxWidth = mMetrics.width;
    float   leftPct = 0.0;
    float   rightPct = 0.0;
    
    if (eStyleUnit_Percent == reflowState.mStyleSpacing->mMargin.GetLeftUnit()) {
      nsStyleCoord  leftCoord;
      
      reflowState.mStyleSpacing->mMargin.GetLeft(leftCoord);
      leftPct = leftCoord.GetPercentValue();

    } else {
      boxWidth += mMargin.left;
    }

    if (eStyleUnit_Percent == reflowState.mStyleSpacing->mMargin.GetRightUnit()) {
      nsStyleCoord  rightCoord;

      reflowState.mStyleSpacing->mMargin.GetRight(rightCoord);
      rightPct = rightCoord.GetPercentValue();

    } else {
      boxWidth += mMargin.right;
    }

    // The total shrink wrap width "sww" is calculated by the expression:
    //   sww = bw + (mp * sww)
    // where "bw" is the box width (frame width plus margins that aren't percentage
    // based) and "mp" are the total margin percentages (i.e., the left percentage
    // value plus the right percentage value)
    // Solving for "sww" gives us:
    //  sww = bw / (1 - mp)
    // Note that this is only well defined for "mp" less than 100%
    float marginPct = leftPct + rightPct;
    if (marginPct >= 1.0) {
      // Ignore the right percentage and just use the left percentage
      // XXX Pay attention to direction property...
      marginPct = leftPct;
      rightPct = 0.0;
    }

    if ((marginPct > 0.0) && (marginPct < 1.0)) {
      double shrinkWrapWidth = float(boxWidth) / (1.0 - marginPct);

      if (eStyleUnit_Percent == reflowState.mStyleSpacing->mMargin.GetLeftUnit()) {
        mMargin.left = NSToCoordFloor(shrinkWrapWidth * leftPct);
        mX += mMargin.left;
      }
      if (eStyleUnit_Percent == reflowState.mStyleSpacing->mMargin.GetRightUnit()) {
        mMargin.right = NSToCoordFloor(shrinkWrapWidth * rightPct);
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
  // When deciding whether it's an empty paragraph we also need to take into
  // account the overflow area
  if ((0 == mMetrics.height) && (0 == mMetrics.mOverflowArea.height)) {
    if (IsHTMLParagraph(mFrame)) {
      // Special "feature" for HTML compatability - empty paragraphs
      // collapse into nothingness, including their margins. Signal
      // the special nature here by returning -1.
      *aBottomMarginResult = -1;
#ifdef NOISY_VERTICAL_MARGINS
      printf("  ");
      nsFrame::ListTag(stdout, mOuterReflowState.frame);
      printf(": ");
      nsFrame::ListTag(stdout, mFrame);
      printf(" -- zapping top & bottom margin; y=%d spaceY=%d\n",
             y, mSpace.y);
#endif
    }
    else {
      // Collapse the bottom margin with the top margin that was already
      // applied.
      nscoord newBottomMargin = MaxMargin(collapsedBottomMargin, mTopMargin);
      *aBottomMarginResult = newBottomMargin;
#ifdef NOISY_VERTICAL_MARGINS
      printf("  ");
      nsFrame::ListTag(stdout, mOuterReflowState.frame);
      printf(": ");
      nsFrame::ListTag(stdout, mFrame);
      printf(" -- collapsing top & bottom margin together; y=%d spaceY=%d\n",
             y, mSpace.y);
#endif
    }

#if XXX
    // For empty blocks we revert the y coordinate back so that the
    // top margin is no longer applied.
    nsBlockFrame* bf;
    nsresult rv = mFrame->QueryInterface(kBlockFrameCID, (void**)&bf);
    if (NS_SUCCEEDED(rv)) {
      // XXX This isn't good enough. What if the floater was placed
      // downward, just below another floater?
      nscoord dy = mSpace.y - mY;
      bf->MoveInSpaceManager(mPresContext, mOuterReflowState.mSpaceManager,
                             dy);
    }
#endif
    y = mSpace.y;

    // Empty blocks do not have anything special done to them and they
    // always fit. Note: don't force the width to 0
    nsRect r(x, y, mMetrics.width, 0);

    // Now place the frame and complete the reflow process
    nsContainerFrame::FinishReflowChild(mFrame, mPresContext, mMetrics, x, y, 0);
    aInFlowBounds = r;

    // Retain combined area information in case we contain a floater
    // and nothing else.
    aCombinedRect = mMetrics.mOverflowArea;
    aCombinedRect.x += x;
    aCombinedRect.y += y;
  }
  else {
    // See if the frame fit. If its the first frame then it always
    // fits.
    if (aForceFit || (y + mMetrics.height <= mSpace.YMost())) {
      // Calculate the actual x-offset and left and right margin
      nsBlockHorizontalAlign  align;
      
      align.mXOffset = x;
      AlignBlockHorizontally(mMetrics.width, align);
      x = align.mXOffset;
      mMargin.left = align.mLeftMargin;
      mMargin.right = align.mRightMargin;

      // Update the in-flow bounds rectangle
      aInFlowBounds.SetRect(x, y,
                            mMetrics.width,
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
      aCombinedRect.x = mMetrics.mOverflowArea.x + x;
      aCombinedRect.y = mMetrics.mOverflowArea.y + y;
      aCombinedRect.width = mMetrics.mOverflowArea.width;
      aCombinedRect.height = mMetrics.mOverflowArea.height;

      // Now place the frame and complete the reflow process
      nsContainerFrame::FinishReflowChild(mFrame, mPresContext, mMetrics, x, y, 0);

// XXX obsolete, i believe...
#if 0
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
                                         mOuterReflowState.mSpaceManager,
                                         dx, dy);
        }
      }
#endif

      // Adjust the max-element-size in the metrics to take into
      // account the margins around the block element. Note that we
      // use the collapsed top and bottom margin values.
      if (nsnull != mMetrics.maxElementSize) {
        nsSize* m = mMetrics.maxElementSize;
        // Do not allow auto margins to impact the max-element size
        // since they are springy and don't really count!
        if (eStyleUnit_Auto != mStyleSpacing->mMargin.GetLeftUnit()) {
          m->width += mMargin.left;
        }
        if (eStyleUnit_Auto != mStyleSpacing->mMargin.GetRightUnit()) {
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
      // Send the DidReflow() notification, but don't bother placing
      // the frame
      mFrame->DidReflow(mPresContext, NS_FRAME_REFLOW_FINISHED);
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
