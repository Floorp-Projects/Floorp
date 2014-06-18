/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsImageBoxFrame.h"
#include "nsGkAtoms.h"
#include "nsStyleContext.h"
#include "nsStyleConsts.h"
#include "nsCOMPtr.h"
#include "nsPresContext.h"
#include "nsBoxLayoutState.h"

#include "nsHTMLParts.h"
#include "nsString.h"
#include "nsLeafFrame.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"
#include "nsImageMap.h"
#include "nsILinkHandler.h"
#include "nsIURL.h"
#include "nsILoadGroup.h"
#include "nsContainerFrame.h"
#include "prprf.h"
#include "nsCSSRendering.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsNameSpaceManager.h"
#include "nsTextFragment.h"
#include "nsIDOMHTMLMapElement.h"
#include "nsTransform2D.h"
#include "nsITheme.h"

#include "nsIServiceManager.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"
#include "nsDisplayList.h"
#include "ImageLayers.h"
#include "ImageContainer.h"

#include "nsContentUtils.h"

#include "mozilla/BasicEvents.h"
#include "mozilla/EventDispatcher.h"

#define ONLOAD_CALLED_TOO_EARLY 1

using namespace mozilla;
using namespace mozilla::layers;

class nsImageBoxFrameEvent : public nsRunnable
{
public:
  nsImageBoxFrameEvent(nsIContent *content, uint32_t message)
    : mContent(content), mMessage(message) {}

  NS_IMETHOD Run() MOZ_OVERRIDE;

private:
  nsCOMPtr<nsIContent> mContent;
  uint32_t mMessage;
};

NS_IMETHODIMP
nsImageBoxFrameEvent::Run()
{
  nsIPresShell *pres_shell = mContent->OwnerDoc()->GetShell();
  if (!pres_shell) {
    return NS_OK;
  }

  nsRefPtr<nsPresContext> pres_context = pres_shell->GetPresContext();
  if (!pres_context) {
    return NS_OK;
  }

  nsEventStatus status = nsEventStatus_eIgnore;
  WidgetEvent event(true, mMessage);

  event.mFlags.mBubbles = false;
  EventDispatcher::Dispatch(mContent, pres_context, &event, nullptr, &status);
  return NS_OK;
}

// Fire off an event that'll asynchronously call the image elements
// onload handler once handled. This is needed since the image library
// can't decide if it wants to call it's observer methods
// synchronously or asynchronously. If an image is loaded from the
// cache the notifications come back synchronously, but if the image
// is loaded from the netswork the notifications come back
// asynchronously.

void
FireImageDOMEvent(nsIContent* aContent, uint32_t aMessage)
{
  NS_ASSERTION(aMessage == NS_LOAD || aMessage == NS_LOAD_ERROR,
               "invalid message");

  nsCOMPtr<nsIRunnable> event = new nsImageBoxFrameEvent(aContent, aMessage);
  if (NS_FAILED(NS_DispatchToCurrentThread(event)))
    NS_WARNING("failed to dispatch image event");
}

//
// NS_NewImageBoxFrame
//
// Creates a new image frame and returns it
//
nsIFrame*
NS_NewImageBoxFrame (nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsImageBoxFrame (aPresShell, aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsImageBoxFrame)

nsresult
nsImageBoxFrame::AttributeChanged(int32_t aNameSpaceID,
                                  nsIAtom* aAttribute,
                                  int32_t aModType)
{
  nsresult rv = nsLeafBoxFrame::AttributeChanged(aNameSpaceID, aAttribute,
                                                 aModType);

  if (aAttribute == nsGkAtoms::src) {
    UpdateImage();
    PresContext()->PresShell()->
      FrameNeedsReflow(this, nsIPresShell::eStyleChange, NS_FRAME_IS_DIRTY);
  }
  else if (aAttribute == nsGkAtoms::validate)
    UpdateLoadFlags();

  return rv;
}

nsImageBoxFrame::nsImageBoxFrame(nsIPresShell* aShell, nsStyleContext* aContext):
  nsLeafBoxFrame(aShell, aContext),
  mIntrinsicSize(0,0),
  mRequestRegistered(false),
  mLoadFlags(nsIRequest::LOAD_NORMAL),
  mUseSrcAttr(false),
  mSuppressStyleCheck(false),
  mFireEventOnDecode(false)
{
  MarkIntrinsicWidthsDirty();
}

nsImageBoxFrame::~nsImageBoxFrame()
{
}


/* virtual */ void
nsImageBoxFrame::MarkIntrinsicWidthsDirty()
{
  SizeNeedsRecalc(mImageSize);
  nsLeafBoxFrame::MarkIntrinsicWidthsDirty();
}

void
nsImageBoxFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  if (mImageRequest) {
    nsLayoutUtils::DeregisterImageRequest(PresContext(), mImageRequest,
                                          &mRequestRegistered);

    // Release image loader first so that it's refcnt can go to zero
    mImageRequest->CancelAndForgetObserver(NS_ERROR_FAILURE);
  }

  if (mListener)
    reinterpret_cast<nsImageBoxListener*>(mListener.get())->SetFrame(nullptr); // set the frame to null so we don't send messages to a dead object.

  nsLeafBoxFrame::DestroyFrom(aDestructRoot);
}


void
nsImageBoxFrame::Init(nsIContent*       aContent,
                      nsContainerFrame* aParent,
                      nsIFrame*         aPrevInFlow)
{
  if (!mListener) {
    nsImageBoxListener *listener = new nsImageBoxListener();
    NS_ADDREF(listener);
    listener->SetFrame(this);
    listener->QueryInterface(NS_GET_IID(imgINotificationObserver), getter_AddRefs(mListener));
    NS_RELEASE(listener);
  }

  mSuppressStyleCheck = true;
  nsLeafBoxFrame::Init(aContent, aParent, aPrevInFlow);
  mSuppressStyleCheck = false;

  UpdateLoadFlags();
  UpdateImage();
}

void
nsImageBoxFrame::UpdateImage()
{
  nsPresContext* presContext = PresContext();

  if (mImageRequest) {
    nsLayoutUtils::DeregisterImageRequest(presContext, mImageRequest,
                                          &mRequestRegistered);
    mImageRequest->CancelAndForgetObserver(NS_ERROR_FAILURE);
    mImageRequest = nullptr;
  }

  // get the new image src
  nsAutoString src;
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::src, src);
  mUseSrcAttr = !src.IsEmpty();
  if (mUseSrcAttr) {
    nsIDocument* doc = mContent->GetDocument();
    if (!doc) {
      // No need to do anything here...
      return;
    }
    nsCOMPtr<nsIURI> baseURI = mContent->GetBaseURI();
    nsCOMPtr<nsIURI> uri;
    nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(uri),
                                              src,
                                              doc,
                                              baseURI);

    if (uri && nsContentUtils::CanLoadImage(uri, mContent, doc,
                                            mContent->NodePrincipal())) {
      nsContentUtils::LoadImage(uri, doc, mContent->NodePrincipal(),
                                doc->GetDocumentURI(), mListener, mLoadFlags,
                                EmptyString(), getter_AddRefs(mImageRequest));

      if (mImageRequest) {
        nsLayoutUtils::RegisterImageRequestIfAnimated(presContext,
                                                      mImageRequest,
                                                      &mRequestRegistered);
      }
    }
  } else {
    // Only get the list-style-image if we aren't being drawn
    // by a native theme.
    uint8_t appearance = StyleDisplay()->mAppearance;
    if (!(appearance && nsBox::gTheme &&
          nsBox::gTheme->ThemeSupportsWidget(nullptr, this, appearance))) {
      // get the list-style-image
      imgRequestProxy *styleRequest = StyleList()->GetListStyleImage();
      if (styleRequest) {
        styleRequest->Clone(mListener, getter_AddRefs(mImageRequest));
      }
    }
  }

  if (!mImageRequest) {
    // We have no image, so size to 0
    mIntrinsicSize.SizeTo(0, 0);
  } else {
    // We don't want discarding or decode-on-draw for xul images.
    mImageRequest->StartDecoding();
    mImageRequest->LockImage();
  }
}

void
nsImageBoxFrame::UpdateLoadFlags()
{
  static nsIContent::AttrValuesArray strings[] =
    {&nsGkAtoms::always, &nsGkAtoms::never, nullptr};
  switch (mContent->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::validate,
                                    strings, eCaseMatters)) {
    case 0:
      mLoadFlags = nsIRequest::VALIDATE_ALWAYS;
      break;
    case 1:
      mLoadFlags = nsIRequest::VALIDATE_NEVER|nsIRequest::LOAD_FROM_CACHE;
      break;
    default:
      mLoadFlags = nsIRequest::LOAD_NORMAL;
      break;
  }
}

void
nsImageBoxFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                  const nsRect&           aDirtyRect,
                                  const nsDisplayListSet& aLists)
{
  nsLeafBoxFrame::BuildDisplayList(aBuilder, aDirtyRect, aLists);

  if ((0 == mRect.width) || (0 == mRect.height)) {
    // Do not render when given a zero area. This avoids some useless
    // scaling work while we wait for our image dimensions to arrive
    // asynchronously.
    return;
  }

  if (!IsVisibleForPainting(aBuilder))
    return;

  nsDisplayList list;
  list.AppendNewToTop(
    new (aBuilder) nsDisplayXULImage(aBuilder, this));

  CreateOwnLayerIfNeeded(aBuilder, &list);

  aLists.Content()->AppendToTop(&list);
}

void
nsImageBoxFrame::PaintImage(nsRenderingContext& aRenderingContext,
                            const nsRect& aDirtyRect, nsPoint aPt,
                            uint32_t aFlags)
{
  nsRect rect;
  GetClientRect(rect);

  rect += aPt;

  if (!mImageRequest)
    return;

  // don't draw if the image is not dirty
  nsRect dirty;
  if (!dirty.IntersectRect(aDirtyRect, rect))
    return;

  nsCOMPtr<imgIContainer> imgCon;
  mImageRequest->GetImage(getter_AddRefs(imgCon));

  if (imgCon) {
    bool hasSubRect = !mUseSrcAttr && (mSubRect.width > 0 || mSubRect.height > 0);
    nsLayoutUtils::DrawSingleImage(&aRenderingContext, PresContext(), imgCon,
        nsLayoutUtils::GetGraphicsFilterForFrame(this),
        rect, dirty, nullptr, aFlags, hasSubRect ? &mSubRect : nullptr);
  }
}

void nsDisplayXULImage::Paint(nsDisplayListBuilder* aBuilder,
                              nsRenderingContext* aCtx)
{
  uint32_t flags = imgIContainer::FLAG_NONE;
  if (aBuilder->ShouldSyncDecodeImages())
    flags |= imgIContainer::FLAG_SYNC_DECODE;
  if (aBuilder->IsPaintingToWindow())
    flags |= imgIContainer::FLAG_HIGH_QUALITY_SCALING;

  static_cast<nsImageBoxFrame*>(mFrame)->
    PaintImage(*aCtx, mVisibleRect, ToReferenceFrame(), flags);
}

void
nsDisplayXULImage::ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                             const nsDisplayItemGeometry* aGeometry,
                                             nsRegion* aInvalidRegion)
{

  if (aBuilder->ShouldSyncDecodeImages()) {
    nsImageBoxFrame* boxFrame = static_cast<nsImageBoxFrame*>(mFrame);
    nsCOMPtr<imgIContainer> image;
    if (boxFrame->mImageRequest) {
      boxFrame->mImageRequest->GetImage(getter_AddRefs(image));
    }

    if  (image && !image->IsDecoded()) {
      bool snap;
      aInvalidRegion->Or(*aInvalidRegion, GetBounds(aBuilder, &snap));
    }
  }

  nsDisplayImageContainer::ComputeInvalidationRegion(aBuilder, aGeometry, aInvalidRegion);
}

void
nsDisplayXULImage::ConfigureLayer(ImageLayer* aLayer, const nsIntPoint& aOffset)
{
  aLayer->SetFilter(nsLayoutUtils::GetGraphicsFilterForFrame(mFrame));

  int32_t factor = mFrame->PresContext()->AppUnitsPerDevPixel();
  nsImageBoxFrame* imageFrame = static_cast<nsImageBoxFrame*>(mFrame);

  nsRect dest;
  imageFrame->GetClientRect(dest);
  dest += ToReferenceFrame();
  gfxRect destRect(dest.x, dest.y, dest.width, dest.height);
  destRect.ScaleInverse(factor); 

  nsCOMPtr<imgIContainer> imgCon;
  imageFrame->mImageRequest->GetImage(getter_AddRefs(imgCon));
  int32_t imageWidth;
  int32_t imageHeight;
  imgCon->GetWidth(&imageWidth);
  imgCon->GetHeight(&imageHeight);

  NS_ASSERTION(imageWidth != 0 && imageHeight != 0, "Invalid image size!");

  gfxPoint p = destRect.TopLeft() + aOffset;
  gfx::Matrix transform;
  transform.Translate(p.x, p.y);
  transform.Scale(destRect.Width()/imageWidth,
                  destRect.Height()/imageHeight);
  aLayer->SetBaseTransform(gfx::Matrix4x4::From2D(transform));
}

already_AddRefed<ImageContainer>
nsDisplayXULImage::GetContainer(LayerManager* aManager, nsDisplayListBuilder* aBuilder)
{
  return static_cast<nsImageBoxFrame*>(mFrame)->GetContainer(aManager);
}

already_AddRefed<ImageContainer>
nsImageBoxFrame::GetContainer(LayerManager* aManager)
{
  bool hasSubRect = !mUseSrcAttr && (mSubRect.width > 0 || mSubRect.height > 0);
  if (hasSubRect || !mImageRequest) {
    return nullptr;
  }

  nsCOMPtr<imgIContainer> imgCon;
  mImageRequest->GetImage(getter_AddRefs(imgCon));
  if (!imgCon) {
    return nullptr;
  }
  
  nsRefPtr<ImageContainer> container;
  nsresult rv = imgCon->GetImageContainer(aManager, getter_AddRefs(container));
  NS_ENSURE_SUCCESS(rv, nullptr);
  return container.forget();
}


//
// DidSetStyleContext
//
// When the style context changes, make sure that all of our image is up to date.
//
/* virtual */ void
nsImageBoxFrame::DidSetStyleContext(nsStyleContext* aOldStyleContext)
{
  nsLeafBoxFrame::DidSetStyleContext(aOldStyleContext);

  // Fetch our subrect.
  const nsStyleList* myList = StyleList();
  mSubRect = myList->mImageRegion; // before |mSuppressStyleCheck| test!

  if (mUseSrcAttr || mSuppressStyleCheck)
    return; // No more work required, since the image isn't specified by style.

  // If we're using a native theme implementation, we shouldn't draw anything.
  const nsStyleDisplay* disp = StyleDisplay();
  if (disp->mAppearance && nsBox::gTheme &&
      nsBox::gTheme->ThemeSupportsWidget(nullptr, this, disp->mAppearance))
    return;

  // If list-style-image changes, we have a new image.
  nsCOMPtr<nsIURI> oldURI, newURI;
  if (mImageRequest)
    mImageRequest->GetURI(getter_AddRefs(oldURI));
  if (myList->GetListStyleImage())
    myList->GetListStyleImage()->GetURI(getter_AddRefs(newURI));
  bool equal;
  if (newURI == oldURI ||   // handles null==null
      (newURI && oldURI &&
       NS_SUCCEEDED(newURI->Equals(oldURI, &equal)) && equal))
    return;

  UpdateImage();
} // DidSetStyleContext

void
nsImageBoxFrame::GetImageSize()
{
  if (mIntrinsicSize.width > 0 && mIntrinsicSize.height > 0) {
    mImageSize.width = mIntrinsicSize.width;
    mImageSize.height = mIntrinsicSize.height;
  } else {
    mImageSize.width = 0;
    mImageSize.height = 0;
  }
}

/**
 * Ok return our dimensions
 */
nsSize
nsImageBoxFrame::GetPrefSize(nsBoxLayoutState& aState)
{
  nsSize size(0,0);
  DISPLAY_PREF_SIZE(this, size);
  if (DoesNeedRecalc(mImageSize))
     GetImageSize();

  if (!mUseSrcAttr && (mSubRect.width > 0 || mSubRect.height > 0))
    size = mSubRect.Size();
  else
    size = mImageSize;

  nsSize intrinsicSize = size;

  nsMargin borderPadding(0,0,0,0);
  GetBorderAndPadding(borderPadding);
  size.width += borderPadding.LeftRight();
  size.height += borderPadding.TopBottom();

  bool widthSet, heightSet;
  nsIFrame::AddCSSPrefSize(this, size, widthSet, heightSet);
  NS_ASSERTION(size.width != NS_INTRINSICSIZE && size.height != NS_INTRINSICSIZE,
               "non-intrinsic size expected");

  nsSize minSize = GetMinSize(aState);
  nsSize maxSize = GetMaxSize(aState);

  if (!widthSet && !heightSet) {
    if (minSize.width != NS_INTRINSICSIZE)
      minSize.width -= borderPadding.LeftRight();
    if (minSize.height != NS_INTRINSICSIZE)
      minSize.height -= borderPadding.TopBottom();
    if (maxSize.width != NS_INTRINSICSIZE)
      maxSize.width -= borderPadding.LeftRight();
    if (maxSize.height != NS_INTRINSICSIZE)
      maxSize.height -= borderPadding.TopBottom();

    size = nsLayoutUtils::ComputeAutoSizeWithIntrinsicDimensions(minSize.width, minSize.height,
                                                                 maxSize.width, maxSize.height,
                                                                 intrinsicSize.width, intrinsicSize.height);
    NS_ASSERTION(size.width != NS_INTRINSICSIZE && size.height != NS_INTRINSICSIZE,
                 "non-intrinsic size expected");
    size.width += borderPadding.LeftRight();
    size.height += borderPadding.TopBottom();
    return size;
  }

  if (!widthSet) {
    if (intrinsicSize.height > 0) {
      // Subtract off the border and padding from the height because the
      // content-box needs to be used to determine the ratio
      nscoord height = size.height - borderPadding.TopBottom();
      size.width = nscoord(int64_t(height) * int64_t(intrinsicSize.width) /
                           int64_t(intrinsicSize.height));
    }
    else {
      size.width = intrinsicSize.width;
    }

    size.width += borderPadding.LeftRight();
  }
  else if (!heightSet) {
    if (intrinsicSize.width > 0) {
      nscoord width = size.width - borderPadding.LeftRight();
      size.height = nscoord(int64_t(width) * int64_t(intrinsicSize.height) /
                            int64_t(intrinsicSize.width));
    }
    else {
      size.height = intrinsicSize.height;
    }

    size.height += borderPadding.TopBottom();
  }

  return BoundsCheck(minSize, size, maxSize);
}

nsSize
nsImageBoxFrame::GetMinSize(nsBoxLayoutState& aState)
{
  // An image can always scale down to (0,0).
  nsSize size(0,0);
  DISPLAY_MIN_SIZE(this, size);
  AddBorderAndPadding(size);
  bool widthSet, heightSet;
  nsIFrame::AddCSSMinSize(aState, this, size, widthSet, heightSet);
  return size;
}

nscoord
nsImageBoxFrame::GetBoxAscent(nsBoxLayoutState& aState)
{
  return GetPrefSize(aState).height;
}

nsIAtom*
nsImageBoxFrame::GetType() const
{
  return nsGkAtoms::imageBoxFrame;
}

#ifdef DEBUG_FRAME_DUMP
nsresult
nsImageBoxFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("ImageBox"), aResult);
}
#endif

nsresult
nsImageBoxFrame::Notify(imgIRequest *aRequest, int32_t aType, const nsIntRect* aData)
{
  if (aType == imgINotificationObserver::SIZE_AVAILABLE) {
    nsCOMPtr<imgIContainer> image;
    aRequest->GetImage(getter_AddRefs(image));
    return OnStartContainer(aRequest, image);
  }

  if (aType == imgINotificationObserver::DECODE_COMPLETE) {
    return OnStopDecode(aRequest);
  }

  if (aType == imgINotificationObserver::LOAD_COMPLETE) {
    uint32_t imgStatus;
    aRequest->GetImageStatus(&imgStatus);
    nsresult status =
        imgStatus & imgIRequest::STATUS_ERROR ? NS_ERROR_FAILURE : NS_OK;
    return OnStopRequest(aRequest, status);
  }

  if (aType == imgINotificationObserver::IS_ANIMATED) {
    return OnImageIsAnimated(aRequest);
  }

  if (aType == imgINotificationObserver::FRAME_UPDATE) {
    return FrameChanged(aRequest);
  }

  return NS_OK;
}

nsresult nsImageBoxFrame::OnStartContainer(imgIRequest *request,
                                           imgIContainer *image)
{
  NS_ENSURE_ARG_POINTER(image);

  // Ensure the animation (if any) is started. Note: There is no
  // corresponding call to Decrement for this. This Increment will be
  // 'cleaned up' by the Request when it is destroyed, but only then.
  request->IncrementAnimationConsumers();

  nscoord w, h;
  image->GetWidth(&w);
  image->GetHeight(&h);

  mIntrinsicSize.SizeTo(nsPresContext::CSSPixelsToAppUnits(w),
                        nsPresContext::CSSPixelsToAppUnits(h));

  if (!(GetStateBits() & NS_FRAME_FIRST_REFLOW)) {
    PresContext()->PresShell()->
      FrameNeedsReflow(this, nsIPresShell::eStyleChange, NS_FRAME_IS_DIRTY);
  }

  return NS_OK;
}

nsresult nsImageBoxFrame::OnStopDecode(imgIRequest *request)
{
  if (mFireEventOnDecode) {
    mFireEventOnDecode = false;

    uint32_t reqStatus;
    request->GetImageStatus(&reqStatus);
    if (!(reqStatus & imgIRequest::STATUS_ERROR)) {
      FireImageDOMEvent(mContent, NS_LOAD);
    } else {
      // Fire an onerror DOM event.
      mIntrinsicSize.SizeTo(0, 0);
      PresContext()->PresShell()->
        FrameNeedsReflow(this, nsIPresShell::eStyleChange, NS_FRAME_IS_DIRTY);
      FireImageDOMEvent(mContent, NS_LOAD_ERROR);
    }
  }

  nsBoxLayoutState state(PresContext());
  this->Redraw(state);

  return NS_OK;
}

nsresult nsImageBoxFrame::OnStopRequest(imgIRequest *request,
                                        nsresult aStatus)
{
  uint32_t reqStatus;
  request->GetImageStatus(&reqStatus);

  // We want to give the decoder a chance to find errors. If we haven't found
  // an error yet and we've already started decoding, we must only fire these
  // events after we finish decoding.
  if (NS_SUCCEEDED(aStatus) && !(reqStatus & imgIRequest::STATUS_ERROR) &&
      reqStatus & imgIRequest::STATUS_DECODE_STARTED) {
    mFireEventOnDecode = true;
  } else {
    if (NS_SUCCEEDED(aStatus)) {
      // Fire an onload DOM event.
      FireImageDOMEvent(mContent, NS_LOAD);
    } else {
      // Fire an onerror DOM event.
      mIntrinsicSize.SizeTo(0, 0);
      PresContext()->PresShell()->
        FrameNeedsReflow(this, nsIPresShell::eStyleChange, NS_FRAME_IS_DIRTY);
      FireImageDOMEvent(mContent, NS_LOAD_ERROR);
    }
  }

  return NS_OK;
}

nsresult nsImageBoxFrame::OnImageIsAnimated(imgIRequest *aRequest)
{
  // Register with our refresh driver, if we're animated.
  nsLayoutUtils::RegisterImageRequest(PresContext(), aRequest,
                                      &mRequestRegistered);

  return NS_OK;
}

nsresult nsImageBoxFrame::FrameChanged(imgIRequest *aRequest)
{
  if ((0 == mRect.width) || (0 == mRect.height)) {
    return NS_OK;
  }
 
  InvalidateLayer(nsDisplayItem::TYPE_XUL_IMAGE);

  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsImageBoxListener, imgINotificationObserver, imgIOnloadBlocker)

nsImageBoxListener::nsImageBoxListener()
{
}

nsImageBoxListener::~nsImageBoxListener()
{
}

NS_IMETHODIMP
nsImageBoxListener::Notify(imgIRequest *request, int32_t aType, const nsIntRect* aData)
{
  if (!mFrame)
    return NS_OK;

  return mFrame->Notify(request, aType, aData);
}

/* void blockOnload (in imgIRequest aRequest); */
NS_IMETHODIMP
nsImageBoxListener::BlockOnload(imgIRequest *aRequest)
{
  if (mFrame && mFrame->GetContent() && mFrame->GetContent()->GetCurrentDoc()) {
    mFrame->GetContent()->GetCurrentDoc()->BlockOnload();
  }

  return NS_OK;
}

/* void unblockOnload (in imgIRequest aRequest); */
NS_IMETHODIMP
nsImageBoxListener::UnblockOnload(imgIRequest *aRequest)
{
  if (mFrame && mFrame->GetContent() && mFrame->GetContent()->GetCurrentDoc()) {
    mFrame->GetContent()->GetCurrentDoc()->UnblockOnload(false);
  }

  return NS_OK;
}
