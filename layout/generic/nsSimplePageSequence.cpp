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
#include "nsHTMLIIDs.h"
#include "nsIDeviceContext.h"
#include "nsIViewManager.h"
#include "nsIPresShell.h"

nsresult
nsSimplePageSequenceFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(0 != aInstancePtr, "null ptr");
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIPageSequenceFrameIID)) {
    *aInstancePtr = (void*)(nsIPageSequenceFrame*)this;
    return NS_OK;
  }
  return nsContainerFrame::QueryInterface(aIID, aInstancePtr);
}

//----------------------------------------------------------------------

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
  PRInt32 extra = aReflowState.availableWidth - 2 * PAGE_SPACING_TWIPS - pageSize.width;

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
    for (nsIFrame* f = mFrames.FirstChild(); f != nextFrame;
         f->GetNextSibling(f)) {
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
    for (nsIFrame* kidFrame = mFrames.FirstChild(); nsnull != kidFrame; ) {
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
        kidFrame->GetStyleContext(&kidSC);
        nsIFrame*  continuingPage;
        nsresult rv = kidFrame->CreateContinuingFrame(aPresContext, this,
                                                      kidSC, continuingPage);
        nsHTMLContainerFrame::CreateViewForFrame(aPresContext, continuingPage,
                                                 kidSC, PR_TRUE);
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

//----------------------------------------------------------------------

NS_IMETHODIMP
nsSimplePageSequenceFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("SimplePageSequence", aResult);
}

NS_IMETHODIMP
nsSimplePageSequenceFrame::Paint(nsIPresContext&      aPresContext,
                                 nsIRenderingContext& aRenderingContext,
                                 const nsRect&        aDirtyRect,
                                 nsFramePaintLayer    aWhichLayer)
{
  if (eFramePaintLayer_Underlay == aWhichLayer) {
    // Paint a white background
    aRenderingContext.SetColor(NS_RGB(255,255,255));
    aRenderingContext.FillRect(aDirtyRect);
    // XXX Crop marks or hash marks would be nice. Use style info...
  }

  return nsContainerFrame::Paint(aPresContext, aRenderingContext, aDirtyRect,
                                 aWhichLayer);
}

void
nsSimplePageSequenceFrame::PaintChild(nsIPresContext&      aPresContext,
                                      nsIRenderingContext& aRenderingContext,
                                      const nsRect&        aDirtyRect,
                                      nsIFrame*            aFrame,
                                      nsFramePaintLayer    aWhichLayer)
{
  // Let the page paint
  nsContainerFrame::PaintChild(aPresContext, aRenderingContext,
                               aDirtyRect, aFrame, aWhichLayer);

  if (eFramePaintLayer_Overlay == aWhichLayer) {
    // XXX Paint a one-pixel border around the page so it's easy to see where
    // each page begins and ends when we're in print preview mode
    nsRect  pageBounds;
    float   p2t = aPresContext.GetPixelsToTwips();

    aRenderingContext.SetColor(NS_RGB(0, 0, 0));
    aFrame->GetRect(pageBounds);
    pageBounds.Inflate(NSToCoordRound(p2t), NSToCoordRound(p2t));
    aRenderingContext.DrawRect(pageBounds);
  }
}

//----------------------------------------------------------------------

// Helper function that sends the progress notification. Returns PR_TRUE
// if printing should continue and PR_FALSE otherwise
static PRBool
SendStatusNotification(nsIPrintStatusCallback* aStatusCallback,
                       PRInt32                 aPageNumber,
                       PRInt32                 aTotalPages,
                       nsPrintStatus           aStatus)
{
  PRBool  ret = PR_TRUE;

  if (nsnull != aStatusCallback) {
    aStatusCallback->OnProgress(aPageNumber, aTotalPages, aStatus,ret);
  }

  return ret;
}

NS_IMETHODIMP
nsSimplePageSequenceFrame::Print(nsIPresContext&         aPresContext,
                                 const nsPrintOptions&   aPrintOptions,
                                 nsIPrintStatusCallback* aStatusCallback)
{
  // If printing a range of pages make sure at least the starting page
  // number is valid
  PRInt32 totalPages = mFrames.GetLength();

  if (ePrintRange_SpecifiedRange == aPrintOptions.range) {
    if (aPrintOptions.startPage > totalPages) {
      return NS_ERROR_INVALID_ARG;
    }
  }

  // Begin printing of the document
  nsIDeviceContext* dc = aPresContext.GetDeviceContext();
  nsIPresShell*     presShell = aPresContext.GetShell();
  nsIViewManager*   vm = presShell->GetViewManager();
  nsresult          rv = NS_OK;
  NS_RELEASE(presShell);

  // Print each specified page
  PRInt32 pageNum = 1;
  for (nsIFrame* page = mFrames.FirstChild(); nsnull != page; page->GetNextSibling(page)) {
    // See whether we should print this page
    PRBool  printThisPage = PR_TRUE;

    // If printing a range of pages check whether the page number is in the
    // range of pages to print
    if (ePrintRange_SpecifiedRange == aPrintOptions.range) {
      if (pageNum < aPrintOptions.startPage) {
        printThisPage = PR_FALSE;
      } else if (pageNum > aPrintOptions.endPage) {
        break;
      }
    }

    // Check for printing of odd and even pages
    if (pageNum & 0x1) {
      if (!aPrintOptions.oddNumberedPages) {
        printThisPage = PR_FALSE;  // don't print odd numbered page
      }
    } else {
      if (!aPrintOptions.evenNumberedPages) {
        printThisPage = PR_FALSE;  // don't print even numbered page
      }
    }

    if (printThisPage) {
      // Start printing of the page
      if (!SendStatusNotification(aStatusCallback, pageNum, totalPages,
                                  ePrintStatus_StartPage)) {
        rv = NS_ERROR_ABORT;
        break;
      }
      rv = dc->BeginPage();
      if (NS_FAILED(rv)) {
        if (nsnull != aStatusCallback) {
          aStatusCallback->OnError(ePrintError_Error);
        }
        break;
      }
  
      // Print the page
      nsIView*  view;
      page->GetView(&view);
      NS_ASSERTION(nsnull != view, "no page view");
      vm->Display(view);
  
      // Finish printing of the page
      if (!SendStatusNotification(aStatusCallback, pageNum, totalPages,
                                  ePrintStatus_EndPage)) {
        rv = NS_ERROR_ABORT;
        break;
      }
      rv = dc->EndPage();
      if (NS_FAILED(rv)) {
        if (nsnull != aStatusCallback) {
          aStatusCallback->OnError(ePrintError_Error);
        }
        break;
      }
    }

    // Increment the page number
    pageNum++;
  }

  NS_RELEASE(vm);
  NS_RELEASE(dc);
  return rv;
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

