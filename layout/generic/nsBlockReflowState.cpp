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
 *   Steve Clark <buster@netscape.com>
 *   Robert O'Callahan <roc+moz@cs.cmu.edu>
 *   L. David Baron <dbaron@dbaron.org>
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

#include "nsBlockReflowContext.h"
#include "nsBlockReflowState.h"
#include "nsBlockFrame.h"
#include "nsLineLayout.h"
#include "nsPresContext.h"
#include "nsLayoutAtoms.h"
#include "nsIFrame.h"
#include "nsFrameManager.h"

#include "nsINameSpaceManager.h"
#include "nsHTMLAtoms.h"


#ifdef DEBUG
#include "nsBlockDebugFlags.h"
#endif

nsBlockReflowState::nsBlockReflowState(const nsHTMLReflowState& aReflowState,
                                       nsPresContext* aPresContext,
                                       nsBlockFrame* aFrame,
                                       const nsHTMLReflowMetrics& aMetrics,
                                       PRBool aBlockMarginRoot)
  : mBlock(aFrame),
    mPresContext(aPresContext),
    mReflowState(aReflowState),
    mPrevBottomMargin(),
    mLineNumber(0),
    mFlags(0),
    mFloatBreakType(NS_STYLE_CLEAR_NONE)
{
  const nsMargin& borderPadding = BorderPadding();

  if (aBlockMarginRoot) {
    SetFlag(BRS_ISTOPMARGINROOT, PR_TRUE);
    SetFlag(BRS_ISBOTTOMMARGINROOT, PR_TRUE);
  }
  if (0 != aReflowState.mComputedBorderPadding.top) {
    SetFlag(BRS_ISTOPMARGINROOT, PR_TRUE);
  }
  if (0 != aReflowState.mComputedBorderPadding.bottom) {
    SetFlag(BRS_ISBOTTOMMARGINROOT, PR_TRUE);
  }
  if (GetFlag(BRS_ISTOPMARGINROOT)) {
    SetFlag(BRS_APPLYTOPMARGIN, PR_TRUE);
  }
  
  mSpaceManager = aReflowState.mSpaceManager;

  NS_ASSERTION(mSpaceManager,
               "SpaceManager should be set in nsBlockReflowState" );
  if (mSpaceManager) {
    // Translate into our content area and then save the 
    // coordinate system origin for later.
    mSpaceManager->Translate(borderPadding.left, borderPadding.top);
    mSpaceManager->GetTranslation(mSpaceManagerX, mSpaceManagerY);
  }

  mReflowStatus = NS_FRAME_COMPLETE;

  mPresContext = aPresContext;
  mBlock->GetNextInFlow(NS_REINTERPRET_CAST(nsIFrame**, &mNextInFlow));
  mKidXMost = 0;

  // Compute content area width (the content area is inside the border
  // and padding)
  if (NS_UNCONSTRAINEDSIZE != aReflowState.mComputedWidth) {
    mContentArea.width = aReflowState.mComputedWidth;
  }
  else {
    if (NS_UNCONSTRAINEDSIZE == aReflowState.availableWidth) {
      mContentArea.width = NS_UNCONSTRAINEDSIZE;
      SetFlag(BRS_UNCONSTRAINEDWIDTH, PR_TRUE);
    }
    else if (NS_UNCONSTRAINEDSIZE != aReflowState.mComputedMaxWidth) {
      // Choose a width based on the content (shrink wrap width) up
      // to the maximum width
      mContentArea.width = aReflowState.mComputedMaxWidth;
      SetFlag(BRS_SHRINKWRAPWIDTH, PR_TRUE);
    }
    else {
      nscoord lr = borderPadding.left + borderPadding.right;
      mContentArea.width = PR_MAX(0, aReflowState.availableWidth - lr);
    }
  }

  // Compute content area height. Unlike the width, if we have a
  // specified style height we ignore it since extra content is
  // managed by the "overflow" property. When we don't have a
  // specified style height then we may end up limiting our height if
  // the availableHeight is constrained (this situation occurs when we
  // are paginated).
  if (NS_UNCONSTRAINEDSIZE != aReflowState.availableHeight) {
    // We are in a paginated situation. The bottom edge is just inside
    // the bottom border and padding. The content area height doesn't
    // include either border or padding edge.
    mBottomEdge = aReflowState.availableHeight - borderPadding.bottom;
    mContentArea.height = PR_MAX(0, mBottomEdge - borderPadding.top);
  }
  else {
    // When we are not in a paginated situation then we always use
    // an constrained height.
    SetFlag(BRS_UNCONSTRAINEDHEIGHT, PR_TRUE);
    mContentArea.height = mBottomEdge = NS_UNCONSTRAINEDSIZE;
  }

  mY = borderPadding.top;
  mBand.Init(mSpaceManager, mContentArea);

  mPrevChild = nsnull;
  mCurrentLine = aFrame->end_lines();

  SetFlag(BRS_COMPUTEMAXELEMENTWIDTH, aMetrics.mComputeMEW);
#ifdef DEBUG
  if (nsBlockFrame::gNoisyMaxElementWidth) {
    nsFrame::IndentBy(stdout, nsBlockFrame::gNoiseIndent);
    printf("BRS: setting compute-MEW to %d\n", aMetrics.mComputeMEW);
  }
#endif
  mMaxElementWidth = 0;
  SetFlag(BRS_COMPUTEMAXWIDTH, 
          (NS_REFLOW_CALC_MAX_WIDTH == (aMetrics.mFlags & NS_REFLOW_CALC_MAX_WIDTH)));
  mMaximumWidth = 0;

  mMinLineHeight = nsHTMLReflowState::CalcLineHeight(mPresContext,
                                                     aReflowState.rendContext,
                                                     aReflowState.frame);
}

nsBlockReflowState::~nsBlockReflowState()
{
  // Restore the coordinate system, unless the space manager is null,
  // which means it was just destroyed.
  if (mSpaceManager) {
    const nsMargin& borderPadding = BorderPadding();
    mSpaceManager->Translate(-borderPadding.left, -borderPadding.top);
  }
}

nsLineBox*
nsBlockReflowState::NewLineBox(nsIFrame* aFrame,
                               PRInt32 aCount,
                               PRBool aIsBlock)
{
  return NS_NewLineBox(mPresContext->PresShell(), aFrame, aCount, aIsBlock);
}

void
nsBlockReflowState::FreeLineBox(nsLineBox* aLine)
{
  if (aLine) {
    aLine->Destroy(mPresContext->PresShell());
  }
}

// Compute the amount of available space for reflowing a block frame
// at the current Y coordinate. This method assumes that
// GetAvailableSpace has already been called.
void
nsBlockReflowState::ComputeBlockAvailSpace(nsIFrame* aFrame,
                                           nsSplittableType aSplitType,
                                           const nsStyleDisplay* aDisplay,
                                           nsRect& aResult)
{
#ifdef REALLY_NOISY_REFLOW
  printf("CBAS frame=%p has float count %d\n", aFrame, mBand.GetFloatCount());
  mBand.List();
#endif
  aResult.y = mY;
  aResult.height = GetFlag(BRS_UNCONSTRAINEDHEIGHT)
    ? NS_UNCONSTRAINEDSIZE
    : mBottomEdge - mY;

  const nsMargin& borderPadding = BorderPadding();

  /* bug 18445: treat elements mapped to display: block such as text controls
   * just like normal blocks   */
  // text controls are not splittable, so make a special case here
  // XXXldb Why not just set the frame state bit?
  PRBool treatAsNotSplittable =
    nsLayoutAtoms::textInputFrame == aFrame->GetType();

  if (NS_FRAME_SPLITTABLE_NON_RECTANGULAR == aSplitType ||    // normal blocks 
      NS_FRAME_NOT_SPLITTABLE == aSplitType ||                // things like images mapped to display: block
      PR_TRUE == treatAsNotSplittable)                        // text input controls mapped to display: block (special case)
  {
    if (mBand.GetFloatCount()) {
      // Use the float-edge property to determine how the child block
      // will interact with the float.
      const nsStyleBorder* borderStyle = aFrame->GetStyleBorder();
      switch (borderStyle->mFloatEdge) {
        default:
        case NS_STYLE_FLOAT_EDGE_CONTENT:  // content and only content does runaround of floats
          // The child block will flow around the float. Therefore
          // give it all of the available space.
          aResult.x = borderPadding.left;
          aResult.width = GetFlag(BRS_UNCONSTRAINEDWIDTH)
            ? NS_UNCONSTRAINEDSIZE
            : mContentArea.width;
           break;
        case NS_STYLE_FLOAT_EDGE_BORDER: 
        case NS_STYLE_FLOAT_EDGE_PADDING:
          {
            // The child block's border should be placed adjacent to,
            // but not overlap the float(s).
            nsMargin m(0, 0, 0, 0);
            const nsStyleMargin* styleMargin = aFrame->GetStyleMargin();
            styleMargin->GetMargin(m); // XXX percentage margins
            if (NS_STYLE_FLOAT_EDGE_PADDING == borderStyle->mFloatEdge) {
              // Add in border too
              nsMargin b;
              borderStyle->GetBorder(b);
              m += b;
            }

            // determine left edge
            if (mBand.GetLeftFloatCount()) {
              aResult.x = mAvailSpaceRect.x + borderPadding.left - m.left;
            }
            else {
              aResult.x = borderPadding.left;
            }

            // determine width
            if (GetFlag(BRS_UNCONSTRAINEDWIDTH)) {
              aResult.width = NS_UNCONSTRAINEDSIZE;
            }
            else {
              if (mBand.GetRightFloatCount()) {
                if (mBand.GetLeftFloatCount()) {
                  aResult.width = mAvailSpaceRect.width + m.left + m.right;
                }
                else {
                  aResult.width = mAvailSpaceRect.width + m.right;
                }
              }
              else {
                aResult.width = mAvailSpaceRect.width + m.left;
              }
            }
          }
          break;

        case NS_STYLE_FLOAT_EDGE_MARGIN:
          {
            // The child block's margins should be placed adjacent to,
            // but not overlap the float.
            aResult.x = mAvailSpaceRect.x + borderPadding.left;
            aResult.width = mAvailSpaceRect.width;
          }
          break;
      }
    }
    else {
      // Since there are no floats present the float-edge property
      // doesn't matter therefore give the block element all of the
      // available space since it will flow around the float itself.
      aResult.x = borderPadding.left;
      aResult.width = GetFlag(BRS_UNCONSTRAINEDWIDTH)
        ? NS_UNCONSTRAINEDSIZE
        : mContentArea.width;
    }
  }
  else {
    // The frame is clueless about the space manager and therefore we
    // only give it free space. An example is a table frame - the
    // tables do not flow around floats.
    aResult.x = mAvailSpaceRect.x + borderPadding.left;
    aResult.width = mAvailSpaceRect.width;
  }

#ifdef REALLY_NOISY_REFLOW
  printf("  CBAS: result %d %d %d %d\n", aResult.x, aResult.y, aResult.width, aResult.height);
#endif
}

void
nsBlockReflowState::GetAvailableSpace(nscoord aY)
{
#ifdef DEBUG
  // Verify that the caller setup the coordinate system properly
  nscoord wx, wy;
  mSpaceManager->GetTranslation(wx, wy);
  NS_ASSERTION((wx == mSpaceManagerX) && (wy == mSpaceManagerY),
               "bad coord system");
#endif

  mBand.GetAvailableSpace(aY - BorderPadding().top, mAvailSpaceRect);

#ifdef DEBUG
  if (nsBlockFrame::gNoisyReflow) {
    nsFrame::IndentBy(stdout, nsBlockFrame::gNoiseIndent);
    printf("GetAvailableSpace: band=%d,%d,%d,%d count=%d\n",
           mAvailSpaceRect.x, mAvailSpaceRect.y,
           mAvailSpaceRect.width, mAvailSpaceRect.height,
           mBand.GetTrapezoidCount());
  }
#endif
}

PRBool
nsBlockReflowState::ClearPastFloats(PRUint8 aBreakType)
{
  nscoord saveY, deltaY;

  PRBool applyTopMargin = PR_FALSE;
  switch (aBreakType) {
  case NS_STYLE_CLEAR_LEFT:
  case NS_STYLE_CLEAR_RIGHT:
  case NS_STYLE_CLEAR_LEFT_AND_RIGHT:
    // Apply the previous margin before clearing
    saveY = mY + mPrevBottomMargin.get();
    ClearFloats(saveY, aBreakType);
#ifdef NOISY_FLOAT_CLEARING
    nsFrame::ListTag(stdout, mBlock);
    printf(": ClearPastFloats: mPrevBottomMargin=%d saveY=%d oldY=%d newY=%d deltaY=%d\n",
           mPrevBottomMargin, saveY, saveY - mPrevBottomMargin, mY,
           mY - saveY);
#endif

    // Determine how far we just moved. If we didn't move then there
    // was nothing to clear to don't mess with the normal margin
    // collapsing behavior. In either case we need to restore the Y
    // coordinate to what it was before the clear.
    deltaY = mY - saveY;
    if (0 != deltaY) {
      // Pretend that the distance we just moved is a previous
      // blocks bottom margin. Note that GetAvailableSpace has been
      // done so that the available space calculations will be done
      // after clearing the appropriate floats.
      //
      // What we are doing here is applying CSS2 section 9.5.2's
      // rules for clearing - "The top margin of the generated box
      // is increased enough that the top border edge is below the
      // bottom outer edge of the floating boxes..."
      //
      // What this will do is cause the top-margin of the block
      // frame we are about to reflow to be collapsed with that
      // distance.
      
      // XXXldb This doesn't handle collapsing with negative margins
      // correctly, although it's arguable what "correct" is.

      // XXX Are all the other margins included by this point?
      mPrevBottomMargin.Zero();
      mPrevBottomMargin.Include(deltaY);
      mY = saveY;

      // Force margin to be applied in this circumstance
      applyTopMargin = PR_TRUE;
    }
    else {
      // Put mY back to its original value since no clearing
      // happened. That way the previous blocks bottom margin will
      // be applied properly.
      mY = saveY - mPrevBottomMargin.get();
    }
    break;
  }
  return applyTopMargin;
}

/*
 * Reconstruct the vertical margin before the line |aLine| in order to
 * do an incremental reflow that begins with |aLine| without reflowing
 * the line before it.  |aLine| may point to the fencepost at the end of
 * the line list, and it is used this way since we (for now, anyway)
 * always need to recover margins at the end of a block.
 *
 * The reconstruction involves walking backward through the line list to
 * find any collapsed margins preceding the line that would have been in
 * the reflow state's |mPrevBottomMargin| when we reflowed that line in
 * a full reflow (under the rule in CSS2 that all adjacent vertical
 * margins of blocks collapse).
 */
void
nsBlockReflowState::ReconstructMarginAbove(nsLineList::iterator aLine)
{
  mPrevBottomMargin.Zero();
  nsBlockFrame *block = mBlock;

  nsLineList::iterator firstLine = block->begin_lines();
  for (;;) {
    --aLine;
    if (aLine->IsBlock()) {
      mPrevBottomMargin = aLine->GetCarriedOutBottomMargin();
      break;
    }
    if (!aLine->IsEmpty()) {
      break;
    }
    if (aLine == firstLine) {
      // If the top margin was carried out (and thus already applied),
      // set it to zero.  Either way, we're done.
      if ((0 == mReflowState.mComputedBorderPadding.top) &&
          !(block->mState & NS_BLOCK_MARGIN_ROOT)) {
        mPrevBottomMargin.Zero();
      }
      break;
    }
  }
}

/**
 * Restore information about floats into the space manager for an
 * incremental reflow, and simultaneously push the floats by
 * |aDeltaY|, which is the amount |aLine| was pushed relative to its
 * parent.  The recovery of state is one of the things that makes
 * incremental reflow O(N^2) and this state should really be kept
 * around, attached to the frame tree.
 */
void
nsBlockReflowState::RecoverFloats(nsLineList::iterator aLine,
                                  nscoord aDeltaY)
{
  if (aLine->HasFloats()) {
    // Place the floats into the space-manager again. Also slide
    // them, just like the regular frames on the line.
    nsFloatCache* fc = aLine->GetFirstFloat();
    while (fc) {
      nsIFrame* floatFrame = fc->mPlaceholder->GetOutOfFlowFrame();
      if (aDeltaY != 0) {
        fc->mRegion.y += aDeltaY;
        fc->mCombinedArea.y += aDeltaY;
        nsPoint p = floatFrame->GetPosition();
        floatFrame->SetPosition(nsPoint(p.x, p.y + aDeltaY));
      }
#ifdef DEBUG
      if (nsBlockFrame::gNoisyReflow || nsBlockFrame::gNoisySpaceManager) {
        nscoord tx, ty;
        mSpaceManager->GetTranslation(tx, ty);
        nsFrame::IndentBy(stdout, nsBlockFrame::gNoiseIndent);
        printf("RecoverFloats: txy=%d,%d (%d,%d) ",
               tx, ty, mSpaceManagerX, mSpaceManagerY);
        nsFrame::ListTag(stdout, floatFrame);
        printf(" aDeltaY=%d region={%d,%d,%d,%d}\n",
               aDeltaY, fc->mRegion.x, fc->mRegion.y,
               fc->mRegion.width, fc->mRegion.height);
      }
#endif
      mSpaceManager->AddRectRegion(floatFrame, fc->mRegion);
      fc = fc->Next();
    }
  } else if (aLine->IsBlock()) {
    nsBlockFrame *kid = nsnull;
    aLine->mFirstChild->QueryInterface(kBlockFrameCID, (void**)&kid);
    // don't recover any state inside a block that has its own space
    // manager (we don't currently have any blocks like this, though,
    // thanks to our use of extra frames for 'overflow')
    if (kid && !(kid->GetStateBits() & NS_BLOCK_SPACE_MGR)) {
      nscoord tx = kid->mRect.x, ty = kid->mRect.y;

      // If the element is relatively positioned, then adjust x and y
      // accordingly so that we consider relatively positioned frames
      // at their original position.
      if (NS_STYLE_POSITION_RELATIVE == kid->GetStyleDisplay()->mPosition) {
        nsPoint *offsets = NS_STATIC_CAST(nsPoint*,
          mPresContext->PropertyTable()->GetProperty(kid,
                                       nsLayoutAtoms::computedOffsetProperty));

        if (offsets) {
          tx -= offsets->x;
          ty -= offsets->y;
        }
      }
 
      mSpaceManager->Translate(tx, ty);
      for (nsBlockFrame::line_iterator line = kid->begin_lines(),
                                   line_end = kid->end_lines();
           line != line_end;
           ++line)
        // Pass 0, not the real DeltaY, since these floats aren't
        // moving relative to their parent block, only relative to
        // the space manager.
        RecoverFloats(line, 0);
      mSpaceManager->Translate(-tx, -ty);
    }
  }
}

/**
 * Everything done in this function is done O(N) times for each pass of
 * reflow so it is O(N*M) where M is the number of incremental reflow
 * passes.  That's bad.  Don't do stuff here.
 *
 * When this function is called, |aLine| has just been slid by |aDeltaY|
 * and the purpose of RecoverStateFrom is to ensure that the
 * nsBlockReflowState is in the same state that it would have been in
 * had the line just been reflowed.
 *
 * Most of the state recovery that we have to do involves floats.
 */
void
nsBlockReflowState::RecoverStateFrom(nsLineList::iterator aLine,
                                     nscoord aDeltaY)
{
  // Make the line being recovered the current line
  mCurrentLine = aLine;

  // Recover mKidXMost and mMaxElementWidth
  nscoord xmost = aLine->mBounds.XMost();
  if (xmost > mKidXMost) {
#ifdef DEBUG
    if (CRAZY_WIDTH(xmost)) {
      nsFrame::ListTag(stdout, mBlock);
      printf(": WARNING: xmost:%d\n", xmost);
    }
#endif
#ifdef NOISY_KIDXMOST
    printf("%p RecoverState block %p aState.mKidXMost=%d\n", this, mBlock, xmost); 
#endif
    mKidXMost = xmost;
  }
  if (GetFlag(BRS_COMPUTEMAXELEMENTWIDTH)) {
#ifdef DEBUG
    if (nsBlockFrame::gNoisyMaxElementWidth) {
      nsFrame::IndentBy(stdout, nsBlockFrame::gNoiseIndent);
      printf("nsBlockReflowState::RecoverStateFrom block %p caching max width %d\n", mBlock, aLine->mMaxElementWidth);
    }
#endif
    UpdateMaxElementWidth(aLine->mMaxElementWidth);

    // Recover the float MEWs for floats in this line (but not in
    // blocks within it, since their MEWs are already part of the block's
    // MEW).
    if (aLine->HasFloats()) {
      for (nsFloatCache* fc = aLine->GetFirstFloat(); fc; fc = fc->Next())
        UpdateMaxElementWidth(fc->mMaxElementWidth);
    }
  }

  // If computing the maximum width, then update mMaximumWidth
  if (GetFlag(BRS_COMPUTEMAXWIDTH)) {
#ifdef NOISY_MAXIMUM_WIDTH
    printf("nsBlockReflowState::RecoverStateFrom block %p caching max width %d\n", mBlock, aLine->mMaximumWidth);
#endif
    UpdateMaximumWidth(aLine->mMaximumWidth);
  }

  // Place floats for this line into the space manager
  if (aLine->HasFloats() || aLine->IsBlock()) {
    // Undo border/padding translation since the nsFloatCache's
    // coordinates are relative to the frame not relative to the
    // border/padding.
    const nsMargin& bp = BorderPadding();
    mSpaceManager->Translate(-bp.left, -bp.top);

    RecoverFloats(aLine, aDeltaY);

#ifdef DEBUG
    if (nsBlockFrame::gNoisyReflow || nsBlockFrame::gNoisySpaceManager) {
      mSpaceManager->List(stdout);
    }
#endif
    // And then put the translation back again
    mSpaceManager->Translate(bp.left, bp.top);
  }
}

PRBool
nsBlockReflowState::IsImpactedByFloat() const
{
#ifdef REALLY_NOISY_REFLOW
  printf("nsBlockReflowState::IsImpactedByFloat %p returned %d\n", 
         this, mBand.GetFloatCount());
#endif
  return mBand.GetFloatCount() > 0;
}


void
nsBlockReflowState::InitFloat(nsLineLayout&       aLineLayout,
                              nsPlaceholderFrame* aPlaceholder,
                              nsReflowStatus&     aReflowStatus)
{
  // Set the geometric parent of the float
  nsIFrame* floatFrame = aPlaceholder->GetOutOfFlowFrame();
  floatFrame->SetParent(mBlock);

  // Then add the float to the current line and place it when
  // appropriate
  AddFloat(aLineLayout, aPlaceholder, PR_TRUE, aReflowStatus);
}

// This is called by the line layout's AddFloat method when a
// place-holder frame is reflowed in a line. If the float is a
// left-most child (it's x coordinate is at the line's left margin)
// then the float is place immediately, otherwise the float
// placement is deferred until the line has been reflowed.

// XXXldb This behavior doesn't quite fit with CSS1 and CSS2 --
// technically we're supposed let the current line flow around the
// float as well unless it won't fit next to what we already have.
// But nobody else implements it that way...
void
nsBlockReflowState::AddFloat(nsLineLayout&       aLineLayout,
                             nsPlaceholderFrame* aPlaceholder,
                             PRBool              aInitialReflow,
                             nsReflowStatus&     aReflowStatus)
{
  NS_PRECONDITION(mBlock->end_lines() != mCurrentLine, "null ptr");

  aReflowStatus = NS_FRAME_COMPLETE;
  // Allocate a nsFloatCache for the float
  nsFloatCache* fc = mFloatCacheFreeList.Alloc();
  fc->mPlaceholder = aPlaceholder;
  fc->mIsCurrentLineFloat = aLineLayout.CanPlaceFloatNow();
  fc->mMaxElementWidth = 0;

  // Now place the float immediately if possible. Otherwise stash it
  // away in mPendingFloats and place it later.
  if (fc->mIsCurrentLineFloat) {
    // Record this float in the current-line list
    mCurrentLineFloats.Append(fc);

    // Because we are in the middle of reflowing a placeholder frame
    // within a line (and possibly nested in an inline frame or two
    // that's a child of our block) we need to restore the space
    // manager's translation to the space that the block resides in
    // before placing the float.
    nscoord ox, oy;
    mSpaceManager->GetTranslation(ox, oy);
    nscoord dx = ox - mSpaceManagerX;
    nscoord dy = oy - mSpaceManagerY;
    mSpaceManager->Translate(-dx, -dy);

    // And then place it
    PRBool isLeftFloat;
    FlowAndPlaceFloat(fc, &isLeftFloat, aReflowStatus);    

    // Pass on updated available space to the current inline reflow engine
    GetAvailableSpace();
    aLineLayout.UpdateBand(mAvailSpaceRect.x + BorderPadding().left, mY,
                           GetFlag(BRS_UNCONSTRAINEDWIDTH) ? NS_UNCONSTRAINEDSIZE : mAvailSpaceRect.width,
                           mAvailSpaceRect.height,
                           isLeftFloat,
                           aPlaceholder->GetOutOfFlowFrame());

    // Restore coordinate system
    mSpaceManager->Translate(dx, dy);
  }
  else {
    // This float will be placed after the line is done (it is a
    // below-current-line float).
    mBelowCurrentLineFloats.Append(fc);
  }
}

void
nsBlockReflowState::UpdateMaxElementWidth(nscoord aMaxElementWidth)
{
#ifdef DEBUG
  nscoord oldWidth = mMaxElementWidth;
#endif
  if (aMaxElementWidth > mMaxElementWidth) {
    mMaxElementWidth = aMaxElementWidth;
  }
#ifdef DEBUG
  if (nsBlockFrame::gNoisyMaxElementWidth) {
    if (mMaxElementWidth != oldWidth) {
      nsFrame::IndentBy(stdout, nsBlockFrame::gNoiseIndent);
      if (NS_UNCONSTRAINEDSIZE == mReflowState.availableWidth) {
        printf("PASS1 ");
      }
      nsFrame::ListTag(stdout, mBlock);
      printf(": old max-element-width=%d new=%d\n",
             oldWidth, mMaxElementWidth);
    }
  }
#endif
}

void
nsBlockReflowState::UpdateMaximumWidth(nscoord aMaximumWidth)
{
  if (aMaximumWidth > mMaximumWidth) {
#ifdef NOISY_MAXIMUM_WIDTH
    printf("nsBlockReflowState::UpdateMaximumWidth block %p caching max width %d\n", mBlock, aMaximumWidth);
#endif
    mMaximumWidth = aMaximumWidth;
  }
}

PRBool
nsBlockReflowState::CanPlaceFloat(const nsRect& aFloatRect,
                                  PRUint8 aFloats)
{
  // If the current Y coordinate is not impacted by any floats
  // then by definition the float fits.
  PRBool result = PR_TRUE;
  if (0 != mBand.GetFloatCount()) {
    // XXX We should allow overflow by up to half a pixel here (bug 21193).
    if (mAvailSpaceRect.width < aFloatRect.width) {
      // The available width is too narrow (and its been impacted by a
      // prior float)
      result = PR_FALSE;
    }
    else {
      // At this point we know that there is enough horizontal space for
      // the float (somewhere). Lets see if there is enough vertical
      // space.
      if (mAvailSpaceRect.height < aFloatRect.height) {
        // The available height is too short. However, its possible that
        // there is enough open space below which is not impacted by a
        // float.
        //
        // Compute the X coordinate for the float based on its float
        // type, assuming its placed on the current line. This is
        // where the float will be placed horizontally if it can go
        // here.
        nscoord xa;
        if (NS_STYLE_FLOAT_LEFT == aFloats) {
          xa = mAvailSpaceRect.x;
        }
        else {
          xa = mAvailSpaceRect.XMost() - aFloatRect.width;

          // In case the float is too big, don't go past the left edge
          // XXXldb This seems wrong, but we might want to fix bug 6976
          // first.
          if (xa < mAvailSpaceRect.x) {
            xa = mAvailSpaceRect.x;
          }
        }
        nscoord xb = xa + aFloatRect.width;

        // Calculate the top and bottom y coordinates, again assuming
        // that the float is placed on the current line.
        const nsMargin& borderPadding = BorderPadding();
        nscoord ya = mY - borderPadding.top;
        if (ya < 0) {
          // CSS2 spec, 9.5.1 rule [4]: "A floating box's outer top may not
          // be higher than the top of its containing block."  (Since the
          // containing block is the content edge of the block box, this
          // means the margin edge of the float can't be higher than the
          // content edge of the block that contains it.)
          ya = 0;
        }
        nscoord yb = ya + aFloatRect.height;

        nscoord saveY = mY;
        for (;;) {
          // Get the available space at the new Y coordinate
          mY += mAvailSpaceRect.height;
          GetAvailableSpace();

          if (0 == mBand.GetFloatCount()) {
            // Winner. This band has no floats on it, therefore
            // there can be no overlap.
            break;
          }

          // Check and make sure the float won't intersect any
          // floats on this band. The floats starting and ending
          // coordinates must be entirely in the available space.
          if ((xa < mAvailSpaceRect.x) || (xb > mAvailSpaceRect.XMost())) {
            // The float can't go here.
            result = PR_FALSE;
            break;
          }

          // See if there is now enough height for the float.
          if (yb < mY + mAvailSpaceRect.height) {
            // Winner. The bottom Y coordinate of the float is in
            // this band.
            break;
          }
        }

        // Restore Y coordinate and available space information
        // regardless of the outcome.
        mY = saveY;
        GetAvailableSpace();
      }
    }
  }
  return result;
}

void
nsBlockReflowState::FlowAndPlaceFloat(nsFloatCache*   aFloatCache,
                                      PRBool*         aIsLeftFloat,
                                      nsReflowStatus& aReflowStatus)
{
  aReflowStatus = NS_FRAME_COMPLETE;
  // Save away the Y coordinate before placing the float. We will
  // restore mY at the end after placing the float. This is
  // necessary because any adjustments to mY during the float
  // placement are for the float only, not for any non-floating
  // content.
  nscoord saveY = mY;

  nsPlaceholderFrame* placeholder = aFloatCache->mPlaceholder;
  nsIFrame*           floatFrame = placeholder->GetOutOfFlowFrame();

  // Grab the float's display information
  const nsStyleDisplay* floatDisplay = floatFrame->GetStyleDisplay();

  // This will hold the float's geometry when we've found a place
  // for it to live.
  nsRect region;

  // The float's old region, so we can propagate damage.
  nsRect oldRegion = floatFrame->GetRect();
  oldRegion.Inflate(aFloatCache->mMargins);

  // Enforce CSS2 9.5.1 rule [2], i.e., make sure that a float isn't
  // ``above'' another float that preceded it in the flow.
  mY = NS_MAX(mSpaceManager->GetLowestRegionTop() + BorderPadding().top, mY);

  // See if the float should clear any preceeding floats...
  if (NS_STYLE_CLEAR_NONE != floatDisplay->mBreakType) {
    // XXXldb Does this handle vertical margins correctly?
    ClearFloats(mY, floatDisplay->mBreakType);
  }
  else {
    // Get the band of available space
    GetAvailableSpace();
  }

  NS_ASSERTION(floatFrame->GetParent() == mBlock,
               "Float frame has wrong parent");

  // Reflow the float
  mBlock->ReflowFloat(*this, placeholder, aFloatCache, aReflowStatus);

  // Get the floats bounding box and margin information
  region = floatFrame->GetRect();

#ifdef DEBUG
  if (nsBlockFrame::gNoisyReflow) {
    nsFrame::IndentBy(stdout, nsBlockFrame::gNoiseIndent);
    printf("flowed float: ");
    nsFrame::ListTag(stdout, floatFrame);
    printf(" (%d,%d,%d,%d)\n",
	   region.x, region.y, region.width, region.height);
  }
#endif

  // Adjust the float size by its margin. That's the area that will
  // impact the space manager.
  region.width += aFloatCache->mMargins.left + aFloatCache->mMargins.right;
  region.height += aFloatCache->mMargins.top + aFloatCache->mMargins.bottom;

  // Find a place to place the float. The CSS2 spec doesn't want
  // floats overlapping each other or sticking out of the containing
  // block if possible (CSS2 spec section 9.5.1, see the rule list).
  NS_ASSERTION((NS_STYLE_FLOAT_LEFT == floatDisplay->mFloats) ||
	       (NS_STYLE_FLOAT_RIGHT == floatDisplay->mFloats),
	       "invalid float type");

  // Can the float fit here?
  PRBool keepFloatOnSameLine = PR_FALSE;

  while (! CanPlaceFloat(region, floatDisplay->mFloats)) {
    // Nope. try to advance to the next band.
    if (NS_STYLE_DISPLAY_TABLE != floatDisplay->mDisplay ||
          eCompatibility_NavQuirks != mPresContext->CompatibilityMode() ) {

      mY += mAvailSpaceRect.height;
      GetAvailableSpace();
    } else {
      // This quirk matches the one in nsBlockFrame::ReflowFloat
      // IE handles float tables in a very special way

      // see if the previous float is also a table and has "align"
      nsFloatCache* fc = mCurrentLineFloats.Head();
      nsIFrame* prevFrame = nsnull;
      while (fc) {
        if (fc->mPlaceholder->GetOutOfFlowFrame() == floatFrame) {
          break;
        }
        prevFrame = fc->mPlaceholder->GetOutOfFlowFrame();
        fc = fc->Next();
      }
      
      if(prevFrame) {
        //get the frame type
        if (nsLayoutAtoms::tableOuterFrame == prevFrame->GetType()) {
          //see if it has "align="
          // IE makes a difference between align and he float property
          nsIContent* content = prevFrame->GetContent();
          if (content) {
            nsAutoString value;
            if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttr(kNameSpaceID_None, nsHTMLAtoms::align, value)) {
              // we're interested only if previous frame is align=left
              // IE messes things up when "right" (overlapping frames) 
              if (value.LowerCaseEqualsLiteral("left")) {
                keepFloatOnSameLine = PR_TRUE;
                // don't advance to next line (IE quirkie behaviour)
                // it breaks rule CSS2/9.5.1/1, but what the hell
                // since we cannot evangelize the world
                break;
              }
            }
          }
        }
      }

      // the table does not fit anymore in this line so advance to next band 
      mY += mAvailSpaceRect.height;
      GetAvailableSpace();
      // reflow the float again now since we have more space
      mBlock->ReflowFloat(*this, placeholder, aFloatCache, aReflowStatus);
      // Get the floats bounding box and margin information
      region = floatFrame->GetRect();
      // Adjust the float size by its margin. That's the area that will
      // impact the space manager.
      region.width += aFloatCache->mMargins.left + aFloatCache->mMargins.right;
      region.height += aFloatCache->mMargins.top + aFloatCache->mMargins.bottom;

    }
  }
  // If the float is continued, it will get the same absolute x value as its prev-in-flow
  nsRect prevRect(0,0,0,0);
  nsIFrame* prevInFlow;
  floatFrame->GetPrevInFlow(&prevInFlow);
  if (prevInFlow) {
    prevRect = prevInFlow->GetRect();

    nsIFrame *placeParentPrev, *prevPlace;
    // If prevInFlow's placeholder is in a block that wasn't continued, we need to adjust 
    // prevRect.x to account for the missing frame offsets.
    nsIFrame* placeParent = placeholder->GetParent();
    placeParent->GetPrevInFlow(&placeParentPrev);
    prevPlace =
      mPresContext->FrameManager()->GetPlaceholderFrameFor(prevInFlow);

    nsIFrame* prevPlaceParent = prevPlace->GetParent();

    for (nsIFrame* ancestor = prevPlaceParent; 
         ancestor && (ancestor != placeParentPrev); 
         ancestor = ancestor->GetParent()) {
      prevRect.x += ancestor->GetRect().x;
    }        
  }
  // Assign an x and y coordinate to the float. Note that the x,y
  // coordinates are computed <b>relative to the translation in the
  // spacemanager</b> which means that the impacted region will be
  // <b>inside</b> the border/padding area.
  PRBool isLeftFloat;
  if (NS_STYLE_FLOAT_LEFT == floatDisplay->mFloats) {
    isLeftFloat = PR_TRUE;
    region.x = (prevInFlow) ? prevRect.x : mAvailSpaceRect.x;
  }
  else {
    isLeftFloat = PR_FALSE;
    if (NS_UNCONSTRAINEDSIZE != mAvailSpaceRect.width) {
      if (prevInFlow) {
        region.x = prevRect.x;
      }
      else if (!keepFloatOnSameLine) {
        region.x = mAvailSpaceRect.XMost() - region.width;
      } 
      else {
        // this is the IE quirk (see few lines above)
        // the table is keept in the same line: don't let it overlap the previous float 
        region.x = mAvailSpaceRect.x;
      }
    }
    else {
      // For unconstrained reflows, pretend that a right float is
      // instead a left float.  This will make us end up with the
      // correct unconstrained width, and we'll place it later.
      region.x = mAvailSpaceRect.x;
    }
  }
  *aIsLeftFloat = isLeftFloat;
  const nsMargin& borderPadding = BorderPadding();
  region.y = mY - borderPadding.top;
  if (region.y < 0) {
    // CSS2 spec, 9.5.1 rule [4]: "A floating box's outer top may not
    // be higher than the top of its containing block."  (Since the
    // containing block is the content edge of the block box, this
    // means the margin edge of the float can't be higher than the
    // content edge of the block that contains it.)
    region.y = 0;
  }

  // Place the float in the space manager
  // if the float split, then take up all of the vertical height 
  if (NS_FRAME_IS_NOT_COMPLETE(aReflowStatus) && 
      (NS_UNCONSTRAINEDSIZE != mContentArea.height)) {
    region.height = PR_MAX(region.height, mContentArea.height);
  }
#ifdef DEBUG
  nsresult rv =
#endif
  mSpaceManager->AddRectRegion(floatFrame, region);
  NS_ABORT_IF_FALSE(NS_SUCCEEDED(rv), "bad float placement");

  // If the float's dimensions have changed, note the damage in the
  // space manager.
  if (region != oldRegion) {
    // XXXwaterson conservative: we could probably get away with noting
    // less damage; e.g., if only height has changed, then only note the
    // area into which the float has grown or from which the float has
    // shrunk.
    nscoord top = NS_MIN(region.y, oldRegion.y);
    nscoord bottom = NS_MAX(region.YMost(), oldRegion.YMost());
    mSpaceManager->IncludeInDamage(top, bottom);
  }

  // Save away the floats region in the spacemanager, after making
  // it relative to the containing block's frame instead of relative
  // to the spacemanager translation (which is inset by the
  // border+padding).
  aFloatCache->mRegion.x = region.x + borderPadding.left;
  aFloatCache->mRegion.y = region.y + borderPadding.top;
  aFloatCache->mRegion.width = region.width;
  aFloatCache->mRegion.height = region.height;
#ifdef NOISY_SPACEMANAGER
  nscoord tx, ty;
  mSpaceManager->GetTranslation(tx, ty);
  nsFrame::ListTag(stdout, mBlock);
  printf(": FlowAndPlaceFloat: AddRectRegion: txy=%d,%d (%d,%d) {%d,%d,%d,%d}\n",
         tx, ty, mSpaceManagerX, mSpaceManagerY,
         aFloatCache->mRegion.x, aFloatCache->mRegion.y,
         aFloatCache->mRegion.width, aFloatCache->mRegion.height);
#endif

  // Set the origin of the float frame, in frame coordinates. These
  // coordinates are <b>not</b> relative to the spacemanager
  // translation, therefore we have to factor in our border/padding.
  nscoord x = borderPadding.left + aFloatCache->mMargins.left + region.x;
  nscoord y = borderPadding.top + aFloatCache->mMargins.top + region.y;

  // If float is relatively positioned, factor that in as well
  // XXXldb Should this be done after handling the combined area
  // below?
  if (NS_STYLE_POSITION_RELATIVE == floatDisplay->mPosition) {
    x += aFloatCache->mOffsets.left;
    y += aFloatCache->mOffsets.top;
  }

  // Position the float and make sure and views are properly
  // positioned. We need to explicitly position its child views as
  // well, since we're moving the float after flowing it.
  floatFrame->SetPosition(nsPoint(x, y));
  nsContainerFrame::PositionFrameView(mPresContext, floatFrame);
  nsContainerFrame::PositionChildViews(mPresContext, floatFrame);

  // Update the float combined area state
  nsRect combinedArea = aFloatCache->mCombinedArea;
  combinedArea.x += x;
  combinedArea.y += y;
  // When we are placing a right float in an unconstrained situation or
  // when shrink wrapping, we don't apply it to the float combined area
  // immediately, since there's no need to since we're guaranteed another
  // reflow, and since there's no need to change the code that was
  // necessary back when the float was positioned relative to
  // NS_UNCONSTRAINEDSIZE.
  if (isLeftFloat ||
      !GetFlag(BRS_UNCONSTRAINEDWIDTH) ||
      !GetFlag(BRS_SHRINKWRAPWIDTH)) {
    mFloatCombinedArea.UnionRect(combinedArea, mFloatCombinedArea);
  } else if (GetFlag(BRS_SHRINKWRAPWIDTH)) {
    // Mark the line dirty so we come back and re-place the float once
    // the shrink wrap width is determined
    mCurrentLine->MarkDirty();
    SetFlag(BRS_NEEDRESIZEREFLOW, PR_TRUE);
  }

  // Now restore mY
  mY = saveY;

#ifdef DEBUG
  if (nsBlockFrame::gNoisyReflow) {
    nsRect r = floatFrame->GetRect();
    nsFrame::IndentBy(stdout, nsBlockFrame::gNoiseIndent);
    printf("placed float: ");
    nsFrame::ListTag(stdout, floatFrame);
    printf(" %d,%d,%d,%d\n", r.x, r.y, r.width, r.height);
  }
#endif
}

/**
 * Place below-current-line floats.
 */
PRBool
nsBlockReflowState::PlaceBelowCurrentLineFloats(nsFloatCacheList& aList)
{
  nsFloatCache* fc = aList.Head();
  while (fc) {
    NS_ASSERTION(!fc->mIsCurrentLineFloat,
                 "A cl float crept into the bcl float list.");
    if (!fc->mIsCurrentLineFloat) {
#ifdef DEBUG
      if (nsBlockFrame::gNoisyReflow) {
        nsFrame::IndentBy(stdout, nsBlockFrame::gNoiseIndent);
        printf("placing bcl float: ");
        nsFrame::ListTag(stdout, fc->mPlaceholder->GetOutOfFlowFrame());
        printf("\n");
      }
#endif

      // Place the float
      PRBool isLeftFloat;
      nsReflowStatus reflowStatus;
      FlowAndPlaceFloat(fc, &isLeftFloat, reflowStatus);

      if (NS_FRAME_IS_TRUNCATED(reflowStatus)) {
        // return before processing all of the floats, since the line will be pushed.
        return PR_FALSE;
      }
      else if (NS_FRAME_IS_NOT_COMPLETE(reflowStatus)) {
        // Create a continuation for the incomplete float and its placeholder.
        nsresult rv = mBlock->SplitPlaceholder(*mPresContext, *fc->mPlaceholder);
        if (NS_FAILED(rv)) 
          return PR_FALSE;
      }
    }
    fc = fc->Next();
  }
  return PR_TRUE;
}

void
nsBlockReflowState::ClearFloats(nscoord aY, PRUint8 aBreakType)
{
#ifdef DEBUG
  if (nsBlockFrame::gNoisyReflow) {
    nsFrame::IndentBy(stdout, nsBlockFrame::gNoiseIndent);
    printf("clear floats: in: mY=%d aY=%d(%d)\n",
           mY, aY, aY - BorderPadding().top);
  }
#endif

#ifdef NOISY_FLOAT_CLEARING
  printf("nsBlockReflowState::ClearFloats: aY=%d breakType=%d\n",
         aY, aBreakType);
  mSpaceManager->List(stdout);
#endif
  
  const nsMargin& bp = BorderPadding();
  nscoord newY = mSpaceManager->ClearFloats(aY - bp.top, aBreakType);
  mY = newY + bp.top;
  GetAvailableSpace();

#ifdef DEBUG
  if (nsBlockFrame::gNoisyReflow) {
    nsFrame::IndentBy(stdout, nsBlockFrame::gNoiseIndent);
    printf("clear floats: out: mY=%d(%d)\n", mY, mY - bp.top);
  }
#endif
}

