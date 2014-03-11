/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* class that a parent frame uses to reflow a block frame */

#include "nsBlockReflowContext.h"
#include "nsBlockReflowState.h"
#include "nsFloatManager.h"
#include "nsContainerFrame.h"
#include "nsBlockFrame.h"
#include "nsLineBox.h"
#include "nsLayoutUtils.h"

#ifdef DEBUG
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
    mMetrics(aParentRS.GetWritingMode())
{
}

static nsIFrame* DescendIntoBlockLevelFrame(nsIFrame* aFrame)
{
  nsIAtom* type = aFrame->GetType();
  if (type == nsGkAtoms::columnSetFrame)
    return DescendIntoBlockLevelFrame(aFrame->GetFirstPrincipalChild());
  return aFrame;
}

bool
nsBlockReflowContext::ComputeCollapsedTopMargin(const nsHTMLReflowState& aRS,
  nsCollapsingMargin* aMargin, nsIFrame* aClearanceFrame,
  bool* aMayNeedRetry, bool* aBlockIsEmpty)
{
  // Include frame's top margin
  aMargin->Include(aRS.ComputedPhysicalMargin().top);

  // The inclusion of the bottom margin when empty is done by the caller
  // since it doesn't need to be done by the top-level (non-recursive)
  // caller.

#ifdef NOISY_VERTICAL_MARGINS
  nsFrame::ListTag(stdout, aRS.frame);
  printf(": %d => %d\n", aRS.ComputedPhysicalMargin().top, aMargin->get());
#endif

  bool dirtiedLine = false;
  bool setBlockIsEmpty = false;

  // Calculate the frame's generational top-margin from its child
  // blocks. Note that if the frame has a non-zero top-border or
  // top-padding then this step is skipped because it will be a margin
  // root.  It is also skipped if the frame is a margin root for other
  // reasons.
  nsIFrame* frame = DescendIntoBlockLevelFrame(aRS.frame);
  nsPresContext* prescontext = frame->PresContext();
  nsBlockFrame* block = nullptr;
  if (0 == aRS.ComputedPhysicalBorderPadding().top) {
    block = nsLayoutUtils::GetAsBlock(frame);
    if (block) {
      bool topMarginRoot, unused;
      block->IsMarginRoot(&topMarginRoot, &unused);
      if (topMarginRoot) {
        block = nullptr;
      }
    }
  }

  // iterate not just through the lines of 'block' but also its
  // overflow lines and the normal and overflow lines of its next in
  // flows. Note that this will traverse some frames more than once:
  // for example, if A contains B and A->nextinflow contains
  // B->nextinflow, we'll traverse B->nextinflow twice. But this is
  // OK because our traversal is idempotent.
  for ( ;block; block = static_cast<nsBlockFrame*>(block->GetNextInFlow())) {
    for (int overflowLines = 0; overflowLines <= 1; ++overflowLines) {
      nsBlockFrame::line_iterator line;
      nsBlockFrame::line_iterator line_end;
      bool anyLines = true;
      if (overflowLines) {
        nsBlockFrame::FrameLines* frames = block->GetOverflowLines();
        nsLineList* lines = frames ? &frames->mLines : nullptr;
        if (!lines) {
          anyLines = false;
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
          dirtiedLine = true;
        }
        
        bool isEmpty;
        if (line->IsInline()) {
          isEmpty = line->IsEmpty();
        } else {
          nsIFrame* kid = line->mFirstChild;
          if (kid == aClearanceFrame) {
            line->SetHasClearance();
            line->MarkDirty();
            dirtiedLine = true;
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
            nsSize availSpace(aRS.ComputedWidth(), aRS.ComputedHeight());
            outerReflowState = new nsHTMLReflowState(prescontext,
                                                     aRS, frame, availSpace);
          }
          {
            nsSize availSpace(outerReflowState->ComputedWidth(),
                              outerReflowState->ComputedHeight());
            nsHTMLReflowState innerReflowState(prescontext,
                                               *outerReflowState, kid,
                                               availSpace);
            // Record that we're being optimistic by assuming the kid
            // has no clearance
            if (kid->StyleDisplay()->mBreakType != NS_STYLE_CLEAR_NONE) {
              *aMayNeedRetry = true;
            }
            if (ComputeCollapsedTopMargin(innerReflowState, aMargin, aClearanceFrame, aMayNeedRetry, &isEmpty)) {
              line->MarkDirty();
              dirtiedLine = true;
            }
            if (isEmpty)
              aMargin->Include(innerReflowState.ComputedPhysicalMargin().bottom);
          }
          if (outerReflowState != &aRS) {
            delete const_cast<nsHTMLReflowState*>(outerReflowState);
          }
        }
        if (!isEmpty) {
          if (!setBlockIsEmpty && aBlockIsEmpty) {
            setBlockIsEmpty = true;
            *aBlockIsEmpty = false;
          }
          goto done;
        }
      }
      if (!setBlockIsEmpty && aBlockIsEmpty) {
        // The first time we reach here is when this is the first block
        // and we have processed all its normal lines.
        setBlockIsEmpty = true;
        // All lines are empty, or we wouldn't be here!
        *aBlockIsEmpty = aRS.frame->IsSelfEmpty();
      }
    }
  }
  done:

  if (!setBlockIsEmpty && aBlockIsEmpty) {
    *aBlockIsEmpty = aRS.frame->IsEmpty();
  }
  
#ifdef NOISY_VERTICAL_MARGINS
  nsFrame::ListTag(stdout, aRS.frame);
  printf(": => %d\n", aMargin->get());
#endif

  return dirtiedLine;
}

nsresult
nsBlockReflowContext::ReflowBlock(const nsRect&       aSpace,
                                  bool                aApplyTopMargin,
                                  nsCollapsingMargin& aPrevMargin,
                                  nscoord             aClearance,
                                  bool                aIsAdjacentWithTop,
                                  nsLineBox*          aLine,
                                  nsHTMLReflowState&  aFrameRS,
                                  nsReflowStatus&     aFrameReflowStatus,
                                  nsBlockReflowState& aState)
{
  nsresult rv = NS_OK;
  mFrame = aFrameRS.frame;
  mSpace = aSpace;

  if (!aIsAdjacentWithTop) {
    aFrameRS.mFlags.mIsTopOfPage = false;  // make sure this is cleared
  }

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
    if (NS_UNCONSTRAINEDSIZE != aFrameRS.AvailableHeight()) {
      aFrameRS.AvailableHeight() -= mTopMargin.get() + aClearance;
    }
  }

  nscoord tx = 0, ty = 0;
  // The values of x and y do not matter for floats, so don't bother calculating
  // them. Floats are guaranteed to have their own float manager, so tx and ty
  // don't matter.  mX and mY don't matter becacuse they are only used in
  // PlaceBlock, which is not used for floats.
  if (aLine) {
    // Compute x/y coordinate where reflow will begin. Use the rules
    // from 10.3.3 to determine what to apply. At this point in the
    // reflow auto left/right margins will have a zero value.

    mX = tx = mSpace.x + aFrameRS.ComputedPhysicalMargin().left;
    mY = ty = mSpace.y + mTopMargin.get() + aClearance;

    if ((mFrame->GetStateBits() & NS_BLOCK_FLOAT_MGR) == 0)
      aFrameRS.mBlockDelta = mOuterReflowState.mBlockDelta + ty - aLine->mBounds.y;
  }

  // Let frame know that we are reflowing it
  mFrame->WillReflow(mPresContext);

#ifdef DEBUG
  mMetrics.Width() = nscoord(0xdeadbeef);
  mMetrics.Height() = nscoord(0xdeadbeef);
#endif

  mOuterReflowState.mFloatManager->Translate(tx, ty);
  rv = mFrame->Reflow(mPresContext, mMetrics, aFrameRS, aFrameReflowStatus);
  mOuterReflowState.mFloatManager->Translate(-tx, -ty);

#ifdef DEBUG
  if (!NS_INLINE_IS_BREAK_BEFORE(aFrameReflowStatus)) {
    if (CRAZY_SIZE(mMetrics.Width()) || CRAZY_SIZE(mMetrics.Height())) {
      printf("nsBlockReflowContext: ");
      nsFrame::ListTag(stdout, mFrame);
      printf(" metrics=%d,%d!\n", mMetrics.Width(), mMetrics.Height());
    }
    if ((mMetrics.Width() == nscoord(0xdeadbeef)) ||
        (mMetrics.Height() == nscoord(0xdeadbeef))) {
      printf("nsBlockReflowContext: ");
      nsFrame::ListTag(stdout, mFrame);
      printf(" didn't set w/h %d,%d!\n", mMetrics.Width(), mMetrics.Height());
    }
  }
#endif

  if (!mFrame->HasOverflowAreas()) {
    mMetrics.SetOverflowAreasToDesiredBounds();
  }

  if (!NS_INLINE_IS_BREAK_BEFORE(aFrameReflowStatus) ||
      (mFrame->GetStateBits() & NS_FRAME_OUT_OF_FLOW)) {
    // If frame is complete and has a next-in-flow, we need to delete
    // them now. Do not do this when a break-before is signaled because
    // the frame is going to get reflowed again (and may end up wanting
    // a next-in-flow where it ends up), unless it is an out of flow frame.
    if (NS_FRAME_IS_FULLY_COMPLETE(aFrameReflowStatus)) {
      nsIFrame* kidNextInFlow = mFrame->GetNextInFlow();
      if (nullptr != kidNextInFlow) {
        // Remove all of the childs next-in-flows. Make sure that we ask
        // the right parent to do the removal (it's possible that the
        // parent is not this because we are executing pullup code).
        // Floats will eventually be removed via nsBlockFrame::RemoveFloat
        // which detaches the placeholder from the float.
        nsOverflowContinuationTracker::AutoFinish fini(aState.mOverflowTracker, mFrame);
        static_cast<nsContainerFrame*>(kidNextInFlow->GetParent())
          ->DeleteNextInFlowChild(kidNextInFlow, true);
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
bool
nsBlockReflowContext::PlaceBlock(const nsHTMLReflowState& aReflowState,
                                 bool                     aForceFit,
                                 nsLineBox*               aLine,
                                 nsCollapsingMargin&      aBottomMarginResult,
                                 nsRect&                  aInFlowBounds,
                                 nsOverflowAreas&         aOverflowAreas,
                                 nsReflowStatus           aReflowStatus)
{
  // Compute collapsed bottom margin value.
  if (NS_FRAME_IS_COMPLETE(aReflowStatus)) {
    aBottomMarginResult = mMetrics.mCarriedOutBottomMargin;
    aBottomMarginResult.Include(aReflowState.ComputedPhysicalMargin().bottom);
  } else {
    // The used bottom-margin is set to zero above a break.
    aBottomMarginResult.Zero();
  }

  nsPoint position(mX, mY);
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
  bool empty = 0 == mMetrics.Height() && aLine->CachedIsEmpty();
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
           position.y, mSpace.y);
#endif
    // Section 8.3.1 of CSS 2.1 says that blocks with adjoining
    // top/bottom margins whose top margin collapses with their
    // parent's top margin should have their top border-edge at the
    // top border-edge of their parent. We actually don't have to do
    // anything special to make this happen. In that situation,
    // nsBlockFrame::ShouldApplyTopMargin will have returned false,
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
  // mMetrics.Height() appear to go beyond the available height.
  if (!empty && !aForceFit && mSpace.height != NS_UNCONSTRAINEDSIZE) {
    nscoord yMost = position.y - backupContainingBlockAdvance + mMetrics.Height();
    if (yMost > mSpace.YMost()) {
      // didn't fit, we must acquit.
      mFrame->DidReflow(mPresContext, &aReflowState, nsDidReflowStatus::FINISHED);
      return false;
    }
  }

  aInFlowBounds = nsRect(position.x, position.y - backupContainingBlockAdvance,
                         mMetrics.Width(), mMetrics.Height());
  
  aReflowState.ApplyRelativePositioning(&position);
  
  // Now place the frame and complete the reflow process
  nsContainerFrame::FinishReflowChild(mFrame, mPresContext, mMetrics,
                                      &aReflowState, position.x, position.y, 0);

  aOverflowAreas = mMetrics.mOverflowAreas + position;

  return true;
}
