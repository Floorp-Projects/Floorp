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
#include "nsIFontMetrics.h"
#include "nsIPrintOptions.h"
#include "nsPageFrame.h"

#define OFFSET_NOT_SET -1

// This is for localization of the "x of n" pages string
// this class contains a helper method we need to get 
// a string from a string bundle
#include "nsFormControlHelper.h"
#define PRINTING_PROPERTIES "chrome://communicator/locale/printing.properties"

// Print Options
#include "nsIPrintOptions.h"
#include "nsGfxCIID.h"
#include "nsIServiceManager.h"
static NS_DEFINE_CID(kPrintOptionsCID, NS_PRINTOPTIONS_CID);
//

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

nsSimplePageSequenceFrame::nsSimplePageSequenceFrame() :
  mIsPrintingSelection(PR_FALSE)
{
  mStartOffset = OFFSET_NOT_SET;
  mEndOffset   = OFFSET_NOT_SET;

  nscoord halfInch = NS_INCHES_TO_TWIPS(0.5);
  mMargin.SizeTo(halfInch, halfInch, halfInch, halfInch);

  // XXX this code and the object data member "mIsPrintingSelection" is only needed
  // for the hack for printing selection where we make the page the max size
  nsresult rv;
  NS_WITH_SERVICE(nsIPrintOptions, printService, kPrintOptionsCID, &rv);
  if (NS_SUCCEEDED(rv) && printService) {
    PRInt16 printType;
    printService->GetPrintRange(&printType);
    mIsPrintingSelection = nsIPrintOptions::kRangeSelection == printType;
  }

}

nsSimplePageSequenceFrame::~nsSimplePageSequenceFrame()
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
  for (nsIFrame* f = mFrames.FirstChild(); f != nextFrame; f->GetNextSibling(&f)) {
    nsSize  size;
    f->GetSize(size);
    aY += size.height + mMargin.top + mMargin.bottom;
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
  aY += kidSize.height + mMargin.top + mMargin.bottom;

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
      aY += mMargin.top + mMargin.bottom;

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

  // XXX - Hack Alert
  // OK,  so ther eis a selection, we will print the entire selection 
  // on one page and then crop the page.
  // This means you can never print any selection that is longer than one page
  // put it keeps it from page breaking in the middle of your print of the selection
  // (see also nsDocumentViewer.cpp)
  if (mIsPrintingSelection) {
    pageSize.height = 0x0FFFFFFF;
  }

  nscoord x = mMargin.left;

  // Running y-offset for each page
  nscoord y = mMargin.top;

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
      kidReflowState.availableWidth  = pageSize.width - mMargin.left - mMargin.right;
      kidReflowState.availableHeight = pageSize.height - mMargin.top - mMargin.bottom;
      kidReflowState.mComputedWidth  = kidReflowState.availableWidth;
      //kidReflowState.mComputedHeight = kidReflowState.availableHeight;

      // Place and size the page. If the page is narrower than our
      // max width then center it horizontally
      ReflowChild(kidFrame, aPresContext, kidSize, kidReflowState, x, y, 0, status);

      FinishReflowChild(kidFrame, aPresContext, kidSize, x, y, 0);
      y += kidSize.height;

      nsIView*  view;
      kidFrame->GetView(aPresContext, &view);
      NS_ASSERTION(nsnull != view, "no page view");
      nsRect rect;
      kidFrame->GetRect(rect);
      nsRect viewRect;
      view->GetBounds(viewRect);

      // Leave a slight gap between the pages
      y += mMargin.top + mMargin.bottom;

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
  aDesiredSize.height  = y;
  aDesiredSize.width   = pageSize.width;
  aDesiredSize.ascent  = aDesiredSize.height;
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
                                 nsIPrintOptions*        aPrintOptions,
                                 nsIPrintStatusCallback* aStatusCallback)
{
  NS_ENSURE_ARG_POINTER(aPresContext);
  NS_ENSURE_ARG_POINTER(aPrintOptions);

  PRInt32 printRangeType;
  PRInt32 fromPageNum;
  PRInt32 toPageNum;
  PRBool  printEvenPages, printOddPages;

  PRInt16 printType;
  aPrintOptions->GetPrintRange(&printType);
  printRangeType = printType;
  aPrintOptions->GetStartPageRange(&fromPageNum);
  aPrintOptions->GetEndPageRange(&toPageNum);
  aPrintOptions->GetMarginInTwips(mMargin);

  aPrintOptions->GetPrintOptions(nsIPrintOptions::kOptPrintEvenPages, &printEvenPages);
  aPrintOptions->GetPrintOptions(nsIPrintOptions::kOptPrintOddPages, &printOddPages);

  PRBool doingPageRange = nsIPrintOptions::kRangeSpecifiedPageRange == printRangeType ||
                          nsIPrintOptions::kRangeSelection == printRangeType;

  // If printing a range of pages make sure at least the starting page
  // number is valid
  PRInt32 totalPages = mFrames.GetLength();

  if (doingPageRange) {
    if (fromPageNum > totalPages) {
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

#if defined(DEBUG_rods) || defined(DEBUG_dcone)
  {
    nsIView * seqView;
    GetView(aPresContext, &seqView);
    nsRect rect;
    GetRect(rect);
    printf("Seq Frame: - %d,%d,%d,%d ", rect.x, rect.y, rect.width, rect.height);
    printf("view: %p ", seqView);
    nsRect viewRect;
    if (seqView) {
      seqView->GetBounds(viewRect);
      printf(" %d,%d,%d,%d", viewRect.x, viewRect.y, viewRect.width, viewRect.height);
    }
    printf("\n");
  }

  {
    PRInt32 pageNum = 1;
    for (nsIFrame* page = mFrames.FirstChild(); nsnull != page; page->GetNextSibling(&page)) {
      nsIView*  view;
      page->GetView(aPresContext, &view);
      NS_ASSERTION(nsnull != view, "no page view");
      nsRect rect;
      page->GetRect(rect);
      nsRect viewRect;
      view->GetBounds(viewRect);
      printf("Page: %d - %d,%d,%d,%d ", pageNum, rect.x, rect.y, rect.width, rect.height);
      printf(" %d,%d,%d,%d\n", viewRect.x, viewRect.y, viewRect.width, viewRect.height);
      pageNum++;
    }
  }
  printf("***** Setting aPresContext %p is painting selection %d\n", aPresContext, nsIPrintOptions::kRangeSelection == printRangeType);
#endif

  // Determine if we are rendering only the selection
  aPresContext->SetIsRenderingOnlySelection(nsIPrintOptions::kRangeSelection == printRangeType);


  if (doingPageRange) {
    // XXX because of the hack for making the selection all print on one page
    // we must make sure that the page is sized correctly before printing.
    PRInt32 width,height;
    dc->GetDeviceSurfaceDimensions(width,height);
    height -= mMargin.top + mMargin.bottom;

    PRInt32 pageNum = 1;
    nscoord y = mMargin.top;
    for (nsIFrame* page = mFrames.FirstChild(); nsnull != page; page->GetNextSibling(&page)) {
      nsIView*  view;
      page->GetView(aPresContext, &view);
      NS_ASSERTION(nsnull != view, "no page view");
      if (pageNum < fromPageNum || pageNum > toPageNum) {
        // XXX Doesn't seem like we need to do this
        // because we ask only the pages we want to print
        //view->SetVisibility(nsViewVisibility_kHide);
      } else {
        nsRect rect;
        page->GetRect(rect);
        rect.y = y;
        rect.height = height;
        page->SetRect(aPresContext, rect);
        nsRect viewRect;
        view->GetBounds(viewRect);
        viewRect.y = y;
        viewRect.height = height;
        view->SetBounds(viewRect);
        y += rect.height + mMargin.top + mMargin.bottom;
      }
      pageNum++;
    }

    // adjust total number of pages
    if (nsIPrintOptions::kRangeSelection == printRangeType) {
      totalPages = toPageNum - fromPageNum + 1;
    } else {
      totalPages = pageNum - 1;
    }
  }

  // XXX - This wouldn't have to be done each time
  // but it isn't that expensive and this the best place 
  // to have access to a localized file properties file
  // 
  // Note: because this is done here it makes a little bit harder
  // to have UI for setting the header/footer font name and size
  //
  // Get default font name and size to be used for the headers and footers
  nsAutoString fontName;
  rv = nsFormControlHelper::GetLocalizedString(PRINTING_PROPERTIES, "fontname", fontName);
  if (NS_FAILED(rv)) {
    fontName.AssignWithConversion("serif");
  }

  nsAutoString fontSizeStr;
  nscoord      pointSize = 10;;
  rv = nsFormControlHelper::GetLocalizedString(PRINTING_PROPERTIES, "fontsize", fontSizeStr);
  if (NS_SUCCEEDED(rv)) {
    PRInt32 errCode;
    pointSize = fontSizeStr.ToInteger(&errCode);
    if (NS_FAILED(errCode)) {
      pointSize = 10;
    }
  }
  aPrintOptions->SetFontNamePointSize(fontName, pointSize);

  // Now go get the Localized Page Formating String
  PRBool doingPageTotals = PR_TRUE;
  aPrintOptions->GetPrintOptions(nsIPrintOptions::kOptPrintPageTotal, &doingPageTotals);

  nsAutoString pageFormatStr;
  rv = nsFormControlHelper::GetLocalizedString(PRINTING_PROPERTIES, 
                                               doingPageTotals?"pageofpages":"pagenumber",
                                               pageFormatStr);
  if (NS_FAILED(rv)) { // back stop formatting
    pageFormatStr.AssignWithConversion(doingPageTotals?"%ld of %ld":"%ld");
  }
  // Sets the format into a static data memeber which will own the memory and free it
  nsPageFrame::SetPageNumberFormat(pageFormatStr.ToNewUnicode());

  // Print each specified page
  // pageNum keeps track of the current page and what pages are printing
  //
  // printedPageNum keeps track of the current page number to be printed
  // Note: When print al the pages or a page range the printed page shows the
  // actual page number, when printing selection it prints the page number starting
  // with the first page of the selection. For example if the user has a 
  // selection that starts on page 2 and ends on page 3, the page numbers when
  // print are 1 and then two (which is different than printing a page range, where
  // the page numbers would have been 2 and then 3)
  PRInt32 pageNum        = 1;
  PRInt32 printedPageNum = 1;
  for (nsIFrame* page = mFrames.FirstChild(); nsnull != page; page->GetNextSibling(&page)) {
    // See whether we should print this page
    PRBool  printThisPage = PR_TRUE;

    // If printing a range of pages check whether the page number is in the
    // range of pages to print
    if (doingPageRange) {
      if (pageNum < fromPageNum) {
        printThisPage = PR_FALSE;
      } else if (pageNum > toPageNum) {
        break;
      }
    }

    // Check for printing of odd and even pages
    if (pageNum & 0x1) {
      if (!printOddPages) {
        printThisPage = PR_FALSE;  // don't print odd numbered page
      }
    } else {
      if (!printEvenPages) {
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

      // cast the frame to be a page frame
      nsPageFrame * pf = NS_STATIC_CAST(nsPageFrame*, page);
      if (pf != nsnull) {
        pf->SetPrintOptions(aPrintOptions);
        pf->SetPageNumInfo(printedPageNum, totalPages);
      }

      // Print the page
      nsIView*  view;
      page->GetView(aPresContext, &view);

      NS_ASSERTION(nsnull != view, "no page view");
#if defined(DEBUG_rods) || defined(DEBUG_dcone)
      {
        char * fontname = fontName.ToNewCString();
        printf("SPSF::Paint -> FontName[%s]  Point Size: %d\n", fontname, pointSize);
        nsMemory::Free(fontname);
        printf("SPSF::Paint -> PageNo: %d  View: %p\n", pageNum, view);
      }
#endif
      vm->Display(view, mMargin.left, mMargin.top);


      // this view was printed and since display set the origin 
      // 0,0 there is a danger that this view can be printed again
      // If it is a sibling to another page/view.  Setting the visibility
      // to hide will keep this page from printing again - dwc
      //
      // XXX Doesn't seem like we need to do this anymore
      //view->SetVisibility(nsViewVisibility_kHide);
  
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

    if (nsIPrintOptions::kRangeSelection != printRangeType ||
        (nsIPrintOptions::kRangeSelection == printRangeType && printThisPage)) {
      printedPageNum++;
    }

    pageNum++;
  }

  return rv;
}

NS_IMETHODIMP
nsSimplePageSequenceFrame::SetOffsets(nscoord aStartOffset, nscoord aEndOffset)
{
  mStartOffset = aStartOffset;
  mEndOffset   = aEndOffset;
  return NS_OK;
}
