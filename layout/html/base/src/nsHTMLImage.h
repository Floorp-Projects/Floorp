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
#ifndef nsHTMLImage_h___
#define nsHTMLImage_h___

#include "nsLeafFrame.h"
#include "nslayout.h"
#include "nsIFrameImageLoader.h"
#include "nsString.h"
class nsIFrame;
class nsIFrameImageLoader;
class nsIPresContext;
class nsISizeOfHandler;
class nsIImageMap;
struct nsHTMLReflowState;
struct nsHTMLReflowMetrics;
struct nsSize;

/**
 * HTML image loader. This is designed to encapsulate the loading
 * and sizing process of html images (basically so that the logic
 * can be reused in the image button form control code and the html
 * image layout code).
 */
class nsHTMLImageLoader {
public:
  nsHTMLImageLoader();
  ~nsHTMLImageLoader();

  void DestroyLoader() {
    NS_IF_RELEASE(mImageLoader);
    mLoadImageFailed = PR_FALSE;
#ifndef _WIN32
    mLoadBrokenImageFailed = PR_FALSE;
#endif
  }

  nsIImage* GetImage();

  nsresult SetURL(const nsString& aURLSpec);

  nsresult SetBaseHREF(const nsString& aBaseHREF);

  void GetURL(nsString& aResult) {
    if (nsnull != mURLSpec) {
      aResult = *mURLSpec;
    }
  }

  void GetBaseHREF(nsString& aResult) {
    aResult.Truncate();
    if (nsnull != mBaseHREF) {
      aResult = *mBaseHREF;
    }
  }

  nsresult StartLoadImage(nsIPresContext* aPresContext,
                          nsIFrame* aForFrame,
                          nsFrameImageLoaderCB aCallBack,
                          PRBool aNeedSizeUpdate,
                          PRIntn& aLoadStatus);

  void GetDesiredSize(nsIPresContext* aPresContext,
                      const nsHTMLReflowState& aReflowState,
                      nsIFrame* aTargetFrame,
                      nsFrameImageLoaderCB aCallBack,
                      nsHTMLReflowMetrics& aDesiredSize);

  PRBool GetLoadImageFailed() const;

  void SizeOf(nsISizeOfHandler* aHandler) const;

protected:
  nsIFrameImageLoader* mImageLoader;
  PRPackedBool mLoadImageFailed;
#ifndef _WIN32
  PRPackedBool mLoadBrokenImageFailed;
#endif
  nsString* mURLSpec;
  nsString* mBaseHREF;
};


#define ImageFrameSuper nsLeafFrame
class nsImageFrame : public ImageFrameSuper {
public:
  NS_IMETHOD DeleteFrame(nsIPresContext& aPresContext);
  NS_IMETHOD SizeOf(nsISizeOfHandler* aHandler) const;
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
  void SizeOfWithoutThis(nsISizeOfHandler* aHandler) const;

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredSize);

  nsIImageMap* GetImageMap();

  void TriggerLink(nsIPresContext& aPresContext,
                   const nsString& aURLSpec,
                   const nsString& aTargetSpec,
                   PRBool aClick);

  PRBool IsServerImageMap();

  PRIntn GetSuppress();

  nscoord MeasureString(const PRUnichar*     aString,
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

  nsHTMLImageLoader mImageLoader;
  nsIImageMap* mImageMap;
  PRBool mSizeFrozen;
  nsMargin mBorderPadding;
};

#endif /* nsHTMLImage_h___ */
