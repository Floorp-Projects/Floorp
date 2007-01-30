/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Baron <dbaron@dbaron.org>
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

/* class that a parent frame uses to reflow a block frame */

#include "nsBlockReflowContext.h"
#include "nsLineLayout.h"
#include "nsSpaceManager.h"
#include "nsIFontMetrics.h"
#include "nsPresContext.h"
#include "nsFrameManager.h"
#include "nsIContent.h"
#include "nsStyleContext.h"
#include "nsHTMLContainerFrame.h"
#include "nsBlockFrame.h"
#include "nsLineBox.h"
#include "nsIDOMHTMLTableCellElement.h"
#include "nsIDOMHTMLBodyElement.h"
#include "nsGkAtoms.h"
#include "nsCOMPtr.h"
#include "nsLayoutUtils.h"

#ifdef NS_DEBUG
#undef  NOISY_MAX_ELEMENT_SIZE
#undef   REALLY_NOISY_MAX_ELEMENT_SIZE
#undef  NOISY_VERTICAL_MARGINS
#else
#undef  NOISY_MAX_ELEMENT_SIZE
#undef   REALLY_NOISY_MAX_ELEMENT_SIZE
#undef  NOISY_VERTICAL_MARGINS
#endif

nsBlockReflowContext::nsBlockReflowContext(nsPresContext* aPresContext,
                                           const nsHTMLReflowState& aParentRS)
  : mPresContext(aPresContext),
    mOuterReflowState(aParentRS),
    mMetrics()
{
  mStyleBorder = nsnull;
  mStyleMargin = nsnull;
  mStylePadding = nsnull;
}

static nsIFrame* DescendIntoBlockLevelFrame(nsIFrame* aFrame)
{
  nsIAtom* type = aFrame->GetType();
  if (type == nsGkAtoms::columnSetFrame)
    return DescendIntoBlockLevelFrame(aFrame->GetFirstChild(nsnull));
  return aFrame;
}

PRBool
nsBlockReflowContext::ComputeCollapsedTopMargin(const nsHTMLReflowState& aRS,
  nsCollapsingMargin* aMargin, nsIFrame* aClearanceFrame,
  PRBool* aMayNeedRetry, PRBool* aBlockIsEmpty)
{
  // Include frame's top margin
  aMargin->Include(aRS.mComputedMargin.top);

  // The inclusion of the bottom margin when empty is done by the caller
  // since it doesn't need to be done by the top-level (non-recursive)
  // caller.

#ifdef NOISY_VERTICAL_MARGINS
  nsFrame::ListTag(stdout, aRS.frame);
  printf(": %d => %d\n", aRS.mComputedMargin.top, aMargin->get());
#endif

  PRBool dirtiedLine = PR_FALSE;
  PRBool setBlockIsEmpty = PR_FALSE;

  // Calculate the frame's generational top-margin from its child
  // blocks. Note that if the frame has a non-zero top-border or
  // top-padding then this step is skipped because it will be a margin
  // root.  It is also skipped if the frame is a margin root for other
  // reasons.
  void* bf;
  nsIFrame* frame = DescendIntoBlockLevelFrame(aRS.frame);
  nsPresContext* prescontext = frame->GetPresContext();
  if (0 == aRS.mComputedBorderPadding.top &&
      !(frame->GetStateBits() & NS_BLOCK_MARGIN_ROOT) &&
      NS_SUCCEEDED(frame->QueryInterface(kBlockFrameCID, &bf))) {
    // iterate not just through the lines of 'block' but also its
    // overflow lines and the normal and overflow lines of its next in
    // flows. Note that this will traverse some frames more than once:
    // for example, if A contains B and A->nextinflow contains
    // B->nextinflow, we'll traverse B->nextinflow twice. But this is
    // OK because our traversal is idempotent.
    for (nsBlockFrame* block = NS_STATIC_CAST(nsBlockFrame*, frame);
         block; block = NS_STATIC_CAST(nsBlockFrame*, block->GetNextInFlow())) {
      for (PRBool overflowLines = PR_FALSE; overflowLines <= PR_TRUE; ++overflowLines) {
        nsBlockFrame::line_iterator line;
        nsBlockFrame::line_iterator line_end;
        PRBool anyLines = PR_TRUE;
        if (overflowLines) {
          nsLineList* lines = block->GetOverflowLines();
          if (!lines) {
            anyLines = PR_FALSE;
          } else {
            line = lines->begin();
            line_end = lines->end();
          }
        } else {
          line = block->begin_lines();
          line_end = block->end_lines();
        }
        for (; anyLines && line != line_end; ++line) {
          if (!aClearanceFrame && line->HasClearance()) {
            // If we don't have a clearance frame, then we're computing
            // the collapsed margin in the first pass, assuming that all
            // lines have no clearance. So clear their clearance flags.
            line->ClearHasClearance();
            line->MarkDirty();
            dirtiedLine = PR_TRUE;
          }
          
          PRBool isEmpty;
          if (line->IsInline()) {
            isEmpty = line->IsEmpty();
          } else {
            nsIFrame* kid = line->mFirstChild;
            if (kid == aClearanceFrame) {
              line->SetHasClearance();
              line->MarkDirty();
              dirtiedLine = PR_TRUE;
              goto done;
            }
            // Here is where we recur. Now that we have determined that a
            // generational collapse is required we need to compute the
            // child blocks margin and so in so that we can look into
            // it. For its margins to be computed we need to have a reflow
            // state for it.
            
            // We may have to construct an extra reflow state here if
            // we drilled down through a block wrapper. At the moment
            // we can only drill down one level so we only have to support
            // one extra reflow state.
            const nsHTMLReflowState* outerReflowState = &aRS;
            if (frame != aRS.frame) {
              NS_ASSERTION(frame->GetParent() == aRS.frame,
                           "Can only drill through one level of block wrapper");
              nsSize availSpace(aRS.ComputedWidth(), aRS.mComputedHeight);
              outerReflowState = new nsHTMLReflowState(prescontext,
                                                       aRS, frame, availSpace);
              if (!outerReflowState)
                goto done;
            }
            {
              nsSize availSpace(outerReflowState->ComputedWidth(),
                                outerReflowState->mComputedHeight);
              nsHTMLReflowState innerReflowState(prescontext,
                                                 *outerReflowState, kid,
                                                 availSpace);
              // Record that we're being optimistic by assuming the kid
              // has no clearance
              if (kid->GetStyleDisplay()->mBreakType != NS_STYLE_CLEAR_NONE) {
                *aMayNeedRetry = PR_TRUE;
              }
              if (ComputeCollapsedTopMargin(innerReflowState, aMargin, aClearanceFrame, aMayNeedRetry, &isEmpty)) {
                line->MarkDirty();
                dirtiedLine = PR_TRUE;
              }
              if (isEmpty)
                aMargin->Include(innerReflowState.mComputedMargin.bottom);
            }
            if (outerReflowState != &aRS) {
              delete NS_CONST_CAST(nsHTMLReflowState*, outerReflowState);
            }
          }
          if (!isEmpty) {
            if (!setBlockIsEmpty && aBlockIsEmpty) {
              setBlockIsEmpty = PR_TRUE;
              *aBlockIsEmpty = PR_FALSE;
            }
            goto done;
          }
        }
        if (!setBlockIsEmpty && aBlockIsEmpty) {
          // The first time we reach here is when this is the first block
          // and we have processed all its normal lines.
          setBlockIsEmpty = PR_TRUE;
          // All lines are empty, or we wouldn't be here!
          *aBlockIsEmpty = aRS.frame->IsSelfEmpty();
        }
      }
    }
  done:
    ;
  }

  if (!setBlockIsEmpty && aBlockIsEmpty) {
    *aBlockIsEmpty = aRS.frame->IsEmpty();
  }
  
#ifdef NOISY_VERTICAL_MARGINS
  nsFrame::ListTag(stdout, aRS.frame);
  printf(": => %d\n", aMargin->get());
#endif

  return dirtiedLine;
}

static void
nsPointDtor(void *aFrame, nsIAtom *aPropertyName,
            void *aPropertyValue, void *aDtorData)
{
  nsPoint *point = NS_STATIC_CAST(nsPoint*, aPropertyValue);
  delete point;
}

nsresult
nsBlockReflowContext::ReflowBlock(const nsRect&       aSpace,
                                  PRBool              aApplyTopMargin,
                                  nsCollapsingMargin& aPrevMargin,
                                  nscoord             aClearance,
                                  PRBool              aIsAdjacentWithTop,
                                  nsMargin&           aComputedOffsets,
                                  nsHTMLReflowState&  aFrameRS,
                                  nsReflowStatus&     aFrameReflowStatus)
{
  nsresult rv = NS_OK;
  mFrame = aFrameRS.frame;
  mSpace = aSpace;

  const nsStyleDisplay* display = mFrame->GetStyleDisplay();

  aComputedOffsets = aFrameRS.mComputedOffsets;
  if (NS_STYLE_POSITION_RELATIVE == display->mPosition) {
    nsPropertyTable *propTable = mPresContext->PropertyTable();

    nsPoint *offsets = NS_STATIC_CAST(nsPoint*,
        propTable->GetProperty(mFrame, nsGkAtoms::computedOffsetProperty));

    if (offsets)
      offsets->MoveTo(aComputedOffsets.left, aComputedOffsets.top);
    else {
      offsets = new nsPoint(aComputedOffsets.left, aComputedOffsets.top);
      if (offsets)
        propTable->SetProperty(mFrame, nsGkAtoms::computedOffsetProperty,
                               offsets, nsPointDtor, nsnull);
    }
  }

  aFrameRS.mLineLayout = nsnull;
  if (!aIsAdjacentWithTop) {
    aFrameRS.mFlags.mIsTopOfPage = PR_FALSE;  // make sure this is cleared
  }
  mComputedWidth = aFrameRS.ComputedWidth();

  if (aApplyTopMargin) {
    mTopMargin = aPrevMargin;

#ifdef NOISY_VERTICAL_MARGINS
    nsFrame::ListTag(stdout, mOuterReflowState.frame);
    printf(": reflowing ");
    nsFrame::ListTag(stdout, mFrame);
    printf(" margin => %d, clearance => %d\n", mTopMargin.get(), aClearance);
#endif

    // Adjust the available height if its constrained so that the
    // child frame doesn't think it can reflow into its margin area.
    if (NS_UNCONSTRAINEDSIZE != aFrameRS.availableHeight) {
      aFrameRS.availableHeight -= mTopMargin.get() + aClearance;
    }
  }

  // Compute x/y coordinate where reflow will begin. Use the rules
  // from 10.3.3 to determine what to apply. At this point in the
  // reflow auto left/right margins will have a zero value.
  mMargin = aFrameRS.mComputedMargin;
  mStyleBorder = aFrameRS.mStyleBorder;
  mStyleMargin = aFrameRS.mStyleMargin;
  mStylePadding = aFrameRS.mStylePadding;
  nscoord x;
  nscoord y = mSpace.y + mTopMargin.get() + aClearance;

  // If it's a right floated element, then calculate the x-offset
  // differently
  if (NS_STYLE_FLOAT_RIGHT == aFrameRS.mStyleDisplay->mFloats) {
    nscoord frameWidth;
     
    if (NS_UNCONSTRAINEDSIZE == aFrameRS.ComputedWidth()) {
      // Use the current frame width
      frameWidth = mFrame->GetSize().width;
    } else {
      frameWidth = aFrameRS.ComputedWidth() +
                   aFrameRS.mComputedBorderPadding.left +
                   aFrameRS.mComputedBorderPadding.right;
    }

    // if this is an unconstrained width reflow, then just place the float at the left margin
    if (NS_UNCONSTRAINEDSIZE == mSpace.width)
      x = mSpace.x;
    else
      x = mSpace.XMost() - mMargin.right - frameWidth;

  } else {
    x = mSpace.x + mMargin.left;
  }
  mX = x;
  mY = y;

   // Compute the translation to be used for adjusting the spacemanagager
   // coordinate system for the frame.  The spacemanager coordinates are
   // <b>inside</b> the callers border+padding, but the x/y coordinates
   // are not (recall that frame coordinates are relative to the parents
   // origin and that the parents border/padding is <b>inside</b> the
   // parent frame. Therefore we have to subtract out the parents
   // border+padding before translating.
   nscoord tx = x - mOuterReflowState.mComputedBorderPadding.left;
   nscoord ty = y - mOuterReflowState.mComputedBorderPadding.top;
 
  // If the element is relatively positioned, then adjust x and y accordingly
  if (NS_STYLE_POSITION_RELATIVE == aFrameRS.mStyleDisplay->mPosition) {
    x += aFrameRS.mComputedOffsets.left;
    y += aFrameRS.mComputedOffsets.top;
  }

  // Let frame know that we are reflowing it
  mFrame->WillReflow(mPresContext);

  // Position it and its view (if it has one)
  // Note: Use "x" and "y" and not "mX" and "mY" because they more accurately
  // represents where we think the block will be placed
  // XXXldb That's fine for view positioning, but not for reflow!
  mFrame->SetPosition(nsPoint(x, y));
  nsContainerFrame::PositionFrameView(mFrame);

#ifdef DEBUG
  mMetrics.width = nscoord(0xdeadbeef);
  mMetrics.height = nscoord(0xdeadbeef);
#endif

  mOuterReflowState.mSpaceManager->Translate(tx, ty);
  rv = mFrame->Reflow(mPresContext, mMetrics, aFrameRS, aFrameReflowStatus);
  mOuterReflowState.mSpaceManager->Translate(-tx, -ty);

#ifdef DEBUG
  if (!NS_INLINE_IS_BREAK_BEFORE(aFrameReflowStatus)) {
    if (CRAZY_WIDTH(mMetrics.width) || CRAZY_HEIGHT(mMetrics.height)) {
      printf("nsBlockReflowContext: ");
      nsFrame::ListTag(stdout, mFrame);
      printf(" metrics=%d,%d!\n", mMetrics.width, mMetrics.height);
    }
    if ((mMetrics.width == nscoord(0xdeadbeef)) ||
        (mMetrics.height == nscoord(0xdeadbeef))) {
      printf("nsBlockReflowContext: ");
      nsFrame::ListTag(stdout, mFrame);
      printf(" didn't set w/h %d,%d!\n", mMetrics.width, mMetrics.height);
    }
  }
#endif

  if (!(NS_FRAME_OUTSIDE_CHILDREN & mFrame->GetStateBits())) {
    // Provide overflow area for child that doesn't have any
    mMetrics.mOverflowArea.x = 0;
    mMetrics.mOverflowArea.y = 0;
    mMetrics.mOverflowArea.width = mMetrics.width;
    mMetrics.mOverflowArea.height = mMetrics.height;
  }

  if (!NS_INLINE_IS_BREAK_BEFORE(aFrameReflowStatus) ||
      (mFrame->GetStateBits() & NS_FRAME_OUT_OF_FLOW)) {
    // If frame is complete and has a next-in-flow, we need to delete
    // them now. Do not do this when a break-before is signaled because
    // the frame is going to get reflowed again (and may end up wanting
    // a next-in-flow where it ends up), unless it is an out of flow frame.
    if (NS_FRAME_IS_COMPLETE(aFrameReflowStatus)) {
      nsIFrame* kidNextInFlow = mFrame->GetNextInFlow();
      if (nsnull != kidNextInFlow) {
        // Remove all of the childs next-in-flows. Make sure that we ask
        // the right parent to do the removal (it's possible that the
        // parent is not this because we are executing pullup code).
        // Floats will eventually be removed via nsBlockFrame::RemoveFloat
        // which detaches the placeholder from the float.
/* XXX promote DeleteChildsNextInFlow to nsIFrame to elminate this cast */
        NS_STATIC_CAST(nsHTMLContainerFrame*, kidNextInFlow->GetParent())
          ->DeleteNextInFlowChild(mPresContext, kidNextInFlow);
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
nsBlockReflowContext::PlaceBlock(const nsHTMLReflowState& aReflowState,
                                 PRBool                   aForceFit,
                                 nsLineBox*               aLine,
                                 const nsMargin&          aComputedOffsets,
                                 nsCollapsingMargin&      aBottomMarginResult,
                                 nsRect&                  aInFlowBounds,
                                 nsRect&                  aCombinedRect,
                                 nsReflowStatus           aReflowStatus)
{
  // Compute collapsed bottom margin value.
  if (NS_FRAME_IS_COMPLETE(aReflowStatus)) {
    aBottomMarginResult = mMetrics.mCarriedOutBottomMargin;
    aBottomMarginResult.Include(mMargin.bottom);
  } else {
    // The used bottom-margin is set to zero above a break.
    aBottomMarginResult.Zero();
  }

  nscoord x = mX;
  nscoord y = mY;
  nscoord backupContainingBlockAdvance = 0;

  // Check whether the block's bottom margin collapses with its top
  // margin. See CSS 2.1 section 8.3.1; those rules seem to match
  // nsBlockFrame::IsEmpty(). Any such block must have zero height so
  // check that first. Note that a block can have clearance and still
  // have adjoining top/bottom margins, because the clearance goes
  // above the top margin.
  // Mark the frame as non-dirty; it has been reflowed (or we wouldn't
  // be here), and we don't want to assert in CachedIsEmpty()
  mFrame->RemoveStateBits(NS_FRAME_IS_DIRTY);
  PRBool empty = 0 == mMetrics.height && aLine->CachedIsEmpty();
  if (empty) {
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
    // Section 8.3.1 of CSS 2.1 says that blocks with adjoining
    // top/bottom margins whose top margin collapses with their
    // parent's top margin should have their top border-edge at the
    // top border-edge of their parent. We actually don't have to do
    // anything special to make this happen. In that situation,
    // nsBlockFrame::ShouldApplyTopMargin will have returned PR_FALSE,
    // and mTopMargin and aClearance will have been zero in
    // ReflowBlock.

    // If we did apply our top margin, but now we're collapsing it
    // into the bottom margin, we need to back up the containing
    // block's y-advance by our top margin so that it doesn't get
    // counted twice. Note that here we're allowing the line's bounds
    // to become different from the block's position; we do this
    // because the containing block will place the next line at the
    // line's YMost, and it must place the next line at a different
    // point from where this empty block will be.
    backupContainingBlockAdvance = mTopMargin.get();
  }

  // See if the frame fit. If it's the first frame or empty then it
  // always fits. If the height is unconstrained then it always fits,
  // even if there's some sort of integer overflow that makes y +
  // mMetrics.height appear to go beyond the available height.
  if (!empty && !aForceFit && mSpace.height != NS_UNCONSTRAINEDSIZE) {
    nscoord yMost = y - backupContainingBlockAdvance + mMetrics.height;
    if (yMost > mSpace.YMost()) {
      // didn't fit, we must acquit.
      mFrame->DidReflow(mPresContext, &aReflowState, NS_FRAME_REFLOW_FINISHED);
      return PR_FALSE;
    }
  }

  aInFlowBounds = nsRect(x, y - backupContainingBlockAdvance,
                         mMetrics.width, mMetrics.height);
  
  // Apply CSS relative positioning
  const nsStyleDisplay* styleDisp = mFrame->GetStyleDisplay();
  if (NS_STYLE_POSITION_RELATIVE == styleDisp->mPosition) {
    x += aComputedOffsets.left;
    y += aComputedOffsets.top;
  }
  
  // Now place the frame and complete the reflow process
  nsContainerFrame::FinishReflowChild(mFrame, mPresContext, &aReflowState, mMetrics, x, y, 0);
  
  aCombinedRect = mMetrics.mOverflowArea + nsPoint(x, y);

  return PR_TRUE;
}
