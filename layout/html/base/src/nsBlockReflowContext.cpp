/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Baron <dbaron@fas.harvard.edu>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsBlockReflowContext.h"
#include "nsLineLayout.h"
#include "nsHTMLIIDs.h"
#include "nsSpaceManager.h"
#include "nsIFontMetrics.h"
#include "nsIPresContext.h"
#include "nsIContent.h"
#include "nsIStyleContext.h"
#include "nsIReflowCommand.h"
#include "nsHTMLContainerFrame.h"
#include "nsBlockFrame.h"
#include "nsIDOMHTMLTableCellElement.h"
#include "nsIDOMHTMLBodyElement.h"
#include "nsLayoutAtoms.h"
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
    mIsTable(PR_FALSE),
    mComputeMaximumWidth(aComputeMaximumWidth),
    mBlockShouldInvalidateItself(PR_FALSE)
{
  mStyleBorder = nsnull;
  mStyleMargin = nsnull;
  mStylePadding = nsnull;
  if (mComputeMaximumWidth)
    mMetrics.mFlags |= NS_REFLOW_CALC_MAX_WIDTH;
}

void
nsBlockReflowContext::ComputeCollapsedTopMargin(nsIPresContext* aPresContext,
                                                nsHTMLReflowState& aRS,
                                   /* inout */  nsCollapsingMargin& aMargin)
{
  // Get aFrame's top margin
  aMargin.Include(aRS.mComputedMargin.top);

#ifdef NOISY_VERTICAL_MARGINS
  nsFrame::ListTag(stdout, aRS.frame);
  printf(": %d => %d\n", aRS.mComputedMargin.top, aMargin.get());
#endif

  // Calculate aFrame's generational top-margin from its child
  // blocks. Note that if aFrame has a non-zero top-border or
  // top-padding then this step is skipped because it will be a margin
  // root.  It is also skipped if the frame is a margin root for other
  // reasons.
  if (0 == aRS.mComputedBorderPadding.top) {
    nsFrameState state;
    aRS.frame->GetFrameState(&state);
    if (!(state & NS_BLOCK_MARGIN_ROOT)) {
      nsBlockFrame* bf;
      if (NS_SUCCEEDED(aRS.frame->QueryInterface(kBlockFrameCID,
                                         NS_REINTERPRET_CAST(void**, &bf)))) {
        // Ask the block frame for the top block child that we should
        // try to collapse the top margin with.

        // XXX If the block is empty, we need to check its bottom margin
        // and its sibling's top margin (etc.) too!  See XXXldb comment about
        // emptyness below in PlaceBlock.

        nsIFrame* childFrame = bf->GetTopBlockChild();
        if (nsnull != childFrame) {

          // Here is where we recur. Now that we have determined that a
          // generational collapse is required we need to compute the
          // child blocks margin and so in so that we can look into
          // it. For its margins to be computed we need to have a reflow
          // state for it.
          nsSize availSpace(aRS.mComputedWidth, aRS.mComputedHeight);
          nsHTMLReflowState reflowState(aPresContext, aRS, childFrame,
                                        availSpace);
          ComputeCollapsedTopMargin(aPresContext, reflowState, aMargin);
        }
      }
    }
  }

#ifdef NOISY_VERTICAL_MARGINS
  nsFrame::ListTag(stdout, aRS.frame);
  printf(": => %d\n", aMargin.get());
#endif
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
  nsStyleUnit leftUnit = mStyleMargin->mMargin.GetLeftUnit();
  if (eStyleUnit_Inherit == leftUnit) {
    leftUnit = GetRealMarginLeftUnit();
  }
  nsStyleUnit rightUnit = mStyleMargin->mMargin.GetRightUnit();
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

        // For normal (non-table) blocks we don't get here because
        // nsHTMLReflowState::CalculateBlockSideMargins handles this.
        // (I think there may be an exception to that, though...)

        // We use a special value of the text-align property for
        // HTML alignment (the CENTER element and DIV ALIGN=...)
        // since it acts on blocks and tables rather than just
        // being a text-align.
        // So, check the text-align value from the parent to see if
        // it has one of these special values.
        const nsStyleText* styleText = mOuterReflowState.mStyleText;
        if (styleText->mTextAlign == NS_STYLE_TEXT_ALIGN_MOZ_RIGHT) {
          aAlign.mXOffset += remainingSpace;
        } else if (styleText->mTextAlign == NS_STYLE_TEXT_ALIGN_MOZ_CENTER) {
          aAlign.mXOffset += remainingSpace / 2;
        } else {
          // If we don't have a special text-align value indicating
          // HTML alignment, then use the CSS rules.

          // When neither margin is auto then the block is said to
          // be over constrained, Depending on the direction, choose
          // which margin to treat as auto.
          PRUint8 direction = mOuterReflowState.mStyleVisibility->mDirection;
          if (NS_STYLE_DIRECTION_RTL == direction) {
            // The left margin becomes auto
            aAlign.mXOffset += remainingSpace;
          }
          //else {
            // The right margin becomes auto which is a no-op
          //}
        }
      }
    }
  }
}

nsresult
nsBlockReflowContext::ReflowBlock(nsIFrame* aFrame,
                                  const nsRect& aSpace,
                                  PRBool aApplyTopMargin,
                                  nsCollapsingMargin& aPrevBottomMargin,
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
      /*
      if (eReflowReason_Resize == reason) {
        // we're doing a resize reflow, even though our outer reflow state is incremental
        // text (and possibly other objects) don't do incremental painting for resize reflows
        // so, we have to handle the invalidation for repainting ourselves
        mBlockShouldInvalidateItself = PR_TRUE;
      }
      */
    }
  }

  // Setup reflow state for reflowing the frame
  // XXX subtract out vertical margin?
  nsSize availSpace(aSpace.width, aSpace.height);

  /* We build a different reflow context based on the width attribute of the block,
   * if it's a floater.
   * Auto-width floaters need to have their containing-block size set explicitly,
   * factoring in other floaters that impact it.  
   * It's possible this should be quirks-only.
   * All other blocks proceed normally.
   */
  // XXXldb We should really fix this in nsHTMLReflowState::InitConstraints instead.
  const nsStylePosition* position;
  aFrame->GetStyleData(eStyleStruct_Position,
                           (const nsStyleStruct*&)position);
  nsStyleUnit widthUnit = position->mWidth.GetUnit();
  const nsStyleDisplay* display;
  aFrame->GetStyleData(eStyleStruct_Display,
                           (const nsStyleStruct*&)display);
  if ((eStyleUnit_Auto == widthUnit) &&
      ((NS_STYLE_FLOAT_LEFT == display->mFloats) ||
       (NS_STYLE_FLOAT_RIGHT == display->mFloats))) {
    // Construct the reflow state using the ctor that explicitly
    // constrains the containing block's width and height to the
    // available width and height.
    nsHTMLReflowState autoReflowState(mPresContext, mOuterReflowState, aFrame,
                                      availSpace, aSpace.width, aSpace.height);
    autoReflowState.reason = reason;
    rv = DoReflowBlock(autoReflowState, reason, aFrame, aSpace, 
                       aApplyTopMargin, aPrevBottomMargin,
                       aIsAdjacentWithTop,
                       aComputedOffsets,
                       aFrameReflowStatus);
  }
  else {
    // Construct the reflow state using the ctor that will use the
    // containing block's computed width and height (or handle derive
    // appropriate values for an absolutely positioned frame).
    nsHTMLReflowState normalReflowState(mPresContext, mOuterReflowState, aFrame,
                                        availSpace, reason);
    rv = DoReflowBlock(normalReflowState, reason, aFrame, aSpace, 
                       aApplyTopMargin, aPrevBottomMargin,
                       aIsAdjacentWithTop,
                       aComputedOffsets,
                       aFrameReflowStatus);
  }
  return rv;
}

static void
ComputeShrinkwrapMargins(const nsStyleMargin* aStyleMargin, nscoord aWidth, nsMargin& aMargin, nscoord& aXToUpdate) {
  nscoord boxWidth = aWidth;
  float   leftPct = 0.0;
  float   rightPct = 0.0;
  
  if (eStyleUnit_Percent == aStyleMargin->mMargin.GetLeftUnit()) {
    nsStyleCoord  leftCoord;
    
    aStyleMargin->mMargin.GetLeft(leftCoord);
    leftPct = leftCoord.GetPercentValue();
    
  } else {
    boxWidth += aMargin.left;
  }
  
  if (eStyleUnit_Percent == aStyleMargin->mMargin.GetRightUnit()) {
    nsStyleCoord  rightCoord;
    
    aStyleMargin->mMargin.GetRight(rightCoord);
    rightPct = rightCoord.GetPercentValue();
    
  } else {
    boxWidth += aMargin.right;
  }
  
  // The total shrink wrap width "sww" is calculated by the expression:
  //   sww = bw + (mp * sww)
  // where "bw" is the box width (frame width plus margins that aren't percentage
  // based) and "mp" are the total margin percentages (i.e., the left percentage
  // value plus the right percentage value)
  // Solving for "sww" gives us:
  //  sww = bw / (1 - mp)
  // Note that this is only well defined for "mp" less than 100%

  // XXXldb  Um... percentage margins are based on the containing block width.
  float marginPct = leftPct + rightPct;
  if (marginPct >= 1.0) {
    // Ignore the right percentage and just use the left percentage
    // XXX Pay attention to direction property...
    marginPct = leftPct;
    rightPct = 0.0;
  }
  
  if ((marginPct > 0.0) && (marginPct < 1.0)) {
    double shrinkWrapWidth = float(boxWidth) / (1.0 - marginPct);
    
    if (eStyleUnit_Percent == aStyleMargin->mMargin.GetLeftUnit()) {
      aMargin.left = NSToCoordFloor((float)(shrinkWrapWidth * leftPct));
      aXToUpdate += aMargin.left;
    }
    if (eStyleUnit_Percent == aStyleMargin->mMargin.GetRightUnit()) {
      aMargin.right = NSToCoordFloor((float)(shrinkWrapWidth * rightPct));
    }
  }
}

nsresult
nsBlockReflowContext::DoReflowBlock(nsHTMLReflowState &aReflowState,
                                    nsReflowReason aReason,
                                    nsIFrame* aFrame,
                                    const nsRect& aSpace,
                                    PRBool aApplyTopMargin,
                                    nsCollapsingMargin& aPrevBottomMargin,
                                    PRBool aIsAdjacentWithTop,
                                    nsMargin& aComputedOffsets,
                                    nsReflowStatus& aFrameReflowStatus)
{
  nsresult rv = NS_OK;
  nsFrameState state;
  aFrame->GetFrameState(&state);
  aComputedOffsets = aReflowState.mComputedOffsets;
  aReflowState.mLineLayout = nsnull;
  if (!aIsAdjacentWithTop) {
    aReflowState.isTopOfPage = PR_FALSE;  // make sure this is cleared
  }
  mIsTable = NS_STYLE_DISPLAY_TABLE == aReflowState.mStyleDisplay->mDisplay;
  mComputedWidth = aReflowState.mComputedWidth;

  if (aApplyTopMargin) {
    // Compute the childs collapsed top margin (its margin collpased
    // with its first childs top-margin -- recursively).
    ComputeCollapsedTopMargin(mPresContext, aReflowState, aPrevBottomMargin);

#ifdef NOISY_VERTICAL_MARGINS
    nsFrame::ListTag(stdout, mOuterReflowState.frame);
    printf(": reflowing ");
    nsFrame::ListTag(stdout, aFrame);
    printf(" margin => %d\n", aPrevBottomMargin.get());
#endif

    // Adjust the available height if its constrained so that the
    // child frame doesn't think it can reflow into its margin area.
    if (NS_UNCONSTRAINEDSIZE != aReflowState.availableHeight) {
      aReflowState.availableHeight -= aPrevBottomMargin.get();
    }
  }
  mTopMargin = aPrevBottomMargin.get();

  // Compute x/y coordinate where reflow will begin. Use the rules
  // from 10.3.3 to determine what to apply. At this point in the
  // reflow auto left/right margins will have a zero value.
  mMargin = aReflowState.mComputedMargin;
  mStyleBorder = aReflowState.mStyleBorder;
  mStyleMargin = aReflowState.mStyleMargin;
  mStylePadding = aReflowState.mStylePadding;
  nscoord x;
  nscoord y = aSpace.y + mTopMargin;

  // If it's a right floated element, then calculate the x-offset
  // differently
  if (NS_STYLE_FLOAT_RIGHT == aReflowState.mStyleDisplay->mFloats) {
    nscoord frameWidth;
     
    if (NS_UNCONSTRAINEDSIZE == aReflowState.mComputedWidth) {
      nsSize  frameSize;

      // Use the current frame width
      aFrame->GetSize(frameSize);
      frameWidth = frameSize.width;

    } else {
      frameWidth = aReflowState.mComputedWidth +
                   aReflowState.mComputedBorderPadding.left +
                   aReflowState.mComputedBorderPadding.right;
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
  // XXX why not for a floating table too?
  if (mIsTable && !aReflowState.mStyleDisplay->IsFloating()) {
    // If this isn't the table's initial reflow, then use its existing
    // width to determine where it will be placed horizontally
    if (aReflowState.reason != eReflowReason_Initial) {
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
  if (NS_STYLE_POSITION_RELATIVE == aReflowState.mStyleDisplay->mPosition) {
    x += aReflowState.mComputedOffsets.left;
    y += aReflowState.mComputedOffsets.top;
  }

  // Let frame know that we are reflowing it
  aFrame->WillReflow(mPresContext);

  // Position it and its view (if it has one)
  // Note: Use "x" and "y" and not "mX" and "mY" because they more accurately
  // represents where we think the block will be placed
  aFrame->MoveTo(mPresContext, x, y);
  nsContainerFrame::PositionFrameView(mPresContext, aFrame);

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
  if (mComputeMaximumWidth && (eReflowReason_Initial == aReason)) {
    nscoord oldAvailableWidth = aReflowState.availableWidth;
    nscoord oldComputedWidth = aReflowState.mComputedWidth;

    aReflowState.availableWidth = NS_UNCONSTRAINEDSIZE;
    aReflowState.mComputedWidth = NS_UNCONSTRAINEDSIZE;
    rv = aFrame->Reflow(mPresContext, mMetrics, aReflowState,
                        aFrameReflowStatus);

    // Update the reflow metrics with the maximum width
    mMetrics.mMaximumWidth = mMetrics.width;
#ifdef NOISY_REFLOW
    printf("*** nsBlockReflowContext::ReflowBlock block %p returning max width %d\n", 
           aFrame, mMetrics.mMaximumWidth);
#endif
    // The second reflow is just as a resize reflow with the constrained
    // width
    aReflowState.availableWidth = oldAvailableWidth;
    aReflowState.mComputedWidth = oldComputedWidth;
    aReason = eReflowReason_Resize;
  }

  rv = aFrame->Reflow(mPresContext, mMetrics, aReflowState,
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
  if (eReflowReason_Initial == aReason) {
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
  if (NS_SHRINKWRAPWIDTH == aReflowState.mComputedWidth) {
    ComputeShrinkwrapMargins(aReflowState.mStyleMargin, mMetrics.width, mMargin, mX);
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
                                 nsCollapsingMargin& aBottomMarginResult,
                                 nsRect& aInFlowBounds,
                                 nsRect& aCombinedRect)
{
  // Compute collapsed bottom margin value
  aBottomMarginResult = mMetrics.mCarriedOutBottomMargin;
  aBottomMarginResult.Include(mMargin.bottom);

  // See if the block will fit in the available space
  PRBool fits = PR_TRUE;
  nscoord x = mX;
  nscoord y = mY;
  // When deciding whether it's empty we also need to take into
  // account the overflow area

  // XXXldb What should really matter is whether there exist non-
  // empty frames in the block (with appropriate whitespace munging).
  // Consider the case where we clip off the overflow with
  // 'overflow: hidden' (which doesn't currently affect mOverflowArea,
  // but probably should.
  if ((0 == mMetrics.height) && (0 == mMetrics.mOverflowArea.height)) 
  {
    // Collapse the bottom margin with the top margin that was already
    // applied.
    aBottomMarginResult.Include(mTopMargin);
#ifdef NOISY_VERTICAL_MARGINS
    printf("  ");
    nsFrame::ListTag(stdout, mOuterReflowState.frame);
    printf(": ");
    nsFrame::ListTag(stdout, mFrame);
    printf(" -- collapsing top & bottom margin together; y=%d spaceY=%d\n",
           y, mSpace.y);
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
    if (aForceFit || (y + mMetrics.height <= mSpace.YMost())) 
    {
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


      // Compute combined-rect in callers coordinate system. The value
      // returned in the reflow metrics is relative to the child
      // frame.
      aCombinedRect.x = mMetrics.mOverflowArea.x + x;
      aCombinedRect.y = mMetrics.mOverflowArea.y + y;
      aCombinedRect.width = mMetrics.mOverflowArea.width;
      aCombinedRect.height = mMetrics.mOverflowArea.height;

      // Apply CSS relative positioning to update x,y coordinates
      // Note that this must be done after changing aCombinedRect
      // since relatively positioned elements should act as if they
      // were at their original position.
      const nsStyleDisplay* styleDisp;
      mFrame->GetStyleData(eStyleStruct_Display,
                           (const nsStyleStruct*&)styleDisp);
      if (NS_STYLE_POSITION_RELATIVE == styleDisp->mPosition) {
        x += aComputedOffsets.left;
        y += aComputedOffsets.top;
      }

      // Now place the frame and complete the reflow process
      nsContainerFrame::FinishReflowChild(mFrame, mPresContext, mMetrics, x, y, 0);

      // Adjust the max-element-size in the metrics to take into
      // account the margins around the block element. Note that we
      // use the collapsed top and bottom margin values.
      if (nsnull != mMetrics.maxElementSize) {
        nsSize* m = mMetrics.maxElementSize;
        nsMargin maxElemMargin = mMargin;

        if (NS_SHRINKWRAPWIDTH == mComputedWidth) {
          nscoord dummyXOffset;
          // Base the margins on the max-element size
          ComputeShrinkwrapMargins(mStyleMargin, m->width, maxElemMargin, dummyXOffset);
        }

        // Do not allow auto margins to impact the max-element size
        // since they are springy and don't really count!
        if (eStyleUnit_Auto != mStyleMargin->mMargin.GetLeftUnit()) {
          m->width += maxElemMargin.left;
        }
        if (eStyleUnit_Auto != mStyleMargin->mMargin.GetRightUnit()) {
          m->width += maxElemMargin.right;
        }

#if 0 // XXX_fix_me
        // Margin height should affect the max-element height (since
        // auto top/bottom margins are always zero)

        // XXXldb Should it?
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
      const nsStyleMargin* margin = (const nsStyleMargin*)
        sc->GetStyleData(eStyleStruct_Margin);
      unit = margin->mMargin.GetLeftUnit();
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
      const nsStyleMargin* margin = (const nsStyleMargin*)
        sc->GetStyleData(eStyleStruct_Margin);
      unit = margin->mMargin.GetRightUnit();
    }
  }
  NS_IF_RELEASE(sc);
  return unit;
}
