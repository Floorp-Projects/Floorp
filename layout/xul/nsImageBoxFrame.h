/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsImageBoxFrame_h___
#define nsImageBoxFrame_h___

#include "mozilla/Attributes.h"
#include "nsLeafBoxFrame.h"

#include "imgILoader.h"
#include "imgIRequest.h"
#include "imgIContainer.h"
#include "imgINotificationObserver.h"
#include "imgIOnloadBlocker.h"

class imgRequestProxy;
class nsImageBoxFrame;

class nsDisplayXULImage;

class nsImageBoxListener final : public imgINotificationObserver,
                                 public imgIOnloadBlocker
{
public:
  nsImageBoxListener();

  NS_DECL_ISUPPORTS
  NS_DECL_IMGINOTIFICATIONOBSERVER
  NS_DECL_IMGIONLOADBLOCKER

  void SetFrame(nsImageBoxFrame *frame) { mFrame = frame; }

private:
  virtual ~nsImageBoxListener();

  nsImageBoxFrame *mFrame;
};

class nsImageBoxFrame final : public nsLeafBoxFrame
{
public:
  typedef mozilla::image::DrawResult DrawResult;
  typedef mozilla::layers::ImageContainer ImageContainer;
  typedef mozilla::layers::LayerManager LayerManager;

  friend class nsDisplayXULImage;
  NS_DECL_FRAMEARENA_HELPERS

  virtual nsSize GetPrefSize(nsBoxLayoutState& aBoxLayoutState) override;
  virtual nsSize GetMinSize(nsBoxLayoutState& aBoxLayoutState) override;
  virtual nscoord GetBoxAscent(nsBoxLayoutState& aBoxLayoutState) override;
  virtual void MarkIntrinsicISizesDirty() override;

  nsresult Notify(imgIRequest *aRequest, int32_t aType, const nsIntRect* aData);

  friend nsIFrame* NS_NewImageBoxFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         asPrevInFlow) override;

  virtual nsresult AttributeChanged(int32_t aNameSpaceID,
                                    nsIAtom* aAttribute,
                                    int32_t aModType) override;

  virtual void DidSetStyleContext(nsStyleContext* aOldStyleContext) override;

  virtual void DestroyFrom(nsIFrame* aDestructRoot) override;

  virtual nsIAtom* GetType() const override;
#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif

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

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override;

  virtual ~nsImageBoxFrame();

  DrawResult PaintImage(nsRenderingContext& aRenderingContext,
                        const nsRect& aDirtyRect,
                        nsPoint aPt, uint32_t aFlags);

  bool IsImageContainerAvailable(LayerManager* aManager, uint32_t aFlags);
  already_AddRefed<ImageContainer> GetContainer(LayerManager* aManager,
                                                uint32_t aFlags);

protected:
  explicit nsImageBoxFrame(nsStyleContext* aContext);

  virtual void GetImageSize();

private:
  nsresult OnSizeAvailable(imgIRequest* aRequest, imgIContainer* aImage);
  nsresult OnDecodeComplete(imgIRequest* aRequest);
  nsresult OnLoadComplete(imgIRequest* aRequest, nsresult aStatus);
  nsresult OnImageIsAnimated(imgIRequest* aRequest);
  nsresult OnFrameUpdate(imgIRequest* aRequest);

  nsRect mSubRect; ///< If set, indicates that only the portion of the image specified by the rect should be used.
  nsSize mIntrinsicSize;
  nsSize mImageSize;

  nsRefPtr<imgRequestProxy> mImageRequest;
  nsCOMPtr<imgINotificationObserver> mListener;

  int32_t mLoadFlags;

  // Boolean variable to determine if the current image request has been
  // registered with the refresh driver.
  bool mRequestRegistered;

  bool mUseSrcAttr; ///< Whether or not the image src comes from an attribute.
  bool mSuppressStyleCheck;
  bool mFireEventOnDecode;
}; // class nsImageBoxFrame

class nsDisplayXULImage : public nsDisplayImageContainer {
public:
  nsDisplayXULImage(nsDisplayListBuilder* aBuilder,
                    nsImageBoxFrame* aFrame) :
    nsDisplayImageContainer(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(nsDisplayXULImage);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayXULImage() {
    MOZ_COUNT_DTOR(nsDisplayXULImage);
  }
#endif

  virtual bool CanOptimizeToImageLayer(LayerManager* aManager,
                                       nsDisplayListBuilder* aBuilder) override;
  virtual already_AddRefed<ImageContainer> GetContainer(LayerManager* aManager,
                                                        nsDisplayListBuilder* aBuilder) override;
  virtual void ConfigureLayer(ImageLayer* aLayer,
                              const ContainerLayerParameters& aParameters) override;
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) override
  {
    *aSnap = true;
    return nsRect(ToReferenceFrame(), Frame()->GetSize());
  }
  virtual nsDisplayItemGeometry* AllocateGeometry(nsDisplayListBuilder* aBuilder) override;
  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion) override;
  // Doesn't handle HitTest because nsLeafBoxFrame already creates an
  // event receiver for us
  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext* aCtx) override;
  NS_DISPLAY_DECL_NAME("XULImage", TYPE_XUL_IMAGE)
};

#endif /* nsImageBoxFrame_h___ */
