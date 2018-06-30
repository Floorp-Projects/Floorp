/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for replaced elements with image data */

#ifndef nsImageFrame_h___
#define nsImageFrame_h___

#include "nsAtomicContainerFrame.h"
#include "nsIIOService.h"
#include "nsIObserver.h"

#include "imgINotificationObserver.h"

#include "nsDisplayList.h"
#include "imgIContainer.h"
#include "mozilla/Attributes.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/StaticPtr.h"
#include "nsIReflowCallback.h"
#include "nsTObserverArray.h"

class nsFontMetrics;
class nsImageMap;
class nsIURI;
class nsILoadGroup;
class nsDisplayImage;
class nsPresContext;
class nsImageFrame;
class nsTransform2D;
class nsImageLoadingContent;

namespace mozilla {
class PresShell;
namespace layers {
  class ImageContainer;
  class ImageLayer;
  class LayerManager;
} // namespace layers
} // namespace mozilla

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

class nsImageFrame : public nsAtomicContainerFrame
                   , public nsIReflowCallback {
public:
  template <typename T> using Maybe = mozilla::Maybe<T>;
  using Nothing = mozilla::Nothing;
  using Visibility = mozilla::Visibility;

  typedef mozilla::image::ImgDrawResult ImgDrawResult;
  typedef mozilla::layers::ImageContainer ImageContainer;
  typedef mozilla::layers::ImageLayer ImageLayer;
  typedef mozilla::layers::LayerManager LayerManager;

  NS_DECL_FRAMEARENA_HELPERS(nsImageFrame)
  NS_DECL_QUERYFRAME

  virtual void DestroyFrom(nsIFrame* aDestructRoot, PostDestroyData& aPostDestroyData) override;
  virtual void DidSetComputedStyle(ComputedStyle* aOldComputedStyle) override;

  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) override;
  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsDisplayListSet& aLists) override;
  virtual nscoord GetMinISize(gfxContext *aRenderingContext) override;
  virtual nscoord GetPrefISize(gfxContext *aRenderingContext) override;
  virtual mozilla::IntrinsicSize GetIntrinsicSize() override;
  virtual nsSize GetIntrinsicRatio() override;
  virtual void Reflow(nsPresContext*           aPresContext,
                      ReflowOutput&     aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus&          aStatus) override;

  virtual nsresult  GetContentForEvent(mozilla::WidgetEvent* aEvent,
                                       nsIContent** aContent) override;
  virtual nsresult HandleEvent(nsPresContext* aPresContext,
                               mozilla::WidgetGUIEvent* aEvent,
                               nsEventStatus* aEventStatus) override;
  virtual nsresult GetCursor(const nsPoint& aPoint,
                             nsIFrame::Cursor& aCursor) override;
  virtual nsresult AttributeChanged(int32_t aNameSpaceID,
                                    nsAtom* aAttribute,
                                    int32_t aModType) override;

  void OnVisibilityChange(Visibility aNewVisibility,
                          const Maybe<OnNonvisible>& aNonvisibleAction = Nothing()) override;

  void ResponsiveContentDensityChanged();
  void SetupForContentURLRequest();

#ifdef ACCESSIBILITY
  virtual mozilla::a11y::AccType AccessibleType() override;
#endif

  virtual bool IsFrameOfType(uint32_t aFlags) const override
  {
    return nsAtomicContainerFrame::IsFrameOfType(aFlags &
      ~(nsIFrame::eReplaced | nsIFrame::eReplacedSizing));
  }

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
  void List(FILE* out = stderr, const char* aPrefix = "",
            uint32_t aFlags = 0) const override;
#endif

  nsSplittableType GetSplittableType() const override
  {
    return NS_FRAME_SPLITTABLE;
  }

  virtual LogicalSides GetLogicalSkipSides(const ReflowInput* aReflowInput = nullptr) const override;

  nsresult GetIntrinsicImageSize(nsSize& aSize);

  static void ReleaseGlobals() {
    if (gIconLoad) {
      gIconLoad->Shutdown();
      gIconLoad = nullptr;
    }
    NS_IF_RELEASE(sIOService);
  }

  already_AddRefed<imgIRequest> GetCurrentRequest() const;
  nsresult Notify(imgIRequest *aRequest, int32_t aType, const nsIntRect* aData);

  /**
   * Function to test whether aContent, which has aComputedStyle as its style,
   * should get an image frame.  Note that this method is only used by the
   * frame constructor; it's only here because it uses gIconLoad for now.
   */
  static bool ShouldCreateImageFrameFor(mozilla::dom::Element* aElement,
                                        ComputedStyle* aComputedStyle);

  ImgDrawResult DisplayAltFeedback(gfxContext& aRenderingContext,
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

  virtual void AddInlineMinISize(gfxContext *aRenderingContext,
                                 InlineMinISizeData *aData) override;

  void DisconnectMap();

  // nsIReflowCallback
  virtual bool ReflowFinished() override;
  virtual void ReflowCallbackCanceled() override;

  // The kind of image frame we are.
  enum class Kind : uint8_t
  {
    // For an nsImageLoadingContent.
    ImageElement,
    // For css 'content: url(..)' on non-generated content, or on generated
    // ::before / ::after with exactly one item in the content property.
    ContentProperty,
    // For a child of a ::before / ::after pseudo-element that had more than one
    // item for the content property.
    ContentPropertyAtIndex,
  };

  // Creates a suitable continuing frame for this frame.
  nsImageFrame* CreateContinuingFrame(nsIPresShell*, ComputedStyle*) const;

private:
  friend nsIFrame* NS_NewImageFrame(nsIPresShell*, ComputedStyle*);
  friend nsIFrame* NS_NewImageFrameForContentProperty(nsIPresShell*, ComputedStyle*);
  friend nsIFrame* NS_NewImageFrameForGeneratedContentIndex(nsIPresShell*, ComputedStyle*);

  nsImageFrame(ComputedStyle* aStyle, Kind aKind)
    : nsImageFrame(aStyle, kClassID, aKind)
  { }

  nsImageFrame(ComputedStyle*, ClassID, Kind);

protected:
  nsImageFrame(ComputedStyle* aStyle, ClassID aID)
    : nsImageFrame(aStyle, aID, Kind::ImageElement)
  { }

  virtual ~nsImageFrame();

  void EnsureIntrinsicSizeAndRatio();

  bool GotInitialReflow() const
  {
    return !HasAnyStateBits(NS_FRAME_FIRST_REFLOW);
  }

  virtual mozilla::LogicalSize
  ComputeSize(gfxContext *aRenderingContext,
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
                        gfxContext&          aContext,
                        nsFontMetrics&      aFontMetrics);

  void DisplayAltText(nsPresContext*      aPresContext,
                      gfxContext&          aRenderingContext,
                      const nsString&      aAltText,
                      const nsRect&        aRect);

  ImgDrawResult PaintImage(gfxContext& aRenderingContext, nsPoint aPt,
                        const nsRect& aDirtyRect, imgIContainer* aImage,
                        uint32_t aFlags);

  /**
   * If we're ready to decode - that is, if our current request's image is
   * available and our decoding heuristics are satisfied - then trigger a decode
   * for our image at the size we predict it will be drawn next time it's
   * painted.
   */
  void MaybeDecodeForPredictedSize();

protected:
  friend class nsImageListener;
  friend class nsImageLoadingContent;
  friend class mozilla::PresShell;

  nsresult OnSizeAvailable(imgIRequest* aRequest, imgIContainer* aImage);
  nsresult OnFrameUpdate(imgIRequest* aRequest, const nsIntRect* aRect);
  nsresult OnLoadComplete(imgIRequest* aRequest, nsresult aStatus);

  /**
   * Notification that aRequest will now be the current request.
   */
  void NotifyNewCurrentRequest(imgIRequest *aRequest, nsresult aStatus);

  /// Always sync decode our image when painting if @aForce is true.
  void SetForceSyncDecoding(bool aForce) { mForceSyncDecoding = aForce; }

  /**
   * Computes the predicted dest rect that we'll draw into, in app units, based
   * upon the provided frame content box. (The content box is what
   * nsDisplayImage::GetBounds() returns.)
   * The result is not necessarily contained in the frame content box.
   */
  nsRect PredictedDestRect(const nsRect& aFrameContentBox);

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

  RefPtr<nsImageMap> mImageMap;

  RefPtr<nsImageListener> mListener;

  // An image request created for content: url(..).
  RefPtr<imgRequestProxy> mContentURLRequest;

  nsCOMPtr<imgIContainer> mImage;
  nsCOMPtr<imgIContainer> mPrevImage;
  nsSize mComputedSize;
  mozilla::IntrinsicSize mIntrinsicSize;
  nsSize mIntrinsicRatio;

  const Kind mKind;
  bool mContentURLRequestRegistered;
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
    RefPtr<imgRequestProxy> mLoadingImage;
    RefPtr<imgRequestProxy> mBrokenImage;
    bool             mPrefForceInlineAltText;
    bool             mPrefShowPlaceholders;
    bool             mPrefShowLoadingPlaceholder;
  };

public:
  // singleton pattern: one LoadIcons instance is used
  static mozilla::StaticRefPtr<IconLoad> gIconLoad;

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
                 imgIContainer* aImage, imgIContainer* aPrevImage)
    : nsDisplayImageContainer(aBuilder, aFrame)
    , mImage(aImage)
    , mPrevImage(aPrevImage)
  {
    MOZ_COUNT_CTOR(nsDisplayImage);
  }
  virtual ~nsDisplayImage() {
    MOZ_COUNT_DTOR(nsDisplayImage);
  }

  virtual nsDisplayItemGeometry* AllocateGeometry(nsDisplayListBuilder* aBuilder) override;
  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion) const override;
  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     gfxContext* aCtx) override;

  virtual already_AddRefed<imgIContainer> GetImage() override;

  /**
   * @return The dest rect we'll use when drawing this image, in app units.
   *         Not necessarily contained in this item's bounds.
   */
  virtual nsRect GetDestRect() const override;

  virtual void UpdateDrawResult(mozilla::image::ImgDrawResult aResult) override
  {
    nsDisplayItemGenericImageGeometry::UpdateDrawResult(this, aResult);
  }

  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters) override;
  nsRect GetBounds(bool* aSnap) const
  {
    *aSnap = true;

    nsImageFrame* imageFrame = static_cast<nsImageFrame*>(mFrame);
    return imageFrame->GetInnerArea() + ToReferenceFrame();
  }

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) const override
  {
    return GetBounds(aSnap);
  }

  virtual nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                   bool* aSnap) const override;

  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerLayerParameters& aContainerParameters) override;
  virtual bool CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                       mozilla::wr::IpcResourceUpdateQueue& aResources,
                                       const StackingContextHelper& aSc,
                                       mozilla::layers::WebRenderLayerManager* aManager,
                                       nsDisplayListBuilder* aDisplayListBuilder) override;

  NS_DISPLAY_DECL_NAME("Image", TYPE_IMAGE)
private:
  nsCOMPtr<imgIContainer> mImage;
  nsCOMPtr<imgIContainer> mPrevImage;
};

#endif /* nsImageFrame_h___ */
