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
 *   Steve Clark <buster@netscape.com>
 *   Robert O'Callahan <roc+moz@cs.cmu.edu>
 *   L. David Baron <dbaron@fas.harvard.edu>
 */

#include "nsBlockReflowContext.h"
#include "nsBlockReflowState.h"
#include "nsBlockFrame.h"
#include "nsLineLayout.h"
#include "nsIPresContext.h"
#include "nsLayoutAtoms.h"
#include "nsIFrame.h"

#ifdef DEBUG
#include "nsBlockDebugFlags.h"
#endif

nsBlockReflowState::nsBlockReflowState(const nsHTMLReflowState& aReflowState,
                                       nsIPresContext* aPresContext,
                                       nsBlockFrame* aFrame,
                                       const nsHTMLReflowMetrics& aMetrics,
                                       PRBool aBlockMarginRoot)
  : mBlock(aFrame),
    mPresContext(aPresContext),
    mReflowState(aReflowState),
    mLastFloaterY(0),
    mNextRCFrame(nsnull),
    mPrevBottomMargin(0),
    mLineNumber(0),
    mFlags(0)
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

  NS_ASSERTION( nsnull != mSpaceManager, "SpaceManager should be set in nsBlockReflowState" );
  if( nsnull != mSpaceManager ) {
    // Translate into our content area and then save the 
    // coordinate system origin for later.
    mSpaceManager->Translate(borderPadding.left, borderPadding.top);
    mSpaceManager->GetTranslation(mSpaceManagerX, mSpaceManagerY);
  }

  mReflowStatus = NS_FRAME_COMPLETE;

  mPresContext = aPresContext;
  mBlock->GetNextInFlow((nsIFrame**)&mNextInFlow);
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
      mContentArea.width = aReflowState.availableWidth - lr;
    }
  }
  mHaveRightFloaters = PR_FALSE;

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
    mContentArea.height = mBottomEdge - borderPadding.top;
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
  mCurrentLine = nsnull;
  mPrevLine = nsnull;

  const nsStyleText* styleText;
  mBlock->GetStyleData(eStyleStruct_Text,
                       (const nsStyleStruct*&) styleText);
  switch (styleText->mWhiteSpace) {
  case NS_STYLE_WHITESPACE_PRE:
  case NS_STYLE_WHITESPACE_NOWRAP:
    SetFlag(BRS_NOWRAP, PR_TRUE);
    break;
  default:
    SetFlag(BRS_NOWRAP, PR_FALSE);
    break;
  }

  SetFlag(BRS_COMPUTEMAXELEMENTSIZE, (nsnull != aMetrics.maxElementSize));
#ifdef NOISY_MAX_ELEMENT_SIZE
  printf("BRS: setting compute-MES to %d\n", (nsnull != aMetrics.maxElementSize));
#endif
  mMaxElementSize.SizeTo(0, 0);
  SetFlag(BRS_COMPUTEMAXWIDTH, 
          (NS_REFLOW_CALC_MAX_WIDTH == (aMetrics.mFlags & NS_REFLOW_CALC_MAX_WIDTH)));
  mMaximumWidth = 0;

  mMinLineHeight = nsHTMLReflowState::CalcLineHeight(mPresContext,
                                                     aReflowState.rendContext,
                                                     aReflowState.frame);
}

nsBlockReflowState::~nsBlockReflowState()
{
  // Restore the coordinate system
  const nsMargin& borderPadding = BorderPadding();
  mSpaceManager->Translate(-borderPadding.left, -borderPadding.top);
}

nsLineBox*
nsBlockReflowState::NewLineBox(nsIFrame* aFrame,
                               PRInt32 aCount,
                               PRBool aIsBlock)
{
  nsCOMPtr<nsIPresShell> shell;
  mPresContext->GetShell(getter_AddRefs(shell));

  return NS_NewLineBox(shell, aFrame, aCount, aIsBlock);
}

void
nsBlockReflowState::FreeLineBox(nsLineBox* aLine)
{
  if (aLine) {
    nsCOMPtr<nsIPresShell> presShell;
    mPresContext->GetShell(getter_AddRefs(presShell));
    
    aLine->Destroy(presShell);
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
  printf("CBAS frame=%p has floater count %d\n", aFrame, mBand.GetFloaterCount());
  mBand.List();
#endif
  aResult.y = mY;
  aResult.height = GetFlag(BRS_UNCONSTRAINEDHEIGHT)
    ? NS_UNCONSTRAINEDSIZE
    : mBottomEdge - mY;

  const nsMargin& borderPadding = BorderPadding();

  /* bug 18445: treat elements mapped to display: block such as text controls
   * just like normal blocks   */
  PRBool treatAsNotSplittable=PR_FALSE;
  nsCOMPtr<nsIAtom>frameType;
  aFrame->GetFrameType(getter_AddRefs(frameType));
  if (frameType) {
    // text controls are not splittable, so make a special case here
    // XXXldb Why not just set the frame state bit?
    if (nsLayoutAtoms::textInputFrame == frameType.get())
      treatAsNotSplittable = PR_TRUE;
  }

  if (NS_FRAME_SPLITTABLE_NON_RECTANGULAR == aSplitType ||    // normal blocks 
      NS_FRAME_NOT_SPLITTABLE == aSplitType ||                // things like images mapped to display: block
      PR_TRUE == treatAsNotSplittable)                        // text input controls mapped to display: block (special case)
  {
    if (mBand.GetFloaterCount()) {
      // Use the float-edge property to determine how the child block
      // will interact with the floater.
      const nsStyleBorder* borderStyle;
      aFrame->GetStyleData(eStyleStruct_Border,
                           (const nsStyleStruct*&) borderStyle);
      switch (borderStyle->mFloatEdge) {
        default:
        case NS_STYLE_FLOAT_EDGE_CONTENT:  // content and only content does runaround of floaters
          // The child block will flow around the floater. Therefore
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
            // but not overlap the floater(s).
            nsMargin m(0, 0, 0, 0);
            const nsStyleMargin* styleMargin;
            aFrame->GetStyleData(eStyleStruct_Margin,
                                 (const nsStyleStruct*&) styleMargin);
            styleMargin->GetMargin(m); // XXX percentage margins
            if (NS_STYLE_FLOAT_EDGE_PADDING == borderStyle->mFloatEdge) {
              // Add in border too
              nsMargin b;
              borderStyle->GetBorder(b);
              m += b;
            }

            // determine left edge
            if (mBand.GetLeftFloaterCount()) {
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
              if (mBand.GetRightFloaterCount()) {
                if (mBand.GetLeftFloaterCount()) {
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
            // but not overlap the floater.
            aResult.x = mAvailSpaceRect.x + borderPadding.left;
            aResult.width = mAvailSpaceRect.width;
          }
          break;
      }
    }
    else {
      // Since there are no floaters present the float-edge property
      // doesn't matter therefore give the block element all of the
      // available space since it will flow around the floater itself.
      aResult.x = borderPadding.left;
      aResult.width = GetFlag(BRS_UNCONSTRAINEDWIDTH)
        ? NS_UNCONSTRAINEDSIZE
        : mContentArea.width;
    }
  }
  else {
    // The frame is clueless about the space manager and therefore we
    // only give it free space. An example is a table frame - the
    // tables do not flow around floaters.
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
nsBlockReflowState::ClearPastFloaters(PRUint8 aBreakType)
{
  nscoord saveY, deltaY;

  PRBool applyTopMargin = PR_FALSE;
  switch (aBreakType) {
  case NS_STYLE_CLEAR_LEFT:
  case NS_STYLE_CLEAR_RIGHT:
  case NS_STYLE_CLEAR_LEFT_AND_RIGHT:
    // Apply the previous margin before clearing
    saveY = mY + mPrevBottomMargin;
    ClearFloaters(saveY, aBreakType);
#ifdef NOISY_FLOATER_CLEARING
    nsFrame::ListTag(stdout, mBlock);
    printf(": ClearPastFloaters: mPrevBottomMargin=%d saveY=%d oldY=%d newY=%d deltaY=%d\n",
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
      // after clearing the appropriate floaters.
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

      mPrevBottomMargin = deltaY;
      mY = saveY;

      // Force margin to be applied in this circumstance
      applyTopMargin = PR_TRUE;
    }
    else {
      // Put mY back to its original value since no clearing
      // happened. That way the previous blocks bottom margin will
      // be applied properly.
      mY = saveY - mPrevBottomMargin;
    }
    break;
  }
  return applyTopMargin;
}

// Recover the collapsed vertical margin values for aLine. Note that
// the values are not collapsed with aState.mPrevBottomMargin, nor are
// they collapsed with each other when the line height is zero.
void
nsBlockReflowState::RecoverVerticalMargins(nsLineBox* aLine,
                                           PRBool aApplyTopMargin,
                                           nscoord* aTopMarginResult,
                                           nscoord* aBottomMarginResult)
{
  if (aLine->IsBlock()) {
    // Update band data
    GetAvailableSpace();

    // Setup reflow state to compute the block childs top and bottom
    // margins
    nsIFrame* frame = aLine->mFirstChild;
    nsRect availSpaceRect;
    const nsStyleDisplay* display;
    frame->GetStyleData(eStyleStruct_Display,
                        (const nsStyleStruct*&) display);
    nsSplittableType splitType = NS_FRAME_NOT_SPLITTABLE;
    frame->IsSplittable(splitType);
    ComputeBlockAvailSpace(frame, splitType, display, availSpaceRect);
    nsSize availSpace(availSpaceRect.width, availSpaceRect.height);
    nsHTMLReflowState reflowState(mPresContext, mReflowState,
                                  frame, availSpace);

    // Compute collapsed top margin
    nscoord topMargin = 0;
    if (aApplyTopMargin) {
      topMargin =
        nsBlockReflowContext::ComputeCollapsedTopMargin(mPresContext,
                                                        reflowState);
    }

    // Compute collapsed bottom margin
    nscoord bottomMargin = reflowState.mComputedMargin.bottom;
    bottomMargin =
      nsBlockReflowContext::MaxMargin(bottomMargin,
                                      aLine->GetCarriedOutBottomMargin());
    *aTopMarginResult = topMargin;
    *aBottomMarginResult = bottomMargin;
  }
  else {
    // XXX_ib, see bug 44188
    *aTopMarginResult = 0;
    *aBottomMarginResult = 0;
  }
}

void
nsBlockReflowState::RecoverStateFrom(nsLineBox* aLine,
                                     PRBool aApplyTopMargin,
                                     nsRect* aDamageRect)
{
  // Make the line being recovered the current line
  mCurrentLine = aLine;

  // Update aState.mPrevChild as if we had reflowed all of the frames
  // in this line.
  mPrevChild = aLine->LastChild();

  // Recover mKidXMost and mMaxElementSize
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
  if (GetFlag(BRS_COMPUTEMAXELEMENTSIZE)) {
#ifdef NOISY_MAX_ELEMENT_SIZE
    printf("nsBlockReflowState::RecoverStateFrom block %p caching max width %d\n", mBlock, aLine->mMaxElementWidth);
#endif
    UpdateMaxElementSize(nsSize(aLine->mMaxElementWidth, aLine->mBounds.height));
  }

  // If computing the maximum width, then update mMaximumWidth
  if (GetFlag(BRS_COMPUTEMAXWIDTH)) {
#ifdef NOISY_MAXIMUM_WIDTH
    printf("nsBlockReflowState::RecoverStateFrom block %p caching max width %d\n", mBlock, aLine->mMaximumWidth);
#endif
    UpdateMaximumWidth(aLine->mMaximumWidth);
  }

  // The line may have clear before semantics.
  if (aLine->IsBlock() && aLine->HasBreak()) {
    // Clear past floaters before the block if the clear style is not none
    aApplyTopMargin = ClearPastFloaters(aLine->GetBreakType());
#ifdef NOISY_VERTICAL_MARGINS
    nsFrame::ListTag(stdout, mBlock);
    printf(": RecoverStateFrom: y=%d child ", mY);
    nsFrame::ListTag(stdout, aLine->mFirstChild);
    printf(" has clear of %d => %s, mPrevBottomMargin=%d\n", aLine->mBreakType,
           aApplyTopMargin ? "applyTopMargin" : "nope", mPrevBottomMargin);
#endif
  }

  // Recover mPrevBottomMargin and calculate the line's new Y
  // coordinate (newLineY)
  nscoord newLineY = mY;
  nsRect lineCombinedArea;
  aLine->GetCombinedArea(&lineCombinedArea);
  if (aLine->IsBlock()) {
    if ((0 == aLine->mBounds.height) && (0 == lineCombinedArea.height)) {
      // The line's top and bottom margin values need to be collapsed
      // with the mPrevBottomMargin to determine a new
      // mPrevBottomMargin value.
      nscoord topMargin, bottomMargin;
      RecoverVerticalMargins(aLine, aApplyTopMargin,
                             &topMargin, &bottomMargin);
      nscoord m = nsBlockReflowContext::MaxMargin(bottomMargin,
                                                  mPrevBottomMargin);
      m = nsBlockReflowContext::MaxMargin(m, topMargin);
      mPrevBottomMargin = m;
    }
    else {
      // Recover the top and bottom margins for this line
      nscoord topMargin, bottomMargin;
      RecoverVerticalMargins(aLine, aApplyTopMargin,
                             &topMargin, &bottomMargin);

      // Compute the collapsed top margin value
      nscoord collapsedTopMargin =
        nsBlockReflowContext::MaxMargin(topMargin, mPrevBottomMargin);

      // The lineY is just below the collapsed top margin value. The
      // mPrevBottomMargin gets set to the bottom margin value for the
      // line.
      newLineY += collapsedTopMargin;
      mPrevBottomMargin = bottomMargin;
    }
  }
  else if (0 == aLine->GetHeight()) {
    // For empty inline lines we leave the previous bottom margin
    // alone so that it's collpased with the next line.
  }
  else {
    // For non-empty inline lines the previous margin is applied
    // before the line. Therefore apply it now and zero it out.
    newLineY += mPrevBottomMargin;
    mPrevBottomMargin = 0;
  }

  // Save away the old combined area for later
  nsRect oldCombinedArea = lineCombinedArea;

  // Slide the frames in the line by the computed delta. This also
  // updates the lines Y coordinate and the combined area's Y
  // coordinate.
  nscoord finalDeltaY = newLineY - aLine->mBounds.y;
  mBlock->SlideLine(*this, aLine, finalDeltaY);
  // aLine has been slided, but...
  // XXX it is not necessary to worry about the ascent of mBlock here, right?
  // Indeed, depending on the status of the first line of mBlock, we can either have:
  // case first line of mBlock is dirty : it will be reflowed by mBlock and so
  // mBlock->mAscent will be recomputed by the block frame, and we will
  // never enter into this RecoverStateFrom(aLine) function.
  // case first line of mBlock is clean : it is untouched by the incremental reflow.
  //      In other words, aLine is never equals to mBlock->mLines in this function.
  //      so mBlock->mAscent will remain unchanged. 

  // Place floaters for this line into the space manager
  if (aLine->HasFloaters()) {
    // Undo border/padding translation since the nsFloaterCache's
    // coordinates are relative to the frame not relative to the
    // border/padding.
    const nsMargin& bp = BorderPadding();
    mSpaceManager->Translate(-bp.left, -bp.top);

    // Place the floaters into the space-manager again. Also slide
    // them, just like the regular frames on the line.
    nsRect r;
    nsFloaterCache* fc = aLine->GetFirstFloater();
    while (fc) {
      fc->mRegion.y += finalDeltaY;
      fc->mCombinedArea.y += finalDeltaY;
      nsIFrame* floater = fc->mPlaceholder->GetOutOfFlowFrame();
      floater->GetRect(r);
      floater->MoveTo(mPresContext, r.x, r.y + finalDeltaY);
#ifdef DEBUG
      if (nsBlockFrame::gNoisyReflow || nsBlockFrame::gNoisySpaceManager) {
        nscoord tx, ty;
        mSpaceManager->GetTranslation(tx, ty);
        nsFrame::IndentBy(stdout, nsBlockFrame::gNoiseIndent);
        printf("RecoverState: txy=%d,%d (%d,%d) ",
               tx, ty, mSpaceManagerX, mSpaceManagerY);
        nsFrame::ListTag(stdout, floater);
        printf(" r.y=%d finalDeltaY=%d (sum=%d) region={%d,%d,%d,%d}\n",
               r.y, finalDeltaY, r.y + finalDeltaY,
               fc->mRegion.x, fc->mRegion.y,
               fc->mRegion.width, fc->mRegion.height);
      }
#endif
      mSpaceManager->AddRectRegion(floater, fc->mRegion);
      mLastFloaterY = fc->mRegion.y;
      fc = fc->Next();
    }
#ifdef DEBUG
    if (nsBlockFrame::gNoisyReflow || nsBlockFrame::gNoisySpaceManager) {
      mSpaceManager->List(stdout);
    }
#endif
    // And then put the translation back again
    mSpaceManager->Translate(bp.left, bp.top);
  }

  // Recover mY
  mY = aLine->mBounds.YMost();

  // Compute the damage area
  if (aDamageRect) {
    if (0 == finalDeltaY) {
      aDamageRect->Empty();
    } else {
      aLine->GetCombinedArea(&lineCombinedArea);
      aDamageRect->UnionRect(oldCombinedArea, lineCombinedArea);
    }
  }

// XXX Does this do anything?  It doesn't seem to work.... (bug 29413)
  // It's possible that the line has clear after semantics
  if (!aLine->IsBlock() && aLine->HasBreak()) {
    PRUint8 breakType = aLine->GetBreakType();
    switch (breakType) {
    case NS_STYLE_CLEAR_LEFT:
    case NS_STYLE_CLEAR_RIGHT:
    case NS_STYLE_CLEAR_LEFT_AND_RIGHT:
      ClearFloaters(mY, breakType);
      break;
    }
  }
}

PRBool
nsBlockReflowState::IsImpactedByFloater() const
{
#ifdef REALLY_NOISY_REFLOW
  printf("nsBlockReflowState::IsImpactedByFloater %p returned %d\n", 
         this, mBand.GetFloaterCount());
#endif
  return mBand.GetFloaterCount();
}


void
nsBlockReflowState::InitFloater(nsLineLayout& aLineLayout,
                                nsPlaceholderFrame* aPlaceholder)
{
  // Set the geometric parent of the floater
  nsIFrame* floater = aPlaceholder->GetOutOfFlowFrame();
  floater->SetParent(mBlock);

  // Then add the floater to the current line and place it when
  // appropriate
  AddFloater(aLineLayout, aPlaceholder, PR_TRUE);
}

// This is called by the line layout's AddFloater method when a
// place-holder frame is reflowed in a line. If the floater is a
// left-most child (it's x coordinate is at the line's left margin)
// then the floater is place immediately, otherwise the floater
// placement is deferred until the line has been reflowed.

// XXXldb This behavior doesn't quite fit with CSS1 and CSS2 --
// technically we're supposed let the current line flow around the
// float as well unless it won't fit next to what we already have.
// But nobody else implements it that way...
void
nsBlockReflowState::AddFloater(nsLineLayout& aLineLayout,
                               nsPlaceholderFrame* aPlaceholder,
                               PRBool aInitialReflow)
{
  NS_PRECONDITION(nsnull != mCurrentLine, "null ptr");

  // Allocate a nsFloaterCache for the floater
  nsFloaterCache* fc = mFloaterCacheFreeList.Alloc();
  fc->mPlaceholder = aPlaceholder;
  fc->mIsCurrentLineFloater = aLineLayout.CanPlaceFloaterNow();

  // Now place the floater immediately if possible. Otherwise stash it
  // away in mPendingFloaters and place it later.
  if (fc->mIsCurrentLineFloater) {
    // Record this floater in the current-line list
    mCurrentLineFloaters.Append(fc);

    // Because we are in the middle of reflowing a placeholder frame
    // within a line (and possibly nested in an inline frame or two
    // that's a child of our block) we need to restore the space
    // manager's translation to the space that the block resides in
    // before placing the floater.
    nscoord ox, oy;
    mSpaceManager->GetTranslation(ox, oy);
    nscoord dx = ox - mSpaceManagerX;
    nscoord dy = oy - mSpaceManagerY;
    mSpaceManager->Translate(-dx, -dy);

    // And then place it
    PRBool isLeftFloater;
    FlowAndPlaceFloater(fc, &isLeftFloater);    

    // Pass on updated available space to the current inline reflow engine
    GetAvailableSpace();
    aLineLayout.UpdateBand(mAvailSpaceRect.x + BorderPadding().left, mY,
                           GetFlag(BRS_UNCONSTRAINEDWIDTH) ? NS_UNCONSTRAINEDSIZE : mAvailSpaceRect.width,
                           mAvailSpaceRect.height,
                           isLeftFloater,
                           aPlaceholder->GetOutOfFlowFrame());

    // Restore coordinate system
    mSpaceManager->Translate(dx, dy);
  }
  else {
    // This floater will be placed after the line is done (it is a
    // below-current-line floater).
    mBelowCurrentLineFloaters.Append(fc);
  }
}

void
nsBlockReflowState::UpdateMaxElementSize(const nsSize& aMaxElementSize)
{
#ifdef NOISY_MAX_ELEMENT_SIZE
  nsSize oldSize = mMaxElementSize;
#endif
  if (aMaxElementSize.width > mMaxElementSize.width) {
    mMaxElementSize.width = aMaxElementSize.width;
  }
  if (aMaxElementSize.height > mMaxElementSize.height) {
    mMaxElementSize.height = aMaxElementSize.height;
  }
#ifdef NOISY_MAX_ELEMENT_SIZE
  if ((mMaxElementSize.width != oldSize.width) ||
      (mMaxElementSize.height != oldSize.height)) {
    nsFrame::IndentBy(stdout, mBlock->GetDepth());
    if (NS_UNCONSTRAINEDSIZE == mReflowState.availableWidth) {
      printf("PASS1 ");
    }
    nsFrame::ListTag(stdout, mBlock);
    printf(": old max-element-size=%d,%d new=%d,%d\n",
           oldSize.width, oldSize.height,
           mMaxElementSize.width, mMaxElementSize.height);
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
nsBlockReflowState::CanPlaceFloater(const nsRect& aFloaterRect,
                                    PRUint8 aFloats)
{
  // If the current Y coordinate is not impacted by any floaters
  // then by definition the floater fits.
  PRBool result = PR_TRUE;
  if (0 != mBand.GetFloaterCount()) {
    // XXX We should allow overflow by up to half a pixel here (bug 21193).
    if (mAvailSpaceRect.width < aFloaterRect.width) {
      // The available width is too narrow (and its been impacted by a
      // prior floater)
      result = PR_FALSE;
    }
    else {
      // At this point we know that there is enough horizontal space for
      // the floater (somewhere). Lets see if there is enough vertical
      // space.
      if (mAvailSpaceRect.height < aFloaterRect.height) {
        // The available height is too short. However, its possible that
        // there is enough open space below which is not impacted by a
        // floater.
        //
        // Compute the X coordinate for the floater based on its float
        // type, assuming its placed on the current line. This is
        // where the floater will be placed horizontally if it can go
        // here.
        nscoord xa;
        if (NS_STYLE_FLOAT_LEFT == aFloats) {
          xa = mAvailSpaceRect.x;
        }
        else {
          xa = mAvailSpaceRect.XMost() - aFloaterRect.width;

          // In case the floater is too big, don't go past the left edge
          if (xa < mAvailSpaceRect.x) {
            xa = mAvailSpaceRect.x;
          }
        }
        nscoord xb = xa + aFloaterRect.width;

        // Calculate the top and bottom y coordinates, again assuming
        // that the floater is placed on the current line.
        const nsMargin& borderPadding = BorderPadding();
        nscoord ya = mY - borderPadding.top;
        if (ya < 0) {
          // CSS2 spec, 9.5.1 rule [4]: "A floating box's outer top may not
          // be higher than the top of its containing block."  (Since the
          // containing block is the content edge of the block box, this
          // means the margin edge of the floater can't be higher than the
          // content edge of the block that contains it.)
          ya = 0;
        }
        nscoord yb = ya + aFloaterRect.height;

        nscoord saveY = mY;
        for (;;) {
          // Get the available space at the new Y coordinate
          mY += mAvailSpaceRect.height;
          GetAvailableSpace();

          if (0 == mBand.GetFloaterCount()) {
            // Winner. This band has no floaters on it, therefore
            // there can be no overlap.
            break;
          }

          // Check and make sure the floater won't intersect any
          // floaters on this band. The floaters starting and ending
          // coordinates must be entirely in the available space.
          if ((xa < mAvailSpaceRect.x) || (xb > mAvailSpaceRect.XMost())) {
            // The floater can't go here.
            result = PR_FALSE;
            break;
          }

          // See if there is now enough height for the floater.
          if (yb < mY + mAvailSpaceRect.height) {
            // Winner. The bottom Y coordinate of the floater is in
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
nsBlockReflowState::FlowAndPlaceFloater(nsFloaterCache* aFloaterCache,
                                        PRBool* aIsLeftFloater)
{
  // Save away the Y coordinate before placing the floater. We will
  // restore mY at the end after placing the floater. This is
  // necessary because any adjustments to mY during the floater
  // placement are for the floater only, not for any non-floating
  // content.
  nscoord saveY = mY;

  // Grab the compatibility mode
  nsCompatibility mode;
  mPresContext->GetCompatibilityMode(&mode);

  nsIFrame* floater = aFloaterCache->mPlaceholder->GetOutOfFlowFrame();

  // Grab the floater's display information
  const nsStyleDisplay* floaterDisplay;
  floater->GetStyleData(eStyleStruct_Display,
                        (const nsStyleStruct*&)floaterDisplay);

  // This will hold the floater's geometry when we've found a place
  // for it to live.
  nsRect region;

  // Advance mY to mLastFloaterY (if it's not past it already) to
  // enforce 9.5.1 rule [2]; i.e., make sure that a float isn't
  // ``above'' another float that preceded it in the flow.
  mY = NS_MAX(mLastFloaterY, mY);

  // See if the floater should clear any preceeding floaters...
  if (NS_STYLE_CLEAR_NONE != floaterDisplay->mBreakType) {
    // XXXldb Does this handle vertical margins correctly?
    ClearFloaters(mY, floaterDisplay->mBreakType);
  }
  else {
    // Get the band of available space
    GetAvailableSpace();
  }

  // Reflow the floater
  mBlock->ReflowFloater(*this, aFloaterCache->mPlaceholder, aFloaterCache->mCombinedArea,
			aFloaterCache->mMargins, aFloaterCache->mOffsets);

  // Get the floaters bounding box and margin information
  floater->GetRect(region);

#ifdef DEBUG
  if (nsBlockFrame::gNoisyReflow) {
    nsFrame::IndentBy(stdout, nsBlockFrame::gNoiseIndent);
    printf("flowed floater: ");
    nsFrame::ListTag(stdout, floater);
    printf(" (%d,%d,%d,%d)\n",
	   region.x, region.y, region.width, region.height);
  }
#endif

  // Adjust the floater size by its margin. That's the area that will
  // impact the space manager.
  region.width += aFloaterCache->mMargins.left + aFloaterCache->mMargins.right;
  region.height += aFloaterCache->mMargins.top + aFloaterCache->mMargins.bottom;

  // Find a place to place the floater. The CSS2 spec doesn't want
  // floaters overlapping each other or sticking out of the containing
  // block if possible (CSS2 spec section 9.5.1, see the rule list).
  NS_ASSERTION((NS_STYLE_FLOAT_LEFT == floaterDisplay->mFloats) ||
	       (NS_STYLE_FLOAT_RIGHT == floaterDisplay->mFloats),
	       "invalid float type");

  // In backwards compatibility mode, we don't bother to see if a
  // floated table can ``really'' fit: in old browsers, floating
  // tables are horizontally stacked regardless of available space.
  // (See bug 43086 about tables vs. non-tables.)
  if ((eCompatibility_NavQuirks != mode) ||
      (NS_STYLE_DISPLAY_TABLE != floaterDisplay->mDisplay)) {
    // Can the floater fit here?
    while (! CanPlaceFloater(region, floaterDisplay->mFloats)) {
      // Nope. Advance to the next band.
      mY += mAvailSpaceRect.height;
      GetAvailableSpace();
    }
  }

  // Assign an x and y coordinate to the floater. Note that the x,y
  // coordinates are computed <b>relative to the translation in the
  // spacemanager</b> which means that the impacted region will be
  // <b>inside</b> the border/padding area.
  PRBool okToAddRectRegion = PR_TRUE;
  PRBool isLeftFloater;
  if (NS_STYLE_FLOAT_LEFT == floaterDisplay->mFloats) {
    isLeftFloater = PR_TRUE;
    region.x = mAvailSpaceRect.x;
  }
  else {
    isLeftFloater = PR_FALSE;
    if (NS_UNCONSTRAINEDSIZE != mAvailSpaceRect.XMost())
      region.x = mAvailSpaceRect.XMost() - region.width;
    else {
      okToAddRectRegion = PR_FALSE;
      region.x = mAvailSpaceRect.x;
    }
  }
  *aIsLeftFloater = isLeftFloater;
  const nsMargin& borderPadding = BorderPadding();
  region.y = mY - borderPadding.top;
  if (region.y < 0) {
    // CSS2 spec, 9.5.1 rule [4]: "A floating box's outer top may not
    // be higher than the top of its containing block."  (Since the
    // containing block is the content edge of the block box, this
    // means the margin edge of the floater can't be higher than the
    // content edge of the block that contains it.)
    region.y = 0;
  }

  // Place the floater in the space manager
  if (okToAddRectRegion) {
#ifdef DEBUG
    nsresult rv =
#endif
    mSpaceManager->AddRectRegion(floater, region);
    NS_ABORT_IF_FALSE(NS_SUCCEEDED(rv), "bad floater placement");
  }

  // Save away the floaters region in the spacemanager, after making
  // it relative to the containing block's frame instead of relative
  // to the spacemanager translation (which is inset by the
  // border+padding).
  aFloaterCache->mRegion.x = region.x + borderPadding.left;
  aFloaterCache->mRegion.y = region.y + borderPadding.top;
  aFloaterCache->mRegion.width = region.width;
  aFloaterCache->mRegion.height = region.height;
#ifdef NOISY_SPACEMANAGER
  nscoord tx, ty;
  mSpaceManager->GetTranslation(tx, ty);
  nsFrame::ListTag(stdout, mBlock);
  printf(": PlaceFloater: AddRectRegion: txy=%d,%d (%d,%d) {%d,%d,%d,%d}\n",
         tx, ty, mSpaceManagerX, mSpaceManagerY,
         aFloaterCache->mRegion.x, aFloaterCache->mRegion.y,
         aFloaterCache->mRegion.width, aFloaterCache->mRegion.height);
#endif

  // Set the origin of the floater frame, in frame coordinates. These
  // coordinates are <b>not</b> relative to the spacemanager
  // translation, therefore we have to factor in our border/padding.
  nscoord x = borderPadding.left + aFloaterCache->mMargins.left + region.x;
  nscoord y = borderPadding.top + aFloaterCache->mMargins.top + region.y;

  // If floater is relatively positioned, factor that in as well
  // XXXldb Should this be done after handling the combined area
  // below?
  if (NS_STYLE_POSITION_RELATIVE == floaterDisplay->mPosition) {
    x += aFloaterCache->mOffsets.left;
    y += aFloaterCache->mOffsets.top;
  }

  // Position the floater and make sure and views are properly
  // positioned. We need to explicitly position its child views as
  // well, since we're moving the floater after flowing it.
  floater->MoveTo(mPresContext, x, y);
  nsContainerFrame::PositionFrameView(mPresContext, floater);
  nsContainerFrame::PositionChildViews(mPresContext, floater);

  // Update the floater combined area state
  nsRect combinedArea = aFloaterCache->mCombinedArea;
  combinedArea.x += x;
  combinedArea.y += y;
  if (!isLeftFloater && 
      (GetFlag(BRS_UNCONSTRAINEDWIDTH) || GetFlag(BRS_SHRINKWRAPWIDTH))) {
    // When we are placing a right floater in an unconstrained situation or
    // when shrink wrapping, we don't apply it to the floater combined area
    // immediately. Otherwise we end up with an infinitely wide combined
    // area. Instead, we save it away in mRightFloaterCombinedArea so that
    // later on when we know the width of a line we can compute a better value.
    if (!mHaveRightFloaters) {
      mRightFloaterCombinedArea = combinedArea;
      mHaveRightFloaters = PR_TRUE;
    }
    else {
      nsBlockFrame::CombineRects(combinedArea, mRightFloaterCombinedArea);
    }
  }
  else {
    nsBlockFrame::CombineRects(combinedArea, mFloaterCombinedArea);
  }

  // Remember the y-coordinate of the floater we've just placed
  mLastFloaterY = mY;

  // Now restore mY
  mY = saveY;

#ifdef DEBUG
  if (nsBlockFrame::gNoisyReflow) {
    nsRect r;
    floater->GetRect(r);
    nsFrame::IndentBy(stdout, nsBlockFrame::gNoiseIndent);
    printf("placed floater: ");
    nsFrame::ListTag(stdout, floater);
    printf(" %d,%d,%d,%d\n", r.x, r.y, r.width, r.height);
  }
#endif
}

/**
 * Place below-current-line floaters.
 */
void
nsBlockReflowState::PlaceBelowCurrentLineFloaters(nsFloaterCacheList& aList)
{
  nsFloaterCache* fc = aList.Head();
  while (fc) {
    if (!fc->mIsCurrentLineFloater) {
#ifdef DEBUG
      if (nsBlockFrame::gNoisyReflow) {
        nsFrame::IndentBy(stdout, nsBlockFrame::gNoiseIndent);
        printf("placing bcl floater: ");
        nsFrame::ListTag(stdout, fc->mPlaceholder->GetOutOfFlowFrame());
        printf("\n");
      }
#endif

      // Place the floater
      PRBool isLeftFloater;
      FlowAndPlaceFloater(fc, &isLeftFloater);
    }
    fc = fc->Next();
  }
}

void
nsBlockReflowState::ClearFloaters(nscoord aY, PRUint8 aBreakType)
{
#ifdef DEBUG
  if (nsBlockFrame::gNoisyReflow) {
    nsFrame::IndentBy(stdout, nsBlockFrame::gNoiseIndent);
    printf("clear floaters: in: mY=%d aY=%d(%d)\n",
           mY, aY, aY - BorderPadding().top);
  }
#endif

#ifdef NOISY_FLOATER_CLEARING
  printf("nsBlockReflowState::ClearFloaters: aY=%d breakType=%d\n",
         aY, aBreakType);
  mSpaceManager->List(stdout);
#endif
  const nsMargin& bp = BorderPadding();
  nscoord newY = mBand.ClearFloaters(aY - bp.top, aBreakType);
  mY = newY + bp.top;
  GetAvailableSpace();

#ifdef DEBUG
  if (nsBlockFrame::gNoisyReflow) {
    nsFrame::IndentBy(stdout, nsBlockFrame::gNoiseIndent);
    printf("clear floaters: out: mY=%d(%d)\n", mY, mY - bp.top);
  }
#endif
}

