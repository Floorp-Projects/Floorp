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
#include "nsBodyFrame.h"
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
#include "nsBlockFrame.h"
#include "nsSpaceManager.h"
#include "nsHTMLAtoms.h"

static NS_DEFINE_IID(kIRunaroundIID, NS_IRUNAROUND_IID);
static NS_DEFINE_IID(kIAnchoredItemsIID, NS_IANCHOREDITEMS_IID);

static NS_DEFINE_IID(kStyleSpacingSID, NS_STYLESPACING_SID);

nsresult nsBodyFrame::NewFrame(nsIFrame** aInstancePtrResult,
                               nsIContent* aContent,
                               nsIFrame*   aParent)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIFrame* it = new nsBodyFrame(aContent, aParent);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aInstancePtrResult = it;
  return NS_OK;
}

nsBodyFrame::nsBodyFrame(nsIContent* aContent, nsIFrame* aParentFrame)
  : nsHTMLContainerFrame(aContent, aParentFrame)
{
  mIsPseudoFrame = IsPseudoFrame();
  mSpaceManager = new SpaceManager(this);
  NS_ADDREF(mSpaceManager);
}

nsBodyFrame::~nsBodyFrame()
{
  NS_RELEASE(mSpaceManager);
}

nsresult
nsBodyFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(0 != aInstancePtr, "null ptr");
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIAnchoredItemsIID)) {
    *aInstancePtr = (void*) ((nsIAnchoredItems*) this);
    return NS_OK;
  }
  return nsHTMLContainerFrame::QueryInterface(aIID, aInstancePtr);
}

void nsBodyFrame::CreateColumnFrame(nsIPresContext* aPresContext)
{
  // Do we have a prev-in-flow?
  if (nsnull == mPrevInFlow) {
    // No, create a column pseudo frame
    nsBlockFrame::NewFrame(&mFirstChild, mContent, this);
    mChildCount = 1;

    // Resolve style and set the style context
    nsIStyleContext* styleContext =
      aPresContext->ResolvePseudoStyleContextFor(nsHTMLAtoms::columnPseudo, this);
    mFirstChild->SetStyleContext(aPresContext,styleContext);
    NS_RELEASE(styleContext);
  } else {
    nsBodyFrame*  prevBody = (nsBodyFrame*)mPrevInFlow;

    nsIFrame* prevColumn = prevBody->mFirstChild;
    NS_ASSERTION(prevBody->ChildIsPseudoFrame(prevColumn), "bad previous column");

    // Create a continuing column
    prevColumn->CreateContinuingFrame(aPresContext, this, nsnull, mFirstChild);
    mChildCount = 1;
  }
}

nsSize nsBodyFrame::GetColumnAvailSpace(nsIPresContext*  aPresContext,
                                        const nsMargin&  aBorderPadding,
                                        const nsSize&    aMaxSize)
{
  nsSize  result(aMaxSize);

  // If we're not being used as a pseudo frame then make adjustments
  // for border/padding and a vertical scrollbar
  if (!mIsPseudoFrame) {
    // If our width is constrained then subtract for the border/padding
    if (aMaxSize.width != NS_UNCONSTRAINEDSIZE) {
      result.width -= aBorderPadding.left +
                      aBorderPadding.right;
      if (! aPresContext->IsPaginated()) {
        nsIDeviceContext* dc = aPresContext->GetDeviceContext();
        result.width -= NS_TO_INT_ROUND(dc->GetScrollBarWidth());
        NS_RELEASE(dc);
      }
    }
    // If our height is constrained then subtract for the border/padding
    if (aMaxSize.height != NS_UNCONSTRAINEDSIZE) {
      result.height -= aBorderPadding.top +
                       aBorderPadding.bottom;
    }
  }

  return result;
}

NS_METHOD nsBodyFrame::ResizeReflow(nsIPresContext*  aPresContext,
                                    nsReflowMetrics& aDesiredSize,
                                    const nsSize&    aMaxSize,
                                    nsSize*          aMaxElementSize,
                                    nsReflowStatus&  aStatus)
{
  NS_FRAME_TRACE_REFLOW_IN("nsBodyFrame::ResizeReflow");

  aStatus = NS_FRAME_COMPLETE;  // initialize out parameter

  // Do we have any children?
  if (nsnull == mFirstChild) {
    // No, create a column frame
    CreateColumnFrame(aPresContext);
  }

  // Reflow the column
  if (nsnull != mFirstChild) {
    // Clear any regions that are marked as unavailable
    mSpaceManager->ClearRegions();

    // Get our border/padding info
    nsStyleSpacing* mySpacing =
      (nsStyleSpacing*)mStyleContext->GetData(kStyleSpacingSID);
    nsMargin  borderPadding;
    mySpacing->CalcBorderPaddingFor(this, borderPadding);

    // Compute the column's max size
    nsSize  columnMaxSize = GetColumnAvailSpace(aPresContext, borderPadding,
                                                aMaxSize);

    // Get the column's desired rect
    nsRect        desiredRect;
    nsIRunaround* reflowRunaround;

    mFirstChild->WillReflow(*aPresContext);
    mSpaceManager->Translate(borderPadding.left, borderPadding.top);
    mFirstChild->QueryInterface(kIRunaroundIID, (void**)&reflowRunaround);
    reflowRunaround->ResizeReflow(aPresContext, mSpaceManager, columnMaxSize,
                desiredRect, aMaxElementSize, aStatus);
    mSpaceManager->Translate(-borderPadding.left, -borderPadding.top);

    // If the frame is complete, then check whether there's a next-in-flow that
    // needs to be deleted
    if (NS_FRAME_IS_COMPLETE(aStatus)) {
      nsIFrame* kidNextInFlow;
       
      mFirstChild->GetNextInFlow(kidNextInFlow);
      if (nsnull != kidNextInFlow) {
        // Remove all of the childs next-in-flows
        DeleteChildsNextInFlow(mFirstChild);
      }
    }

    // Place and size the column
    desiredRect.x += borderPadding.left;
    desiredRect.y += borderPadding.top;
    mFirstChild->SetRect(desiredRect);
    mFirstChild->DidReflow(*aPresContext, NS_FRAME_REFLOW_FINISHED);

    // Set our last content offset and whether the last content is complete
    // based on the state of the pseudo frame
    nsBlockFrame* blockPseudoFrame = (nsBlockFrame*)mFirstChild;
    mLastContentOffset = blockPseudoFrame->GetLastContentOffset();
    mLastContentIsComplete = blockPseudoFrame->GetLastContentIsComplete();

    // Return our desired size
    ComputeDesiredSize(desiredRect, aMaxSize, borderPadding, aDesiredSize);
  }

  NS_FRAME_TRACE_REFLOW_OUT("nsBodyFrame::ResizeReflow", aStatus);
  return NS_OK;
}

NS_METHOD nsBodyFrame::VerifyTree() const
{
#ifdef NS_DEBUG
  // Check our child count
  PRInt32 len = LengthOf(mFirstChild);
  NS_ASSERTION(len == mChildCount, "bad child count");
  nsIFrame* lastChild;
   
  LastChild(lastChild);
  if (len != 0) {
    NS_ASSERTION(nsnull != lastChild, "bad last child");
  }
  NS_ASSERTION(nsnull == mOverflowList, "bad overflow list");

  // Make sure our content offsets are sane
  NS_ASSERTION(mFirstContentOffset <= mLastContentOffset, "bad offsets");

  // Verify child content offsets
  nsIFrame* child = mFirstChild;
  while (nsnull != child) {
    // Make sure that the child's tree is valid
    child->VerifyTree();
    child->GetNextSibling(child);
  }

  // Make sure that our flow blocks offsets are all correct
  if (nsnull == mPrevInFlow) {
    PRInt32 nextOffset = NextChildOffset();
    nsBodyFrame* nextInFlow = (nsBodyFrame*) mNextInFlow;
    while (nsnull != nextInFlow) {
      NS_ASSERTION(0 != nextInFlow->mChildCount, "empty next-in-flow");
      NS_ASSERTION(nextInFlow->GetFirstContentOffset() == nextOffset,
                    "bad next-in-flow first offset");
      nextOffset = nextInFlow->NextChildOffset();
      nextInFlow = (nsBodyFrame*) nextInFlow->mNextInFlow;
    }
  }
#endif
  return NS_OK;
}

NS_METHOD nsBodyFrame::IncrementalReflow(nsIPresContext*  aPresContext,
                                         nsReflowMetrics& aDesiredSize,
                                         const nsSize&    aMaxSize,
                                         nsReflowCommand& aReflowCommand,
                                         nsReflowStatus&  aStatus)
{
  NS_FRAME_TRACE_REFLOW_IN("nsBodyFrame::IncrementalReflow");

  // Get our border/padding info
  nsStyleSpacing* mySpacing =
    (nsStyleSpacing*)mStyleContext->GetData(kStyleSpacingSID);
  nsMargin  borderPadding;
  mySpacing->CalcBorderPaddingFor(this, borderPadding);

  mSpaceManager->Translate(borderPadding.left, borderPadding.top);

  // The reflow command should never be target for us
  NS_ASSERTION(aReflowCommand.GetTarget() != this, "bad reflow command target");

  // Compute the child frame's max size
  nsSize  columnMaxSize = GetColumnAvailSpace(aPresContext, borderPadding,
                                              aMaxSize);

  // Pass the command along to our column pseudo frame
  nsRect      desiredRect;
  nsIFrame*   nextFrame;

  NS_ASSERTION(nsnull != mFirstChild, "no first child");
  mFirstChild->WillReflow(*aPresContext);
  aStatus = aReflowCommand.Next(mSpaceManager, desiredRect, columnMaxSize,
                                nextFrame);

  // Place and size the frame
  desiredRect.x += borderPadding.left;
  desiredRect.y += borderPadding.top;
  mFirstChild->SetRect(desiredRect);
  mFirstChild->DidReflow(*aPresContext, NS_FRAME_REFLOW_FINISHED);

  // Set our last content offset and whether the last content is complete
  // based on the state of the pseudo frame
  nsBlockFrame* blockPseudoFrame = (nsBlockFrame*)mFirstChild;
  mLastContentOffset = blockPseudoFrame->GetLastContentOffset();
  mLastContentIsComplete = blockPseudoFrame->GetLastContentIsComplete();

  // Return our desired size
  ComputeDesiredSize(desiredRect, aMaxSize, borderPadding, aDesiredSize);

  mSpaceManager->Translate(-borderPadding.left, -borderPadding.top);

  NS_FRAME_TRACE_REFLOW_OUT("nsBodyFrame::IncrementalReflow", aStatus);
  return NS_OK;
}

void
nsBodyFrame::ComputeDesiredSize(const nsRect&    aDesiredRect,
                                const nsSize&    aMaxSize,
                                const nsMargin&  aBorderPadding,
                                nsReflowMetrics& aDesiredSize)
{
  // Note: Body used as a pseudo-frame shrink wraps
  aDesiredSize.height = PR_MAX(aDesiredRect.YMost(), mSpaceManager->YMost());
  aDesiredSize.width = aDesiredRect.XMost();
  if (!mIsPseudoFrame) {
    aDesiredSize.width = PR_MAX(aDesiredSize.width, aMaxSize.width);
    aDesiredSize.height += aBorderPadding.top +
                           aBorderPadding.bottom;
  }
  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;
}

NS_METHOD nsBodyFrame::ContentAppended(nsIPresShell*   aShell,
                                       nsIPresContext* aPresContext,
                                       nsIContent*     aContainer)
{
  NS_ASSERTION(mContent == aContainer, "bad content-appended target");

  // Pass along the notification to our pseudo frame. It will generate a
  // reflow command
  return mFirstChild->ContentAppended(aShell, aPresContext, aContainer);
}

NS_METHOD nsBodyFrame::ContentInserted(nsIPresShell*   aShell,
                                       nsIPresContext* aPresContext,
                                       nsIContent*     aContainer,
                                       nsIContent*     aChild,
                                       PRInt32         aIndexInParent)
{
  NS_ASSERTION(mContent == aContainer, "bad content-inserted target");

  // Pass along the notification to our pseudo frame that maps all the content
  return mFirstChild->ContentInserted(aShell, aPresContext, aContainer,
                                      aChild, aIndexInParent);
}

void nsBodyFrame::AddAnchoredItem(nsIFrame*         aAnchoredItem,
                                  AnchoringPosition aPosition,
                                  nsIFrame*         aContainer)
{
  aAnchoredItem->SetGeometricParent(this);
  // Add the item to the end of the child list
  nsIFrame* lastChild;

  LastChild(lastChild);
  lastChild->SetNextSibling(aAnchoredItem);
  aAnchoredItem->SetNextSibling(nsnull);
  mChildCount++;
}

void nsBodyFrame::RemoveAnchoredItem(nsIFrame* aAnchoredItem)
{
  NS_PRECONDITION(IsChild(aAnchoredItem), "bad anchored item");
  NS_ASSERTION(aAnchoredItem != mFirstChild, "unexpected anchored item");
  // Remove the anchored item from the child list
  // XXX Implement me
  mChildCount--;
}

NS_METHOD
nsBodyFrame::CreateContinuingFrame(nsIPresContext*  aPresContext,
                                   nsIFrame*        aParent,
                                   nsIStyleContext* aStyleContext,
                                   nsIFrame*&       aContinuingFrame)
{
  nsBodyFrame* cf = new nsBodyFrame(mContent, aParent);
  if (nsnull == cf) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  PrepareContinuingFrame(aPresContext, aParent, aStyleContext, cf);
  aContinuingFrame = cf;
  return NS_OK;
}

// XXX use same logic as block frame?
PRIntn nsBodyFrame::GetSkipSides() const
{
  PRIntn skip = 0;
  if (nsnull != mPrevInFlow) {
    skip |= 1 << NS_SIDE_TOP;
  }
  if (nsnull != mNextInFlow) {
    skip |= 1 << NS_SIDE_BOTTOM;
  }
  return skip;
}
