/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsPageFrame_h___
#define nsPageFrame_h___

#include "nsContainerFrame.h"
#include "nsIPrintOptions.h"

// Page frame class used by the simple page sequence frame
class nsPageFrame : public nsContainerFrame {
public:
  friend nsresult NS_NewPageFrame(nsIPresShell* aPresShell, nsIFrame** aResult);

  // nsIFrame
  NS_IMETHOD  Reflow(nsIPresContext*      aPresContext,
                     nsHTMLReflowMetrics& aDesiredSize,
                     const nsHTMLReflowState& aMaxSize,
                     nsReflowStatus&      aStatus);

  NS_IMETHOD  Paint(nsIPresContext*      aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect&        aDirtyRect,
                    nsFramePaintLayer    aWhichLayer,
                    PRUint32             aFlags = 0);

  NS_IMETHOD IsPercentageBase(PRBool& aBase) const;

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::pageFrame
   */
  NS_IMETHOD GetFrameType(nsIAtom** aType) const;
  
#ifdef NS_DEBUG
  // Debugging
  NS_IMETHOD  GetFrameName(nsString& aResult) const;
  void SetDebugFD(FILE* aFD) { mDebugFD = aFD; }
  FILE * mDebugFD;
#endif

  //////////////////
  // For Printing
  //////////////////

  // Set the print options object into the page for printing
  virtual void  SetPrintOptions(nsIPrintOptions * aPrintOptions);

  // Tell the page which page number it is out of how many
  virtual void  SetPageNumInfo(PRInt32 aPageNumber, PRInt32 aTotalPages);

  virtual void  SuppressHeadersAndFooters(PRBool aDoSup) { mSupressHF = aDoSup; }
  virtual void  SetClipRect(nsRect* aClipRect)           { mClipRect = *aClipRect; }

  static void SetDateTimeStr(PRUnichar * aDateTimeStr);

  // This is class is now responsible for freeing the memory
  static void SetPageNumberFormat(PRUnichar * aFormatStr, PRBool aForPageNumOnly);

protected:
  nsPageFrame();
  virtual ~nsPageFrame();

  typedef enum {
    eHeader,
    eFooter
  } nsHeaderFooterEnum;

  nscoord GetXPosition(nsIRenderingContext& aRenderingContext, 
                       const nsRect&        aRect, 
                       PRInt32              aJust,
                       const nsString&      aStr);

  void DrawHeaderFooter(nsIRenderingContext& aRenderingContext,
                        nsIFrame *           aFrame,
                        nsHeaderFooterEnum   aHeaderFooter,
                        PRInt32              aJust,
                        const nsString&      sStr,
                        const nsRect&        aRect,
                        nscoord              aHeight,
                        nscoord              aAscent,
                        nscoord              aWidth);

  void DrawHeaderFooter(nsIRenderingContext& aRenderingContext,
                        nsIFrame *           aFrame,
                        nsHeaderFooterEnum   aHeaderFooter,
                        PRInt32              aJust,
                        const nsString&      aStr1,
                        const nsString&      aStr2,
                        const nsString&      aStr3,
                        const nsRect&        aRect,
                        nscoord              aAscent,
                        nscoord              aHeight);

  void ProcessSpecialCodes(const nsString& aStr, nsString& aNewStr);

  nsCOMPtr<nsIPrintOptions> mPrintOptions;
  PRInt32     mPageNum;
  PRInt32     mTotNumPages;
  nsMargin    mMargin;

  PRPackedBool mSupressHF;
  nsRect       mClipRect;

  static PRUnichar * mDateTimeStr;
  static nsFont *    mHeadFootFont;
  static PRUnichar * mPageNumFormat;
  static PRUnichar * mPageNumAndTotalsFormat;

};

#endif /* nsPageFrame_h___ */

