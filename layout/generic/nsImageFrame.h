/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for replaced elements with image data */

#ifndef nsImageFrame_h___
#define nsImageFrame_h___

#include "nsSplittableFrame.h"
#include "nsIIOService.h"
#include "nsIObserver.h"

#include "imgINotificationObserver.h"

#include "nsDisplayList.h"
#include "imgIContainer.h"
#include "mozilla/Attributes.h"
#include "mozilla/DebugOnly.h"
#include "nsIReflowCallback.h"
#include "nsTObserverArray.h"

class nsFontMetrics;
class nsImageMap;
class nsIURI;
class nsILoadGroup;
struct nsHTMLReflowState;
class nsHTMLReflowMetrics;
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

class nsImageListener : public imgINotificationObserver
{
protected:
  virtual ~nsImageListener();

public:
  explicit nsImageListener(nsImageFrame *aFrame);

  NS_DECL_ISUPPORTS
  NS_DECL_IMGINOTIFICATIONOBSERVER

  void SetFrame(nsImageFrame *frame) { mFrame = frame; }

private:
  nsImageFrame *mFrame;
};

typedef nsSplittableFrame ImageFrameSuper;

class nsImageFrame : public ImageFrameSuper,
                     public nsIReflowCallback {
public:
  typedef mozilla::image::DrawResult DrawResult;
  typedef mozilla::layers::ImageContainer ImageContainer;
  typedef mozilla::layers::ImageLayer ImageLayer;
  typedef mozilla::layers::LayerManager LayerManager;

  NS_DECL_FRAMEARENA_HELPERS

  explicit nsImageFrame(nsStyleContext* aContext);

  NS_DECL_QUERYFRAME_TARGET(nsImageFrame)
  NS_DECL_QUERYFRAME

  virtual void DestroyFrom(nsIFrame* aDestructRoot) override;
  virtual void DidSetStyleContext(nsStyleContext* aOldStyleContext) override;

  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) override;
  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override;
  virtual nscoord GetMinISize(nsRenderingContext *aRenderingContext) override;
  virtual nscoord GetPrefISize(nsRenderingContext *aRenderingContext) override;
  virtual mozilla::IntrinsicSize GetIntrinsicSize() override;
  virtual nsSize GetIntrinsicRatio() override;
  virtual void Reflow(nsPresContext*           aPresContext,
                      nsHTMLReflowMetrics&     aDesiredSize,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&          aStatus) override;
  
  virtual nsresult  GetContentForEvent(mozilla::WidgetEvent* aEvent,
                                       nsIContent** aContent) override;
  virtual nsresult HandleEvent(nsPresContext* aPresContext,
                               mozilla::WidgetGUIEvent* aEvent,
                               nsEventStatus* aEventStatus) override;
  virtual nsresult GetCursor(const nsPoint& aPoint,
                             nsIFrame::Cursor& aCursor) override;
  virtual nsresult AttributeChanged(int32_t aNameSpaceID,
                                    nsIAtom* aAttribute,
                                    int32_t aModType) override;

#ifdef ACCESSIBILITY
  virtual mozilla::a11y::AccType AccessibleType() override;
#endif

  virtual nsIAtom* GetType() const override;

  virtual bool IsFrameOfType(uint32_t aFlags) const override
  {
    return ImageFrameSuper::IsFrameOfType(aFlags & ~(nsIFrame::eReplaced));
  }

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
  void List(FILE* out = stderr, const char* aPrefix = "", 
            uint32_t aFlags = 0) const override;
#endif

  virtual LogicalSides GetLogicalSkipSides(const nsHTMLReflowState* aReflowState = nullptr) const override;

  nsresult GetIntrinsicImageSize(nsSize& aSize);

  static void ReleaseGlobals() {
    if (gIconLoad) {
      gIconLoad->Shutdown();
      NS_RELEASE(gIconLoad);
    }
    NS_IF_RELEASE(sIOService);
  }

  nsresult Notify(imgIRequest *aRequest, int32_t aType, const nsIntRect* aData);

  /**
   * Function to test whether aContent, which has aStyleContext as its style,
   * should get an image frame.  Note that this method is only used by the
   * frame constructor; it's only here because it uses gIconLoad for now.
   */
  static bool ShouldCreateImageFrameFor(mozilla::dom::Element* aElement,
                                          nsStyleContext* aStyleContext);
  
  DrawResult DisplayAltFeedback(nsRenderingContext& aRenderingContext,
                                const nsRect& aDirtyRect,
                                nsPoint aPt,
                                uint32_t aFlags);

  nsRect GetInnerArea() const;

  /**
   * Return a map element associated with this image.
   */
  mozilla::dom::Element* GetMapElement() const;

  /**
   * Return true if the image has associated image map.
   */
  bool HasImageMap() const { return mImageMap || GetMapElement(); }

  nsImageMap* GetImageMap();
  nsImageMap* GetExistingImageMap() const { return mImageMap; }

  virtual void AddInlineMinISize(nsRenderingContext *aRenderingContext,
                                 InlineMinISizeData *aData) override;

  void DisconnectMap();

  // nsIReflowCallback
  virtual bool ReflowFinished() override;
  virtual void ReflowCallbackCanceled() override;

protected:
  virtual ~nsImageFrame();

  void EnsureIntrinsicSizeAndRatio();

  virtual mozilla::LogicalSize
  ComputeSize(nsRenderingContext *aRenderingContext,
              mozilla::WritingMode aWritingMode,
              const mozilla::LogicalSize& aCBSize,
              nscoord aAvailableISize,
              const mozilla::LogicalSize& aMargin,
              const mozilla::LogicalSize& aBorder,
              const mozilla::LogicalSize& aPadding,
              ComputeSizeFlags aFlags) override;

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
  nscoord MeasureString(const char16_t*     aString,
                        int32_t              aLength,
                        nscoord              aMaxWidth,
                        uint32_t&            aMaxFit,
                        nsRenderingContext& aContext,
                        nsFontMetrics&      aFontMetrics);

  void DisplayAltText(nsPresContext*      aPresContext,
                      nsRenderingContext& aRenderingContext,
                      const nsString&      aAltText,
                      const nsRect&        aRect);

  DrawResult PaintImage(nsRenderingContext& aRenderingContext, nsPoint aPt,
                        const nsRect& aDirtyRect, imgIContainer* aImage,
                        uint32_t aFlags);

protected:
  friend class nsImageListener;
  friend class nsImageLoadingContent;

  nsresult OnSizeAvailable(imgIRequest* aRequest, imgIContainer* aImage);
  nsresult OnFrameUpdate(imgIRequest* aRequest, const nsIntRect* aRect);
  nsresult OnLoadComplete(imgIRequest* aRequest, nsresult aStatus);

  /**
   * Notification that aRequest will now be the current request.
   */
  void NotifyNewCurrentRequest(imgIRequest *aRequest, nsresult aStatus);

  /// Always sync decode our image when painting if @aForce is true.
  void SetForceSyncDecoding(bool aForce) { mForceSyncDecoding = aForce; }

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
   * Helper function to check whether the request corresponds to a load we don't
   * care about.  Most of the decoder observer methods will bail early if this
   * returns true.
   */
  bool IsPendingLoad(imgIRequest* aRequest) const;

  /**
   * Function to convert a dirty rect in the source image to a dirty
   * rect for the image frame.
   */
  nsRect SourceRectToDest(const nsIntRect & aRect);

  /**
   * Triggers invalidation for both our image display item and, if appropriate,
   * our alt-feedback display item.
   *
   * @param aLayerInvalidRect The area to invalidate in layer space. If null, the
   *                          entire layer will be invalidated.
   * @param aFrameInvalidRect The area to invalidate in frame space. If null, the
   *                          entire frame will be invalidated.
   */
  void InvalidateSelf(const nsIntRect* aLayerInvalidRect,
                      const nsRect* aFrameInvalidRect);

  nsRefPtr<nsImageMap> mImageMap;

  nsCOMPtr<imgINotificationObserver> mListener;

  nsCOMPtr<imgIContainer> mImage;
  nsSize mComputedSize;
  mozilla::IntrinsicSize mIntrinsicSize;
  nsSize mIntrinsicRatio;

  bool mDisplayingIcon;
  bool mFirstFrameComplete;
  bool mReflowCallbackPosted;
  bool mForceSyncDecoding;

  static nsIIOService* sIOService;
  
  /* loading / broken image icon support */

  // XXXbz this should be handled by the prescontext, I think; that
  // way we would have a single iconload per mozilla session instead
  // of one per document...

  // LoadIcons: initiate the loading of the static icons used to show
  // loading / broken images
  nsresult LoadIcons(nsPresContext *aPresContext);
  nsresult LoadIcon(const nsAString& aSpec, nsPresContext *aPresContext,
                    imgRequestProxy **aRequest);

  class IconLoad final : public nsIObserver,
                         public imgINotificationObserver
  {
    // private class that wraps the data and logic needed for
    // broken image and loading image icons
  public:
    IconLoad();

    void Shutdown();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
    NS_DECL_IMGINOTIFICATIONOBSERVER

    void AddIconObserver(nsImageFrame *frame) {
        MOZ_ASSERT(!mIconObservers.Contains(frame),
                   "Observer shouldn't aleady be in array");
        mIconObservers.AppendElement(frame);
    }

    void RemoveIconObserver(nsImageFrame *frame) {
      mozilla::DebugOnly<bool> didRemove = mIconObservers.RemoveElement(frame);
      MOZ_ASSERT(didRemove, "Observer not in array");
    }

  private:
    ~IconLoad() {}

    void GetPrefs();
    nsTObserverArray<nsImageFrame*> mIconObservers;


  public:
    nsRefPtr<imgRequestProxy> mLoadingImage;
    nsRefPtr<imgRequestProxy> mBrokenImage;
    bool             mPrefForceInlineAltText;
    bool             mPrefShowPlaceholders;
    bool             mPrefShowLoadingPlaceholder;
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
class nsDisplayImage : public nsDisplayImageContainer {
public:
  typedef mozilla::layers::LayerManager LayerManager;

  nsDisplayImage(nsDisplayListBuilder* aBuilder, nsImageFrame* aFrame,
                 imgIContainer* aImage)
    : nsDisplayImageContainer(aBuilder, aFrame), mImage(aImage) {
    MOZ_COUNT_CTOR(nsDisplayImage);
  }
  virtual ~nsDisplayImage() {
    MOZ_COUNT_DTOR(nsDisplayImage);
  }

  virtual nsDisplayItemGeometry* AllocateGeometry(nsDisplayListBuilder* aBuilder) override;
  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion) override;
  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext* aCtx) override;

  virtual bool CanOptimizeToImageLayer(LayerManager* aManager,
                                       nsDisplayListBuilder* aBuilder) override;

  /**
   * Returns an ImageContainer for this image if the image type
   * supports it (TYPE_RASTER only).
   */
  virtual already_AddRefed<ImageContainer> GetContainer(LayerManager* aManager,
                                                        nsDisplayListBuilder* aBuilder) override;

  /**
   * @return the dest rect we'll use when drawing this image, in app units.
   */
  nsRect GetDestRect(bool* aSnap = nullptr);

  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters) override;
  nsRect GetBounds(bool* aSnap)
  {
    *aSnap = true;

    nsImageFrame* imageFrame = static_cast<nsImageFrame*>(mFrame);
    return imageFrame->GetInnerArea() + ToReferenceFrame();
  }

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) override
  {
    return GetBounds(aSnap);
  }

  virtual nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                   bool* aSnap) override;

  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerLayerParameters& aContainerParameters) override;

  /**
   * Configure an ImageLayer for this display item.
   * Set the required filter and scaling transform.
   */
  virtual void ConfigureLayer(ImageLayer* aLayer,
                              const ContainerLayerParameters& aParameters) override;

  NS_DISPLAY_DECL_NAME("Image", TYPE_IMAGE)
private:
  nsCOMPtr<imgIContainer> mImage;
};

#endif /* nsImageFrame_h___ */
