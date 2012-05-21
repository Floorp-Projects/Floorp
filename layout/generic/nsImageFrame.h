/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for replaced elements with bitmap image data */

#ifndef nsImageFrame_h___
#define nsImageFrame_h___

#include "nsSplittableFrame.h"
#include "nsIIOService.h"
#include "nsIObserver.h"

#include "nsStubImageDecoderObserver.h"
#include "imgIDecoderObserver.h"

#include "nsDisplayList.h"
#include "imgIContainer.h"

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
class nsImageLoadingContent;

namespace mozilla {
namespace layers {
  class ImageContainer;
  class ImageLayer;
  class LayerManager;
}
}

class nsImageListener : public nsStubImageDecoderObserver
{
public:
  nsImageListener(nsImageFrame *aFrame);
  virtual ~nsImageListener();

  NS_DECL_ISUPPORTS
  // imgIDecoderObserver (override nsStubImageDecoderObserver)
  NS_IMETHOD OnStartContainer(imgIRequest *aRequest, imgIContainer *aImage);
  NS_IMETHOD OnDataAvailable(imgIRequest *aRequest, bool aCurrentFrame,
                             const nsIntRect *aRect);
  NS_IMETHOD OnStopDecode(imgIRequest *aRequest, nsresult status,
                          const PRUnichar *statusArg);
  // imgIContainerObserver (override nsStubImageDecoderObserver)
  NS_IMETHOD FrameChanged(imgIRequest *aRequest,
                          imgIContainer *aContainer,
                          const nsIntRect *dirtyRect);

  void SetFrame(nsImageFrame *frame) { mFrame = frame; }

private:
  nsImageFrame *mFrame;
};

#define IMAGE_SIZECONSTRAINED       NS_FRAME_STATE_BIT(20)
#define IMAGE_GOTINITIALREFLOW      NS_FRAME_STATE_BIT(21)

#define ImageFrameSuper nsSplittableFrame

class nsImageFrame : public ImageFrameSuper {
public:
  typedef mozilla::layers::ImageContainer ImageContainer;
  typedef mozilla::layers::ImageLayer ImageLayer;
  typedef mozilla::layers::LayerManager LayerManager;

  NS_DECL_FRAMEARENA_HELPERS

  nsImageFrame(nsStyleContext* aContext);

  NS_DECL_QUERYFRAME_TARGET(nsImageFrame)
  NS_DECL_QUERYFRAME

  virtual void DestroyFrom(nsIFrame* aDestructRoot);
  NS_IMETHOD Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow);
  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);
  virtual nscoord GetMinWidth(nsRenderingContext *aRenderingContext);
  virtual nscoord GetPrefWidth(nsRenderingContext *aRenderingContext);
  virtual IntrinsicSize GetIntrinsicSize();
  virtual nsSize GetIntrinsicRatio();
  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);
  
  NS_IMETHOD  GetContentForEvent(nsEvent* aEvent,
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

  virtual bool IsFrameOfType(PRUint32 aFlags) const
  {
    return ImageFrameSuper::IsFrameOfType(aFlags & ~(nsIFrame::eReplaced));
  }

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
  NS_IMETHOD List(FILE* out, PRInt32 aIndent) const;
#endif

  virtual PRIntn GetSkipSides() const;

  nsresult GetIntrinsicImageSize(nsSize& aSize);

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
  static bool ShouldCreateImageFrameFor(mozilla::dom::Element* aElement,
                                          nsStyleContext* aStyleContext);
  
  void DisplayAltFeedback(nsRenderingContext& aRenderingContext,
                          const nsRect&        aDirtyRect,
                          imgIRequest*         aRequest,
                          nsPoint              aPt);

  nsRect GetInnerArea() const;

  /**
   * Return a map element associated with this image.
   */
  mozilla::dom::Element* GetMapElement() const
  {
    nsAutoString usemap;
    if (mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::usemap, usemap)) {
      return mContent->OwnerDoc()->FindImageMap(usemap);
    }
    return nsnull;
  }

  /**
   * Return true if the image has associated image map.
   */
  bool HasImageMap() const { return mImageMap || GetMapElement(); }

  nsImageMap* GetImageMap();
  nsImageMap* GetExistingImageMap() const { return mImageMap; }

  virtual void AddInlineMinWidth(nsRenderingContext *aRenderingContext,
                                 InlineMinWidthData *aData);

  void DisconnectMap();
protected:
  virtual ~nsImageFrame();

  void EnsureIntrinsicSizeAndRatio(nsPresContext* aPresContext);

  virtual nsSize ComputeSize(nsRenderingContext *aRenderingContext,
                             nsSize aCBSize, nscoord aAvailableWidth,
                             nsSize aMargin, nsSize aBorder, nsSize aPadding,
                             PRUint32 aFlags) MOZ_OVERRIDE;

  bool IsServerImageMap();

  void TranslateEventCoords(const nsPoint& aPoint,
                            nsIntPoint& aResult);

  bool GetAnchorHREFTargetAndNode(nsIURI** aHref, nsString& aTarget,
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
                        nsRenderingContext& aContext);

  void DisplayAltText(nsPresContext*      aPresContext,
                      nsRenderingContext& aRenderingContext,
                      const nsString&      aAltText,
                      const nsRect&        aRect);

  void PaintImage(nsRenderingContext& aRenderingContext, nsPoint aPt,
                  const nsRect& aDirtyRect, imgIContainer* aImage,
                  PRUint32 aFlags);

protected:
  friend class nsImageListener;
  friend class nsImageLoadingContent;
  nsresult OnStartContainer(imgIRequest *aRequest, imgIContainer *aImage);
  nsresult OnDataAvailable(imgIRequest *aRequest, bool aCurrentFrame,
                           const nsIntRect *rect);
  nsresult OnStopDecode(imgIRequest *aRequest,
                        nsresult aStatus,
                        const PRUnichar *aStatusArg);
  nsresult FrameChanged(imgIRequest *aRequest,
                        imgIContainer *aContainer,
                        const nsIntRect *aDirtyRect);
  /**
   * Notification that aRequest will now be the current request.
   */
  void NotifyNewCurrentRequest(imgIRequest *aRequest, nsresult aStatus);

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
  bool UpdateIntrinsicSize(imgIContainer* aImage);

  /**
   * Recalculate mIntrinsicRatio from the image.
   *
   * @return whether aImage's ratio did _not_
   *         match our previous intrinsic ratio.
   */
  bool UpdateIntrinsicRatio(imgIContainer* aImage);

  /**
   * This function calculates the transform for converting between
   * source space & destination space. May fail if our image has a
   * percent-valued or zero-valued height or width.
   *
   * @param aTransform The transform object to populate.
   *
   * @return whether we succeeded in creating the transform.
   */
  bool GetSourceToDestTransform(nsTransform2D& aTransform);

  /**
   * Helper functions to check whether the request or image container
   * corresponds to a load we don't care about.  Most of the decoder
   * observer methods will bail early if these return true.
   */
  bool IsPendingLoad(imgIRequest* aRequest) const;
  bool IsPendingLoad(imgIContainer* aContainer) const;

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

  bool mDisplayingIcon;

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

    void Shutdown();

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
#ifdef DEBUG
        bool rv =
#endif
            mIconObservers.RemoveElement(frame);
        NS_ABORT_IF_FALSE(rv, "Observer not in array");
    }

  private:
    void GetPrefs();
    nsTObserverArray<nsImageFrame*> mIconObservers;


  public:
    nsCOMPtr<imgIRequest> mLoadingImage;
    nsCOMPtr<imgIRequest> mBrokenImage;
    bool             mPrefForceInlineAltText;
    bool             mPrefShowPlaceholders;
  };
  
public:
  static IconLoad* gIconLoad; // singleton pattern: one LoadIcons instance is used
  
  friend class nsDisplayImage;
};

/**
 * Note that nsDisplayImage does not receive events. However, an image element
 * is replaced content so its background will be z-adjacent to the
 * image itself, and hence receive events just as if the image itself
 * received events.
 */
class nsDisplayImage : public nsDisplayItem {
public:
  typedef mozilla::layers::ImageContainer ImageContainer;
  typedef mozilla::layers::ImageLayer ImageLayer;
  typedef mozilla::layers::LayerManager LayerManager;

  nsDisplayImage(nsDisplayListBuilder* aBuilder, nsImageFrame* aFrame,
                 imgIContainer* aImage)
    : nsDisplayItem(aBuilder, aFrame), mImage(aImage) {
    MOZ_COUNT_CTOR(nsDisplayImage);
  }
  virtual ~nsDisplayImage() {
    MOZ_COUNT_DTOR(nsDisplayImage);
  }
  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext* aCtx);

  /**
   * Returns an ImageContainer for this image if the image type
   * supports it (TYPE_RASTER only).
   */
  already_AddRefed<ImageContainer> GetContainer();

  gfxRect GetDestRect();

  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerParameters& aParameters);

  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerParameters& aContainerParameters);

  /**
   * Configure an ImageLayer for this display item.
   * Set the required filter and scaling transform.
   */
  void ConfigureLayer(ImageLayer* aLayer);

  NS_DISPLAY_DECL_NAME("Image", TYPE_IMAGE)
private:
  nsCOMPtr<imgIContainer> mImage;
};

#endif /* nsImageFrame_h___ */
