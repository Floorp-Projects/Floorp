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
#include "nsPresContext.h"
#include "gfxContext.h"
#include "nsRenderingContext.h"
#include "nsGkAtoms.h"
#include "nsIPresShell.h"
#include "nsIPrintSettings.h"
#include "nsPageFrame.h"
#include "nsStyleConsts.h"
#include "nsRegion.h"
#include "nsCSSFrameConstructor.h"
#include "nsContentUtils.h"
#include "nsDisplayList.h"
#include "mozilla/Preferences.h"

// DateTime Includes
#include "nsDateTimeFormatCID.h"

#define OFFSET_NOT_SET -1

// Print Options
#include "nsIPrintSettings.h"
#include "nsIPrintOptions.h"
#include "nsGfxCIID.h"
#include "nsIServiceManager.h"

using namespace mozilla;

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
  mReflowSize(0,0),
  mReflowMargin(0,0,0,0),
  mExtraMargin(0,0,0,0),
  mEdgePaperMargin(0,0,0,0),
  mPageContentXMost(0),
  mPageContentSize(0)
{
}

nsSharedPageData::~nsSharedPageData()
{
  nsMemory::Free(mDateTimeStr);
  delete mHeadFootFont;
  nsMemory::Free(mPageNumFormat);
  nsMemory::Free(mPageNumAndTotalsFormat);
  if (mDocTitle) nsMemory::Free(mDocTitle);
  if (mDocURL) nsMemory::Free(mDocURL);
}

nsIFrame*
NS_NewSimplePageSequenceFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsSimplePageSequenceFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsSimplePageSequenceFrame)

nsSimplePageSequenceFrame::nsSimplePageSequenceFrame(nsStyleContext* aContext) :
  nsContainerFrame(aContext),
  mTotalPages(-1),
  mSelectionHeight(-1),
  mYSelOffset(0)
{
  nscoord halfInch = PresContext()->CSSTwipsToAppUnits(NS_INCHES_TO_TWIPS(0.5));
  mMargin.SizeTo(halfInch, halfInch, halfInch, halfInch);

  // XXX Unsafe to assume successful allocation
  mPageData = new nsSharedPageData();
  mPageData->mHeadFootFont = new nsFont(*PresContext()->GetDefaultFont(kGenericFont_serif));
  mPageData->mHeadFootFont->size = nsPresContext::CSSPointsToAppUnits(10);

  nsresult rv;
  mPageData->mPrintOptions = do_GetService(sPrintOptionsContractID, &rv);

  // Doing this here so we only have to go get these formats once
  SetPageNumberFormat("pagenumber",  "%1$d", PR_TRUE);
  SetPageNumberFormat("pageofpages", "%1$d of %2$d", PR_FALSE);
}

nsSimplePageSequenceFrame::~nsSimplePageSequenceFrame()
{
  delete mPageData;
}

NS_QUERYFRAME_HEAD(nsSimplePageSequenceFrame)
  NS_QUERYFRAME_ENTRY(nsIPageSequenceFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

//----------------------------------------------------------------------

void
nsSimplePageSequenceFrame::SetDesiredSize(nsHTMLReflowMetrics& aDesiredSize,
                                          const nsHTMLReflowState& aReflowState,
                                          nscoord aWidth,
                                          nscoord aHeight)
{
    // Aim to fill the whole size of the document, not only so we
    // can act as a background in print preview but also handle overflow
    // in child page frames correctly.
    // Use availableWidth so we don't cause a needless horizontal scrollbar.
    aDesiredSize.width = NS_MAX(aReflowState.availableWidth,
                                nscoord(aWidth * PresContext()->GetPrintPreviewScale()));
    aDesiredSize.height = NS_MAX(aReflowState.ComputedHeight(),
                                 nscoord(aHeight * PresContext()->GetPrintPreviewScale()));
}

NS_IMETHODIMP
nsSimplePageSequenceFrame::Reflow(nsPresContext*          aPresContext,
                                  nsHTMLReflowMetrics&     aDesiredSize,
                                  const nsHTMLReflowState& aReflowState,
                                  nsReflowStatus&          aStatus)
{
  NS_PRECONDITION(aPresContext->IsRootPaginatedDocument(),
                  "A Page Sequence is only for real pages");
  DO_GLOBAL_REFLOW_COUNT("nsSimplePageSequenceFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
  NS_FRAME_TRACE_REFLOW_IN("nsSimplePageSequenceFrame::Reflow");

  aStatus = NS_FRAME_COMPLETE;  // we're always complete

  // Don't do incremental reflow until we've taught tables how to do
  // it right in paginated mode.
  if (!(GetStateBits() & NS_FRAME_FIRST_REFLOW)) {
    // Return our desired size
    SetDesiredSize(aDesiredSize, aReflowState, mSize.width, mSize.height);
    aDesiredSize.SetOverflowAreasToDesiredBounds();
    FinishAndStoreOverflow(&aDesiredSize);
    return NS_OK;
  }

  // See if we can get a Print Settings from the Context
  if (!mPageData->mPrintSettings &&
      aPresContext->Medium() == nsGkAtoms::print) {
      mPageData->mPrintSettings = aPresContext->GetPrintSettings();
  }

  // now get out margins & edges
  if (mPageData->mPrintSettings) {
    nsIntMargin unwriteableTwips;
    mPageData->mPrintSettings->GetUnwriteableMarginInTwips(unwriteableTwips);
    NS_ASSERTION(unwriteableTwips.left  >= 0 && unwriteableTwips.top >= 0 &&
                 unwriteableTwips.right >= 0 && unwriteableTwips.bottom >= 0,
                 "Unwriteable twips should be non-negative");

    nsIntMargin marginTwips;
    mPageData->mPrintSettings->GetMarginInTwips(marginTwips);
    mMargin = aPresContext->CSSTwipsToAppUnits(marginTwips + unwriteableTwips);

    PRInt16 printType;
    mPageData->mPrintSettings->GetPrintRange(&printType);
    mPrintRangeType = printType;

    nsIntMargin edgeTwips;
    mPageData->mPrintSettings->GetEdgeInTwips(edgeTwips);

    // sanity check the values. three inches are sometimes needed
    PRInt32 inchInTwips = NS_INCHES_TO_INT_TWIPS(3.0);
    edgeTwips.top = NS_MIN(NS_MAX(edgeTwips.top, 0), inchInTwips);
    edgeTwips.bottom = NS_MIN(NS_MAX(edgeTwips.bottom, 0), inchInTwips);
    edgeTwips.left = NS_MIN(NS_MAX(edgeTwips.left, 0), inchInTwips);
    edgeTwips.right = NS_MIN(NS_MAX(edgeTwips.right, 0), inchInTwips);

    mPageData->mEdgePaperMargin =
      aPresContext->CSSTwipsToAppUnits(edgeTwips + unwriteableTwips);
  }

  // *** Special Override ***
  // If this is a sub-sdoc (meaning it doesn't take the whole page)
  // and if this Document is in the upper left hand corner
  // we need to suppress the top margin or it will reflow too small

  nsSize pageSize = aPresContext->GetPageSize();

  mPageData->mReflowSize = pageSize;
  // If we're printing a selection, we need to reflow with
  // unconstrained height, to make sure we'll get to the selection
  // even if it's beyond the first page of content.
  if (nsIPrintSettings::kRangeSelection == mPrintRangeType) {
    mPageData->mReflowSize.height = NS_UNCONSTRAINEDSIZE;
  }
  mPageData->mReflowMargin = mMargin;

  // Compute the size of each page and the x coordinate that each page will
  // be placed at
  nscoord extraThreshold = NS_MAX(pageSize.width, pageSize.height)/10;
  PRInt32 gapInTwips = Preferences::GetInt("print.print_extra_margin");
  gapInTwips = NS_MAX(0, gapInTwips);

  nscoord extraGap = aPresContext->CSSTwipsToAppUnits(gapInTwips);
  extraGap = NS_MIN(extraGap, extraThreshold); // clamp to 1/10 of the largest dim of the page

  nsMargin extraMargin(0,0,0,0);
  if (aPresContext->IsScreen()) {
    extraMargin.SizeTo(extraGap, extraGap, extraGap, extraGap);
  }

  mPageData->mExtraMargin = extraMargin;

  // We use the CSS "margin" property on the -moz-page pseudoelement
  // to determine the space between each page in print preview.
  // Keep a running y-offset for each page.
  nscoord y = 0;
  nscoord maxXMost = 0;

  nsSize availSize(pageSize.width + extraMargin.LeftRight(),
                   pageSize.height + extraMargin.TopBottom());

  // Tile the pages vertically
  nsHTMLReflowMetrics kidSize;
  for (nsIFrame* kidFrame = mFrames.FirstChild(); nsnull != kidFrame; ) {
    // Set the shared data into the page frame before reflow
    nsPageFrame * pf = static_cast<nsPageFrame*>(kidFrame);
    pf->SetSharedPageData(mPageData);

    // Reflow the page
    nsHTMLReflowState kidReflowState(aPresContext, aReflowState, kidFrame,
                                     availSize);
    nsReflowStatus  status;

    kidReflowState.SetComputedWidth(kidReflowState.availableWidth);
    //kidReflowState.SetComputedHeight(kidReflowState.availableHeight);
    PR_PL(("AV W: %d   H: %d\n", kidReflowState.availableWidth, kidReflowState.availableHeight));

    nsMargin pageCSSMargin = kidReflowState.mComputedMargin;
    y += pageCSSMargin.top;
    const nscoord x = pageCSSMargin.left;

    // Place and size the page. If the page is narrower than our
    // max width then center it horizontally
    ReflowChild(kidFrame, aPresContext, kidSize, kidReflowState, x, y, 0, status);

    FinishReflowChild(kidFrame, aPresContext, nsnull, kidSize, x, y, 0);
    y += kidSize.height;
    y += pageCSSMargin.bottom;

    maxXMost = NS_MAX(maxXMost, x + kidSize.width + pageCSSMargin.right);

    // Is the page complete?
    nsIFrame* kidNextInFlow = kidFrame->GetNextInFlow();

    if (NS_FRAME_IS_FULLY_COMPLETE(status)) {
      NS_ASSERTION(!kidNextInFlow, "bad child flow list");
    } else if (!kidNextInFlow) {
      // The page isn't complete and it doesn't have a next-in-flow, so
      // create a continuing page.
      nsIFrame* continuingPage;
      nsresult rv = aPresContext->PresShell()->FrameConstructor()->
        CreateContinuingFrame(aPresContext, kidFrame, this, &continuingPage);
      if (NS_FAILED(rv)) {
        break;
      }

      // Add it to our child list
      mFrames.InsertFrame(nsnull, kidFrame, continuingPage);
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
    nsPageFrame * pf = static_cast<nsPageFrame*>(page);
    if (pf != nsnull) {
      pf->SetPageNumInfo(pageNum, pageTot);
    }
    pageNum++;
  }

  // Create current Date/Time String
  if (!mDateFormatter)
    mDateFormatter = do_CreateInstance(NS_DATETIMEFORMAT_CONTRACTID);

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

  // Return our desired size
  // Adjust the reflow size by PrintPreviewScale so the scrollbars end up the
  // correct size
  SetDesiredSize(aDesiredSize, aReflowState, maxXMost, y);

  aDesiredSize.SetOverflowAreasToDesiredBounds();
  FinishAndStoreOverflow(&aDesiredSize);

  // cache the size so we can set the desired size 
  // for the other reflows that happen
  mSize.width  = maxXMost;
  mSize.height = y;

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
nsSimplePageSequenceFrame::IsDoingPrintRange(bool* aDoing)
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
nsSimplePageSequenceFrame::SetPageNumberFormat(const char* aPropName, const char* aDefPropVal, bool aPageNumOnly)
{
  // Doing this here so we only have to go get these formats once
  nsXPIDLString pageNumberFormat;
  // Now go get the Localized Page Formating String
  nsresult rv =
    nsContentUtils::GetLocalizedString(nsContentUtils::ePRINTING_PROPERTIES,
                                       aPropName, pageNumberFormat);
  if (NS_FAILED(rv)) { // back stop formatting
    pageNumberFormat.AssignASCII(aDefPropVal);
  }

  // Sets the format into a static data member which will own the memory and free it
  PRUnichar* uStr = ToNewUnicode(pageNumberFormat);
  if (uStr != nsnull) {
    SetPageNumberFormat(uStr, aPageNumOnly); // nsPageFrame will own the memory
  }

}

NS_IMETHODIMP
nsSimplePageSequenceFrame::StartPrint(nsPresContext*   aPresContext,
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

  aPrintSettings->GetStartPageRange(&mFromPageNum);
  aPrintSettings->GetEndPageRange(&mToPageNum);

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

  // Determine if we are rendering only the selection
  aPresContext->SetIsRenderingOnlySelection(nsIPrintSettings::kRangeSelection == mPrintRangeType);


  if (mDoingPageRange) {
    // XXX because of the hack for making the selection all print on one page
    // we must make sure that the page is sized correctly before printing.
    nscoord height = aPresContext->GetPageSize().height;

    PRInt32 pageNum = 1;
    nscoord y = 0;//mMargin.top;

    for (nsIFrame* page = mFrames.FirstChild(); page;
         page = page->GetNextSibling()) {
      if (pageNum >= mFromPageNum && pageNum <= mToPageNum) {
        nsRect rect = page->GetRect();
        rect.y = y;
        rect.height = height;
        page->SetRect(rect);
        y += rect.height + mMargin.top + mMargin.bottom;
      }
      pageNum++;
    }

    // adjust total number of pages
    if (nsIPrintSettings::kRangeSelection != mPrintRangeType) {
      totalPages = pageNum - 1;
    }
  }

  mPageNum          = 1;
  mCurrentPageFrame = mFrames.FirstChild();

  if (mTotalPages == -1) {
    mTotalPages = totalPages;
  }

  return rv;
}

NS_IMETHODIMP
nsSimplePageSequenceFrame::PrintNextPage()
{
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

  bool printEvenPages, printOddPages;
  mPageData->mPrintSettings->GetPrintOptions(nsIPrintSettings::kPrintEvenPages, &printEvenPages);
  mPageData->mPrintSettings->GetPrintOptions(nsIPrintSettings::kPrintOddPages, &printOddPages);

  // Begin printing of the document
  nsDeviceContext *dc = PresContext()->DeviceContext();

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
  
  if (nsIPrintSettings::kRangeSelection == mPrintRangeType) {
    mPrintThisPage = PR_TRUE;
  }

  if (mPrintThisPage) {
    // XXX This is temporary fix for printing more than one page of a selection
    // This does a poor man's "dump" pagination (see Bug 89353)
    // It has laid out as one long page and now we are just moving or view up/down 
    // one page at a time and printing the contents of what is exposed by the rect.
    // currently this does not work for IFrames
    // I will soon improve this to work with IFrames 
    bool    continuePrinting = true;
    nscoord width, height;
    width = PresContext()->GetPageSize().width;
    height = PresContext()->GetPageSize().height;
    height -= mMargin.top + mMargin.bottom;
    width  -= mMargin.left + mMargin.right;
    nscoord selectionY = height;
    nsIFrame* conFrame = mCurrentPageFrame->GetFirstPrincipalChild();
    if (mSelectionHeight >= 0) {
      conFrame->SetPosition(conFrame->GetPosition() + nsPoint(0, -mYSelOffset));
      nsContainerFrame::PositionChildViews(conFrame);
    }

    // cast the frame to be a page frame
    nsPageFrame * pf = static_cast<nsPageFrame*>(mCurrentPageFrame);
    pf->SetPageNumInfo(mPageNum, mTotalPages);
    pf->SetSharedPageData(mPageData);

    PRInt32 printedPageNum = 1;
    while (continuePrinting) {
      if (PresContext()->IsRootPaginatedDocument()) {
        PR_PL(("\n"));
        PR_PL(("***************** BeginPage *****************\n"));
        rv = dc->BeginPage();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      PR_PL(("SeqFr::PrintNextPage -> %p PageNo: %d", pf, mPageNum));

      nsRefPtr<nsRenderingContext> renderingContext;
      dc->CreateRenderingContext(*getter_AddRefs(renderingContext));
      NS_ENSURE_TRUE(renderingContext, NS_ERROR_OUT_OF_MEMORY);

      nsRect drawingRect(nsPoint(0, 0),
                         mCurrentPageFrame->GetSize());
      nsRegion drawingRegion(drawingRect);
      nsLayoutUtils::PaintFrame(renderingContext, mCurrentPageFrame,
                                drawingRegion, NS_RGBA(0,0,0,0),
                                nsLayoutUtils::PAINT_SYNC_DECODE_IMAGES);

      if (mSelectionHeight >= 0 && selectionY < mSelectionHeight) {
        selectionY += height;
        printedPageNum++;
        pf->SetPageNumInfo(printedPageNum, mTotalPages);
        conFrame->SetPosition(conFrame->GetPosition() + nsPoint(0, -height));
        nsContainerFrame::PositionChildViews(conFrame);

        PR_PL(("***************** End Page (PrintNextPage) *****************\n"));
        rv = dc->EndPage();
        NS_ENSURE_SUCCESS(rv, rv);
      } else {
        continuePrinting = PR_FALSE;
      }
    }
  }
  return rv;
}

NS_IMETHODIMP
nsSimplePageSequenceFrame::DoPageEnd()
{
  nsresult rv = NS_OK;
  if (PresContext()->IsRootPaginatedDocument() && mPrintThisPage) {
    PR_PL(("***************** End Page (DoPageEnd) *****************\n"));
    rv = PresContext()->DeviceContext()->EndPage();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mPageNum++;

  if (mCurrentPageFrame) {
    mCurrentPageFrame = mCurrentPageFrame->GetNextSibling();
  }
  
  return rv;
}

static void PaintPageSequence(nsIFrame* aFrame, nsRenderingContext* aCtx,
                             const nsRect& aDirtyRect, nsPoint aPt)
{
  static_cast<nsSimplePageSequenceFrame*>(aFrame)->PaintPageSequence(*aCtx, aDirtyRect, aPt);
}

//------------------------------------------------------------------------------
void
nsSimplePageSequenceFrame::PaintPageSequence(nsRenderingContext& aRenderingContext,
                                             const nsRect&        aDirtyRect,
                                             nsPoint              aPt) {
  nsRect rect = aDirtyRect;
  float scale = PresContext()->GetPrintPreviewScale();
  aRenderingContext.PushState();
  nsPoint framePos = aPt;
  aRenderingContext.Translate(framePos);
  rect -= framePos;
  aRenderingContext.Scale(scale, scale);
  rect.ScaleRoundOut(1.0f / scale);

  // Now the rect and the rendering coordinates are are relative to this frame.
  // Loop over the pages and paint them.
  nsIFrame* child = GetFirstPrincipalChild();
  while (child) {
    nsPoint pt = child->GetPosition();
    // The rendering context has to be translated before each call to PaintFrame
    aRenderingContext.PushState();
    aRenderingContext.Translate(pt);
    nsLayoutUtils::PaintFrame(&aRenderingContext, child,
                              nsRegion(rect - pt), NS_RGBA(0,0,0,0),
                              nsLayoutUtils::PAINT_SYNC_DECODE_IMAGES);
    aRenderingContext.PopState();
    child = child->GetNextSibling();
  }

  aRenderingContext.PopState();
}

NS_IMETHODIMP
nsSimplePageSequenceFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                            const nsRect&           aDirtyRect,
                                            const nsDisplayListSet& aLists)
{
  nsresult rv = DisplayBorderBackgroundOutline(aBuilder, aLists);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aLists.Content()->AppendNewToTop(new (aBuilder)
        nsDisplayGeneric(aBuilder, this, ::PaintPageSequence, "PageSequence",
                         nsDisplayItem::TYPE_PAGE_SEQUENCE));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsIAtom*
nsSimplePageSequenceFrame::GetType() const
{
  return nsGkAtoms::sequenceFrame; 
}

//------------------------------------------------------------------------------
void
nsSimplePageSequenceFrame::SetPageNumberFormat(PRUnichar * aFormatStr, bool aForPageNumOnly)
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
