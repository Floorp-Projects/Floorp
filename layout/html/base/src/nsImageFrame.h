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
#ifndef nsImageFrame_h___
#define nsImageFrame_h___

#include "nsLeafFrame.h"
#include "nsString.h"
#include "nsIPresContext.h"
#include "nsHTMLImageLoader.h"

class nsIFrame;
class nsImageMap;
class nsIImage;
class nsIURI;
struct nsHTMLReflowState;
struct nsHTMLReflowMetrics;
struct nsSize;

#define ImageFrameSuper nsLeafFrame

class nsImageFrame : public ImageFrameSuper {
public:
  nsImageFrame();

  NS_IMETHOD Destroy(nsIPresContext* aPresContext);
  NS_IMETHOD Init(nsIPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext,
                  nsIFrame*        aPrevInFlow);
  NS_IMETHOD Paint(nsIPresContext* aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect,
                   nsFramePaintLayer aWhichLayer);
  NS_IMETHOD Reflow(nsIPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);
  NS_IMETHOD  GetContentForEvent(nsIPresContext* aPresContext,
                                 nsEvent* aEvent,
                                 nsIContent** aContent);
  NS_METHOD HandleEvent(nsIPresContext* aPresContext,
                        nsGUIEvent* aEvent,
                        nsEventStatus* aEventStatus);
  NS_IMETHOD GetCursor(nsIPresContext* aPresContext,
                       nsPoint& aPoint,
                       PRInt32& aCursor);
  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent* aChild,
                              PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32 aHint);
  NS_IMETHOD GetFrameType(nsIAtom** aResult) const;
  NS_IMETHOD GetIntrinsicImageSize(nsSize& aSize);
  NS_IMETHOD IsImageComplete(PRBool* aComplete);

#ifdef DEBUG
  NS_IMETHOD SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const;
#endif

protected:
  virtual ~nsImageFrame();

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredSize);

  nsresult UpdateImage(nsIPresContext* aPresContext, PRUint32 aStatus, void* aClosure);

  nsImageMap* GetImageMap(nsIPresContext* aPresContext);

  void TriggerLink(nsIPresContext* aPresContext,
                   const nsString& aURLSpec,
                   const nsString& aTargetSpec,
                   PRBool aClick);

  PRBool IsServerImageMap();

  void TranslateEventCoords(nsIPresContext* aPresContext,
                            const nsPoint& aPoint,
                            nsPoint& aResult);

  PRBool GetAnchorHREF(nsString& aResult);

  PRIntn GetSuppress();

  void MeasureString(const PRUnichar*     aString,
                     PRInt32              aLength,
                     nscoord              aMaxWidth,
                     PRUint32&            aMaxFit,
                     nsIRenderingContext& aContext);

  void DisplayAltText(nsIPresContext*      aPresContext,
                      nsIRenderingContext& aRenderingContext,
                      const nsString&      aAltText,
                      const nsRect&        aRect);

  void DisplayAltFeedback(nsIPresContext*      aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          PRInt32              aIconId);

  void GetInnerArea(nsIPresContext* aPresContext,
                    nsRect& aInnerArea) const;

  static nsresult UpdateImageFrame(nsIPresContext* aPresContext,
                                   nsHTMLImageLoader* aLoader,
                                   nsIFrame* aFrame,
                                   void* aClosure,
                                   PRUint32 aStatus);

  nsHTMLImageLoader   mImageLoader;
  nsHTMLImageLoader * mLowSrcImageLoader;
  nsImageMap*         mImageMap;
  PRPackedBool        mSizeFrozen;
  PRPackedBool        mInitialLoadCompleted;
  PRPackedBool        mCanSendLoadEvent;
  nsMargin            mBorderPadding;
};

#endif /* nsImageFrame_h___ */
