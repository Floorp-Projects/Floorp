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
#include "nsStubImageDecoderObserver.h"

class nsImageBoxFrame;

class nsDisplayXULImage;

class nsImageBoxListener : public nsStubImageDecoderObserver
{
public:
  nsImageBoxListener();
  virtual ~nsImageBoxListener();

  NS_DECL_ISUPPORTS
  // imgIDecoderObserver (override nsStubImageDecoderObserver)
  NS_IMETHOD OnStartContainer(imgIRequest *request, imgIContainer *image);
  NS_IMETHOD OnStopContainer(imgIRequest *request, imgIContainer *image);
  NS_IMETHOD OnStopDecode(imgIRequest *request, nsresult status,
                          const PRUnichar *statusArg);
  NS_IMETHOD OnImageIsAnimated(imgIRequest* aRequest);

  // imgIContainerObserver (override nsStubImageDecoderObserver)
  NS_IMETHOD FrameChanged(imgIRequest *aRequest,
                          imgIContainer *aContainer,
                          const nsIntRect *aDirtyRect);

  void SetFrame(nsImageBoxFrame *frame) { mFrame = frame; }

private:
  nsImageBoxFrame *mFrame;
};

class nsImageBoxFrame : public nsLeafBoxFrame
{
public:
  friend class nsDisplayXULImage;
  NS_DECL_FRAMEARENA_HELPERS

  virtual nsSize GetPrefSize(nsBoxLayoutState& aBoxLayoutState);
  virtual nsSize GetMinSize(nsBoxLayoutState& aBoxLayoutState);
  virtual nscoord GetBoxAscent(nsBoxLayoutState& aBoxLayoutState) MOZ_OVERRIDE;
  virtual void MarkIntrinsicWidthsDirty() MOZ_OVERRIDE;

  friend nsIFrame* NS_NewImageBoxFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  NS_IMETHOD  Init(nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsIFrame*        asPrevInFlow) MOZ_OVERRIDE;

  NS_IMETHOD AttributeChanged(int32_t aNameSpaceID,
                              nsIAtom* aAttribute,
                              int32_t aModType) MOZ_OVERRIDE;

  virtual void DidSetStyleContext(nsStyleContext* aOldStyleContext) MOZ_OVERRIDE;

  virtual void DestroyFrom(nsIFrame* aDestructRoot);

  virtual nsIAtom* GetType() const MOZ_OVERRIDE;
#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const MOZ_OVERRIDE;
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

  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists) MOZ_OVERRIDE;

  NS_IMETHOD OnStartContainer(imgIRequest *request, imgIContainer *image);
  NS_IMETHOD OnStopContainer(imgIRequest *request, imgIContainer *image);
  NS_IMETHOD OnStopDecode(imgIRequest *request,
                          nsresult status,
                          const PRUnichar *statusArg);
  NS_IMETHOD OnImageIsAnimated(imgIRequest* aRequest);

  NS_IMETHOD FrameChanged(imgIRequest *aRequest,
                          imgIContainer *aContainer,
                          const nsIntRect *aDirtyRect);

  virtual ~nsImageBoxFrame();

  void  PaintImage(nsRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect,
                   nsPoint aPt, uint32_t aFlags);

  already_AddRefed<mozilla::layers::ImageContainer> GetContainer();
protected:
  nsImageBoxFrame(nsIPresShell* aShell, nsStyleContext* aContext);

  virtual void GetImageSize();

private:

  nsRect mSubRect; ///< If set, indicates that only the portion of the image specified by the rect should be used.
  nsSize mIntrinsicSize;
  nsSize mImageSize;

  // Boolean variable to determine if the current image request has been
  // registered with the refresh driver.
  bool mRequestRegistered;

  nsCOMPtr<imgIRequest> mImageRequest;
  nsCOMPtr<imgIDecoderObserver> mListener;

  int32_t mLoadFlags;

  bool mUseSrcAttr; ///< Whether or not the image src comes from an attribute.
  bool mSuppressStyleCheck;
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

  virtual already_AddRefed<ImageContainer> GetContainer() MOZ_OVERRIDE;
  virtual void ConfigureLayer(ImageLayer* aLayer, const nsIntPoint& aOffset) MOZ_OVERRIDE;

  // Doesn't handle HitTest because nsLeafBoxFrame already creates an
  // event receiver for us
  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext* aCtx);
  NS_DISPLAY_DECL_NAME("XULImage", TYPE_XUL_IMAGE)
};

#endif /* nsImageBoxFrame_h___ */
