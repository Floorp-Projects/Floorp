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
#include "mozilla/ComputedStyle.h"
#include "nsStyleConsts.h"
#include "nsStyleUtil.h"
#include "nsCOMPtr.h"
#include "nsPresContext.h"
#include "nsBoxLayoutState.h"

#include "nsHTMLParts.h"
#include "nsString.h"
#include "nsLeafFrame.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/DocumentInlines.h"
#include "nsImageMap.h"
#include "nsContainerFrame.h"
#include "nsCSSRendering.h"
#include "nsNameSpaceManager.h"
#include "nsTextFragment.h"
#include "nsTransform2D.h"
#include "nsITheme.h"

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
#include "mozilla/PresShell.h"
#include "SVGImageContext.h"
#include "Units.h"
#include "mozilla/layers/RenderRootStateManager.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "mozilla/dom/ImageTracker.h"

#if defined(XP_WIN)
// Undefine LoadImage to prevent naming conflict with Windows.
#  undef LoadImage
#endif

#define ONLOAD_CALLED_TOO_EARLY 1

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::image;
using namespace mozilla::layers;

using mozilla::dom::Document;
using mozilla::dom::Element;
using mozilla::dom::ReferrerInfo;

class nsImageBoxFrameEvent : public Runnable {
 public:
  nsImageBoxFrameEvent(nsIContent* content, EventMessage message)
      : mozilla::Runnable("nsImageBoxFrameEvent"),
        mContent(content),
        mMessage(message) {}

  NS_IMETHOD Run() override;

 private:
  nsCOMPtr<nsIContent> mContent;
  EventMessage mMessage;
};

NS_IMETHODIMP
nsImageBoxFrameEvent::Run() {
  RefPtr<nsPresContext> pres_context = mContent->OwnerDoc()->GetPresContext();
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
static void FireImageDOMEvent(nsIContent* aContent, EventMessage aMessage) {
  NS_ASSERTION(aMessage == eLoad || aMessage == eLoadError, "invalid message");

  nsCOMPtr<nsIRunnable> event = new nsImageBoxFrameEvent(aContent, aMessage);
  nsresult rv =
      aContent->OwnerDoc()->Dispatch(TaskCategory::Other, event.forget());
  if (NS_FAILED(rv)) {
    NS_WARNING("failed to dispatch image event");
  }
}

//
// NS_NewImageBoxFrame
//
// Creates a new image frame and returns it
//
nsIFrame* NS_NewImageBoxFrame(PresShell* aPresShell, ComputedStyle* aStyle) {
  return new (aPresShell) nsImageBoxFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsImageBoxFrame)
NS_QUERYFRAME_HEAD(nsImageBoxFrame)
  NS_QUERYFRAME_ENTRY(nsImageBoxFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsLeafBoxFrame)

nsresult nsImageBoxFrame::AttributeChanged(int32_t aNameSpaceID,
                                           nsAtom* aAttribute,
                                           int32_t aModType) {
  nsresult rv =
      nsLeafBoxFrame::AttributeChanged(aNameSpaceID, aAttribute, aModType);

  if (aAttribute == nsGkAtoms::src) {
    UpdateImage();
    PresShell()->FrameNeedsReflow(this, IntrinsicDirty::StyleChange,
                                  NS_FRAME_IS_DIRTY);
  } else if (aAttribute == nsGkAtoms::validate)
    UpdateLoadFlags();

  return rv;
}

nsImageBoxFrame::nsImageBoxFrame(ComputedStyle* aStyle,
                                 nsPresContext* aPresContext)
    : nsLeafBoxFrame(aStyle, aPresContext, kClassID),
      mIntrinsicSize(0, 0),
      mLoadFlags(nsIRequest::LOAD_NORMAL),
      mRequestRegistered(false),
      mUseSrcAttr(false),
      mSuppressStyleCheck(false) {
  MarkIntrinsicISizesDirty();
}

nsImageBoxFrame::~nsImageBoxFrame() = default;

/* virtual */
void nsImageBoxFrame::MarkIntrinsicISizesDirty() {
  XULSizeNeedsRecalc(mImageSize);
  nsLeafBoxFrame::MarkIntrinsicISizesDirty();
}

void nsImageBoxFrame::DestroyFrom(nsIFrame* aDestructRoot,
                                  PostDestroyData& aPostDestroyData) {
  if (mImageRequest) {
    nsLayoutUtils::DeregisterImageRequest(PresContext(), mImageRequest,
                                          &mRequestRegistered);

    mImageRequest->UnlockImage();

    if (mUseSrcAttr) {
      PresContext()->Document()->ImageTracker()->Remove(mImageRequest);
    }

    // Release image loader first so that it's refcnt can go to zero
    mImageRequest->CancelAndForgetObserver(NS_ERROR_FAILURE);
  }

  if (mListener) {
    // set the frame to null so we don't send messages to a dead object.
    reinterpret_cast<nsImageBoxListener*>(mListener.get())->ClearFrame();
  }

  nsLeafBoxFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}

void nsImageBoxFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                           nsIFrame* aPrevInFlow) {
  if (!mListener) {
    RefPtr<nsImageBoxListener> listener = new nsImageBoxListener(this);
    mListener = std::move(listener);
  }

  mSuppressStyleCheck = true;
  nsLeafBoxFrame::Init(aContent, aParent, aPrevInFlow);
  mSuppressStyleCheck = false;

  UpdateLoadFlags();
  UpdateImage();
}

void nsImageBoxFrame::RestartAnimation() {
  if (mImageRequest && !mRequestRegistered) {
    nsLayoutUtils::RegisterImageRequestIfAnimated(PresContext(), mImageRequest,
                                                  &mRequestRegistered);
  }
}

void nsImageBoxFrame::StopAnimation() {
  if (mImageRequest && mRequestRegistered) {
    nsLayoutUtils::DeregisterImageRequest(PresContext(), mImageRequest,
                                          &mRequestRegistered);
  }
}

void nsImageBoxFrame::UpdateImage() {
  nsPresContext* presContext = PresContext();
  Document* doc = presContext->Document();

  RefPtr<imgRequestProxy> oldImageRequest = mImageRequest;

  if (mImageRequest) {
    nsLayoutUtils::DeregisterImageRequest(presContext, mImageRequest,
                                          &mRequestRegistered);
    mImageRequest->CancelAndForgetObserver(NS_ERROR_FAILURE);
    if (mUseSrcAttr) {
      doc->ImageTracker()->Remove(mImageRequest);
    }
    mImageRequest = nullptr;
  }

  // get the new image src
  nsAutoString src;
  mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::src, src);
  mUseSrcAttr = !src.IsEmpty();
  if (mUseSrcAttr) {
    nsContentPolicyType contentPolicyType;
    nsCOMPtr<nsIPrincipal> triggeringPrincipal;
    uint64_t requestContextID = 0;
    nsContentUtils::GetContentPolicyTypeForUIImageLoading(
        mContent, getter_AddRefs(triggeringPrincipal), contentPolicyType,
        &requestContextID);

    nsCOMPtr<nsIURI> uri;
    nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(uri), src, doc,
                                              mContent->GetBaseURI());
    if (uri) {
      nsCOMPtr<nsIReferrerInfo> referrerInfo = new ReferrerInfo();
      referrerInfo->InitWithNode(mContent);

      nsresult rv = nsContentUtils::LoadImage(
          uri, mContent, doc, triggeringPrincipal, requestContextID,
          referrerInfo, mListener, mLoadFlags, EmptyString(),
          getter_AddRefs(mImageRequest), contentPolicyType);

      if (NS_SUCCEEDED(rv) && mImageRequest) {
        nsLayoutUtils::RegisterImageRequestIfAnimated(
            presContext, mImageRequest, &mRequestRegistered);

        // Add to the ImageTracker so that we can find it when media
        // feature values change (e.g. when the system theme changes)
        // and invalidate the image.  This allows favicons to respond
        // to these changes.
        doc->ImageTracker()->Add(mImageRequest);
      }
    }
  } else if (auto* styleRequest = GetRequestFromStyle()) {
    styleRequest->SyncClone(mListener, mContent->GetComposedDoc(),
                            getter_AddRefs(mImageRequest));
  }

  if (!mImageRequest) {
    // We have no image, so size to 0
    mIntrinsicSize.SizeTo(0, 0);
  } else {
    // We don't want discarding or decode-on-draw for xul images.
    mImageRequest->StartDecoding(imgIContainer::FLAG_ASYNC_NOTIFY);
    mImageRequest->LockImage();
  }

  // Do this _after_ locking the new image in case they are the same image.
  if (oldImageRequest) {
    oldImageRequest->UnlockImage();
  }
}

void nsImageBoxFrame::UpdateLoadFlags() {
  static Element::AttrValuesArray strings[] = {nsGkAtoms::always,
                                               nsGkAtoms::never, nullptr};
  switch (mContent->AsElement()->FindAttrValueIn(
      kNameSpaceID_None, nsGkAtoms::validate, strings, eCaseMatters)) {
    case 0:
      mLoadFlags = nsIRequest::VALIDATE_ALWAYS;
      break;
    case 1:
      mLoadFlags = nsIRequest::VALIDATE_NEVER | nsIRequest::LOAD_FROM_CACHE;
      break;
    default:
      mLoadFlags = nsIRequest::LOAD_NORMAL;
      break;
  }
}

void nsImageBoxFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                       const nsDisplayListSet& aLists) {
  nsLeafBoxFrame::BuildDisplayList(aBuilder, aLists);

  if ((0 == mRect.width) || (0 == mRect.height)) {
    // Do not render when given a zero area. This avoids some useless
    // scaling work while we wait for our image dimensions to arrive
    // asynchronously.
    return;
  }

  if (!IsVisibleForPainting()) return;

  uint32_t clipFlags =
      nsStyleUtil::ObjectPropsMightCauseOverflow(StylePosition())
          ? 0
          : DisplayListClipState::ASSUME_DRAWING_RESTRICTED_TO_CONTENT_RECT;

  DisplayListClipState::AutoClipContainingBlockDescendantsToContentBox clip(
      aBuilder, this, clipFlags);

  nsDisplayList list;
  list.AppendNewToTop<nsDisplayXULImage>(aBuilder, this);

  CreateOwnLayerIfNeeded(aBuilder, &list,
                         nsDisplayOwnLayer::OwnLayerForImageBoxFrame);

  aLists.Content()->AppendToTop(&list);
}

already_AddRefed<imgIContainer> nsImageBoxFrame::GetImageContainerForPainting(
    const nsPoint& aPt, ImgDrawResult& aDrawResult,
    Maybe<nsPoint>& aAnchorPoint, nsRect& aDest) {
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

ImgDrawResult nsImageBoxFrame::PaintImage(gfxContext& aRenderingContext,
                                          const nsRect& aDirtyRect, nsPoint aPt,
                                          uint32_t aFlags) {
  ImgDrawResult result;
  Maybe<nsPoint> anchorPoint;
  nsRect dest;
  nsCOMPtr<imgIContainer> imgCon =
      GetImageContainerForPainting(aPt, result, anchorPoint, dest);
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
      aRenderingContext, PresContext(), imgCon,
      nsLayoutUtils::GetSamplingFilterForFrame(this), dest, dirty, svgContext,
      aFlags, anchorPoint.ptrOr(nullptr), hasSubRect ? &mSubRect : nullptr);
}

ImgDrawResult nsImageBoxFrame::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager, nsDisplayItem* aItem,
    nsPoint aPt, uint32_t aFlags) {
  ImgDrawResult result;
  Maybe<nsPoint> anchorPoint;
  nsRect dest;
  nsCOMPtr<imgIContainer> imgCon =
      GetImageContainerForPainting(aPt, result, anchorPoint, dest);
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
  LayoutDeviceRect fillRect =
      LayoutDeviceRect::FromAppUnits(dest, appUnitsPerDevPixel);

  Maybe<SVGImageContext> svgContext;
  gfx::IntSize decodeSize =
      nsLayoutUtils::ComputeImageContainerDrawingParameters(
          imgCon, aItem->Frame(), fillRect, aSc, containerFlags, svgContext);

  RefPtr<layers::ImageContainer> container;
  result = imgCon->GetImageContainerAtSize(aManager->LayerManager(), decodeSize,
                                           svgContext, containerFlags,
                                           getter_AddRefs(container));
  if (!container) {
    NS_WARNING("Failed to get image container");
    return result;
  }

  mozilla::wr::ImageRendering rendering = wr::ToImageRendering(
      nsLayoutUtils::GetSamplingFilterForFrame(aItem->Frame()));
  gfx::IntSize size;
  Maybe<wr::ImageKey> key = aManager->CommandBuilder().CreateImageKey(
      aItem, container, aBuilder, aResources, rendering, aSc, size, Nothing());
  if (key.isNothing()) {
    return result;
  }

  wr::LayoutRect fill = wr::ToLayoutRect(fillRect);
  aBuilder.PushImage(fill, fill, !BackfaceIsHidden(), rendering, key.value());

  return result;
}

nsRect nsImageBoxFrame::GetDestRect(const nsPoint& aOffset,
                                    Maybe<nsPoint>& aAnchorPoint) {
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
    AspectRatio intrinsicRatio;
    if (mIntrinsicSize.width > 0 && mIntrinsicSize.height > 0) {
      // Image has a valid size; use it as intrinsic size & ratio.
      intrinsicSize =
          IntrinsicSize(mIntrinsicSize.width, mIntrinsicSize.height);
      intrinsicRatio =
          AspectRatio::FromSize(mIntrinsicSize.width, mIntrinsicSize.height);
    } else {
      // Image doesn't have a (valid) intrinsic size.
      // Try to look up intrinsic ratio and use that at least.
      intrinsicRatio = imgCon->GetIntrinsicRatio().valueOr(AspectRatio());
    }
    aAnchorPoint.emplace();
    dest = nsLayoutUtils::ComputeObjectDestRect(clientRect, intrinsicSize,
                                                intrinsicRatio, StylePosition(),
                                                aAnchorPoint.ptr());
  }

  return dest;
}

void nsDisplayXULImage::Paint(nsDisplayListBuilder* aBuilder,
                              gfxContext* aCtx) {
  // Even though we call StartDecoding when we get a new image we pass
  // FLAG_SYNC_DECODE_IF_FAST here for the case where the size we draw at is not
  // the intrinsic size of the image and we aren't likely to implement
  // predictive decoding at the correct size for this class like nsImageFrame
  // has.
  uint32_t flags = imgIContainer::FLAG_SYNC_DECODE_IF_FAST;
  if (aBuilder->ShouldSyncDecodeImages())
    flags |= imgIContainer::FLAG_SYNC_DECODE;
  if (aBuilder->IsPaintingToWindow())
    flags |= imgIContainer::FLAG_HIGH_QUALITY_SCALING;

  ImgDrawResult result = static_cast<nsImageBoxFrame*>(mFrame)->PaintImage(
      *aCtx, GetPaintRect(), ToReferenceFrame(), flags);

  nsDisplayItemGenericImageGeometry::UpdateDrawResult(this, result);
}

bool nsDisplayXULImage::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  nsImageBoxFrame* imageFrame = static_cast<nsImageBoxFrame*>(mFrame);
  if (!imageFrame->CanOptimizeToImageLayer()) {
    return false;
  }

  if (!imageFrame->mImageRequest) {
    return true;
  }

  uint32_t flags = imgIContainer::FLAG_SYNC_DECODE_IF_FAST;
  if (aDisplayListBuilder->ShouldSyncDecodeImages()) {
    flags |= imgIContainer::FLAG_SYNC_DECODE;
  }
  if (aDisplayListBuilder->IsPaintingToWindow()) {
    flags |= imgIContainer::FLAG_HIGH_QUALITY_SCALING;
  }

  ImgDrawResult result = imageFrame->CreateWebRenderCommands(
      aBuilder, aResources, aSc, aManager, this, ToReferenceFrame(), flags);
  if (result == ImgDrawResult::NOT_SUPPORTED) {
    return false;
  }

  nsDisplayItemGenericImageGeometry::UpdateDrawResult(this, result);
  return true;
}

nsDisplayItemGeometry* nsDisplayXULImage::AllocateGeometry(
    nsDisplayListBuilder* aBuilder) {
  return new nsDisplayItemGenericImageGeometry(this, aBuilder);
}

void nsDisplayXULImage::ComputeInvalidationRegion(
    nsDisplayListBuilder* aBuilder, const nsDisplayItemGeometry* aGeometry,
    nsRegion* aInvalidRegion) const {
  auto boxFrame = static_cast<nsImageBoxFrame*>(mFrame);
  auto geometry =
      static_cast<const nsDisplayItemGenericImageGeometry*>(aGeometry);

  if (aBuilder->ShouldSyncDecodeImages() && boxFrame->mImageRequest &&
      geometry->ShouldInvalidateToSyncDecodeImages()) {
    bool snap;
    aInvalidRegion->Or(*aInvalidRegion, GetBounds(aBuilder, &snap));
  }

  nsDisplayImageContainer::ComputeInvalidationRegion(aBuilder, aGeometry,
                                                     aInvalidRegion);
}

bool nsDisplayXULImage::CanOptimizeToImageLayer(
    LayerManager* aManager, nsDisplayListBuilder* aBuilder) {
  nsImageBoxFrame* imageFrame = static_cast<nsImageBoxFrame*>(mFrame);
  if (!imageFrame->CanOptimizeToImageLayer()) {
    return false;
  }

  return nsDisplayImageContainer::CanOptimizeToImageLayer(aManager, aBuilder);
}

already_AddRefed<imgIContainer> nsDisplayXULImage::GetImage() {
  nsImageBoxFrame* imageFrame = static_cast<nsImageBoxFrame*>(mFrame);
  if (!imageFrame->mImageRequest) {
    return nullptr;
  }

  nsCOMPtr<imgIContainer> imgCon;
  imageFrame->mImageRequest->GetImage(getter_AddRefs(imgCon));

  return imgCon.forget();
}

nsRect nsDisplayXULImage::GetDestRect() const {
  Maybe<nsPoint> anchorPoint;
  return static_cast<nsImageBoxFrame*>(mFrame)->GetDestRect(ToReferenceFrame(),
                                                            anchorPoint);
}

bool nsImageBoxFrame::CanOptimizeToImageLayer() {
  bool hasSubRect = !mUseSrcAttr && (mSubRect.width > 0 || mSubRect.height > 0);
  if (hasSubRect) {
    return false;
  }
  return true;
}

imgRequestProxy* nsImageBoxFrame::GetRequestFromStyle() {
  const nsStyleDisplay* disp = StyleDisplay();
  if (disp->HasAppearance()) {
    nsPresContext* pc = PresContext();
    if (pc->Theme()->ThemeSupportsWidget(pc, this, disp->mAppearance)) {
      return nullptr;
    }
  }

  return StyleList()->GetListStyleImage();
}

/* virtual */
void nsImageBoxFrame::DidSetComputedStyle(ComputedStyle* aOldStyle) {
  nsLeafBoxFrame::DidSetComputedStyle(aOldStyle);

  // Fetch our subrect.
  const nsStyleList* myList = StyleList();
  mSubRect = myList->GetImageRegion();  // before |mSuppressStyleCheck| test!

  if (mUseSrcAttr || mSuppressStyleCheck)
    return;  // No more work required, since the image isn't specified by style.

  // If the image to use changes, we have a new image.
  nsCOMPtr<nsIURI> oldURI, newURI;
  if (mImageRequest) {
    mImageRequest->GetURI(getter_AddRefs(oldURI));
  }
  if (auto* newImage = GetRequestFromStyle()) {
    newImage->GetURI(getter_AddRefs(newURI));
  }
  bool equal;
  if (newURI == oldURI ||  // handles null==null
      (newURI && oldURI && NS_SUCCEEDED(newURI->Equals(oldURI, &equal)) &&
       equal)) {
    return;
  }

  UpdateImage();
}  // DidSetComputedStyle

void nsImageBoxFrame::GetImageSize() {
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
nsSize nsImageBoxFrame::GetXULPrefSize(nsBoxLayoutState& aState) {
  nsSize size(0, 0);
  DISPLAY_PREF_SIZE(this, size);
  if (XULNeedsRecalc(mImageSize)) {
    GetImageSize();
  }

  if (!mUseSrcAttr && (mSubRect.width > 0 || mSubRect.height > 0)) {
    size = mSubRect.Size();
  } else {
    size = mImageSize;
  }

  nsSize intrinsicSize = size;

  nsMargin borderPadding(0, 0, 0, 0);
  GetXULBorderAndPadding(borderPadding);
  size.width += borderPadding.LeftRight();
  size.height += borderPadding.TopBottom();

  bool widthSet, heightSet;
  nsIFrame::AddXULPrefSize(this, size, widthSet, heightSet);
  NS_ASSERTION(
      size.width != NS_UNCONSTRAINEDSIZE && size.height != NS_UNCONSTRAINEDSIZE,
      "non-intrinsic size expected");

  nsSize minSize = GetXULMinSize(aState);
  nsSize maxSize = GetXULMaxSize(aState);

  if (!widthSet && !heightSet) {
    if (minSize.width != NS_UNCONSTRAINEDSIZE)
      minSize.width -= borderPadding.LeftRight();
    if (minSize.height != NS_UNCONSTRAINEDSIZE)
      minSize.height -= borderPadding.TopBottom();
    if (maxSize.width != NS_UNCONSTRAINEDSIZE)
      maxSize.width -= borderPadding.LeftRight();
    if (maxSize.height != NS_UNCONSTRAINEDSIZE)
      maxSize.height -= borderPadding.TopBottom();

    size = nsLayoutUtils::ComputeAutoSizeWithIntrinsicDimensions(
        minSize.width, minSize.height, maxSize.width, maxSize.height,
        intrinsicSize.width, intrinsicSize.height);
    NS_ASSERTION(size.width != NS_UNCONSTRAINEDSIZE &&
                     size.height != NS_UNCONSTRAINEDSIZE,
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
    } else {
      size.width = intrinsicSize.width;
    }

    size.width += borderPadding.LeftRight();
  } else if (!heightSet) {
    if (intrinsicSize.width > 0) {
      nscoord width = size.width - borderPadding.LeftRight();
      size.height = nscoord(int64_t(width) * int64_t(intrinsicSize.height) /
                            int64_t(intrinsicSize.width));
    } else {
      size.height = intrinsicSize.height;
    }

    size.height += borderPadding.TopBottom();
  }

  return XULBoundsCheck(minSize, size, maxSize);
}

nsSize nsImageBoxFrame::GetXULMinSize(nsBoxLayoutState& aState) {
  // An image can always scale down to (0,0).
  nsSize size(0, 0);
  DISPLAY_MIN_SIZE(this, size);
  AddXULBorderAndPadding(size);
  bool widthSet, heightSet;
  nsIFrame::AddXULMinSize(this, size, widthSet, heightSet);
  return size;
}

nscoord nsImageBoxFrame::GetXULBoxAscent(nsBoxLayoutState& aState) {
  return GetXULPrefSize(aState).height;
}

#ifdef DEBUG_FRAME_DUMP
nsresult nsImageBoxFrame::GetFrameName(nsAString& aResult) const {
  return MakeFrameName(NS_LITERAL_STRING("ImageBox"), aResult);
}
#endif

void nsImageBoxFrame::Notify(imgIRequest* aRequest, int32_t aType,
                             const nsIntRect* aData) {
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
}

void nsImageBoxFrame::OnSizeAvailable(imgIRequest* aRequest,
                                      imgIContainer* aImage) {
  if (NS_WARN_IF(!aImage)) {
    return;
  }

  // Ensure the animation (if any) is started. Note: There is no
  // corresponding call to Decrement for this. This Increment will be
  // 'cleaned up' by the Request when it is destroyed, but only then.
  aRequest->IncrementAnimationConsumers();

  aImage->SetAnimationMode(PresContext()->ImageAnimationMode());

  nscoord w, h;
  aImage->GetWidth(&w);
  aImage->GetHeight(&h);

  mIntrinsicSize.SizeTo(nsPresContext::CSSPixelsToAppUnits(w),
                        nsPresContext::CSSPixelsToAppUnits(h));

  if (!(GetStateBits() & NS_FRAME_FIRST_REFLOW)) {
    PresShell()->FrameNeedsReflow(this, IntrinsicDirty::StyleChange,
                                  NS_FRAME_IS_DIRTY);
  }
}

void nsImageBoxFrame::OnDecodeComplete(imgIRequest* aRequest) {
  nsBoxLayoutState state(PresContext());
  this->XULRedraw(state);
}

void nsImageBoxFrame::OnLoadComplete(imgIRequest* aRequest, nsresult aStatus) {
  if (NS_SUCCEEDED(aStatus)) {
    // Fire an onload DOM event.
    FireImageDOMEvent(mContent, eLoad);
  } else {
    // Fire an onerror DOM event.
    mIntrinsicSize.SizeTo(0, 0);
    PresShell()->FrameNeedsReflow(this, IntrinsicDirty::StyleChange,
                                  NS_FRAME_IS_DIRTY);
    FireImageDOMEvent(mContent, eLoadError);
  }
}

void nsImageBoxFrame::OnImageIsAnimated(imgIRequest* aRequest) {
  // Register with our refresh driver, if we're animated.
  nsLayoutUtils::RegisterImageRequest(PresContext(), aRequest,
                                      &mRequestRegistered);
}

void nsImageBoxFrame::OnFrameUpdate(imgIRequest* aRequest) {
  if ((0 == mRect.width) || (0 == mRect.height)) {
    return;
  }

  // Check if WebRender has interacted with this frame. If it has
  // we need to let it know that things have changed.
  const auto type = DisplayItemType::TYPE_XUL_IMAGE;
  const auto producerId = aRequest->GetProducerId();
  if (WebRenderUserData::ProcessInvalidateForImage(this, type, producerId)) {
    return;
  }

  InvalidateLayer(type);
}

NS_IMPL_ISUPPORTS(nsImageBoxListener, imgINotificationObserver)

nsImageBoxListener::nsImageBoxListener(nsImageBoxFrame* frame)
    : mFrame(frame) {}

nsImageBoxListener::~nsImageBoxListener() = default;

void nsImageBoxListener::Notify(imgIRequest* request, int32_t aType,
                                const nsIntRect* aData) {
  if (!mFrame) {
    return;
  }

  return mFrame->Notify(request, aType, aData);
}
