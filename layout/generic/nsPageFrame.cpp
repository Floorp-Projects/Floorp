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
#include "nsPageFrame.h"
#include "nsIContent.h"
#include "nsIContentDelegate.h"
#include "nsIPresContext.h"
#include "nsIStyleContext.h"
#include "nsReflowCommand.h"
#include "nsIRenderingContext.h"

nsPageFrame::nsPageFrame(nsIContent* aContent, nsIFrame* aParent)
  : nsContainerFrame(aContent, aParent)
{
}

void nsPageFrame::CreateFirstChild(nsIPresContext* aPresContext)
{
  // Create a frame for our one and only content child
  if (mContent->ChildCount() > 0) {
    nsIContent* child = mContent->ChildAt(0);

    if (nsnull != child) {
      // Create a frame
      nsIContentDelegate* cd = child->GetDelegate(aPresContext);
      if (nsnull != cd) {
        nsIStyleContext* kidStyleContext =
          aPresContext->ResolveStyleContextFor(child, this);
        nsresult rv = cd->CreateFrame(aPresContext, child, this,
                                      kidStyleContext, mFirstChild);
        NS_RELEASE(kidStyleContext);
        if (NS_OK == mFirstChild) {
          mChildCount = 1;
          mLastContentOffset = mFirstContentOffset;
        }
        NS_RELEASE(cd);
      }
      NS_RELEASE(child);
    }
  }
}

NS_METHOD nsPageFrame::Reflow(nsIPresContext*      aPresContext,
                              nsReflowMetrics&     aDesiredSize,
                              const nsReflowState& aReflowState,
                              nsReflowStatus&      aStatus)
{
#ifdef NS_DEBUG
  PreReflowCheck();
#endif
  aStatus = NS_FRAME_COMPLETE;  // initialize out parameter

  // XXX Do something sensible in page mode...
  if (eReflowReason_Incremental == aReflowState.reason) {
    // We don't expect the target of the reflow command to be page frame
    NS_ASSERTION(nsnull != aReflowState.reflowCommand, "null reflow command");
    NS_ASSERTION(aReflowState.reflowCommand->GetTarget() != this,
                 "page frame is reflow command target");
  
    // Verify the next reflow command frame is our one and only child frame
    nsIFrame* next = aReflowState.reflowCommand->GetNext();
    NS_ASSERTION(next == mFirstChild, "bad reflow frame");

    // Dispatch the reflow command to our content child. Allow it to be as high
    // as it wants
    nsSize        maxSize(aReflowState.maxSize.width, NS_UNCONSTRAINEDSIZE);
    nsReflowState reflowState(aReflowState.reflowCommand, maxSize);
  
    aStatus = ReflowChild(mFirstChild, aPresContext, aDesiredSize, reflowState);
  
    // Place and size the child. Make sure the child is at least as
    // tall as our max size (the containing window)
    if (aDesiredSize.height < aReflowState.maxSize.height) {
      aDesiredSize.height = aReflowState.maxSize.height;
    }

    nsRect  rect(0, 0, aDesiredSize.width, aDesiredSize.height);
    mFirstChild->SetRect(rect);

  } else {
    // Do we have any children?
    if (nsnull == mFirstChild) {
      if (nsnull == mPrevInFlow) {
        // Create the first child frame
        CreateFirstChild(aPresContext);
      } else {
        nsPageFrame*  prevPage = (nsPageFrame*)mPrevInFlow;
  
        NS_ASSERTION(!prevPage->mLastContentIsComplete, "bad continuing page");
        nsIFrame* prevLastChild;
        prevPage->LastChild(prevLastChild);
  
        // Create a continuing child of the previous page's last child
        nsIStyleContext* kidSC;
        prevLastChild->GetStyleContext(aPresContext, kidSC);
        nsresult rv = prevLastChild->CreateContinuingFrame(aPresContext, this,
                                                           kidSC, mFirstChild);
        NS_RELEASE(kidSC);
  
        mChildCount = 1;
        mLastContentOffset = mFirstContentOffset;
      }
    }
  
    // Resize our frame allowing it only to be as big as we are
    // XXX Pay attention to the page's border and padding...
    if (nsnull != mFirstChild) {
      // Get the child's desired size
      mFirstChild->WillReflow(*aPresContext);
      aStatus = ReflowChild(mFirstChild, aPresContext, aDesiredSize, aReflowState);
      mLastContentIsComplete = NS_FRAME_IS_COMPLETE(aStatus);
  
      // Make sure the child is at least as tall as our max size (the containing window)
      if (aDesiredSize.height < aReflowState.maxSize.height) {
        aDesiredSize.height = aReflowState.maxSize.height;
      }
  
      // Place and size the child
      nsRect  rect(0, 0, aDesiredSize.width, aDesiredSize.height);
      mFirstChild->SetRect(rect);
      mFirstChild->DidReflow(*aPresContext, NS_FRAME_REFLOW_FINISHED);
  
      // Is the frame complete?
      if (NS_FRAME_IS_COMPLETE(aStatus)) {
        nsIFrame* childNextInFlow;
  
        mFirstChild->GetNextInFlow(childNextInFlow);
        NS_ASSERTION(nsnull == childNextInFlow, "bad child flow list");
      }
    }

    // Return our desired size
    aDesiredSize.width = aReflowState.maxSize.width;
    aDesiredSize.height = aReflowState.maxSize.height;
  }

#ifdef NS_DEBUG
  PostReflowCheck(aStatus);
#endif
  return NS_OK;
}

NS_METHOD
nsPageFrame::CreateContinuingFrame(nsIPresContext*  aPresContext,
                                   nsIFrame*        aParent,
                                   nsIStyleContext* aStyleContext,
                                   nsIFrame*&       aContinuingFrame)
{
  nsPageFrame* cf = new nsPageFrame(mContent, aParent);
  if (nsnull == cf) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  PrepareContinuingFrame(aPresContext, aParent, aStyleContext, cf);
  aContinuingFrame = cf;
  return NS_OK;
}

NS_METHOD nsPageFrame::Paint(nsIPresContext&      aPresContext,
                           nsIRenderingContext& aRenderingContext,
                           const nsRect&        aDirtyRect)
{
  nsContainerFrame::Paint(aPresContext, aRenderingContext, aDirtyRect);

  // For the time being paint a border around the page so we know
  // where each page begins and ends
  aRenderingContext.SetColor(NS_RGB(0, 0, 0));
  aRenderingContext.DrawRect(0, 0, mRect.width, mRect.height);
  return NS_OK;
}

NS_METHOD nsPageFrame::ListTag(FILE* out) const
{
  fprintf(out, "*PAGE@%p", this);
  return NS_OK;
}

