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
#ifndef nsSimplePageSequence_h___
#define nsSimplePageSequence_h___

#include "nsIPageSequenceFrame.h"
#include "nsContainerFrame.h"
#include "nsIPrintSettings.h"
#include "nsIPrintOptions.h"
#include "nsIDateTimeFormat.h"

//-----------------------------------------------
// This class maintains all the data that 
// is used by all the page frame
// It lives while the nsSimplePageSequenceFrame lives
class nsSharedPageData {
public:
  nsSharedPageData();
  ~nsSharedPageData();

  PRUnichar * mDateTimeStr;
  nsFont *    mHeadFootFont;
  PRUnichar * mPageNumFormat;
  PRUnichar * mPageNumAndTotalsFormat;
  PRUnichar * mDocTitle;
  PRUnichar * mDocURL;

  nsRect      mReflowRect;
  nsMargin    mReflowMargin;
  nsSize      mShadowSize;       // shadow of page in PrintPreview
  nsMargin    mDeadSpaceMargin;  // Extra dead space around outside of Page in PrintPreview
  nsMargin    mExtraMargin;      // Extra Margin between the printable area and the edge of the page
  nsMargin    mEdgePaperMargin;  // In twips, gap between the Margin and edge of page

  nsCOMPtr<nsIPrintSettings> mPrintSettings;
  nsCOMPtr<nsIPrintOptions> mPrintOptions;

  nscoord      mPageContentXMost;      // xmost size from Reflow(width)
  nscoord      mPageContentSize;       // constrained size (width)
};

// Simple page sequence frame class. Used when we're in paginated mode
class nsSimplePageSequenceFrame : public nsContainerFrame,
                                  public nsIPageSequenceFrame {
public:
  friend nsresult NS_NewSimplePageSequenceFrame(nsIPresShell* aPresShell, nsIFrame** aResult);

  // nsISupports
  NS_IMETHOD  QueryInterface(const nsIID& aIID, void** aInstancePtr);

  // nsIFrameReflow
  NS_IMETHOD  Reflow(nsPresContext*      aPresContext,
                     nsHTMLReflowMetrics& aDesiredSize,
                     const nsHTMLReflowState& aMaxSize,
                     nsReflowStatus&      aStatus);

  NS_IMETHOD  Paint(nsPresContext*      aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect&        aDirtyRect,
                    nsFramePaintLayer    aWhichLayer);

  // nsIPageSequenceFrame
  NS_IMETHOD SetOffsets(nscoord aStartOffset, nscoord aEndOffset);
  NS_IMETHOD SetPageNo(PRInt32 aPageNo) { return NS_OK;}
  NS_IMETHOD SetSelectionHeight(nscoord aYOffset, nscoord aHeight) { mYSelOffset = aYOffset; mSelectionHeight = aHeight; return NS_OK; }
  NS_IMETHOD SetTotalNumPages(PRInt32 aTotal) { mTotalPages = aTotal; return NS_OK; }

  // Gets the dead space (the gray area) around the Print Preview Page
  NS_IMETHOD GetDeadSpaceValue(nscoord* aValue) { *aValue = NS_INCHES_TO_TWIPS(0.25); return NS_OK; };
  
  // For Shrink To Fit
  NS_IMETHOD GetSTFPercent(float& aSTFPercent);

  // Async Printing
  NS_IMETHOD StartPrint(nsPresContext*  aPresContext,
                        nsIPrintSettings* aPrintSettings,
                        PRUnichar*        aDocTitle,
                        PRUnichar*        aDocURL);
  NS_IMETHOD PrintNextPage(nsPresContext*  aPresContext);
  NS_IMETHOD GetCurrentPageNum(PRInt32* aPageNum);
  NS_IMETHOD GetNumPages(PRInt32* aNumPages);
  NS_IMETHOD IsDoingPrintRange(PRBool* aDoing);
  NS_IMETHOD GetPrintRange(PRInt32* aFromPage, PRInt32* aToPage);
  NS_IMETHOD SkipPageBegin() { mSkipPageBegin = PR_TRUE; return NS_OK; }
  NS_IMETHOD SkipPageEnd() { mSkipPageEnd = PR_TRUE; return NS_OK; }
  NS_IMETHOD DoPageEnd(nsPresContext*  aPresContext);
  NS_IMETHOD GetPrintThisPage(PRBool*  aPrintThisPage) { *aPrintThisPage = mPrintThisPage; return NS_OK; }
  NS_IMETHOD SetOffset(nscoord aX, nscoord aY) { mOffsetX = aX; mOffsetY = aY; return NS_OK; }
  NS_IMETHOD SuppressHeadersAndFooters(PRBool aDoSup);
  NS_IMETHOD SetClipRect(nsPresContext* aPresContext, nsRect* aSize);

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::sequenceFrame
   */
  virtual nsIAtom* GetType() const;
  
#ifdef NS_DEBUG
  NS_IMETHOD  GetFrameName(nsAString& aResult) const;
#endif


protected:
  nsSimplePageSequenceFrame();
  virtual ~nsSimplePageSequenceFrame();

  nsresult CreateContinuingPageFrame(nsPresContext* aPresContext,
                                     nsIFrame*       aPageFrame,
                                     nsIFrame**      aContinuingFrame);

  void SetPageNumberFormat(const char* aPropName, const char* aDefPropVal, PRBool aPageNumOnly);

  // SharedPageData Helper methods
  void SetDateTimeStr(PRUnichar * aDateTimeStr);
  void SetPageNumberFormat(PRUnichar * aFormatStr, PRBool aForPageNumOnly);
  void SetPageSizes(const nsRect& aRect, const nsMargin& aMarginRect);

  void GetEdgePaperMarginCoord(char* aPrefName, nscoord& aCoord);
  void GetEdgePaperMargin(nsMargin& aMargin);

  NS_IMETHOD_(nsrefcnt) AddRef(void) {return nsContainerFrame::AddRef();}
  NS_IMETHOD_(nsrefcnt) Release(void) {return nsContainerFrame::Release();}


  nscoord  mStartOffset;
  nscoord  mEndOffset;

  nsMargin mMargin;
  PRBool   mIsPrintingSelection;

  // Asynch Printing
  PRInt32      mPageNum;
  PRInt32      mTotalPages;
  PRInt32      mPrintedPageNum;
  nsIFrame *   mCurrentPageFrame;
  PRPackedBool mDoingPageRange;
  PRInt32      mPrintRangeType;
  PRInt32      mFromPageNum;
  PRInt32      mToPageNum;
  PRPackedBool mSkipPageBegin;
  PRPackedBool mSkipPageEnd;
  PRPackedBool mPrintThisPage;

  PRPackedBool mSupressHF;
  nscoord      mOffsetX;
  nscoord      mOffsetY;

  nsSize       mSize;
  nsSharedPageData* mPageData; // data shared by all the nsPageFrames

  // Selection Printing Info
  nscoord      mSelectionHeight;
  nscoord      mYSelOffset;

  // I18N date formatter service which we'll want to cache locally.
  nsCOMPtr<nsIDateTimeFormat> mDateFormatter;

private:
  void CacheBackground(nsPresContext* aPresContext);

};

#endif /* nsSimplePageSequence_h___ */

