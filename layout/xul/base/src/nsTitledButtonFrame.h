/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

/**

 
**/

#include "nsHTMLImage.h"
#include "nsLeafFrame.h"
#include "nsButtonFrameRenderer.h"

class nsIPopUpMenu;

class nsTitledButtonImageLoader : public nsHTMLImageLoader {
public:

  void GetDesiredSize(nsIPresContext* aPresContext,
                      const nsHTMLReflowState& aReflowState,
                      nsIFrame* aTargetFrame,
                      nsFrameImageLoaderCB aCallBack,
                      nsHTMLReflowMetrics& aDesiredSize);

  void StopLoadImage(nsIPresContext& aPresContext,
                     nsIFrame* aTargetFrame);

};

class nsTitledButtonFrame : public nsLeafFrame
{
public:

  enum TruncationStyle { Left, Right, Center };

  friend nsresult NS_NewTitledButtonFrame(nsIFrame*& aNewFrame);

    NS_IMETHOD  Init(nsIPresContext&  aPresContext,
                   nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsIStyleContext* aContext,
                   nsIFrame*        asPrevInFlow);

      NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent* aChild,
                              nsIAtom* aAttribute,
                              PRInt32 aHint);

  NS_IMETHOD  ReResolveStyleContext ( nsIPresContext* aPresContext, 
                                      nsIStyleContext* aParentContext,
                                      PRInt32 aParentChange,
                                      nsStyleChangeList* aChangeList,
                                      PRInt32* aLocalChange) ;

  NS_IMETHOD DeleteFrame(nsIPresContext& aPresContext);


  virtual void UpdateAttributes(nsIPresContext&  aPresContext);

    // nsIHTMLReflow overrides
  NS_IMETHOD Reflow(nsIPresContext&          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD  Paint(nsIPresContext& aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect& aDirtyRect,
                    nsFramePaintLayer aWhichLayer);

  NS_IMETHOD HandleEvent(nsIPresContext& aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus& aEventStatus);

 
protected:

	  NS_IMETHOD  PaintTitle(nsIPresContext& aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect& aDirtyRect,
                    nsFramePaintLayer aWhichLayer);

      NS_IMETHOD  PaintImage(nsIPresContext& aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect& aDirtyRect,
                    nsFramePaintLayer aWhichLayer);

      virtual void LayoutTitleAndImage(nsIPresContext& aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect& aDirtyRect,
                    nsFramePaintLayer aWhichLayer);

	  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredSize);


 void DisplayAltFeedback(nsIPresContext&      aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          PRInt32              aIconId);
 void DisplayAltText(nsIPresContext&      aPresContext,
                      nsIRenderingContext& aRenderingContext,
                      const nsString&      aAltText,
                      const nsRect&        aRect);

 void MeasureString(const PRUnichar*     aString,
                     PRInt32              aLength,
                     nscoord              aMaxWidth,
                     PRUint32&            aMaxFit,
                     nsIRenderingContext& aContext);

  nsTitledButtonFrame();
  virtual void CalculateTitleForWidth(nsIPresContext& aPresContext, nsIRenderingContext& aRenderingContext, nscoord aWidth);
  virtual void GetTextSize(nsIPresContext& aPresContext, nsIRenderingContext& aRenderingContext, const nsString& aString, nsSize& aSize);
  virtual void SetDisabled(nsAutoString aDisabled);

private:

  TruncationStyle mTruncationType;
  PRIntn mAlign;
  nsString mTitle;
  nsString mTruncatedTitle;

  nsTitledButtonImageLoader mImageLoader;
  PRBool mSizeFrozen;
  nsMargin mBorderPadding;
  nsRect mImageRect;
  nsRect mTitleRect;
  PRBool mNeedsLayout;
  nscoord mSpacing;
  nsSize mMinSize;
  nsButtonFrameRenderer mRenderer;
  PRBool mHasImage;

  nsIPopUpMenu * mPopUpMenu;
  PRBool         mMenuIsPoppedUp;

 
 
}; // class nsTitledButtonFrame

