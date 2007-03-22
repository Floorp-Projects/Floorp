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

/* state used in reflow of block frames */

#include "nsBlockReflowContext.h"
#include "nsBlockReflowState.h"
#include "nsBlockFrame.h"
#include "nsLineLayout.h"
#include "nsPresContext.h"
#include "nsGkAtoms.h"
#include "nsIFrame.h"
#include "nsFrameManager.h"

#include "nsINameSpaceManager.h"


#ifdef DEBUG
#include "nsBlockDebugFlags.h"
#endif

nsBlockReflowState::nsBlockReflowState(const nsHTMLReflowState& aReflowState,
                                       nsPresContext* aPresContext,
                                       nsBlockFrame* aFrame,
                                       const nsHTMLReflowMetrics& aMetrics,
                                       PRBool aTopMarginRoot,
                                       PRBool aBottomMarginRoot,
                                       PRBool aBlockNeedsSpaceManager)
  : mBlock(aFrame),
    mPresContext(aPresContext),
    mReflowState(aReflowState),
    mPrevBottomMargin(),
    mLineNumber(0),
    mFlags(0),
    mFloatBreakType(NS_STYLE_CLEAR_NONE)
{
  SetFlag(BRS_ISFIRSTINFLOW, aFrame->GetPrevInFlow() == nsnull);

  const nsMargin& borderPadding = BorderPadding();

  if (aTopMarginRoot || 0 != aReflowState.mComputedBorderPadding.top) {
    SetFlag(BRS_ISTOPMARGINROOT, PR_TRUE);
  }
  if (aBottomMarginRoot || 0 != aReflowState.mComputedBorderPadding.bottom) {
    SetFlag(BRS_ISBOTTOMMARGINROOT, PR_TRUE);
  }
  if (GetFlag(BRS_ISTOPMARGINROOT)) {
    SetFlag(BRS_APPLYTOPMARGIN, PR_TRUE);
  }
  if (aBlockNeedsSpaceManager) {
    SetFlag(BRS_SPACE_MGR, PR_TRUE);
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
  mNextInFlow = NS_STATIC_CAST(nsBlockFrame*, mBlock->GetNextInFlow());

  NS_ASSERTION(NS_UNCONSTRAINEDSIZE != aReflowState.ComputedWidth(),
               "no unconstrained widths should be present anymore");
  mContentArea.width = aReflowState.ComputedWidth();

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

  mMinLineHeight = nsHTMLReflowState::CalcLineHeight(mPresContext,
                                                     aReflowState.rendContext,
                                                     aReflowState.frame);
}

void
nsBlockReflowState::SetupOverflowPlaceholdersProperty()
{
  if (mReflowState.availableHeight != NS_UNCONSTRAINEDSIZE ||
      !mOverflowPlaceholders.IsEmpty()) {
    mBlock->SetProperty(nsGkAtoms::overflowPlaceholdersProperty,
                        &mOverflowPlaceholders, nsnull);
    mBlock->AddStateBits(NS_BLOCK_HAS_OVERFLOW_PLACEHOLDERS);
  }
}

nsBlockReflowState::~nsBlockReflowState()
{
  NS_ASSERTION(mOverflowPlaceholders.IsEmpty(),
               "Leaking overflow placeholder frames");

  // Restore the coordinate system, unless the space manager is null,
  // which means it was just destroyed.
  if (mSpaceManager) {
    const nsMargin& borderPadding = BorderPadding();
    mSpaceManager->Translate(-borderPadding.left, -borderPadding.top);
  }

  if (mBlock->GetStateBits() & NS_BLOCK_HAS_OVERFLOW_PLACEHOLDERS) {
    mBlock->UnsetProperty(nsGkAtoms::overflowPlaceholdersProperty);
    mBlock->RemoveStateBits(NS_BLOCK_HAS_OVERFLOW_PLACEHOLDERS);
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

  // text controls are not splittable
  // XXXldb Why not just set the frame state bit?

  nsSplittableType splitType = aFrame->GetSplittableType();
  if ((NS_FRAME_SPLITTABLE_NON_RECTANGULAR == splitType ||     // normal blocks 
       NS_FRAME_NOT_SPLITTABLE == splitType) &&                // things like images mapped to display: block
      !(aFrame->IsFrameOfType(nsIFrame::eReplaced)) &&         // but not replaced elements
      aFrame->GetType() != nsGkAtoms::scrollFrame)         // or scroll frames
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
          aResult.width = mContentArea.width;
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
              m += borderStyle->GetBorder();
            }

            // determine left edge
            if (mBand.GetLeftFloatCount()) {
              aResult.x = mAvailSpaceRect.x + borderPadding.left - m.left;
            }
            else {
              aResult.x = borderPadding.left;
            }

            // determine width
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
      aResult.width = mContentArea.width;
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
nsBlockReflowState::GetAvailableSpace(nscoord aY, PRBool aRelaxHeightConstraint)
{
#ifdef DEBUG
  // Verify that the caller setup the coordinate system properly
  nscoord wx, wy;
  mSpaceManager->GetTranslation(wx, wy);
  NS_ASSERTION((wx == mSpaceManagerX) && (wy == mSpaceManagerY),
               "bad coord system");
#endif

  mBand.GetAvailableSpace(aY - BorderPadding().top, aRelaxHeightConstraint,
                          mAvailSpaceRect);

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
      if (!GetFlag(BRS_ISTOPMARGINROOT)) {
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
    if (kid && !nsBlockFrame::BlockNeedsSpaceManager(kid)) {
      nscoord tx = kid->mRect.x, ty = kid->mRect.y;

      // If the element is relatively positioned, then adjust x and y
      // accordingly so that we consider relatively positioned frames
      // at their original position.
      if (NS_STYLE_POSITION_RELATIVE == kid->GetStyleDisplay()->mPosition) {
        nsPoint *offsets = NS_STATIC_CAST(nsPoint*,
          mPresContext->PropertyTable()->GetProperty(kid,
                                       nsGkAtoms::computedOffsetProperty));

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


PRBool
nsBlockReflowState::InitFloat(nsLineLayout&       aLineLayout,
                              nsPlaceholderFrame* aPlaceholder,
                              nsReflowStatus&     aReflowStatus)
{
  // Set the geometric parent of the float
  nsIFrame* floatFrame = aPlaceholder->GetOutOfFlowFrame();
  floatFrame->SetParent(mBlock);

  // Then add the float to the current line and place it when
  // appropriate
  return AddFloat(aLineLayout, aPlaceholder, PR_TRUE, aReflowStatus);
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
PRBool
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

  PRBool placed;

  // Now place the float immediately if possible. Otherwise stash it
  // away in mPendingFloats and place it later.
  if (fc->mIsCurrentLineFloat) {
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
    // force it to fit if we're at the top of the block and we can't
    // break before this
    PRBool forceFit = IsAdjacentWithTop() && !aLineLayout.LineIsBreakable();
    placed = FlowAndPlaceFloat(fc, &isLeftFloat, aReflowStatus, forceFit);
    NS_ASSERTION(placed || !forceFit,
                 "If we asked for force-fit, it should have been placed");
    if (placed) {
      // Pass on updated available space to the current inline reflow engine
      GetAvailableSpace(mY, forceFit);
      aLineLayout.UpdateBand(mAvailSpaceRect.x + BorderPadding().left, mY,
                             mAvailSpaceRect.width,
                             mAvailSpaceRect.height,
                             isLeftFloat,
                             aPlaceholder->GetOutOfFlowFrame());
      
      // Record this float in the current-line list
      mCurrentLineFloats.Append(fc);
    }
    else {
      delete fc;
    }

    // Restore coordinate system
    mSpaceManager->Translate(dx, dy);
  }
  else {
    // This float will be placed after the line is done (it is a
    // below-current-line float).
    mBelowCurrentLineFloats.Append(fc);
    if (mReflowState.availableHeight != NS_UNCONSTRAINEDSIZE ||
        aPlaceholder->GetNextInFlow()) {
      // If the float might not be complete, mark it incomplete now to
      // prevent the placeholders being torn down. We will destroy any
      // placeholders later if PlaceBelowCurrentLineFloats finds the
      // float is complete.
      // Note that we could have unconstrained height and yet have
      // a next-in-flow placeholder --- for example columns can switch
      // from constrained height to unconstrained height.
      if (aPlaceholder->GetSplittableType() == NS_FRAME_NOT_SPLITTABLE) {
        placed = PR_FALSE;
      }
      else {
        placed = PR_TRUE;
        aReflowStatus = NS_FRAME_NOT_COMPLETE;
      }
    }
    else {
      placed = PR_TRUE;
    }
  }
  return placed;
}

PRBool
nsBlockReflowState::CanPlaceFloat(const nsSize& aFloatSize,
                                  PRUint8 aFloats, PRBool aForceFit)
{
  // If the current Y coordinate is not impacted by any floats
  // then by definition the float fits.
  PRBool result = PR_TRUE;
  if (0 != mBand.GetFloatCount()) {
    // XXX We should allow overflow by up to half a pixel here (bug 21193).
    if (mAvailSpaceRect.width < aFloatSize.width) {
      // The available width is too narrow (and its been impacted by a
      // prior float)
      result = PR_FALSE;
    }
    else {
      // At this point we know that there is enough horizontal space for
      // the float (somewhere). Lets see if there is enough vertical
      // space.
      if (mAvailSpaceRect.height < aFloatSize.height) {
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
          xa = mAvailSpaceRect.XMost() - aFloatSize.width;

          // In case the float is too big, don't go past the left edge
          // XXXldb This seems wrong, but we might want to fix bug 6976
          // first.
          if (xa < mAvailSpaceRect.x) {
            xa = mAvailSpaceRect.x;
          }
        }
        nscoord xb = xa + aFloatSize.width;

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
        nscoord yb = ya + aFloatSize.height;

        nscoord saveY = mY;
        for (;;) {
          // Get the available space at the new Y coordinate
          if (mAvailSpaceRect.height <= 0) {
            // there is no more available space. We lose.
            result = PR_FALSE;
            break;
          }

          mY += mAvailSpaceRect.height;
          GetAvailableSpace(mY, aForceFit);

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
        GetAvailableSpace(mY, aForceFit);
      }
    }
  }
  return result;
}

PRBool
nsBlockReflowState::FlowAndPlaceFloat(nsFloatCache*   aFloatCache,
                                      PRBool*         aIsLeftFloat,
                                      nsReflowStatus& aReflowStatus,
                                      PRBool          aForceFit)
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

  // The float's old region, so we can propagate damage.
  nsRect oldRegion = floatFrame->GetRect();
  oldRegion.Inflate(aFloatCache->mMargins);

  // Enforce CSS2 9.5.1 rule [2], i.e., make sure that a float isn't
  // ``above'' another float that preceded it in the flow.
  mY = NS_MAX(mSpaceManager->GetLowestRegionTop() + BorderPadding().top, mY);

  // See if the float should clear any preceding floats...
  if (NS_STYLE_CLEAR_NONE != floatDisplay->mBreakType) {
    // XXXldb Does this handle vertical margins correctly?
    mY = ClearFloats(mY, floatDisplay->mBreakType);
  }
    // Get the band of available space
  GetAvailableSpace(mY, aForceFit);

  NS_ASSERTION(floatFrame->GetParent() == mBlock,
               "Float frame has wrong parent");

  // Reflow the float
  mBlock->ReflowFloat(*this, placeholder, aFloatCache, aReflowStatus);

#ifdef DEBUG
  if (nsBlockFrame::gNoisyReflow) {
    nsRect region = floatFrame->GetRect();
    nsFrame::IndentBy(stdout, nsBlockFrame::gNoiseIndent);
    printf("flowed float: ");
    nsFrame::ListTag(stdout, floatFrame);
    printf(" (%d,%d,%d,%d)\n",
	   region.x, region.y, region.width, region.height);
  }
#endif

  nsSize floatSize = floatFrame->GetSize();
  // Adjust the float size by its margin. That's the area that will
  // impact the space manager.
  floatSize.width += aFloatCache->mMargins.left + aFloatCache->mMargins.right;
  floatSize.height += aFloatCache->mMargins.top + aFloatCache->mMargins.bottom;

  // Find a place to place the float. The CSS2 spec doesn't want
  // floats overlapping each other or sticking out of the containing
  // block if possible (CSS2 spec section 9.5.1, see the rule list).
  NS_ASSERTION((NS_STYLE_FLOAT_LEFT == floatDisplay->mFloats) ||
	       (NS_STYLE_FLOAT_RIGHT == floatDisplay->mFloats),
	       "invalid float type");

  // Can the float fit here?
  PRBool keepFloatOnSameLine = PR_FALSE;

  while (!CanPlaceFloat(floatSize, floatDisplay->mFloats, aForceFit)) {
    if (mAvailSpaceRect.height <= 0) {
      // No space, nowhere to put anything.
      mY = saveY;
      return PR_FALSE;
    }

    // Nope. try to advance to the next band.
    if (NS_STYLE_DISPLAY_TABLE != floatDisplay->mDisplay ||
          eCompatibility_NavQuirks != mPresContext->CompatibilityMode() ) {

      mY += mAvailSpaceRect.height;
      GetAvailableSpace(mY, aForceFit);
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
        if (nsGkAtoms::tableOuterFrame == prevFrame->GetType()) {
          //see if it has "align="
          // IE makes a difference between align and he float property
          nsIContent* content = prevFrame->GetContent();
          if (content) {
            // we're interested only if previous frame is align=left
            // IE messes things up when "right" (overlapping frames) 
            if (content->AttrValueIs(kNameSpaceID_None, nsGkAtoms::align,
                                     NS_LITERAL_STRING("left"), eIgnoreCase)) {
              keepFloatOnSameLine = PR_TRUE;
              // don't advance to next line (IE quirkie behaviour)
              // it breaks rule CSS2/9.5.1/1, but what the hell
              // since we cannot evangelize the world
              break;
            }
          }
        }
      }

      // the table does not fit anymore in this line so advance to next band 
      mY += mAvailSpaceRect.height;
      GetAvailableSpace(mY, aForceFit);
      // reflow the float again now since we have more space
      // XXXldb We really don't need to Reflow in a loop, we just need
      // to ComputeSize in a loop (once ComputeSize depends on
      // availableWidth, which should make this work again).
      mBlock->ReflowFloat(*this, placeholder, aFloatCache, aReflowStatus);
      // Get the floats bounding box and margin information
      floatSize = floatFrame->GetSize();
      // Adjust the float size by its margin. That's the area that will
      // impact the space manager.
      floatSize.width += aFloatCache->mMargins.left + aFloatCache->mMargins.right;
      floatSize.height += aFloatCache->mMargins.top + aFloatCache->mMargins.bottom;
    }
  }
  // If the float is continued, it will get the same absolute x value as its prev-in-flow
  nsRect prevRect(0,0,0,0);

  // We don't worry about the geometry of the prev in flow, let the continuation
  // place and size itself as required.

  // Assign an x and y coordinate to the float. Note that the x,y
  // coordinates are computed <b>relative to the translation in the
  // spacemanager</b> which means that the impacted region will be
  // <b>inside</b> the border/padding area.
  PRBool isLeftFloat;
  nscoord floatX, floatY;
  if (NS_STYLE_FLOAT_LEFT == floatDisplay->mFloats) {
    isLeftFloat = PR_TRUE;
    floatX = mAvailSpaceRect.x;
  }
  else {
    isLeftFloat = PR_FALSE;
    if (!keepFloatOnSameLine) {
      floatX = mAvailSpaceRect.XMost() - floatSize.width;
    } 
    else {
      // this is the IE quirk (see few lines above)
      // the table is kept in the same line: don't let it overlap the
      // previous float 
      floatX = mAvailSpaceRect.x;
    }
  }
  *aIsLeftFloat = isLeftFloat;
  const nsMargin& borderPadding = BorderPadding();
  floatY = mY - borderPadding.top;
  if (floatY < 0) {
    // CSS2 spec, 9.5.1 rule [4]: "A floating box's outer top may not
    // be higher than the top of its containing block."  (Since the
    // containing block is the content edge of the block box, this
    // means the margin edge of the float can't be higher than the
    // content edge of the block that contains it.)
    floatY = 0;
  }

  // Place the float in the space manager
  // if the float split, then take up all of the vertical height 
  if (NS_FRAME_IS_NOT_COMPLETE(aReflowStatus) && 
      (NS_UNCONSTRAINEDSIZE != mContentArea.height)) {
    floatSize.height = PR_MAX(floatSize.height, mContentArea.height - floatY);
  }

  nsRect region(floatX, floatY, floatSize.width, floatSize.height);
  
  // Don't send rectangles with negative margin-box width or height to
  // the space manager; it can't deal with them.
  if (region.width < 0) {
    // Preserve the right margin-edge for left floats and the left
    // margin-edge for right floats
    if (isLeftFloat) {
      region.x = region.XMost();
    }
    region.width = 0;
  }
  if (region.height < 0) {
    region.height = 0;
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
  nscoord x = borderPadding.left + aFloatCache->mMargins.left + floatX;
  nscoord y = borderPadding.top + aFloatCache->mMargins.top + floatY;

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
  nsContainerFrame::PositionFrameView(floatFrame);
  nsContainerFrame::PositionChildViews(floatFrame);

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
  mFloatCombinedArea.UnionRect(combinedArea, mFloatCombinedArea);

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

  return PR_TRUE;
}

/**
 * Place below-current-line floats.
 */
PRBool
nsBlockReflowState::PlaceBelowCurrentLineFloats(nsFloatCacheFreeList& aList, PRBool aForceFit)
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
      PRBool placed = FlowAndPlaceFloat(fc, &isLeftFloat, reflowStatus, aForceFit);
      NS_ASSERTION(placed || !aForceFit,
                   "If we're in force-fit mode, we should have placed the float");

      if (!placed || NS_FRAME_IS_TRUNCATED(reflowStatus)) {
        // return before processing all of the floats, since the line will be pushed.
        return PR_FALSE;
      }
      else if (NS_FRAME_IS_NOT_COMPLETE(reflowStatus)) {
        // Create a continuation for the incomplete float and its placeholder.
        nsresult rv = mBlock->SplitPlaceholder(*this, fc->mPlaceholder);
        if (NS_FAILED(rv)) 
          return PR_FALSE;
      } else {
        // Float is complete. We need to delete any leftover placeholders now.
        nsIFrame* nextPlaceholder = fc->mPlaceholder->GetNextInFlow();
        if (nextPlaceholder) {
          nsHTMLContainerFrame* parent =
            NS_STATIC_CAST(nsHTMLContainerFrame*, nextPlaceholder->GetParent());
          parent->DeleteNextInFlowChild(mPresContext, nextPlaceholder);
        }
      }
    }
    fc = fc->Next();
  }
  return PR_TRUE;
}

nscoord
nsBlockReflowState::ClearFloats(nscoord aY, PRUint8 aBreakType)
{
#ifdef DEBUG
  if (nsBlockFrame::gNoisyReflow) {
    nsFrame::IndentBy(stdout, nsBlockFrame::gNoiseIndent);
    printf("clear floats: in: aY=%d(%d)\n",
           aY, aY - BorderPadding().top);
  }
#endif

#ifdef NOISY_FLOAT_CLEARING
  printf("nsBlockReflowState::ClearFloats: aY=%d breakType=%d\n",
         aY, aBreakType);
  mSpaceManager->List(stdout);
#endif
  
  const nsMargin& bp = BorderPadding();
  nscoord newY = mSpaceManager->ClearFloats(aY - bp.top, aBreakType);
  newY += bp.top;

#ifdef DEBUG
  if (nsBlockFrame::gNoisyReflow) {
    nsFrame::IndentBy(stdout, nsBlockFrame::gNoiseIndent);
    printf("clear floats: out: y=%d(%d)\n", newY, newY - bp.top);
  }
#endif

  return newY;
}

