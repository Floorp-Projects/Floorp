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
#ifndef nsXULTextFrame_h___
#define nsXULTextFrame_h___

#include "nsLeafFrame.h"
#include "nsIBox.h"

class nsAccessKeyInfo;

class nsXULTextFrame : public nsLeafFrame, public nsIBox
{
public:

  enum CroppingStyle { CropNone, CropLeft, CropRight, CropCenter };

  friend nsresult NS_NewXULTextFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

  // nsIBox frame interface
  NS_IMETHOD GetBoxInfo(nsIPresContext* aPresContext, const nsHTMLReflowState& aReflowState, nsBoxInfo& aSize);
  NS_IMETHOD InvalidateCache(nsIFrame* aChild);

  NS_DECL_ISUPPORTS

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

  NS_IMETHOD GetFrameName(nsString& aResult) const;

  virtual void UpdateAttributes(nsIPresContext*  aPresContext, nsIAtom* aAttribute, PRBool& aResize, PRBool& aRedraw, PRBool& aUpdateAccessUnderline);

  // nsIHTMLReflow overrides
  NS_IMETHOD Reflow(nsIPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD  Paint(nsIPresContext* aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect& aDirtyRect,
                    nsFramePaintLayer aWhichLayer);

 
  ~nsXULTextFrame();
protected:

  void UpdateAccessUnderline();

  NS_IMETHOD  PaintTitle(nsIPresContext* aPresContext,
                         nsIRenderingContext& aRenderingContext,
                         const nsRect& aDirtyRect,
                         nsFramePaintLayer aWhichLayer,
                         const nsRect& aTextRect);

  virtual void LayoutTitle(nsIPresContext* aPresContext,
                                   nsIRenderingContext& aRenderingContext,
                                   const nsRect& aDirtyRect,
                                   nsFramePaintLayer aWhichLayer,
                                   const nsRect& aTextRect);

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredSize);


  nsXULTextFrame();

  virtual void CalculateTitleForWidth(nsIPresContext* aPresContext, nsIRenderingContext& aRenderingContext, nscoord aWidth);
  virtual void GetTextSize(nsIPresContext* aPresContext, nsIRenderingContext& aRenderingContext, const nsString& aString, nsSize& aSize, nscoord& aAscent);

private:

  CroppingStyle mCropType;
  nsString mTitle;
  nsString mCroppedTitle;
  nscoord mTitleWidth;
  nsAccessKeyInfo* mAccessKeyInfo;
}; // class nsXULTextFrame

#endif /* nsTitledButtonFrame_h___ */
