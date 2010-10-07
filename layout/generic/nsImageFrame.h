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
 *   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation
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
#include "nsIImageFrame.h"
#include "nsIIOService.h"
#include "nsIObserver.h"

#include "imgIRequest.h"
#include "nsStubImageDecoderObserver.h"
#include "imgIDecoderObserver.h"

class nsIFrame;
class nsImageMap;
class nsIURI;
class nsILoadGroup;
struct nsHTMLReflowState;
struct nsHTMLReflowMetrics;
struct nsSize;
class nsDisplayImage;
class nsPresContext;
class nsImageFrame;
class nsTransform2D;

class nsImageListener : public nsStubImageDecoderObserver
{
public:
  nsImageListener(nsImageFrame *aFrame);
  virtual ~nsImageListener();

  NS_DECL_ISUPPORTS
  // imgIDecoderObserver (override nsStubImageDecoderObserver)
  NS_IMETHOD OnStartContainer(imgIRequest *aRequest, imgIContainer *aImage);
  NS_IMETHOD OnDataAvailable(imgIRequest *aRequest, PRBool aCurrentFrame,
                             const nsIntRect *aRect);
  NS_IMETHOD OnStopDecode(imgIRequest *aRequest, nsresult status,
                          const PRUnichar *statusArg);
  // imgIContainerObserver (override nsStubImageDecoderObserver)
  NS_IMETHOD FrameChanged(imgIContainer *aContainer,
                          const nsIntRect *dirtyRect);

  void SetFrame(nsImageFrame *frame) { mFrame = frame; }

private:
  nsImageFrame *mFrame;
};

#define IMAGE_SIZECONSTRAINED       NS_FRAME_STATE_BIT(20)
#define IMAGE_GOTINITIALREFLOW      NS_FRAME_STATE_BIT(21)

#define ImageFrameSuper nsSplittableFrame

class nsImageFrame : public ImageFrameSuper, public nsIImageFrame {
public:
  NS_DECL_FRAMEARENA_HELPERS

  nsImageFrame(nsStyleContext* aContext);

  NS_DECL_QUERYFRAME

  virtual void DestroyFrom(nsIFrame* aDestructRoot);
  NS_IMETHOD Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow);
  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);
  virtual nscoord GetMinWidth(nsIRenderingContext *aRenderingContext);
  virtual nscoord GetPrefWidth(nsIRenderingContext *aRenderingContext);
  virtual IntrinsicSize GetIntrinsicSize();
  virtual nsSize GetIntrinsicRatio();
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
  virtual already_AddRefed<nsAccessible> CreateAccessible();
#endif

  virtual nsIAtom* GetType() const;

  virtual PRBool IsFrameOfType(PRUint32 aFlags) const
  {
    return ImageFrameSuper::IsFrameOfType(aFlags & ~(nsIFrame::eReplaced));
  }

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
  NS_IMETHOD List(FILE* out, PRInt32 aIndent) const;
#endif

  virtual PRIntn GetSkipSides() const;

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
                          const nsRect&        aDirtyRect,
                          imgIRequest*         aRequest,
                          nsPoint              aPt);

  nsRect GetInnerArea() const;

  nsImageMap* GetImageMap(nsPresContext* aPresContext);

  virtual void AddInlineMinWidth(nsIRenderingContext *aRenderingContext,
                                 InlineMinWidthData *aData);

protected:
  virtual ~nsImageFrame();

  void EnsureIntrinsicSizeAndRatio(nsPresContext* aPresContext);

  virtual nsSize ComputeSize(nsIRenderingContext *aRenderingContext,
                             nsSize aCBSize, nscoord aAvailableWidth,
                             nsSize aMargin, nsSize aBorder, nsSize aPadding,
                             PRBool aShrinkWrap);

  PRBool IsServerImageMap();

  void TranslateEventCoords(const nsPoint& aPoint,
                            nsIntPoint& aResult);

  PRBool GetAnchorHREFTargetAndNode(nsIURI** aHref, nsString& aTarget,
                                    nsIContent** aNode);
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
                  const nsRect& aDirtyRect, imgIContainer* aImage,
                  PRUint32 aFlags);
                  
protected:
  friend class nsImageListener;
  nsresult OnStartContainer(imgIRequest *aRequest, imgIContainer *aImage);
  nsresult OnDataAvailable(imgIRequest *aRequest, PRBool aCurrentFrame,
                           const nsIntRect *rect);
  nsresult OnStopDecode(imgIRequest *aRequest,
                        nsresult aStatus,
                        const PRUnichar *aStatusArg);
  nsresult FrameChanged(imgIContainer *aContainer,
                        const nsIntRect *aDirtyRect);

private:
  // random helpers
  inline void SpecToURI(const nsAString& aSpec, nsIIOService *aIOService,
                        nsIURI **aURI);

  inline void GetLoadGroup(nsPresContext *aPresContext,
                           nsILoadGroup **aLoadGroup);
  nscoord GetContinuationOffset() const;
  void GetDocumentCharacterSet(nsACString& aCharset) const;
  bool ShouldDisplaySelection();

  /**
   * Recalculate mIntrinsicSize from the image.
   *
   * @return whether aImage's size did _not_
   *         match our previous intrinsic size.
   */
  PRBool UpdateIntrinsicSize(imgIContainer* aImage);

  /**
   * Recalculate mIntrinsicRatio from the image.
   *
   * @return whether aImage's ratio did _not_
   *         match our previous intrinsic ratio.
   */
  PRBool UpdateIntrinsicRatio(imgIContainer* aImage);

  /**
   * This function calculates the transform for converting between
   * source space & destination space. May fail if our image has a
   * percent-valued or zero-valued height or width.
   *
   * @param aTransform The transform object to populate.
   *
   * @return whether we succeeded in creating the transform.
   */
  PRBool GetSourceToDestTransform(nsTransform2D& aTransform);

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
  nsRect SourceRectToDest(const nsIntRect & aRect);

  nsImageMap*         mImageMap;

  nsCOMPtr<imgIDecoderObserver> mListener;

  nsSize mComputedSize;
  nsIFrame::IntrinsicSize mIntrinsicSize;
  nsSize mIntrinsicRatio;

  PRBool mDisplayingIcon;

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

  class IconLoad : public nsIObserver,
                   public imgIDecoderObserver {
    // private class that wraps the data and logic needed for
    // broken image and loading image icons
  public:
    IconLoad();

    void Shutdown()
    {
      // in case the pref service releases us later
      if (mLoadingImage) {
        mLoadingImage->CancelAndForgetObserver(NS_ERROR_FAILURE);
        mLoadingImage = nsnull;
      }
      if (mBrokenImage) {
        mBrokenImage->CancelAndForgetObserver(NS_ERROR_FAILURE);
        mBrokenImage = nsnull;
      }
    }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
    NS_DECL_IMGICONTAINEROBSERVER
    NS_DECL_IMGIDECODEROBSERVER

    void AddIconObserver(nsImageFrame *frame) {
        NS_ABORT_IF_FALSE(!mIconObservers.Contains(frame),
                          "Observer shouldn't aleady be in array");
        mIconObservers.AppendElement(frame);
    }

    void RemoveIconObserver(nsImageFrame *frame) {
        PRBool rv = mIconObservers.RemoveElement(frame);
        NS_ABORT_IF_FALSE(rv, "Observer not in array");
    }

  private:
    void GetPrefs();
    nsTObserverArray<nsImageFrame*> mIconObservers;


  public:
    nsCOMPtr<imgIRequest> mLoadingImage;
    nsCOMPtr<imgIRequest> mBrokenImage;
    PRPackedBool     mPrefForceInlineAltText;
    PRPackedBool     mPrefShowPlaceholders;
  };
  
public:
  static IconLoad* gIconLoad; // singleton pattern: one LoadIcons instance is used
  
  friend class nsDisplayImage;
};

#endif /* nsImageFrame_h___ */
