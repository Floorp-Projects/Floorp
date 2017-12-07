/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "gfxContext.h"
#include "nsImageBoxFrame.h"
#include "nsGkAtoms.h"
#include "nsStyleContext.h"
#include "nsStyleConsts.h"
#include "nsStyleUtil.h"
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
#include "nsCSSRendering.h"
#include "nsNameSpaceManager.h"
#include "nsTextFragment.h"
#include "nsTransform2D.h"
#include "nsITheme.h"

#include "nsIServiceManager.h"
#include "nsIURI.h"
#include "nsThreadUtils.h"
#include "nsDisplayList.h"
#include "ImageLayers.h"
#include "ImageContainer.h"
#include "nsIContent.h"

#include "nsContentUtils.h"

#include "mozilla/BasicEvents.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/Maybe.h"
#include "SVGImageContext.h"
#include "Units.h"
#include "mozilla/layers/WebRenderLayerManager.h"

#if defined(XP_WIN)
// Undefine LoadImage to prevent naming conflict with Windows.
#undef LoadImage
#endif

#define ONLOAD_CALLED_TOO_EARLY 1

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::image;
using namespace mozilla::layers;

class nsImageBoxFrameEvent : public Runnable
{
public:
  nsImageBoxFrameEvent(nsIContent* content, EventMessage message)
    : mozilla::Runnable("nsImageBoxFrameEvent")
    , mContent(content)
    , mMessage(message)
  {
  }

  NS_IMETHOD Run() override;

private:
  nsCOMPtr<nsIContent> mContent;
  EventMessage mMessage;
};

NS_IMETHODIMP
nsImageBoxFrameEvent::Run()
{
  nsIPresShell *pres_shell = mContent->OwnerDoc()->GetShell();
  if (!pres_shell) {
    return NS_OK;
  }

  RefPtr<nsPresContext> pres_context = pres_shell->GetPresContext();
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
// can't decide if it wants to call its observer methods
// synchronously or asynchronously. If an image is loaded from the
// cache the notifications come back synchronously, but if the image
// is loaded from the network the notifications come back
// asynchronously.

void
FireImageDOMEvent(nsIContent* aContent, EventMessage aMessage)
{
  NS_ASSERTION(aMessage == eLoad || aMessage == eLoadError,
               "invalid message");

  nsCOMPtr<nsIRunnable> event = new nsImageBoxFrameEvent(aContent, aMessage);
  nsresult rv = aContent->OwnerDoc()->Dispatch(TaskCategory::Other,
                                               event.forget());
  if (NS_FAILED(rv)) {
    NS_WARNING("failed to dispatch image event");
  }
}

//
// NS_NewImageBoxFrame
//
// Creates a new image frame and returns it
//
nsIFrame*
NS_NewImageBoxFrame (nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsImageBoxFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsImageBoxFrame)

nsresult
nsImageBoxFrame::AttributeChanged(int32_t aNameSpaceID,
                                  nsAtom* aAttribute,
                                  int32_t aModType)
{
  nsresult rv = nsLeafBoxFrame::AttributeChanged(aNameSpaceID, aAttribute,
                                                 aModType);

  if (aAttribute == nsGkAtoms::src) {
    UpdateImage();
    PresShell()->
      FrameNeedsReflow(this, nsIPresShell::eStyleChange, NS_FRAME_IS_DIRTY);
  }
  else if (aAttribute == nsGkAtoms::validate)
    UpdateLoadFlags();

  return rv;
}

nsImageBoxFrame::nsImageBoxFrame(nsStyleContext* aContext)
  : nsLeafBoxFrame(aContext, kClassID)
  , mIntrinsicSize(0, 0)
  , mLoadFlags(nsIRequest::LOAD_NORMAL)
  , mRequestRegistered(false)
  , mUseSrcAttr(false)
  , mSuppressStyleCheck(false)
{
  MarkIntrinsicISizesDirty();
}

nsImageBoxFrame::~nsImageBoxFrame()
{
}


/* virtual */ void
nsImageBoxFrame::MarkIntrinsicISizesDirty()
{
  SizeNeedsRecalc(mImageSize);
  nsLeafBoxFrame::MarkIntrinsicISizesDirty();
}

void
nsImageBoxFrame::DestroyFrom(nsIFrame* aDestructRoot, PostDestroyData& aPostDestroyData)
{
  if (mImageRequest) {
    nsLayoutUtils::DeregisterImageRequest(PresContext(), mImageRequest,
                                          &mRequestRegistered);

    // Release image loader first so that it's refcnt can go to zero
    mImageRequest->CancelAndForgetObserver(NS_ERROR_FAILURE);
  }

  if (mListener)
    reinterpret_cast<nsImageBoxListener*>(mListener.get())->SetFrame(nullptr); // set the frame to null so we don't send messages to a dead object.

  nsLeafBoxFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}


void
nsImageBoxFrame::Init(nsIContent*       aContent,
                      nsContainerFrame* aParent,
                      nsIFrame*         aPrevInFlow)
{
  if (!mListener) {
    RefPtr<nsImageBoxListener> listener = new nsImageBoxListener();
    listener->SetFrame(this);
    mListener = listener.forget();
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

  RefPtr<imgRequestProxy> oldImageRequest = mImageRequest;

  if (mImageRequest) {
    nsLayoutUtils::DeregisterImageRequest(presContext, mImageRequest,
                                          &mRequestRegistered);
    mImageRequest->CancelAndForgetObserver(NS_ERROR_FAILURE);
    mImageRequest = nullptr;
  }

  // get the new image src
  nsAutoString src;
  mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::src, src);
  mUseSrcAttr = !src.IsEmpty();
  if (mUseSrcAttr) {
    nsIDocument* doc = mContent->GetComposedDoc();
    if (doc) {
      nsContentPolicyType contentPolicyType;
      nsCOMPtr<nsIPrincipal> triggeringPrincipal;
      uint64_t requestContextID = 0;
      nsContentUtils::GetContentPolicyTypeForUIImageLoading(mContent,
                                                            getter_AddRefs(triggeringPrincipal),
                                                            contentPolicyType,
                                                            &requestContextID);

      nsCOMPtr<nsIURI> baseURI = mContent->GetBaseURI();
      nsCOMPtr<nsIURI> uri;
      nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(uri),
                                                src,
                                                doc,
                                                baseURI);
      if (uri) {
        nsresult rv = nsContentUtils::LoadImage(uri, mContent, doc,
                                                triggeringPrincipal,
                                                requestContextID,
                                                doc->GetDocumentURI(),
                                                doc->GetReferrerPolicy(),
                                                mListener, mLoadFlags,
                                                EmptyString(),
                                                getter_AddRefs(mImageRequest),
                                                contentPolicyType);

        if (NS_SUCCEEDED(rv) && mImageRequest) {
          nsLayoutUtils::RegisterImageRequestIfAnimated(presContext,
                                                        mImageRequest,
                                                        &mRequestRegistered);
        }
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
        styleRequest->SyncClone(mListener,
                                mContent->GetComposedDoc(),
                                getter_AddRefs(mImageRequest));
      }
    }
  }

  if (!mImageRequest) {
    // We have no image, so size to 0
    mIntrinsicSize.SizeTo(0, 0);
  } else {
    // We don't want discarding or decode-on-draw for xul images.
    mImageRequest->StartDecoding(imgIContainer::FLAG_NONE);
    mImageRequest->LockImage();
  }

  // Do this _after_ locking the new image in case they are the same image.
  if (oldImageRequest) {
    oldImageRequest->UnlockImage();
  }
}

void
nsImageBoxFrame::UpdateLoadFlags()
{
  static Element::AttrValuesArray strings[] =
    {&nsGkAtoms::always, &nsGkAtoms::never, nullptr};
  switch (mContent->AsElement()->FindAttrValueIn(kNameSpaceID_None,
                                                 nsGkAtoms::validate, strings,
                                                 eCaseMatters)) {
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
                                  const nsDisplayListSet& aLists)
{
  nsLeafBoxFrame::BuildDisplayList(aBuilder, aLists);

  if ((0 == mRect.width) || (0 == mRect.height)) {
    // Do not render when given a zero area. This avoids some useless
    // scaling work while we wait for our image dimensions to arrive
    // asynchronously.
    return;
  }

  if (!IsVisibleForPainting(aBuilder))
    return;

  uint32_t clipFlags =
    nsStyleUtil::ObjectPropsMightCauseOverflow(StylePosition()) ?
    0 : DisplayListClipState::ASSUME_DRAWING_RESTRICTED_TO_CONTENT_RECT;

  DisplayListClipState::AutoClipContainingBlockDescendantsToContentBox
    clip(aBuilder, this, clipFlags);

  nsDisplayList list;
  list.AppendToTop(
    new (aBuilder) nsDisplayXULImage(aBuilder, this));

  CreateOwnLayerIfNeeded(aBuilder, &list);

  aLists.Content()->AppendToTop(&list);
}

already_AddRefed<imgIContainer>
nsImageBoxFrame::GetImageContainerForPainting(const nsPoint& aPt,
                                              ImgDrawResult& aDrawResult,
                                              Maybe<nsPoint>& aAnchorPoint,
                                              nsRect& aDest)
{
  if (!mImageRequest) {
    // This probably means we're drawn by a native theme.
    aDrawResult = ImgDrawResult::SUCCESS;
    return nullptr;
  }

  // Don't draw if the image's size isn't available.
  uint32_t imgStatus;
  if (!NS_SUCCEEDED(mImageRequest->GetImageStatus(&imgStatus)) ||
      !(imgStatus & imgIRequest::STATUS_SIZE_AVAILABLE)) {
    aDrawResult = ImgDrawResult::NOT_READY;
    return nullptr;
  }

  nsCOMPtr<imgIContainer> imgCon;
  mImageRequest->GetImage(getter_AddRefs(imgCon));

  if (!imgCon) {
    aDrawResult = ImgDrawResult::NOT_READY;
    return nullptr;
  }

  aDest = GetDestRect(aPt, aAnchorPoint);
  aDrawResult = ImgDrawResult::SUCCESS;
  return imgCon.forget();
}

ImgDrawResult
nsImageBoxFrame::PaintImage(gfxContext& aRenderingContext,
                            const nsRect& aDirtyRect, nsPoint aPt,
                            uint32_t aFlags)
{
  ImgDrawResult result;
  Maybe<nsPoint> anchorPoint;
  nsRect dest;
  nsCOMPtr<imgIContainer> imgCon = GetImageContainerForPainting(aPt, result,
                                                                anchorPoint,
                                                                dest);
  if (!imgCon) {
    return result;
  }

  // don't draw if the image is not dirty
  // XXX(seth): Can this actually happen anymore?
  nsRect dirty;
  if (!dirty.IntersectRect(aDirtyRect, dest)) {
    return ImgDrawResult::TEMPORARY_ERROR;
  }

  bool hasSubRect = !mUseSrcAttr && (mSubRect.width > 0 || mSubRect.height > 0);

  Maybe<SVGImageContext> svgContext;
  SVGImageContext::MaybeStoreContextPaint(svgContext, this, imgCon);
  return nsLayoutUtils::DrawSingleImage(
           aRenderingContext,
           PresContext(), imgCon,
           nsLayoutUtils::GetSamplingFilterForFrame(this),
           dest, dirty,
           svgContext, aFlags,
           anchorPoint.ptrOr(nullptr),
           hasSubRect ? &mSubRect : nullptr);
}

ImgDrawResult
nsImageBoxFrame::CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                         mozilla::wr::IpcResourceUpdateQueue& aResources,
                                         const StackingContextHelper& aSc,
                                         mozilla::layers::WebRenderLayerManager* aManager,
                                         nsDisplayItem* aItem,
                                         nsPoint aPt,
                                         uint32_t aFlags)
{
  ImgDrawResult result;
  Maybe<nsPoint> anchorPoint;
  nsRect dest;
  nsCOMPtr<imgIContainer> imgCon = GetImageContainerForPainting(aPt, result,
                                                                anchorPoint,
                                                                dest);
  if (!imgCon) {
    return result;
  }

  uint32_t containerFlags = imgIContainer::FLAG_ASYNC_NOTIFY;
  if (aFlags & nsImageRenderer::FLAG_PAINTING_TO_WINDOW) {
    containerFlags |= imgIContainer::FLAG_HIGH_QUALITY_SCALING;
  }
  if (aFlags & nsImageRenderer::FLAG_SYNC_DECODE_IMAGES) {
    containerFlags |= imgIContainer::FLAG_SYNC_DECODE;
  }

  const int32_t appUnitsPerDevPixel = PresContext()->AppUnitsPerDevPixel();
  LayoutDeviceRect fillRect = LayoutDeviceRect::FromAppUnits(dest,
                                                             appUnitsPerDevPixel);
  Maybe<SVGImageContext> svgContext;
  gfx::IntSize decodeSize =
    nsLayoutUtils::ComputeImageContainerDrawingParameters(imgCon, aItem->Frame(), fillRect,
                                                          aSc, containerFlags, svgContext);
  RefPtr<layers::ImageContainer> container =
    imgCon->GetImageContainerAtSize(aManager, decodeSize, svgContext, containerFlags);
  if (!container) {
    NS_WARNING("Failed to get image container");
    return ImgDrawResult::NOT_READY;
  }

  gfx::IntSize size;
  Maybe<wr::ImageKey> key = aManager->CommandBuilder().CreateImageKey(aItem, container,
                                                                      aBuilder, aResources,
                                                                      aSc, size, Nothing());
  if (key.isNothing()) {
    return ImgDrawResult::BAD_IMAGE;
  }
  wr::LayoutRect fill = aSc.ToRelativeLayoutRect(fillRect);

  LayoutDeviceSize gapSize(0, 0);
  SamplingFilter sampleFilter = nsLayoutUtils::GetSamplingFilterForFrame(aItem->Frame());
  aBuilder.PushImage(fill, fill, !BackfaceIsHidden(),
                     wr::ToLayoutSize(fillRect.Size()), wr::ToLayoutSize(gapSize),
                     wr::ToImageRendering(sampleFilter), key.value());

  return ImgDrawResult::SUCCESS;
}

nsRect
nsImageBoxFrame::GetDestRect(const nsPoint& aOffset, Maybe<nsPoint>& aAnchorPoint)
{
  nsCOMPtr<imgIContainer> imgCon;
  mImageRequest->GetImage(getter_AddRefs(imgCon));
  MOZ_ASSERT(imgCon);

  nsRect clientRect;
  GetXULClientRect(clientRect);
  clientRect += aOffset;
  nsRect dest;
  if (!mUseSrcAttr) {
    // Our image (if we have one) is coming from the CSS property
    // 'list-style-image' (combined with '-moz-image-region'). For now, ignore
    // 'object-fit' & 'object-position' in this case, and just fill our rect.
    // XXXdholbert Should we even honor these properties in this case? They only
    // apply to replaced elements, and I'm not sure we count as a replaced
    // element when our image data is determined by CSS.
    dest = clientRect;
  } else {
    // Determine dest rect based on intrinsic size & ratio, along with
    // 'object-fit' & 'object-position' properties:
    IntrinsicSize intrinsicSize;
    nsSize intrinsicRatio;
    if (mIntrinsicSize.width > 0 && mIntrinsicSize.height > 0) {
      // Image has a valid size; use it as intrinsic size & ratio.
      intrinsicSize.width.SetCoordValue(mIntrinsicSize.width);
      intrinsicSize.height.SetCoordValue(mIntrinsicSize.height);
      intrinsicRatio = mIntrinsicSize;
    } else {
      // Image doesn't have a (valid) intrinsic size.
      // Try to look up intrinsic ratio and use that at least.
      imgCon->GetIntrinsicRatio(&intrinsicRatio);
    }
    aAnchorPoint.emplace();
    dest = nsLayoutUtils::ComputeObjectDestRect(clientRect,
                                                intrinsicSize,
                                                intrinsicRatio,
                                                StylePosition(),
                                                aAnchorPoint.ptr());
  }

  return dest;
}

void nsDisplayXULImage::Paint(nsDisplayListBuilder* aBuilder,
                              gfxContext* aCtx)
{
  // Even though we call StartDecoding when we get a new image we pass
  // FLAG_SYNC_DECODE_IF_FAST here for the case where the size we draw at is not
  // the intrinsic size of the image and we aren't likely to implement predictive
  // decoding at the correct size for this class like nsImageFrame has.
  uint32_t flags = imgIContainer::FLAG_SYNC_DECODE_IF_FAST;
  if (aBuilder->ShouldSyncDecodeImages())
    flags |= imgIContainer::FLAG_SYNC_DECODE;
  if (aBuilder->IsPaintingToWindow())
    flags |= imgIContainer::FLAG_HIGH_QUALITY_SCALING;

  ImgDrawResult result = static_cast<nsImageBoxFrame*>(mFrame)->
    PaintImage(*aCtx, mVisibleRect, ToReferenceFrame(), flags);

  nsDisplayItemGenericImageGeometry::UpdateDrawResult(this, result);
}

LayerState
nsDisplayXULImage::GetLayerState(nsDisplayListBuilder* aBuilder,
                                 LayerManager* aManager,
                                 const ContainerLayerParameters& aParameters)
{
  if (ShouldUseAdvancedLayer(aManager, gfxPrefs::LayersAllowImageLayers) &&
      CanOptimizeToImageLayer(aManager, aBuilder)) {
    return LAYER_ACTIVE;
  }
  return LAYER_NONE;
}

already_AddRefed<Layer>
nsDisplayXULImage::BuildLayer(nsDisplayListBuilder* aBuilder,
                           LayerManager* aManager,
                           const ContainerLayerParameters& aContainerParameters)
{
  return BuildDisplayItemLayer(aBuilder, aManager, aContainerParameters);
}

bool
nsDisplayXULImage::CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                           mozilla::wr::IpcResourceUpdateQueue& aResources,
                                           const StackingContextHelper& aSc,
                                           mozilla::layers::WebRenderLayerManager* aManager,
                                           nsDisplayListBuilder* aDisplayListBuilder)
{
  nsImageBoxFrame* imageFrame = static_cast<nsImageBoxFrame*>(mFrame);
  if (!imageFrame->CanOptimizeToImageLayer()) {
    return false;
  }

  if (!imageFrame->mImageRequest) {
    return false;
  }

  uint32_t flags = imgIContainer::FLAG_SYNC_DECODE_IF_FAST;
  if (aDisplayListBuilder->ShouldSyncDecodeImages()) {
    flags |= imgIContainer::FLAG_SYNC_DECODE;
  }
  if (aDisplayListBuilder->IsPaintingToWindow()) {
    flags |= imgIContainer::FLAG_HIGH_QUALITY_SCALING;
  }

  ImgDrawResult result = imageFrame->
    CreateWebRenderCommands(aBuilder, aResources, aSc, aManager, this, ToReferenceFrame(), flags);

  nsDisplayItemGenericImageGeometry::UpdateDrawResult(this, result);
  return true;
}

nsDisplayItemGeometry*
nsDisplayXULImage::AllocateGeometry(nsDisplayListBuilder* aBuilder)
{
  return new nsDisplayItemGenericImageGeometry(this, aBuilder);
}

void
nsDisplayXULImage::ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                             const nsDisplayItemGeometry* aGeometry,
                                             nsRegion* aInvalidRegion) const
{
  auto boxFrame = static_cast<nsImageBoxFrame*>(mFrame);
  auto geometry =
    static_cast<const nsDisplayItemGenericImageGeometry*>(aGeometry);

  if (aBuilder->ShouldSyncDecodeImages() &&
      boxFrame->mImageRequest &&
      geometry->ShouldInvalidateToSyncDecodeImages()) {
      bool snap;
      aInvalidRegion->Or(*aInvalidRegion, GetBounds(aBuilder, &snap));
  }

  nsDisplayImageContainer::ComputeInvalidationRegion(aBuilder, aGeometry, aInvalidRegion);
}

bool
nsDisplayXULImage::CanOptimizeToImageLayer(LayerManager* aManager,
                                           nsDisplayListBuilder* aBuilder)
{
  nsImageBoxFrame* imageFrame = static_cast<nsImageBoxFrame*>(mFrame);
  if (!imageFrame->CanOptimizeToImageLayer()) {
    return false;
  }

  return nsDisplayImageContainer::CanOptimizeToImageLayer(aManager, aBuilder);
}

already_AddRefed<imgIContainer>
nsDisplayXULImage::GetImage()
{
  nsImageBoxFrame* imageFrame = static_cast<nsImageBoxFrame*>(mFrame);
  if (!imageFrame->mImageRequest) {
    return nullptr;
  }

  nsCOMPtr<imgIContainer> imgCon;
  imageFrame->mImageRequest->GetImage(getter_AddRefs(imgCon));

  return imgCon.forget();
}

nsRect
nsDisplayXULImage::GetDestRect() const
{
  Maybe<nsPoint> anchorPoint;
  return static_cast<nsImageBoxFrame*>(mFrame)->GetDestRect(ToReferenceFrame(), anchorPoint);
}

bool
nsImageBoxFrame::CanOptimizeToImageLayer()
{
  bool hasSubRect = !mUseSrcAttr && (mSubRect.width > 0 || mSubRect.height > 0);
  if (hasSubRect) {
    return false;
  }
  return true;
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
nsImageBoxFrame::GetXULPrefSize(nsBoxLayoutState& aState)
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
  GetXULBorderAndPadding(borderPadding);
  size.width += borderPadding.LeftRight();
  size.height += borderPadding.TopBottom();

  bool widthSet, heightSet;
  nsIFrame::AddXULPrefSize(this, size, widthSet, heightSet);
  NS_ASSERTION(size.width != NS_INTRINSICSIZE && size.height != NS_INTRINSICSIZE,
               "non-intrinsic size expected");

  nsSize minSize = GetXULMinSize(aState);
  nsSize maxSize = GetXULMaxSize(aState);

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
nsImageBoxFrame::GetXULMinSize(nsBoxLayoutState& aState)
{
  // An image can always scale down to (0,0).
  nsSize size(0,0);
  DISPLAY_MIN_SIZE(this, size);
  AddBorderAndPadding(size);
  bool widthSet, heightSet;
  nsIFrame::AddXULMinSize(aState, this, size, widthSet, heightSet);
  return size;
}

nscoord
nsImageBoxFrame::GetXULBoxAscent(nsBoxLayoutState& aState)
{
  return GetXULPrefSize(aState).height;
}

#ifdef DEBUG_FRAME_DUMP
nsresult
nsImageBoxFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("ImageBox"), aResult);
}
#endif

nsresult
nsImageBoxFrame::Notify(imgIRequest* aRequest,
                        int32_t aType,
                        const nsIntRect* aData)
{
  if (aType == imgINotificationObserver::SIZE_AVAILABLE) {
    nsCOMPtr<imgIContainer> image;
    aRequest->GetImage(getter_AddRefs(image));
    return OnSizeAvailable(aRequest, image);
  }

  if (aType == imgINotificationObserver::DECODE_COMPLETE) {
    return OnDecodeComplete(aRequest);
  }

  if (aType == imgINotificationObserver::LOAD_COMPLETE) {
    uint32_t imgStatus;
    aRequest->GetImageStatus(&imgStatus);
    nsresult status =
        imgStatus & imgIRequest::STATUS_ERROR ? NS_ERROR_FAILURE : NS_OK;
    return OnLoadComplete(aRequest, status);
  }

  if (aType == imgINotificationObserver::IS_ANIMATED) {
    return OnImageIsAnimated(aRequest);
  }

  if (aType == imgINotificationObserver::FRAME_UPDATE) {
    return OnFrameUpdate(aRequest);
  }

  return NS_OK;
}

nsresult
nsImageBoxFrame::OnSizeAvailable(imgIRequest* aRequest, imgIContainer* aImage)
{
  NS_ENSURE_ARG_POINTER(aImage);

  // Ensure the animation (if any) is started. Note: There is no
  // corresponding call to Decrement for this. This Increment will be
  // 'cleaned up' by the Request when it is destroyed, but only then.
  aRequest->IncrementAnimationConsumers();

  nscoord w, h;
  aImage->GetWidth(&w);
  aImage->GetHeight(&h);

  mIntrinsicSize.SizeTo(nsPresContext::CSSPixelsToAppUnits(w),
                        nsPresContext::CSSPixelsToAppUnits(h));

  if (!(GetStateBits() & NS_FRAME_FIRST_REFLOW)) {
    PresShell()->
      FrameNeedsReflow(this, nsIPresShell::eStyleChange, NS_FRAME_IS_DIRTY);
  }

  return NS_OK;
}

nsresult
nsImageBoxFrame::OnDecodeComplete(imgIRequest* aRequest)
{
  nsBoxLayoutState state(PresContext());
  this->XULRedraw(state);
  return NS_OK;
}

nsresult
nsImageBoxFrame::OnLoadComplete(imgIRequest* aRequest, nsresult aStatus)
{
  if (NS_SUCCEEDED(aStatus)) {
    // Fire an onload DOM event.
    FireImageDOMEvent(mContent, eLoad);
  } else {
    // Fire an onerror DOM event.
    mIntrinsicSize.SizeTo(0, 0);
    PresShell()->
      FrameNeedsReflow(this, nsIPresShell::eStyleChange, NS_FRAME_IS_DIRTY);
    FireImageDOMEvent(mContent, eLoadError);
  }

  return NS_OK;
}

nsresult
nsImageBoxFrame::OnImageIsAnimated(imgIRequest* aRequest)
{
  // Register with our refresh driver, if we're animated.
  nsLayoutUtils::RegisterImageRequest(PresContext(), aRequest,
                                      &mRequestRegistered);

  return NS_OK;
}

nsresult
nsImageBoxFrame::OnFrameUpdate(imgIRequest* aRequest)
{
  if ((0 == mRect.width) || (0 == mRect.height)) {
    return NS_OK;
  }

  InvalidateLayer(DisplayItemType::TYPE_XUL_IMAGE);

  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsImageBoxListener, imgINotificationObserver)

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
