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
#include "nsIPtr.h"

#ifdef NS_DEBUG
static PRBool gsDebug = PR_FALSE;
//#define NOISY
//#define NOISY_FLOW
//#define NOISY_MARGINS
#else
static const PRBool gsDebug = PR_FALSE;
#endif

static NS_DEFINE_IID(kStyleTextSID, NS_STYLETEXT_SID);
static NS_DEFINE_IID(kStyleSpacingSID, NS_STYLESPACING_SID);
static NS_DEFINE_IID(kITableContentIID, NS_ITABLECONTENT_IID);

NS_DEF_PTR(nsIStyleContext);
NS_DEF_PTR(nsIContent);

struct OuterTableReflowState {

  // The presentation context
  nsIPresContext *pc;

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
                        const nsSize&    aMaxSize)
  {
    pc = aPresContext;
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

NS_METHOD nsTableOuterFrame::Paint(nsIPresContext& aPresContext,
                                   nsIRenderingContext& aRenderingContext,
                                   const nsRect& aDirtyRect)
{
  // for debug...
  if (nsIFrame::GetShowFrameBorders()) {
    aRenderingContext.SetColor(NS_RGB(255,0,0));
    aRenderingContext.DrawRect(0, 0, mRect.width, mRect.height);
  }

  PaintChildren(aPresContext, aRenderingContext, aDirtyRect);
  return NS_OK;
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
NS_METHOD nsTableOuterFrame::ResizeReflow(nsIPresContext* aPresContext,
                                          nsReflowMetrics& aDesiredSize,
                                          const nsSize& aMaxSize,
                                          nsSize* aMaxElementSize,
                                          ReflowStatus& aStatus)
{
  if (PR_TRUE==gsDebug) 
    printf ("***table outer frame reflow \t\t%p\n", this);
  if (PR_TRUE==gsDebug)
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

  PRBool  reflowMappedOK = PR_TRUE;

  aStatus = frComplete;
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
  if (nsnull==mFirstChild  ||  nsnull==mInnerTableFrame) {
    //ERROR!
    aStatus = frComplete;
    return NS_OK;
  }
  
  OuterTableReflowState state(aPresContext, aMaxSize);

  // lay out captions pass 1, if necessary
  if (PR_FALSE==IsFirstPassValid())
  {
    mFirstPassValid = PR_TRUE;
    aStatus = ResizeReflowCaptionsPass1(aPresContext);

  }

  // lay out inner table, if required
  if (PR_FALSE==mInnerTableFrame->IsFirstPassValid())
  { // we treat the table as if we've never seen the layout data before
    mInnerTableFrame->SetReflowPass(nsTableFrame::kPASS_FIRST);
    aStatus = mInnerTableFrame->ResizeReflowPass1(aPresContext, aDesiredSize, aMaxSize, 
                                                  &innerTableMaxElementSize);
  
#ifdef NOISY_MARGINS
    nsIContent* content = nsnull;
    mInnerTableFrame->GetContent(content);
    nsTablePart *table = (nsTablePart*)content;
    if (table != nsnull)
      table->DumpCellMap();
    mInnerTableFrame->RecalcLayoutData();
    mInnerTableFrame->ListColumnLayoutData(stdout,1);
#endif

  }
  mInnerTableFrame->SetReflowPass(nsTableFrame::kPASS_SECOND);
  // assign table width info only if the inner table frame is a first-in-flow
  nsIFrame* prevInFlow;

  mInnerTableFrame->GetPrevInFlow(prevInFlow);
  if (nsnull==prevInFlow)
  {
    // assign column widths, and assign aMaxElementSize->width
    mInnerTableFrame->BalanceColumnWidths(aPresContext, aMaxSize, aMaxElementSize);
    // assign table width
    mInnerTableFrame->SetTableWidth(aPresContext);
  }
  // inner table max is now the computed width and assigned  height
  nsSize  innerTableSize;

  mInnerTableFrame->GetSize(innerTableSize);
  state.innerTableMaxSize.width = innerTableSize.width;
  state.innerTableMaxSize.height = aMaxSize.height; 

  // Reflow the child frames
  if (nsnull != mFirstChild) {
    reflowMappedOK = ReflowMappedChildren(aPresContext, state, aMaxElementSize);
    if (PR_FALSE == reflowMappedOK) {
      aStatus = frNotComplete;
    }
  }

  // Did we successfully relow our mapped children?
  if (PR_TRUE == reflowMappedOK) {
    // Any space left?
    if (state.availSize.height <= 0) {
      // No space left. Don't try to pull-up children or reflow unmapped
      if (NextChildOffset() < mContent->ChildCount()) {
        aStatus = frNotComplete;
      }
    } else if (NextChildOffset() < mContent->ChildCount()) {
      // Try and pull-up some children from a next-in-flow
      if (PullUpChildren(aPresContext, state, aMaxElementSize)) {
        // If we still have unmapped children then create some new frames
        NS_ABORT(); // huge error for tables!
      } else {
        // We were unable to pull-up all the existing frames from the next in flow
        aStatus = frNotComplete;
      }
    }
  }

  if (frComplete == aStatus) {
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
  /* if we're incomplete, take up all the remaining height so we don't waste time
   * trying to lay out in a slot that we know isn't tall enough to fit our minimum.
   * otherwise, we're as tall as our kids want us to be */
  if (frNotComplete == aStatus)
    aDesiredSize.height = aMaxSize.height;
  else 
    aDesiredSize.height = state.y;

  if (gsDebug==PR_TRUE) 
  {
    if (nsnull!=aMaxElementSize)
      printf("Outer frame Reflow complete, returning %s with aDesiredSize = %d,%d and aMaxElementSize=%d,%d\n",
              aStatus==frComplete ? "Complete" : "Not Complete",
              aDesiredSize.width, aDesiredSize.height, 
              aMaxElementSize->width, aMaxElementSize->height);
    else
      printf("Outer frame Reflow complete, returning %s with aDesiredSize = %d,%d and NSNULL aMaxElementSize\n",
              aStatus==frComplete ? "Complete" : "Not Complete",
              aDesiredSize.width, aDesiredSize.height);
  }

#ifdef NS_DEBUG
  // replace with a check that does not assume linear placement of children
  // PostReflowCheck(status);
#endif
  // REMOVE ME!
  /*
  nsIFrame *next = this;
  nsIFrame *root=nsnull;
  while (nsnull!=next)
  {
	  root = next;
	  next = next->GetGeometricParent();
  }
  root->List();
  */
  // end REMOVE ME!

  return NS_OK;

}

// Collapse child's top margin with previous bottom margin
nscoord nsTableOuterFrame::GetTopMarginFor(nsIPresContext*        aCX,
                                           OuterTableReflowState& aState,
                                           nsStyleSpacing* aKidSpacing)
{
  nscoord margin;
  nscoord maxNegTopMargin = 0;
  nscoord maxPosTopMargin = 0;
  if ((margin = aKidSpacing->mMargin.top) < 0) {
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
void nsTableOuterFrame::PlaceChild( OuterTableReflowState& aState,
                                    nsIFrame*          aKidFrame,
                                    const nsRect&      aKidRect,
                                    nsSize*            aMaxElementSize,
                                    nsSize&            aKidMaxElementSize)
{
  if (PR_TRUE==gsDebug) 
    printf ("outer table place child: %p with aKidRect %d %d %d %d\n", 
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
  PRBool    originalLastContentIsComplete = mLastContentIsComplete;

  nsSize    kidMaxElementSize;
  nsSize*   pKidMaxElementSize = (nsnull != aMaxElementSize) ? &kidMaxElementSize : nsnull;
  PRBool    result = PR_TRUE;
  aState.availSize.width = aState.innerTableMaxSize.width;
  for (nsIFrame* kidFrame = mFirstChild; nsnull != kidFrame; ) {
    nsReflowMetrics         kidSize;
    nsIFrame::ReflowStatus  status;

    SetReflowState(aState, kidFrame);

    if (PR_TRUE==gsDebug) printf ("ReflowMappedChildren: %p with state = %s\n", 
                                  kidFrame, aState.processingCaption?"caption":"inner");

    // Get top margin for this kid
    nsIStyleContextPtr kidSC;

    kidFrame->GetStyleContext(aPresContext, kidSC.AssignRef());
    nsStyleSpacing* kidSpacing =
      (nsStyleSpacing*)kidSC->GetData(kStyleSpacingSID);
    nscoord topMargin = GetTopMarginFor(aPresContext, aState, kidSpacing);
    nscoord bottomMargin = kidSpacing->mMargin.bottom;

    // Figure out the amount of available size for the child (subtract
    // off the top margin we are going to apply to it)
    if (PR_FALSE == aState.unconstrainedHeight) {
      aState.availSize.height -= topMargin;
    }
    // Subtract off for left and right margin
    if (PR_FALSE == aState.unconstrainedWidth) {
      aState.availSize.width -= kidSpacing->mMargin.left + kidSpacing->mMargin.right;
    }

    // Only skip the reflow if this is not our first child and we are
    // out of space.
    if ((kidFrame == mFirstChild) || (aState.availSize.height > 0)) {
      // Reflow the child into the available space
      status = ReflowChild(kidFrame, aPresContext, kidSize,
                           aState.availSize, pKidMaxElementSize,
                           aState);
    }

    // Did the child fit?
    if ((kidSize.height > aState.availSize.height) && (kidFrame != mFirstChild)) {
      // The child is too tall to fit in the available space, and it's
      // not our first child

      // Since we are giving the next-in-flow our last child, we
      // give it our original mLastContentIsComplete too (in case we
      // are pushing into an empty next-in-flow)
      PushChildren(kidFrame, prevKidFrame, originalLastContentIsComplete);
      SetLastContentOffset(prevKidFrame);

      result = PR_FALSE;
      break;
    }

    // Place the child after taking into account it's margin
    aState.y += topMargin;
    nsRect kidRect (0, 0, kidSize.width, kidSize.height);
    kidRect.x += kidSpacing->mMargin.left;
    kidRect.y += aState.y;
    PlaceChild(aState, kidFrame, kidRect, aMaxElementSize, kidMaxElementSize);
    if (bottomMargin < 0) {
      aState.prevMaxNegBottomMargin = -bottomMargin;
    } else {
      aState.prevMaxPosBottomMargin = bottomMargin;
    }
    childCount++;

    // Remember where we just were in case we end up pushing children
    prevKidFrame = kidFrame;

    // Update mLastContentIsComplete now that this kid fits
    mLastContentIsComplete = PRBool(status == frComplete);

    if (frNotComplete == status) {
      // No, the child isn't complete
      nsIFrame* kidNextInFlow;

      kidFrame->GetNextInFlow(kidNextInFlow);
      PRBool lastContentIsComplete = mLastContentIsComplete;
      if (nsnull == kidNextInFlow) {
        // The child doesn't have a next-in-flow so create a continuing
        // frame. This hooks the child into the flow
        nsIFrame* continuingFrame;
         
        kidFrame->CreateContinuingFrame(aPresContext, this, continuingFrame);
        NS_ASSERTION(nsnull != continuingFrame, "frame creation failed");

        // Add the continuing frame to the sibling list
        nsIFrame* nextSib;
         
        kidFrame->GetNextSibling(nextSib);
        continuingFrame->SetNextSibling(nextSib);
        kidFrame->SetNextSibling(continuingFrame);
        if (nsnull == nextSib) {
          // Assume that the continuation frame we just created is
          // complete, for now. It will get reflowed by our
          // next-in-flow (we are going to push it now)
          lastContentIsComplete = PR_TRUE;
        }
      }

      // We've used up all of our available space so push the remaining
      // children to the next-in-flow
      nsIFrame* nextSibling;
       
      kidFrame->GetNextSibling(nextSibling);
      if (nsnull != nextSibling) {
        PushChildren(nextSibling, kidFrame, lastContentIsComplete);
        /* debug */
        /*
        printf ("having just pushed children, here is the frame hierarchy.\n");
        nsIFrame *p;
        GetGeometricParent(p);
        nsIFrame *root;
        while (nsnull!=p)
        {
          root = p;
          p->GetGeometricParent(p);
        }
        root->List();
        fflush(stdout);
        */
        /* end debug */
        SetLastContentOffset(prevKidFrame);
      }
      result = PR_FALSE;
      break;
    }

    // Get the next child
    kidFrame->GetNextSibling(kidFrame);

    // XXX talk with troy about checking for available space here
  }

  // Update the child count
  mChildCount = childCount;
  NS_POSTCONDITION(LengthOf(mFirstChild) == mChildCount, "bad child count");

  // Set the last content offset based on the last child we mapped.
  NS_ASSERTION(IsLastChild(prevKidFrame), "unexpected last child");
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
PRBool nsTableOuterFrame::PullUpChildren( nsIPresContext*      aPresContext,
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
  nsTableOuterFrame* nextInFlow = (nsTableOuterFrame*)mNextInFlow;
  nsSize         kidMaxElementSize;
  nsSize*        pKidMaxElementSize = (nsnull != aMaxElementSize) ? &kidMaxElementSize : nsnull;
#ifdef NS_DEBUG
  PRInt32        kidIndex = NextChildOffset();
#endif
  nsIFrame*      prevKidFrame;
   
  LastChild(prevKidFrame);

  // This will hold the prevKidFrame's mLastContentIsComplete
  // status. If we have to push the frame that follows prevKidFrame
  // then this will become our mLastContentIsComplete state. Since
  // prevKidFrame is initially our last frame, it's completion status
  // is our mLastContentIsComplete value.
  PRBool        prevLastContentIsComplete = mLastContentIsComplete;

  PRBool        result = PR_TRUE;

  while (nsnull != nextInFlow) {
    nsReflowMetrics kidSize;
    ReflowStatus    status;

    // Get the next child
    nsIFrame* kidFrame = nextInFlow->mFirstChild;

    // Any more child frames?
    if (nsnull == kidFrame) {
      // No. Any frames on its overflow list?
      if (nsnull != nextInFlow->mOverflowList) {
        // Move the overflow list to become the child list
        //NS_ABORT();
        nextInFlow->AppendChildren(nextInFlow->mOverflowList);
        nextInFlow->mOverflowList = nsnull;
        kidFrame = nextInFlow->mFirstChild;
      } else {
        // We've pulled up all the children, so move to the next-in-flow.
        nextInFlow->GetNextInFlow((nsIFrame*&)nextInFlow);
        continue;
      }
    }

    // See if the child fits in the available space. If it fits or
    // it's splittable then reflow it. The reason we can't just move
    // it is that we still need ascent/descent information
    nsSize          kidFrameSize;
    SplittableType  kidIsSplittable;

    kidFrame->GetSize(kidFrameSize);
    kidFrame->IsSplittable(kidIsSplittable);

    if ((kidFrameSize.width  > aState.availSize.height) &&
        (kidIsSplittable == frNotSplittable)) {
      result = PR_FALSE;
      mLastContentIsComplete = prevLastContentIsComplete;
      break;
    }
    status = ReflowChild(kidFrame, aPresContext, kidSize, aState.availSize,
                         pKidMaxElementSize, aState);

    // Did the child fit?
    if ((kidSize.height > aState.availSize.height) && (nsnull != mFirstChild)) {
      // The child is too wide to fit in the available space, and it's
      // not our first child
      result = PR_FALSE;
      mLastContentIsComplete = prevLastContentIsComplete;
      break;
    }

    // Place the child after taking into account it's margin
//    aState.y += topMargin;
    nsRect kidRect (0, 0, kidSize.width, kidSize.height);
//    kidRect.x += kidMol->margin.left;
    kidRect.y += aState.y;
    PlaceChild(aState, kidFrame, kidRect, aMaxElementSize, *pKidMaxElementSize);

    // Remove the frame from its current parent
    kidFrame->GetNextSibling(nextInFlow->mFirstChild);
    nextInFlow->mChildCount--;
    // Update the next-in-flows first content offset
    if (nsnull != nextInFlow->mFirstChild) {
      nextInFlow->SetFirstContentOffset(nextInFlow->mFirstChild);
    }

    // Link the frame into our list of children
    kidFrame->SetGeometricParent(this);
    nsIFrame* kidContentParent;

    kidFrame->GetContentParent(kidContentParent);
    if (nextInFlow == kidContentParent) {
      kidFrame->SetContentParent(this);
    }
    if (nsnull == prevKidFrame) {
      mFirstChild = kidFrame;
      SetFirstContentOffset(kidFrame);
    } else {
      prevKidFrame->SetNextSibling(kidFrame);
    }
    kidFrame->SetNextSibling(nsnull);
    mChildCount++;

    // Remember where we just were in case we end up pushing children
    prevKidFrame = kidFrame;
    prevLastContentIsComplete = mLastContentIsComplete;

    // Is the child we just pulled up complete?
    mLastContentIsComplete = PRBool(status == frComplete);
    if (frNotComplete == status) {
      // No the child isn't complete
      nsIFrame* kidNextInFlow;
       
      kidFrame->GetNextInFlow(kidNextInFlow);
      if (nsnull == kidNextInFlow) {
        // The child doesn't have a next-in-flow so create a
        // continuing frame. The creation appends it to the flow and
        // prepares it for reflow.
        nsIFrame* continuingFrame;
         
        kidFrame->CreateContinuingFrame(aPresContext, this, continuingFrame);
        NS_ASSERTION(nsnull != continuingFrame, "frame creation failed");

        // Add the continuing frame to our sibling list and then push
        // it to the next-in-flow. This ensures the next-in-flow's
        // content offsets and child count are set properly. Note that
        // we can safely assume that the continuation is complete so
        // we pass PR_TRUE into PushChidren in case our next-in-flow
        // was just drained and now needs to know it's
        // mLastContentIsComplete state.
        kidFrame->SetNextSibling(continuingFrame);

        PushChildren(continuingFrame, kidFrame, PR_TRUE);

        // After we push the continuation frame we don't need to fuss
        // with mLastContentIsComplete beause the continuation frame
        // is no longer on *our* list.
      }

      // If the child isn't complete then it means that we've used up
      // all of our available space.
      result = PR_FALSE;
      break;
    }
  }

  // Update our last content offset
  if (nsnull != prevKidFrame) {
    NS_ASSERTION(IsLastChild(prevKidFrame), "bad last child");
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
        nextInFlow->GetNextInFlow((nsIFrame*&)nextInFlow);
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
  nsIContentPtr kid;
   
  aKidFrame->GetContent(kid.AssignRef());                             // kid: REFCNT++
  nsITableContent *tableContentInterface = nsnull;
  kid->QueryInterface(kITableContentIID, (void**)&tableContentInterface);// tableContentInterface: REFCNT++
  if (nsnull!=tableContentInterface)
  {
    aState.processingCaption = PR_TRUE;
    NS_RELEASE(tableContentInterface);                                   // tableContentInterface: REFCNT--
  }
  else
    aState.processingCaption = PR_FALSE;
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
  if (PR_TRUE==aState.processingCaption)
  { // it's a caption, find out if it's top or bottom
    // Resolve style
    nsIStyleContextPtr captionStyleContext;

    aKidFrame->GetStyleContext(aPresContext, captionStyleContext.AssignRef());
    NS_ASSERTION(captionStyleContext.IsNotNull(), "null style context for caption");
    nsStyleText* captionStyle =
      (nsStyleText*)captionStyleContext->GetData(kStyleTextSID);
    NS_ASSERTION(nsnull != captionStyle, "null style molecule for caption");
    if (NS_STYLE_VERTICAL_ALIGN_BOTTOM==captionStyle->mVerticalAlignFlags)
    {
      if (PR_TRUE==gsDebug) printf("reflowChild called with a bottom caption\n");
      status = ResizeReflowBottomCaptionsPass2(aPresContext, aDesiredSize,
                                               aMaxSize, aMaxElementSize,
                                               aState.y);
    }
    else
    {
      if (PR_TRUE==gsDebug) printf("reflowChild called with a top caption\n");
      status = ResizeReflowTopCaptionsPass2(aPresContext, aDesiredSize,
                                            aMaxSize, aMaxElementSize);
    }
  }
  else
  {
    if (PR_TRUE==gsDebug) printf("reflowChild called with a table body\n");
    status = ((nsTableFrame*)aKidFrame)->ResizeReflowPass2(aPresContext, aDesiredSize, aState.innerTableMaxSize, 
                                                           aMaxElementSize,
                                                           mMinCaptionWidth, mMaxCaptionWidth);
  }
  if (PR_TRUE==gsDebug)
  {
  }

  if (frComplete == status) {
    nsIFrame* kidNextInFlow;
     
    aKidFrame->GetNextInFlow(kidNextInFlow);
    if (nsnull != kidNextInFlow) {
      // Remove all of the childs next-in-flows. Make sure that we ask
      // the right parent to do the removal (it's possible that the
      // parent is not this because we are executing pullup code)
      nsTableOuterFrame* parent;
       
      aKidFrame->GetGeometricParent((nsIFrame*&)parent);
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
  nsIStyleContextPtr kidStyleContext =
    aPresContext->ResolveStyleContextFor(mContent, this);
  NS_ASSERTION(kidStyleContext.IsNotNull(), "bad style context for kid.");
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
    nsIContentPtr caption = mContent->ChildAt(kidIndex);   // caption: REFCNT++
    if (caption.IsNull()) {
      break;
    }
    const PRInt32 contentType = ((nsTableContent *)(nsIContent*)caption)->GetType();
    if (contentType==nsITableContent::kTableCaptionType)
    {
      nsIFrame *captionFrame=nsnull;
      frameCreated = nsTableCaptionFrame::NewFrame(&captionFrame, caption, 0, this);
      if (NS_OK!=frameCreated)
        return;  // SEC: an error!!!!
      // Resolve style
      nsIStyleContextPtr captionStyleContext =
        aPresContext->ResolveStyleContextFor(caption, this);
      NS_ASSERTION(captionStyleContext.IsNotNull(), "bad style context for caption.");
      nsStyleText* captionStyle = 
        (nsStyleText*)captionStyleContext->GetData(kStyleTextSID);
      captionFrame->SetStyleContext(captionStyleContext);
      mChildCount++;
      // Link child frame into the list of children
      if (NS_STYLE_VERTICAL_ALIGN_BOTTOM==captionStyle->mVerticalAlignFlags)
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
    }
    else
    {
      break;
    }
  }
}


nsIFrame::ReflowStatus
nsTableOuterFrame::ResizeReflowCaptionsPass1(nsIPresContext* aPresContext)
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
      ReflowStatus  status;
      captionFrame->ResizeReflow(aPresContext, desiredSize, maxSize, &maxElementSize, status);
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
                                                nsSize*          aMaxElementSize)
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
      nsIStyleContextPtr captionStyleContext;
       
      captionFrame->GetStyleContext(aPresContext, captionStyleContext.AssignRef());
      NS_ASSERTION(captionStyleContext.IsNotNull(), "null style context for caption");
      nsStyleText* captionStyle =
        (nsStyleText*)captionStyleContext->GetData(kStyleTextSID);
      NS_ASSERTION(nsnull != captionStyle, "null style molecule for caption");

      if (NS_STYLE_VERTICAL_ALIGN_BOTTOM==captionStyle->mVerticalAlignFlags)
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
          PRInt32 captionIndexInParent;

          captionFrame->GetIndexInParent(captionIndexInParent);
          if (0==captionIndexInParent)
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
      nsIStyleContextPtr captionStyleContext = captionFrame->GetStyleContext(aPresContext);
      NS_ASSERTION(nsnull != captionStyleContext, "null style context for caption");
      nsStyleMolecule* captionStyle =
        (nsStyleMolecule*)captionStyleContext->GetData(kStyleMoleculeSID);
      NS_ASSERTION(nsnull != captionStyle, "null style molecule for caption");
*/
      // reflow the caption
      nsReflowMetrics desiredSize;
      result = nsContainerFrame::ReflowChild(captionFrame, aPresContext, desiredSize, aMaxSize, nsnull);

      // place the caption
      nsRect rect;
       
      captionFrame->GetRect(rect);
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

NS_METHOD
nsTableOuterFrame::IncrementalReflow(nsIPresContext* aPresContext,
                                    nsReflowMetrics& aDesiredSize,
                                    const nsSize&    aMaxSize,
                                    nsReflowCommand& aReflowCommand,
                                     ReflowStatus&   aStatus)
{
  if (PR_TRUE==gsDebug) printf("nsTableOuterFrame::IncrementalReflow\n");
  // total hack for now, just some hard-coded values
  ResizeReflow(aPresContext, aDesiredSize, aMaxSize, nsnull, aStatus);
  return NS_OK;
}

NS_METHOD nsTableOuterFrame::CreateContinuingFrame(nsIPresContext* aPresContext,
                                                   nsIFrame*       aParent,
                                                   nsIFrame*&      aContinuingFrame)
{
  nsTableOuterFrame* cf = new nsTableOuterFrame(mContent, mIndexInParent, aParent);
  PrepareContinuingFrame(aPresContext, aParent, cf);
  cf->SetFirstPassValid(PR_TRUE);
  if (PR_TRUE==gsDebug)
    printf("nsTableOuterFrame::CCF parent = %p, this=%p, cf=%p\n", aParent, this, cf);
  aContinuingFrame = cf;
  return NS_OK;
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
  nsIStyleContextPtr styleContext =
    aPresContext->ResolveStyleContextFor(mContent, aParent);
  aContFrame->SetStyleContext(styleContext);
}

NS_METHOD nsTableOuterFrame::VerifyTree() const
{
#ifdef NS_DEBUG
  
#endif 
  return NS_OK;
}

/**
 * Remove and delete aChild's next-in-flow(s). Updates the sibling and flow
 * pointers.
 *
 * Updates the child count and content offsets of all containers that are
 * affected
 *
 * Overloaded here because nsContainerFrame makes assumptions about pseudo-frames
 * that are not true for tables.
 *
 * @param   aChild child this child's next-in-flow
 * @return  PR_TRUE if successful and PR_FALSE otherwise
 */
PRBool nsTableOuterFrame::DeleteChildsNextInFlow(nsIFrame* aChild)
{
  NS_PRECONDITION(IsChild(aChild), "bad geometric parent");

  nsIFrame* nextInFlow;
   
  aChild->GetNextInFlow(nextInFlow);

  NS_PRECONDITION(nsnull != nextInFlow, "null next-in-flow");
  nsTableOuterFrame* parent;
   
  nextInFlow->GetGeometricParent((nsIFrame*&)parent);

  // If the next-in-flow has a next-in-flow then delete it too (and
  // delete it first).
  nsIFrame* nextNextInFlow;

  nextInFlow->GetNextInFlow(nextNextInFlow);
  if (nsnull != nextNextInFlow) {
    parent->DeleteChildsNextInFlow(nextInFlow);
  }

#ifdef NS_DEBUG
  PRInt32   childCount;
  nsIFrame* firstChild;

  nextInFlow->ChildCount(childCount);
  nextInFlow->FirstChild(firstChild);

  NS_ASSERTION(childCount == 0, "deleting !empty next-in-flow");

  NS_ASSERTION((0 == childCount) && (nsnull == firstChild), "deleting !empty next-in-flow");
#endif

  // Disconnect the next-in-flow from the flow list
  nextInFlow->BreakFromPrevFlow();

  // Take the next-in-flow out of the parent's child list
  if (parent->mFirstChild == nextInFlow) {
    nextInFlow->GetNextSibling(parent->mFirstChild);
    if (nsnull != parent->mFirstChild) {
      parent->SetFirstContentOffset(parent->mFirstChild);
      if (parent->IsPseudoFrame()) {
        parent->PropagateContentOffsets();
      }
    }

    // When a parent loses it's last child and that last child is a
    // pseudo-frame then the parent's content offsets are now wrong.
    // However, we know that the parent will eventually be reflowed
    // in one of two ways: it will either get a chance to pullup
    // children or it will be deleted because it's prev-in-flow
    // (e.g. this) is complete. In either case, the content offsets
    // will be repaired.

  } else {
    nsIFrame* nextSibling;

    // Because the next-in-flow is not the first child of the parent
    // we know that it shares a parent with aChild. Therefore, we need
    // to capture the next-in-flow's next sibling (in case the
    // next-in-flow is the last next-in-flow for aChild AND the
    // next-in-flow is not the last child in parent)
    NS_ASSERTION(parent->IsChild(aChild), "screwy flow");
    aChild->GetNextSibling(nextSibling);
    NS_ASSERTION(nextSibling == nextInFlow, "unexpected sibling");

    nextInFlow->GetNextSibling(nextSibling);
    aChild->SetNextSibling(nextSibling);
  }

  // Delete the next-in-flow frame and adjust it's parent's child count
  nextInFlow->DeleteFrame();
  parent->mChildCount--;

#ifdef NS_DEBUG
  aChild->GetNextInFlow(nextInFlow);
  NS_POSTCONDITION(nsnull == nextInFlow, "non null next-in-flow");
#endif

  return PR_TRUE;
}


void nsTableOuterFrame::CreateInnerTableFrame(nsIPresContext* aPresContext)
{
  // Do we have a prev-in-flow?
  if (nsnull == mPrevInFlow) {
    // No, create a column pseudo frame
    mInnerTableFrame = new nsTableFrame(mContent, mIndexInParent, this);
    mChildCount++;

    // Resolve style and set the style context
    nsIStyleContextPtr styleContext =
      aPresContext->ResolveStyleContextFor(mContent, this);
    mInnerTableFrame->SetStyleContext(styleContext);
  } else {
    nsTableOuterFrame*  prevOuterTable = (nsTableOuterFrame*)mPrevInFlow;

    nsIFrame* prevInnerTable = prevOuterTable->mInnerTableFrame;

    // Create a continuing column
    prevInnerTable->GetNextInFlow((nsIFrame*&)mInnerTableFrame);
    if (nsnull==mInnerTableFrame)
    {
      nsIFrame* continuingFrame;

      prevInnerTable->CreateContinuingFrame(aPresContext, this, continuingFrame);
      mInnerTableFrame = (nsTableFrame*)continuingFrame;
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
