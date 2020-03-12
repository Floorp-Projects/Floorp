/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsImageBoxFrame_h___
#define nsImageBoxFrame_h___

#include "mozilla/Attributes.h"
#include "nsLeafBoxFrame.h"

#include "imgIRequest.h"
#include "imgIContainer.h"
#include "imgINotificationObserver.h"

class imgRequestProxy;
class nsImageBoxFrame;

class nsDisplayXULImage;

namespace mozilla {
class PresShell;
}  // namespace mozilla

class nsImageBoxListener final : public imgINotificationObserver {
 public:
  explicit nsImageBoxListener(nsImageBoxFrame* frame);

  NS_DECL_ISUPPORTS
  NS_DECL_IMGINOTIFICATIONOBSERVER

  void ClearFrame() { mFrame = nullptr; }

 private:
  virtual ~nsImageBoxListener();

  nsImageBoxFrame* mFrame;
};

class nsImageBoxFrame final : public nsLeafBoxFrame {
 public:
  typedef mozilla::image::ImgDrawResult ImgDrawResult;
  typedef mozilla::layers::ImageContainer ImageContainer;
  typedef mozilla::layers::LayerManager LayerManager;

  friend class nsDisplayXULImage;
  NS_DECL_FRAMEARENA_HELPERS(nsImageBoxFrame)
  NS_DECL_QUERYFRAME

  virtual nsSize GetXULPrefSize(nsBoxLayoutState& aBoxLayoutState) override;
  virtual nsSize GetXULMinSize(nsBoxLayoutState& aBoxLayoutState) override;
  virtual nscoord GetXULBoxAscent(nsBoxLayoutState& aBoxLayoutState) override;
  virtual void MarkIntrinsicISizesDirty() override;

  nsresult Notify(imgIRequest* aRequest, int32_t aType, const nsIntRect* aData);

  friend nsIFrame* NS_NewImageBoxFrame(mozilla::PresShell* aPresShell,
                                       ComputedStyle* aStyle);

  virtual void Init(nsIContent* aContent, nsContainerFrame* aParent,
                    nsIFrame* asPrevInFlow) override;

  virtual nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                                    int32_t aModType) override;

  virtual void DidSetComputedStyle(ComputedStyle* aOldStyle) override;

  virtual void DestroyFrom(nsIFrame* aDestructRoot,
                           PostDestroyData& aPostDestroyData) override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif

  /**
   * Gets the image request to be loaded from the current style.
   *
   * May be null if themed.
   */
  imgRequestProxy* GetRequestFromStyle();

  /**
   * Update mUseSrcAttr from appropriate content attributes or from
   * style, throw away the current image, and load the appropriate
   * image.
   * */
  void UpdateImage();

  /**
   * Update mLoadFlags from content attributes. Does not attempt to reload the
   * image using the new load flags.
   */
  void UpdateLoadFlags();

  void RestartAnimation();
  void StopAnimation();

  virtual void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                const nsDisplayListSet& aLists) override;

  virtual ~nsImageBoxFrame();

  already_AddRefed<imgIContainer> GetImageContainerForPainting(
      const nsPoint& aPt, ImgDrawResult& aDrawResult,
      Maybe<nsPoint>& aAnchorPoint, nsRect& aDest);

  ImgDrawResult PaintImage(gfxContext& aRenderingContext,
                           const nsRect& aDirtyRect, nsPoint aPt,
                           uint32_t aFlags);

  ImgDrawResult CreateWebRenderCommands(
      mozilla::wr::DisplayListBuilder& aBuilder,
      mozilla::wr::IpcResourceUpdateQueue& aResources,
      const mozilla::layers::StackingContextHelper& aSc,
      mozilla::layers::RenderRootStateManager* aManager, nsDisplayItem* aItem,
      nsPoint aPt, uint32_t aFlags);

  bool CanOptimizeToImageLayer();

  nsRect GetDestRect(const nsPoint& aOffset, Maybe<nsPoint>& aAnchorPoint);

 protected:
  explicit nsImageBoxFrame(ComputedStyle* aStyle, nsPresContext* aPresContext);

  virtual void GetImageSize();

 private:
  nsresult OnSizeAvailable(imgIRequest* aRequest, imgIContainer* aImage);
  nsresult OnDecodeComplete(imgIRequest* aRequest);
  nsresult OnLoadComplete(imgIRequest* aRequest, nsresult aStatus);
  nsresult OnImageIsAnimated(imgIRequest* aRequest);
  nsresult OnFrameUpdate(imgIRequest* aRequest);

  nsRect mSubRect;  ///< If set, indicates that only the portion of the image
                    ///< specified by the rect should be used.
  nsSize mIntrinsicSize;
  nsSize mImageSize;

  RefPtr<imgRequestProxy> mImageRequest;
  nsCOMPtr<imgINotificationObserver> mListener;

  int32_t mLoadFlags;

  // Boolean variable to determine if the current image request has been
  // registered with the refresh driver.
  bool mRequestRegistered;

  bool mUseSrcAttr;  ///< Whether or not the image src comes from an attribute.
  bool mSuppressStyleCheck;
};  // class nsImageBoxFrame

class nsDisplayXULImage final : public nsDisplayImageContainer {
 public:
  nsDisplayXULImage(nsDisplayListBuilder* aBuilder, nsImageBoxFrame* aFrame)
      : nsDisplayImageContainer(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(nsDisplayXULImage);
  }
  MOZ_COUNTED_DTOR_OVERRIDE(nsDisplayXULImage)

  virtual bool CanOptimizeToImageLayer(LayerManager* aManager,
                                       nsDisplayListBuilder* aBuilder) override;
  virtual already_AddRefed<imgIContainer> GetImage() override;
  virtual nsRect GetDestRect() const override;
  virtual void UpdateDrawResult(
      mozilla::image::ImgDrawResult aResult) override {
    nsDisplayItemGenericImageGeometry::UpdateDrawResult(this, aResult);
  }
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) const override {
    *aSnap = true;
    return nsRect(ToReferenceFrame(), Frame()->GetSize());
  }
  virtual nsDisplayItemGeometry* AllocateGeometry(
      nsDisplayListBuilder* aBuilder) override;
  virtual void ComputeInvalidationRegion(
      nsDisplayListBuilder* aBuilder, const nsDisplayItemGeometry* aGeometry,
      nsRegion* aInvalidRegion) const override;
  // Doesn't handle HitTest because nsLeafBoxFrame already creates an
  // event receiver for us
  virtual void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;

  virtual bool CreateWebRenderCommands(
      mozilla::wr::DisplayListBuilder& aBuilder,
      mozilla::wr::IpcResourceUpdateQueue& aResources,
      const StackingContextHelper& aSc,
      mozilla::layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override;

  NS_DISPLAY_DECL_NAME("XULImage", TYPE_XUL_IMAGE)
};

#endif /* nsImageBoxFrame_h___ */
