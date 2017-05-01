/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSimplePageSequenceFrame.h"

#include "DateTimeFormat.h"
#include "nsCOMPtr.h"
#include "nsDeviceContext.h"
#include "nsPresContext.h"
#include "gfxContext.h"
#include "nsRenderingContext.h"
#include "nsGkAtoms.h"
#include "nsIPresShell.h"
#include "nsIPrintSettings.h"
#include "nsPageFrame.h"
#include "nsSubDocumentFrame.h"
#include "nsRegion.h"
#include "nsCSSFrameConstructor.h"
#include "nsContentUtils.h"
#include "nsDisplayList.h"
#include "nsHTMLCanvasFrame.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "nsICanvasRenderingContextInternal.h"
#include "nsServiceManagerUtils.h"
#include <algorithm>

#define OFFSET_NOT_SET -1

using namespace mozilla;
using namespace mozilla::dom;

#include "mozilla/Logging.h"
mozilla::LazyLogModule gLayoutPrintingLog("printing-layout");

#define PR_PL(_p1)  MOZ_LOG(gLayoutPrintingLog, mozilla::LogLevel::Debug, _p1)

nsSimplePageSequenceFrame*
NS_NewSimplePageSequenceFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsSimplePageSequenceFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsSimplePageSequenceFrame)

nsSimplePageSequenceFrame::nsSimplePageSequenceFrame(nsStyleContext* aContext)
  : nsContainerFrame(aContext, LayoutFrameType::Sequence)
  , mTotalPages(-1)
  , mSelectionHeight(-1)
  , mYSelOffset(0)
  , mCalledBeginPage(false)
  , mCurrentCanvasListSetup(false)
{
  nscoord halfInch = PresContext()->CSSTwipsToAppUnits(NS_INCHES_TO_TWIPS(0.5));
  mMargin.SizeTo(halfInch, halfInch, halfInch, halfInch);

  // XXX Unsafe to assume successful allocation
  mPageData = new nsSharedPageData();
  mPageData->mHeadFootFont =
    *PresContext()->GetDefaultFont(kGenericFont_serif,
                                   aContext->StyleFont()->mLanguage);
  mPageData->mHeadFootFont.size = nsPresContext::CSSPointsToAppUnits(10);

  // Doing this here so we only have to go get these formats once
  SetPageNumberFormat("pagenumber",  "%1$d", true);
  SetPageNumberFormat("pageofpages", "%1$d of %2$d", false);
}

nsSimplePageSequenceFrame::~nsSimplePageSequenceFrame()
{
  delete mPageData;
  ResetPrintCanvasList();
}

NS_QUERYFRAME_HEAD(nsSimplePageSequenceFrame)
  NS_QUERYFRAME_ENTRY(nsIPageSequenceFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

//----------------------------------------------------------------------

void
nsSimplePageSequenceFrame::SetDesiredSize(ReflowOutput& aDesiredSize,
                                          const ReflowInput& aReflowInput,
                                          nscoord aWidth,
                                          nscoord aHeight)
{
  // Aim to fill the whole size of the document, not only so we
  // can act as a background in print preview but also handle overflow
  // in child page frames correctly.
  // Use availableISize so we don't cause a needless horizontal scrollbar.
  WritingMode wm = aReflowInput.GetWritingMode();
  nscoord scaledWidth = aWidth * PresContext()->GetPrintPreviewScale();
  nscoord scaledHeight = aHeight * PresContext()->GetPrintPreviewScale();

  nscoord scaledISize = (wm.IsVertical() ? scaledHeight : scaledWidth);
  nscoord scaledBSize = (wm.IsVertical() ? scaledWidth : scaledHeight);

  aDesiredSize.ISize(wm) = std::max(scaledISize, aReflowInput.AvailableISize());
  aDesiredSize.BSize(wm) = std::max(scaledBSize, aReflowInput.ComputedBSize());
}

// Helper function to compute the offset needed to center a child
// page-frame's margin-box inside our content-box.
nscoord
nsSimplePageSequenceFrame::ComputeCenteringMargin(
  nscoord aContainerContentBoxWidth,
  nscoord aChildPaddingBoxWidth,
  const nsMargin& aChildPhysicalMargin)
{
  // We'll be centering our child's margin-box, so get the size of that:
  nscoord childMarginBoxWidth =
    aChildPaddingBoxWidth + aChildPhysicalMargin.LeftRight();

  // When rendered, our child's rect will actually be scaled up by the
  // print-preview scale factor, via ComputePageSequenceTransform().
  // We really want to center *that scaled-up rendering* inside of
  // aContainerContentBoxWidth.  So, we scale up its margin-box here...
  auto ppScale = PresContext()->GetPrintPreviewScale();
  nscoord scaledChildMarginBoxWidth =
    NSToCoordRound(childMarginBoxWidth * ppScale);

  // ...and see we how much space is left over, when we subtract that scaled-up
  // size from the container width:
  nscoord scaledExtraSpace =
    aContainerContentBoxWidth - scaledChildMarginBoxWidth;

  if (scaledExtraSpace <= 0) {
    // (Don't bother centering if there's zero/negative space.)
    return 0;
  }

  // To center the child, we want to give it an additional left-margin of half
  // of the extra space.  And then, we have to scale that space back down, so
  // that it'll produce the correct scaled-up amount when we render (because
  // rendering will scale it back up):
  return NSToCoordRound(scaledExtraSpace * 0.5 / ppScale);
}

/*
 * Note: we largely position/size out our children (page frames) using
 * \*physical\* x/y/width/height values, because the print preview UI is always
 * arranged in the same orientation, regardless of writing mode.
 */
void
nsSimplePageSequenceFrame::Reflow(nsPresContext*     aPresContext,
                                  ReflowOutput&      aDesiredSize,
                                  const ReflowInput& aReflowInput,
                                  nsReflowStatus&    aStatus)
{
  MarkInReflow();
  NS_PRECONDITION(aPresContext->IsRootPaginatedDocument(),
                  "A Page Sequence is only for real pages");
  DO_GLOBAL_REFLOW_COUNT("nsSimplePageSequenceFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aDesiredSize, aStatus);
  NS_FRAME_TRACE_REFLOW_IN("nsSimplePageSequenceFrame::Reflow");

  aStatus.Reset();  // we're always complete

  // Don't do incremental reflow until we've taught tables how to do
  // it right in paginated mode.
  if (!(GetStateBits() & NS_FRAME_FIRST_REFLOW)) {
    // Return our desired size
    SetDesiredSize(aDesiredSize, aReflowInput, mSize.width, mSize.height);
    aDesiredSize.SetOverflowAreasToDesiredBounds();
    FinishAndStoreOverflow(&aDesiredSize);

    if (GetRect().Width() != aDesiredSize.Width()) {
      // Our width is changing; we need to re-center our children (our pages).
      for (nsFrameList::Enumerator e(mFrames); !e.AtEnd(); e.Next()) {
        nsIFrame* child = e.get();
        nsMargin pageCSSMargin = child->GetUsedMargin();
        nscoord centeringMargin =
          ComputeCenteringMargin(aReflowInput.ComputedWidth(),
                                 child->GetRect().Width(),
                                 pageCSSMargin);
        nscoord newX = pageCSSMargin.left + centeringMargin;

        // Adjust the child's x-position:
        child->MovePositionBy(nsPoint(newX - child->GetNormalPosition().x, 0));
      }
    }
    return;
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

    int16_t printType;
    mPageData->mPrintSettings->GetPrintRange(&printType);
    mPrintRangeType = printType;

    nsIntMargin edgeTwips;
    mPageData->mPrintSettings->GetEdgeInTwips(edgeTwips);

    // sanity check the values. three inches are sometimes needed
    int32_t inchInTwips = NS_INCHES_TO_INT_TWIPS(3.0);
    edgeTwips.top    = clamped(edgeTwips.top,    0, inchInTwips);
    edgeTwips.bottom = clamped(edgeTwips.bottom, 0, inchInTwips);
    edgeTwips.left   = clamped(edgeTwips.left,   0, inchInTwips);
    edgeTwips.right  = clamped(edgeTwips.right,  0, inchInTwips);

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

  // We use the CSS "margin" property on the -moz-page pseudoelement
  // to determine the space between each page in print preview.
  // Keep a running y-offset for each page.
  nscoord y = 0;
  nscoord maxXMost = 0;

  // Tile the pages vertically
  ReflowOutput kidSize(aReflowInput);
  for (nsFrameList::Enumerator e(mFrames); !e.AtEnd(); e.Next()) {
    nsIFrame* kidFrame = e.get();
    // Set the shared data into the page frame before reflow
    nsPageFrame * pf = static_cast<nsPageFrame*>(kidFrame);
    pf->SetSharedPageData(mPageData);

    // Reflow the page
    ReflowInput kidReflowInput(aPresContext, aReflowInput, kidFrame,
                               LogicalSize(kidFrame->GetWritingMode(),
                                                 pageSize));
    nsReflowStatus  status;

    kidReflowInput.SetComputedISize(kidReflowInput.AvailableISize());
    //kidReflowInput.SetComputedHeight(kidReflowInput.AvailableHeight());
    PR_PL(("AV ISize: %d   BSize: %d\n",
           kidReflowInput.AvailableISize(),
           kidReflowInput.AvailableBSize()));

    nsMargin pageCSSMargin = kidReflowInput.ComputedPhysicalMargin();
    y += pageCSSMargin.top;

    nscoord x = pageCSSMargin.left;

    // Place and size the page.
    ReflowChild(kidFrame, aPresContext, kidSize, kidReflowInput, x, y, 0, status);

    // If the page is narrower than our width, then center it horizontally:
    x += ComputeCenteringMargin(aReflowInput.ComputedWidth(),
                                kidSize.Width(), pageCSSMargin);

    FinishReflowChild(kidFrame, aPresContext, kidSize, nullptr, x, y, 0);
    y += kidSize.Height();
    y += pageCSSMargin.bottom;

    maxXMost = std::max(maxXMost, x + kidSize.Width() + pageCSSMargin.right);

    // Is the page complete?
    nsIFrame* kidNextInFlow = kidFrame->GetNextInFlow();

    if (status.IsFullyComplete()) {
      NS_ASSERTION(!kidNextInFlow, "bad child flow list");
    } else if (!kidNextInFlow) {
      // The page isn't complete and it doesn't have a next-in-flow, so
      // create a continuing page.
      nsIFrame* continuingPage = aPresContext->PresShell()->FrameConstructor()->
        CreateContinuingFrame(aPresContext, kidFrame, this);

      // Add it to our child list
      mFrames.InsertFrame(nullptr, kidFrame, continuingPage);
    }
  }

  // Get Total Page Count
  // XXXdholbert technically we could calculate this in the loop above,
  // instead of needing a separate walk.
  int32_t pageTot = mFrames.GetLength();

  // Set Page Number Info
  int32_t pageNum = 1;
  for (nsFrameList::Enumerator e(mFrames); !e.AtEnd(); e.Next()) {
    MOZ_ASSERT(e.get()->IsPageFrame(),
               "only expecting nsPageFrame children. Other children will make "
               "this static_cast bogus & probably violate other assumptions");
    nsPageFrame* pf = static_cast<nsPageFrame*>(e.get());
    pf->SetPageNumInfo(pageNum, pageTot);
    pageNum++;
  }

  nsAutoString formattedDateString;
  time_t ltime;
  time( &ltime );
  if (NS_SUCCEEDED(DateTimeFormat::FormatTime(kDateFormatShort,
                                              kTimeFormatNoSeconds,
                                              ltime,
                                              formattedDateString))) {
    SetDateTimeStr(formattedDateString);
  }

  // Return our desired size
  // Adjust the reflow size by PrintPreviewScale so the scrollbars end up the
  // correct size
  SetDesiredSize(aDesiredSize, aReflowInput, maxXMost, y);

  aDesiredSize.SetOverflowAreasToDesiredBounds();
  FinishAndStoreOverflow(&aDesiredSize);

  // cache the size so we can set the desired size 
  // for the other reflows that happen
  mSize.width  = maxXMost;
  mSize.height = y;

  NS_FRAME_TRACE_REFLOW_OUT("nsSimplePageSequeceFrame::Reflow", aStatus);
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aDesiredSize);
}

//----------------------------------------------------------------------

#ifdef DEBUG_FRAME_DUMP
nsresult
nsSimplePageSequenceFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("SimplePageSequence"), aResult);
}
#endif

//====================================================================
//== Asynch Printing
//====================================================================
NS_IMETHODIMP
nsSimplePageSequenceFrame::GetCurrentPageNum(int32_t* aPageNum)
{
  NS_ENSURE_ARG_POINTER(aPageNum);

  *aPageNum = mPageNum;
  return NS_OK;
}

NS_IMETHODIMP
nsSimplePageSequenceFrame::GetNumPages(int32_t* aNumPages)
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
nsSimplePageSequenceFrame::GetPrintRange(int32_t* aFromPage, int32_t* aToPage)
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

  SetPageNumberFormat(pageNumberFormat, aPageNumOnly);
}

NS_IMETHODIMP
nsSimplePageSequenceFrame::StartPrint(nsPresContext*    aPresContext,
                                      nsIPrintSettings* aPrintSettings,
                                      const nsAString&  aDocTitle,
                                      const nsAString&  aDocURL)
{
  NS_ENSURE_ARG_POINTER(aPresContext);
  NS_ENSURE_ARG_POINTER(aPrintSettings);

  if (!mPageData->mPrintSettings) {
    mPageData->mPrintSettings = aPrintSettings;
  }

  if (!aDocTitle.IsEmpty()) {
    mPageData->mDocTitle = aDocTitle;
  }
  if (!aDocURL.IsEmpty()) {
    mPageData->mDocURL = aDocURL;
  }

  aPrintSettings->GetStartPageRange(&mFromPageNum);
  aPrintSettings->GetEndPageRange(&mToPageNum);
  aPrintSettings->GetPageRanges(mPageRanges);

  mDoingPageRange = nsIPrintSettings::kRangeSpecifiedPageRange == mPrintRangeType ||
                    nsIPrintSettings::kRangeSelection == mPrintRangeType;

  // If printing a range of pages make sure at least the starting page
  // number is valid
  int32_t totalPages = mFrames.GetLength();

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

    int32_t pageNum = 1;
    nscoord y = 0;//mMargin.top;

    for (nsFrameList::Enumerator e(mFrames); !e.AtEnd(); e.Next()) {
      nsIFrame* page = e.get();
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

  mPageNum = 1;

  if (mTotalPages == -1) {
    mTotalPages = totalPages;
  }

  return rv;
}

void
GetPrintCanvasElementsInFrame(nsIFrame* aFrame, nsTArray<RefPtr<HTMLCanvasElement> >* aArr)
{
  if (!aFrame) {
    return;
  }
  for (nsIFrame::ChildListIterator childLists(aFrame);
    !childLists.IsDone(); childLists.Next()) {

    nsFrameList children = childLists.CurrentList();
    for (nsFrameList::Enumerator e(children); !e.AtEnd(); e.Next()) {
      nsIFrame* child = e.get();

      // Check if child is a nsHTMLCanvasFrame.
      nsHTMLCanvasFrame* canvasFrame = do_QueryFrame(child);

      // If there is a canvasFrame, try to get actual canvas element.
      if (canvasFrame) {
        HTMLCanvasElement* canvas =
          HTMLCanvasElement::FromContentOrNull(canvasFrame->GetContent());
        if (canvas && canvas->GetMozPrintCallback()) {
          aArr->AppendElement(canvas);
          continue;
        }
      }

      if (!child->PrincipalChildList().FirstChild()) {
        nsSubDocumentFrame* subdocumentFrame = do_QueryFrame(child);
        if (subdocumentFrame) {
          // Descend into the subdocument
          nsIFrame* root = subdocumentFrame->GetSubdocumentRootFrame();
          child = root;
        }
      }
      // The current child is not a nsHTMLCanvasFrame OR it is but there is
      // no HTMLCanvasElement on it. Check if children of `child` might
      // contain a HTMLCanvasElement.
      GetPrintCanvasElementsInFrame(child, aArr);
    }
  }
}

void
nsSimplePageSequenceFrame::DetermineWhetherToPrintPage()
{
  // See whether we should print this page
  mPrintThisPage = true;
  bool printEvenPages, printOddPages;
  mPageData->mPrintSettings->GetPrintOptions(nsIPrintSettings::kPrintEvenPages, &printEvenPages);
  mPageData->mPrintSettings->GetPrintOptions(nsIPrintSettings::kPrintOddPages, &printOddPages);

  // If printing a range of pages check whether the page number is in the
  // range of pages to print
  if (mDoingPageRange) {
    if (mPageNum < mFromPageNum) {
      mPrintThisPage = false;
    } else if (mPageNum > mToPageNum) {
      mPageNum++;
      mPrintThisPage = false;
      return;
    } else {
      int32_t length = mPageRanges.Length();
    
      // Page ranges are pairs (start, end)
      if (length && (length % 2 == 0)) {
        mPrintThisPage = false;
      
        int32_t i;
        for (i = 0; i < length; i += 2) {          
          if (mPageRanges[i] <= mPageNum && mPageNum <= mPageRanges[i+1]) {
            mPrintThisPage = true;
            break;
          }
        }
      }
    }
  }

  // Check for printing of odd and even pages
  if (mPageNum & 0x1) {
    if (!printOddPages) {
      mPrintThisPage = false;  // don't print odd numbered page
    }
  } else {
    if (!printEvenPages) {
      mPrintThisPage = false;  // don't print even numbered page
    }
  }
  
  if (nsIPrintSettings::kRangeSelection == mPrintRangeType) {
    mPrintThisPage = true;
  }
}

nsIFrame*
nsSimplePageSequenceFrame::GetCurrentPageFrame()
{
  int32_t i = 1;
  for (nsFrameList::Enumerator childFrames(mFrames); !childFrames.AtEnd();
       childFrames.Next()) {
    if (i == mPageNum) {
      return childFrames.get();
    }
    ++i;
  }
  return nullptr;
}

NS_IMETHODIMP
nsSimplePageSequenceFrame::PrePrintNextPage(nsITimerCallback* aCallback, bool* aDone)
{
  nsIFrame* currentPage = GetCurrentPageFrame();
  if (!currentPage) {
    *aDone = true;
    return NS_ERROR_FAILURE;
  }
  
  DetermineWhetherToPrintPage();
  // Nothing to do if the current page doesn't get printed OR rendering to
  // preview. For preview, the `CallPrintCallback` is called from within the
  // HTMLCanvasElement::HandlePrintCallback.
  if (!mPrintThisPage || !PresContext()->IsRootPaginatedDocument()) {
    *aDone = true;
    return NS_OK;
  }

  // If the canvasList is null, then generate it and start the render
  // process for all the canvas.
  if (!mCurrentCanvasListSetup) {
    mCurrentCanvasListSetup = true;
    GetPrintCanvasElementsInFrame(currentPage, &mCurrentCanvasList);

    if (mCurrentCanvasList.Length() != 0) {
      nsresult rv = NS_OK;

      // Begin printing of the document
      nsDeviceContext *dc = PresContext()->DeviceContext();
      PR_PL(("\n"));
      PR_PL(("***************** BeginPage *****************\n"));
      rv = dc->BeginPage();
      NS_ENSURE_SUCCESS(rv, rv);

      mCalledBeginPage = true;
      
      RefPtr<gfxContext> renderingContext = dc->CreateRenderingContext();
      NS_ENSURE_TRUE(renderingContext, NS_ERROR_OUT_OF_MEMORY);

      DrawTarget* drawTarget = renderingContext->GetDrawTarget();
      if (NS_WARN_IF(!drawTarget)) {
        return NS_ERROR_FAILURE;
      }

      for (int32_t i = mCurrentCanvasList.Length() - 1; i >= 0 ; i--) {
        HTMLCanvasElement* canvas = mCurrentCanvasList[i];
        nsIntSize size = canvas->GetSize();

        RefPtr<DrawTarget> canvasTarget =
          drawTarget->CreateSimilarDrawTarget(size, drawTarget->GetFormat());
        if (!canvasTarget) {
          continue;
        }

        nsICanvasRenderingContextInternal* ctx = canvas->GetContextAtIndex(0);
        if (!ctx) {
          continue;
        }

        // Initialize the context with the new DrawTarget.
        ctx->InitializeWithDrawTarget(nullptr, WrapNotNull(canvasTarget));

        // Start the rendering process.
        AutoWeakFrame weakFrame = this;
        canvas->DispatchPrintCallback(aCallback);
        NS_ENSURE_STATE(weakFrame.IsAlive());
      }
    }
  }
  uint32_t doneCounter = 0;
  for (int32_t i = mCurrentCanvasList.Length() - 1; i >= 0 ; i--) {
    HTMLCanvasElement* canvas = mCurrentCanvasList[i];

    if (canvas->IsPrintCallbackDone()) {
      doneCounter++;
    }
  }
  // If all canvas have finished rendering, return true, otherwise false.
  *aDone = doneCounter == mCurrentCanvasList.Length();

  return NS_OK;
}

NS_IMETHODIMP
nsSimplePageSequenceFrame::ResetPrintCanvasList()
{
  for (int32_t i = mCurrentCanvasList.Length() - 1; i >= 0 ; i--) {
    HTMLCanvasElement* canvas = mCurrentCanvasList[i];
    canvas->ResetPrintCallback();
  }

  mCurrentCanvasList.Clear();
  mCurrentCanvasListSetup = false; 
  return NS_OK;
} 

NS_IMETHODIMP
nsSimplePageSequenceFrame::PrintNextPage()
{
  // This method would be very straightforward except that the
  // "Print Selection Only" functionality (which is a broken mess) is
  // integrated here.  The thing to understand is that if we're printing a
  // selection (which may contain multiple ranges) then we only enter this
  // function once since the content to print is laid out as one arbitrarily
  // long nsPageFrame instead of multiple nsPageFrames that are sized to fit
  // the printer paper size(!).  Each of the "pages" between the start and end
  // of the selection are printed by offsetting the nsPageContentFrame by the
  // index of the page being printed and then drawing the nsPageContentFrame.
  // This does not work for IFrames.
  //
  // Note: When print al the pages or a page range the printed page shows the
  // actual page number, when printing selection it prints the page number starting
  // with the first page of the selection. For example if the user has a 
  // selection that starts on page 2 and ends on page 3, the page numbers when
  // print are 1 and then two (which is different than printing a page range, where
  // the page numbers would have been 2 and then 3)

  nsIFrame* currentPageFrame = GetCurrentPageFrame();
  if (!currentPageFrame) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = NS_OK;

  DetermineWhetherToPrintPage();

  if (mPrintThisPage) {
    nsDeviceContext* dc = PresContext()->DeviceContext();

    nsPageFrame * pf = static_cast<nsPageFrame*>(currentPageFrame);
    pf->SetPageNumInfo(mPageNum, mTotalPages);
    pf->SetSharedPageData(mPageData);

    // Only used if we're printing a selection:
    nsIFrame* selectionContentFrame = nullptr;
    nscoord pageContentHeight =
      PresContext()->GetPageSize().height - (mMargin.top + mMargin.bottom);
    nscoord selectionY = pageContentHeight;
    int32_t selectionCurrentPageNum = 1;
    bool haveUnfinishedSelectionToPrint = false;

    if (mSelectionHeight >= 0) {
      selectionContentFrame = currentPageFrame->PrincipalChildList().FirstChild();
      MOZ_ASSERT(selectionContentFrame->IsPageContentFrame() &&
                 !selectionContentFrame->GetNextSibling(),
                 "Unexpected frame tree");
      // To print a selection we reposition the page content frame for each
      // page.  We can do this (and not have to bother resetting the position
      // after we're done) because we are printing from a static clone document
      // that is thrown away after we finish printing.
      selectionContentFrame->SetPosition(selectionContentFrame->GetPosition() +
                                         nsPoint(0, -mYSelOffset));
      nsContainerFrame::PositionChildViews(selectionContentFrame);
    }

    do {
      if (PresContext()->IsRootPaginatedDocument()) {
        if (!mCalledBeginPage) {
          // We must make sure BeginPage() has been called since some printing
          // backends can't give us a valid rendering context for a [physical]
          // page otherwise.
          PR_PL(("\n"));
          PR_PL(("***************** BeginPage *****************\n"));
          rv = dc->BeginPage();
          NS_ENSURE_SUCCESS(rv, rv);
        }

        // Reset this flag. We reset it early here because if we loop around to
        // print another page's worth of selection we need to call BeginPage
        // again:
        mCalledBeginPage = false;
      }

      PR_PL(("SeqFr::PrintNextPage -> %p PageNo: %d", pf, mPageNum));

      // CreateRenderingContext can fail
      RefPtr<gfxContext> gCtx = dc->CreateRenderingContext();
      NS_ENSURE_TRUE(gCtx, NS_ERROR_OUT_OF_MEMORY);

      nsRenderingContext renderingContext(gCtx);

      nsRect drawingRect(nsPoint(0, 0), currentPageFrame->GetSize());
      nsRegion drawingRegion(drawingRect);
      nsLayoutUtils::PaintFrame(&renderingContext, currentPageFrame,
                                drawingRegion, NS_RGBA(0,0,0,0),
                                nsDisplayListBuilderMode::PAINTING,
                                nsLayoutUtils::PaintFrameFlags::PAINT_SYNC_DECODE_IMAGES);

      if (mSelectionHeight >= 0) {
        haveUnfinishedSelectionToPrint = (selectionY < mSelectionHeight);
        if (haveUnfinishedSelectionToPrint) {
          selectionY += pageContentHeight;
          selectionCurrentPageNum++;
          pf->SetPageNumInfo(selectionCurrentPageNum, mTotalPages);
          selectionContentFrame->SetPosition(selectionContentFrame->GetPosition() +
                                             nsPoint(0, -pageContentHeight));
          nsContainerFrame::PositionChildViews(selectionContentFrame);

          // We're going to loop and call BeginPage to print another page's worth
          // of selection so we need to call EndPage first.  (Otherwise, EndPage
          // is called in DoEndPage.)
          PR_PL(("***************** End Page (PrintNextPage) *****************\n"));
          rv = dc->EndPage();
          NS_ENSURE_SUCCESS(rv, rv);
        }
      }
    } while (haveUnfinishedSelectionToPrint);
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

  ResetPrintCanvasList();

  mPageNum++;
  
  return rv;
}

inline gfx::Matrix4x4
ComputePageSequenceTransform(nsIFrame* aFrame, float aAppUnitsPerPixel)
{
  float scale = aFrame->PresContext()->GetPrintPreviewScale();
  return gfx::Matrix4x4::Scaling(scale, scale, 1);
}

void
nsSimplePageSequenceFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                            const nsRect&           aDirtyRect,
                                            const nsDisplayListSet& aLists)
{
  DisplayBorderBackgroundOutline(aBuilder, aLists);

  nsDisplayList content;

  {
    // Clear clip state while we construct the children of the
    // nsDisplayTransform, since they'll be in a different coordinate system.
    DisplayListClipState::AutoSaveRestore clipState(aBuilder);
    clipState.Clear();

    nsIFrame* child = PrincipalChildList().FirstChild();
    nsRect dirty = aDirtyRect;
    dirty.ScaleInverseRoundOut(PresContext()->GetPrintPreviewScale());

    while (child) {
      if (child->GetVisualOverflowRectRelativeToParent().Intersects(dirty)) {
        child->BuildDisplayListForStackingContext(aBuilder,
            dirty - child->GetPosition(), &content);
        aBuilder->ResetMarkedFramesForDisplayList();
      }
      child = child->GetNextSibling();
    }
  }

  content.AppendNewToTop(new (aBuilder)
      nsDisplayTransform(aBuilder, this, &content, content.GetVisibleRect(),
                         ::ComputePageSequenceTransform));

  aLists.Content()->AppendToTop(&content);
}

//------------------------------------------------------------------------------
void
nsSimplePageSequenceFrame::SetPageNumberFormat(const nsAString& aFormatStr, bool aForPageNumOnly)
{ 
  NS_ASSERTION(mPageData != nullptr, "mPageData string cannot be null!");

  if (aForPageNumOnly) {
    mPageData->mPageNumFormat = aFormatStr;
  } else {
    mPageData->mPageNumAndTotalsFormat = aFormatStr;
  }
}

//------------------------------------------------------------------------------
void
nsSimplePageSequenceFrame::SetDateTimeStr(const nsAString& aDateTimeStr)
{ 
  NS_ASSERTION(mPageData != nullptr, "mPageData string cannot be null!");

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
  aSTFPercent = mPageData->mShrinkToFitRatio;
  return NS_OK;
}
