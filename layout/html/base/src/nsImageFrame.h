/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nsImageFrame_h___
#define nsImageFrame_h___

#include "nsLeafFrame.h"
#include "nsString.h"
#include "nsIPresContext.h"
#include "nsHTMLImageLoader.h"

class nsIFrame;
class nsImageMap;
class nsIImage;
class nsIURL;
struct nsHTMLReflowState;
struct nsHTMLReflowMetrics;
struct nsSize;

#define ImageFrameSuper nsLeafFrame

class nsImageFrame : public ImageFrameSuper {
public:
  NS_IMETHOD DeleteFrame(nsIPresContext& aPresContext);
  NS_IMETHOD Init(nsIPresContext&  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext,
                  nsIFrame*        aPrevInFlow);
  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect,
                   nsFramePaintLayer aWhichLayer);
  NS_IMETHOD Reflow(nsIPresContext&          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);
  NS_METHOD HandleEvent(nsIPresContext& aPresContext,
                        nsGUIEvent* aEvent,
                        nsEventStatus& aEventStatus);
  NS_IMETHOD GetCursor(nsIPresContext& aPresContext,
                       nsPoint& aPoint,
                       PRInt32& aCursor);
  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent* aChild,
                              nsIAtom* aAttribute,
                              PRInt32 aHint);

protected:
  virtual ~nsImageFrame();

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredSize);

  nsresult UpdateImage(nsIPresContext* aPresContext, PRUint32 aStatus);

  nsImageMap* GetImageMap();

  void TriggerLink(nsIPresContext& aPresContext,
                   const nsString& aURLSpec,
                   const nsString& aTargetSpec,
                   PRBool aClick);

  PRBool IsServerImageMap();

  PRIntn GetSuppress();

  void MeasureString(const PRUnichar*     aString,
                     PRInt32              aLength,
                     nscoord              aMaxWidth,
                     PRUint32&            aMaxFit,
                     nsIRenderingContext& aContext);

  void DisplayAltText(nsIPresContext&      aPresContext,
                      nsIRenderingContext& aRenderingContext,
                      const nsString&      aAltText,
                      const nsRect&        aRect);

  void DisplayAltFeedback(nsIPresContext&      aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          PRInt32              aIconId);

  void GetInnerArea(nsIPresContext* aPresContext,
                    nsRect& aInnerArea) const;

  static nsresult UpdateImageFrame(nsIPresContext* aPresContext,
                                   nsHTMLImageLoader* aLoader,
                                   nsIFrame* aFrame,
                                   void* aClosure,
                                   PRUint32 aStatus);

  nsHTMLImageLoader mImageLoader;
  nsImageMap* mImageMap;
  PRBool mSizeFrozen;
  nsMargin mBorderPadding;
};

#endif /* nsImageFrame_h___ */
