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
#include "nsHTMLParts.h"
#include "nsIContent.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "nsIRenderingContext.h"
#include "nsHTMLAtoms.h"
#include "nsLayoutAtoms.h"
#include "nsIPresShell.h"
#include "nsCSSFrameConstructor.h"
#include "nsIDeviceContext.h"
#include "nsReadableUtils.h"
#include "nsPageContentFrame.h"
#include "nsTextFrame.h" // for function BinarySearchForPosition

#include "nsIView.h" // view flags for clipping
#include "nsCSSRendering.h"

#include "nsHTMLContainerFrame.h" // view creation

#include "nsSimplePageSequence.h" // for nsSharedPageData
#include "nsRegion.h"
#include "nsIViewManager.h"

// for page number localization formatting
#include "nsTextFormatter.h"

#ifdef IBMBIDI
#include "nsBidiUtils.h"
#include "nsBidiPresUtils.h"
#endif

// Temporary
#include "nsIFontMetrics.h"

// Print Options
#include "nsIPrintSettings.h"
#include "nsGfxCIID.h"
#include "nsIServiceManager.h"

// Widget Creation
#include "nsIWidget.h"
#include "nsWidgetsCID.h"
static NS_DEFINE_CID(kCChildCID, NS_CHILD_CID);

#if defined(DEBUG_rods) || defined(DEBUG_dcone)
//#define DEBUG_PRINTING
#endif

#include "prlog.h"
#ifdef PR_LOGGING 
extern PRLogModuleInfo * kLayoutPrintingLogMod;
#define PR_PL(_p1)  PR_LOG(kLayoutPrintingLogMod, PR_LOG_DEBUG, _p1)
#else
#define PR_PL(_p1)
#endif

// XXX Part of Temporary fix for Bug 127263
PRBool nsPageFrame::mDoCreateWidget = PR_TRUE;

nsresult
NS_NewPageFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");

  nsPageFrame* it = new (aPresShell) nsPageFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsPageFrame::nsPageFrame() :
  mSupressHF(PR_FALSE),
  mClipRect(-1, -1, -1, -1)
{
}

nsPageFrame::~nsPageFrame()
{
}


NS_IMETHODIMP
nsPageFrame::SetInitialChildList(nsPresContext* aPresContext,
                                      nsIAtom*        aListName,
                                      nsIFrame*       aChildList)
{
  nsIView* view = aChildList->GetView();
  if (view && mDoCreateWidget) {
    if (aPresContext->Type() == nsPresContext::eContext_PrintPreview &&
        view->GetNearestWidget(nsnull)) {
      view->CreateWidget(kCChildCID);  
    }
  }

  return nsContainerFrame::SetInitialChildList(aPresContext, aListName, aChildList);
}

NS_IMETHODIMP nsPageFrame::Reflow(nsPresContext*          aPresContext,
                                  nsHTMLReflowMetrics&     aDesiredSize,
                                  const nsHTMLReflowState& aReflowState,
                                  nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsPageFrame", aReflowState.reason);
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
  aStatus = NS_FRAME_COMPLETE;  // initialize out parameter

  if (eReflowReason_Incremental != aReflowState.reason) {
    // Do we have any children?
    // XXX We should use the overflow list instead...
    nsIFrame*           firstFrame  = mFrames.FirstChild();
    nsPageContentFrame* contentPage = NS_STATIC_CAST(nsPageContentFrame*, firstFrame);
    NS_ASSERTION(contentPage, "There should always be a content page");
    NS_ASSERTION(nsLayoutAtoms::pageContentFrame == firstFrame->GetType(),
                 "This frame isn't a pageContentFrame");

    if (contentPage && mPrevInFlow) {
      nsPageFrame*        prevPage        = NS_STATIC_CAST(nsPageFrame*, mPrevInFlow);
      nsPageContentFrame* prevContentPage = NS_STATIC_CAST(nsPageContentFrame*, prevPage->mFrames.FirstChild());
      nsIFrame*           prevLastChild   = prevContentPage->mFrames.LastChild();

      // Create a continuing child of the previous page's last child
      nsIFrame*     newFrame;

      aPresContext->PresShell()->FrameConstructor()->
        CreateContinuingFrame(aPresContext, prevLastChild,
                              contentPage, &newFrame);

      // Make the new area frame the 1st child of the page content frame. There may already be
      // children placeholders which don't get reflowed but must not be destroyed until the 
      // page content frame is destroyed.
      contentPage->mFrames.InsertFrame(contentPage, nsnull, newFrame);
    }

    // Resize our frame allowing it only to be as big as we are
    // XXX Pay attention to the page's border and padding...
    if (mFrames.NotEmpty()) {
      nsIFrame* frame = mFrames.FirstChild();
      // When availableHeight is NS_UNCONSTRAINEDSIZE it means we are reflowing a single page
      // to print selection. So this means we want to use NS_UNCONSTRAINEDSIZE without altering it
      nscoord avHeight;
      if (aReflowState.availableHeight == NS_UNCONSTRAINEDSIZE) {
        avHeight = NS_UNCONSTRAINEDSIZE;
      } else {
        avHeight = mPD->mReflowRect.height - mPD->mReflowMargin.top - mPD->mReflowMargin.bottom;
      }
      nsSize  maxSize(mPD->mReflowRect.width - mPD->mReflowMargin.right - mPD->mReflowMargin.left, 
                      avHeight);
      // Get the number of Twips per pixel from the PresContext
      nscoord onePixelInTwips = aPresContext->IntScaledPixelsToTwips(1);
      NS_ASSERTION(maxSize.width >= onePixelInTwips, "maxSize.width must be >= 1 pixel");
      NS_ASSERTION(maxSize.height >= onePixelInTwips, "maxSize.height must be >= 1 pixel");
      // insurance against infinite reflow, when reflowing less than a pixel
      if (maxSize.width < onePixelInTwips || maxSize.height < onePixelInTwips) {
        aDesiredSize.width  = 0;
        aDesiredSize.height = 0;
        return NS_OK;
      }

      nsHTMLReflowState kidReflowState(aPresContext, aReflowState, frame, maxSize);
      kidReflowState.mFlags.mIsTopOfPage = PR_TRUE;

      // calc location of frame
      nscoord xc = mPD->mReflowMargin.left + mPD->mDeadSpaceMargin.left + mPD->mExtraMargin.left;
      nscoord yc = mPD->mReflowMargin.top + mPD->mDeadSpaceMargin.top + mPD->mExtraMargin.top;

      // Get the child's desired size
      ReflowChild(frame, aPresContext, aDesiredSize, kidReflowState, xc, yc, 0, aStatus);


      // Place and size the child
      FinishReflowChild(frame, aPresContext, &kidReflowState, aDesiredSize, xc, yc, 0);

      // Make sure the child is at least as tall as our max size (the containing window)
      if (aDesiredSize.height < aReflowState.availableHeight &&
          aReflowState.availableHeight != NS_UNCONSTRAINEDSIZE) {
        aDesiredSize.height = aReflowState.availableHeight;
      }

      nsIView* view = frame->GetView();
      if (view) {
        nsRegion region(nsRect(0, 0, aDesiredSize.width, aDesiredSize.height));
        view->GetViewManager()->SetViewChildClipRegion(view, &region);
      }

#ifdef NS_DEBUG
      // Is the frame complete?
      if (NS_FRAME_IS_COMPLETE(aStatus)) {
        nsIFrame* childNextInFlow;

        frame->GetNextInFlow(&childNextInFlow);
        NS_ASSERTION(nsnull == childNextInFlow, "bad child flow list");
      }
#endif
    }
    PR_PL(("PageFrame::Reflow %p ", this));
    PR_PL(("[%d,%d][%d,%d]\n", aDesiredSize.width, aDesiredSize.height, aReflowState.availableWidth, aReflowState.availableHeight));

    // Return our desired size
    aDesiredSize.width = aReflowState.availableWidth;
    if (aReflowState.availableHeight != NS_UNCONSTRAINEDSIZE) {
      aDesiredSize.height = aReflowState.availableHeight;
    }
  }
  PR_PL(("PageFrame::Reflow %p ", this));
  PR_PL(("[%d,%d]\n", aReflowState.availableWidth, aReflowState.availableHeight));

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return NS_OK;
}

void nsPageFrame::SetClipRect(nsRect* aClipRect) 
{
  mClipRect = *aClipRect; 
  nsIFrame*           firstFrame  = mFrames.FirstChild();
  nsPageContentFrame* contentPage = NS_STATIC_CAST(nsPageContentFrame*, firstFrame);
  NS_ASSERTION(contentPage, "There should always be a content page");
  contentPage->SetClipRect(aClipRect);
}


nsIAtom*
nsPageFrame::GetType() const
{
  return nsLayoutAtoms::pageFrame; 
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

// Remove fix below when string gets fixed
#define WORKAROUND_FOR_BUG_110335
// replace the &<code> with the value, but if the value is empty
// set the string to zero length
static void
SubstValueForCode(nsString& aStr, const PRUnichar * aUKey, const PRUnichar * aUStr)
{
#ifdef WORKAROUND_FOR_BUG_110335
  const PRUnichar* uKeyStr = aUKey;

  // Check to make sure our subst code &<code> isn't in the data string
  // for example &T for title is in QB&T
  nsAutoString dataStr(aUStr);
  nsAutoString newKey(aUKey);
  PRBool fixingSubstr = (dataStr.Find(newKey) != kNotFound);
  if (fixingSubstr) {
    // well, the code is in the data str so make up a new code
    // but make sure it it isn't in either substs string or the data string
    char* code = "~!@#$%^*()_+=][}{`';:|?/.,:\"<>";
    PRInt32 codeInx = 0;
    PRInt32 codeLen = PRUint32(strlen(code));
    while ((dataStr.Find(newKey) > -1 || aStr.Find(newKey) > -1) && codeInx < codeLen) {
      newKey.SetCharAt((PRUnichar)code[codeInx++], 0);
    }

    // Check to see if we can do the substitution
    if (codeInx == codeLen) {
      aStr.SetLength(0);
      return; // bail we just can't do it
    }

    // Ok, we have the new code, so repplace the old code 
    // in the dest str with the new code
    aStr.ReplaceSubstring(aUKey, newKey.get());
    uKeyStr = ToNewUnicode(newKey);
  }

  if (!aUStr || !*aUStr) {
    aStr.SetLength(0);
  } else {
    aStr.ReplaceSubstring(uKeyStr, aUStr);
  }

  // Free uKeyStr only if we fixed the string.
  if (fixingSubstr) {
    nsMemory::Free(NS_CONST_CAST(PRUnichar*, uKeyStr));
  }
#else

  if (!aUStr || !*aUStr) {
    aStr.SetLength(0);
  } else {
    aStr.ReplaceSubstring(aUKey, aUStr);
  }
#endif  // WORKAROUND_FOR_BUG_110335
}
// done with static helper functions
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
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
    return;
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
    return;
  }

  // Search to see if the page number code is in the string
  // and replace the page number code with the actual value
  NS_NAMED_LITERAL_STRING(kPage, "&P");
  if (aStr.Find(kPage) != kNotFound) {
    PRUnichar * uStr = nsTextFormatter::smprintf(mPD->mPageNumFormat, mPageNum);
    aNewStr.ReplaceSubstring(kPage.get(), uStr);
    nsMemory::Free(uStr);
    return;
  }

  NS_NAMED_LITERAL_STRING(kTitle, "&T");
  if (aStr.Find(kTitle) != kNotFound) {
    SubstValueForCode(aNewStr, kTitle.get(), mPD->mDocTitle);
    return;
  }

  NS_NAMED_LITERAL_STRING(kDocURL, "&U");
  if (aStr.Find(kDocURL) != kNotFound) {
    SubstValueForCode(aNewStr, kDocURL.get(), mPD->mDocURL);
    return;
  }
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

  NS_ASSERTION(x >= 0, "x can't be less than zero");
  x = PR_MAX(x, 0);
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
nsPageFrame::DrawHeaderFooter(nsPresContext*      aPresContext,
                              nsIRenderingContext& aRenderingContext,
                              nsIFrame *           aFrame,
                              nsHeaderFooterEnum   aHeaderFooter,
                              PRInt32              aJust,
                              const nsString&      aStr1,
                              const nsString&      aStr2,
                              const nsString&      aStr3,
                              const nsRect&        aRect,
                              nscoord              aAscent,
                              nscoord              aHeight)
{
  PRInt32 numStrs = 0;
  if (!aStr1.IsEmpty()) numStrs++;
  if (!aStr2.IsEmpty()) numStrs++;
  if (!aStr3.IsEmpty()) numStrs++;

  if (numStrs == 0) return;
  nscoord strSpace = aRect.width / numStrs;

  if (!aStr1.IsEmpty()) {
    DrawHeaderFooter(aPresContext, aRenderingContext, aFrame, aHeaderFooter, nsIPrintSettings::kJustLeft, aStr1, aRect, aAscent, aHeight, strSpace);
  }
  if (!aStr2.IsEmpty()) {
    DrawHeaderFooter(aPresContext, aRenderingContext, aFrame, aHeaderFooter, nsIPrintSettings::kJustCenter, aStr2, aRect, aAscent, aHeight, strSpace);
  }
  if (!aStr3.IsEmpty()) {
    DrawHeaderFooter(aPresContext, aRenderingContext, aFrame, aHeaderFooter, nsIPrintSettings::kJustRight, aStr3, aRect, aAscent, aHeight, strSpace);
  }
}

//------------------------------------------------------------------------------
// Draw a Header or footer text lrft,right or center justified
// @parm aRenderingContext - rendering content ot draw into
// @parm aHeaderFooter - indicates whether it is a header or footer
// @parm aJust - indicates the justification of the text
// @parm aStr - The string to be drawn
// @parm aRect - the rect of the page
// @parm aHeight - the height of the text
// @parm aWidth - available width for any one of the strings
void
nsPageFrame::DrawHeaderFooter(nsPresContext*      aPresContext,
                              nsIRenderingContext& aRenderingContext,
                              nsIFrame *           aFrame,
                              nsHeaderFooterEnum   aHeaderFooter,
                              PRInt32              aJust,
                              const nsString&      aStr,
                              const nsRect&        aRect,
                              nscoord              aAscent,
                              nscoord              aHeight,
                              nscoord              aWidth)
{

  nscoord contentWidth = aWidth - (mPD->mEdgePaperMargin.left + mPD->mEdgePaperMargin.right);

  // first make sure we have a vaild string and that the height of the
  // text will fit in the margin
  if (!aStr.IsEmpty() && 
      ((aHeaderFooter == eHeader && aHeight < mMargin.top) ||
       (aHeaderFooter == eFooter && aHeight < mMargin.bottom))) {
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
    if (BinarySearchForPosition(&aRenderingContext, text, 0, 0, 0, len,
                                PRInt32(contentWidth), indx, textWidth)) {
      if (indx < len-1 && len > 3) {
        str.SetLength(indx-3);
        str.AppendLiteral("...");
      }
    } else { 
      return; // bail if couldn't find the correct length
    }

    // cacl the x and y positions of the text
    nsRect rect(aRect);
    nscoord x = GetXPosition(aRenderingContext, rect, aJust, str);
    nscoord y;
    if (aHeaderFooter == eHeader) {
      y = rect.y + mPD->mExtraMargin.top + mPD->mEdgePaperMargin.top;
    } else {
      y = rect.y + rect.height - aHeight - mPD->mExtraMargin.bottom - mPD->mEdgePaperMargin.bottom;
    }

    // set up new clip and draw the text
    aRenderingContext.PushState();
    aRenderingContext.SetColor(NS_RGB(0,0,0));
    aRenderingContext.SetClipRect(rect, nsClipCombine_kReplace);
#ifdef IBMBIDI
    nsresult rv = NS_ERROR_FAILURE;

    if (aPresContext->BidiEnabled()) {
      nsBidiPresUtils* bidiUtils = aPresContext->GetBidiUtils();
      
      if (bidiUtils) {
        PRUnichar* buffer = str.BeginWriting();
        // Base direction is always LTR for now. If bug 139337 is fixed, 
        // that should change.
        rv = bidiUtils->RenderText(buffer, str.Length(), NSBIDI_LTR,
                                   aPresContext, aRenderingContext,
                                   x, y + aAscent);
      }
    }
    if (NS_FAILED(rv))
#endif // IBMBIDI
    aRenderingContext.DrawString(str, x, y + aAscent);
    aRenderingContext.PopState();

#ifdef DEBUG_PRINTING
    PR_PL(("Page: %p", this));
    PR_PL((" [%s]", NS_ConvertUCS2toUTF8(str).get()));
    char justStr[64];
    switch (aJust) {
      case nsIPrintSettings::kJustLeft:strcpy(justStr, "Left");break;
      case nsIPrintSettings::kJustCenter:strcpy(justStr, "Center");break;
      case nsIPrintSettings::kJustRight:strcpy(justStr, "Right");break;
    } // switch
    PR_PL((" HF: %s ", aHeaderFooter==eHeader?"Header":"Footer"));
    PR_PL((" JST: %s ", justStr));
    PR_PL((" x,y: %d,%d", x, y));
    PR_PL((" Hgt: %d \n", aHeight));
#endif
  }
}

//------------------------------------------------------------------------------
NS_IMETHODIMP
nsPageFrame::Paint(nsPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer,
                   PRUint32             aFlags)
{
  aRenderingContext.PushState();
  aRenderingContext.SetColor(NS_RGB(255,255,255));

  nsRect rect;
  PRBool specialClipIsSet = mClipRect.width != -1 || mClipRect.height != -1;

  if (specialClipIsSet) {
#ifdef DEBUG_PRINTING
    if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {
      printf("*** ClipRect: %5d,%5d,%5d,%5d\n", mClipRect.x, mClipRect.y, mClipRect.width, mClipRect.height);
    }
#endif
    aRenderingContext.SetClipRect(mClipRect, nsClipCombine_kReplace);
    rect = mClipRect;
  } else {
    rect = mRect;
  }

  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {

    if (aPresContext->Type() == nsPresContext::eContext_PrintPreview) {
      // fill page with White
      aRenderingContext.SetColor(NS_RGB(255,255,255));
      rect.x = 0;
      rect.y = 0;
      rect.width  -= mPD->mShadowSize.width;
      rect.height -= mPD->mShadowSize.height;
      aRenderingContext.FillRect(rect);
      // draw line around outside of page
      aRenderingContext.SetColor(NS_RGB(0,0,0));
      aRenderingContext.DrawRect(rect);

      if (mPD->mShadowSize.width > 0 && mPD->mShadowSize.height > 0) {
        aRenderingContext.SetColor(NS_RGB(51,51,51));
        nsRect r(0,0, mRect.width, mRect.height);
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
  }

  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
    DrawBackground(aPresContext,aRenderingContext,aDirtyRect);
  }

  nsresult rv = nsContainerFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);

#if defined(DEBUG_rods) || defined(DEBUG_dcone)
  if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {
    PR_PL(("PF::Paint    -> %p  SupHF: %s  Rect: [%5d,%5d,%5d,%5d] SC:%s\n", this, 
            mSupressHF?"Yes":"No", mRect.x, mRect.y, mRect.width, mRect.height, specialClipIsSet?"Yes":"No"));
    PR_PL(("PF::Paint    -> %p  SupHF: %s  Rect: [%5d,%5d,%5d,%5d] SC:%s\n", this, 
            mSupressHF?"Yes":"No", mRect.x, mRect.y, mRect.width, mRect.height, specialClipIsSet?"Yes":"No"));
  }
#endif

  if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer && !mSupressHF) {
    // For PrintPreview the 
    if (!mPD->mPrintSettings) {
      if (aPresContext->Type() == nsPresContext::eContext_PrintPreview) {
        mPD->mPrintSettings = aPresContext->GetPrintSettings();
      }
    }
    NS_ASSERTION(mPD->mPrintSettings, "Must have a good PrintSettings here!");

    // get the current margin
    mPD->mPrintSettings->GetMarginInTwips(mMargin);

    rect.SetRect(0, 0, mRect.width - mPD->mShadowSize.width, mRect.height - mPD->mShadowSize.height);

    aRenderingContext.SetFont(*mPD->mHeadFootFont, nsnull);
    aRenderingContext.SetColor(NS_RGB(0,0,0));

    // Get the FontMetrics to determine width.height of strings
    nsCOMPtr<nsIFontMetrics> fontMet;
    aPresContext->DeviceContext()->GetMetricsFor(*mPD->mHeadFootFont, nsnull,
                                                 *getter_AddRefs(fontMet));
    nscoord ascent = 0;
    nscoord visibleHeight = 0;
    if (fontMet) {
      fontMet->GetHeight(visibleHeight);
      fontMet->GetMaxAscent(ascent);
    }

    // print document headers and footers
    PRUnichar * headers[3];
    mPD->mPrintSettings->GetHeaderStrLeft(&headers[0]);   // creates memory
    mPD->mPrintSettings->GetHeaderStrCenter(&headers[1]); // creates memory
    mPD->mPrintSettings->GetHeaderStrRight(&headers[2]);  // creates memory
    DrawHeaderFooter(aPresContext, aRenderingContext, this, eHeader, nsIPrintSettings::kJustLeft, 
                     nsAutoString(headers[0]), nsAutoString(headers[1]), nsAutoString(headers[2]), 
                     rect, ascent, visibleHeight);
    PRInt32 i;
    for (i=0;i<3;i++) nsMemory::Free(headers[i]);

    PRUnichar * footers[3];
    mPD->mPrintSettings->GetFooterStrLeft(&footers[0]);   // creates memory
    mPD->mPrintSettings->GetFooterStrCenter(&footers[1]); // creates memory
    mPD->mPrintSettings->GetFooterStrRight(&footers[2]);  // creates memory
    DrawHeaderFooter(aPresContext, aRenderingContext, this, eFooter, nsIPrintSettings::kJustRight, 
                     nsAutoString(footers[0]), nsAutoString(footers[1]), nsAutoString(footers[2]), 
                     rect, ascent, visibleHeight);
    for (i=0;i<3;i++) nsMemory::Free(footers[i]);

  }

  aRenderingContext.PopState();

  return rv;
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
nsPageFrame::DrawBackground(nsPresContext*      aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect&        aDirtyRect) 
{
  nsSimplePageSequenceFrame* seqFrame = NS_STATIC_CAST(nsSimplePageSequenceFrame*, mParent);
  if (seqFrame != nsnull) {
    nsIFrame* pageContentFrame  = mFrames.FirstChild();
    NS_ASSERTION(pageContentFrame, "Must always be there.");

    const nsStyleBorder* border = GetStyleBorder();
    const nsStylePadding* padding = GetStylePadding();

    nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                    aDirtyRect, pageContentFrame->GetRect(), *border, *padding,
                                    PR_TRUE);
  }
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

nsresult
NS_NewPageBreakFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aPresShell && aNewFrame, "null PresShell or OUT ptr");
#ifdef DEBUG
  //check that we are only creating page break frames when printing
  nsCOMPtr<nsPresContext> presContext;    
  aPresShell->GetPresContext(getter_AddRefs(presContext));
  NS_ASSERTION(presContext->IsPaginated(), "created a page break frame while not printing");
#endif

  nsPageBreakFrame* it = new (aPresShell) nsPageBreakFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsPageBreakFrame::nsPageBreakFrame()
: mHaveReflowed(PR_FALSE)
{
}

nsPageBreakFrame::~nsPageBreakFrame()
{
}

void 
nsPageBreakFrame::GetDesiredSize(nsPresContext*          aPresContext,
                                 const nsHTMLReflowState& aReflowState,
                                 nsHTMLReflowMetrics&     aDesiredSize)
{
  NS_PRECONDITION(aPresContext, "null pres context");
  nscoord onePixel = aPresContext->IntScaledPixelsToTwips(1);

  aDesiredSize.width  = onePixel;
  if (mHaveReflowed) {
    // If blocks reflow us a 2nd time trying to put us on a new page, then return
    // a desired height of 0 to avoid an extra page break. 
    aDesiredSize.height = 0;
  }
  else {
    aDesiredSize.height = aReflowState.availableHeight;
    // round the height down to the nearest pixel
    aDesiredSize.height -= aDesiredSize.height % onePixel;
  }

  if (aDesiredSize.mComputeMEW) {
    aDesiredSize.mMaxElementWidth  = onePixel;
  }
  aDesiredSize.ascent  = 0;
  aDesiredSize.descent = 0;
}

nsresult 
nsPageBreakFrame::Reflow(nsPresContext*          aPresContext,
                         nsHTMLReflowMetrics&     aDesiredSize,
                         const nsHTMLReflowState& aReflowState,
                         nsReflowStatus&          aStatus)
{
  NS_PRECONDITION(aPresContext, "null pres context");
  DO_GLOBAL_REFLOW_COUNT("nsTableFrame", aReflowState.reason);
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  aStatus = NS_FRAME_COMPLETE; 
  GetDesiredSize(aPresContext, aReflowState, aDesiredSize);
  mHaveReflowed = PR_TRUE;

  return NS_OK;
}

nsIAtom*
nsPageBreakFrame::GetType() const
{
  return nsLayoutAtoms::pageBreakFrame; 
}


