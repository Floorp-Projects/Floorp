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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsCOMPtr.h" 
#include "nsSimplePageSequence.h"
#include "nsIPresContext.h"
#include "nsIReflowCommand.h"
#include "nsIRenderingContext.h"
#include "nsIStyleContext.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIDeviceContext.h"
#include "nsIViewManager.h"
#include "nsIPresShell.h"
#include "nsIStyleSet.h"

nsresult
NS_NewSimplePageSequenceFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsSimplePageSequenceFrame*  it = new (aPresShell) nsSimplePageSequenceFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsSimplePageSequenceFrame::nsSimplePageSequenceFrame()
{
}

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

// Creates a continuing page frame
nsresult
nsSimplePageSequenceFrame::CreateContinuingPageFrame(nsIPresContext* aPresContext,
                                                     nsIFrame*       aPageFrame,
                                                     nsIFrame**      aContinuingPage)
{
  nsIPresShell* presShell;
  nsIStyleSet*  styleSet;
  nsresult      rv;

  // Create the continuing frame
  aPresContext->GetShell(&presShell);
  presShell->GetStyleSet(&styleSet);
  NS_RELEASE(presShell);
  rv = styleSet->CreateContinuingFrame(aPresContext, aPageFrame, this, aContinuingPage);
  NS_RELEASE(styleSet);
  return rv;
}

// Handles incremental reflow
nsresult
nsSimplePageSequenceFrame::IncrementalReflow(nsIPresContext*          aPresContext,
                                             const nsHTMLReflowState& aReflowState,
                                             nsSize&                  aPageSize,
                                             nscoord                  aX,
                                             nscoord&                 aY)
{
  // We don't expect the target of the reflow command to be the simple
  // page sequence frame
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
       f->GetNextSibling(&f)) {
    nsSize  size;

    f->GetSize(size);
    aY += size.height + PAGE_SPACING_TWIPS;
  }

  // Reflow the page
  nsHTMLReflowState   kidReflowState(aPresContext, aReflowState,
                                     nextFrame, aPageSize);
  nsHTMLReflowMetrics kidSize(nsnull);
  nsReflowStatus      status;

  // Dispatch the reflow command to our child frame. Allow it to be as high
  // as it wants
  ReflowChild(nextFrame, aPresContext, kidSize, kidReflowState, aX, aY, 0, status);

  // Place and size the page. If the page is narrower than our max width, then
  // center it horizontally
  FinishReflowChild(nextFrame, aPresContext, kidSize, aX, aY, 0);
  aY += kidSize.height + PAGE_SPACING_TWIPS;

  // Check if the page is complete...
  nsIFrame* kidNextInFlow;
  nextFrame->GetNextInFlow(&kidNextInFlow);

  if (NS_FRAME_IS_COMPLETE(status)) {
    NS_ASSERTION(nsnull == kidNextInFlow, "bad child flow list");
  } else {
    nsReflowReason  reflowReason = eReflowReason_Resize;
    
    if (!kidNextInFlow) {
      // The page isn't complete and it doesn't have a next-in-flow so
      // create a continuing page
      nsIFrame* continuingPage;
      CreateContinuingPageFrame(aPresContext, nextFrame, &continuingPage);

      // Add it to our child list
      nextFrame->SetNextSibling(continuingPage);
      reflowReason = eReflowReason_Initial;
    }

    // Reflow the remaining pages
    // XXX Ideally we would only reflow the next page if the current page indicated
    // its next-in-flow was dirty...
    nsIFrame* kidFrame;
    nextFrame->GetNextSibling(&kidFrame);

    while (kidFrame) {
      // Reflow the page
      nsHTMLReflowMetrics childSize(nsnull);
      nsHTMLReflowState childReflowState(aPresContext, aReflowState, kidFrame,
                                         aPageSize, reflowReason);

      // Place and size the page. If the page is narrower than our
      // max width then center it horizontally
      ReflowChild(kidFrame, aPresContext, childSize, childReflowState,
                  aX, aY, 0, status);

      FinishReflowChild(kidFrame, aPresContext, childSize, aX, aY, 0);
      aY += childSize.height;

      // Leave a slight gap between the pages
      aY += PAGE_SPACING_TWIPS;

      // Is the page complete?
      kidFrame->GetNextInFlow(&kidNextInFlow);

      if (NS_FRAME_IS_COMPLETE(status)) {
        NS_ASSERTION(nsnull == kidNextInFlow, "bad child flow list");
      } else if (nsnull == kidNextInFlow) {
        // The page isn't complete and it doesn't have a next-in-flow, so
        // create a continuing page
        nsIFrame*     continuingPage;
        CreateContinuingPageFrame(aPresContext, kidFrame, &continuingPage);

        // Add it to our child list
        kidFrame->SetNextSibling(continuingPage);
        reflowReason = eReflowReason_Initial;
      }

      // Get the next page
      kidFrame->GetNextSibling(&kidFrame);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSimplePageSequenceFrame::Reflow(nsIPresContext*          aPresContext,
                                  nsHTMLReflowMetrics&     aDesiredSize,
                                  const nsHTMLReflowState& aReflowState,
                                  nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsSimplePageSequenceFrame", aReflowState.reason);
  NS_FRAME_TRACE_REFLOW_IN("nsSimplePageSequenceFrame::Reflow");

  aStatus = NS_FRAME_COMPLETE;  // we're always complete

  // Compute the size of each page and the x coordinate that each page will
  // be placed at
  nsSize  pageSize;
  aPresContext->GetPageWidth(&pageSize.width);
  aPresContext->GetPageHeight(&pageSize.height);
  PRInt32 extra = aReflowState.availableWidth - 2 * PAGE_SPACING_TWIPS - pageSize.width;

  nscoord x = PAGE_SPACING_TWIPS;
  if (extra > 0) {
    x += extra / 2;
  }

  // Running y-offset for each page
  nscoord y = PAGE_SPACING_TWIPS;

  // See if it's an incremental reflow command
  if (eReflowReason_Incremental == aReflowState.reason) {
    IncrementalReflow(aPresContext, aReflowState, pageSize, x, y);

  } else {
    nsReflowReason  reflowReason = aReflowState.reason;

    // Tile the pages vertically
    nsHTMLReflowMetrics kidSize(nsnull);
    for (nsIFrame* kidFrame = mFrames.FirstChild(); nsnull != kidFrame; ) {
      // Reflow the page
      nsHTMLReflowState kidReflowState(aPresContext, aReflowState, kidFrame,
                                       pageSize, reflowReason);
      nsReflowStatus  status;

      // Place and size the page. If the page is narrower than our
      // max width then center it horizontally
      ReflowChild(kidFrame, aPresContext, kidSize, kidReflowState, x, y, 0, status);

      FinishReflowChild(kidFrame, aPresContext, kidSize, x, y, 0);
      y += kidSize.height;

      // Leave a slight gap between the pages
      y += PAGE_SPACING_TWIPS;

      // Is the page complete?
      nsIFrame* kidNextInFlow;
      kidFrame->GetNextInFlow(&kidNextInFlow);

      if (NS_FRAME_IS_COMPLETE(status)) {
        NS_ASSERTION(nsnull == kidNextInFlow, "bad child flow list");
      } else if (nsnull == kidNextInFlow) {
        // The page isn't complete and it doesn't have a next-in-flow, so
        // create a continuing page
        nsIFrame*     continuingPage;
        CreateContinuingPageFrame(aPresContext, kidFrame, &continuingPage);

        // Add it to our child list
        kidFrame->SetNextSibling(continuingPage);
        reflowReason = eReflowReason_Initial;
      }

      // Get the next page
      kidFrame->GetNextSibling(&kidFrame);
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

#ifdef DEBUG
NS_IMETHODIMP
nsSimplePageSequenceFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("SimplePageSequence", aResult);
}
#endif

NS_IMETHODIMP
nsSimplePageSequenceFrame::Paint(nsIPresContext*      aPresContext,
                                 nsIRenderingContext& aRenderingContext,
                                 const nsRect&        aDirtyRect,
                                 nsFramePaintLayer    aWhichLayer)
{
  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
    // Paint a white background
    aRenderingContext.SetColor(NS_RGB(255,255,255));
    aRenderingContext.FillRect(aDirtyRect);
    // XXX Crop marks or hash marks would be nice. Use style info...
  }

  return nsContainerFrame::Paint(aPresContext, aRenderingContext, aDirtyRect,
                                 aWhichLayer);
}

void
nsSimplePageSequenceFrame::PaintChild(nsIPresContext*      aPresContext,
                                      nsIRenderingContext& aRenderingContext,
                                      const nsRect&        aDirtyRect,
                                      nsIFrame*            aFrame,
                                      nsFramePaintLayer    aWhichLayer)
{
  // Let the page paint
  nsContainerFrame::PaintChild(aPresContext, aRenderingContext,
                               aDirtyRect, aFrame, aWhichLayer);

  if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {
    // XXX Paint a one-pixel border around the page so it's easy to see where
    // each page begins and ends when we're in print preview mode
    nsRect  pageBounds;
    float   p2t;
    aPresContext->GetPixelsToTwips(&p2t);

    aRenderingContext.SetColor(NS_RGB(0, 0, 0));
    aFrame->GetRect(pageBounds);
    pageBounds.Inflate(NSToCoordRound(p2t), NSToCoordRound(p2t));
    
    // this paints a rectangle around the for the printer output, 
    // which sometimes appears, other times
    // goes away.  I have to look into this further or ask troy.. 
    // I took it out for the time being to fix some bugs -- dwc
    //aRenderingContext.DrawRect(pageBounds);
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
nsSimplePageSequenceFrame::Print(nsIPresContext*         aPresContext,
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
  nsCOMPtr<nsIDeviceContext> dc;
  aPresContext->GetDeviceContext(getter_AddRefs(dc));
  nsCOMPtr<nsIPresShell>     presShell;
  aPresContext->GetShell(getter_AddRefs(presShell));
  nsCOMPtr<nsIViewManager>   vm;
  presShell->GetViewManager(getter_AddRefs(vm));
  nsresult          rv = NS_OK;

  // Print each specified page
  PRInt32 pageNum = 1;
  for (nsIFrame* page = mFrames.FirstChild(); nsnull != page; page->GetNextSibling(&page)) {
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
      page->GetView(aPresContext, &view);
      NS_ASSERTION(nsnull != view, "no page view");
      vm->Display(view);

      // this view was printed and since display set the origin 
      // 0,0 there is a danger that this view can be printed again
      // If it is a sibling to another page/view.  Setting the visibility
      // to hide will keep this page from printing again - dwc
      view->SetVisibility(nsViewVisibility_kHide);
  
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

  return rv;
}
