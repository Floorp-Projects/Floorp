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

#include "nsPageFrame.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "nsIRenderingContext.h"
#include "nsGkAtoms.h"
#include "nsIPresShell.h"
#include "nsCSSFrameConstructor.h"
#include "nsIDeviceContext.h"
#include "nsReadableUtils.h"
#include "nsPageContentFrame.h"
#include "nsDisplayList.h"
#include "nsLayoutUtils.h" // for function BinarySearchForPosition
#include "nsCSSRendering.h"
#include "nsSimplePageSequence.h" // for nsSharedPageData
#include "nsTextFormatter.h" // for page number localization formatting
#ifdef IBMBIDI
#include "nsBidiUtils.h"
#include "nsBidiPresUtils.h"
#endif
#include "nsIFontMetrics.h"
#include "nsIPrintSettings.h"
#include "nsRegion.h"

#include "prlog.h"
#ifdef PR_LOGGING 
extern PRLogModuleInfo * kLayoutPrintingLogMod;
#define PR_PL(_p1)  PR_LOG(kLayoutPrintingLogMod, PR_LOG_DEBUG, _p1)
#else
#define PR_PL(_p1)
#endif

nsIFrame*
NS_NewPageFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsPageFrame(aContext);
}

nsPageFrame::nsPageFrame(nsStyleContext* aContext)
: nsContainerFrame(aContext)
{
}

nsPageFrame::~nsPageFrame()
{
}

NS_IMETHODIMP nsPageFrame::Reflow(nsPresContext*          aPresContext,
                                  nsHTMLReflowMetrics&     aDesiredSize,
                                  const nsHTMLReflowState& aReflowState,
                                  nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsPageFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
  aStatus = NS_FRAME_COMPLETE;  // initialize out parameter

  // Do we have any children?
  // XXX We should use the overflow list instead...
  nsIFrame*           firstFrame  = mFrames.FirstChild();
  nsPageContentFrame* contentPage = NS_STATIC_CAST(nsPageContentFrame*, firstFrame);
  NS_ASSERTION(contentPage, "There should always be a content page");
  NS_ASSERTION(nsGkAtoms::pageContentFrame == firstFrame->GetType(),
               "This frame isn't a pageContentFrame");

  if (contentPage && GetPrevInFlow() && !contentPage->GetFirstChild(nsnull)) {

    nsPageFrame*        prevPage        = NS_STATIC_CAST(nsPageFrame*, GetPrevInFlow());
    nsPageContentFrame* prevContentPage = NS_STATIC_CAST(nsPageContentFrame*, prevPage->mFrames.FirstChild());
    nsIFrame*           prevLastChild   = prevContentPage->mFrames.LastChild();

    // Create a continuing child of the previous page's last child
    nsIFrame*     newFrame;

    nsresult rv = aPresContext->PresShell()->FrameConstructor()->
      CreateContinuingFrame(aPresContext, prevLastChild,
                            contentPage, &newFrame);
    if (NS_FAILED(rv)) {
      return rv;
    }
    // Make the new area frame the 1st child of the page content frame. There may already be
    // children placeholders which don't get reflowed but must not be destroyed until the 
    // page content frame is destroyed.
    contentPage->mFrames.InsertFrame(contentPage, nsnull, newFrame);
  }

  // Resize our frame allowing it only to be as big as we are
  // XXX Pay attention to the page's border and padding...
  if (mFrames.NotEmpty()) {
    nsIFrame* frame = mFrames.FirstChild();
    // When the reflow size is NS_UNCONSTRAINEDSIZE it means we are reflowing
    // a single page to print selection. So this means we want to use
    // NS_UNCONSTRAINEDSIZE without altering it
    nscoord avHeight;
    if (mPD->mReflowSize.height == NS_UNCONSTRAINEDSIZE) {
      avHeight = NS_UNCONSTRAINEDSIZE;
    } else {
      avHeight = mPD->mReflowSize.height - mPD->mReflowMargin.TopBottom();
    }
    nsSize  maxSize(mPD->mReflowSize.width - mPD->mReflowMargin.LeftRight(),
                    avHeight);
    float scale = aPresContext->GetPageScale();
    maxSize.width = NSToCoordCeil(maxSize.width / scale);
    maxSize.height = NSToCoordCeil(maxSize.height / scale);
    // Get the number of Twips per pixel from the PresContext
    nscoord onePixelInTwips = nsPresContext::CSSPixelsToAppUnits(1);
    // insurance against infinite reflow, when reflowing less than a pixel
    // XXX Shouldn't we do something more friendly when invalid margins
    //     are set?
    if (maxSize.width < onePixelInTwips || maxSize.height < onePixelInTwips) {
      aDesiredSize.width  = 0;
      aDesiredSize.height = 0;
      NS_WARNING("Reflow aborted; no space for content");
      return NS_OK;
    }

    nsHTMLReflowState kidReflowState(aPresContext, aReflowState, frame, maxSize);
    kidReflowState.mFlags.mIsTopOfPage = PR_TRUE;
    kidReflowState.mFlags.mTableIsSplittable = PR_TRUE;

    // calc location of frame
    nscoord xc = mPD->mReflowMargin.left + mPD->mExtraMargin.left;
    nscoord yc = mPD->mReflowMargin.top + mPD->mExtraMargin.top;

    // Get the child's desired size
    ReflowChild(frame, aPresContext, aDesiredSize, kidReflowState, xc, yc, 0, aStatus);

    // Place and size the child
    FinishReflowChild(frame, aPresContext, &kidReflowState, aDesiredSize, xc, yc, 0);

    NS_ASSERTION(!NS_FRAME_IS_COMPLETE(aStatus) ||
                 !frame->GetNextInFlow(), "bad child flow list");
  }
  PR_PL(("PageFrame::Reflow %p ", this));
  PR_PL(("[%d,%d][%d,%d]\n", aDesiredSize.width, aDesiredSize.height, aReflowState.availableWidth, aReflowState.availableHeight));

  // Return our desired size
  aDesiredSize.width = aReflowState.availableWidth;
  if (aReflowState.availableHeight != NS_UNCONSTRAINEDSIZE) {
    aDesiredSize.height = aReflowState.availableHeight;
  }
  PR_PL(("PageFrame::Reflow %p ", this));
  PR_PL(("[%d,%d]\n", aReflowState.availableWidth, aReflowState.availableHeight));

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return NS_OK;
}

nsIAtom*
nsPageFrame::GetType() const
{
  return nsGkAtoms::pageFrame; 
}

#ifdef DEBUG
NS_IMETHODIMP
nsPageFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("Page"), aResult);
}
#endif

/* virtual */ PRBool
nsPageFrame::IsContainingBlock() const
{
  return PR_TRUE;
}

void 
nsPageFrame::ProcessSpecialCodes(const nsString& aStr, nsString& aNewStr)
{

  aNewStr = aStr;

  // Search to see if the &D code is in the string 
  // then subst in the current date/time
  NS_NAMED_LITERAL_STRING(kDate, "&D");
  if (aStr.Find(kDate) != kNotFound) {
    if (mPD->mDateTimeStr != nsnull) {
      aNewStr.ReplaceSubstring(kDate.get(), mPD->mDateTimeStr);
    } else {
      aNewStr.ReplaceSubstring(kDate.get(), EmptyString().get());
    }
  }

  // NOTE: Must search for &PT before searching for &P
  //
  // Search to see if the "page number and page" total code are in the string
  // and replace the page number and page total code with the actual
  // values
  NS_NAMED_LITERAL_STRING(kPageAndTotal, "&PT");
  if (aStr.Find(kPageAndTotal) != kNotFound) {
    PRUnichar * uStr = nsTextFormatter::smprintf(mPD->mPageNumAndTotalsFormat, mPageNum, mTotNumPages);
    aNewStr.ReplaceSubstring(kPageAndTotal.get(), uStr);
    nsMemory::Free(uStr);
  }

  // Search to see if the page number code is in the string
  // and replace the page number code with the actual value
  NS_NAMED_LITERAL_STRING(kPage, "&P");
  if (aStr.Find(kPage) != kNotFound) {
    PRUnichar * uStr = nsTextFormatter::smprintf(mPD->mPageNumFormat, mPageNum);
    aNewStr.ReplaceSubstring(kPage.get(), uStr);
    nsMemory::Free(uStr);
  }

  NS_NAMED_LITERAL_STRING(kTitle, "&T");
  if (aStr.Find(kTitle) != kNotFound) {
    if (mPD->mDocTitle != nsnull) {
      aNewStr.ReplaceSubstring(kTitle.get(), mPD->mDocTitle);
    } else {
      aNewStr.ReplaceSubstring(kTitle.get(), EmptyString().get());
    }
  }

  NS_NAMED_LITERAL_STRING(kDocURL, "&U");
  if (aStr.Find(kDocURL) != kNotFound) {
    if (mPD->mDocURL != nsnull) {
      aNewStr.ReplaceSubstring(kDocURL.get(), mPD->mDocURL);
    } else {
      aNewStr.ReplaceSubstring(kDocURL.get(), EmptyString().get());
    }
  }

  NS_NAMED_LITERAL_STRING(kPageTotal, "&L");
  if (aStr.Find(kPageTotal) != kNotFound) {
    PRUnichar * uStr = nsTextFormatter::smprintf(mPD->mPageNumFormat, mTotNumPages);
    aNewStr.ReplaceSubstring(kPageTotal.get(), uStr);
    nsMemory::Free(uStr);
  }
}


//------------------------------------------------------------------------------
nscoord nsPageFrame::GetXPosition(nsIRenderingContext& aRenderingContext, 
                                  const nsRect&        aRect, 
                                  PRInt32              aJust,
                                  const nsString&      aStr)
{
  nscoord width = nsLayoutUtils::GetStringWidth(this, &aRenderingContext,
                                                aStr.get(), aStr.Length());

  nscoord x = aRect.x;
  switch (aJust) {
    case nsIPrintSettings::kJustLeft:
      x += mPD->mExtraMargin.left + mPD->mEdgePaperMargin.left;
      break;

    case nsIPrintSettings::kJustCenter:
      x += (aRect.width - width) / 2;
      break;

    case nsIPrintSettings::kJustRight:
      x += aRect.width - width - mPD->mExtraMargin.right - mPD->mEdgePaperMargin.right;
      break;
  } // switch

  return x;
}

// Draw a header or footer
// @param aRenderingContext - rendering content ot draw into
// @param aHeaderFooter - indicates whether it is a header or footer
// @param aStrLeft - string for the left header or footer; can be empty
// @param aStrCenter - string for the center header or footer; can be empty
// @param aStrRight - string for the right header or footer; can be empty
// @param aRect - the rect of the page
// @param aAscent - the ascent of the font
// @param aHeight - the height of the font
void
nsPageFrame::DrawHeaderFooter(nsIRenderingContext& aRenderingContext,
                              nsHeaderFooterEnum   aHeaderFooter,
                              const nsString&      aStrLeft,
                              const nsString&      aStrCenter,
                              const nsString&      aStrRight,
                              const nsRect&        aRect,
                              nscoord              aAscent,
                              nscoord              aHeight)
{
  PRInt32 numStrs = 0;
  if (!aStrLeft.IsEmpty()) numStrs++;
  if (!aStrCenter.IsEmpty()) numStrs++;
  if (!aStrRight.IsEmpty()) numStrs++;

  if (numStrs == 0) return;
  nscoord strSpace = aRect.width / numStrs;

  if (!aStrLeft.IsEmpty()) {
    DrawHeaderFooter(aRenderingContext, aHeaderFooter,
                     nsIPrintSettings::kJustLeft, aStrLeft, aRect, aAscent,
                     aHeight, strSpace);
  }
  if (!aStrCenter.IsEmpty()) {
    DrawHeaderFooter(aRenderingContext, aHeaderFooter,
                     nsIPrintSettings::kJustCenter, aStrCenter, aRect, aAscent,
                     aHeight, strSpace);
  }
  if (!aStrRight.IsEmpty()) {
    DrawHeaderFooter(aRenderingContext, aHeaderFooter,
                     nsIPrintSettings::kJustRight, aStrRight, aRect, aAscent,
                     aHeight, strSpace);
  }
}

// Draw a header or footer string
// @param aRenderingContext - rendering content ot draw into
// @param aHeaderFooter - indicates whether it is a header or footer
// @param aJust - indicates where the string is located within the header/footer
// @param aStr - the string to be drawn
// @param aRect - the rect of the page
// @param aHeight - the height of the font
// @param aAscent - the ascent of the font
// @param aWidth - available width for the string
void
nsPageFrame::DrawHeaderFooter(nsIRenderingContext& aRenderingContext,
                              nsHeaderFooterEnum   aHeaderFooter,
                              PRInt32              aJust,
                              const nsString&      aStr,
                              const nsRect&        aRect,
                              nscoord              aAscent,
                              nscoord              aHeight,
                              nscoord              aWidth)
{

  nscoord contentWidth = aWidth - (mPD->mEdgePaperMargin.left + mPD->mEdgePaperMargin.right);

  if ((aHeaderFooter == eHeader && aHeight < mPD->mReflowMargin.top) ||
      (aHeaderFooter == eFooter && aHeight < mPD->mReflowMargin.bottom)) {
    nsAutoString str;
    ProcessSpecialCodes(aStr, str);

    PRInt32 indx;
    PRInt32 textWidth = 0;
    const PRUnichar* text = str.get();

    PRInt32 len = (PRInt32)str.Length();
    if (len == 0) {
      return; // bail is empty string
    }
    // find how much text fits, the "position" is the size of the available area
    if (nsLayoutUtils::BinarySearchForPosition(&aRenderingContext, text, 0, 0, 0, len,
                                PRInt32(contentWidth), indx, textWidth)) {
      if (indx < len-1 ) {
        // we can't fit in all the text
        if (indx > 3) {
          // But we can fit in at least 4 chars.  Show all but 3 of them, then
          // an ellipsis.
          // XXXbz for non-plane0 text, this may be cutting things in the
          // middle of a codepoint!  Also, we have no guarantees that the three
          // dots will fit in the space the three chars we removed took up with
          // these font metrics!
          str.Truncate(indx-3);
          str.AppendLiteral("...");
        } else {
          // We can only fit 3 or fewer chars.  Just show nothing
          str.Truncate();
        }
      }
    } else { 
      return; // bail if couldn't find the correct length
    }

    // cacl the x and y positions of the text
    nscoord x = GetXPosition(aRenderingContext, aRect, aJust, str);
    nscoord y;
    if (aHeaderFooter == eHeader) {
      y = aRect.y + mPD->mExtraMargin.top + mPD->mEdgePaperMargin.top;
    } else {
      y = aRect.YMost() - aHeight - mPD->mExtraMargin.bottom - mPD->mEdgePaperMargin.bottom;
    }

    // set up new clip and draw the text
    aRenderingContext.PushState();
    aRenderingContext.SetColor(NS_RGB(0,0,0));
    aRenderingContext.SetClipRect(aRect, nsClipCombine_kReplace);
    nsLayoutUtils::DrawString(this, &aRenderingContext, str.get(), str.Length(), nsPoint(x, y + aAscent));
    aRenderingContext.PopState();
  }
}

static void PaintPrintPreviewBackground(nsIFrame* aFrame, nsIRenderingContext* aCtx,
                                        const nsRect& aDirtyRect, nsPoint aPt)
{
  NS_STATIC_CAST(nsPageFrame*, aFrame)->PaintPrintPreviewBackground(*aCtx, aPt);
}

static void PaintPageContent(nsIFrame* aFrame, nsIRenderingContext* aCtx,
                             const nsRect& aDirtyRect, nsPoint aPt)
{
  NS_STATIC_CAST(nsPageFrame*, aFrame)->PaintPageContent(*aCtx, aDirtyRect, aPt);
}

static void PaintHeaderFooter(nsIFrame* aFrame, nsIRenderingContext* aCtx,
                              const nsRect& aDirtyRect, nsPoint aPt)
{
  NS_STATIC_CAST(nsPageFrame*, aFrame)->PaintHeaderFooter(*aCtx, aPt);
}

//------------------------------------------------------------------------------
NS_IMETHODIMP
nsPageFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists)
{
  nsDisplayListCollection set;
  nsresult rv;

  if (GetPresContext()->IsScreen()) {
    rv = set.BorderBackground()->AppendNewToTop(new (aBuilder)
        nsDisplayGeneric(this, ::PaintPrintPreviewBackground, "PrintPreviewBackground"));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = set.BorderBackground()->AppendNewToTop(new (aBuilder)
        nsDisplayGeneric(this, ::PaintPageContent, "PageContent"));
  NS_ENSURE_SUCCESS(rv, rv);

  if (GetPresContext()->IsRootPaginatedDocument()) {
    rv = set.Content()->AppendNewToTop(new (aBuilder)
        nsDisplayGeneric(this, ::PaintHeaderFooter, "HeaderFooter"));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  set.MoveTo(aLists);
  return NS_OK;
}

//------------------------------------------------------------------------------
void
nsPageFrame::SetPageNumInfo(PRInt32 aPageNumber, PRInt32 aTotalPages) 
{ 
  mPageNum     = aPageNumber; 
  mTotNumPages = aTotalPages;
}


void
nsPageFrame::PaintPrintPreviewBackground(nsIRenderingContext& aRenderingContext,
                                         nsPoint aPt)
{
  // fill page with White
  aRenderingContext.SetColor(NS_RGB(255,255,255));
  // REVIEW: this used to have rect's width and height be the
  // mClipRect if specialClipIsSet ... but that seems completely bogus
  // and inconsistent with the painting of the shadow below
  nsRect rect(aPt, GetSize());
  rect.width  -= mPD->mShadowSize.width;
  rect.height -= mPD->mShadowSize.height;
  aRenderingContext.FillRect(rect);
  // draw line around outside of page
  aRenderingContext.SetColor(NS_RGB(0,0,0));
  aRenderingContext.DrawRect(rect);

  if (mPD->mShadowSize.width > 0 && mPD->mShadowSize.height > 0) {
    aRenderingContext.SetColor(NS_RGB(51,51,51));
    nsRect r(aPt.x,aPt.y, mRect.width, mRect.height);
    nsRect shadowRect;
    shadowRect.x = r.x + r.width - mPD->mShadowSize.width;
    shadowRect.y = r.y + mPD->mShadowSize.height;
    shadowRect.width  = mPD->mShadowSize.width;
    shadowRect.height = r.height - mPD->mShadowSize.height;
    aRenderingContext.FillRect(shadowRect);

    shadowRect.x = r.x + mPD->mShadowSize.width;
    shadowRect.y = r.y + r.height - mPD->mShadowSize.height;
    shadowRect.width  = r.width - mPD->mShadowSize.width;
    shadowRect.height = mPD->mShadowSize.height;
    aRenderingContext.FillRect(shadowRect);
  }
}

void
nsPageFrame::PaintHeaderFooter(nsIRenderingContext& aRenderingContext,
                               nsPoint aPt)
{
  nsPresContext* pc = GetPresContext();

  if (!mPD->mPrintSettings) {
    if (pc->Type() == nsPresContext::eContext_PrintPreview || pc->IsDynamic())
      mPD->mPrintSettings = pc->GetPrintSettings();
    if (!mPD->mPrintSettings)
      return;
  }

  nsRect rect(aPt.x, aPt.y, mRect.width - mPD->mShadowSize.width,
              mRect.height - mPD->mShadowSize.height);

  aRenderingContext.SetFont(*mPD->mHeadFootFont, nsnull);
  aRenderingContext.SetColor(NS_RGB(0,0,0));

  // Get the FontMetrics to determine width.height of strings
  nsCOMPtr<nsIFontMetrics> fontMet;
  pc->DeviceContext()->GetMetricsFor(*mPD->mHeadFootFont, nsnull,
                                     *getter_AddRefs(fontMet));
  nscoord ascent = 0;
  nscoord visibleHeight = 0;
  if (fontMet) {
    fontMet->GetHeight(visibleHeight);
    fontMet->GetMaxAscent(ascent);
  }

  // print document headers and footers
  nsXPIDLString headerLeft, headerCenter, headerRight;
  mPD->mPrintSettings->GetHeaderStrLeft(getter_Copies(headerLeft));
  mPD->mPrintSettings->GetHeaderStrCenter(getter_Copies(headerCenter));
  mPD->mPrintSettings->GetHeaderStrRight(getter_Copies(headerRight));
  DrawHeaderFooter(aRenderingContext, eHeader,
                   headerLeft, headerCenter, headerRight,
                   rect, ascent, visibleHeight);

  nsXPIDLString footerLeft, footerCenter, footerRight;
  mPD->mPrintSettings->GetFooterStrLeft(getter_Copies(footerLeft));
  mPD->mPrintSettings->GetFooterStrCenter(getter_Copies(footerCenter));
  mPD->mPrintSettings->GetFooterStrRight(getter_Copies(footerRight));
  DrawHeaderFooter(aRenderingContext, eFooter,
                   footerLeft, footerCenter, footerRight,
                   rect, ascent, visibleHeight);
}

//------------------------------------------------------------------------------
void
nsPageFrame::PaintPageContent(nsIRenderingContext& aRenderingContext,
                              const nsRect&        aDirtyRect,
                              nsPoint              aPt) {
  nsIFrame* pageContentFrame  = mFrames.FirstChild();
  nsRect rect = aDirtyRect;
  float scale = GetPresContext()->GetPageScale();
  aRenderingContext.PushState();
  // Make sure we don't draw where we aren't supposed to draw, especially
  // when printing selection
  nsRect clipRect(nsPoint(0, 0), GetSize());
  clipRect.Deflate(mPD->mReflowMargin);
  aRenderingContext.SetClipRect(clipRect, nsClipCombine_kIntersect);
  // aPt translates to coords relative to this, then margins translate to
  // pageContentFrame's coords
  nsPoint framePos = aPt + pageContentFrame->GetOffsetTo(this);
  aRenderingContext.Translate(framePos.x, framePos.y);
  rect -= framePos;
  aRenderingContext.Scale(scale, scale);
  rect.ScaleRoundOut(1.0f / scale);

  const nsStyleBorder* border = GetStyleBorder();
  const nsStylePadding* padding = GetStylePadding();
  nsRect backgroundRect = nsRect(nsPoint(0, 0), pageContentFrame->GetSize());
  nsCSSRendering::PaintBackground(GetPresContext(), aRenderingContext, this,
                                  rect, backgroundRect, *border, *padding,
                                  PR_TRUE);

  nsLayoutUtils::PaintFrame(&aRenderingContext, pageContentFrame,
                            nsRegion(rect), NS_RGBA(0,0,0,0));

  aRenderingContext.PopState();
}

void
nsPageFrame::SetSharedPageData(nsSharedPageData* aPD) 
{ 
  mPD = aPD;
  // Set the shared data into the page frame before reflow
  nsPageContentFrame * pcf = NS_STATIC_CAST(nsPageContentFrame*, mFrames.FirstChild());
  if (pcf) {
    pcf->SetSharedPageData(mPD);
  }

}

nsIFrame*
NS_NewPageBreakFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  NS_PRECONDITION(aPresShell, "null PresShell");
  //check that we are only creating page break frames when printing
  NS_ASSERTION(aPresShell->GetPresContext()->IsPaginated(), "created a page break frame while not printing");

  return new (aPresShell) nsPageBreakFrame(aContext);
}

nsPageBreakFrame::nsPageBreakFrame(nsStyleContext* aContext) :
  nsLeafFrame(aContext), mHaveReflowed(PR_FALSE)
{
}

nsPageBreakFrame::~nsPageBreakFrame()
{
}

nscoord
nsPageBreakFrame::GetIntrinsicWidth()
{
  return nsPresContext::CSSPixelsToAppUnits(1);
}

nsresult 
nsPageBreakFrame::Reflow(nsPresContext*          aPresContext,
                         nsHTMLReflowMetrics&     aDesiredSize,
                         const nsHTMLReflowState& aReflowState,
                         nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsPageBreakFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  // Override reflow, since we don't want to deal with what our
  // computed values are.
  aDesiredSize.width = GetIntrinsicWidth();
  aDesiredSize.height = aReflowState.availableHeight;
  // round the height down to the nearest pixel
  aDesiredSize.height -=
    aDesiredSize.height % nsPresContext::CSSPixelsToAppUnits(1);

  // Note: not using NS_FRAME_FIRST_REFLOW here, since it's not clear whether
  // DidReflow will always get called before the next Reflow() call.
  mHaveReflowed = PR_TRUE;
  aStatus = NS_FRAME_COMPLETE; 
  return NS_OK;
}

nsIAtom*
nsPageBreakFrame::GetType() const
{
  return nsGkAtoms::pageBreakFrame; 
}


