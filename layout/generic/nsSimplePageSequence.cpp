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
 
#include "nsSimplePageSequence.h"
#include "nsPageFrame.h"
#include "nsIPresContext.h"
#include "nsIReflowCommand.h"
#include "nsIRenderingContext.h"
#include "nsIStyleContext.h"
#include "nsHTMLAtoms.h"

NS_IMETHODIMP
nsSimplePageSequenceFrame::SetInitialChildList(nsIPresContext& aPresContext,
                                               nsIAtom*        aListName,
                                               nsIFrame*       aChildList)
{
  // Create a page frame and initialize it
  mFirstChild = new nsPageFrame;

  // XXX This is all wrong...
  nsIStyleContext* pseudoStyleContext =
   aPresContext.ResolvePseudoStyleContextFor(mContent, nsHTMLAtoms::columnPseudo, mStyleContext);
  mFirstChild->Init(aPresContext, mContent, this, pseudoStyleContext);
  NS_RELEASE(pseudoStyleContext);

  // Set the geometric and content parent for each of the child frames to be the
  // page frame
  for (nsIFrame* frame = aChildList; nsnull != frame; frame->GetNextSibling(frame)) {
    frame->SetGeometricParent(mFirstChild);
    frame->SetContentParent(mFirstChild);
  }

  // Queue up the frames for the page frame
  return mFirstChild->SetInitialChildList(aPresContext, nsnull, aChildList);
}

// XXX Hack
#define PAGE_SPACING_TWIPS 100

NS_IMETHODIMP
nsSimplePageSequenceFrame::Reflow(nsIPresContext&          aPresContext,
                                  nsHTMLReflowMetrics&     aDesiredSize,
                                  const nsHTMLReflowState& aReflowState,
                                  nsReflowStatus&          aStatus)
{
  NS_FRAME_TRACE_REFLOW_IN("nsSimplePageSequenceFrame::Reflow");

  aStatus = NS_FRAME_COMPLETE;

  // Compute the size of each page and the x coordinate that each page will
  // be placed at
  nsSize  pageSize(aPresContext.GetPageWidth(), aPresContext.GetPageHeight());
  PRInt32 extra = aReflowState.maxSize.width - 2*PAGE_SPACING_TWIPS - pageSize.width;

  // Note: nscoord is an unsigned type so don't combine these
  // two statements or the extra will be promoted to unsigned
  // and the >0 won't work!
  nscoord x = PAGE_SPACING_TWIPS;
  if (extra > 0) {
    x += extra / 2;
  }

  // Running y-offset for each page
  nscoord y = PAGE_SPACING_TWIPS;

  if (eReflowReason_Incremental == aReflowState.reason) {
    // We don't expect the target of the reflow command to be the root
    // content frame
#ifdef NS_DEBUG
    nsIFrame* targetFrame;
    aReflowState.reflowCommand->GetTarget(targetFrame);
    NS_ASSERTION(targetFrame != this, "simple page sequence frame is reflow command target");
#endif
  
    // Get the next child frame in the target chain
    nsIFrame* nextFrame;
    aReflowState.reflowCommand->GetNext(nextFrame);

    // Compute the y-offset of this page
    for (nsIFrame* f = mFirstChild; f != nextFrame; f->GetNextSibling(f)) {
      nsSize  size;

      f->GetSize(size);
      y += size.height + PAGE_SPACING_TWIPS;
    }

    // Reflow the page
    nsHTMLReflowState   kidReflowState(aPresContext, nextFrame, aReflowState, pageSize);
    nsHTMLReflowMetrics kidSize(nsnull);
  
    // Dispatch the reflow command to our child frame. Allow it to be as high
    // as it wants
    ReflowChild(nextFrame, aPresContext, kidSize, kidReflowState, aStatus);
  
    // Place and size the page. If the page is narrower than our max width then
    // center it horizontally
    nsRect  rect(x, y, kidSize.width, kidSize.height);
    nextFrame->SetRect(rect);

    // XXX Check if the page is complete...

    // Update the y-offset to reflect the remaining pages
    nextFrame->GetNextSibling(nextFrame);
    while (nsnull != nextFrame) {
      nsSize  size;

      nextFrame->GetSize(size);
      y += size.height + PAGE_SPACING_TWIPS;
      nextFrame->GetNextSibling(nextFrame);
    }

  } else {
    nsReflowReason  reflowReason = aReflowState.reason;

    // Tile the pages vertically
    nsHTMLReflowMetrics kidSize(nsnull);
    for (nsIFrame* kidFrame = mFirstChild; nsnull != kidFrame; ) {
      // Reflow the page
      nsHTMLReflowState kidReflowState(aPresContext, kidFrame,
                                       aReflowState, pageSize,
                                       reflowReason);
      nsReflowStatus  status;

      // Place and size the page. If the page is narrower than our
      // max width then center it horizontally
      ReflowChild(kidFrame, aPresContext, kidSize, kidReflowState, status);
      kidFrame->SetRect(nsRect(x, y, kidSize.width, kidSize.height));
      y += kidSize.height;

      // Leave a slight gap between the pages
      y += PAGE_SPACING_TWIPS;

      // Is the page complete?
      nsIFrame* kidNextInFlow;
       
      kidFrame->GetNextInFlow(kidNextInFlow);
      if (NS_FRAME_IS_COMPLETE(status)) {
        NS_ASSERTION(nsnull == kidNextInFlow, "bad child flow list");
      } else if (nsnull == kidNextInFlow) {
        // The page isn't complete and it doesn't have a next-in-flow so
        // create a continuing page
        nsIStyleContext* kidSC;
        kidFrame->GetStyleContext(kidSC);
        nsIFrame*  continuingPage;
        nsresult rv = kidFrame->CreateContinuingFrame(aPresContext, this,
                                                      kidSC, continuingPage);
        NS_RELEASE(kidSC);
        reflowReason = eReflowReason_Initial;

        // Add it to our child list
#ifdef NS_DEBUG
        nsIFrame* kidNextSibling;
        kidFrame->GetNextSibling(kidNextSibling);
        NS_ASSERTION(nsnull == kidNextSibling, "unexpected sibling");
#endif
        kidFrame->SetNextSibling(continuingPage);
      }

      // Get the next page
      kidFrame->GetNextSibling(kidFrame);
    }
  }

  // Return our desired size
  aDesiredSize.height = y;
  aDesiredSize.width = 2*PAGE_SPACING_TWIPS + pageSize.width;
  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;

  NS_FRAME_TRACE_REFLOW_OUT("nsSimplePageSequeceFrame::Reflow", aStatus);
  return NS_OK;
}

NS_IMETHODIMP
nsSimplePageSequenceFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("SimplePageSequence", aResult);
}

NS_IMETHODIMP
nsSimplePageSequenceFrame::Paint(nsIPresContext&      aPresContext,
                                 nsIRenderingContext& aRenderingContext,
                                 const nsRect&        aDirtyRect)
{
  // Paint a white background
  // XXX Crop marks or hash marks would be nice. Use style info...
  aRenderingContext.SetColor(NS_RGB(255,255,255));
  aRenderingContext.FillRect(aDirtyRect);
  return nsContainerFrame::Paint(aPresContext, aRenderingContext, aDirtyRect);
}

//----------------------------------------------------------------------

nsresult
NS_NewSimplePageSequenceFrame(nsIFrame*& aResult)
{
  nsSimplePageSequenceFrame*  it = new nsSimplePageSequenceFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aResult = it;
  return NS_OK;
}

