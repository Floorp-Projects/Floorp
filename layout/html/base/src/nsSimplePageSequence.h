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

  // nsIPageSequenceFrame
  NS_IMETHOD  Print(nsIPresContext*         aPresContext,
                    nsIPrintOptions*        aPrintOptions,
                    nsIPrintStatusCallback* aStatusCallback);
  NS_IMETHOD SetOffsets(nscoord aStartOffset, nscoord aEndOffset);
  NS_IMETHOD SetPageNo(PRInt32 aPageNo) { return NS_OK;}

#ifdef DEBUG
  // Debugging
  NS_IMETHOD  GetFrameName(nsString& aResult) const;
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
  NS_IMETHOD_(nsrefcnt) AddRef(void) {return nsContainerFrame::AddRef();}
  NS_IMETHOD_(nsrefcnt) Release(void) {return nsContainerFrame::Release();}

  nscoord  mStartOffset;
  nscoord  mEndOffset;

  nsMargin mMargin;
  PRBool   mIsPrintingSelection;
};

#endif /* nsSimplePageSequence_h___ */

