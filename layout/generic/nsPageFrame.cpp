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
#include "nsPageFrame.h"
#include "nsHTMLParts.h"
#include "nsIContent.h"
#include "nsIPresContext.h"
#include "nsIStyleContext.h"
#include "nsIReflowCommand.h"
#include "nsIRenderingContext.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsLayoutAtoms.h"
#include "nsIStyleSet.h"
#include "nsIPresShell.h"
#include "nsIDeviceContext.h"

// for page number localization formatting
#include "nsTextFormatter.h"

// DateTime Includes
#include "nsDateTimeFormatCID.h"
#include "nsIDateTimeFormat.h"
#include "nsIServiceManager.h"
#include "nsILocale.h"
#include "nsLocaleCID.h"
#include "nsILocaleService.h"

static NS_DEFINE_CID(kDateTimeFormatCID, NS_DATETIMEFORMAT_CID);
static NS_DEFINE_CID(kLocaleServiceCID, NS_LOCALESERVICE_CID); 

// Temporary
#include "nsIFontMetrics.h"

// tstaic data members
PRUnichar * nsPageFrame::mPageNumFormat = nsnull;

nsresult
NS_NewPageFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsPageFrame* it = new (aPresShell) nsPageFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsPageFrame::nsPageFrame() :
  mHeadFootFont(nsnull)
{
}

nsPageFrame::~nsPageFrame()
{
  if (mHeadFootFont) 
    delete mHeadFootFont;

  if (mPageNumFormat) {
    nsMemory::Free(mPageNumFormat);
    mPageNumFormat = nsnull;
  }
}

NS_METHOD nsPageFrame::Reflow(nsIPresContext*          aPresContext,
                              nsHTMLReflowMetrics&     aDesiredSize,
                              const nsHTMLReflowState& aReflowState,
                              nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsPageFrame", aReflowState.reason);
  aStatus = NS_FRAME_COMPLETE;  // initialize out parameter

  if (eReflowReason_Incremental == aReflowState.reason) {
    // We don't expect the target of the reflow command to be page frame
#ifdef NS_DEUG
    NS_ASSERTION(nsnull != aReflowState.reflowCommand, "null reflow command");

    nsIFrame* target;
    aReflowState.reflowCommand->GetTarget(target);
    NS_ASSERTION(target != this, "page frame is reflow command target");
#endif
  
    // Verify the next reflow command frame is our one and only child frame
    nsIFrame* next;
    aReflowState.reflowCommand->GetNext(next);
    NS_ASSERTION(next == mFrames.FirstChild(), "bad reflow frame");

    // Dispatch the reflow command to our frame
    nsSize            maxSize(aReflowState.availableWidth, aReflowState.availableHeight);
    nsHTMLReflowState kidReflowState(aPresContext, aReflowState,
                                     mFrames.FirstChild(), maxSize);
  
    kidReflowState.isTopOfPage = PR_TRUE;
    ReflowChild(mFrames.FirstChild(), aPresContext, aDesiredSize,
                kidReflowState, 0, 0, 0, aStatus);
  
    // Place and size the child. Make sure the child is at least as
    // tall as our max size (the containing window)
    if (aDesiredSize.height < aReflowState.availableHeight) {
      aDesiredSize.height = aReflowState.availableHeight;
    }

    FinishReflowChild(mFrames.FirstChild(), aPresContext, aDesiredSize,
                      0, 0, 0);

  } else {
    // Do we have any children?
    // XXX We should use the overflow list instead...
    if (mFrames.IsEmpty() && (nsnull != mPrevInFlow)) {
      nsPageFrame*  prevPage = (nsPageFrame*)mPrevInFlow;

      nsIFrame* prevLastChild = prevPage->mFrames.LastChild();

      // Create a continuing child of the previous page's last child
      nsIPresShell* presShell;
      nsIStyleSet*  styleSet;
      nsIFrame*     newFrame;

      aPresContext->GetShell(&presShell);
      presShell->GetStyleSet(&styleSet);
      NS_RELEASE(presShell);
      styleSet->CreateContinuingFrame(aPresContext, prevLastChild, this, &newFrame);
      NS_RELEASE(styleSet);
      mFrames.SetFrames(newFrame);
    }

    // Resize our frame allowing it only to be as big as we are
    // XXX Pay attention to the page's border and padding...
    if (mFrames.NotEmpty()) {
      nsIFrame* frame = mFrames.FirstChild();
      nsSize  maxSize(aReflowState.availableWidth, aReflowState.availableHeight);
      nsHTMLReflowState kidReflowState(aPresContext, aReflowState, frame,
                                       maxSize);
      kidReflowState.isTopOfPage = PR_TRUE;

      // Get the child's desired size
      ReflowChild(frame, aPresContext, aDesiredSize, kidReflowState, 0, 0, 0, aStatus);

      // Make sure the child is at least as tall as our max size (the containing window)
      if (aDesiredSize.height < aReflowState.availableHeight) {
        aDesiredSize.height = aReflowState.availableHeight;
      }

      // Place and size the child
      FinishReflowChild(frame, aPresContext, aDesiredSize, 0, 0, 0);

      // Is the frame complete?
      if (NS_FRAME_IS_COMPLETE(aStatus)) {
        nsIFrame* childNextInFlow;

        frame->GetNextInFlow(&childNextInFlow);
        NS_ASSERTION(nsnull == childNextInFlow, "bad child flow list");
      }
    }

    // Return our desired size
    aDesiredSize.width = aReflowState.availableWidth;
    aDesiredSize.height = aReflowState.availableHeight;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPageFrame::GetFrameType(nsIAtom** aType) const
{
  NS_PRECONDITION(nsnull != aType, "null OUT parameter pointer");
  *aType = nsLayoutAtoms::pageFrame; 
  NS_ADDREF(*aType);
  return NS_OK;
}

#ifdef DEBUG
NS_IMETHODIMP
nsPageFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("Page", aResult);
}
#endif

NS_IMETHODIMP
nsPageFrame::IsPercentageBase(PRBool& aBase) const
{
  aBase = PR_TRUE;
  return NS_OK;
}

//------------------------------------------------------------------------------
nscoord nsPageFrame::GetXPosition(nsIRenderingContext& aRenderingContext, 
                                  const nsRect&        aRect, 
                                  PRInt32              aJust,
                                  const nsString&      aStr)
{
  PRInt32 width;
  aRenderingContext.GetWidth(aStr, width);

  nscoord x = aRect.x;
  switch (aJust) {
    case nsIPrintOptions::kJustLeft:
      // do nothing, already set
      break;

    case nsIPrintOptions::kJustCenter:
      x += (aRect.width - width) / 2;
      break;

    case nsIPrintOptions::kJustRight:
      x += aRect.width - width;
      break;
  } // switch

  return x;
}

//------------------------------------------------------------------------------
// Draw a Header or footer text lrft,right or center justified
// @parm aRenderingContext - rendering content ot draw into
// @parm aHeaderFooter - indicates whether it is a header or footer
// @parm aJust - indicates the justification of the text
// @parm aStr - The string to be drawn
// @parm aRect - the rect of the page
// @parm aHeight - the height of the text
// @parm aUseHalfThePage - indicates whether the text should limited to  the width
//                         of the entire page or just half the page
void
nsPageFrame::DrawHeaderFooter(nsIRenderingContext& aRenderingContext,
                              nsIFrame *           aFrame,
                              nsHeaderFooterEnum   aHeaderFooter,
                              PRInt32              aJust,
                              const nsString&      aStr,
                              const nsRect&        aRect,
                              nscoord              aHeight,
                              PRBool               aUseHalfThePage)
{
  // first make sure we have a vaild string and that the height of the
  // text will fit in the margin
  if (aStr.Length() > 0 && 
      ((aHeaderFooter == eHeader && aHeight < mMargin.top) ||
       (aHeaderFooter == eFooter && aHeight < mMargin.bottom))) {
    // measure the width of the text
    nsString str = aStr;
    PRInt32 width;
    aRenderingContext.GetWidth(str, width);
    PRBool addEllipse = PR_FALSE;
    nscoord halfWidth = aRect.width;
    if (aUseHalfThePage) {
      halfWidth /= 2;
    }
    // trim the text and add the elipses if it won't fit
    while (width >= halfWidth && str.Length() > 1) {
      str.SetLength(str.Length()-1);
      aRenderingContext.GetWidth(str, width);
      addEllipse = PR_TRUE;
    }
    if (addEllipse && str.Length() > 3) {
      str.SetLength(str.Length()-3);
      str.AppendWithConversion("...");
      aRenderingContext.GetWidth(str, width);
    }

    // cacl the x and y positions of the text
    nsRect rect(aRect);
    nscoord x = GetXPosition(aRenderingContext, rect, aJust, str);
    nscoord y;
    if (aHeaderFooter == eHeader) {
      nscoord offset = ((mMargin.top - aHeight) / 2);
      y = rect.y - offset - aHeight;
      rect.Inflate(0, offset + aHeight);
    } else {
      nscoord offset = ((mMargin.bottom - aHeight) / 2);
      y = rect.y + rect.height + offset;
      rect.height += offset + aHeight;
    }

    // set up new clip and draw the text
    PRBool clipEmpty;
    aRenderingContext.PushState();
    aRenderingContext.SetClipRect(rect, nsClipCombine_kReplace, clipEmpty);
    aRenderingContext.DrawString(str, x, y);
    aRenderingContext.PopState(clipEmpty);
  }
}

//------------------------------------------------------------------------------
NS_IMETHODIMP
nsPageFrame::Paint(nsIPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer)
{
  nsresult rv = nsContainerFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);

  if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {
    // get the current margin
    mPrintOptions->GetMarginInTwips(mMargin);

    nsRect  rect(0,0,mRect.width, mRect.height);

#if defined(DEBUG_rods) || defined(DEBUG_dcone)
    // XXX Paint a one-pixel border around the page so it's easy to see where
    // each page begins and ends when we're
    float   p2t;
    aPresContext->GetPixelsToTwips(&p2t);
    rect.Deflate(NSToCoordRound(p2t), NSToCoordRound(p2t));
    aRenderingContext.SetColor(NS_RGB(0, 0, 0));
    //aRenderingContext.DrawRect(rect);
    rect.Inflate(NSToCoordRound(p2t), NSToCoordRound(p2t));
    printf("SPSF::PaintChild -> Painting Frame %p Page No: %d\n", this, mPageNum);
#endif
    // use the whole page
    rect.width  += mMargin.left + mMargin.right;
    rect.x      -= mMargin.left;

    aRenderingContext.SetFont(*mHeadFootFont);
    aRenderingContext.SetColor(NS_RGB(0,0,0));

    // Get the FontMetrics to determine width.height of strings
    nsCOMPtr<nsIDeviceContext> deviceContext;
    aPresContext->GetDeviceContext(getter_AddRefs(deviceContext));
    NS_ASSERTION(deviceContext, "Couldn't get the device context"); 
    nsCOMPtr<nsIFontMetrics> fontMet;
    deviceContext->GetMetricsFor(*mHeadFootFont, *getter_AddRefs(fontMet));
    nscoord visibleHeight = 0;
    if (fontMet) {
      fontMet->GetHeight(visibleHeight);
    }

    // get the print options bits so we can figure out all the 
    // the extra pieces of text that need to be drawn
    PRInt32 printOptBits;
    mPrintOptions->GetPrintOptionsBits(&printOptBits);

    // print page numbers
    if (printOptBits & nsIPrintOptions::kOptPrintPageNums && mPageNumFormat != nsnull) {
      PRInt16 justify = nsIPrintOptions::kJustLeft;
      mPrintOptions->GetPageNumJust(&justify);

      PRUnichar *  valStr;
      // print page number totals "x of x"
      if (printOptBits & nsIPrintOptions::kOptPrintPageTotal) {
        valStr = nsTextFormatter::smprintf(mPageNumFormat, mPageNum, mTotNumPages);
      } else {
        valStr = nsTextFormatter::smprintf(mPageNumFormat, mPageNum);
      }
      nsAutoString pageNoStr(valStr);
      nsMemory::Free(valStr);
      DrawHeaderFooter(aRenderingContext, this, eFooter, justify, pageNoStr, rect, visibleHeight);
    }

    // print localized date
    if (printOptBits & nsIPrintOptions::kOptPrintDatePrinted) {
      // Get Locale for Formating DateTime
      nsCOMPtr<nsILocale> locale; 
      NS_WITH_SERVICE(nsILocaleService, localeSvc, kLocaleServiceCID, &rv);
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
            rv = dateTime->FormatTMTime(locale, kDateFormatShort, kTimeFormatNoSeconds, localtime( &ltime ), dateString);
            if (NS_SUCCEEDED(rv)) {
              DrawHeaderFooter(aRenderingContext, this, eFooter, nsIPrintOptions::kJustRight, dateString, rect, visibleHeight);
            }
          }
        }
      }
    }


    PRBool usingHalfThePage = (printOptBits & nsIPrintOptions::kOptPrintDocTitle) && 
                              (printOptBits & nsIPrintOptions::kOptPrintDocLoc);
        
    // print document title
    PRUnichar * title;
    mPrintOptions->GetTitle(&title); // creates memory
    if (title != nsnull && (printOptBits & nsIPrintOptions::kOptPrintDocTitle)) {
      DrawHeaderFooter(aRenderingContext, this, eHeader, nsIPrintOptions::kJustLeft, nsAutoString(title), rect, visibleHeight, usingHalfThePage);
      nsMemory::Free(title);
    }

    // print document URL
    PRUnichar * url;
    mPrintOptions->GetDocURL(&url);
    if (title != url && (printOptBits & nsIPrintOptions::kOptPrintDocLoc)) {
      DrawHeaderFooter(aRenderingContext, this, eHeader, nsIPrintOptions::kJustRight, nsAutoString(url), rect, visibleHeight, usingHalfThePage);
      nsMemory::Free(url);
    }

  }

  return rv;
}

//------------------------------------------------------------------------------
void
nsPageFrame::SetPrintOptions(nsIPrintOptions * aPrintOptions) 
{ 
  NS_ASSERTION(aPrintOptions != nsnull, "Print Options can not be null!");

  mPrintOptions = aPrintOptions;
  // create a default font
  mHeadFootFont = new nsFont("serif", NS_FONT_STYLE_NORMAL,NS_FONT_VARIANT_NORMAL,
                             NS_FONT_WEIGHT_NORMAL,0,NSIntPointsToTwips(10));
  // now get the default font form the print options
  mPrintOptions->GetDefaultFont(*mHeadFootFont);
}

//------------------------------------------------------------------------------
void
nsPageFrame::SetPageNumInfo(PRInt32 aPageNumber, PRInt32 aTotalPages) 
{ 
  mPageNum     = aPageNumber; 
  mTotNumPages = aTotalPages; 
}


//------------------------------------------------------------------------------
void
nsPageFrame::SetPageNumberFormat(PRUnichar * aFormatStr)
{ 
  NS_ASSERTION(aFormatStr != nsnull, "Format string cannot be null!");

  if (mPageNumFormat != nsnull) {
    nsMemory::Free(mPageNumFormat);
  }
  mPageNumFormat = aFormatStr;
}
