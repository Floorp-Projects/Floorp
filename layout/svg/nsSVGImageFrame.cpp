/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSVGImageFrame.h"

// Keep in (case-insensitive) order:
#include "gfxContext.h"
#include "gfxPlatform.h"
#include "mozilla/gfx/2D.h"
#include "imgIContainer.h"
#include "nsContainerFrame.h"
#include "nsIImageLoadingContent.h"
#include "nsLayoutUtils.h"
#include "imgINotificationObserver.h"
#include "SVGObserverUtils.h"
#include "nsSVGUtils.h"
#include "SVGContentUtils.h"
#include "SVGGeometryFrame.h"
#include "SVGImageContext.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/MutationEventBinding.h"
#include "mozilla/dom/SVGImageElement.h"
#include "nsIReflowCallback.h"
#include "mozilla/Unused.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::image;

// ---------------------------------------------------------------------
// nsQueryFrame methods
NS_QUERYFRAME_HEAD(nsSVGImageFrame)
  NS_QUERYFRAME_ENTRY(nsSVGImageFrame)
NS_QUERYFRAME_TAIL_INHERITING(SVGGeometryFrame)

nsIFrame* NS_NewSVGImageFrame(PresShell* aPresShell, ComputedStyle* aStyle) {
  return new (aPresShell) nsSVGImageFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGImageFrame)

nsSVGImageFrame::~nsSVGImageFrame() {
  // set the frame to null so we don't send messages to a dead object.
  if (mListener) {
    nsCOMPtr<nsIImageLoadingContent> imageLoader =
        do_QueryInterface(GetContent());
    if (imageLoader) {
      imageLoader->RemoveNativeObserver(mListener);
    }
    reinterpret_cast<nsSVGImageListener*>(mListener.get())->SetFrame(nullptr);
  }
  mListener = nullptr;
}

void nsSVGImageFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                           nsIFrame* aPrevInFlow) {
  NS_ASSERTION(aContent->IsSVGElement(nsGkAtoms::image),
               "Content is not an SVG image!");

  SVGGeometryFrame::Init(aContent, aParent, aPrevInFlow);

  if (GetStateBits() & NS_FRAME_IS_NONDISPLAY) {
    // Non-display frames are likely to be patterns, masks or the like.
    // Treat them as always visible.
    // This call must happen before the FrameCreated. This is because the
    // primary frame pointer on our content node isn't set until after this
    // function ends, so there is no way for the resulting OnVisibilityChange
    // notification to get a frame. FrameCreated has a workaround for this in
    // that it passes our frame around so it can be accessed. OnVisibilityChange
    // doesn't have that workaround.
    IncApproximateVisibleCount();
  }

  mListener = new nsSVGImageListener(this);
  nsCOMPtr<nsIImageLoadingContent> imageLoader =
      do_QueryInterface(GetContent());
  if (!imageLoader) {
    MOZ_CRASH("Why is this not an image loading content?");
  }

  // We should have a PresContext now, so let's notify our image loader that
  // we need to register any image animations with the refresh driver.
  imageLoader->FrameCreated(this);

  imageLoader->AddNativeObserver(mListener);
}

/* virtual */
void nsSVGImageFrame::DestroyFrom(nsIFrame* aDestructRoot,
                                  PostDestroyData& aPostDestroyData) {
  if (GetStateBits() & NS_FRAME_IS_NONDISPLAY) {
    DecApproximateVisibleCount();
  }

  if (mReflowCallbackPosted) {
    PresShell()->CancelReflowCallback(this);
    mReflowCallbackPosted = false;
  }

  nsCOMPtr<nsIImageLoadingContent> imageLoader =
      do_QueryInterface(nsFrame::mContent);

  if (imageLoader) {
    imageLoader->FrameDestroyed(this);
  }

  nsFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}

//----------------------------------------------------------------------
// nsIFrame methods:

nsresult nsSVGImageFrame::AttributeChanged(int32_t aNameSpaceID,
                                           nsAtom* aAttribute,
                                           int32_t aModType) {
  if (aNameSpaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::x || aAttribute == nsGkAtoms::y ||
        aAttribute == nsGkAtoms::width || aAttribute == nsGkAtoms::height) {
      nsLayoutUtils::PostRestyleEvent(
          mContent->AsElement(), RestyleHint{0},
          nsChangeHint_InvalidateRenderingObservers);
      nsSVGUtils::ScheduleReflowSVG(this);
      return NS_OK;
    }
    if (aAttribute == nsGkAtoms::preserveAspectRatio) {
      // We don't paint the content of the image using display lists, therefore
      // we have to invalidate for this children-only transform changes since
      // there is no layer tree to notice that the transform changed and
      // recomposite.
      InvalidateFrame();
      return NS_OK;
    }
  }

  // Currently our SMIL implementation does not modify the DOM attributes. Once
  // we implement the SVG 2 SMIL behaviour this can be removed
  // SVGImageElement::AfterSetAttr's implementation will be sufficient.
  if (aModType == MutationEvent_Binding::SMIL &&
      aAttribute == nsGkAtoms::href &&
      (aNameSpaceID == kNameSpaceID_XLink ||
       aNameSpaceID == kNameSpaceID_None)) {
    SVGImageElement* element = static_cast<SVGImageElement*>(GetContent());

    bool hrefIsSet =
        element->mStringAttributes[SVGImageElement::HREF].IsExplicitlySet() ||
        element->mStringAttributes[SVGImageElement::XLINK_HREF]
            .IsExplicitlySet();
    if (hrefIsSet) {
      element->LoadSVGImage(true, true);
    } else {
      element->CancelImageRequests(true);
    }
  }

  return SVGGeometryFrame::AttributeChanged(aNameSpaceID, aAttribute, aModType);
}

void nsSVGImageFrame::OnVisibilityChange(
    Visibility aNewVisibility, const Maybe<OnNonvisible>& aNonvisibleAction) {
  nsCOMPtr<nsIImageLoadingContent> imageLoader =
      do_QueryInterface(GetContent());
  if (!imageLoader) {
    SVGGeometryFrame::OnVisibilityChange(aNewVisibility, aNonvisibleAction);
    return;
  }

  imageLoader->OnVisibilityChange(aNewVisibility, aNonvisibleAction);

  SVGGeometryFrame::OnVisibilityChange(aNewVisibility, aNonvisibleAction);
}

gfx::Matrix nsSVGImageFrame::GetRasterImageTransform(int32_t aNativeWidth,
                                                     int32_t aNativeHeight) {
  float x, y, width, height;
  SVGImageElement* element = static_cast<SVGImageElement*>(GetContent());
  element->GetAnimatedLengthValues(&x, &y, &width, &height, nullptr);

  Matrix viewBoxTM = SVGContentUtils::GetViewBoxTransform(
      width, height, 0, 0, aNativeWidth, aNativeHeight,
      element->mPreserveAspectRatio);

  return viewBoxTM * gfx::Matrix::Translation(x, y);
}

gfx::Matrix nsSVGImageFrame::GetVectorImageTransform() {
  float x, y, width, height;
  SVGImageElement* element = static_cast<SVGImageElement*>(GetContent());
  element->GetAnimatedLengthValues(&x, &y, &width, &height, nullptr);

  // No viewBoxTM needed here -- our height/width overrides any concept of
  // "native size" that the SVG image has, and it will handle viewBox and
  // preserveAspectRatio on its own once we give it a region to draw into.

  return gfx::Matrix::Translation(x, y);
}

bool nsSVGImageFrame::TransformContextForPainting(gfxContext* aGfxContext,
                                                  const gfxMatrix& aTransform) {
  gfx::Matrix imageTransform;
  if (mImageContainer->GetType() == imgIContainer::TYPE_VECTOR) {
    imageTransform = GetVectorImageTransform() * ToMatrix(aTransform);
  } else {
    int32_t nativeWidth, nativeHeight;
    if (NS_FAILED(mImageContainer->GetWidth(&nativeWidth)) ||
        NS_FAILED(mImageContainer->GetHeight(&nativeHeight)) ||
        nativeWidth == 0 || nativeHeight == 0) {
      return false;
    }
    imageTransform = GetRasterImageTransform(nativeWidth, nativeHeight) *
                     ToMatrix(aTransform);

    // NOTE: We need to cancel out the effects of Full-Page-Zoom, or else
    // it'll get applied an extra time by DrawSingleUnscaledImage.
    nscoord appUnitsPerDevPx = PresContext()->AppUnitsPerDevPixel();
    gfxFloat pageZoomFactor =
        nsPresContext::AppUnitsToFloatCSSPixels(appUnitsPerDevPx);
    imageTransform.PreScale(pageZoomFactor, pageZoomFactor);
  }

  if (imageTransform.IsSingular()) {
    return false;
  }

  aGfxContext->Multiply(ThebesMatrix(imageTransform));
  return true;
}

//----------------------------------------------------------------------
// nsSVGDisplayableFrame methods:
void nsSVGImageFrame::PaintSVG(gfxContext& aContext,
                               const gfxMatrix& aTransform,
                               imgDrawingParams& aImgParams,
                               const nsIntRect* aDirtyRect) {
  if (!StyleVisibility()->IsVisible()) {
    return;
  }

  float x, y, width, height;
  SVGImageElement* imgElem = static_cast<SVGImageElement*>(GetContent());
  imgElem->GetAnimatedLengthValues(&x, &y, &width, &height, nullptr);
  NS_ASSERTION(width > 0 && height > 0,
               "Should only be painting things with valid width/height");

  if (!mImageContainer) {
    nsCOMPtr<imgIRequest> currentRequest;
    nsCOMPtr<nsIImageLoadingContent> imageLoader =
        do_QueryInterface(GetContent());
    if (imageLoader)
      imageLoader->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                              getter_AddRefs(currentRequest));

    if (currentRequest)
      currentRequest->GetImage(getter_AddRefs(mImageContainer));
  }

  if (mImageContainer) {
    gfxContextAutoSaveRestore autoRestorer(&aContext);

    if (StyleDisplay()->IsScrollableOverflow()) {
      gfxRect clipRect =
          nsSVGUtils::GetClipRectForFrame(this, x, y, width, height);
      nsSVGUtils::SetClipRect(&aContext, aTransform, clipRect);
    }

    if (!TransformContextForPainting(&aContext, aTransform)) {
      return;
    }

    // fill-opacity doesn't affect <image>, so if we're allowed to
    // optimize group opacity, the opacity used for compositing the
    // image into the current canvas is just the group opacity.
    float opacity = 1.0f;
    if (nsSVGUtils::CanOptimizeOpacity(this)) {
      opacity = StyleEffects()->mOpacity;
    }

    if (opacity != 1.0f ||
        StyleEffects()->mMixBlendMode != NS_STYLE_BLEND_NORMAL) {
      aContext.PushGroupForBlendBack(gfxContentType::COLOR_ALPHA, opacity);
    }

    nscoord appUnitsPerDevPx = PresContext()->AppUnitsPerDevPixel();
    nsRect dirtyRect;  // only used if aDirtyRect is non-null
    if (aDirtyRect) {
      NS_ASSERTION(!NS_SVGDisplayListPaintingEnabled() ||
                       (mState & NS_FRAME_IS_NONDISPLAY),
                   "Display lists handle dirty rect intersection test");
      dirtyRect = ToAppUnits(*aDirtyRect, appUnitsPerDevPx);
      // Adjust dirtyRect to match our local coordinate system.
      nsRect rootRect = nsSVGUtils::TransformFrameRectToOuterSVG(
          mRect, aTransform, PresContext());
      dirtyRect.MoveBy(-rootRect.TopLeft());
    }

    uint32_t flags = aImgParams.imageFlags;
    if (mForceSyncDecoding) {
      flags |= imgIContainer::FLAG_SYNC_DECODE;
    }

    if (mImageContainer->GetType() == imgIContainer::TYPE_VECTOR) {
      // Package up the attributes of this image element which can override the
      // attributes of mImageContainer's internal SVG document.  The 'width' &
      // 'height' values we're passing in here are in CSS units (though they
      // come from width/height *attributes* in SVG). They influence the region
      // of the SVG image's internal document that is visible, in combination
      // with preserveAspectRatio and viewBox.
      const Maybe<SVGImageContext> context(Some(
          SVGImageContext(Some(CSSIntSize::Truncate(width, height)),
                          Some(imgElem->mPreserveAspectRatio.GetAnimValue()))));

      // For the actual draw operation to draw crisply (and at the right size),
      // our destination rect needs to be |width|x|height|, *in dev pixels*.
      LayoutDeviceSize devPxSize(width, height);
      nsRect destRect(nsPoint(), LayoutDevicePixel::ToAppUnits(
                                     devPxSize, appUnitsPerDevPx));

      // Note: Can't use DrawSingleUnscaledImage for the TYPE_VECTOR case.
      // That method needs our image to have a fixed native width & height,
      // and that's not always true for TYPE_VECTOR images.
      aImgParams.result &= nsLayoutUtils::DrawSingleImage(
          aContext, PresContext(), mImageContainer,
          nsLayoutUtils::GetSamplingFilterForFrame(this), destRect,
          aDirtyRect ? dirtyRect : destRect, context, flags);
    } else {  // mImageContainer->GetType() == TYPE_RASTER
      aImgParams.result &= nsLayoutUtils::DrawSingleUnscaledImage(
          aContext, PresContext(), mImageContainer,
          nsLayoutUtils::GetSamplingFilterForFrame(this), nsPoint(0, 0),
          aDirtyRect ? &dirtyRect : nullptr, Nothing(), flags);
    }

    if (opacity != 1.0f ||
        StyleEffects()->mMixBlendMode != NS_STYLE_BLEND_NORMAL) {
      aContext.PopGroupAndBlend();
    }
    // gfxContextAutoSaveRestore goes out of scope & cleans up our gfxContext
  }
}

nsIFrame* nsSVGImageFrame::GetFrameForPoint(const gfxPoint& aPoint) {
  if (!(GetStateBits() & NS_STATE_SVG_CLIPPATH_CHILD) && !GetHitTestFlags()) {
    return nullptr;
  }

  Rect rect;
  SVGImageElement* element = static_cast<SVGImageElement*>(GetContent());
  element->GetAnimatedLengthValues(&rect.x, &rect.y, &rect.width, &rect.height,
                                   nullptr);

  if (!rect.Contains(ToPoint(aPoint))) {
    return nullptr;
  }

  // Special case for raster images -- we only want to accept points that fall
  // in the underlying image's (scaled to fit) native bounds.  That region
  // doesn't necessarily map to our <image> element's [x,y,width,height] if the
  // raster image's aspect ratio is being preserved.  We have to look up the
  // native image size & our viewBox transform in order to filter out points
  // that fall outside that area.  (This special case doesn't apply to vector
  // images because they don't limit their drawing to explicit "native
  // bounds" -- they have an infinite canvas on which to place content.)
  if (StyleDisplay()->IsScrollableOverflow() && mImageContainer) {
    if (mImageContainer->GetType() == imgIContainer::TYPE_RASTER) {
      int32_t nativeWidth, nativeHeight;
      if (NS_FAILED(mImageContainer->GetWidth(&nativeWidth)) ||
          NS_FAILED(mImageContainer->GetHeight(&nativeHeight)) ||
          nativeWidth == 0 || nativeHeight == 0) {
        return nullptr;
      }
      Matrix viewBoxTM = SVGContentUtils::GetViewBoxTransform(
          rect.width, rect.height, 0, 0, nativeWidth, nativeHeight,
          element->mPreserveAspectRatio);
      if (!nsSVGUtils::HitTestRect(viewBoxTM, 0, 0, nativeWidth, nativeHeight,
                                   aPoint.x - rect.x, aPoint.y - rect.y)) {
        return nullptr;
      }
    }
  }

  return this;
}

//----------------------------------------------------------------------
// SVGGeometryFrame methods:

// Lie about our fill/stroke so that covered region and hit detection work
// properly

void nsSVGImageFrame::ReflowSVG() {
  NS_ASSERTION(nsSVGUtils::OuterSVGIsCallingReflowSVG(this),
               "This call is probably a wasteful mistake");

  MOZ_ASSERT(!(GetStateBits() & NS_FRAME_IS_NONDISPLAY),
             "ReflowSVG mechanism not designed for this");

  if (!nsSVGUtils::NeedsReflowSVG(this)) {
    return;
  }

  float x, y, width, height;
  SVGImageElement* element = static_cast<SVGImageElement*>(GetContent());
  element->GetAnimatedLengthValues(&x, &y, &width, &height, nullptr);

  Rect extent(x, y, width, height);

  if (!extent.IsEmpty()) {
    mRect = nsLayoutUtils::RoundGfxRectToAppRect(extent, AppUnitsPerCSSPixel());
  } else {
    mRect.SetEmpty();
  }

  if (mState & NS_FRAME_FIRST_REFLOW) {
    // Make sure we have our filter property (if any) before calling
    // FinishAndStoreOverflow (subsequent filter changes are handled off
    // nsChangeHint_UpdateEffects):
    SVGObserverUtils::UpdateEffects(this);

    if (!mReflowCallbackPosted) {
      mReflowCallbackPosted = true;
      PresShell()->PostReflowCallback(this);
    }
  }

  nsRect overflow = nsRect(nsPoint(0, 0), mRect.Size());
  nsOverflowAreas overflowAreas(overflow, overflow);
  FinishAndStoreOverflow(overflowAreas, mRect.Size());

  RemoveStateBits(NS_FRAME_FIRST_REFLOW | NS_FRAME_IS_DIRTY |
                  NS_FRAME_HAS_DIRTY_CHILDREN);

  // Invalidate, but only if this is not our first reflow (since if it is our
  // first reflow then we haven't had our first paint yet).
  if (!(GetParent()->GetStateBits() & NS_FRAME_FIRST_REFLOW)) {
    InvalidateFrame();
  }
}

bool nsSVGImageFrame::ReflowFinished() {
  mReflowCallbackPosted = false;

  // XXX(seth): We don't need this. The purpose of updating visibility
  // synchronously is to ensure that animated images start animating
  // immediately. In the short term, however,
  // nsImageLoadingContent::OnUnlockedDraw() is enough to ensure that
  // animations start as soon as the image is painted for the first time, and in
  // the long term we want to update visibility information from the display
  // list whenever we paint, so we don't actually need to do this. However, to
  // avoid behavior changes during the transition from the old image visibility
  // code, we'll leave it in for now.
  UpdateVisibilitySynchronously();

  return false;
}

void nsSVGImageFrame::ReflowCallbackCanceled() {
  mReflowCallbackPosted = false;
}

uint16_t nsSVGImageFrame::GetHitTestFlags() {
  uint16_t flags = 0;

  switch (StyleUI()->mPointerEvents) {
    case NS_STYLE_POINTER_EVENTS_NONE:
      break;
    case NS_STYLE_POINTER_EVENTS_VISIBLEPAINTED:
    case NS_STYLE_POINTER_EVENTS_AUTO:
      if (StyleVisibility()->IsVisible()) {
        /* XXX: should check pixel transparency */
        flags |= SVG_HIT_TEST_FILL;
      }
      break;
    case NS_STYLE_POINTER_EVENTS_VISIBLEFILL:
    case NS_STYLE_POINTER_EVENTS_VISIBLESTROKE:
    case NS_STYLE_POINTER_EVENTS_VISIBLE:
      if (StyleVisibility()->IsVisible()) {
        flags |= SVG_HIT_TEST_FILL;
      }
      break;
    case NS_STYLE_POINTER_EVENTS_PAINTED:
      /* XXX: should check pixel transparency */
      flags |= SVG_HIT_TEST_FILL;
      break;
    case NS_STYLE_POINTER_EVENTS_FILL:
    case NS_STYLE_POINTER_EVENTS_STROKE:
    case NS_STYLE_POINTER_EVENTS_ALL:
      flags |= SVG_HIT_TEST_FILL;
      break;
    default:
      NS_ERROR("not reached");
      break;
  }

  return flags;
}

//----------------------------------------------------------------------
// nsSVGImageListener implementation

NS_IMPL_ISUPPORTS(nsSVGImageListener, imgINotificationObserver)

nsSVGImageListener::nsSVGImageListener(nsSVGImageFrame* aFrame)
    : mFrame(aFrame) {}

NS_IMETHODIMP
nsSVGImageListener::Notify(imgIRequest* aRequest, int32_t aType,
                           const nsIntRect* aData) {
  if (!mFrame) {
    return NS_ERROR_FAILURE;
  }

  if (aType == imgINotificationObserver::LOAD_COMPLETE) {
    mFrame->InvalidateFrame();
    nsLayoutUtils::PostRestyleEvent(mFrame->GetContent()->AsElement(),
                                    RestyleHint{0},
                                    nsChangeHint_InvalidateRenderingObservers);
    nsSVGUtils::ScheduleReflowSVG(mFrame);
  }

  if (aType == imgINotificationObserver::FRAME_UPDATE) {
    // No new dimensions, so we don't need to call
    // nsSVGUtils::InvalidateAndScheduleBoundsUpdate.
    nsLayoutUtils::PostRestyleEvent(mFrame->GetContent()->AsElement(),
                                    RestyleHint{0},
                                    nsChangeHint_InvalidateRenderingObservers);
    mFrame->InvalidateFrame();
  }

  if (aType == imgINotificationObserver::SIZE_AVAILABLE) {
    // Called once the resource's dimensions have been obtained.
    nsCOMPtr<imgIContainer> image;
    aRequest->GetImage(getter_AddRefs(image));
    if (image) {
      image->SetAnimationMode(mFrame->PresContext()->ImageAnimationMode());
      mFrame->mImageContainer = image.forget();
    }
    mFrame->InvalidateFrame();
    nsLayoutUtils::PostRestyleEvent(mFrame->GetContent()->AsElement(),
                                    RestyleHint{0},
                                    nsChangeHint_InvalidateRenderingObservers);
    nsSVGUtils::ScheduleReflowSVG(mFrame);
  }

  return NS_OK;
}
