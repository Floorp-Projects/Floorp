/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGImageFrame.h"

// Keep in (case-insensitive) order:
#include "gfxContext.h"
#include "gfxPlatform.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/layers/RenderRootStateManager.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "imgIContainer.h"
#include "nsContainerFrame.h"
#include "nsIImageLoadingContent.h"
#include "nsLayoutUtils.h"
#include "imgINotificationObserver.h"
#include "SVGGeometryProperty.h"
#include "SVGGeometryFrame.h"
#include "mozilla/PresShell.h"
#include "mozilla/SVGContentUtils.h"
#include "mozilla/SVGImageContext.h"
#include "mozilla/SVGObserverUtils.h"
#include "mozilla/SVGUtils.h"
#include "mozilla/dom/MutationEventBinding.h"
#include "mozilla/dom/SVGImageElement.h"
#include "nsIReflowCallback.h"
#include "mozilla/Unused.h"

using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::image;
using namespace mozilla::dom::SVGPreserveAspectRatio_Binding;
namespace SVGT = SVGGeometryProperty::Tags;

namespace mozilla {

class SVGImageListener final : public imgINotificationObserver {
 public:
  explicit SVGImageListener(SVGImageFrame* aFrame);

  NS_DECL_ISUPPORTS
  NS_DECL_IMGINOTIFICATIONOBSERVER

  void SetFrame(SVGImageFrame* frame) { mFrame = frame; }

 private:
  ~SVGImageListener() = default;

  SVGImageFrame* mFrame;
};

// ---------------------------------------------------------------------
// nsQueryFrame methods
NS_QUERYFRAME_HEAD(SVGImageFrame)
  NS_QUERYFRAME_ENTRY(SVGImageFrame)
NS_QUERYFRAME_TAIL_INHERITING(SVGGeometryFrame)

}  // namespace mozilla

nsIFrame* NS_NewSVGImageFrame(mozilla::PresShell* aPresShell,
                              mozilla::ComputedStyle* aStyle) {
  return new (aPresShell)
      mozilla::SVGImageFrame(aStyle, aPresShell->GetPresContext());
}

namespace mozilla {

NS_IMPL_FRAMEARENA_HELPERS(SVGImageFrame)

SVGImageFrame::~SVGImageFrame() {
  // set the frame to null so we don't send messages to a dead object.
  if (mListener) {
    nsCOMPtr<nsIImageLoadingContent> imageLoader =
        do_QueryInterface(GetContent());
    if (imageLoader) {
      imageLoader->RemoveNativeObserver(mListener);
    }
    reinterpret_cast<SVGImageListener*>(mListener.get())->SetFrame(nullptr);
  }
  mListener = nullptr;
}

void SVGImageFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                         nsIFrame* aPrevInFlow) {
  NS_ASSERTION(aContent->IsSVGElement(nsGkAtoms::image),
               "Content is not an SVG image!");

  SVGGeometryFrame::Init(aContent, aParent, aPrevInFlow);

  if (HasAnyStateBits(NS_FRAME_IS_NONDISPLAY)) {
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

  mListener = new SVGImageListener(this);
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
void SVGImageFrame::DestroyFrom(nsIFrame* aDestructRoot,
                                PostDestroyData& aPostDestroyData) {
  if (HasAnyStateBits(NS_FRAME_IS_NONDISPLAY)) {
    DecApproximateVisibleCount();
  }

  if (mReflowCallbackPosted) {
    PresShell()->CancelReflowCallback(this);
    mReflowCallbackPosted = false;
  }

  nsCOMPtr<nsIImageLoadingContent> imageLoader =
      do_QueryInterface(nsIFrame::mContent);

  if (imageLoader) {
    imageLoader->FrameDestroyed(this);
  }

  nsIFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}

/* virtual */
void SVGImageFrame::DidSetComputedStyle(ComputedStyle* aOldStyle) {
  SVGGeometryFrame::DidSetComputedStyle(aOldStyle);

  if (!mImageContainer || !aOldStyle) {
    return;
  }

  auto newOrientation = StyleVisibility()->mImageOrientation;

  if (aOldStyle->StyleVisibility()->mImageOrientation != newOrientation) {
    nsCOMPtr<imgIContainer> image(mImageContainer->Unwrap());
    mImageContainer = nsLayoutUtils::OrientImage(image, newOrientation);
  }

  // TODO(heycam): We should handle aspect-ratio, like nsImageFrame does.
}

//----------------------------------------------------------------------
// nsIFrame methods:

nsresult SVGImageFrame::AttributeChanged(int32_t aNameSpaceID,
                                         nsAtom* aAttribute, int32_t aModType) {
  if (aNameSpaceID == kNameSpaceID_None) {
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

void SVGImageFrame::OnVisibilityChange(
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

gfx::Matrix SVGImageFrame::GetRasterImageTransform(int32_t aNativeWidth,
                                                   int32_t aNativeHeight) {
  float x, y, width, height;
  SVGImageElement* element = static_cast<SVGImageElement*>(GetContent());
  SVGGeometryProperty::ResolveAll<SVGT::X, SVGT::Y, SVGT::Width, SVGT::Height>(
      element, &x, &y, &width, &height);

  Matrix viewBoxTM = SVGContentUtils::GetViewBoxTransform(
      width, height, 0, 0, aNativeWidth, aNativeHeight,
      element->mPreserveAspectRatio);

  return viewBoxTM * gfx::Matrix::Translation(x, y);
}

gfx::Matrix SVGImageFrame::GetVectorImageTransform() {
  float x, y;
  SVGImageElement* element = static_cast<SVGImageElement*>(GetContent());
  SVGGeometryProperty::ResolveAll<SVGT::X, SVGT::Y>(element, &x, &y);

  // No viewBoxTM needed here -- our height/width overrides any concept of
  // "native size" that the SVG image has, and it will handle viewBox and
  // preserveAspectRatio on its own once we give it a region to draw into.

  return gfx::Matrix::Translation(x, y);
}

bool SVGImageFrame::GetIntrinsicImageDimensions(
    mozilla::gfx::Size& aSize, mozilla::AspectRatio& aAspectRatio) const {
  if (!mImageContainer) {
    return false;
  }

  ImageResolution resolution = mImageContainer->GetResolution();

  int32_t width, height;
  if (NS_FAILED(mImageContainer->GetWidth(&width))) {
    aSize.width = -1;
  } else {
    aSize.width = width;
    resolution.ApplyXTo(aSize.width);
  }

  if (NS_FAILED(mImageContainer->GetHeight(&height))) {
    aSize.height = -1;
  } else {
    aSize.height = height;
    resolution.ApplyYTo(aSize.height);
  }

  Maybe<AspectRatio> asp = mImageContainer->GetIntrinsicRatio();
  aAspectRatio = asp.valueOr(AspectRatio{});

  return true;
}

bool SVGImageFrame::TransformContextForPainting(gfxContext* aGfxContext,
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
    mImageContainer->GetResolution().ApplyTo(nativeWidth, nativeHeight);
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
// ISVGDisplayableFrame methods:
void SVGImageFrame::PaintSVG(gfxContext& aContext, const gfxMatrix& aTransform,
                             imgDrawingParams& aImgParams,
                             const nsIntRect* aDirtyRect) {
  if (!StyleVisibility()->IsVisible()) {
    return;
  }

  float x, y, width, height;
  SVGImageElement* imgElem = static_cast<SVGImageElement*>(GetContent());
  SVGGeometryProperty::ResolveAll<SVGT::X, SVGT::Y, SVGT::Width, SVGT::Height>(
      imgElem, &x, &y, &width, &height);
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
          SVGUtils::GetClipRectForFrame(this, x, y, width, height);
      SVGUtils::SetClipRect(&aContext, aTransform, clipRect);
    }

    if (!TransformContextForPainting(&aContext, aTransform)) {
      return;
    }

    // fill-opacity doesn't affect <image>, so if we're allowed to
    // optimize group opacity, the opacity used for compositing the
    // image into the current canvas is just the group opacity.
    float opacity = 1.0f;
    if (SVGUtils::CanOptimizeOpacity(this)) {
      opacity = StyleEffects()->mOpacity;
    }

    if (opacity != 1.0f ||
        StyleEffects()->mMixBlendMode != StyleBlend::Normal) {
      aContext.PushGroupForBlendBack(gfxContentType::COLOR_ALPHA, opacity);
    }

    nscoord appUnitsPerDevPx = PresContext()->AppUnitsPerDevPixel();
    nsRect dirtyRect;  // only used if aDirtyRect is non-null
    if (aDirtyRect) {
      NS_ASSERTION(!NS_SVGDisplayListPaintingEnabled() ||
                       (mState & NS_FRAME_IS_NONDISPLAY),
                   "Display lists handle dirty rect intersection test");
      dirtyRect = ToAppUnits(*aDirtyRect, appUnitsPerDevPx);

      // dirtyRect is relative to the outer <svg>, we should transform it
      // down to <image>.
      Rect dir(dirtyRect.x, dirtyRect.y, dirtyRect.width, dirtyRect.height);
      dir.Scale(1.f / AppUnitsPerCSSPixel());

      // FIXME: This isn't correct if there is an inner <svg> enclosing
      // the <image>. But that seems to be a quite obscure usecase, we can
      // add a dedicated utility for that purpose to replace the GetCTM
      // here if necessary.
      auto mat = SVGContentUtils::GetCTM(
          static_cast<SVGImageElement*>(GetContent()), false);
      if (mat.IsSingular()) {
        return;
      }

      mat.Invert();
      dir = mat.TransformRect(dir);

      // x, y offset of <image> is not included in CTM.
      dir.MoveBy(-x, -y);

      dir.Scale(AppUnitsPerCSSPixel());
      dir.Round();
      dirtyRect = nsRect(dir.x, dir.y, dir.width, dir.height);
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
        StyleEffects()->mMixBlendMode != StyleBlend::Normal) {
      aContext.PopGroupAndBlend();
    }
    // gfxContextAutoSaveRestore goes out of scope & cleans up our gfxContext
  }
}

bool SVGImageFrame::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const mozilla::layers::StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder, DisplaySVGGeometry* aItem,
    bool aDryRun) {
  if (!StyleVisibility()->IsVisible()) {
    return true;
  }

  float opacity = 1.0f;
  if (SVGUtils::CanOptimizeOpacity(this)) {
    opacity = StyleEffects()->mOpacity;
  }

  if (opacity != 1.0f) {
    // FIXME: not implemented, might be trivial
    return false;
  }
  if (StyleEffects()->mMixBlendMode != StyleBlend::Normal) {
    // FIXME: not implemented
    return false;
  }

  // try to setup the image
  if (!mImageContainer) {
    nsCOMPtr<imgIRequest> currentRequest;
    nsCOMPtr<nsIImageLoadingContent> imageLoader =
        do_QueryInterface(GetContent());
    if (imageLoader) {
      imageLoader->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                              getter_AddRefs(currentRequest));
    }

    if (currentRequest) {
      currentRequest->GetImage(getter_AddRefs(mImageContainer));
    }
  }

  if (!mImageContainer) {
    // nothing to draw (yet)
    return true;
  }

  uint32_t flags = aDisplayListBuilder->GetImageDecodeFlags();

  // Compute bounds of the image
  nscoord appUnitsPerDevPx = PresContext()->AppUnitsPerDevPixel();
  int32_t appUnitsPerCSSPixel = AppUnitsPerCSSPixel();

  float x, y, width, height;
  SVGImageElement* imgElem = static_cast<SVGImageElement*>(GetContent());
  SVGGeometryProperty::ResolveAll<SVGT::X, SVGT::Y, SVGT::Width, SVGT::Height>(
      imgElem, &x, &y, &width, &height);
  NS_ASSERTION(width > 0 && height > 0,
               "Should only be painting things with valid width/height");

  auto toReferenceFrame = aItem->ToReferenceFrame();
  auto appRect = nsLayoutUtils::RoundGfxRectToAppRect(Rect(0, 0, width, height),
                                                      appUnitsPerCSSPixel);
  appRect += toReferenceFrame;
  auto destRect = LayoutDeviceRect::FromAppUnits(appRect, appUnitsPerDevPx);
  auto clipRect = destRect;

  if (StyleDisplay()->IsScrollableOverflow()) {
    // Apply potential non-trivial clip
    auto cssClip = SVGUtils::GetClipRectForFrame(this, 0, 0, width, height);
    auto appClip =
        nsLayoutUtils::RoundGfxRectToAppRect(cssClip, appUnitsPerCSSPixel);
    appClip += toReferenceFrame;
    clipRect = LayoutDeviceRect::FromAppUnits(appClip, appUnitsPerDevPx);

    // Apply preserveAspectRatio
    if (mImageContainer->GetType() == imgIContainer::TYPE_RASTER) {
      int32_t nativeWidth, nativeHeight;
      if (NS_FAILED(mImageContainer->GetWidth(&nativeWidth)) ||
          NS_FAILED(mImageContainer->GetHeight(&nativeHeight)) ||
          nativeWidth == 0 || nativeHeight == 0) {
        // Image has no size; nothing to draw
        return true;
      }

      auto preserveAspectRatio = imgElem->mPreserveAspectRatio.GetAnimValue();
      uint16_t align = preserveAspectRatio.GetAlign();
      uint16_t meetOrSlice = preserveAspectRatio.GetMeetOrSlice();

      // default to the defaults
      if (align == SVG_PRESERVEASPECTRATIO_UNKNOWN) {
        align = SVG_PRESERVEASPECTRATIO_XMIDYMID;
      }
      if (meetOrSlice == SVG_MEETORSLICE_UNKNOWN) {
        meetOrSlice = SVG_MEETORSLICE_MEET;
      }

      // aspect > 1 is horizontal
      // aspect < 1 is vertical
      float nativeAspect = ((float)nativeWidth) / ((float)nativeHeight);
      float viewAspect = width / height;

      // "Meet" is "fit image to view"; "Slice" is "cover view with image".
      //
      // Whether we meet or slice, one side of the destRect will always be
      // perfectly spanned by our image. The only questions to answer are
      // "which side won't span perfectly" and "should that side be grown
      // or shrunk".
      //
      // Because we fit our image to the destRect, this all just reduces to:
      // "if meet, shrink to fit. if slice, grow to fit."
      if (align != SVG_PRESERVEASPECTRATIO_NONE && nativeAspect != viewAspect) {
        // Slightly redundant bools, but they make the conditions clearer
        bool tooTall = nativeAspect > viewAspect;
        bool tooWide = nativeAspect < viewAspect;
        if ((meetOrSlice == SVG_MEETORSLICE_MEET && tooTall) ||
            (meetOrSlice == SVG_MEETORSLICE_SLICE && tooWide)) {
          // Adjust height and realign y
          auto oldHeight = destRect.height;
          destRect.height = destRect.width / nativeAspect;
          auto heightChange = oldHeight - destRect.height;
          switch (align) {
            case SVG_PRESERVEASPECTRATIO_XMINYMIN:
            case SVG_PRESERVEASPECTRATIO_XMIDYMIN:
            case SVG_PRESERVEASPECTRATIO_XMAXYMIN:
              // align to top (no-op)
              break;
            case SVG_PRESERVEASPECTRATIO_XMINYMID:
            case SVG_PRESERVEASPECTRATIO_XMIDYMID:
            case SVG_PRESERVEASPECTRATIO_XMAXYMID:
              // align to center
              destRect.y += heightChange / 2.0f;
              break;
            case SVG_PRESERVEASPECTRATIO_XMINYMAX:
            case SVG_PRESERVEASPECTRATIO_XMIDYMAX:
            case SVG_PRESERVEASPECTRATIO_XMAXYMAX:
              // align to bottom
              destRect.y += heightChange;
              break;
            default:
              MOZ_ASSERT_UNREACHABLE("Unknown value for align");
          }
        } else if ((meetOrSlice == SVG_MEETORSLICE_MEET && tooWide) ||
                   (meetOrSlice == SVG_MEETORSLICE_SLICE && tooTall)) {
          // Adjust width and realign x
          auto oldWidth = destRect.width;
          destRect.width = destRect.height * nativeAspect;
          auto widthChange = oldWidth - destRect.width;
          switch (align) {
            case SVG_PRESERVEASPECTRATIO_XMINYMIN:
            case SVG_PRESERVEASPECTRATIO_XMINYMID:
            case SVG_PRESERVEASPECTRATIO_XMINYMAX:
              // align to left (no-op)
              break;
            case SVG_PRESERVEASPECTRATIO_XMIDYMIN:
            case SVG_PRESERVEASPECTRATIO_XMIDYMID:
            case SVG_PRESERVEASPECTRATIO_XMIDYMAX:
              // align to center
              destRect.x += widthChange / 2.0f;
              break;
            case SVG_PRESERVEASPECTRATIO_XMAXYMIN:
            case SVG_PRESERVEASPECTRATIO_XMAXYMID:
            case SVG_PRESERVEASPECTRATIO_XMAXYMAX:
              // align to right
              destRect.x += widthChange;
              break;
            default:
              MOZ_ASSERT_UNREACHABLE("Unknown value for align");
          }
        }
      }
    }
  }

  Maybe<SVGImageContext> svgContext;
  if (mImageContainer->GetType() == imgIContainer::TYPE_VECTOR) {
    // Forward preserveAspectRatio to inner SVGs
    svgContext.emplace(Some(CSSIntSize::Truncate(width, height)),
                       Some(imgElem->mPreserveAspectRatio.GetAnimValue()));
  }

  IntSize decodeSize = nsLayoutUtils::ComputeImageContainerDrawingParameters(
      mImageContainer, this, destRect, aSc, flags, svgContext);

  RefPtr<layers::ImageContainer> container;
  ImgDrawResult drawResult = mImageContainer->GetImageContainerAtSize(
      aManager->LayerManager(), decodeSize, svgContext, flags,
      getter_AddRefs(container));

  // While we got a container, it may not contain a fully decoded surface. If
  // that is the case, and we have an image we were previously displaying which
  // has a fully decoded surface, then we should prefer the previous image.
  switch (drawResult) {
    case ImgDrawResult::NOT_READY:
    case ImgDrawResult::INCOMPLETE:
    case ImgDrawResult::TEMPORARY_ERROR:
      // nothing to draw (yet)
      return true;
    case ImgDrawResult::NOT_SUPPORTED:
      // things we haven't implemented for WR yet
      return false;
    default:
      // image is ready to draw
      break;
  }

  // Don't do any actual mutations to state if we're doing a dry run
  // (used to decide if we're making this into an active layer)
  if (!aDryRun) {
    // If the image container is empty, we don't want to fallback. Any other
    // failure will be due to resource constraints and fallback is unlikely to
    // help us. Hence we can ignore the return value from PushImage.
    if (container) {
      aManager->CommandBuilder().PushImage(aItem, container, aBuilder,
                                           aResources, aSc, destRect, clipRect);
    }

    nsDisplayItemGenericImageGeometry::UpdateDrawResult(aItem, drawResult);
  }

  return true;
}

nsIFrame* SVGImageFrame::GetFrameForPoint(const gfxPoint& aPoint) {
  if (!HasAnyStateBits(NS_STATE_SVG_CLIPPATH_CHILD) && !GetHitTestFlags()) {
    return nullptr;
  }

  Rect rect;
  SVGImageElement* element = static_cast<SVGImageElement*>(GetContent());
  SVGGeometryProperty::ResolveAll<SVGT::X, SVGT::Y, SVGT::Width, SVGT::Height>(
      element, &rect.x, &rect.y, &rect.width, &rect.height);

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
      mImageContainer->GetResolution().ApplyTo(nativeWidth, nativeHeight);
      Matrix viewBoxTM = SVGContentUtils::GetViewBoxTransform(
          rect.width, rect.height, 0, 0, nativeWidth, nativeHeight,
          element->mPreserveAspectRatio);
      if (!SVGUtils::HitTestRect(viewBoxTM, 0, 0, nativeWidth, nativeHeight,
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

void SVGImageFrame::ReflowSVG() {
  NS_ASSERTION(SVGUtils::OuterSVGIsCallingReflowSVG(this),
               "This call is probably a wasteful mistake");

  MOZ_ASSERT(!HasAnyStateBits(NS_FRAME_IS_NONDISPLAY),
             "ReflowSVG mechanism not designed for this");

  if (!SVGUtils::NeedsReflowSVG(this)) {
    return;
  }

  float x, y, width, height;
  SVGImageElement* element = static_cast<SVGImageElement*>(GetContent());
  SVGGeometryProperty::ResolveAll<SVGT::X, SVGT::Y, SVGT::Width, SVGT::Height>(
      element, &x, &y, &width, &height);

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
  OverflowAreas overflowAreas(overflow, overflow);
  FinishAndStoreOverflow(overflowAreas, mRect.Size());

  RemoveStateBits(NS_FRAME_FIRST_REFLOW | NS_FRAME_IS_DIRTY |
                  NS_FRAME_HAS_DIRTY_CHILDREN);

  // Invalidate, but only if this is not our first reflow (since if it is our
  // first reflow then we haven't had our first paint yet).
  if (!GetParent()->HasAnyStateBits(NS_FRAME_FIRST_REFLOW)) {
    InvalidateFrame();
  }
}

bool SVGImageFrame::ReflowFinished() {
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

void SVGImageFrame::ReflowCallbackCanceled() { mReflowCallbackPosted = false; }

uint16_t SVGImageFrame::GetHitTestFlags() {
  uint16_t flags = 0;

  switch (StyleUI()->mPointerEvents) {
    case StylePointerEvents::None:
      break;
    case StylePointerEvents::Visiblepainted:
    case StylePointerEvents::Auto:
      if (StyleVisibility()->IsVisible()) {
        /* XXX: should check pixel transparency */
        flags |= SVG_HIT_TEST_FILL;
      }
      break;
    case StylePointerEvents::Visiblefill:
    case StylePointerEvents::Visiblestroke:
    case StylePointerEvents::Visible:
      if (StyleVisibility()->IsVisible()) {
        flags |= SVG_HIT_TEST_FILL;
      }
      break;
    case StylePointerEvents::Painted:
      /* XXX: should check pixel transparency */
      flags |= SVG_HIT_TEST_FILL;
      break;
    case StylePointerEvents::Fill:
    case StylePointerEvents::Stroke:
    case StylePointerEvents::All:
      flags |= SVG_HIT_TEST_FILL;
      break;
    default:
      NS_ERROR("not reached");
      break;
  }

  return flags;
}

//----------------------------------------------------------------------
// SVGImageListener implementation

NS_IMPL_ISUPPORTS(SVGImageListener, imgINotificationObserver)

SVGImageListener::SVGImageListener(SVGImageFrame* aFrame) : mFrame(aFrame) {}

void SVGImageListener::Notify(imgIRequest* aRequest, int32_t aType,
                              const nsIntRect* aData) {
  if (!mFrame) {
    return;
  }

  if (aType == imgINotificationObserver::LOAD_COMPLETE) {
    mFrame->InvalidateFrame();
    nsLayoutUtils::PostRestyleEvent(mFrame->GetContent()->AsElement(),
                                    RestyleHint{0},
                                    nsChangeHint_InvalidateRenderingObservers);
    SVGUtils::ScheduleReflowSVG(mFrame);
  }

  if (aType == imgINotificationObserver::FRAME_UPDATE) {
    // No new dimensions, so we don't need to call
    // SVGUtils::InvalidateAndScheduleBoundsUpdate.
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
      image = nsLayoutUtils::OrientImage(
          image, mFrame->StyleVisibility()->mImageOrientation);
      image->SetAnimationMode(mFrame->PresContext()->ImageAnimationMode());
      mFrame->mImageContainer = std::move(image);
    }
    mFrame->InvalidateFrame();
    nsLayoutUtils::PostRestyleEvent(mFrame->GetContent()->AsElement(),
                                    RestyleHint{0},
                                    nsChangeHint_InvalidateRenderingObservers);
    SVGUtils::ScheduleReflowSVG(mFrame);
  }
}

}  // namespace mozilla
