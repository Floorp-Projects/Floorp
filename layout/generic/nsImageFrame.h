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
#include "nsAReadableString.h"
#include "nsIPresContext.h"
#include "nsIImageFrame.h"

#include "nsTransform2D.h"
#include "imgIRequest.h"
#include "imgIDecoderObserver.h"
#include "imgIContainerObserver.h"

class nsIFrame;
class nsImageMap;
class nsIURI;
class nsILoadGroup;
struct nsHTMLReflowState;
struct nsHTMLReflowMetrics;
struct nsSize;

class nsImageFrame;

class nsImageListener : public imgIDecoderObserver
{
public:
  nsImageListener(nsImageFrame *aFrame);
  virtual ~nsImageListener();

  NS_DECL_ISUPPORTS
  NS_DECL_IMGIDECODEROBSERVER
  NS_DECL_IMGICONTAINEROBSERVER

  void SetFrame(nsImageFrame *frame) { mFrame = frame; }

private:
  nsImageFrame *mFrame;
};

#define ImageFrameSuper nsLeafFrame

class nsImageFrame : public ImageFrameSuper, public nsIImageFrame {
public:
  nsImageFrame();

  // nsISupports 
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  NS_IMETHOD Destroy(nsIPresContext* aPresContext);
  NS_IMETHOD Init(nsIPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext,
                  nsIFrame*        aPrevInFlow);
  NS_IMETHOD Paint(nsIPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer,
                   PRUint32             aFlags = 0);
  NS_IMETHOD Reflow(nsIPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);
  
  NS_IMETHOD CanContinueTextRun(PRBool& aContinueTextRun) const;


  NS_IMETHOD  GetContentForEvent(nsIPresContext* aPresContext,
                                 nsEvent* aEvent,
                                 nsIContent** aContent);
  NS_IMETHOD HandleEvent(nsIPresContext* aPresContext,
                        nsGUIEvent* aEvent,
                        nsEventStatus* aEventStatus);
  NS_IMETHOD GetCursor(nsIPresContext* aPresContext,
                       nsPoint& aPoint,
                       PRInt32& aCursor);
  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent* aChild,
                              PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32 aModType, 
                              PRInt32 aHint);

#ifdef ACCESSIBILITY
  NS_IMETHOD GetAccessible(nsIAccessible** aAccessible);
#endif

  NS_IMETHOD GetFrameType(nsIAtom** aResult) const;
  NS_IMETHOD GetIntrinsicImageSize(nsSize& aSize);

  NS_IMETHOD GetNaturalImageSize(PRUint32* naturalWidth, 
                                 PRUint32 *naturalHeight);

  NS_IMETHOD GetImageRequest(imgIRequest **aRequest);

  NS_IMETHOD IsImageComplete(PRBool* aComplete);

#ifdef DEBUG
  NS_IMETHOD SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const;
#endif

  NS_IMETHOD OnStartDecode(imgIRequest *aRequest, nsIPresContext *aCX);
  NS_IMETHOD OnStartContainer(imgIRequest *aRequest, nsIPresContext *aCX, imgIContainer *aImage);
  NS_IMETHOD OnStartFrame(imgIRequest *aRequest, nsIPresContext *aCX, gfxIImageFrame *aFrame);
  NS_IMETHOD OnDataAvailable(imgIRequest *aRequest, nsIPresContext *aCX, gfxIImageFrame *aFrame, const nsRect * rect);
  NS_IMETHOD OnStopFrame(imgIRequest *aRequest, nsIPresContext *aCX, gfxIImageFrame *aFrame);
  NS_IMETHOD OnStopContainer(imgIRequest *aRequest, nsIPresContext *aCX, imgIContainer *aImage);
  NS_IMETHOD OnStopDecode(imgIRequest *aRequest, nsIPresContext *aCX, nsresult aStatus, const PRUnichar *aStatusArg);
  NS_IMETHOD FrameChanged(imgIContainer *aContainer, nsIPresContext *aCX, gfxIImageFrame *aNewframe, nsRect *aDirtyRect);

protected:
  // nsISupports
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);

  virtual ~nsImageFrame();

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredSize);

  nsImageMap* GetImageMap(nsIPresContext* aPresContext);

  void TriggerLink(nsIPresContext* aPresContext,
                   const nsString& aURLSpec,
                   const nsString& aTargetSpec,
                   PRBool aClick);

  PRBool IsServerImageMap();

  void TranslateEventCoords(nsIPresContext* aPresContext,
                            const nsPoint& aPoint,
                            nsPoint& aResult);

  PRBool GetAnchorHREFAndTarget(nsString& aHref, nsString& aTarget);

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


  nsresult LoadImage(const nsAReadableString& aSpec, nsIPresContext *aPresContext, imgIRequest **aRequest);

  inline PRBool CanLoadImage(nsIURI *aURI);

  inline void GetURI(const nsAReadableString& aSpec, nsIURI **aURI);
  inline void GetRealURI(const nsAReadableString& aSpec, nsIURI **aURI);

  inline void GetBaseURI(nsIURI **uri);
  inline void GetLoadGroup(nsIPresContext *aPresContext, nsILoadGroup **aLoadGroup);

  void FireDOMEvent(PRUint32 aMessage);

  nsImageMap*         mImageMap;

  nsCOMPtr<imgIRequest> mImageRequest;
  nsCOMPtr<imgIRequest> mLowImageRequest;

  nsCOMPtr<imgIDecoderObserver> mListener;

  nsSize mComputedSize;
  nsSize mIntrinsicSize;

  nsTransform2D mTransform;

  PRPackedBool        mSizeConstrained;
  PRPackedBool        mGotInitialReflow;
  PRPackedBool        mInitialLoadCompleted;
  PRPackedBool        mCanSendLoadEvent;

  PRBool              mFailureReplace;

  nsMargin            mBorderPadding;
  PRUint32            mNaturalImageWidth, 
                      mNaturalImageHeight;
};

#endif /* nsImageFrame_h___ */
