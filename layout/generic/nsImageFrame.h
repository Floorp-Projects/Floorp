/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for replaced elements with image data */

#ifndef nsImageFrame_h___
#define nsImageFrame_h___

#include "nsAtomicContainerFrame.h"
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
class nsPresContext;
class nsImageFrame;
class nsTransform2D;
class nsImageLoadingContent;

namespace mozilla {
class nsDisplayImage;
class PresShell;
namespace layers {
class ImageContainer;
class ImageLayer;
class LayerManager;
}  // namespace layers
}  // namespace mozilla

class nsImageListener final : public imgINotificationObserver {
 protected:
  virtual ~nsImageListener();

 public:
  explicit nsImageListener(nsImageFrame* aFrame);

  NS_DECL_ISUPPORTS
  NS_DECL_IMGINOTIFICATIONOBSERVER

  void SetFrame(nsImageFrame* frame) { mFrame = frame; }

 private:
  nsImageFrame* mFrame;
};

class nsImageFrame : public nsAtomicContainerFrame, public nsIReflowCallback {
 public:
  template <typename T>
  using Maybe = mozilla::Maybe<T>;
  using Nothing = mozilla::Nothing;
  using Visibility = mozilla::Visibility;

  typedef mozilla::image::ImgDrawResult ImgDrawResult;
  typedef mozilla::layers::ImageContainer ImageContainer;
  typedef mozilla::layers::ImageLayer ImageLayer;
  typedef mozilla::layers::LayerManager LayerManager;

  NS_DECL_FRAMEARENA_HELPERS(nsImageFrame)
  NS_DECL_QUERYFRAME

  void DestroyFrom(nsIFrame* aDestructRoot, PostDestroyData&) override;
  void DidSetComputedStyle(ComputedStyle* aOldStyle) final;

  void Init(nsIContent* aContent, nsContainerFrame* aParent,
            nsIFrame* aPrevInFlow) override;
  void BuildDisplayList(nsDisplayListBuilder*, const nsDisplayListSet&) final;
  nscoord GetMinISize(gfxContext* aRenderingContext) final;
  nscoord GetPrefISize(gfxContext* aRenderingContext) final;
  mozilla::IntrinsicSize GetIntrinsicSize() final { return mIntrinsicSize; }
  mozilla::AspectRatio GetIntrinsicRatio() const final {
    return mIntrinsicRatio;
  }
  void Reflow(nsPresContext*, ReflowOutput&, const ReflowInput&,
              nsReflowStatus&) override;

  nsresult GetContentForEvent(mozilla::WidgetEvent*,
                              nsIContent** aContent) final;
  nsresult HandleEvent(nsPresContext*, mozilla::WidgetGUIEvent*,
                       nsEventStatus*) override;
  mozilla::Maybe<Cursor> GetCursor(const nsPoint&) override;
  nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                            int32_t aModType) final;

  void OnVisibilityChange(
      Visibility aNewVisibility,
      const Maybe<OnNonvisible>& aNonvisibleAction = Nothing()) final;

  void ResponsiveContentDensityChanged();
  void SetupForContentURLRequest();
  bool ShouldShowBrokenImageIcon() const;

  const mozilla::StyleImage* GetImageFromStyle() const;

#ifdef ACCESSIBILITY
  mozilla::a11y::AccType AccessibleType() override;
#endif

  bool IsFrameOfType(uint32_t aFlags) const final {
    return nsAtomicContainerFrame::IsFrameOfType(
        aFlags & ~(nsIFrame::eReplaced | nsIFrame::eReplacedSizing));
  }

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override;
  void List(FILE* out = stderr, const char* aPrefix = "",
            ListFlags aFlags = ListFlags()) const final;
#endif

  LogicalSides GetLogicalSkipSides() const final;

  static void ReleaseGlobals() {
    if (gIconLoad) {
      gIconLoad->Shutdown();
      gIconLoad = nullptr;
    }
  }

  nsresult RestartAnimation();
  nsresult StopAnimation();

  already_AddRefed<imgIRequest> GetCurrentRequest() const;
  void Notify(imgIRequest*, int32_t aType, const nsIntRect* aData);

  /**
   * Function to test whether given an element and its style, that element
   * should get an image frame.  Note that this method is only used by the
   * frame constructor; it's only here because it uses gIconLoad for now.
   */
  static bool ShouldCreateImageFrameFor(const mozilla::dom::Element&,
                                        ComputedStyle&);

  ImgDrawResult DisplayAltFeedback(gfxContext& aRenderingContext,
                                   const nsRect& aDirtyRect, nsPoint aPt,
                                   uint32_t aFlags);

  ImgDrawResult DisplayAltFeedbackWithoutLayer(
      nsDisplayItem*, mozilla::wr::DisplayListBuilder&,
      mozilla::wr::IpcResourceUpdateQueue&,
      const mozilla::layers::StackingContextHelper&,
      mozilla::layers::RenderRootStateManager*, nsDisplayListBuilder*,
      nsPoint aPt, uint32_t aFlags);

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

  void AddInlineMinISize(gfxContext* aRenderingContext,
                         InlineMinISizeData* aData) final;

  void DisconnectMap();

  // nsIReflowCallback
  bool ReflowFinished() final;
  void ReflowCallbackCanceled() final;

  // The kind of image frame we are.
  enum class Kind : uint8_t {
    // For an nsImageLoadingContent.
    ImageElement,
    // For css 'content: url(..)' on non-generated content.
    ContentProperty,
    // For a child of a ::before / ::after pseudo-element that had an url() item
    // for the content property.
    ContentPropertyAtIndex,
    // For a list-style-image ::marker.
    ListStyleImage,
  };

  // Creates a suitable continuing frame for this frame.
  nsImageFrame* CreateContinuingFrame(mozilla::PresShell*,
                                      ComputedStyle*) const;

 private:
  friend nsIFrame* NS_NewImageFrame(mozilla::PresShell*, ComputedStyle*);
  friend nsIFrame* NS_NewImageFrameForContentProperty(mozilla::PresShell*,
                                                      ComputedStyle*);
  friend nsIFrame* NS_NewImageFrameForGeneratedContentIndex(mozilla::PresShell*,
                                                            ComputedStyle*);
  friend nsIFrame* NS_NewImageFrameForListStyleImage(mozilla::PresShell*,
                                                     ComputedStyle*);

  nsImageFrame(ComputedStyle* aStyle, nsPresContext* aPresContext, Kind aKind)
      : nsImageFrame(aStyle, aPresContext, kClassID, aKind) {}

  nsImageFrame(ComputedStyle*, nsPresContext* aPresContext, ClassID, Kind);

 protected:
  nsImageFrame(ComputedStyle* aStyle, nsPresContext* aPresContext, ClassID aID)
      : nsImageFrame(aStyle, aPresContext, aID, Kind::ImageElement) {}

  ~nsImageFrame() override;

  void EnsureIntrinsicSizeAndRatio();

  bool GotInitialReflow() const {
    return !HasAnyStateBits(NS_FRAME_FIRST_REFLOW);
  }

  SizeComputationResult ComputeSize(
      gfxContext* aRenderingContext, mozilla::WritingMode aWM,
      const mozilla::LogicalSize& aCBSize, nscoord aAvailableISize,
      const mozilla::LogicalSize& aMargin,
      const mozilla::LogicalSize& aBorderPadding,
      const mozilla::StyleSizeOverrides& aSizeOverrides,
      mozilla::ComputeSizeFlags aFlags) final;

  bool IsServerImageMap();

  void TranslateEventCoords(const nsPoint& aPoint, nsIntPoint& aResult);

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
  nscoord MeasureString(const char16_t* aString, int32_t aLength,
                        nscoord aMaxWidth, uint32_t& aMaxFit,
                        gfxContext& aContext, nsFontMetrics& aFontMetrics);

  void DisplayAltText(nsPresContext* aPresContext,
                      gfxContext& aRenderingContext, const nsString& aAltText,
                      const nsRect& aRect);

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

  /**
   * Is this frame part of a ::marker pseudo?
   */
  bool IsForMarkerPseudo() const;

 protected:
  friend class nsImageListener;
  friend class nsImageLoadingContent;
  friend class mozilla::PresShell;

  void OnSizeAvailable(imgIRequest* aRequest, imgIContainer* aImage);
  void OnFrameUpdate(imgIRequest* aRequest, const nsIntRect* aRect);
  void OnLoadComplete(imgIRequest* aRequest, nsresult aStatus);

  /**
   * Notification that aRequest will now be the current request.
   */
  void NotifyNewCurrentRequest(imgIRequest* aRequest, nsresult aStatus);

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
  void MaybeRecordContentUrlOnImageTelemetry();

  // random helpers
  inline void SpecToURI(const nsAString& aSpec, nsIURI** aURI);

  inline void GetLoadGroup(nsPresContext* aPresContext,
                           nsILoadGroup** aLoadGroup);
  nscoord GetContinuationOffset() const;
  void GetDocumentCharacterSet(nsACString& aCharset) const;
  bool ShouldDisplaySelection();

  // Whether the image frame should use the mapped aspect ratio from width=""
  // and height="".
  bool ShouldUseMappedAspectRatio() const;

  // Recalculate mIntrinsicSize from the image.
  bool UpdateIntrinsicSize();

  // Recalculate mIntrinsicRatio from the image.
  bool UpdateIntrinsicRatio();

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
  bool IsPendingLoad(imgIRequest*) const;

  /**
   * Updates mImage based on the current image request (cannot be null), and the
   * image passed in (can be null), and invalidate layout and paint as needed.
   */
  void UpdateImage(imgIRequest*, imgIContainer*);

  /**
   * Function to convert a dirty rect in the source image to a dirty
   * rect for the image frame.
   */
  nsRect SourceRectToDest(const nsIntRect& aRect);

  /**
   * Triggers invalidation for both our image display item and, if appropriate,
   * our alt-feedback display item.
   *
   * @param aLayerInvalidRect The area to invalidate in layer space. If null,
   * the entire layer will be invalidated.
   * @param aFrameInvalidRect The area to invalidate in frame space. If null,
   * the entire frame will be invalidated.
   */
  void InvalidateSelf(const nsIntRect* aLayerInvalidRect,
                      const nsRect* aFrameInvalidRect);

  RefPtr<nsImageMap> mImageMap;

  RefPtr<nsImageListener> mListener;

  // An image request created for content: url(..) or list-style-image.
  RefPtr<imgRequestProxy> mContentURLRequest;

  nsCOMPtr<imgIContainer> mImage;
  nsCOMPtr<imgIContainer> mPrevImage;
  nsSize mComputedSize;
  mozilla::IntrinsicSize mIntrinsicSize;

  // Stores mImage's intrinsic ratio, or a default AspectRatio if there's no
  // intrinsic ratio.
  mozilla::AspectRatio mIntrinsicRatio;

  const Kind mKind;
  bool mContentURLRequestRegistered;
  bool mDisplayingIcon;
  bool mFirstFrameComplete;
  bool mReflowCallbackPosted;
  bool mForceSyncDecoding;

  /* loading / broken image icon support */

  // XXXbz this should be handled by the prescontext, I think; that
  // way we would have a single iconload per mozilla session instead
  // of one per document...

  // LoadIcons: initiate the loading of the static icons used to show
  // loading / broken images
  nsresult LoadIcons(nsPresContext* aPresContext);
  nsresult LoadIcon(const nsAString& aSpec, nsPresContext* aPresContext,
                    imgRequestProxy** aRequest);

  class IconLoad final : public nsIObserver, public imgINotificationObserver {
    // private class that wraps the data and logic needed for
    // broken image and loading image icons
   public:
    IconLoad();

    void Shutdown();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
    NS_DECL_IMGINOTIFICATIONOBSERVER

    void AddIconObserver(nsImageFrame* frame) {
      MOZ_ASSERT(!mIconObservers.Contains(frame),
                 "Observer shouldn't aleady be in array");
      mIconObservers.AppendElement(frame);
    }

    void RemoveIconObserver(nsImageFrame* frame) {
      mozilla::DebugOnly<bool> didRemove = mIconObservers.RemoveElement(frame);
      MOZ_ASSERT(didRemove, "Observer not in array");
    }

   private:
    ~IconLoad() = default;

    void GetPrefs();
    nsTObserverArray<nsImageFrame*> mIconObservers;

   public:
    RefPtr<imgRequestProxy> mLoadingImage;
    RefPtr<imgRequestProxy> mBrokenImage;
    bool mPrefForceInlineAltText;
    bool mPrefShowPlaceholders;
    bool mPrefShowLoadingPlaceholder;
  };

 public:
  // singleton pattern: one LoadIcons instance is used
  static mozilla::StaticRefPtr<IconLoad> gIconLoad;

  friend class mozilla::nsDisplayImage;
  friend class nsDisplayGradient;
};

namespace mozilla {
/**
 * Note that nsDisplayImage does not receive events. However, an image element
 * is replaced content so its background will be z-adjacent to the
 * image itself, and hence receive events just as if the image itself
 * received events.
 */
class nsDisplayImage final : public nsDisplayImageContainer {
 public:
  typedef mozilla::layers::LayerManager LayerManager;

  nsDisplayImage(nsDisplayListBuilder* aBuilder, nsImageFrame* aFrame,
                 imgIContainer* aImage, imgIContainer* aPrevImage)
      : nsDisplayImageContainer(aBuilder, aFrame),
        mImage(aImage),
        mPrevImage(aPrevImage) {
    MOZ_COUNT_CTOR(nsDisplayImage);
  }
  ~nsDisplayImage() final { MOZ_COUNT_DTOR(nsDisplayImage); }

  nsDisplayItemGeometry* AllocateGeometry(nsDisplayListBuilder*) final;
  void ComputeInvalidationRegion(nsDisplayListBuilder*,
                                 const nsDisplayItemGeometry*,
                                 nsRegion* aInvalidRegion) const final;
  void Paint(nsDisplayListBuilder*, gfxContext* aCtx) final;

  already_AddRefed<imgIContainer> GetImage() final;

  /**
   * @return The dest rect we'll use when drawing this image, in app units.
   *         Not necessarily contained in this item's bounds.
   */
  nsRect GetDestRect() const final;

  void UpdateDrawResult(mozilla::image::ImgDrawResult aResult) final {
    nsDisplayItemGenericImageGeometry::UpdateDrawResult(this, aResult);
  }

  LayerState GetLayerState(nsDisplayListBuilder*, LayerManager*,
                           const ContainerLayerParameters&) final;
  nsRect GetBounds(bool* aSnap) const {
    *aSnap = true;

    nsImageFrame* imageFrame = static_cast<nsImageFrame*>(mFrame);
    return imageFrame->GetInnerArea() + ToReferenceFrame();
  }

  nsRect GetBounds(nsDisplayListBuilder*, bool* aSnap) const final {
    return GetBounds(aSnap);
  }

  nsRegion GetOpaqueRegion(nsDisplayListBuilder*, bool* aSnap) const final;

  already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder*, LayerManager*,
                                     const ContainerLayerParameters&) final;
  bool CreateWebRenderCommands(mozilla::wr::DisplayListBuilder&,
                               mozilla::wr::IpcResourceUpdateQueue&,
                               const StackingContextHelper&,
                               mozilla::layers::RenderRootStateManager*,
                               nsDisplayListBuilder*) final;

  NS_DISPLAY_DECL_NAME("Image", TYPE_IMAGE)
 private:
  nsCOMPtr<imgIContainer> mImage;
  nsCOMPtr<imgIContainer> mPrevImage;
};

}  // namespace mozilla

#endif /* nsImageFrame_h___ */
