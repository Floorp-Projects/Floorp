/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsCOMPtr.h" 
#include "nsReadableUtils.h"
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

// DateTime Includes
#include "nsDateTimeFormatCID.h"
#include "nsIDateTimeFormat.h"
#include "nsIServiceManager.h"
#include "nsILocale.h"
#include "nsLocaleCID.h"
#include "nsILocaleService.h"
static NS_DEFINE_CID(kDateTimeFormatCID, NS_DATETIMEFORMAT_CID);
static NS_DEFINE_CID(kLocaleServiceCID, NS_LOCALESERVICE_CID); 

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

#if defined(DEBUG_rods) || defined(DEBUG_dcone)
#define DEBUG_PRINTING
#endif

#ifdef DEBUG_PRINTING
#define PRINT_DEBUG_MSG1(_msg1) fprintf(mDebugFD, (_msg1))
#define PRINT_DEBUG_MSG2(_msg1, _msg2) fprintf(mDebugFD, (_msg1), (_msg2))
#define PRINT_DEBUG_MSG3(_msg1, _msg2, _msg3) fprintf(mDebugFD, (_msg1), (_msg2), (_msg3))
#define PRINT_DEBUG_MSG4(_msg1, _msg2, _msg3, _msg4) fprintf(mDebugFD, (_msg1), (_msg2), (_msg3), (_msg4))
#define PRINT_DEBUG_MSG5(_msg1, _msg2, _msg3, _msg4, _msg5) fprintf(mDebugFD, (_msg1), (_msg2), (_msg3), (_msg4), (_msg5))
#else //--------------
#define PRINT_DEBUG_MSG1(_msg) 
#define PRINT_DEBUG_MSG2(_msg1, _msg2) 
#define PRINT_DEBUG_MSG3(_msg1, _msg2, _msg3) 
#define PRINT_DEBUG_MSG4(_msg1, _msg2, _msg3, _msg4) 
#define PRINT_DEBUG_MSG5(_msg1, _msg2, _msg3, _msg4, _msg5) 
#endif

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
  nsCOMPtr<nsIPrintOptions> printService = 
           do_GetService(kPrintOptionsCID, &rv);
  if (NS_SUCCEEDED(rv) && printService) {
    PRInt16 printType;
    printService->GetPrintRange(&printType);
    mIsPrintingSelection = nsIPrintOptions::kRangeSelection == printType;
    printService->GetMarginInTwips(mMargin);
  }
  mSkipPageBegin = PR_FALSE;
  mSkipPageEnd   = PR_FALSE;
  mPrintThisPage = PR_FALSE;
  mOffsetX       = 0;
  mOffsetY       = 0;
#ifdef NS_DEBUG
  mDebugFD = stdout;
#endif
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
  nsRect  pageSize;
  nsRect  adjSize;
  aPresContext->GetPageDim(&pageSize, &adjSize);

  PRBool suppressLeftMargin   = PR_FALSE;
  PRBool suppressRightMargin  = PR_FALSE;
  PRBool suppressTopMargin    = PR_FALSE;
  PRBool suppressBottomMargin = PR_FALSE;

  if (pageSize != adjSize &&
      (adjSize.x != 0 || adjSize.y != 0 || adjSize.width != 0 || adjSize.height != 0)) {
    suppressLeftMargin   = pageSize.x != adjSize.x;
    suppressTopMargin    = pageSize.y != adjSize.y;
    if (pageSize.width  != adjSize.width) {
      suppressRightMargin = PR_TRUE;
      pageSize.width = adjSize.width;
    }
    if (pageSize.height != adjSize.height) {
      suppressBottomMargin = PR_TRUE;
      pageSize.height = adjSize.height;
    }
  }

  // XXX - Hack Alert
  // OK,  so ther eis a selection, we will print the entire selection 
  // on one page and then crop the page.
  // This means you can never print any selection that is longer than one page
  // put it keeps it from page breaking in the middle of your print of the selection
  // (see also nsDocumentViewer.cpp)
  if (mIsPrintingSelection) {
    pageSize.height = NS_UNCONSTRAINEDSIZE;
  }

  // *** Special Override ***
  // If this is a sub-sdoc (meaning it doesn't take the whole page)
  // and if this Document is in the upper left hand corner
  // we need to suppress the top margin or it will reflow too small
  // Start by getting the actual printer page dimensions to see if we are not a whole page
  nsCOMPtr<nsIDeviceContext> dc;
  aPresContext->GetDeviceContext(getter_AddRefs(dc));
  NS_ASSERTION(dc, "nsIDeviceContext can't be NULL!");
  nscoord width, height;
  dc->GetDeviceSurfaceDimensions(width, height);

  if (adjSize.x == 0 && adjSize.y == 0 && (pageSize.width != width || pageSize.height != height)) {
    suppressTopMargin = PR_TRUE;
  }

  // only use this local margin for sizing, 
  // not for positioning
  nsMargin margin(suppressLeftMargin?0:mMargin.left,
                  suppressTopMargin?0:mMargin.top,
                  suppressRightMargin?0:mMargin.right,
                  suppressBottomMargin?0:mMargin.bottom);

  nscoord x = mMargin.left;
  nscoord y = mMargin.top;// Running y-offset for each page

  // See if it's an incremental reflow command
  if (eReflowReason_Incremental == aReflowState.reason) {
    // XXX Skip Incremental reflow, 
    // in fact, all we want is the initial reflow
    //IncrementalReflow(aPresContext, aReflowState, pageSize, x, y);

  } else {
    nsReflowReason  reflowReason = aReflowState.reason;

    // Tile the pages vertically
    nsHTMLReflowMetrics kidSize(nsnull);
    for (nsIFrame* kidFrame = mFrames.FirstChild(); nsnull != kidFrame; ) {
      // Reflow the page
      nsSize availSize(pageSize.width, pageSize.height);
      nsHTMLReflowState kidReflowState(aPresContext, aReflowState, kidFrame,
                                       availSize, reflowReason);
      nsReflowStatus  status;
      kidReflowState.availableWidth  = pageSize.width - margin.left - margin.right;
      kidReflowState.availableHeight = pageSize.height - margin.top - margin.bottom;

      kidReflowState.mComputedWidth  = kidReflowState.availableWidth;
      //kidReflowState.mComputedHeight = kidReflowState.availableHeight;
      PRINT_DEBUG_MSG3("AV W: %d   H: %d\n", kidReflowState.availableWidth, kidReflowState.availableHeight);

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

    // Get Total Page Count
    nsIFrame* page;
    PRInt32 pageTot = 0;
    for (page = mFrames.FirstChild(); nsnull != page; page->GetNextSibling(&page)) {
      pageTot++;
    }

    // Set Page Number Info
    PRInt32 pageNum = 1;
    for (page = mFrames.FirstChild(); nsnull != page; page->GetNextSibling(&page)) {
      nsPageFrame * pf = NS_STATIC_CAST(nsPageFrame*, page);
      if (pf != nsnull) {
        //pf->SetPrintOptions(aPrintOptions);
        pf->SetPageNumInfo(pageNum, pageTot);
      }
      pageNum++;
    }

    // Create current Date/Time String
    nsresult rv;
    nsCOMPtr<nsILocale> locale; 
    nsCOMPtr<nsILocaleService> localeSvc = do_GetService(kLocaleServiceCID, &rv);
    if (NS_SUCCEEDED(rv)) {
      rv = localeSvc->GetApplicationLocale(getter_AddRefs(locale));
      if (NS_SUCCEEDED(rv) && locale) {
        nsCOMPtr<nsIDateTimeFormat> dateTime;
        rv = nsComponentManager::CreateInstance(kDateTimeFormatCID,
                                               NULL,
                                               NS_GET_IID(nsIDateTimeFormat),
                                               (void**) getter_AddRefs(dateTime));
        if (NS_SUCCEEDED(rv)) {
          nsAutoString dateString;
          time_t ltime;
          time( &ltime );
          if (NS_SUCCEEDED(dateTime->FormatTime(locale, kDateFormatShort, kTimeFormatNoSeconds, ltime, dateString))) {
            PRUnichar * uStr = ToNewUnicode(dateString);
            nsPageFrame::SetDateTimeStr(uStr); // nsPageFrame will own memory
          }
        }
      }
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
  NS_ASSERTION(0, "No longer being used.");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimplePageSequenceFrame::SetOffsets(nscoord aStartOffset, nscoord aEndOffset)
{
  mStartOffset = aStartOffset;
  mEndOffset   = aEndOffset;
  return NS_OK;
}
//====================================================================
//== Asynch Printing
//====================================================================
NS_IMETHODIMP
nsSimplePageSequenceFrame::GetCurrentPageNum(PRInt32* aPageNum)
{
  NS_ENSURE_ARG_POINTER(aPageNum);

  *aPageNum = mPageNum;
  return NS_OK;
}

NS_IMETHODIMP
nsSimplePageSequenceFrame::GetNumPages(PRInt32* aNumPages)
{
  NS_ENSURE_ARG_POINTER(aNumPages);

  *aNumPages = mTotalPages;
  return NS_OK;
}

NS_IMETHODIMP
nsSimplePageSequenceFrame::IsDoingPrintRange(PRBool* aDoing)
{
  NS_ENSURE_ARG_POINTER(aDoing);

  *aDoing = mDoingPageRange;
  return NS_OK;
}

NS_IMETHODIMP
nsSimplePageSequenceFrame::GetPrintRange(PRInt32* aFromPage, PRInt32* aToPage)
{
  NS_ENSURE_ARG_POINTER(aFromPage);
  NS_ENSURE_ARG_POINTER(aToPage);

  *aFromPage = mFromPageNum;
  *aToPage   = mToPageNum;
  return NS_OK;
}

// Helper Function
void 
nsSimplePageSequenceFrame::SetPageNumberFormat(const char* aPropName, const char* aDefPropVal, PRBool aPageNumOnly)
{
  // Doing this here so we only have to go get these formats once
  nsAutoString pageNumberFormat;
  // Now go get the Localized Page Formating String
  nsAutoString propName;
  propName.AssignWithConversion(aPropName);
  PRUnichar* uPropName = ToNewUnicode(propName);
  if (uPropName != nsnull) {
    nsresult rv = nsFormControlHelper::GetLocalizedString(PRINTING_PROPERTIES, uPropName, pageNumberFormat);
    if (NS_FAILED(rv)) { // back stop formatting
      pageNumberFormat.AssignWithConversion(aDefPropVal);
    }
    nsMemory::Free(uPropName);
  }
  // Sets the format into a static data memeber which will own the memory and free it
  PRUnichar* uStr = ToNewUnicode(pageNumberFormat);
  if (uStr != nsnull) {
    nsPageFrame::SetPageNumberFormat(uStr, aPageNumOnly); // nsPageFrame will own the memory
  }

}

NS_IMETHODIMP
nsSimplePageSequenceFrame::StartPrint(nsIPresContext*  aPresContext,
                                      nsIPrintOptions* aPrintOptions)
{
  NS_ENSURE_ARG_POINTER(aPresContext);
  NS_ENSURE_ARG_POINTER(aPrintOptions);

  PRInt16 printType;
  aPrintOptions->GetPrintRange(&printType);
  mPrintRangeType = printType;
  aPrintOptions->GetStartPageRange(&mFromPageNum);
  aPrintOptions->GetEndPageRange(&mToPageNum);
  aPrintOptions->GetMarginInTwips(mMargin);

  mDoingPageRange = nsIPrintOptions::kRangeSpecifiedPageRange == mPrintRangeType ||
                    nsIPrintOptions::kRangeSelection == mPrintRangeType;

  // If printing a range of pages make sure at least the starting page
  // number is valid
  PRInt32 totalPages = mFrames.GetLength();

  if (mDoingPageRange) {
    if (mFromPageNum > totalPages) {
      return NS_ERROR_INVALID_ARG;
    }
  }

  // Begin printing of the document
  nsCOMPtr<nsIDeviceContext> dc;
  aPresContext->GetDeviceContext(getter_AddRefs(dc));
  NS_ASSERTION(dc, "nsIDeviceContext can't be NULL!");

  nsCOMPtr<nsIPresShell> presShell;
  aPresContext->GetShell(getter_AddRefs(presShell));
  NS_ASSERTION(presShell, "nsIPresShell can't be NULL!");

  nsresult rv = NS_OK;

#if defined(DEBUG_rods) || defined(DEBUG_dcone)
  {
    nsIView * seqView;
    GetView(aPresContext, &seqView);
    nsRect rect;
    GetRect(rect);
    fprintf(mDebugFD, "Seq Frame: %p - [%5d,%5d,%5d,%5d] ", this, rect.x, rect.y, rect.width, rect.height);
    fprintf(mDebugFD, "view: %p ", seqView);
    nsRect viewRect;
    if (seqView) {
      seqView->GetBounds(viewRect);
      fprintf(mDebugFD, " [%5d,%5d,%5d,%5d]", viewRect.x, viewRect.y, viewRect.width, viewRect.height);
    }
    fprintf(mDebugFD, "\n");
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
      fprintf(mDebugFD, " Page: %p  No: %d - [%5d,%5d,%5d,%5d] ", page, pageNum, rect.x, rect.y, rect.width, rect.height);
      fprintf(mDebugFD, " [%5d,%5d,%5d,%5d]\n", viewRect.x, viewRect.y, viewRect.width, viewRect.height);
      pageNum++;
    }
  }
  //printf("***** Setting aPresContext %p is painting selection %d\n", aPresContext, nsIPrintOptions::kRangeSelection == mPrintRangeType);
#endif

  // Determine if we are rendering only the selection
  aPresContext->SetIsRenderingOnlySelection(nsIPrintOptions::kRangeSelection == mPrintRangeType);


  if (mDoingPageRange) {
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
      if (pageNum < mFromPageNum || pageNum > mToPageNum) {
        // Hide the pages that won't be printed to the Viewmanager
        // doesn't put them in the display list. Also, makde
        // sure the child views don't get asked to print
        // but my guess is that there won't be any
        view->SetVisibility(nsViewVisibility_kHide);
        view->SetChildClip(0,0,0,0);
        view->SetViewFlags(NS_VIEW_PUBLIC_FLAG_CLIPCHILDREN);
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
    if (nsIPrintOptions::kRangeSelection == mPrintRangeType) {
      totalPages = mToPageNum - mFromPageNum + 1;
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
  rv = nsFormControlHelper::GetLocalizedString(PRINTING_PROPERTIES, NS_LITERAL_STRING("fontname").get(), fontName);
  if (NS_FAILED(rv)) {
    fontName.AssignWithConversion("serif");
  }

  nsAutoString fontSizeStr;
  nscoord      pointSize = 10;;
  rv = nsFormControlHelper::GetLocalizedString(PRINTING_PROPERTIES, NS_LITERAL_STRING("fontsize").get(), fontSizeStr);
  if (NS_SUCCEEDED(rv)) {
    PRInt32 errCode;
    pointSize = fontSizeStr.ToInteger(&errCode);
    if (NS_FAILED(errCode)) {
      pointSize = 10;
    }
  }
  aPrintOptions->SetFontNamePointSize(fontName, pointSize);

  // Doing this here so we only have to go get these formats once
  SetPageNumberFormat("pagenumber",  "%1$d", PR_TRUE);
  SetPageNumberFormat("pageofpages", "%1$d of %2$d", PR_FALSE);

  mPageNum          = 1;
  mPrintedPageNum   = 1;
  mTotalPages       = totalPages;
  mCurrentPageFrame = mFrames.FirstChild();

  //rv = PrintNextPage(aPresContext, aPrintOptions);

  return rv;
}

NS_IMETHODIMP
nsSimplePageSequenceFrame::PrintNextPage(nsIPresContext*  aPresContext,
                                         nsIPrintOptions* aPrintOptions)
{
  NS_ENSURE_ARG_POINTER(aPresContext);
  NS_ENSURE_ARG_POINTER(aPrintOptions);

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

  if (mCurrentPageFrame == nsnull) {
    return NS_ERROR_FAILURE;
  }

  PRBool printEvenPages, printOddPages;
  aPrintOptions->GetPrintOptions(nsIPrintOptions::kOptPrintEvenPages, &printEvenPages);
  aPrintOptions->GetPrintOptions(nsIPrintOptions::kOptPrintOddPages, &printOddPages);

  // Begin printing of the document
  nsCOMPtr<nsIDeviceContext> dc;
  aPresContext->GetDeviceContext(getter_AddRefs(dc));
  NS_ASSERTION(dc, "nsIDeviceContext can't be NULL!");

  nsCOMPtr<nsIPresShell> presShell;
  aPresContext->GetShell(getter_AddRefs(presShell));
  NS_ASSERTION(presShell, "nsIPresShell can't be NULL!");

  nsCOMPtr<nsIViewManager> vm;
  presShell->GetViewManager(getter_AddRefs(vm));
  NS_ASSERTION(vm, "nsIViewManager can't be NULL!");

  nsresult rv = NS_OK;

  // See whether we should print this page
  mPrintThisPage = PR_TRUE;

  // If printing a range of pages check whether the page number is in the
  // range of pages to print
  if (mDoingPageRange) {
    if (mPageNum < mFromPageNum) {
      mPrintThisPage = PR_FALSE;
    } else if (mPageNum > mToPageNum) {
      mPageNum++;
      mCurrentPageFrame = nsnull;
      return NS_OK;
    }
  }

  // Check for printing of odd and even pages
  if (mPageNum & 0x1) {
    if (!printOddPages) {
      mPrintThisPage = PR_FALSE;  // don't print odd numbered page
    }
  } else {
    if (!printEvenPages) {
      mPrintThisPage = PR_FALSE;  // don't print even numbered page
    }
  }

  if (mPrintThisPage) {
    if (!mSkipPageBegin) {
      PRINT_DEBUG_MSG1("\n***************** BeginPage *****************\n");
      rv = dc->BeginPage();
      if (NS_FAILED(rv)) {
        return rv;
      }
    }

    // cast the frame to be a page frame
    nsPageFrame * pf = NS_STATIC_CAST(nsPageFrame*, mCurrentPageFrame);
    if (pf != nsnull) {
      pf->SetPrintOptions(aPrintOptions);
      pf->SetPageNumInfo(mPrintedPageNum, mTotalPages);
    }

    // Print the page
    nsIView*  view;
    mCurrentPageFrame->GetView(aPresContext, &view);

    NS_ASSERTION(nsnull != view, "no page view");

    PRINT_DEBUG_MSG4("SeqFr::Paint -> %p PageNo: %d  View: %p", pf, mPageNum, view);
    PRINT_DEBUG_MSG3(" At: %d,%d\n", mMargin.left+mOffsetX, mMargin.top+mOffsetY);

    view->SetContentTransparency(PR_FALSE);
    vm->Display(view, mMargin.left+mOffsetX, mMargin.top+mOffsetY);


    // this view was printed and since display set the origin 
    // 0,0 there is a danger that this view can be printed again
    // If it is a sibling to another page/view.  Setting the visibility
    // to hide will keep this page from printing again - dwc
    //
    // XXX Doesn't seem like we need to do this anymore
    //view->SetVisibility(nsViewVisibility_kHide);

    if (!mSkipPageEnd) {
      PRINT_DEBUG_MSG1("***************** End Page (PrintNextPage) *****************\n");
      rv = dc->EndPage();
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }

  if (!mSkipPageEnd) {
    if (nsIPrintOptions::kRangeSelection != mPrintRangeType ||
        (nsIPrintOptions::kRangeSelection == mPrintRangeType && mPrintThisPage)) {
      mPrintedPageNum++;
    }

    mPageNum++;
    rv = mCurrentPageFrame->GetNextSibling(&mCurrentPageFrame);
  }

  return rv;
}

NS_IMETHODIMP
nsSimplePageSequenceFrame::DoPageEnd(nsIPresContext*  aPresContext)
{
	nsresult rv = NS_OK;
	
  if (mPrintThisPage) {
    nsCOMPtr<nsIDeviceContext> dc;
    aPresContext->GetDeviceContext(getter_AddRefs(dc));
    NS_ASSERTION(dc, "nsIDeviceContext can't be NULL!");

    if(mSkipPageEnd){
	    PRINT_DEBUG_MSG1("***************** End Page (DoPageEnd) *****************\n");
      nsresult rv = dc->EndPage();
	    if (NS_FAILED(rv)) {
	      return rv;
      }
    }
  }

  if (nsIPrintOptions::kRangeSelection != mPrintRangeType ||
      (nsIPrintOptions::kRangeSelection == mPrintRangeType && mPrintThisPage)) {
    mPrintedPageNum++;
  }

  mPageNum++;
  
  if( nsnull != mCurrentPageFrame){
    rv = mCurrentPageFrame->GetNextSibling(&mCurrentPageFrame);
  }
  
  return rv;
}

NS_IMETHODIMP
nsSimplePageSequenceFrame::SuppressHeadersAndFooters(PRBool aDoSup)
{
  for (nsIFrame* f = mFrames.FirstChild(); f != nsnull; f->GetNextSibling(&f)) {
    nsPageFrame * pf = NS_STATIC_CAST(nsPageFrame*, f);
    if (pf != nsnull) {
      pf->SuppressHeadersAndFooters(aDoSup);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSimplePageSequenceFrame::SetClipRect(nsIPresContext*  aPresContext, nsRect* aRect)
{
  for (nsIFrame* f = mFrames.FirstChild(); f != nsnull; f->GetNextSibling(&f)) {
    nsPageFrame * pf = NS_STATIC_CAST(nsPageFrame*, f);
    if (pf != nsnull) {
      pf->SetClipRect(aRect);
    }
  }
  return NS_OK;
}

#ifdef NS_DEBUG
NS_IMETHODIMP 
nsSimplePageSequenceFrame::SetDebugFD(FILE* aFD)
{
  mDebugFD = aFD;
  for (nsIFrame* f = mFrames.FirstChild(); f != nsnull; f->GetNextSibling(&f)) {
    nsPageFrame * pf = NS_STATIC_CAST(nsPageFrame*, f);
    if (pf != nsnull) {
      pf->SetDebugFD(aFD);
    }
  }
  return NS_OK;
}
#endif

//------------------------------------------------------------------------------
NS_IMETHODIMP
nsSimplePageSequenceFrame::Paint(nsIPresContext*      aPresContext,
                                 nsIRenderingContext& aRenderingContext,
                                 const nsRect&        aDirtyRect,
                                 nsFramePaintLayer    aWhichLayer)
{
  aRenderingContext.PushState();
  aRenderingContext.SetColor(NS_RGB(255,255,255));


  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
    nsRect rect = mRect;
    aRenderingContext.SetColor(NS_RGB(255,255,255));
    rect.x = 0;
    rect.y = 0;
    aRenderingContext.FillRect(rect);
  }

  nsresult rv = nsContainerFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);

  PRBool clipEmpty;
  aRenderingContext.PopState(clipEmpty);
  return rv;
}
