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
#ifndef nsImageFrame_h___
#define nsImageFrame_h___

#include "nsSplittableFrame.h"
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


struct ImageLoad {
  ImageLoad() : mIntrinsicSize(0,0) { }
  nsCOMPtr<imgIRequest> mRequest;
  nsSize mIntrinsicSize;

  nsTransform2D mTransform;
};

#define ImageFrameSuper nsSplittableFrame

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
  
  NS_IMETHOD  ContentChanged(nsIPresContext* aPresContext,
                             nsIContent*     aChild,
                             nsISupports*    aSubContent);

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
#ifdef DEBUG
  NS_IMETHOD List(nsIPresContext* aPresContext, FILE* out, PRInt32 aIndent) const;
#endif

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

protected:

  inline PRBool CanLoadImage(nsIURI *aURI);

  inline void GetURI(const nsAReadableString& aSpec, nsIURI **aURI);
  inline void GetRealURI(const nsAReadableString& aSpec, nsIURI **aURI);

  inline void GetBaseURI(nsIURI **uri);
  inline void GetLoadGroup(nsIPresContext *aPresContext, nsILoadGroup **aLoadGroup);

  void FireDOMEvent(PRUint32 aMessage);

private:
  nsresult LoadImage(const nsAReadableString& aSpec, nsIPresContext *aPresContext, imgIRequest *aRequest);
  nsresult RealLoadImage(const nsAReadableString& aSpec, nsIPresContext *aPresContext, imgIRequest *aRequest);
  inline int GetImageLoad(imgIRequest *aRequest);
  nscoord GetContinuationOffset(nscoord* aWidth = 0) const;

  nsImageMap*         mImageMap;

  nsCOMPtr<imgIDecoderObserver> mListener;

  /**
   * 0 is the current image being displayed on the screen.
   * 1 is for attribute changed images.
   * when the load from 1 completes, it will replace 0.
   */
  struct ImageLoad mLoads[2];

  nsSize mComputedSize;
  nsSize mIntrinsicSize;

  PRPackedBool        mSizeConstrained;
  PRPackedBool        mGotInitialReflow;
  PRPackedBool        mInitialLoadCompleted;
  PRPackedBool        mCanSendLoadEvent;

  PRBool              mFailureReplace;

  nsMargin            mBorderPadding;
  PRUint32            mNaturalImageWidth, 
                      mNaturalImageHeight;

  /* loading / broken image icon support */

  // LoadIcons: initiate the loading of the static icons used to show loading / broken images
  nsresult LoadIcons(nsIPresContext *aPresContext);
  // HandleIconLoads: See if the request is for an Icon load. If it is, handle it and return TRUE
  // otherwise, return FALSE (aCompleted is an input arg telling the routine if the request has completed)
  PRBool HandleIconLoads(imgIRequest* aRequest, PRBool aCompleted);
  void InvalidateIcon(nsIPresContext *aPresContext);

  class IconLoad {
    // private class that wraps the data and logic needed for 
    // broken image and loading image icons
  public:
    IconLoad(nsIPresContext *aPresContext):mRefCount(0),mIconsLoaded(PR_FALSE) { GetAltModePref(aPresContext); }
    void AddRef(void) { ++mRefCount; }
    PRBool Release(void) { return --mRefCount == 0; }
    void GetAltModePref(nsIPresContext *aPresContext);

    PRUint32         mRefCount;
    struct ImageLoad mIconLoads[2];   // 0 is for the 'loading' icon, 1 is for the 'broken' icon
    PRPackedBool     mIconsLoaded;
    PRPackedBool     mPrefForceInlineAltText;
  };
  static IconLoad* mIconLoad; // singleton patern: one LoadIcons instance is used
};

#endif /* nsImageFrame_h___ */
