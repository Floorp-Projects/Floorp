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
#ifndef nsSimplePageSequence_h___
#define nsSimplePageSequence_h___

#include "nsIPageSequenceFrame.h"
#include "nsContainerFrame.h"
#include "nsIPrintOptions.h"

// Simple page sequence frame class. Used when we're in paginated mode
class nsSimplePageSequenceFrame : public nsContainerFrame,
                                  public nsIPageSequenceFrame {
public:
  friend nsresult NS_NewSimplePageSequenceFrame(nsIPresShell* aPresShell, nsIFrame** aResult);

  // nsISupports
  NS_IMETHOD  QueryInterface(const nsIID& aIID, void** aInstancePtr);

  // nsIFrameReflow
  NS_IMETHOD  Reflow(nsIPresContext*      aPresContext,
                     nsHTMLReflowMetrics& aDesiredSize,
                     const nsHTMLReflowState& aMaxSize,
                     nsReflowStatus&      aStatus);

  NS_IMETHOD  Paint(nsIPresContext*      aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect&        aDirtyRect,
                    nsFramePaintLayer    aWhichLayer);

  // nsIPageSequenceFrame
  NS_IMETHOD  Print(nsIPresContext*         aPresContext,
                    nsIPrintOptions*        aPrintOptions,
                    nsIPrintStatusCallback* aStatusCallback);
  NS_IMETHOD SetOffsets(nscoord aStartOffset, nscoord aEndOffset);
  NS_IMETHOD SetPageNo(PRInt32 aPageNo) { return NS_OK;}

  // Async Printing
  NS_IMETHOD StartPrint(nsIPresContext*  aPresContext,
                        nsIPrintOptions* aPrintOptions);
  NS_IMETHOD PrintNextPage(nsIPresContext*  aPresContext,
                           nsIPrintOptions* aPrintOptions);
  NS_IMETHOD GetCurrentPageNum(PRInt32* aPageNum);
  NS_IMETHOD GetNumPages(PRInt32* aNumPages);
  NS_IMETHOD IsDoingPrintRange(PRBool* aDoing);
  NS_IMETHOD GetPrintRange(PRInt32* aFromPage, PRInt32* aToPage);
  NS_IMETHOD SkipPageBegin() { mSkipPageBegin = PR_TRUE; return NS_OK; }
  NS_IMETHOD SkipPageEnd() { mSkipPageEnd = PR_TRUE; return NS_OK; }
  NS_IMETHOD DoPageEnd(nsIPresContext*  aPresContext);
  NS_IMETHOD GetPrintThisPage(PRBool*  aPrintThisPage) { *aPrintThisPage = mPrintThisPage; return NS_OK; }
  NS_IMETHOD SetOffset(nscoord aX, nscoord aY) { mOffsetX = aX; mOffsetY = aY; return NS_OK; }
  NS_IMETHOD SuppressHeadersAndFooters(PRBool aDoSup);
  NS_IMETHOD SetClipRect(nsIPresContext* aPresContext, nsRect* aSize);

#ifdef NS_DEBUG
  // Debugging
  NS_IMETHOD  GetFrameName(nsString& aResult) const;
  NS_IMETHOD SetDebugFD(FILE* aFD);
  FILE * mDebugFD;
#endif

protected:
  nsSimplePageSequenceFrame();
  virtual ~nsSimplePageSequenceFrame();

  nsresult IncrementalReflow(nsIPresContext*          aPresContext,
                             const nsHTMLReflowState& aReflowState,
                             nsSize&                  aPageSize,
                             nscoord                  aX,
                             nscoord&                 aY);

  nsresult CreateContinuingPageFrame(nsIPresContext* aPresContext,
                                     nsIFrame*       aPageFrame,
                                     nsIFrame**      aContinuingFrame);

  void SetPageNumberFormat(const char* aPropName, const char* aDefPropVal, PRBool aPageNumOnly);

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

};

#endif /* nsSimplePageSequence_h___ */

