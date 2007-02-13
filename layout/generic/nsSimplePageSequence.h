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

  nsSize      mReflowSize;
  nsMargin    mReflowMargin;
  // shadow of page in PrintPreview; drawn around bottom and right edges
  nsSize      mShadowSize;
  // Extra Margin between the device area and the edge of the page;
  // approximates unprintable area
  nsMargin    mExtraMargin;
  // Margin for headers and footers; it defaults to 4/100 of an inch on UNIX 
  // and 0 elsewhere; I think it has to do with some inconsistency in page size
  // computations
  nsMargin    mEdgePaperMargin;

  nsCOMPtr<nsIPrintSettings> mPrintSettings;
  nsCOMPtr<nsIPrintOptions> mPrintOptions;

  nscoord      mPageContentXMost;      // xmost size from Reflow(width)
  nscoord      mPageContentSize;       // constrained size (width)
};

// Simple page sequence frame class. Used when we're in paginated mode
class nsSimplePageSequenceFrame : public nsContainerFrame,
                                  public nsIPageSequenceFrame {
public:
  friend nsIFrame* NS_NewSimplePageSequenceFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  // nsISupports
  NS_IMETHOD  QueryInterface(const nsIID& aIID, void** aInstancePtr);

  // nsIFrame
  NS_IMETHOD  Reflow(nsPresContext*      aPresContext,
                     nsHTMLReflowMetrics& aDesiredSize,
                     const nsHTMLReflowState& aMaxSize,
                     nsReflowStatus&      aStatus);

  NS_IMETHOD  BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                               const nsRect&           aDirtyRect,
                               const nsDisplayListSet& aLists);

  // nsIPageSequenceFrame
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
  NS_IMETHOD PrintNextPage();
  NS_IMETHOD GetCurrentPageNum(PRInt32* aPageNum);
  NS_IMETHOD GetNumPages(PRInt32* aNumPages);
  NS_IMETHOD IsDoingPrintRange(PRBool* aDoing);
  NS_IMETHOD GetPrintRange(PRInt32* aFromPage, PRInt32* aToPage);
  NS_IMETHOD DoPageEnd();

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::sequenceFrame
   */
  virtual nsIAtom* GetType() const;
  
#ifdef NS_DEBUG
  NS_IMETHOD  GetFrameName(nsAString& aResult) const;
#endif

  void PaintPageSequence(nsIRenderingContext& aRenderingContext,
                         const nsRect&        aDirtyRect,
                         nsPoint              aPt);

protected:
  nsSimplePageSequenceFrame(nsStyleContext* aContext);
  virtual ~nsSimplePageSequenceFrame();

  nsresult CreateContinuingPageFrame(nsPresContext* aPresContext,
                                     nsIFrame*       aPageFrame,
                                     nsIFrame**      aContinuingFrame);

  void SetPageNumberFormat(const char* aPropName, const char* aDefPropVal, PRBool aPageNumOnly);

  // SharedPageData Helper methods
  void SetDateTimeStr(PRUnichar * aDateTimeStr);
  void SetPageNumberFormat(PRUnichar * aFormatStr, PRBool aForPageNumOnly);

  void GetEdgePaperMarginCoord(const char* aPrefName, nscoord& aCoord);
  void GetEdgePaperMargin(nsMargin& aMargin);

  NS_IMETHOD_(nsrefcnt) AddRef(void) {return nsContainerFrame::AddRef();}
  NS_IMETHOD_(nsrefcnt) Release(void) {return nsContainerFrame::Release();}

  nsMargin mMargin;
  PRBool   mIsPrintingSelection;

  // Asynch Printing
  PRInt32      mPageNum;
  PRInt32      mTotalPages;
  nsIFrame *   mCurrentPageFrame;
  PRPackedBool mDoingPageRange;
  PRInt32      mPrintRangeType;
  PRInt32      mFromPageNum;
  PRInt32      mToPageNum;
  PRPackedBool mPrintThisPage;

  nsSize       mSize;
  nsSharedPageData* mPageData; // data shared by all the nsPageFrames

  // Selection Printing Info
  nscoord      mSelectionHeight;
  nscoord      mYSelOffset;

  // I18N date formatter service which we'll want to cache locally.
  nsCOMPtr<nsIDateTimeFormat> mDateFormatter;

};

#endif /* nsSimplePageSequence_h___ */

