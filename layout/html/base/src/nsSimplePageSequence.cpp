/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsCOMPtr.h" 
#include "nsReadableUtils.h"
#include "nsSimplePageSequence.h"
#include "nsIPresContext.h"
#include "nsIRenderingContext.h"
#include "nsHTMLAtoms.h"
#include "nsIDeviceContext.h"
#include "nsIViewManager.h"
#include "nsIPresShell.h"
#include "nsIFontMetrics.h"
#include "nsIPrintSettings.h"
#include "nsPageFrame.h"
#include "nsStyleConsts.h"
#include "nsRegion.h"
#include "nsLayoutAtoms.h"
#include "nsCSSFrameConstructor.h"
#include "nsContentUtils.h"

// DateTime Includes
#include "nsDateTimeFormatCID.h"
static NS_DEFINE_CID(kDateTimeFormatCID, NS_DATETIMEFORMAT_CID);

#define OFFSET_NOT_SET -1

// This is for localization of the "x of n" pages string
// this class contains a helper method we need to get 
// a string from a string bundle
#include "nsFormControlHelper.h"
#define PRINTING_PROPERTIES "chrome://global/locale/printing.properties"

// Print Options
#include "nsIPrintSettings.h"
#include "nsIPrintOptions.h"
#include "nsGfxCIID.h"
#include "nsIServiceManager.h"

static const char sPrintOptionsContractID[] = "@mozilla.org/gfx/printsettings-service;1";

//

#include "prlog.h"
#ifdef PR_LOGGING 
PRLogModuleInfo * kLayoutPrintingLogMod = PR_NewLogModule("printing-layout");
#define PR_PL(_p1)  PR_LOG(kLayoutPrintingLogMod, PR_LOG_DEBUG, _p1)
#else
#define PR_PL(_p1)
#endif

// This object a shared by all the nsPageFrames 
// parented to a SimplePageSequenceFrame
nsSharedPageData::nsSharedPageData() :
  mDateTimeStr(nsnull),
  mHeadFootFont(nsnull),
  mPageNumFormat(nsnull),
  mPageNumAndTotalsFormat(nsnull),
  mDocTitle(nsnull),
  mDocURL(nsnull),
  mReflowRect(0,0,0,0),
  mReflowMargin(0,0,0,0),
  mShadowSize(0,0),
  mDeadSpaceMargin(0,0,0,0),
  mExtraMargin(0,0,0,0),
  mEdgePaperMargin(0,0,0,0),
  mPageContentXMost(0),
  mPageContentSize(0)
{
}

nsSharedPageData::~nsSharedPageData()
{
  nsMemory::Free(mDateTimeStr);
  if (mHeadFootFont) delete mHeadFootFont;
  nsMemory::Free(mPageNumFormat);
  nsMemory::Free(mPageNumAndTotalsFormat);
  if (mDocTitle) nsMemory::Free(mDocTitle);
  if (mDocURL) nsMemory::Free(mDocURL);
}

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
  mIsPrintingSelection(PR_FALSE),
  mTotalPages(-1),
  mSelectionHeight(-1),
  mYSelOffset(0)
{
  mStartOffset = OFFSET_NOT_SET;
  mEndOffset   = OFFSET_NOT_SET;

  nscoord halfInch = NS_INCHES_TO_TWIPS(0.5);
  mMargin.SizeTo(halfInch, halfInch, halfInch, halfInch);

  mPageData = new nsSharedPageData();
  NS_ASSERTION(mPageData != nsnull, "Can't be null!");
  if (mPageData->mHeadFootFont == nsnull) {
    mPageData->mHeadFootFont = new nsFont("serif", NS_FONT_STYLE_NORMAL,NS_FONT_VARIANT_NORMAL,
                               NS_FONT_WEIGHT_NORMAL,0,NSIntPointsToTwips(10));
  }

  // XXX this code and the object data member "mIsPrintingSelection" is only needed
  // for the hack for printing selection where we make the page the max size
  nsresult rv;
  mPageData->mPrintOptions = do_GetService(sPrintOptionsContractID, &rv);
  if (NS_SUCCEEDED(rv) && mPageData->mPrintOptions) {
    // now get the default font form the print options
    mPageData->mPrintOptions->GetDefaultFont(*mPageData->mHeadFootFont);
  }
  mSkipPageBegin = PR_FALSE;
  mSkipPageEnd   = PR_FALSE;
  mPrintThisPage = PR_FALSE;
  mOffsetX       = 0;
  mOffsetY       = 0;

  // Doing this here so we only have to go get these formats once
  SetPageNumberFormat("pagenumber",  "%1$d", PR_TRUE);
  SetPageNumberFormat("pageofpages", "%1$d of %2$d", PR_FALSE);
}

nsSimplePageSequenceFrame::~nsSimplePageSequenceFrame()
{
  if (mPageData) delete mPageData;
}

nsresult
nsSimplePageSequenceFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(0 != aInstancePtr, "null ptr");
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(NS_GET_IID(nsIPageSequenceFrame))) {
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
  // Create the continuing frame
  return aPresContext->PresShell()->FrameConstructor()->
    CreateContinuingFrame(aPresContext, aPageFrame, this, aContinuingPage);
}

void
nsSimplePageSequenceFrame::GetEdgePaperMarginCoord(char* aPrefName,
                                                   nscoord& aCoord)
{
  nsresult rv = mPageData->mPrintOptions->
    GetPrinterPrefInt(mPageData->mPrintSettings, 
                      NS_ConvertASCIItoUCS2(aPrefName).get(),
                      &aCoord);

  if (NS_SUCCEEDED(rv)) {
    nscoord inchInTwips = NS_INCHES_TO_TWIPS(1.0);
    aCoord = PR_MAX(NS_INCHES_TO_TWIPS(float(aCoord)/100.0f), 0);
    aCoord = PR_MIN(aCoord, inchInTwips); // an inch is still probably excessive
  }
}

void
nsSimplePageSequenceFrame::GetEdgePaperMargin(nsMargin& aMargin)
{
  aMargin.SizeTo(0,0,0,0);
  GetEdgePaperMarginCoord("print_edge_top",    aMargin.top);
  GetEdgePaperMarginCoord("print_edge_left",   aMargin.left);
  GetEdgePaperMarginCoord("print_edge_bottom", aMargin.bottom);
  GetEdgePaperMarginCoord("print_edge_right",  aMargin.right);
}

NS_IMETHODIMP
nsSimplePageSequenceFrame::Reflow(nsIPresContext*          aPresContext,
                                  nsHTMLReflowMetrics&     aDesiredSize,
                                  const nsHTMLReflowState& aReflowState,
                                  nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsSimplePageSequenceFrame", aReflowState.reason);
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
  NS_FRAME_TRACE_REFLOW_IN("nsSimplePageSequenceFrame::Reflow");

  aStatus = NS_FRAME_COMPLETE;  // we're always complete

  // absolutely ignore all other types of reflows
  // we only want to have done the Initial Reflow
  if (eReflowReason_Resize == aReflowState.reason ||
      eReflowReason_Incremental == aReflowState.reason ||
      eReflowReason_StyleChange == aReflowState.reason ||
      eReflowReason_Dirty == aReflowState.reason) {
    // Return our desired size
    aDesiredSize.height  = mSize.height;
    aDesiredSize.width   = mSize.width;
    aDesiredSize.ascent  = aDesiredSize.height;
    aDesiredSize.descent = 0;
    return NS_OK;
  }

  // Turn on the scaling of twips so any of the scrollbars
  // in the UI no longer get scaled
  PRBool isPrintPreview =
    aPresContext->Type() == nsIPresContext::eContext_PrintPreview;
  if (isPrintPreview) {
    aPresContext->SetScalingOfTwips(PR_TRUE);
  }

  // See if we can get a Print Settings from the Context
  if (!mPageData->mPrintSettings &&
      aPresContext->Medium() == nsLayoutAtoms::print) {
      mPageData->mPrintSettings = aPresContext->GetPrintSettings();
  }

  // now get out margins
  if (mPageData->mPrintSettings) {
    mPageData->mPrintSettings->GetMarginInTwips(mMargin);
    PRInt16 printType;
    mPageData->mPrintSettings->GetPrintRange(&printType);
    mPrintRangeType = printType;
    mIsPrintingSelection = nsIPrintSettings::kRangeSelection == printType;
  }

  // *** Special Override ***
  // If this is a sub-sdoc (meaning it doesn't take the whole page)
  // and if this Document is in the upper left hand corner
  // we need to suppress the top margin or it will reflow too small
  // Start by getting the actual printer page dimensions to see if we are not a whole page
  nscoord width, height;
  aPresContext->DeviceContext()->GetDeviceSurfaceDimensions(width, height);

  // Compute the size of each page and the x coordinate that each page will
  // be placed at
  nsRect  pageSize;
  nsRect  adjSize;
  aPresContext->GetPageDim(&pageSize, &adjSize);

  GetEdgePaperMargin(mPageData->mEdgePaperMargin);
  nscoord extraThreshold = PR_MAX(pageSize.width, pageSize.height)/10;
  PRInt32 gapInTwips = nsContentUtils::GetIntPref("print.print_extra_margin");

  gapInTwips = PR_MAX(gapInTwips, 0);
  gapInTwips = PR_MIN(gapInTwips, extraThreshold); // clamp to 1/10 of the largest dim of the page

  nscoord extraGap = nscoord(gapInTwips);

  nscoord  deadSpaceGap;
  GetDeadSpaceValue(&deadSpaceGap);

  nsMargin deadSpaceMargin(0,0,0,0);
  nsMargin extraMargin(0,0,0,0);
  nsSize   shadowSize(0,0);
  if (isPrintPreview) {
    if (adjSize.width == width && adjSize.height == height) {
      deadSpaceMargin.SizeTo(deadSpaceGap, deadSpaceGap, deadSpaceGap, deadSpaceGap);
      extraMargin.SizeTo(extraGap, extraGap, extraGap, extraGap);
      nscoord fourPixels = aPresContext->IntScaledPixelsToTwips(4);
      shadowSize.SizeTo(fourPixels, fourPixels);
    }
  }

  mPageData->mShadowSize      = shadowSize;
  mPageData->mExtraMargin     = extraMargin;
  mPageData->mDeadSpaceMargin = deadSpaceMargin;

  PRBool suppressLeftMargin   = PR_FALSE;
  PRBool suppressRightMargin  = PR_FALSE;
  PRBool suppressTopMargin    = PR_FALSE;
  PRBool suppressBottomMargin = PR_FALSE;

  if (pageSize != adjSize &&
      (adjSize.x != 0 || adjSize.y != 0 || adjSize.width != 0 || adjSize.height != 0)) {
    suppressLeftMargin   = pageSize.x != adjSize.x || (pageSize.x == adjSize.x && !adjSize.x);
    suppressTopMargin    = pageSize.y != adjSize.y || (pageSize.y == adjSize.y && !adjSize.y);
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
    suppressLeftMargin = PR_FALSE;
    suppressTopMargin = PR_FALSE;
    suppressRightMargin = PR_FALSE;
    suppressBottomMargin = PR_FALSE;
  }


  // only use this local margin for sizing, 
  // not for positioning
  nsMargin margin(suppressLeftMargin?0:mMargin.left,
                  suppressTopMargin?0:mMargin.top,
                  suppressRightMargin?0:mMargin.right,
                  suppressBottomMargin?0:mMargin.bottom);

  nscoord x = deadSpaceMargin.left;
  nscoord y = deadSpaceMargin.top;// Running y-offset for each page

  nsSize reflowPageSize(0,0);

  // See if it's an incremental reflow command
  if (eReflowReason_Incremental == aReflowState.reason) {
    // XXX Skip Incremental reflow, 
    // in fact, all we want is the initial reflow
    y = mRect.height;
  } else {
    // XXX Part of Temporary fix for Bug 127263
    nsPageFrame::SetCreateWidget(PR_TRUE);

    nsReflowReason  reflowReason = aReflowState.reason;

    SetPageSizes(pageSize, margin);

    // Tile the pages vertically
    nsHTMLReflowMetrics kidSize(nsnull);
    for (nsIFrame* kidFrame = mFrames.FirstChild(); nsnull != kidFrame; ) {
      // Reflow the page
      // The availableHeight always comes in NS_UNCONSTRAINEDSIZE, so we need to check
      // the "adjusted rect" to see if that is being reflowed NS_UNCONSTRAINEDSIZE or not
      // When it is NS_UNCONSTRAINEDSIZE it means we are reflowing a single page
      // to print selection. So this means we want to use NS_UNCONSTRAINEDSIZE without altering it
      nsRect actualRect;
      nsRect adjRect;
      aPresContext->GetPageDim(&actualRect, &adjRect);
      nscoord avHeight;
      if (adjRect.height == NS_UNCONSTRAINEDSIZE) {
        avHeight = NS_UNCONSTRAINEDSIZE;
      } else {
        avHeight = pageSize.height+deadSpaceMargin.top+deadSpaceMargin.bottom+shadowSize.height+extraMargin.top+extraMargin.bottom;
      }
      nsSize availSize(pageSize.width+deadSpaceMargin.right+deadSpaceMargin.left+shadowSize.width+extraMargin.right+extraMargin.left, 
                       avHeight);
      nsHTMLReflowState kidReflowState(aPresContext, aReflowState, kidFrame,
                                       availSize, reflowReason);
      nsReflowStatus  status;

      kidReflowState.mComputedWidth  = kidReflowState.availableWidth;
      //kidReflowState.mComputedHeight = kidReflowState.availableHeight;
      PR_PL(("AV W: %d   H: %d\n", kidReflowState.availableWidth, kidReflowState.availableHeight));

      // Set the shared data into the page frame before reflow
      nsPageFrame * pf = NS_STATIC_CAST(nsPageFrame*, kidFrame);
      pf->SetSharedPageData(mPageData);

      // Place and size the page. If the page is narrower than our
      // max width then center it horizontally
      ReflowChild(kidFrame, aPresContext, kidSize, kidReflowState, x, y, 0, status);

      reflowPageSize.SizeTo(kidSize.width, kidSize.height);

      FinishReflowChild(kidFrame, aPresContext, nsnull, kidSize, x, y, 0);
      y += kidSize.height;

      // XXX Temporary fix for Bug 127263
      // This tells the nsPageFrame class to stop creating clipping widgets
      // once we reach the 32k boundary for positioning
      if (nsPageFrame::GetCreateWidget()) {
        float t2p;
        t2p = aPresContext->TwipsToPixels();
        nscoord xp = NSTwipsToIntPixels(x, t2p);
        nscoord yp = NSTwipsToIntPixels(y, t2p);
        nsPageFrame::SetCreateWidget(xp < 0x8000 && yp < 0x8000);
      }

      // Leave a slight gap between the pages
      y += deadSpaceGap;

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
      kidFrame = kidFrame->GetNextSibling();
    }

    // Get Total Page Count
    nsIFrame* page;
    PRInt32 pageTot = 0;
    for (page = mFrames.FirstChild(); page; page = page->GetNextSibling()) {
      pageTot++;
    }

    // Set Page Number Info
    PRInt32 pageNum = 1;
    for (page = mFrames.FirstChild(); page; page = page->GetNextSibling()) {
      nsPageFrame * pf = NS_STATIC_CAST(nsPageFrame*, page);
      if (pf != nsnull) {
        pf->SetPageNumInfo(pageNum, pageTot);
      }
      pageNum++;
    }

    // Create current Date/Time String
    if (!mDateFormatter)
      mDateFormatter = do_CreateInstance(kDateTimeFormatCID);

    NS_ENSURE_TRUE(mDateFormatter, NS_ERROR_FAILURE);

    nsAutoString formattedDateString;
    time_t ltime;
    time( &ltime );
    if (NS_SUCCEEDED(mDateFormatter->FormatTime(nsnull /* nsILocale* locale */,
                                                kDateFormatShort,
                                                kTimeFormatNoSeconds,
                                                ltime,
                                                formattedDateString))) {
      PRUnichar * uStr = ToNewUnicode(formattedDateString);
      SetDateTimeStr(uStr); // memory will be freed
    }

  }

  // Return our desired size
  aDesiredSize.height  = y;
  aDesiredSize.width   = reflowPageSize.width+deadSpaceMargin.right+shadowSize.width+extraMargin.right+extraMargin.left;
  aDesiredSize.ascent  = aDesiredSize.height;
  aDesiredSize.descent = 0;

  // cache the size so we can set the desired size 
  // for the other reflows that happen
  mSize.width  = aDesiredSize.width;
  mSize.height = aDesiredSize.height;

  // Turn off the scaling of twips so any of the scrollbars
  // in the document get scaled
  if (isPrintPreview) {
    aPresContext->SetScalingOfTwips(PR_FALSE);
  }

  NS_FRAME_TRACE_REFLOW_OUT("nsSimplePageSequeceFrame::Reflow", aStatus);
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return NS_OK;
}

//----------------------------------------------------------------------

#ifdef DEBUG
NS_IMETHODIMP
nsSimplePageSequenceFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("SimplePageSequence"), aResult);
}
#endif

//----------------------------------------------------------------------

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
  nsresult rv = nsFormControlHelper::GetLocalizedString(PRINTING_PROPERTIES, NS_ConvertUTF8toUCS2(aPropName).get(), pageNumberFormat);
  if (NS_FAILED(rv)) { // back stop formatting
    pageNumberFormat.AssignASCII(aDefPropVal);
  }

  // Sets the format into a static data memeber which will own the memory and free it
  PRUnichar* uStr = ToNewUnicode(pageNumberFormat);
  if (uStr != nsnull) {
    SetPageNumberFormat(uStr, aPageNumOnly); // nsPageFrame will own the memory
  }

}

NS_IMETHODIMP
nsSimplePageSequenceFrame::StartPrint(nsIPresContext*   aPresContext,
                                      nsIPrintSettings* aPrintSettings,
                                      PRUnichar*        aDocTitle,
                                      PRUnichar*        aDocURL)
{
  NS_ENSURE_ARG_POINTER(aPresContext);
  NS_ENSURE_ARG_POINTER(aPrintSettings);

  if (!mPageData->mPrintSettings) {
    mPageData->mPrintSettings = aPrintSettings;
  }

  // Only set them if they are not null
  if (aDocTitle) mPageData->mDocTitle = aDocTitle;
  if (aDocURL) mPageData->mDocURL   = aDocURL;

  aPrintSettings->GetMarginInTwips(mMargin);

  aPrintSettings->GetStartPageRange(&mFromPageNum);
  aPrintSettings->GetEndPageRange(&mToPageNum);
  aPrintSettings->GetMarginInTwips(mMargin);

  mDoingPageRange = nsIPrintSettings::kRangeSpecifiedPageRange == mPrintRangeType ||
                    nsIPrintSettings::kRangeSelection == mPrintRangeType;

  // If printing a range of pages make sure at least the starting page
  // number is valid
  PRInt32 totalPages = mFrames.GetLength();

  if (mDoingPageRange) {
    if (mFromPageNum > totalPages) {
      return NS_ERROR_INVALID_ARG;
    }
  }

  // Begin printing of the document
  nsresult rv = NS_OK;

#if defined(DEBUG_rods) || defined(DEBUG_dcone)
  {
    nsIView* seqView = GetView();
    nsRect rect = GetRect();
    PR_PL(("Seq Frame: %p - [%5d,%5d,%5d,%5d] ", this, rect.x, rect.y, rect.width, rect.height));
    PR_PL(("view: %p ", seqView));
    if (seqView) {
      nsRect viewRect = seqView->GetBounds();
      PR_PL((" [%5d,%5d,%5d,%5d]", viewRect.x, viewRect.y, viewRect.width, viewRect.height));
    }
    PR_PL(("\n"));
  }

  {
    PRInt32 pageNum = 1;
    for (nsIFrame* page = mFrames.FirstChild(); page;
         page = page->GetNextSibling()) {
      nsIView* view = page->GetView();
      NS_ASSERTION(view, "no page view");
      nsRect rect = page->GetRect();
      nsRect viewRect = view->GetBounds();
      PR_PL((" Page: %p  No: %d - [%5d,%5d,%5d,%5d] ", page, pageNum, rect.x, rect.y, rect.width, rect.height));
      PR_PL((" [%5d,%5d,%5d,%5d]\n", viewRect.x, viewRect.y, viewRect.width, viewRect.height));
      pageNum++;
    }
  }
  //printf("***** Setting aPresContext %p is painting selection %d\n", aPresContext, nsIPrintSettings::kRangeSelection == mPrintRangeType);
#endif

  // Determine if we are rendering only the selection
  aPresContext->SetIsRenderingOnlySelection(nsIPrintSettings::kRangeSelection == mPrintRangeType);


  if (mDoingPageRange) {
    // XXX because of the hack for making the selection all print on one page
    // we must make sure that the page is sized correctly before printing.
    PRInt32 width, height;
    aPresContext->DeviceContext()->GetDeviceSurfaceDimensions(width, height);

    PRInt32 pageNum = 1;
    nscoord y = 0;//mMargin.top;

    for (nsIFrame* page = mFrames.FirstChild(); page;
         page = page->GetNextSibling()) {
      nsIView* view = page->GetView();
      NS_ASSERTION(view, "no page view");

      nsIViewManager* vm = view->GetViewManager();
      NS_ASSERTION(vm, "no view manager");

      if (pageNum < mFromPageNum || pageNum > mToPageNum) {
        // Hide the pages that won't be printed to the Viewmanager
        // doesn't put them in the display list. Also, makde
        // sure the child views don't get asked to print
        // but my guess is that there won't be any
        vm->SetViewVisibility(view, nsViewVisibility_kHide);
        nsRegion emptyRegion;
        vm->SetViewChildClipRegion(view, &emptyRegion);
      } else {
        nsRect rect = page->GetRect();
        rect.y = y;
        rect.height = height;
        page->SetRect(rect);

        nsRect viewRect = view->GetBounds();
        viewRect.y = y;
        viewRect.height = height;
        vm->MoveViewTo(view, viewRect.x, viewRect.y);
        viewRect.x = 0;
        viewRect.y = 0;
        vm->ResizeView(view, viewRect);
        y += rect.height + mMargin.top + mMargin.bottom;
      }
      pageNum++;
    }

    // adjust total number of pages
    if (nsIPrintSettings::kRangeSelection != mPrintRangeType) {
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
    fontName.AssignLiteral("serif");
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
  mPageData->mPrintOptions->SetFontNamePointSize(fontName, pointSize);

  // Doing this here so we only have to go get these formats once
  SetPageNumberFormat("pagenumber",  "%1$d", PR_TRUE);
  SetPageNumberFormat("pageofpages", "%1$d of %2$d", PR_FALSE);

  mPageNum          = 1;
  mPrintedPageNum   = 1;
  mCurrentPageFrame = mFrames.FirstChild();

  if (mTotalPages == -1) {
    mTotalPages = totalPages;
  }

  return rv;
}

NS_IMETHODIMP
nsSimplePageSequenceFrame::PrintNextPage(nsIPresContext*  aPresContext)
{
  NS_ENSURE_ARG_POINTER(aPresContext);

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
  mPageData->mPrintSettings->GetPrintOptions(nsIPrintSettings::kPrintEvenPages, &printEvenPages);
  mPageData->mPrintSettings->GetPrintOptions(nsIPrintSettings::kPrintOddPages, &printOddPages);

  // Begin printing of the document
  nsIDeviceContext *dc = aPresContext->DeviceContext();
  NS_ASSERTION(dc, "nsIDeviceContext can't be NULL!");

  nsIViewManager* vm = aPresContext->GetViewManager();
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
    // XXX This is temporary fix for printing more than one page of a selection
    // This does a poor man's "dump" pagination (see Bug 89353)
    // It has laid out as one long page and now we are just moving or view up/down 
    // one page at a time and printing the contents of what is exposed by the rect.
    // currently this does not work for IFrames
    // I will soon improve this to work with IFrames 
    PRBool  continuePrinting = PR_TRUE;
    PRInt32 width, height;
    dc->GetDeviceSurfaceDimensions(width, height);
    nsRect clipRect(0, 0, width, height);
    nsRect slidingRect(-1, -1, -1, -1);
    height -= mMargin.top + mMargin.bottom;
    width  -= mMargin.left + mMargin.right;
    nscoord selectionY = height;
    nsIView* containerView = nsnull;
    nsRect   containerRect;
    if (mSelectionHeight > -1) {
      nsIFrame* childFrame = mFrames.FirstChild();
      nsIFrame* conFrame = childFrame->GetFirstChild(nsnull);
      containerView = conFrame->GetView();
      NS_ASSERTION(containerView, "Container view can't be null!");
      containerRect = containerView->GetBounds();
      containerRect.y -= mYSelOffset;
      slidingRect.SetRect(0,mYSelOffset,width,height);
      
      vm->MoveViewTo(containerView, containerRect.x, containerRect.y);
      nsRect r(0, 0, containerRect.width, containerRect.height);
      vm->ResizeView(containerView, r, PR_FALSE);
      clipRect.SetRect(mMargin.left, mMargin.right, width, height);
    }

    while (continuePrinting) {
      if (!mSkipPageBegin) {
        PR_PL(("\n"));
        PR_PL(("***************** BeginPage *****************\n"));
        rv = dc->BeginPage();
        if (NS_FAILED(rv)) {
          return rv;
        }
      }

      // cast the frame to be a page frame
      nsPageFrame * pf = NS_STATIC_CAST(nsPageFrame*, mCurrentPageFrame);
      if (pf != nsnull) {
        pf->SetPageNumInfo(mPrintedPageNum, mTotalPages);
        pf->SetSharedPageData(mPageData);
        if (mSelectionHeight > -1) {
          pf->SetClipRect(&slidingRect);
        }
      }

      // Print the page
      nsIView* view = mCurrentPageFrame->GetView();

      NS_ASSERTION(view, "no page view");

      PR_PL(("SeqFr::Paint -> %p PageNo: %d  View: %p", pf, mPageNum, view));
      PR_PL((" At: %d,%d\n", mMargin.left+mOffsetX, mMargin.top+mOffsetY));

      vm->SetViewContentTransparency(view, PR_FALSE);

      vm->Display(view, mOffsetX, mOffsetY, clipRect);

      // this view was printed and since display set the origin 
      // 0,0 there is a danger that this view can be printed again
      // If it is a sibling to another page/view.  Setting the visibility
      // to hide will keep this page from printing again - dwc
      //
      // XXX Doesn't seem like we need to do this anymore
      //view->SetVisibility(nsViewVisibility_kHide);

      if (!mSkipPageEnd) {
        PR_PL(("***************** End Page (PrintNextPage) *****************\n"));
        rv = dc->EndPage();
        if (NS_FAILED(rv)) {
          return rv;
        }
      }

      if (mSelectionHeight > -1 && selectionY < mSelectionHeight) {
        selectionY += height;

        mPrintedPageNum++;
        pf->SetPageNumInfo(mPrintedPageNum, mTotalPages);
        containerRect.y -= height;
        containerRect.height += height;
        vm->MoveViewTo(containerView, containerRect.x, containerRect.y);
        nsRect r(0, 0, containerRect.width, containerRect.height);
        vm->ResizeView(containerView, r, PR_FALSE);
        if (pf != nsnull) {
          slidingRect.y += height;
        }

      } else {
        continuePrinting = PR_FALSE;
      }
    }
  }

  if (!mSkipPageEnd) {
    if (nsIPrintSettings::kRangeSelection != mPrintRangeType ||
        (nsIPrintSettings::kRangeSelection == mPrintRangeType && mPrintThisPage)) {
      mPrintedPageNum++;
    }

    mPageNum++;
    mCurrentPageFrame = mCurrentPageFrame->GetNextSibling();
  }

  return rv;
}

NS_IMETHODIMP
nsSimplePageSequenceFrame::DoPageEnd(nsIPresContext*  aPresContext)
{
	nsresult rv = NS_OK;
	
  if (mPrintThisPage) {
    if(mSkipPageEnd){
	    PR_PL(("***************** End Page (DoPageEnd) *****************\n"));
      nsresult rv = aPresContext->DeviceContext()->EndPage();
	    if (NS_FAILED(rv)) {
	      return rv;
      }
    }
  }

  if (nsIPrintSettings::kRangeSelection != mPrintRangeType ||
      (nsIPrintSettings::kRangeSelection == mPrintRangeType && mPrintThisPage)) {
    mPrintedPageNum++;
  }

  mPageNum++;
  
  if (mCurrentPageFrame) {
    mCurrentPageFrame = mCurrentPageFrame->GetNextSibling();
  }
  
  return rv;
}

NS_IMETHODIMP
nsSimplePageSequenceFrame::SuppressHeadersAndFooters(PRBool aDoSup)
{
  for (nsIFrame* f = mFrames.FirstChild(); f; f = f->GetNextSibling()) {
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
  for (nsIFrame* f = mFrames.FirstChild(); f; f = f->GetNextSibling()) {
    nsPageFrame * pf = NS_STATIC_CAST(nsPageFrame*, f);
    if (pf != nsnull) {
      pf->SetClipRect(aRect);
    }
  }
  return NS_OK;
}

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

  aRenderingContext.PopState();
  return rv;
}

nsIAtom*
nsSimplePageSequenceFrame::GetType() const
{
  return nsLayoutAtoms::sequenceFrame; 
}

//------------------------------------------------------------------------------
void
nsSimplePageSequenceFrame::SetPageNumberFormat(PRUnichar * aFormatStr, PRBool aForPageNumOnly)
{ 
  NS_ASSERTION(aFormatStr != nsnull, "Format string cannot be null!");
  NS_ASSERTION(mPageData != nsnull, "mPageData string cannot be null!");

  if (aForPageNumOnly) {
    if (mPageData->mPageNumFormat != nsnull) {
      nsMemory::Free(mPageData->mPageNumFormat);
    }
    mPageData->mPageNumFormat = aFormatStr;
  } else {
    if (mPageData->mPageNumAndTotalsFormat != nsnull) {
      nsMemory::Free(mPageData->mPageNumAndTotalsFormat);
    }
    mPageData->mPageNumAndTotalsFormat = aFormatStr;
  }
}

//------------------------------------------------------------------------------
void
nsSimplePageSequenceFrame::SetDateTimeStr(PRUnichar * aDateTimeStr)
{ 
  NS_ASSERTION(aDateTimeStr != nsnull, "DateTime string cannot be null!");
  NS_ASSERTION(mPageData != nsnull, "mPageData string cannot be null!");

  if (mPageData->mDateTimeStr != nsnull) {
    nsMemory::Free(mPageData->mDateTimeStr);
  }
  mPageData->mDateTimeStr = aDateTimeStr;
}

//------------------------------------------------------------------------------
void
nsSimplePageSequenceFrame::SetPageSizes(const nsRect& aRect, const nsMargin& aMarginRect)
{ 
  NS_ASSERTION(mPageData != nsnull, "mPageData string cannot be null!");

  mPageData->mReflowRect   = aRect;
  mPageData->mReflowMargin = aMarginRect;
}

//------------------------------------------------------------------------------
// For Shrink To Fit
//
// Return the percentage that the page needs to shrink to 
//
NS_IMETHODIMP
nsSimplePageSequenceFrame::GetSTFPercent(float& aSTFPercent)
{
  NS_ENSURE_TRUE(mPageData, NS_ERROR_UNEXPECTED);
  aSTFPercent = 1.0f;
  if (mPageData && (mPageData->mPageContentXMost > mPageData->mPageContentSize)) {
    aSTFPercent = float(mPageData->mPageContentSize) / float(mPageData->mPageContentXMost);
  }
  return NS_OK;
}
