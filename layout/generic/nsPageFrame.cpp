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

PageFrame::PageFrame(nsIContent* aContent, PRInt32 aIndexInParent, nsIFrame* aParent)
  : nsContainerFrame(aContent, aIndexInParent, aParent)
{
}

void PageFrame::CreateFirstChild(nsIPresContext* aPresContext)
{
  // Create a frame for our one and only content child
  if (mContent->ChildCount() > 0) {
    nsIContent* child = mContent->ChildAt(0);

    if (nsnull != child) {
      // Create a frame
      nsIContentDelegate* cd = child->GetDelegate(aPresContext);
      if (nsnull != cd) {
        mFirstChild = cd->CreateFrame(aPresContext, child, 0, this);
        if (nsnull != mFirstChild) {
          mChildCount = 1;
          mLastContentOffset = mFirstContentOffset;

          // Resolve style and set the style context
          nsIStyleContext* kidStyleContext =
            aPresContext->ResolveStyleContextFor(child, this);
          mFirstChild->SetStyleContext(kidStyleContext);
          NS_RELEASE(kidStyleContext);
        }
        NS_RELEASE(cd);
      }
      NS_RELEASE(child);
    }
  }
}

NS_METHOD PageFrame::ResizeReflow(nsIPresContext*  aPresContext,
                                  nsReflowMetrics& aDesiredSize,
                                  const nsSize&    aMaxSize,
                                  nsSize*          aMaxElementSize,
                                  ReflowStatus&    aStatus)
{
#ifdef NS_DEBUG
  PreReflowCheck();
#endif
  aStatus = frComplete;  // initialize out parameter

  // Do we have any children?
  if (nsnull == mFirstChild) {
    if (nsnull == mPrevInFlow) {
      // Create the first child frame
      CreateFirstChild(aPresContext);
    } else {
      PageFrame*  prevPage = (PageFrame*)mPrevInFlow;

      NS_ASSERTION(!prevPage->mLastContentIsComplete, "bad continuing page");
      nsIFrame* prevLastChild;
      prevPage->LastChild(prevLastChild);

      // Create a continuing child of the previous page's last child
      prevLastChild->CreateContinuingFrame(aPresContext, this, mFirstChild);
      mChildCount = 1;
      mLastContentOffset = mFirstContentOffset;
    }
  }

  // Resize our frame allowing it only to be as big as we are
  // XXX Pay attention to the page's border and padding...
  if (nsnull != mFirstChild) {
    // Get the child's desired size
    aStatus = ReflowChild(mFirstChild, aPresContext, aDesiredSize, aMaxSize,
                          aMaxElementSize);
    mLastContentIsComplete = PRBool(aStatus == frComplete);

    // Make sure the child is at least as tall as our max size (the containing window)
    if (aDesiredSize.height < aMaxSize.height) {
      aDesiredSize.height = aMaxSize.height;
    }

    // Place and size the child
    nsRect  rect(0, 0, aDesiredSize.width, aDesiredSize.height);
    mFirstChild->SetRect(rect);

    // Is the frame complete?
    if (frComplete == aStatus) {
      nsIFrame* childNextInFlow;

      mFirstChild->GetNextInFlow(childNextInFlow);
      NS_ASSERTION(nsnull == childNextInFlow, "bad child flow list");
    }
  }

  // Return our desired size
  aDesiredSize.width = aMaxSize.width;
  aDesiredSize.height = aMaxSize.height;

#ifdef NS_DEBUG
  PostReflowCheck(aStatus);
#endif
  return NS_OK;
}

// XXX Do something sensible in page mode...
NS_METHOD PageFrame::IncrementalReflow(nsIPresContext*  aPresContext,
                                       nsReflowMetrics& aDesiredSize,
                                       const nsSize&    aMaxSize,
                                       nsReflowCommand& aReflowCommand,
                                       ReflowStatus&    aStatus)
{
  // We don't expect the target of the reflow command to be page frame
  NS_ASSERTION(aReflowCommand.GetTarget() != this, "page frame is reflow command target");

  nsSize        maxSize(aMaxSize.width, NS_UNCONSTRAINEDSIZE);
  nsIFrame*     child;

  // Dispatch the reflow command to our pseudo frame. Allow it to be as high
  // as it wants
  aStatus = aReflowCommand.Next(aDesiredSize, maxSize, child);

  // Place and size the child. Make sure the child is at least as
  // tall as our max size (the containing window)
  if (nsnull != child) {
    NS_ASSERTION(child == mFirstChild, "unexpected reflow command frame");
    if (aDesiredSize.height < aMaxSize.height) {
      aDesiredSize.height = aMaxSize.height;
    }

    nsRect  rect(0, 0, aDesiredSize.width, aDesiredSize.height);
    child->SetRect(rect);
  }

  return NS_OK;
}

NS_METHOD PageFrame::CreateContinuingFrame(nsIPresContext* aPresContext,
                                           nsIFrame*       aParent,
                                           nsIFrame*&      aContinuingFrame)
{
  PageFrame* cf = new PageFrame(mContent, mIndexInParent, aParent);
  PrepareContinuingFrame(aPresContext, aParent, cf);
  aContinuingFrame = cf;
  return NS_OK;
}

NS_METHOD PageFrame::Paint(nsIPresContext&      aPresContext,
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

NS_METHOD PageFrame::ListTag(FILE* out) const
{
  fprintf(out, "*PAGE@%p", this);
  return NS_OK;
}

