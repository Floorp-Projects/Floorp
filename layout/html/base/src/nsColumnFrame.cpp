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
#include "nsColumnFrame.h"
#include "nsIContent.h"
#include "nsIContentDelegate.h"
#include "nsBlockFrame.h"
#include "nsReflowCommand.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIViewManager.h"
#include "nsIDeviceContext.h"
#include "nsISpaceManager.h"
#include "nsIPtr.h"
#include "nsAbsoluteFrame.h"

#ifdef NS_DEBUG
#undef NOISY
#undef NOISY_FLOW
#else
#undef NOISY
#undef NOISY_FLOW
#endif

static NS_DEFINE_IID(kIRunaroundIID, NS_IRUNAROUND_IID);
static NS_DEFINE_IID(kStyleSpacingSID, NS_STYLESPACING_SID);
static NS_DEFINE_IID(kStyleDisplaySID, NS_STYLEDISPLAY_SID);
static NS_DEFINE_IID(kStylePositionSID, NS_STYLEPOSITION_SID);

NS_DEF_PTR(nsIStyleContext);
NS_DEF_PTR(nsIContent);

struct ColumnReflowState {
  // The space manager to use
  nsISpaceManager* spaceManager;

  // The body's available size (computed from the body's parent)
  nsSize availSize;

  // The running max child x-most (used to size our frame when done).
  // This includes the child's right margin.
  nscoord kidXMost;

  // Running y-offset
  nscoord y;

  // Margin tracking information
  nscoord prevMaxPosBottomMargin;
  nscoord prevMaxNegBottomMargin;

  // Flags for whether the max size is unconstrained
  PRBool  unconstrainedWidth;
  PRBool  unconstrainedHeight;

  ColumnReflowState(nsIPresContext*  aPresContext,
                    nsISpaceManager* aSpaceManager,
                    const nsSize&    aMaxSize)
  {
    spaceManager = aSpaceManager;
    availSize.width = aMaxSize.width;
    availSize.height = aMaxSize.height;
    kidXMost = 0;
    prevMaxPosBottomMargin = 0;
    prevMaxNegBottomMargin = 0;
    unconstrainedWidth = PRBool(aMaxSize.width == NS_UNCONSTRAINEDSIZE);
    unconstrainedHeight = PRBool(aMaxSize.height == NS_UNCONSTRAINEDSIZE);

    // Since we're always used as a pseudo frame we have no border/padding
    y = 0;
  }

  ~ColumnReflowState() {
  }
};

ColumnFrame::ColumnFrame(nsIContent* aContent,
                         PRInt32     aIndexInParent,
                         nsIFrame*   aParentFrame)
  : nsHTMLContainerFrame(aContent, aIndexInParent, aParentFrame)
{
  NS_PRECONDITION(IsPseudoFrame(), "can only be used as a pseudo frame");
}

ColumnFrame::~ColumnFrame()
{
}

nsresult ColumnFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(0 != aInstancePtr, "null ptr");
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIRunaroundIID)) {
    *aInstancePtr = (void**)((nsIRunaround*)this);
    return NS_OK;
  }
  return nsHTMLContainerFrame::QueryInterface(aIID, aInstancePtr);
}

// Collapse child's top margin with previous bottom margin
nscoord ColumnFrame::GetTopMarginFor(nsIPresContext*    aCX,
                                     ColumnReflowState& aState,
                                     nsIFrame*          aKidFrame,
                                     nsStyleSpacing*    aKidSpacing)
{
  // Does the frame have a prev-in-flow?
  nsIFrame* kidPrevInFlow;

  aKidFrame->GetPrevInFlow(kidPrevInFlow);

  if (nsnull == kidPrevInFlow) {
    nscoord margin;
    nscoord maxNegTopMargin = 0;
    nscoord maxPosTopMargin = 0;

    // XXX Style system should be zero'ing out margins for pseudo frames...
    if (!ChildIsPseudoFrame(aKidFrame)) {
      if ((margin = aKidSpacing->mMargin.top) < 0) {
        maxNegTopMargin = -margin;
      } else {
        maxPosTopMargin = margin;
      }
    }
  
    nscoord maxPos = PR_MAX(aState.prevMaxPosBottomMargin, maxPosTopMargin);
    nscoord maxNeg = PR_MAX(aState.prevMaxNegBottomMargin, maxNegTopMargin);
    margin = maxPos - maxNeg;
  
    return margin;
  } else {
    return 0;
  }
}

// Position and size aKidFrame and update our reflow state. The origin of
// aKidRect is relative to the upper-left origin of our frame, and includes
// any left/top margin.
void ColumnFrame::PlaceChild(nsIPresContext*    aPresContext,
                             ColumnReflowState& aState,
                             nsIFrame*          aKidFrame,
                             nsStyleSpacing*    aKidSpacing,
                             const nsRect&      aKidRect,
                             nsSize*            aMaxElementSize,
                             nsSize&            aKidMaxElementSize)
{
  // Place and size the child
  aKidFrame->SetRect(aKidRect);

  // Adjust the running y-offset
  aState.y += aKidRect.height;
  aState.spaceManager->Translate(0, aKidRect.height);

  // Update the x-most
  nscoord xMost = aKidRect.XMost() + aKidSpacing->mMargin.right;
  if (xMost > aState.kidXMost) {
    aState.kidXMost = xMost;
  }

  // If our height is constrained then update the available height
  if (PR_FALSE == aState.unconstrainedHeight) {
    aState.availSize.height -= aKidRect.height;
  }

  // Update the maximum element size
  if (nsnull != aMaxElementSize) {
    if (aKidMaxElementSize.width > aMaxElementSize->width) {
      aMaxElementSize->width = aKidMaxElementSize.width;
    }
    if (aKidMaxElementSize.height > aMaxElementSize->height) {
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
PRBool ColumnFrame::ReflowMappedChildren(nsIPresContext*    aPresContext,
                                         ColumnReflowState& aState,
                                         nsSize*            aMaxElementSize)
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
    ColumnFrame* flow = (ColumnFrame*) mNextInFlow;
    while (flow != 0) {
      printf("  %p: [%d,%d,%c]\n",
             flow, flow->mFirstContentOffset, flow->mLastContentOffset,
             (flow->mLastContentIsComplete ? 'T' : 'F'));
      flow = (ColumnFrame*) flow->mNextInFlow;
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

  for (nsIFrame*  kidFrame = mFirstChild; nsnull != kidFrame; ) {
    nsSize                  kidAvailSize(aState.availSize);
    nsRect                  kidRect;
    nsIFrame::ReflowStatus  status;

    // Get top margin for this kid
    nsIStyleContextPtr kidSC;

    kidFrame->GetStyleContext(aPresContext, kidSC.AssignRef());
    nsStyleSpacing* kidSpacing = (nsStyleSpacing*)
      kidSC->GetData(kStyleSpacingSID);
    nscoord topMargin = GetTopMarginFor(aPresContext, aState,
                                        kidFrame, kidSpacing);

    // XXX Style system should do this...
    nscoord bottomMargin = ChildIsPseudoFrame(kidFrame)
      ? 0
      : kidSpacing->mMargin.bottom;

    // Figure out the amount of available size for the child (subtract
    // off the top margin we are going to apply to it)
    //
    // Note: don't subtract off for the child's left/right margins. ReflowChild()
    // will do that when it determines the current left/right edge
    if (PR_FALSE == aState.unconstrainedHeight) {
      kidAvailSize.height -= topMargin;
    }

    // Only skip the reflow if this is not our first child and we are
    // out of space.
    if ((kidFrame == mFirstChild) || (kidAvailSize.height > 0)) {
      // Reflow the child into the available space
      aState.spaceManager->Translate(0, topMargin);
      status = ReflowChild(kidFrame, aPresContext, aState.spaceManager,
                           kidAvailSize, kidRect, pKidMaxElementSize);
      aState.spaceManager->Translate(0, -topMargin);
    }

    // Did the child fit?
    if ((kidFrame != mFirstChild) &&
        ((kidAvailSize.height <= 0) ||
         (kidRect.YMost() > kidAvailSize.height)))
    {
      // The child's height is too big to fit at all in our remaining space,
      // and it's not our first child.
      //
      // Note that if the width is too big that's okay and we allow the
      // child to extend horizontally outside of the reflow area

      // Since we are giving the next-in-flow our last child, we
      // give it our original mLastContentIsComplete, too (in case we
      // are pushing into an empty next-in-flow)
      PushChildren(kidFrame, prevKidFrame, lastContentIsComplete);

      // Our mLastContentIsComplete was already set by the last kid we
      // reflowed reflow's status
      result = PR_FALSE;
      break;
    }

    // Place the child after taking into account it's margin
    aState.y += topMargin;
    aState.spaceManager->Translate(0, topMargin);
    kidRect.x += kidSpacing->mMargin.left;
    kidRect.y += aState.y;
    PlaceChild(aPresContext, aState, kidFrame, kidSpacing,
               kidRect, aMaxElementSize, kidMaxElementSize);
    if (bottomMargin < 0) {
      aState.prevMaxNegBottomMargin = -bottomMargin;
    } else {
      aState.prevMaxPosBottomMargin = bottomMargin;
    }
    childCount++;

    // Update mLastContentIsComplete now that this kid fits
    mLastContentIsComplete = NS_FRAME_IS_COMPLETE(status);

    // Special handling for incomplete children
    if (frNotComplete == status) {
      nsIFrame* kidNextInFlow;
       
      kidFrame->GetNextInFlow(kidNextInFlow);
      if (nsnull == kidNextInFlow) {
        // No the child isn't complete, and it doesn't have a next in flow so
        // create a continuing frame. This hooks the child into the flow.
        nsIFrame* continuingFrame;
         
        kidFrame->CreateContinuingFrame(aPresContext, this, continuingFrame);

        // Insert the frame. We'll reflow it next pass through the loop
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
    }

    // Get the next child
    prevKidFrame = kidFrame;
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
    ColumnFrame* flow = (ColumnFrame*) mNextInFlow;
    while (flow != 0) {
      printf("  %p: [%d,%d,%c]\n",
             flow, flow->mFirstContentOffset, flow->mLastContentOffset,
             (flow->mLastContentIsComplete ? 'T' : 'F'));
      flow = (ColumnFrame*) flow->mNextInFlow;
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
PRBool ColumnFrame::PullUpChildren(nsIPresContext*    aPresContext,
                                   ColumnReflowState& aState,
                                   nsSize*            aMaxElementSize)
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
    ColumnFrame* flow = (ColumnFrame*) mNextInFlow;
    while (flow != 0) {
      printf("  %p: [%d,%d,%c]\n",
             flow, flow->mFirstContentOffset, flow->mLastContentOffset,
             (flow->mLastContentIsComplete ? 'T' : 'F'));
      flow = (ColumnFrame*) flow->mNextInFlow;
    }
  }
#endif
#endif
  NS_PRECONDITION(nsnull != mNextInFlow, "null next-in-flow");
  ColumnFrame*  nextInFlow = (ColumnFrame*)mNextInFlow;
  nsSize        kidMaxElementSize;
  nsSize*       pKidMaxElementSize = (nsnull != aMaxElementSize) ? &kidMaxElementSize : nsnull;

  // The frame previous to the current frame we are reflowing. This
  // starts out initially as our last frame.
  nsIFrame*     prevKidFrame;
   
  LastChild(prevKidFrame);

  // This will hold the prevKidFrame's mLastContentIsComplete
  // status. If we have to push the frame that follows prevKidFrame
  // then this will become our mLastContentIsComplete state. Since
  // prevKidFrame is initially our last frame, it's completion status
  // is our mLastContentIsComplete value.
  PRBool        prevLastContentIsComplete = mLastContentIsComplete;
  PRBool        result = PR_TRUE;

  while (nsnull != nextInFlow) {
    nsRect  kidRect;
    nsSize  kidAvailSize(aState.availSize);

    // Get first available frame from the next-in-flow
    nsIFrame* kidFrame = PullUpOneChild(nextInFlow, prevKidFrame);
    if (nsnull == kidFrame) {
      // We've pulled up all the children from that next-in-flow, so
      // move to the next next-in-flow.
      nextInFlow = (ColumnFrame*) nextInFlow->mNextInFlow;
      continue;
    }

    // Get top margin for this kid
    nsIStyleContextPtr kidSC;
     
    kidFrame->GetStyleContext(aPresContext, kidSC.AssignRef());
    nsStyleSpacing* kidSpacing = (nsStyleSpacing*)
      kidSC->GetData(kStyleSpacingSID);
    nscoord topMargin = GetTopMarginFor(aPresContext, aState,
                                        kidFrame, kidSpacing);

    // XXX Style system should do this...
    nscoord bottomMargin = ChildIsPseudoFrame(kidFrame)
      ? 0
      : kidSpacing->mMargin.bottom;

    // Figure out the amount of available size for the child (subtract
    // off the top margin we are going to apply to it)
    //
    // Note: don't subtract off for the child's left/right margins. ReflowChild()
    // will do that when it determines the current left/right edge
    if (PR_FALSE == aState.unconstrainedHeight) {
      kidAvailSize.height -= topMargin;
    }

    ReflowStatus status;
    do {
      // Only skip the reflow if this is not our first child and we are
      // out of space.
      if ((kidFrame == mFirstChild) || (kidAvailSize.height > 0)) {
        aState.spaceManager->Translate(0, topMargin);
        status = ReflowChild(kidFrame, aPresContext, aState.spaceManager,
                             kidAvailSize, kidRect, pKidMaxElementSize);
        aState.spaceManager->Translate(0, -topMargin);
      }

      // Did the child fit?
      if ((kidFrame != mFirstChild) &&
          ((kidAvailSize.height <= 0) ||
           (kidRect.YMost() > kidAvailSize.height)))
      {
        // The child's height is too big to fit at all in our remaining space,
        // and it wouldn't have been our first child.
        //
        // Note that if the width is too big that's okay and we allow the
        // child to extend horizontally outside of the reflow area
        nsIFrame* kidNextInFlow;

        kidFrame->GetNextInFlow(kidNextInFlow);
        PRBool lastComplete = PRBool(nsnull == kidNextInFlow);
        PushChildren(kidFrame, prevKidFrame, lastComplete);
        mLastContentIsComplete = prevLastContentIsComplete;
        mChildCount--;
        result = PR_FALSE;
        goto push_done;
      }

      // Place the child
      aState.y += topMargin;
      aState.spaceManager->Translate(0, topMargin);
      kidRect.x += kidSpacing->mMargin.left;
      kidRect.y += aState.y;
      PlaceChild(aPresContext, aState, kidFrame, kidSpacing,
                 kidRect, aMaxElementSize, kidMaxElementSize);
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
        nsIFrame* kidNextInFlow;
         
        kidFrame->GetNextInFlow(kidNextInFlow);
        if (nsnull == kidNextInFlow) {
          // The child doesn't have a next-in-flow so create a
          // continuing frame. The creation appends it to the flow and
          // prepares it for reflow.
          nsIFrame* continuingFrame;

          kidFrame->CreateContinuingFrame(aPresContext, this, continuingFrame);

          // Add the continuing frame to our sibling list.
          nsIFrame* kidNextSibling;

          kidFrame->GetNextSibling(kidNextSibling);
          continuingFrame->SetNextSibling(kidNextSibling);
          kidFrame->SetNextSibling(continuingFrame);
          prevKidFrame = kidFrame;
          prevLastContentIsComplete = mLastContentIsComplete;
          kidFrame = continuingFrame;
          mChildCount++;
        } else {
          // The child has a next-in-flow, but it's not one of ours.
          // It *must* be in one of our next-in-flows. Collect it
          // then.
          NS_ASSERTION(!IsChild(kidNextInFlow), "busted kid next-in-flow");
          break;
        }
      }
    } while (frNotComplete == status);

    prevKidFrame = kidFrame;
    prevLastContentIsComplete = mLastContentIsComplete;
  }

 push_done:;
  // Update our last content index
  if (nsnull != prevKidFrame) {
    NS_ASSERTION(IsLastChild(prevKidFrame), "bad last child");
    SetLastContentOffset(prevKidFrame);
  }

  // We need to make sure the first content offset is correct for any empty
  // next-in-flow frames (frames where we pulled up all the child frames)
  nextInFlow = (ColumnFrame*)mNextInFlow;
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
    ColumnFrame* flow = (ColumnFrame*) mNextInFlow;
    while (flow != 0) {
      printf("  %p: [%d,%d,%c]\n",
             flow, flow->mFirstContentOffset, flow->mLastContentOffset,
             (flow->mLastContentIsComplete ? 'T' : 'F'));
      flow = (ColumnFrame*) flow->mNextInFlow;
    }
  }
#endif
#endif
  return result;
}

/**
 * Create new frames for content we haven't yet mapped
 *
 * @param   aPresContext presentation context to use
 * @param   aState current inline state
 * @return  frComplete if all content has been mapped and frNotComplete
 *            if we should be continued
 */
nsIFrame::ReflowStatus
ColumnFrame::ReflowUnmappedChildren(nsIPresContext*    aPresContext,
                                    ColumnReflowState& aState,
                                    nsSize*            aMaxElementSize)
{
#ifdef NS_DEBUG
  VerifyLastIsComplete();
#endif
  nsIFrame*     kidPrevInFlow = nsnull;
  ReflowStatus  result = frNotComplete;

  // If we have no children and we have a prev-in-flow then we need to pick
  // up where it left off. If we have children, e.g. we're being resized, then
  // our content offset should already be set correctly...
  if ((nsnull == mFirstChild) && (nsnull != mPrevInFlow)) {
    ColumnFrame* prev = (ColumnFrame*)mPrevInFlow;
    NS_ASSERTION(prev->mLastContentOffset >= prev->mFirstContentOffset, "bad prevInFlow");

    mFirstContentOffset = prev->NextChildOffset();
    if (!prev->mLastContentIsComplete) {
      // Our prev-in-flow's last child is not complete
      prev->LastChild(kidPrevInFlow);
    }
  }

  // Place our children, one at a time, until we are out of children
  nsSize    kidMaxElementSize;
  nsSize*   pKidMaxElementSize = (nsnull != aMaxElementSize) ? &kidMaxElementSize : nsnull;
  PRInt32   kidIndex = NextChildOffset();
  nsIFrame* prevKidFrame;
   
  LastChild(prevKidFrame);  // XXX remember this...

  for (;;) {
    // Get the next content object
    nsIContentPtr kid = mContent->ChildAt(kidIndex);
    if (kid.IsNull()) {
      result = frComplete;
      break;
    }

    // Make sure we still have room left
    if (aState.availSize.height <= 0) {
      // Note: return status was set to frNotComplete above...
      break;
    }

    // Resolve style
    nsIStyleContextPtr kidStyleContext =
      aPresContext->ResolveStyleContextFor(kid, this);
    nsStyleSpacing* kidSpacing = (nsStyleSpacing*)
      kidStyleContext->GetData(kStyleSpacingSID);
    nsStyleDisplay* kidDisplay = (nsStyleDisplay*)
      kidStyleContext->GetData(kStyleDisplaySID);
    nsStylePosition* kidPosition = (nsStylePosition*)
      kidStyleContext->GetData(kStylePositionSID);

    nsBlockFrame*   pseudoFrame = nsnull;
    nsIFrame*       kidFrame;
    nsRect          kidRect;
    ReflowStatus    status;

    // Create a child frame
    if (NS_STYLE_POSITION_ABSOLUTE == kidPosition->mPosition) {
      AbsoluteFrame::NewFrame(&kidFrame, kid, kidIndex, this);
      kidFrame->SetStyleContext(aPresContext,kidStyleContext);
    } else if (nsnull == kidPrevInFlow) {
      // Figure out how to treat the content
      nsIContentDelegate* kidDel = nsnull;
      switch (kidDisplay->mDisplay) {
      case NS_STYLE_DISPLAY_NONE:
        // Create a placeholder frame that takes up no space
        NS_ASSERTION(nsnull == kidPrevInFlow, "bad prev in flow");
        nsFrame::NewFrame(&kidFrame, kid, kidIndex, this);
        break;

      case NS_STYLE_DISPLAY_BLOCK:
      case NS_STYLE_DISPLAY_LIST_ITEM:
        // Get the content delegate to use. We'll need this to create a frame
        kidDel = kid->GetDelegate(aPresContext);
        kidFrame = kidDel->CreateFrame(aPresContext, kid, kidIndex, this);
        NS_RELEASE(kidDel);
        break;

      case NS_STYLE_DISPLAY_INLINE:
        // Inline elements are wrapped in a block pseudo frame; that way the
        // body doesn't have to deal with 2D layout
        nsBlockFrame::NewFrame(&kidFrame, mContent, mIndexInParent, this);

        // Set the content offset for the pseudo frame, so it knows
        // which content to begin with
        pseudoFrame = (nsBlockFrame*) kidFrame;
        pseudoFrame->SetFirstContentOffset(kidIndex);
        break;
      }
      kidFrame->SetStyleContext(aPresContext,kidStyleContext);
    } else {
      kidPrevInFlow->CreateContinuingFrame(aPresContext, this, kidFrame);
      if (ChildIsPseudoFrame(kidFrame)) {
        pseudoFrame = (nsBlockFrame*) kidFrame;
      }
    }

    // Get the child's margins
    nscoord topMargin = GetTopMarginFor(aPresContext, aState,
                                        kidFrame, kidSpacing);

    // XXX Style system should do this...
    nscoord bottomMargin = ChildIsPseudoFrame(kidFrame)
      ? 0
      : kidSpacing->mMargin.bottom;

    // Link the child frame into the list of children and update the
    // child count
    if (nsnull != prevKidFrame) {
#ifdef NS_DEBUG
      nsIFrame* prevNextSibling;

      prevKidFrame->GetNextSibling(prevNextSibling);
      NS_ASSERTION(nsnull == prevNextSibling, "bad append");
#endif
      prevKidFrame->SetNextSibling(kidFrame);
    } else {
      NS_ASSERTION(nsnull == mFirstChild, "bad create");
      mFirstChild = kidFrame;
      SetFirstContentOffset(kidFrame);
    }
    mChildCount++;

    do {
      nsSize  kidAvailSize(aState.availSize);

      // Figure out the amount of available size for the child (subtract
      // off the margin we are going to apply to it)
      //
      // Note: don't subtract off for the child's left/right margins. ReflowChild()
      // will do that when it determines the current left/right edge
      if (PR_FALSE == aState.unconstrainedHeight) {
        kidAvailSize.height -= topMargin;
      }

      // Try to reflow the child into the available space. It might not
      // fit or might need continuing
      if (kidAvailSize.height > 0) {
        aState.spaceManager->Translate(0, topMargin);
        status = ReflowChild(kidFrame, aPresContext, aState.spaceManager,
                             kidAvailSize, kidRect, pKidMaxElementSize);
        aState.spaceManager->Translate(0, -topMargin);
      }

      // Did the child fit?
      if ((kidFrame != mFirstChild) &&
          ((kidAvailSize.height <= 0) ||
           (kidRect.YMost() > kidAvailSize.height))) {
        // The child's height is too big to fit in our remaining
        // space, and it's not our first child.
        NS_ASSERTION(nsnull == mOverflowList, "bad overflow list");
        NS_ASSERTION(nsnull == mNextInFlow, "whoops");

        // Chop off the part of our child list that's being overflowed
#ifdef NS_DEBUG
        nsIFrame* prevNextSibling;

        prevKidFrame->GetNextSibling(prevNextSibling);
        NS_ASSERTION(prevNextSibling == kidFrame, "bad list");
#endif
        prevKidFrame->SetNextSibling(nsnull);

        // Create overflow list
        mOverflowList = kidFrame;

        // Fixup child count by subtracting off the number of children
        // that just ended up on the reflow list.
        PRInt32 overflowKids = 0;
        nsIFrame* f = kidFrame;
        while (nsnull != f) {
          overflowKids++;
          f->GetNextSibling(f);
        }
        mChildCount -= overflowKids;
        goto done;
      }

      // Advance y by the topMargin between children. Zero out the
      // topMargin in case this frame is continued because
      // continuations do not have a top margin. Update the prev
      // bottom margin state in the body reflow state so that we can
      // apply the bottom margin when we hit the next child (or
      // finish).
      aState.y += topMargin;
      aState.spaceManager->Translate(0, topMargin);
      kidRect.x += kidSpacing->mMargin.left;
      kidRect.y += aState.y;
      PlaceChild(aPresContext, aState, kidFrame, kidSpacing,
                 kidRect, aMaxElementSize, kidMaxElementSize);
      if (bottomMargin < 0) {
        aState.prevMaxNegBottomMargin = -bottomMargin;
      } else {
        aState.prevMaxPosBottomMargin = bottomMargin;
      }
      topMargin = 0;

      mLastContentIsComplete = PRBool(status == frComplete);
      if (frNotComplete == status) {
        // Child didn't complete so create a continuing frame
        kidPrevInFlow = kidFrame;
        nsIFrame* continuingFrame;

        kidFrame->CreateContinuingFrame(aPresContext, this, continuingFrame);

        // Add the continuing frame to the sibling list
        nsIFrame* kidNextSibling;

        kidFrame->GetNextSibling(kidNextSibling);
        continuingFrame->SetNextSibling(kidNextSibling);
        kidFrame->SetNextSibling(continuingFrame);
        prevKidFrame = kidFrame;
        kidFrame = continuingFrame;
        mChildCount++;

        if (nsnull != pseudoFrame) {
          pseudoFrame = (nsBlockFrame*) kidFrame;
        }
      }
    } while (frNotComplete == status);

    prevKidFrame = kidFrame;
    kidPrevInFlow = nsnull;

    // Update the kidIndex
    if (nsnull != pseudoFrame) {
      // Adjust kidIndex to reflect the number of children mapped by
      // the pseudo frame
      kidIndex = pseudoFrame->NextChildOffset();
    } else {
      kidIndex++;
    }
  }

done:
  // Update the content mapping
  NS_ASSERTION(IsLastChild(prevKidFrame), "bad last child");
  if (0 != mChildCount) {
    SetLastContentOffset(prevKidFrame);
  }
#ifdef NS_DEBUG
  PRInt32 len = LengthOf(mFirstChild);
  NS_ASSERTION(len == mChildCount, "bad child count");
  VerifyLastIsComplete();
#endif
  return result;
}

NS_METHOD ColumnFrame::ResizeReflow(nsIPresContext*  aPresContext,
                                    nsISpaceManager* aSpaceManager,
                                    const nsSize&    aMaxSize,
                                    nsRect&          aDesiredRect,
                                    nsSize*          aMaxElementSize,
                                    ReflowStatus&    aStatus)
{
#ifdef NS_DEBUG
  PreReflowCheck();
  
  nscoord txIn, tyIn;
  aSpaceManager->GetTranslation(txIn, tyIn);
#endif
  //XXX NS_PRECONDITION((aMaxSize.width > 0) && (aMaxSize.height > 0), "unexpected max size");

  PRBool        reflowMappedOK = PR_TRUE;

  aStatus = frComplete;  // initialize out parameter

  // Initialize out parameter
  if (nsnull != aMaxElementSize) {
    aMaxElementSize->width = 0;
    aMaxElementSize->height = 0;
  }

  // Initialize body reflow state
  ColumnReflowState state(aPresContext, aSpaceManager, aMaxSize);

  // Check for an overflow list
  MoveOverflowToChildList();

  // Reflow the existing frames
  if (nsnull != mFirstChild) {
    reflowMappedOK =
      ReflowMappedChildren(aPresContext, state, aMaxElementSize);
    if (PR_FALSE == reflowMappedOK) {
      aStatus = frNotComplete;
    }
  }

  // Did we successfully relow our mapped children?
  if (PR_TRUE == reflowMappedOK) {
    // Any space left?
    if ((nsnull != mFirstChild) && (state.availSize.height <= 0)) {
      // No space left. Don't try to pull-up children or reflow unmapped
      if (NextChildOffset() < mContent->ChildCount()) {
        aStatus = frNotComplete;
      }
    } else if (NextChildOffset() < mContent->ChildCount()) {
      // Try and pull-up some children from a next-in-flow
      if ((nsnull == mNextInFlow) ||
          PullUpChildren(aPresContext, state, aMaxElementSize)) {
        // If we still have unmapped children then create some new frames
        if (NextChildOffset() < mContent->ChildCount()) {
          aStatus =
            ReflowUnmappedChildren(aPresContext, state, aMaxElementSize);
        }
      } else {
        // We were unable to pull-up all the existing frames from the
        // next in flow
        aStatus = frNotComplete;
      }
    }
  }

  // Restore the coordinate space
  aSpaceManager->Translate(0, -state.y);

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
  aDesiredRect.x = 0;
  aDesiredRect.y = 0;
  aDesiredRect.width = state.kidXMost;
  aDesiredRect.height = state.y;

  // Our desired width is always at least as big as our parent's width
  // even though we reflowed our children to a narrower width.
  if (!state.unconstrainedWidth && (aDesiredRect.width < aMaxSize.width)) {
    aDesiredRect.width = aMaxSize.width;
  }

#ifdef NS_DEBUG
  PostReflowCheck(aStatus);

  // Verify we properly restored the coordinate space
  nscoord txOut, tyOut;

  aSpaceManager->GetTranslation(txOut, tyOut);
  NS_POSTCONDITION((txIn == txOut) && (tyIn == tyOut), "bad translation");
#endif
  return NS_OK;
}

NS_METHOD ColumnFrame::IncrementalReflow(nsIPresContext*  aPresContext,
                                         nsISpaceManager* aSpaceManager,
                                         const nsSize&    aMaxSize,
                                         nsRect&          aDesiredRect,
                                         nsReflowCommand& aReflowCommand,
                                         ReflowStatus&    aStatus)
{
#ifdef NS_DEBUG
  nscoord txIn, tyIn;
  aSpaceManager->GetTranslation(txIn, tyIn);
#endif

  // Initialize our reflow state
  ColumnReflowState state(aPresContext, aSpaceManager, aMaxSize);
  
  // XXX This nees to be computed the hard way. This value will be
  // wrong because it includes our previous border+padding
  // values. Since those values may have changed we need to
  // recalculate our maxChildWidth based on our children and then we
  // can add back in order border+padding
  state.kidXMost = mRect.width;

  nsIFrame* target = aReflowCommand.GetTarget();
  if (this == target) {
    if (aReflowCommand.GetType() == nsReflowCommand::FrameAppended) {
      nsIFrame* prevKidFrame;
      LastChild(prevKidFrame);

      // Factor in the previous kid's bottom margin information
      if (nsnull != prevKidFrame) {
        // When we have a previous kid frame, get it's y most coordinate
        // and then setup the state so that the starting y is correct
        // and the previous kid's bottom margin information is correct.
        nsRect startKidRect;
        prevKidFrame->GetRect(startKidRect);
  
        // Get style info
        nsStyleSpacing* kidSpacing;
        prevKidFrame->GetStyleData(kStyleSpacingSID, (nsStyleStruct*&)kidSpacing);
  
        // XXX Style system should do this...
        nscoord bottomMargin = ChildIsPseudoFrame(prevKidFrame)
          ? 0
          : kidSpacing->mMargin.bottom;
  
        state.y = startKidRect.YMost();
        if (bottomMargin < 0) {
          state.prevMaxNegBottomMargin = -bottomMargin;
        } else {
          state.prevMaxPosBottomMargin = bottomMargin;
        }

        if (PR_FALSE == state.unconstrainedHeight) {
          state.availSize.height -= state.y;
        }
      }

      aSpaceManager->Translate(0, state.y);
  
      // Now reflow unmapped children
      aStatus = ReflowUnmappedChildren(aPresContext, state, nsnull);
  
      // Restore the coordinate space
      aSpaceManager->Translate(0, -state.y);
    
    } else {
      NS_NOTYETIMPLEMENTED("unexpected reflow command");
    }
  } else if (ChildIsPseudoFrame(target)) {
    if (aReflowCommand.GetType() == nsReflowCommand::FrameAppended) {
      // Because our last child is a pseudo-frame we passed along the content
      // appended notification. First let the pseudo-frame reflow unmapped,
      // and then we'll reflow unmapped
      nsRect  kidRect;

      // Restore our state's running y-offset and available size
      target->GetRect(kidRect);
      state.y = kidRect.y;
      if (PR_FALSE == state.unconstrainedHeight) {
        state.availSize.height -= state.y;
      }

      nsIRunaround* reflowRunaround;
      nsSize        kidAvailSize(state.availSize);

      aSpaceManager->Translate(0, state.y);
      target->QueryInterface(kIRunaroundIID, (void**)&reflowRunaround);
      reflowRunaround->IncrementalReflow(aPresContext, aSpaceManager,
        kidAvailSize, kidRect, aReflowCommand, aStatus);
      
      // Place and size the child
      nsSize          kidMaxElementSize;
      nsStyleSpacing* kidSpacing;

      target->GetStyleData(kStyleSpacingSID, (nsStyleStruct*&)kidSpacing);
      PlaceChild(aPresContext, state, target, kidSpacing, kidRect,
                 nsnull, kidMaxElementSize);

      // Set our last content offset
      SetLastContentOffset(target);

      // If the child's status is complete then reflow unmapped children
      if (frComplete == aStatus) {
        aStatus = ReflowUnmappedChildren(aPresContext, state, nsnull);
      }

      aSpaceManager->Translate(0, -state.y);

    } else {
      NS_NOTYETIMPLEMENTED("unexpected reflow command");
    }
  } else {
    NS_NOTYETIMPLEMENTED("unexpected reflow command");
  }

  // Return our desired size
  // XXX What about adding in the bottom margin from our last child like we
  // did in ResizeReflow()?
  aDesiredRect.x = 0;
  aDesiredRect.y = 0;
  aDesiredRect.width = state.kidXMost;/* XXX */
  aDesiredRect.height = state.y;

#ifdef NS_DEBUG
  // Verify we properly restored the coordinate space
  nscoord txOut, tyOut;

  aSpaceManager->GetTranslation(txOut, tyOut);
  NS_POSTCONDITION((txIn == txOut) && (tyIn == tyOut), "bad translation");
#endif
  return NS_OK;
}

NS_METHOD ColumnFrame::ContentAppended(nsIPresShell* aShell,
                                       nsIPresContext* aPresContext,
                                       nsIContent* aContainer)
{
  // Get the last-in-flow
  ColumnFrame* lastInFlow = (ColumnFrame*)GetLastInFlow();

  // Get the last child frame, and see if it's a pseudo frame
  nsIFrame* lastKidFrame;
  LastChild(lastKidFrame);

  if ((nsnull != lastKidFrame) && ChildIsPseudoFrame(lastKidFrame)) {
    // Pass along the notification to the pseudo frame
    return lastKidFrame->ContentAppended(aShell, aPresContext, aContainer);

  } else {
    // Generate a reflow command for the last-in-flow
    nsReflowCommand* cmd = new nsReflowCommand(aPresContext, lastInFlow,
                                               nsReflowCommand::FrameAppended);
    aShell->AppendReflowCommand(cmd);
    return NS_OK;
  }
}

NS_METHOD ColumnFrame::CreateContinuingFrame(nsIPresContext* aPresContext,
                                             nsIFrame*       aParent,
                                             nsIFrame*&      aContinuingFrame)
{
  ColumnFrame* cf = new ColumnFrame(mContent, mIndexInParent, aParent);
  PrepareContinuingFrame(aPresContext, aParent, cf);
  aContinuingFrame = cf;
  return NS_OK;
}

PRIntn ColumnFrame::GetSkipSides() const
{
  // Because we're only ever used as a pseudo frame we shouldn't have any borders
  // or padding. But just to be safe...
  return 0x0F;
}

NS_METHOD ColumnFrame::ListTag(FILE* out) const
{
  fprintf(out, "*COLUMN@%p", this);
  return NS_OK;
}

