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
#include "nsColumnFrame.h"
#include "nsSpaceManager.h"

static NS_DEFINE_IID(kIRunaroundIID, NS_IRUNAROUND_IID);
static NS_DEFINE_IID(kIAnchoredItemsIID, NS_IANCHOREDITEMS_IID);
static NS_DEFINE_IID(kStyleMoleculeSID, NS_STYLEMOLECULE_SID);

nsresult nsBodyFrame::NewFrame(nsIFrame** aInstancePtrResult,
                               nsIContent* aContent,
                               PRInt32     aIndexInParent,
                               nsIFrame*   aParent)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIFrame* it = new nsBodyFrame(aContent, aIndexInParent, aParent);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aInstancePtrResult = it;
  return NS_OK;
}

nsBodyFrame::nsBodyFrame(nsIContent* aContent,
                         PRInt32     aIndexInParent,
                         nsIFrame*   aParentFrame)
  : nsHTMLContainerFrame(aContent, aIndexInParent, aParentFrame)
{
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
    mFirstChild = new ColumnFrame(mContent, mIndexInParent, this);
    mChildCount = 1;

    // Resolve style and set the style context
    nsIStyleContext* styleContext =
      aPresContext->ResolveStyleContextFor(mContent, this);
    mFirstChild->SetStyleContext(styleContext);
    NS_RELEASE(styleContext);
  } else {
    nsBodyFrame*  prevBody = (nsBodyFrame*)mPrevInFlow;

    nsIFrame* prevColumn = prevBody->mFirstChild;
    NS_ASSERTION(prevBody->ChildIsPseudoFrame(prevColumn), "bad previous column");

    // Create a continuing column
    mFirstChild = prevColumn->CreateContinuingFrame(aPresContext, this);
    mChildCount = 1;
  }
}

nsSize nsBodyFrame::GetColumnAvailSpace(nsIPresContext*  aPresContext,
                                        nsStyleMolecule* aMol,
                                        const nsSize&    aMaxSize)
{
  nsSize  result(aMaxSize);

  // If we're not being used as a pseudo frame then make adjustments
  // for border/padding and a vertical scrollbar
  if (!IsPseudoFrame()) {
    // If our width is constrained then subtract for the border/padding
    if (aMaxSize.width != NS_UNCONSTRAINEDSIZE) {
      result.width -= aMol->borderPadding.left + aMol->borderPadding.right;
      if (! aPresContext->IsPaginated()) {
        nsIDeviceContext* dc = aPresContext->GetDeviceContext();
        result.width -= NS_TO_INT_ROUND(dc->GetScrollBarWidth());
        NS_RELEASE(dc);
      }
    }
    // If our height is constrained then subtract for the border/padding
    if (aMaxSize.height != NS_UNCONSTRAINEDSIZE) {
      result.height -= aMol->borderPadding.top + aMol->borderPadding.bottom;
    }
  }

  return result;
}

nsIFrame::ReflowStatus
nsBodyFrame::ResizeReflow(nsIPresContext*  aPresContext,
                          nsReflowMetrics& aDesiredSize,
                          const nsSize&    aMaxSize,
                          nsSize*          aMaxElementSize)
{
  nsIFrame::ReflowStatus  status = frComplete;

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
    nsStyleMolecule* myMol =
      (nsStyleMolecule*)mStyleContext->GetData(kStyleMoleculeSID);

    // Compute the column's max size
    nsSize  columnMaxSize = GetColumnAvailSpace(aPresContext, myMol, aMaxSize);

    // XXX Style code should be dealing with this...
    PRBool  isPseudoFrame = IsPseudoFrame();
    nscoord leftInset = 0, topInset = 0;
    if (!isPseudoFrame) {
      leftInset = myMol->borderPadding.left;
      topInset = myMol->borderPadding.top;
    }

    // Get the column's desired rect
    nsRect  desiredRect;

    mSpaceManager->Translate(leftInset, topInset);
    status = ReflowChild(mFirstChild, aPresContext, mSpaceManager, columnMaxSize,
                         desiredRect, aMaxElementSize);
    mSpaceManager->Translate(-leftInset, -topInset);

    // Place and size the column
    desiredRect.x += leftInset;
    desiredRect.y += topInset;
    mFirstChild->SetRect(desiredRect);

    // Set our last content offset and whether the last content is complete
    // based on the state of the pseudo frame
    SetLastContentOffset(mFirstChild);

    // Return our desired size. Take into account the y-most floater
    aDesiredSize.width = desiredRect.XMost();
    aDesiredSize.height = PR_MAX(desiredRect.YMost(), mSpaceManager->YMost());

    if (!isPseudoFrame) {
      aDesiredSize.width += myMol->borderPadding.left + myMol->borderPadding.right;
      aDesiredSize.height += myMol->borderPadding.top + myMol->borderPadding.bottom;
    }
    aDesiredSize.ascent = aDesiredSize.height;
    aDesiredSize.descent = 0;
  }

  return status;
}

void nsBodyFrame::VerifyTree() const
{
#ifdef NS_DEBUG
  // Check our child count
  PRInt32 len = LengthOf(mFirstChild);
  NS_ASSERTION(len == mChildCount, "bad child count");
  nsIFrame* lastChild = LastChild();
  if (len != 0) {
    NS_ASSERTION(nsnull != lastChild, "bad last child");
  }
  NS_ASSERTION(nsnull == mOverflowList, "bad overflow list");

  // Make sure our content offsets are sane
  NS_ASSERTION(mFirstContentOffset <= mLastContentOffset, "bad offsets");

  // Verify child content offsets
  PRInt32 offset = mFirstContentOffset;
  nsIFrame* child = mFirstChild;
  while (nsnull != child) {
    // Make sure that the child's tree is valid
    child->VerifyTree();
    child = child->GetNextSibling();
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
}

nsIFrame::ReflowStatus
nsBodyFrame::IncrementalReflow(nsIPresContext*  aPresContext,
                               nsReflowMetrics& aDesiredSize,
                               const nsSize&    aMaxSize,
                               nsReflowCommand& aReflowCommand)
{
  ReflowStatus  status;

  // Get our border/padding info
  nsStyleMolecule* myMol =
    (nsStyleMolecule*)mStyleContext->GetData(kStyleMoleculeSID);

  // XXX Style code should be dealing with this...
  PRBool  isPseudoFrame = IsPseudoFrame();
  nscoord leftInset = 0, topInset = 0;
  if (!isPseudoFrame) {
    leftInset = myMol->borderPadding.left;
    topInset = myMol->borderPadding.top;
  }

  // XXX Clear the list of regions. This fixes a problem with the way reflow
  // appended is currently working (we're reflowing some framems twice)
  mSpaceManager->ClearRegions();
  mSpaceManager->Translate(leftInset, topInset);

  // Is the reflow command targeted for us?
  if (aReflowCommand.GetTarget() == this) {
    // Currently we only support appended content
    if (aReflowCommand.GetType() != nsReflowCommand::FrameAppended) {
      NS_NOTYETIMPLEMENTED("unexpected reflow command");
    }

    // Compute the column's max size
    nsSize  columnMaxSize = GetColumnAvailSpace(aPresContext, myMol, aMaxSize);

    // Pass the command along to our column pseudo frame
    nsIRunaround* reflowRunaround;
    nsRect        aDesiredRect;

    NS_ASSERTION(nsnull != mFirstChild, "no first child");
    mFirstChild->QueryInterface(kIRunaroundIID, (void**)&reflowRunaround);
    status = reflowRunaround->IncrementalReflow(aPresContext, mSpaceManager,
      columnMaxSize, aDesiredRect, aReflowCommand);

    // Place and size the column
    aDesiredRect.x += leftInset;
    aDesiredRect.y += topInset;
    mFirstChild->SetRect(aDesiredRect);

    // Set our last content offset and whether the last content is complete
    // based on the state of the pseudo frame
    SetLastContentOffset(mFirstChild);

    // Return our desired size
    aDesiredSize.width = aDesiredRect.XMost();
    aDesiredSize.height = aDesiredRect.YMost();
    if (!isPseudoFrame) {
      aDesiredSize.width += myMol->borderPadding.left + myMol->borderPadding.right;
      aDesiredSize.height += myMol->borderPadding.top + myMol->borderPadding.bottom;
    }
    aDesiredSize.ascent = aDesiredSize.height;
    aDesiredSize.descent = 0;
  } else {
    NS_NOTYETIMPLEMENTED("unexpected reflow command");
    nsRect    desiredRect;
    nsIFrame* child;

    status = aReflowCommand.Next(mSpaceManager, desiredRect, aMaxSize, child);

    // XXX Deal with next in flow, adjusting of siblings, adjusting the
    // content length...

    // Return our desired size
    aDesiredSize.width = 0;
    aDesiredSize.height = 0;
    aDesiredSize.ascent = aDesiredSize.height;
    aDesiredSize.descent = 0;
  }

  mSpaceManager->Translate(-leftInset, -topInset);
  return status;
}

void nsBodyFrame::ContentAppended(nsIPresShell* aShell,
                                  nsIPresContext* aPresContext,
                                  nsIContent* aContainer)
{
  NS_ASSERTION(mContent == aContainer, "bad content-appended target");

  // Zip down to the end-of-flow
  nsBodyFrame* flow = this;
  for (;;) {
    nsBodyFrame* next = (nsBodyFrame*) flow->GetNextInFlow();
    if (nsnull == next) {
      break;
    }
    flow = next;
  }

  // Since body frame's have only a single pseudo-frame in them,
  // pass on the content-appended call to the pseudo-frame
  PRInt32 oldLastContentOffset = mLastContentOffset;
  flow->mFirstChild->ContentAppended(aShell, aPresContext, aContainer);

  // Now generate a frame reflow command aimed at flow
  nsReflowCommand* rc =
    new nsReflowCommand(aPresContext, flow, nsReflowCommand::FrameAppended,
                        oldLastContentOffset);
  aShell->AppendReflowCommand(rc);
}

void nsBodyFrame::AddAnchoredItem(nsIFrame*         aAnchoredItem,
                                  AnchoringPosition aPosition,
                                  nsIFrame*         aContainer)
{
  aAnchoredItem->SetGeometricParent(this);
  // Add the item to the end of the child list
  LastChild()->SetNextSibling(aAnchoredItem);
  aAnchoredItem->SetNextSibling(nsnull);
  mChildCount++;
}

void nsBodyFrame::RemoveAnchoredItem(nsIFrame* aAnchoredItem)
{
  NS_PRECONDITION(this == aAnchoredItem->GetGeometricParent(), "bad anchored item");
  NS_ASSERTION(aAnchoredItem != mFirstChild, "unexpected anchored item");
  // Remove the anchored item from the child list
  // XXX Implement me
  mChildCount--;
}

nsIFrame* nsBodyFrame::CreateContinuingFrame(nsIPresContext* aPresContext,
                                             nsIFrame*       aParent)
{
  nsBodyFrame* cf = new nsBodyFrame(mContent, mIndexInParent, aParent);
  PrepareContinuingFrame(aPresContext, aParent, cf);
  return cf;
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
