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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef nsTitledButtonFrame_h___
#define nsTitledButtonFrame_h___

#include "nsHTMLImageLoader.h"
#include "nsLeafBoxFrame.h"

class nsIPopUpMenu;
class nsTitledButtonRenderer;

class nsTitledButtonFrame : public nsLeafBoxFrame
{
public:

  // nsIBox
  NS_IMETHOD GetPrefSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize);
  NS_IMETHOD GetMinSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize);
  NS_IMETHOD GetAscent(nsBoxLayoutState& aBoxLayoutState, nscoord& aAscent);
  NS_IMETHOD Layout(nsBoxLayoutState& aBoxLayoutState);
  NS_IMETHOD NeedsRecalc();

  // our methods
  enum CroppingStyle { CropNone, CropLeft, CropRight, CropCenter };

  friend nsresult NS_NewTitledButtonFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

  // nsIBox frame interface

  NS_IMETHOD  Init(nsIPresContext*  aPresContext,
                   nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsIStyleContext* aContext,
                   nsIFrame*        asPrevInFlow);

  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent* aChild,
                              PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32 aHint);

  NS_IMETHOD  GetAdditionalStyleContext(PRInt32 aIndex, 
                                        nsIStyleContext** aStyleContext) const;
  NS_IMETHOD  SetAdditionalStyleContext(PRInt32 aIndex, 
                                        nsIStyleContext* aStyleContext);
  NS_IMETHOD  DidSetStyleContext (nsIPresContext* aPresContext);

  NS_IMETHOD Destroy(nsIPresContext* aPresContext);

  NS_IMETHOD GetFrameName(nsString& aResult) const;

  virtual PRIntn GetDefaultAlignment();
  virtual void UpdateAttributes(nsIPresContext*  aPresContext, nsIAtom* aAttribute, PRBool& aResize, PRBool& aRedraw, PRBool& aUpdateAccessUnderline);
  virtual void UpdateImage(nsIPresContext*  aPresContext, PRBool& aResize);


  NS_IMETHOD  Paint(nsIPresContext* aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect& aDirtyRect,
                    nsFramePaintLayer aWhichLayer);

  NS_IMETHOD HandleEvent(nsIPresContext* aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus* aEventStatus);

 
  virtual ~nsTitledButtonFrame();
protected:

  void CacheSizes(nsBoxLayoutState& aBoxLayoutState);

  enum CheckState { eUnset, eOff, eOn, eMixed } ;

  CheckState GetCurrentCheckState();
  void SetCurrentCheckState(CheckState aState);
  void UpdateAccessUnderline();

  virtual void MouseClicked(nsIPresContext* aPresContext);

  NS_IMETHOD  PaintTitle(nsIPresContext* aPresContext,
                         nsIRenderingContext& aRenderingContext,
                         const nsRect& aDirtyRect,
                         nsFramePaintLayer aWhichLayer);

  NS_IMETHOD  PaintImage(nsIPresContext* aPresContext,
                         nsIRenderingContext& aRenderingContext,
                         const nsRect& aDirtyRect,
                         nsFramePaintLayer aWhichLayer);

  virtual void LayoutTitleAndImage(nsIPresContext* aPresContext,
                                   nsIRenderingContext& aRenderingContext,
                                   const nsRect& aDirtyRect,
                                   nsFramePaintLayer aWhichLayer);


  void DisplayAltFeedback(nsIPresContext*      aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          PRInt32              aIconId);
  void DisplayAltText(nsIPresContext*      aPresContext,
                      nsIRenderingContext& aRenderingContext,
                      const nsString&      aAltText,
                      const nsRect&        aRect);

  void MeasureString(const PRUnichar*     aString,
                     PRInt32              aLength,
                     nscoord              aMaxWidth,
                     PRUint32&            aMaxFit,
                     nsIRenderingContext& aContext);

  nsTitledButtonFrame(nsIPresShell* aShell);

  virtual void CalculateTitleForWidth(nsIPresContext* aPresContext, nsIRenderingContext& aRenderingContext, nscoord aWidth);

  virtual void GetTextSize(nsIPresContext* aPresContext, nsIRenderingContext& aRenderingContext, const nsString& aString, nsSize& aSize, nscoord& aAscent);

  virtual void SetDisabled(nsAutoString aDisabled);

  static nsresult UpdateImageFrame(nsIPresContext* aPresContext,
                                   nsHTMLImageLoader* aLoader,
                                   nsIFrame* aFrame,
                                   void* aClosure,
                                   PRUint32 aStatus);

  void GetImageSource(nsString& aResult);

  virtual void GetImageSize(nsIPresContext* aPresContext);

private:

  // tri state methods
  void CheckStateToString ( CheckState inState, nsString& outStateAsString ) ;
  CheckState StringToCheckState ( const nsString & aStateAsString ) ;

  PRBool mHasOnceBeenInMixedState;
 
  CroppingStyle mCropType;
  PRIntn mAlign;
  nsString mTitle;
  nsString mCroppedTitle;

  nsHTMLImageLoader mImageLoader;
  PRBool mSizeFrozen;
  nsMargin mBorderPadding;
  nsRect mImageRect;
  nsRect mTitleRect;
  PRBool mNeedsLayout;
  nscoord mSpacing;
  nsTitledButtonRenderer* mRenderer;
  PRBool mHasImage;

  // accesskey highlighting
  PRBool mNeedsAccessUpdate;
  PRInt32 mAccesskeyIndex;
  nscoord mBeforeWidth, mAccessWidth, mAccessUnderlineSize, mAccessOffset;

  nsSize mPrefSize;
  nsSize mMinSize;
  nscoord mAscent;
  PRBool mNeedsRecalc;


 // nsIPopUpMenu * mPopUpMenu;
 // PRBool         mMenuIsPoppedUp;
 
}; // class nsTitledButtonFrame

#endif /* nsTitledButtonFrame_h___ */
