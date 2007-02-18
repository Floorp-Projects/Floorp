/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* rendering object for replaced elements with bitmap image data */

#ifndef nsImageFrame_h___
#define nsImageFrame_h___

#include "nsSplittableFrame.h"
#include "nsString.h"
#include "nsAString.h"
#include "nsPresContext.h"
#include "nsIImageFrame.h"
#include "nsIIOService.h"
#include "nsIObserver.h"

#include "nsTransform2D.h"
#include "imgIRequest.h"
#include "nsStubImageDecoderObserver.h"

class nsIFrame;
class nsImageMap;
class nsIURI;
class nsILoadGroup;
struct nsHTMLReflowState;
struct nsHTMLReflowMetrics;
struct nsSize;
class nsDisplayImage;

class nsImageFrame;

class nsImageListener : public nsStubImageDecoderObserver
{
public:
  nsImageListener(nsImageFrame *aFrame);
  virtual ~nsImageListener();

  NS_DECL_ISUPPORTS
  // imgIDecoderObserver (override nsStubImageDecoderObserver)
  NS_IMETHOD OnStartContainer(imgIRequest *aRequest, imgIContainer *aImage);
  NS_IMETHOD OnDataAvailable(imgIRequest *aRequest, gfxIImageFrame *aFrame,
                             const nsRect *aRect);
  NS_IMETHOD OnStopDecode(imgIRequest *aRequest, nsresult status,
                          const PRUnichar *statusArg);
  // imgIContainerObserver (override nsStubImageDecoderObserver)
  NS_IMETHOD FrameChanged(imgIContainer *aContainer, gfxIImageFrame *newframe,
                          nsRect * dirtyRect);

  void SetFrame(nsImageFrame *frame) { mFrame = frame; }

private:
  nsImageFrame *mFrame;
};

#define IMAGE_SIZECONSTRAINED       0x00100000
#define IMAGE_GOTINITIALREFLOW      0x00200000

#define ImageFrameSuper nsSplittableFrame

class nsImageFrame : public ImageFrameSuper, public nsIImageFrame {
public:
  nsImageFrame(nsStyleContext* aContext);

  // nsISupports 
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  virtual void Destroy();
  NS_IMETHOD Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow);
  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);
  virtual nscoord GetMinWidth(nsIRenderingContext *aRenderingContext);
  virtual nscoord GetPrefWidth(nsIRenderingContext *aRenderingContext);
  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);
  
  NS_IMETHOD  GetContentForEvent(nsPresContext* aPresContext,
                                 nsEvent* aEvent,
                                 nsIContent** aContent);
  NS_IMETHOD HandleEvent(nsPresContext* aPresContext,
                        nsGUIEvent* aEvent,
                        nsEventStatus* aEventStatus);
  NS_IMETHOD GetCursor(const nsPoint& aPoint,
                       nsIFrame::Cursor& aCursor);
  NS_IMETHOD AttributeChanged(PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32 aModType);

#ifdef ACCESSIBILITY
  NS_IMETHOD GetAccessible(nsIAccessible** aAccessible);
#endif

  virtual nsIAtom* GetType() const;
  virtual PRBool IsFrameOfType(PRUint32 aFlags) const;
#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
  NS_IMETHOD List(FILE* out, PRInt32 aIndent) const;
#endif

  NS_IMETHOD GetImageMap(nsPresContext *aPresContext, nsIImageMap **aImageMap);

  NS_IMETHOD GetIntrinsicImageSize(nsSize& aSize);

  static void ReleaseGlobals() {
    if (gIconLoad) {
      gIconLoad->Shutdown();
      NS_RELEASE(gIconLoad);
    }
    NS_IF_RELEASE(sIOService);
  }

  /**
   * Function to test whether aContent, which has aStyleContext as its style,
   * should get an image frame.  Note that this method is only used by the
   * frame constructor; it's only here because it uses gIconLoad for now.
   */
  static PRBool ShouldCreateImageFrameFor(nsIContent* aContent,
                                          nsStyleContext* aStyleContext);
  
  void DisplayAltFeedback(nsIRenderingContext& aRenderingContext,
                          imgIRequest*         aRequest,
                          nsPoint              aPt);

  nsRect GetInnerArea() const;

  nsImageMap* GetImageMap(nsPresContext* aPresContext);

protected:
  // nsISupports
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);

  virtual ~nsImageFrame();

  void EnsureIntrinsicSize(nsPresContext* aPresContext);

  virtual nsSize ComputeSize(nsIRenderingContext *aRenderingContext,
                             nsSize aCBSize, nscoord aAvailableWidth,
                             nsSize aMargin, nsSize aBorder, nsSize aPadding,
                             PRBool aShrinkWrap);

  void TriggerLink(nsPresContext* aPresContext,
                   nsIURI* aURI,
                   const nsString& aTargetSpec,
                   nsINode* aTriggerNode,
                   PRBool aClick);

  PRBool IsServerImageMap();

  void TranslateEventCoords(const nsPoint& aPoint,
                            nsIntPoint& aResult);

  PRBool GetAnchorHREFTargetAndNode(nsIURI** aHref, nsString& aTarget,
                                    nsINode** aNode);
  /**
   * Computes the width of the string that fits into the available space
   *
   * @param in aLength total length of the string in PRUnichars
   * @param in aMaxWidth width not to be exceeded
   * @param out aMaxFit length of the string that fits within aMaxWidth
   *            in PRUnichars
   * @return width of the string that fits within aMaxWidth
   */
  nscoord MeasureString(const PRUnichar*     aString,
                        PRInt32              aLength,
                        nscoord              aMaxWidth,
                        PRUint32&            aMaxFit,
                        nsIRenderingContext& aContext);

  void DisplayAltText(nsPresContext*      aPresContext,
                      nsIRenderingContext& aRenderingContext,
                      const nsString&      aAltText,
                      const nsRect&        aRect);

  void PaintImage(nsIRenderingContext& aRenderingContext, nsPoint aPt,
                  const nsRect& aDirtyRect, imgIContainer* aImage);
                  
protected:
  friend class nsImageListener;
  nsresult OnStartContainer(imgIRequest *aRequest, imgIContainer *aImage);
  nsresult OnDataAvailable(imgIRequest *aRequest,
                           gfxIImageFrame *aFrame,
                           const nsRect * rect);
  nsresult OnStopDecode(imgIRequest *aRequest,
                        nsresult aStatus,
                        const PRUnichar *aStatusArg);
  nsresult FrameChanged(imgIContainer *aContainer,
                        gfxIImageFrame *aNewframe,
                        nsRect *aDirtyRect);

private:
  // random helpers
  inline void SpecToURI(const nsAString& aSpec, nsIIOService *aIOService,
                        nsIURI **aURI);

  inline void GetLoadGroup(nsPresContext *aPresContext,
                           nsILoadGroup **aLoadGroup);
  nscoord GetContinuationOffset(nscoord* aWidth = 0) const;
  void GetDocumentCharacterSet(nsACString& aCharset) const;

  /**
   * This function will recalculate mTransform.  If a non-null image
   * is passed in, mIntrinsicSize will be recalculated from the image
   * size.  Otherwise, mIntrinsicSize will not be touched.
   *
   * @return PR_TRUE if aImage is non-null and its size did _not_
   *         match our previous intrinsic size
   * @return PR_FALSE otherwise
   */
  PRBool RecalculateTransform(imgIContainer* aImage);

  /**
   * Helper functions to check whether the request or image container
   * corresponds to a load we don't care about.  Most of the decoder
   * observer methods will bail early if these return true.
   */
  PRBool IsPendingLoad(imgIRequest* aRequest) const;
  PRBool IsPendingLoad(imgIContainer* aContainer) const;

  /**
   * Function to convert a dirty rect in the source image to a dirty
   * rect for the image frame.
   */
  nsRect SourceRectToDest(const nsRect & aRect);

  nsImageMap*         mImageMap;

  nsCOMPtr<imgIDecoderObserver> mListener;

  nsSize mComputedSize;
  nsSize mIntrinsicSize;
  nsTransform2D mTransform;
  
  nsMargin            mBorderPadding;

  static nsIIOService* sIOService;

  /* loading / broken image icon support */

  // XXXbz this should be handled by the prescontext, I think; that
  // way we would have a single iconload per mozilla session instead
  // of one per document...

  // LoadIcons: initiate the loading of the static icons used to show
  // loading / broken images
  nsresult LoadIcons(nsPresContext *aPresContext);
  nsresult LoadIcon(const nsAString& aSpec, nsPresContext *aPresContext,
                    imgIRequest **aRequest);
  
  // HandleIconLoads: See if the request is for an Icon load. If it
  // is, handle it and return TRUE otherwise, return FALSE (aCompleted
  // is an input arg telling the routine if the request has completed)
  PRBool HandleIconLoads(imgIRequest* aRequest, PRBool aCompleted);

  class IconLoad : public nsIObserver {
    // private class that wraps the data and logic needed for 
    // broken image and loading image icons
  public:
    IconLoad(imgIDecoderObserver* aObserver);

    void Shutdown()
    {
      // in case the pref service releases us later
      if (mLoadingImage) {
        mLoadingImage->Cancel(NS_ERROR_FAILURE);
        mLoadingImage = nsnull;
      }
      if (mBrokenImage) {
        mBrokenImage->Cancel(NS_ERROR_FAILURE);
        mBrokenImage = nsnull;
      }
    }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER

  private:
    void GetPrefs();

  public:
    nsCOMPtr<imgIRequest> mLoadingImage;
    nsCOMPtr<imgIRequest> mBrokenImage;
    nsCOMPtr<imgIDecoderObserver> mLoadObserver; // keeps the observer alive
    PRUint8          mIconsLoaded;
    PRPackedBool     mPrefForceInlineAltText;
    PRPackedBool     mPrefShowPlaceholders;
  };
  
public:
  static IconLoad* gIconLoad; // singleton pattern: one LoadIcons instance is used
  
  friend class nsDisplayImage;
};

#endif /* nsImageFrame_h___ */
