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
 *   Peter Annema <disttsc@bart.nl>
 */
#ifndef nsTextBoxFrame_h___
#define nsTextBoxFrame_h___

#include "nsLeafBoxFrame.h"

class nsAccessKeyInfo;

class nsTextBoxFrame : public nsLeafBoxFrame
{
public:

  // nsIBox
  NS_IMETHOD GetPrefSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize);
  NS_IMETHOD GetMinSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize);
  NS_IMETHOD GetAscent(nsBoxLayoutState& aBoxLayoutState, nscoord& aAscent);
  NS_IMETHOD DoLayout(nsBoxLayoutState& aBoxLayoutState);
  NS_IMETHOD NeedsRecalc();

  enum CroppingStyle { CropNone, CropLeft, CropRight, CropCenter };

  friend nsresult NS_NewTextBoxFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

  NS_IMETHOD  Init(nsIPresContext*  aPresContext,
                   nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsIStyleContext* aContext,
                   nsIFrame*        asPrevInFlow);

  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent*     aChild,
                              PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aModType, 
                              PRInt32         aHint);

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsString& aResult) const;
#endif

  virtual void UpdateAttributes(nsIPresContext*  aPresContext,
                                nsIAtom*         aAttribute,
                                PRBool&          aResize,
                                PRBool&          aRedraw);


  NS_IMETHOD Paint(nsIPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer,
                   PRUint32             aFlags = 0);


  virtual ~nsTextBoxFrame();
protected:

  void UpdateAccessTitle();
  void UpdateAccessIndex();

  NS_IMETHOD PaintTitle(nsIPresContext*      aPresContext,
                        nsIRenderingContext& aRenderingContext,
                        const nsRect&        aDirtyRect,
                        const nsRect&        aRect);

  virtual void LayoutTitle(nsIPresContext*      aPresContext,
                           nsIRenderingContext& aRenderingContext,
                           const nsRect&        aRect);

  virtual void CalculateUnderline(nsIRenderingContext& aRenderingContext);

  virtual void CalcTextSize(nsBoxLayoutState& aBoxLayoutState);

  nsTextBoxFrame(nsIPresShell* aShell);

  virtual void CalculateTitleForWidth(nsIPresContext*      aPresContext,
                                      nsIRenderingContext& aRenderingContext,
                                      nscoord              aWidth);

  virtual void GetTextSize(nsIPresContext*      aPresContext,
                           nsIRenderingContext& aRenderingContext,
                           const nsString&      aString,
                           nsSize&              aSize,
                           nscoord&             aAscent);

private:

  CroppingStyle mCropType;
  nsString mTitle;
  nsString mCroppedTitle;
  nsString mAccessKey;
  nscoord mTitleWidth;
  nsAccessKeyInfo* mAccessKeyInfo;
  PRBool mNeedsRecalc;
  nsSize mTextSize;
  nscoord mAscent;
  static PRBool gAlwaysAppendAccessKey;
  static PRBool gAccessKeyPrefInitialized;
}; // class nsTextBoxFrame

#endif /* nsTextBoxFrame_h___ */
