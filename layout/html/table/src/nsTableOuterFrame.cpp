/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "nsTableOuterFrame.h"
#include "nsTableFrame.h"
#include "nsTableCaptionFrame.h"
#include "nsITableContent.h"
#include "nsTableContent.h"
#include "nsBodyFrame.h"
#include "nsReflowCommand.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIRenderingContext.h"
#include "nsCSSRendering.h"
#include "nsIContent.h"
#include "nsIContentDelegate.h"
#include "nsCSSLayout.h"
#include "nsVoidArray.h"
#include "nsReflowCommand.h"

#ifdef NS_DEBUG
static PRBool gsDebug = PR_FALSE;
//#define NOISY
//#define NOISY_FLOW
//#define NOISY_MARGINS
#else
static const PRBool gsDebug = PR_FALSE;
#endif

static NS_DEFINE_IID(kStyleMoleculeSID, NS_STYLEMOLECULE_SID);
static NS_DEFINE_IID(kITableContentIID, NS_ITABLECONTENT_IID);

struct OuterTableReflowState {

  // The body's style molecule
  nsStyleMolecule* mol;

  // The total available size (computed from the parent)
  nsSize availSize;
  // The available size for the inner table frame
  nsSize innerTableMaxSize;

  // Margin tracking information
  nscoord prevMaxPosBottomMargin;
  nscoord prevMaxNegBottomMargin;

  // Flags for whether the max size is unconstrained
  PRBool  unconstrainedWidth;
  PRBool  unconstrainedHeight;

  // Running y-offset
  nscoord y;

  // Flags for where we are in the content
  PRBool firstRowGroup;
  PRBool processingCaption;

  OuterTableReflowState(nsIPresContext*  aPresContext,
                        const nsSize&    aMaxSize,
                        nsStyleMolecule* aMol)
  {
    mol = aMol;
    availSize.width = aMaxSize.width;
    availSize.height = aMaxSize.height;
    prevMaxPosBottomMargin = 0;
    prevMaxNegBottomMargin = 0;
    y=0;  // border/padding/margin???
    unconstrainedWidth = PRBool(aMaxSize.width == NS_UNCONSTRAINEDSIZE);
    unconstrainedHeight = PRBool(aMaxSize.height == NS_UNCONSTRAINEDSIZE);
    firstRowGroup = PR_TRUE;
    processingCaption = PR_FALSE;
  }

  ~OuterTableReflowState() {
  }
};



/* ----------- nsTableOuterFrame ---------- */


/**
  */
nsTableOuterFrame::nsTableOuterFrame(nsIContent* aContent,
                                     PRInt32     aIndexInParent,
                                     nsIFrame*   aParentFrame)
  : nsContainerFrame(aContent, aIndexInParent, aParentFrame),
  mInnerTableFrame(nsnull),
  mCaptionFrames(nsnull),
  mBottomCaptions(nsnull),
  mMinCaptionWidth(0),
  mMaxCaptionWidth(0),
  mFirstPassValid(PR_FALSE)
{
}

nsTableOuterFrame::~nsTableOuterFrame()
{
  if (nsnull!=mCaptionFrames)
    delete mCaptionFrames;
  if (nsnull!=mBottomCaptions)
    delete mBottomCaptions;
}

void nsTableOuterFrame::Paint(nsIPresContext& aPresContext,
                               nsIRenderingContext& aRenderingContext,
                               const nsRect& aDirtyRect)
{
  // for debug...
  if (nsIFrame::GetShowFrameBorders()) {
    aRenderingContext.SetColor(NS_RGB(255,0,0));
    aRenderingContext.DrawRect(0, 0, mRect.width, mRect.height);
  }

  PaintChildren(aPresContext, aRenderingContext, aDirtyRect);
}

PRBool nsTableOuterFrame::NeedsReflow(const nsSize& aMaxSize)
{
  PRBool result=PR_TRUE;
  if (nsnull!=mInnerTableFrame)
    result = mInnerTableFrame->NeedsReflow(aMaxSize);
  return result;
}

PRBool nsTableOuterFrame::IsFirstPassValid()
{
  return mFirstPassValid;
}

void nsTableOuterFrame::SetFirstPassValid(PRBool aValidState)
{
  mFirstPassValid = aValidState;
}

/**
  * ResizeReflow is a 2-step process.
  * In the first step, we lay out all of the captions and the inner table in NS_UNCONSTRAINEDSIZE space.
  * This gives us absolute minimum and maximum widths.
  * In the second step, we force all the captions and the table to the width of the widest component, 
  * given the table's width constraints.
  * With the widths known, we reflow the captions and table.
  * NOTE: for breaking across pages, this method has to account for table content that is not laid out
  *       linearly vis a vis the frames.  That is, content hierarchy and the frame hierarchy do not match.
  */
nsIFrame::ReflowStatus nsTableOuterFrame::ResizeReflow(nsIPresContext* aPresContext,
                                                       nsReflowMetrics& aDesiredSize,
                                                       const nsSize& aMaxSize,
                                                       nsSize* aMaxElementSize)
{
  if (PR_TRUE==gsDebug) printf ("***table outer frame reflow \t\t%d\n", this);
  if (gsDebug==PR_TRUE)
    printf("nsTableOuterFrame::ResizeReflow : maxSize=%d,%d\n",
           aMaxSize.width, aMaxSize.height);

#ifdef NS_DEBUG
  // replace with a check that does not assume linear placement of children
  // PreReflowCheck();
#endif

  // Initialize out parameter
  if (nsnull != aMaxElementSize) {
    aMaxElementSize->width = 0;
    aMaxElementSize->height = 0;
  }

  PRBool        reflowMappedOK = PR_TRUE;
  ReflowStatus  status = frComplete;
  nsSize innerTableMaxElementSize(0,0);

  // Set up our kids.  They're already present, on an overflow list, 
  // or there are none so we'll create them now
  MoveOverflowToChildList();
  if (nsnull==mFirstChild)
    CreateChildFrames(aPresContext);
  if (nsnull!=mPrevInFlow && nsnull==mInnerTableFrame)
  { // if I am a continuing frame, my inner table is my prev-in-flow's mInnerTableFrame's next-in-flow
    CreateInnerTableFrame(aPresContext);
  }
  // at this point, we must have at least one child frame, and we must have an inner table frame
  NS_ASSERTION(nsnull!=mFirstChild, "no children");
  NS_ASSERTION(nsnull!=mInnerTableFrame, "no mInnerTableFrame");
  if (nsnull==mFirstChild  ||  nsnull==mInnerTableFrame) //ERROR!
    return frComplete;

  nsStyleMolecule* tableStyleMol =
    (nsStyleMolecule*)mStyleContext->GetData(kStyleMoleculeSID);
  
  OuterTableReflowState state(aPresContext, aMaxSize, tableStyleMol);

  // lay out captions pass 1, if necessary
  if (PR_FALSE==IsFirstPassValid())
  {
    mFirstPassValid = PR_TRUE;
    status = ResizeReflowCaptionsPass1(aPresContext, tableStyleMol);

  }

  // lay out inner table, if required
  if (PR_FALSE==mInnerTableFrame->IsFirstPassValid())
  { // we treat the table as if we've never seen the layout data before
    mInnerTableFrame->SetReflowPass(nsTableFrame::kPASS_FIRST);
    status = mInnerTableFrame->ResizeReflowPass1(aPresContext, aDesiredSize, aMaxSize, 
                                                 &innerTableMaxElementSize, tableStyleMol);
  
#ifdef NOISY_MARGINS
    nsIContent* content = mInnerTableFrame->GetContent();
    nsTablePart *table = (nsTablePart*)content;
    if (table != nsnull)
      table->DumpCellMap();
    mInnerTableFrame->ResetColumnLayoutData();
    mInnerTableFrame->ListColumnLayoutData(stdout,1);
    NS_IF_RELEASE(content);
#endif

  }
  mInnerTableFrame->SetReflowPass(nsTableFrame::kPASS_SECOND);
  // assign table width info only if the inner table frame is a first-in-flow
  if (nsnull==mInnerTableFrame->GetPrevInFlow())
  {
    // assign column widths, and assign aMaxElementSize->width
    mInnerTableFrame->BalanceColumnWidths(aPresContext, tableStyleMol, aMaxSize, aMaxElementSize);
    // assign table width
    mInnerTableFrame->SetTableWidth(aPresContext, tableStyleMol);
  }
  // inner table max is now the computed width and assigned  height
  state.innerTableMaxSize.width = mInnerTableFrame->GetWidth();
  state.innerTableMaxSize.height = aMaxSize.height; 

  // Reflow the child frames
  if (nsnull != mFirstChild) {
    reflowMappedOK = ReflowMappedChildren(aPresContext, state, aMaxElementSize);
    if (PR_FALSE == reflowMappedOK) {
      status = frNotComplete;
    }
  }

  // Did we successfully relow our mapped children?
  if (PR_TRUE == reflowMappedOK) {
    // Any space left?
    if ((nsnull != mFirstChild) && (state.availSize.height <= 0)) {
      // No space left. Don't try to pull-up children or reflow unmapped
      if (NextChildOffset() < mContent->ChildCount()) {
        status = frNotComplete;
      }
    } else if (NextChildOffset() < mContent->ChildCount()) {
      // Try and pull-up some children from a next-in-flow
      if ((nsnull == mNextInFlow) ||
          PullUpChildren(aPresContext, state, aMaxElementSize)) {
          // nothing to do, we will never have unmapped children!
      } else {
        // We were unable to pull-up all the existing frames from the
        // next in flow
        status = frNotComplete;
      }
    }
  }

  if (frComplete == status) {
    // Don't forget to add in the bottom margin from our last child.
    // Only add it in if there's room for it.
    nscoord margin = state.prevMaxPosBottomMargin -
      state.prevMaxNegBottomMargin;
    if (state.availSize.height >= margin) {
      state.y += margin;
    }
  }

  // Return our desired rect
  //NS_ASSERTION(0<state.y, "illegal height after reflow");
  //NS_ASSERTION(0<state.innerTableMaxSize.width, "illegal width after reflow");
  aDesiredSize.width  = state.innerTableMaxSize.width;
  aDesiredSize.height = state.y;

  if (gsDebug==PR_TRUE) 
  {
    if (nsnull!=aMaxElementSize)
      printf("Outer frame Reflow complete, returning aDesiredSize = %d,%d and aMaxElementSize=%d,%d\n",
              aDesiredSize.width, aDesiredSize.height, 
              aMaxElementSize->width, aMaxElementSize->height);
    else
      printf("Outer frame Reflow complete, returning aDesiredSize = %d,%d and NSNULL aMaxElementSize\n",
              aDesiredSize.width, aDesiredSize.height);
  }

#ifdef NS_DEBUG
  // replace with a check that does not assume linear placement of children
  // PostReflowCheck(status);
#endif

  return status;

}

// Collapse child's top margin with previous bottom margin
nscoord nsTableOuterFrame::GetTopMarginFor(nsIPresContext*        aCX,
                                           OuterTableReflowState& aState,
                                           nsStyleMolecule*       aKidMol)
{
  nscoord margin;
  nscoord maxNegTopMargin = 0;
  nscoord maxPosTopMargin = 0;
  if ((margin = aKidMol->margin.top) < 0) {
    maxNegTopMargin = -margin;
  } else {
    maxPosTopMargin = margin;
  }

  nscoord maxPos = PR_MAX(aState.prevMaxPosBottomMargin, maxPosTopMargin);
  nscoord maxNeg = PR_MAX(aState.prevMaxNegBottomMargin, maxNegTopMargin);
  margin = maxPos - maxNeg;

  return margin;
}

// Position and size aKidFrame and update our reflow state. The origin of
// aKidRect is relative to the upper-left origin of our frame, and includes
// any left/top margin.
void nsTableOuterFrame::PlaceChild( nsIPresContext*    aPresContext,
                                    OuterTableReflowState& aState,
                                    nsIFrame*          aKidFrame,
                                    const nsRect&      aKidRect,
                                    nsStyleMolecule*   aKidMol,
                                    nsSize*            aMaxElementSize,
                                    nsSize&            aKidMaxElementSize)
{
  if (PR_TRUE==gsDebug) printf ("place child: %p with aKidRect %d %d %d %d\n", 
                                 aKidFrame, aKidRect.x, aKidRect.y,
                                 aKidRect.width, aKidRect.height);
  // Place and size the child
  aKidFrame->SetRect(aKidRect);

  // Adjust the running y-offset
  aState.y += aKidRect.height;

  // If our height is constrained then update the available height
  if (PR_FALSE == aState.unconstrainedHeight) {
    aState.availSize.height -= aKidRect.height;
  }

  /* Update the maximum element size, which is the sum of:
   *   the maxElementSize of our first row
   *   plus the maxElementSize of the top caption if we include it
   *   plus the maxElementSize of the bottom caption if we include it
   */
  if (PR_TRUE==aState.processingCaption || PR_TRUE==aState.firstRowGroup)
  {
    if (PR_FALSE==aState.processingCaption)
      aState.firstRowGroup = PR_FALSE;
    if (nsnull != aMaxElementSize) {
      aMaxElementSize->width = aKidMaxElementSize.width;
      aMaxElementSize->height = aKidMaxElementSize.height;
    }
  }
}

/**
 * Reflow the frames we've already created
 *
 * @param   aPresContext presentation context to use
 * @param   aState current inline state
 * @return  true if we successfully reflowed all the mapped children and false
 *            otherwise, e.g. we pushed children to the next in flow
 */
PRBool nsTableOuterFrame::ReflowMappedChildren( nsIPresContext*      aPresContext,
                                                OuterTableReflowState& aState,
                                                nsSize*              aMaxElementSize)
{
#ifdef NS_DEBUG
  VerifyLastIsComplete();
#endif
#ifdef NOISY
  ListTag(stdout);
  printf(": reflow mapped (childCount=%d) [%d,%d,%c]\n",
         mChildCount,
         mFirstContentOffset, mLastContentOffset,
         (mLastContentIsComplete ? 'T' : 'F'));
#ifdef NOISY_FLOW
  {
    nsTableOuterFrame* flow = (nsTableOuterFrame*) mNextInFlow;
    while (flow != 0) {
      printf("  %p: [%d,%d,%c]\n",
             flow, flow->mFirstContentOffset, flow->mLastContentOffset,
             (flow->mLastContentIsComplete ? 'T' : 'F'));
      flow = (nsTableOuterFrame*) flow->mNextInFlow;
    }
  }
#endif
#endif
  NS_PRECONDITION(nsnull != mFirstChild, "no children");

  PRInt32   childCount = 0;
  nsIFrame* prevKidFrame = nsnull;

  // Remember our original mLastContentIsComplete so that if we end up
  // having to push children, we have the correct value to hand to
  // PushChildren.
  PRBool    lastContentIsComplete = mLastContentIsComplete;

  nsSize    kidMaxElementSize;
  nsSize*   pKidMaxElementSize = (nsnull != aMaxElementSize) ? &kidMaxElementSize : nsnull;
  PRBool    result = PR_TRUE;

  for (nsIFrame* kidFrame = mFirstChild; nsnull != kidFrame; ) {
    nsSize                  kidAvailSize(aState.innerTableMaxSize.width, aState.availSize.height);
    nsReflowMetrics         desiredSize;
    nsIFrame::ReflowStatus  status;

    SetReflowState(aState, kidFrame);

    if (PR_TRUE==gsDebug) printf ("ReflowMappedChildren: %p with state = %s\n", 
                                  kidFrame, aState.processingCaption?"caption":"inner");

    // Get top margin for this kid
    nsIStyleContext* kidSC = kidFrame->GetStyleContext(aPresContext);
    nsStyleMolecule* kidMol = (nsStyleMolecule*)kidSC->GetData(kStyleMoleculeSID);
    nscoord topMargin = GetTopMarginFor(aPresContext, aState, kidMol);
    nscoord bottomMargin = kidMol->margin.bottom;
    NS_RELEASE(kidSC);

    // Figure out the amount of available size for the child (subtract
    // off the top margin we are going to apply to it)
    if (PR_FALSE == aState.unconstrainedHeight) {
      kidAvailSize.height -= topMargin;
    }
    // Subtract off for left and right margin
    if (PR_FALSE == aState.unconstrainedWidth) {
      kidAvailSize.width -= kidMol->margin.left + kidMol->margin.right;
    }

    // Only skip the reflow if this is not our first child and we are
    // out of space.
    if ((kidFrame == mFirstChild) || (kidAvailSize.height > 0)) {
      // Reflow the child into the available space
      status = ReflowChild(kidFrame, aPresContext, desiredSize,
                           kidAvailSize, pKidMaxElementSize,
                           aState);
    }

    // Did the child fit?
    if ((kidFrame != mFirstChild) &&
        ((kidAvailSize.height <= 0) ||
         (desiredSize.height > kidAvailSize.height)))
    {
      // The child's height is too big to fit at all in our remaining space,
      // and it's not our first child.
      //
      // Note that if the width is too big that's okay and we allow the
      // child to extend horizontally outside of the reflow area

      // Since we are giving the next-in-flow our last child, we
      // give it our original mLastContentIsComplete, too (in case we
      // are pushing into an empty next-in-flow)
      if (PR_TRUE==gsDebug) printf ("ReflowMappedChildren: calling PushChildren\n");
      PushChildren(kidFrame, prevKidFrame, lastContentIsComplete);

      // Our mLastContentIsComplete was already set by the last kid we
      // reflowed reflow's status
      result = PR_FALSE;
      break;
    }

    // Place the child after taking into account it's margin
    aState.y += topMargin;
    nsRect kidRect (0, 0, desiredSize.width, desiredSize.height);
    kidRect.x += kidMol->margin.left;
    kidRect.y += aState.y;
    if (PR_TRUE==gsDebug) printf ("ReflowMappedChildren: calling PlaceChild\n");
    PlaceChild(aPresContext, aState, kidFrame, kidRect, kidMol, aMaxElementSize,
               kidMaxElementSize);
    if (bottomMargin < 0) {
      aState.prevMaxNegBottomMargin = -bottomMargin;
    } else {
      aState.prevMaxPosBottomMargin = bottomMargin;
    }
    childCount++;

    // Update mLastContentIsComplete now that this kid fits
    mLastContentIsComplete = PRBool(status == frComplete);

    // Special handling for incomplete children
    if (frNotComplete == status) {
      // XXX It's good to assume that we might still have room
      // even if the child didn't complete (floaters will want this)
      nsIFrame* kidNextInFlow = kidFrame->GetNextInFlow();
      if (nsnull == kidNextInFlow) {
        // No the child isn't complete, and it doesn't have a next in flow so
        // create a continuing frame. This hooks the child into the flow.
        nsIFrame* continuingFrame =
          kidFrame->CreateContinuingFrame(aPresContext, this);

        // Insert the frame. We'll reflow it next pass through the loop
        nsIFrame* nextSib = kidFrame->GetNextSibling();
        continuingFrame->SetNextSibling(nextSib);
        kidFrame->SetNextSibling(continuingFrame);
        if (nsnull == nextSib) {
          // Assume that the continuation frame we just created is
          // complete, for now. It will get reflowed by our
          // next-in-flow (we are going to push it now)
          lastContentIsComplete = PR_TRUE;
        }
      }
    }

    // Get the next child
    prevKidFrame = kidFrame;
    kidFrame = kidFrame->GetNextSibling();

    // XXX talk with troy about checking for available space here
  }

  // Update the child count
  mChildCount = childCount;
  NS_POSTCONDITION(LengthOf(mFirstChild) == mChildCount, "bad child count");

  // Set the last content offset based on the last child we mapped.
  NS_ASSERTION(LastChild() == prevKidFrame, "unexpected last child");
  SetLastContentOffset(prevKidFrame);

#ifdef NS_DEBUG
  VerifyLastIsComplete();
#endif
#ifdef NOISY
  ListTag(stdout);
  printf(": reflow mapped %sok (childCount=%d) [%d,%d,%c]\n",
         (result ? "" : "NOT "),
         mChildCount,
         mFirstContentOffset, mLastContentOffset,
         (mLastContentIsComplete ? 'T' : 'F'));
#ifdef NOISY_FLOW
  {
    nsTableOuterFrame* flow = (nsTableOuterFrame*) mNextInFlow;
    while (flow != 0) {
      printf("  %p: [%d,%d,%c]\n",
             flow, flow->mFirstContentOffset, flow->mLastContentOffset,
             (flow->mLastContentIsComplete ? 'T' : 'F'));
      flow = (nsTableOuterFrame*) flow->mNextInFlow;
    }
  }
#endif
#endif
  return result;
}

/**
 * Try and pull-up frames from our next-in-flow
 *
 * @param   aPresContext presentation context to use
 * @param   aState current inline state
 * @return  true if we successfully pulled-up all the children and false
 *            otherwise, e.g. child didn't fit
 */
PRBool nsTableOuterFrame::PullUpChildren(nsIPresContext*      aPresContext,
                                            OuterTableReflowState& aState,
                                            nsSize*              aMaxElementSize)
{
#ifdef NS_DEBUG
  VerifyLastIsComplete();
#endif
#ifdef NOISY
  ListTag(stdout);
  printf(": pullup (childCount=%d) [%d,%d,%c]\n",
         mChildCount,
         mFirstContentOffset, mLastContentOffset,
         (mLastContentIsComplete ? 'T' : 'F'));
#ifdef NOISY_FLOW
  {
    nsTableOuterFrame* flow = (nsTableOuterFrame*) mNextInFlow;
    while (flow != 0) {
      printf("  %p: [%d,%d,%c]\n",
             flow, flow->mFirstContentOffset, flow->mLastContentOffset,
             (flow->mLastContentIsComplete ? 'T' : 'F'));
      flow = (nsTableOuterFrame*) flow->mNextInFlow;
    }
  }
#endif
#endif
  NS_PRECONDITION(nsnull != mNextInFlow, "null next-in-flow");
  nsTableOuterFrame*  nextInFlow = (nsTableOuterFrame*)mNextInFlow;
  nsSize        kidMaxElementSize;
  nsSize*       pKidMaxElementSize = (nsnull != aMaxElementSize) ? &kidMaxElementSize : nsnull;

  // The frame previous to the current frame we are reflowing. This
  // starts out initially as our last frame.
  nsIFrame*     prevKidFrame = LastChild();

  // This will hold the prevKidFrame's mLastContentIsComplete
  // status. If we have to push the frame that follows prevKidFrame
  // then this will become our mLastContentIsComplete state. Since
  // prevKidFrame is initially our last frame, it's completion status
  // is our mLastContentIsComplete value.
  PRBool        prevLastContentIsComplete = mLastContentIsComplete;
  PRBool        result = PR_TRUE;

  while (nsnull != nextInFlow) {
    nsReflowMetrics desiredSize;
    nsSize  kidAvailSize(aState.availSize);

    // Get first available frame from the next-in-flow
    nsIFrame* kidFrame = PullUpOneChild(nextInFlow, prevKidFrame);
    if (nsnull == kidFrame) {
      // We've pulled up all the children from that next-in-flow, so
      // move to the next next-in-flow.
      nextInFlow = (nsTableOuterFrame*) nextInFlow->mNextInFlow;
      continue;
    }

    // Get top margin for this kid
    nsIContent* kid = kidFrame->GetContent();
    nsIStyleContext* kidSC = kidFrame->GetStyleContext(aPresContext);
    nsStyleMolecule* kidMol = (nsStyleMolecule*)kidSC->GetData(kStyleMoleculeSID);
    nscoord topMargin = GetTopMarginFor(aPresContext, aState, kidMol);
    nscoord bottomMargin = kidMol->margin.bottom;

    // Figure out the amount of available size for the child (subtract
    // off the top margin we are going to apply to it)
    if (PR_FALSE == aState.unconstrainedHeight) {
      kidAvailSize.height -= topMargin;
    }
    // Subtract off for left and right margin
    if (PR_FALSE == aState.unconstrainedWidth) {
      kidAvailSize.width -= kidMol->margin.left + kidMol->margin.right;
    }

    ReflowStatus status;
    do {
      // Only skip the reflow if this is not our first child and we are
      // out of space.
      if ((kidFrame == mFirstChild) || (kidAvailSize.height > 0)) {
        SetReflowState(aState, kidFrame);
        status = ReflowChild(kidFrame, aPresContext, desiredSize,
                             kidAvailSize, pKidMaxElementSize,
                             aState);
      }

      // Did the child fit?
      if ((kidFrame != mFirstChild) &&
          ((kidAvailSize.height <= 0) ||
           (desiredSize.height > kidAvailSize.height)))
      {
        // The child's height is too big to fit at all in our remaining space,
        // and it wouldn't have been our first child.
        //
        // Note that if the width is too big that's okay and we allow the
        // child to extend horizontally outside of the reflow area
        PRBool lastComplete = PRBool(nsnull == kidFrame->GetNextInFlow());
        PushChildren(kidFrame, prevKidFrame, lastComplete);
        mLastContentIsComplete = prevLastContentIsComplete;
        mChildCount--;
        result = PR_FALSE;
        NS_RELEASE(kid);
        NS_RELEASE(kidSC);
        goto push_done;
      }

      // Place the child
      aState.y += topMargin;
      nsRect kidRect (0, 0, desiredSize.width, desiredSize.height);
      kidRect.x += kidMol->margin.left;
      kidRect.y += aState.y;
      PlaceChild(aPresContext, aState, kidFrame, kidRect, kidMol, aMaxElementSize,
                 kidMaxElementSize);
      if (bottomMargin < 0) {
        aState.prevMaxNegBottomMargin = -bottomMargin;
      } else {
        aState.prevMaxPosBottomMargin = bottomMargin;
      }
      mLastContentIsComplete = PRBool(status == frComplete);

#ifdef NOISY
      ListTag(stdout);
      printf(": pulled up ");
      ((nsFrame*)kidFrame)->ListTag(stdout);
      printf("\n");
#endif

      // Is the child we just pulled up complete?
      if (frNotComplete == status) {
        // No the child isn't complete.
        nsIFrame* kidNextInFlow = kidFrame->GetNextInFlow();
        if (nsnull == kidNextInFlow) {
          // The child doesn't have a next-in-flow so create a
          // continuing frame. The creation appends it to the flow and
          // prepares it for reflow.
          nsIFrame* continuingFrame =
            kidFrame->CreateContinuingFrame(aPresContext, this);

          // Add the continuing frame to our sibling list.
          continuingFrame->SetNextSibling(kidFrame->GetNextSibling());
          kidFrame->SetNextSibling(continuingFrame);
          prevKidFrame = kidFrame;
          prevLastContentIsComplete = mLastContentIsComplete;
          kidFrame = continuingFrame;
          mChildCount++;
        } else {
          // The child has a next-in-flow, but it's not one of ours.
          // It *must* be in one of our next-in-flows. Collect it
          // then.
          NS_ASSERTION(kidNextInFlow->GetGeometricParent() != this,
                       "busted kid next-in-flow");
          break;
        }
      }
    } while (frNotComplete == status);
    NS_RELEASE(kid);
    NS_RELEASE(kidSC);

    prevKidFrame = kidFrame;
    prevLastContentIsComplete = mLastContentIsComplete;
  }

 push_done:;
  // Update our last content index
  if (nsnull != prevKidFrame) {
    NS_ASSERTION(LastChild() == prevKidFrame, "bad last child");
    SetLastContentOffset(prevKidFrame);
  }

  // We need to make sure the first content offset is correct for any empty
  // next-in-flow frames (frames where we pulled up all the child frames)
  nextInFlow = (nsTableOuterFrame*)mNextInFlow;
  if ((nsnull != nextInFlow) && (nsnull == nextInFlow->mFirstChild)) {
    // We have at least one empty frame. Did we succesfully pull up all the
    // child frames?
    if (PR_FALSE == result) {
      // No, so we need to adjust the first content offset of all the empty
      // frames
      AdjustOffsetOfEmptyNextInFlows();
#ifdef NS_DEBUG
    } else {
      // Yes, we successfully pulled up all the child frames which means all
      // the next-in-flows must be empty. Do a sanity check
      while (nsnull != nextInFlow) {
        NS_ASSERTION(nsnull == nextInFlow->mFirstChild, "non-empty next-in-flow");
        nextInFlow = (nsTableOuterFrame*)nextInFlow->GetNextInFlow();
      }
#endif
    }
  }

#ifdef NS_DEBUG
  PRInt32 len = LengthOf(mFirstChild);
  NS_ASSERTION(len == mChildCount, "bad child count");
  VerifyLastIsComplete();
#endif
#ifdef NOISY
  ListTag(stdout);
  printf(": pullup %sok (childCount=%d) [%d,%d,%c]\n",
         (result ? "" : "NOT "),
         mChildCount,
         mFirstContentOffset, mLastContentOffset,
         (mLastContentIsComplete ? 'T' : 'F'));
#ifdef NOISY_FLOW
  {
    nsTableOuterFrame* flow = (nsTableOuterFrame*) mNextInFlow;
    while (flow != 0) {
      printf("  %p: [%d,%d,%c]\n",
             flow, flow->mFirstContentOffset, flow->mLastContentOffset,
             (flow->mLastContentIsComplete ? 'T' : 'F'));
      flow = (nsTableOuterFrame*) flow->mNextInFlow;
    }
  }
#endif
#endif
  return result;
}

/** set the OuterTableReflowState based on the child frame we are currently processing.
  * must be called before ReflowChild, at a minimum.
  */
void nsTableOuterFrame::SetReflowState(OuterTableReflowState& aState, nsIFrame* aKidFrame)
{
  nsIContent *kid = aKidFrame->GetContent();                             // kid: REFCNT++
  nsITableContent *tableContentInterface = nsnull;
  kid->QueryInterface(kITableContentIID, (void**)&tableContentInterface);// tableContentInterface: REFCNT++
  if (nsnull!=tableContentInterface)
  {
    aState.processingCaption = PR_TRUE;
    NS_RELEASE(tableContentInterface);                                   // tableContentInterface: REFCNT--
  }
  else
    aState.processingCaption = PR_FALSE;
  NS_RELEASE(kid);                                                       // kid: REFCNT--
}

/**
 * Reflow a child frame and return the status of the reflow. If the
 * child is complete and it has next-in-flows (it was a splittable child)
 * then delete the next-in-flows.
 */
nsIFrame::ReflowStatus
nsTableOuterFrame::ReflowChild( nsIFrame*        aKidFrame,
                                nsIPresContext*  aPresContext,
                                nsReflowMetrics& aDesiredSize,
                                const nsSize&    aMaxSize,
                                nsSize*          aMaxElementSize,
                                OuterTableReflowState& aState)
{
  ReflowStatus status;

  /* call the appropriate reflow method based on the type and position of the child */
//  if (((nsSplittableFrame*)aKidFrame)->GetFirstInFlow()!=mInnerTableFrame)
  if (PR_TRUE==aState.processingCaption)
  { // it's a caption, find out if it's top or bottom
    // Resolve style
    nsIStyleContext* captionStyleContext = aKidFrame->GetStyleContext(aPresContext);
    NS_ASSERTION(nsnull != captionStyleContext, "null style context for caption");
    nsStyleMolecule* captionStyle =
      (nsStyleMolecule*)captionStyleContext->GetData(kStyleMoleculeSID);
    NS_ASSERTION(nsnull != captionStyle, "null style molecule for caption");
    if (NS_STYLE_VERTICAL_ALIGN_BOTTOM==captionStyle->verticalAlign)
      status = ResizeReflowBottomCaptionsPass2(aPresContext, aDesiredSize,
                                               aMaxSize, aMaxElementSize,
                                               aState.mol, aState.y);
    else
      status = ResizeReflowTopCaptionsPass2(aPresContext, aDesiredSize,
                                            aMaxSize, aMaxElementSize,
                                            aState.mol);
  }
  else
    status = ((nsTableFrame*)aKidFrame)->ResizeReflowPass2(aPresContext, aDesiredSize, aState.innerTableMaxSize, 
                                                           aMaxElementSize, aState.mol,
                                                           mMinCaptionWidth, mMaxCaptionWidth);

  if (frComplete == status) {
    nsIFrame* kidNextInFlow = aKidFrame->GetNextInFlow();
    if (nsnull != kidNextInFlow) {
      // Remove all of the childs next-in-flows. Make sure that we ask
      // the right parent to do the removal (it's possible that the
      // parent is not this because we are executing pullup code)
      nsTableOuterFrame* parent = (nsTableOuterFrame*)
        aKidFrame->GetGeometricParent();
      parent->DeleteChildsNextInFlow(aKidFrame);
    }
  }
  return status;
}

void nsTableOuterFrame::CreateChildFrames(nsIPresContext*  aPresContext)
{
  nsIFrame *prevKidFrame = nsnull;
  nsresult frameCreated = nsTableFrame::NewFrame((nsIFrame **)(&mInnerTableFrame), mContent, 0, this);
  if (NS_OK!=frameCreated)
    return;  // SEC: an error!!!!
  // Resolve style
  nsIStyleContext* kidStyleContext =
    aPresContext->ResolveStyleContextFor(mContent, this);
  NS_ASSERTION(nsnull!=kidStyleContext, "bad style context for kid.");
  mInnerTableFrame->SetStyleContext(kidStyleContext);
  mChildCount++;
  // Link child frame into the list of children
  mFirstChild = mInnerTableFrame;
  prevKidFrame = mInnerTableFrame;

  // now create the caption frames, prepending top captions and
  // appending bottom captions
  mCaptionFrames = new nsVoidArray();
  // create caption frames as needed
  nsIFrame *lastTopCaption = nsnull;
  for (PRInt32 kidIndex=0; /* nada */ ;kidIndex++) {
    nsIContent* caption = mContent->ChildAt(kidIndex);   // caption: REFCNT++
    if (nsnull == caption) {
      break;
    }
    const PRInt32 contentType = ((nsTableContent *)caption)->GetType();
    if (contentType==nsITableContent::kTableCaptionType)
    {
      nsIFrame *captionFrame=nsnull;
      frameCreated = nsTableCaptionFrame::NewFrame(&captionFrame, caption, 0, this);
      if (NS_OK!=frameCreated)
        return;  // SEC: an error!!!!
      // Resolve style
      nsIStyleContext* captionStyleContext =
        aPresContext->ResolveStyleContextFor(caption, this);
      NS_ASSERTION(nsnull!=captionStyleContext, "bad style context for caption.");
      nsStyleMolecule* captionStyle = 
        (nsStyleMolecule*)captionStyleContext->GetData(kStyleMoleculeSID);
      captionFrame->SetStyleContext(captionStyleContext);
      mChildCount++;
      // Link child frame into the list of children
      if (NS_STYLE_VERTICAL_ALIGN_BOTTOM==captionStyle->verticalAlign)
      { // bottom captions get added to the end of the outer frame child list
        prevKidFrame->SetNextSibling(captionFrame);
        prevKidFrame = captionFrame;
        // bottom captions get remembered in an instance variable for easy access later
        if (nsnull==mBottomCaptions)
        {
          mBottomCaptions = new nsVoidArray();
        }
        mBottomCaptions->AppendElement(captionFrame);
      }
      else
      {  // top captions get prepended to the outer frame child list
        if (nsnull == lastTopCaption) 
        { // our first top caption, therefore our first child
          mFirstChild = captionFrame;
          mFirstChild->SetNextSibling(mInnerTableFrame);
        }
        else 
        { // just another grub in the brood of top captions
          lastTopCaption->SetNextSibling(captionFrame);
          lastTopCaption = captionFrame;
        }
        lastTopCaption = captionFrame;
      }
      mCaptionFrames->AppendElement(captionFrame);
      NS_RELEASE(caption);                                 // caption: REFCNT--
    }
    else
    {
      NS_RELEASE(caption);                                 // caption: REFCNT--
      break;
    }
  }
}


nsIFrame::ReflowStatus
nsTableOuterFrame::ResizeReflowCaptionsPass1(nsIPresContext* aPresContext, nsStyleMolecule* aTableStyle)
{
  if (nsnull!=mCaptionFrames)
  {
    PRInt32 numCaptions = mCaptionFrames->Count();
    for (PRInt32 captionIndex = 0; captionIndex < numCaptions; captionIndex++)
    {
      nsSize maxElementSize(0,0);
      nsSize maxSize(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
      nsReflowMetrics desiredSize;
      nsTableCaptionFrame *captionFrame = (nsTableCaptionFrame *)mCaptionFrames->ElementAt(captionIndex);
      captionFrame->ResizeReflow(aPresContext, desiredSize, maxSize, &maxElementSize);
      if (mMinCaptionWidth<maxElementSize.width)
        mMinCaptionWidth = maxElementSize.width;
      if (mMaxCaptionWidth<desiredSize.width)
        mMaxCaptionWidth = desiredSize.width;
      captionFrame->VerticallyAlignChild(aPresContext);

    }
  }
  return frComplete;
}

nsIFrame::ReflowStatus
nsTableOuterFrame::ResizeReflowTopCaptionsPass2(nsIPresContext*  aPresContext,
                                                nsReflowMetrics& aDesiredSize,
                                                const nsSize&    aMaxSize,
                                                nsSize*          aMaxElementSize,
                                                nsStyleMolecule* aTableStyle)
{
  ReflowStatus result = frComplete;
  nscoord topCaptionY = 0;
  if (nsnull!=mCaptionFrames)
  {
    PRInt32 numCaptions = mCaptionFrames->Count();
    for (PRInt32 captionIndex = 0; captionIndex < numCaptions; captionIndex++)
    {
      // get the caption
      nsTableCaptionFrame *captionFrame = (nsTableCaptionFrame *)mCaptionFrames->ElementAt(captionIndex);

      // Resolve style
      nsIStyleContext* captionStyleContext = captionFrame->GetStyleContext(aPresContext);
      NS_ASSERTION(nsnull != captionStyleContext, "null style context for caption");
      nsStyleMolecule* captionStyle =
        (nsStyleMolecule*)captionStyleContext->GetData(kStyleMoleculeSID);
      NS_ASSERTION(nsnull != captionStyle, "null style molecule for caption");

      if (NS_STYLE_VERTICAL_ALIGN_BOTTOM==captionStyle->verticalAlign)
      { 
      }
      else
      { // top-align captions, the default
        // reflow the caption, skipping top captions after the first that doesn't fit
        if (frComplete==result)
        {
          nsReflowMetrics desiredSize;
          result = nsContainerFrame::ReflowChild(captionFrame, aPresContext, desiredSize, aMaxSize, nsnull);
          // place the caption
          captionFrame->SetRect(nsRect(0, topCaptionY, desiredSize.width, desiredSize.height));
          if (NS_UNCONSTRAINEDSIZE!=desiredSize.height)
            topCaptionY += desiredSize.height;
          else
            topCaptionY = NS_UNCONSTRAINEDSIZE;
          mInnerTableFrame->MoveTo(0, topCaptionY);
          if (0==captionFrame->GetIndexInParent())
          {
            SetFirstContentOffset(captionFrame);
            if (gsDebug) printf("OUTER: set first content offset to %d\n", GetFirstContentOffset()); //@@@
          }
        }
        // else continue on, so we can build the mBottomCaptions list
      }
    }
  }
  aDesiredSize.width = aMaxSize.width;
  aDesiredSize.height = topCaptionY;
  if (nsnull!=aMaxElementSize)
  { // SEC: this isn't right
    aMaxElementSize->width = aMaxSize.width;
    aMaxElementSize->height = topCaptionY;
  }
  return result;
}

nsIFrame::ReflowStatus
nsTableOuterFrame::ResizeReflowBottomCaptionsPass2(nsIPresContext*  aPresContext,
                                                   nsReflowMetrics& aDesiredSize,
                                                   const nsSize&    aMaxSize,
                                                   nsSize*          aMaxElementSize,
                                                   nsStyleMolecule* aTableStyle,
                                                   nscoord          aYOffset)
{
  ReflowStatus result = frComplete;
  nscoord bottomCaptionY = aYOffset;
  // for now, assume all captions are stacked vertically
  if (nsnull!=mBottomCaptions)
  {
    PRInt32 numCaptions = mBottomCaptions->Count();
    for (PRInt32 captionIndex = 0; captionIndex < numCaptions; captionIndex++)
    {
      // get the caption
      nsTableCaptionFrame *captionFrame = (nsTableCaptionFrame *)mBottomCaptions->ElementAt(captionIndex);

      // Resolve style
/*
      nsIStyleContext* captionStyleContext = captionFrame->GetStyleContext(aPresContext);
      NS_ASSERTION(nsnull != captionStyleContext, "null style context for caption");
      nsStyleMolecule* captionStyle =
        (nsStyleMolecule*)captionStyleContext->GetData(kStyleMoleculeSID);
      NS_ASSERTION(nsnull != captionStyle, "null style molecule for caption");
*/
      // reflow the caption
      nsReflowMetrics desiredSize;
      result = nsContainerFrame::ReflowChild(captionFrame, aPresContext, desiredSize, aMaxSize, nsnull);

      // place the caption
      nsRect rect = captionFrame->GetRect();
      rect.y = bottomCaptionY;
      rect.width=desiredSize.width;
      rect.height=desiredSize.height;
      captionFrame->SetRect(rect);
      if (NS_UNCONSTRAINEDSIZE!=desiredSize.height)
        bottomCaptionY += desiredSize.height;
      else
        bottomCaptionY = NS_UNCONSTRAINEDSIZE;
    }
  }
  aDesiredSize.width = aMaxSize.width;
  aDesiredSize.height = bottomCaptionY-aYOffset;
  if (nsnull!=aMaxElementSize)
  { // SEC: not right
    aMaxElementSize->width = aMaxSize.width;
    aMaxElementSize->height = bottomCaptionY-aYOffset;
  }
  return result;
}

/**
 * Sets the last content offset based on the last child frame. If the last
 * child is a pseudo frame then it sets mLastContentIsComplete to be the same
 * as the last child's mLastContentIsComplete
 */
/*
void nsTableOuterFrame::SetLastContentOffset(const nsIFrame* aLastChild)
{
  NS_PRECONDITION(nsnull != aLastChild, "bad argument");
  NS_PRECONDITION(aLastChild->GetGeometricParent() == this, "bad geometric parent");

  if (ChildIsPseudoFrame(aLastChild)) {
    nsContainerFrame* pseudoFrame = (nsContainerFrame*)aLastChild;
    mLastContentOffset = pseudoFrame->GetLastContentOffset();
    mLastContentIsComplete = pseudoFrame->GetLastContentIsComplete();
  } else {
    mLastContentOffset = aLastChild->GetIndexInParent();
  }
#ifdef NS_DEBUG
  if (mLastContentOffset < mFirstContentOffset) {
    nsIFrame* top = this;
    while (top->GetGeometricParent() != nsnull) {
      top = top->GetGeometricParent();
    }
    top->List();
  }
#endif
  // it is this next line that forces us to re-implement this method here
  //NS_ASSERTION(mLastContentOffset >= mFirstContentOffset, "unexpected content mapping");
}
*/

nsIFrame::ReflowStatus
nsTableOuterFrame::IncrementalReflow(nsIPresContext* aPresContext,
                                    nsReflowMetrics& aDesiredSize,
                                    const nsSize&    aMaxSize,
                                    nsReflowCommand& aReflowCommand)
{
  if (gsDebug == PR_TRUE) printf("nsTableOuterFrame::IncrementalReflow\n");
  // total hack for now, just some hard-coded values
  ResizeReflow(aPresContext, aDesiredSize, aMaxSize, nsnull);

  return frComplete;
}

nsIFrame* nsTableOuterFrame::CreateContinuingFrame(nsIPresContext* aPresContext,
                                                   nsIFrame*       aParent)
{
  nsTableOuterFrame* cf = new nsTableOuterFrame(mContent, mIndexInParent, aParent);
  PrepareContinuingFrame(aPresContext, aParent, cf);
  cf->SetFirstPassValid(PR_TRUE);
  printf("nsTableOuterFrame::CCF parent = %p, this=%p, cf=%p\n", aParent, this, cf);
  return cf;
}

void nsTableOuterFrame::PrepareContinuingFrame(nsIPresContext*    aPresContext,
                                               nsIFrame*          aParent,
                                               nsTableOuterFrame* aContFrame)
{
  // Append the continuing frame to the flow
  aContFrame->AppendToFlow(this);

  // Initialize it's content offsets. Note that we assume for now that
  // the continuingFrame will map the remainder of the content and
  // that therefore mLastContentIsComplete will be true.
  PRInt32 nextOffset;
  if (mChildCount > 0) {
    nextOffset = mLastContentOffset;
    if (mLastContentIsComplete) {
      nextOffset++;
    }
  } else {
    nextOffset = mFirstContentOffset;
  }

  aContFrame->SetFirstContentOffset(nextOffset);
  aContFrame->SetLastContentOffset(nextOffset);
  aContFrame->SetLastContentIsComplete(PR_TRUE);

  // Resolve style for the continuing frame and set its style context.
  // XXX presumptive
  nsIStyleContext* styleContext =
    aPresContext->ResolveStyleContextFor(mContent, aParent);
  aContFrame->SetStyleContext(styleContext);
  NS_RELEASE(styleContext);
}

void nsTableOuterFrame::VerifyTree() const
{
#ifdef NS_DEBUG
  
#endif 
}

void nsTableOuterFrame::CreateInnerTableFrame(nsIPresContext* aPresContext)
{
  // Do we have a prev-in-flow?
  if (nsnull == mPrevInFlow) {
    // No, create a column pseudo frame
    mInnerTableFrame = new nsTableFrame(mContent, mIndexInParent, this);
    mChildCount++;

    // Resolve style and set the style context
    nsIStyleContext* styleContext =
      aPresContext->ResolveStyleContextFor(mContent, this);
    mInnerTableFrame->SetStyleContext(styleContext);
    NS_RELEASE(styleContext);
  } else {
    nsTableOuterFrame*  prevOuterTable = (nsTableOuterFrame*)mPrevInFlow;

    nsIFrame* prevInnerTable = prevOuterTable->mInnerTableFrame;

    // Create a continuing column
    mInnerTableFrame = (nsTableFrame *)prevInnerTable->GetNextInFlow();
    if (nsnull==mInnerTableFrame)
    {
      mInnerTableFrame = (nsTableFrame *)prevInnerTable->CreateContinuingFrame(aPresContext, this);
      mChildCount++;
    }
  }
}

/* ----- static methods ----- */

nsresult nsTableOuterFrame::NewFrame(nsIFrame** aInstancePtrResult,
                                    nsIContent* aContent,
                                    PRInt32     aIndexInParent,
                                    nsIFrame*   aParent)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIFrame* it = new nsTableOuterFrame(aContent, aIndexInParent, aParent);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aInstancePtrResult = it;
  return NS_OK;
}
