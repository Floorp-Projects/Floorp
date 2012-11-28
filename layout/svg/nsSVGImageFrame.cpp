/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Keep in (case-insensitive) order:
#include "gfxContext.h"
#include "gfxMatrix.h"
#include "gfxPlatform.h"
#include "imgIContainer.h"
#include "nsIDOMSVGImageElement.h"
#include "nsLayoutUtils.h"
#include "nsRenderingContext.h"
#include "imgINotificationObserver.h"
#include "nsSVGEffects.h"
#include "nsSVGImageElement.h"
#include "nsSVGPathGeometryFrame.h"
#include "nsSVGSVGElement.h"
#include "nsSVGUtils.h"
#include "SVGContentUtils.h"

using namespace mozilla;

class nsSVGImageFrame;

class nsSVGImageListener MOZ_FINAL : public imgINotificationObserver
{
public:
  nsSVGImageListener(nsSVGImageFrame *aFrame);

  NS_DECL_ISUPPORTS
  NS_DECL_IMGINOTIFICATIONOBSERVER

  void SetFrame(nsSVGImageFrame *frame) { mFrame = frame; }

private:
  nsSVGImageFrame *mFrame;
};

typedef nsSVGPathGeometryFrame nsSVGImageFrameBase;

class nsSVGImageFrame : public nsSVGImageFrameBase
{
  friend nsIFrame*
  NS_NewSVGImageFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

protected:
  nsSVGImageFrame(nsStyleContext* aContext) : nsSVGImageFrameBase(aContext) {}
  virtual ~nsSVGImageFrame();

public:
  NS_DECL_FRAMEARENA_HELPERS

  // nsISVGChildFrame interface:
  NS_IMETHOD PaintSVG(nsRenderingContext *aContext, const nsIntRect *aDirtyRect);
  NS_IMETHOD_(nsIFrame*) GetFrameForPoint(const nsPoint &aPoint);
  virtual void ReflowSVG();

  // nsSVGPathGeometryFrame methods:
  virtual uint16_t GetHitTestFlags();

  // nsIFrame interface:
  NS_IMETHOD  AttributeChanged(int32_t         aNameSpaceID,
                               nsIAtom*        aAttribute,
                               int32_t         aModType);
  NS_IMETHOD Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow);
  virtual void DestroyFrom(nsIFrame* aDestructRoot);

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgImageFrame
   */
  virtual nsIAtom* GetType() const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGImage"), aResult);
  }
#endif

private:
  gfxMatrix GetRasterImageTransform(int32_t aNativeWidth,
                                    int32_t aNativeHeight,
                                    uint32_t aFor);
  gfxMatrix GetVectorImageTransform(uint32_t aFor);
  bool      TransformContextForPainting(gfxContext* aGfxContext);

  nsCOMPtr<imgINotificationObserver> mListener;

  nsCOMPtr<imgIContainer> mImageContainer;

  friend class nsSVGImageListener;
};

//----------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGImageFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsSVGImageFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGImageFrame)

nsSVGImageFrame::~nsSVGImageFrame()
{
  // set the frame to null so we don't send messages to a dead object.
  if (mListener) {
    nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(mContent);
    if (imageLoader) {
      // Push a null JSContext on the stack so that code that runs
      // within the below code doesn't think it's being called by
      // JS. See bug 604262.
      nsCxPusher pusher;
      pusher.PushNull();

      imageLoader->RemoveObserver(mListener);
    }
    reinterpret_cast<nsSVGImageListener*>(mListener.get())->SetFrame(nullptr);
  }
  mListener = nullptr;
}

NS_IMETHODIMP
nsSVGImageFrame::Init(nsIContent* aContent,
                      nsIFrame* aParent,
                      nsIFrame* aPrevInFlow)
{
#ifdef DEBUG
  nsCOMPtr<nsIDOMSVGImageElement> image = do_QueryInterface(aContent);
  NS_ASSERTION(image, "Content is not an SVG image!");
#endif

  nsresult rv = nsSVGImageFrameBase::Init(aContent, aParent, aPrevInFlow);
  if (NS_FAILED(rv)) return rv;
  
  mListener = new nsSVGImageListener(this);
  if (!mListener) return NS_ERROR_OUT_OF_MEMORY;
  nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(mContent);
  NS_ENSURE_TRUE(imageLoader, NS_ERROR_UNEXPECTED);

  // We should have a PresContext now, so let's notify our image loader that
  // we need to register any image animations with the refresh driver.
  imageLoader->FrameCreated(this);

  // Push a null JSContext on the stack so that code that runs within
  // the below code doesn't think it's being called by JS. See bug
  // 604262.
  nsCxPusher pusher;
  pusher.PushNull();

  imageLoader->AddObserver(mListener);

  return NS_OK; 
}

/* virtual */ void
nsSVGImageFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  nsCOMPtr<nsIImageLoadingContent> imageLoader =
    do_QueryInterface(nsFrame::mContent);

  if (imageLoader) {
    imageLoader->FrameDestroyed(this);
  }

  nsFrame::DestroyFrom(aDestructRoot);
}

//----------------------------------------------------------------------
// nsIFrame methods:

NS_IMETHODIMP
nsSVGImageFrame::AttributeChanged(int32_t         aNameSpaceID,
                                  nsIAtom*        aAttribute,
                                  int32_t         aModType)
{
  if (aNameSpaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::x ||
        aAttribute == nsGkAtoms::y ||
        aAttribute == nsGkAtoms::width ||
        aAttribute == nsGkAtoms::height) {
      nsSVGUtils::InvalidateBounds(this, false);
      nsSVGUtils::ScheduleReflowSVG(this);
      return NS_OK;
    }
    else if (aAttribute == nsGkAtoms::preserveAspectRatio) {
      nsSVGUtils::InvalidateBounds(this);
      return NS_OK;
    }
  }
  if (aNameSpaceID == kNameSpaceID_XLink &&
      aAttribute == nsGkAtoms::href) {

    // Prevent setting image.src by exiting early
    if (nsContentUtils::IsImageSrcSetDisabled()) {
      return NS_OK;
    }
    nsSVGImageElement *element = static_cast<nsSVGImageElement*>(mContent);

    if (element->mStringAttributes[nsSVGImageElement::HREF].IsExplicitlySet()) {
      element->LoadSVGImage(true, true);
    } else {
      element->CancelImageRequests(true);
    }
  }

  return nsSVGImageFrameBase::AttributeChanged(aNameSpaceID,
                                               aAttribute, aModType);
}

gfxMatrix
nsSVGImageFrame::GetRasterImageTransform(int32_t aNativeWidth,
                                         int32_t aNativeHeight,
                                         uint32_t aFor)
{
  float x, y, width, height;
  nsSVGImageElement *element = static_cast<nsSVGImageElement*>(mContent);
  element->GetAnimatedLengthValues(&x, &y, &width, &height, nullptr);

  gfxMatrix viewBoxTM =
    SVGContentUtils::GetViewBoxTransform(element,
                                         width, height,
                                         0, 0, aNativeWidth, aNativeHeight,
                                         element->mPreserveAspectRatio);

  return viewBoxTM * gfxMatrix().Translate(gfxPoint(x, y)) * GetCanvasTM(aFor);
}

gfxMatrix
nsSVGImageFrame::GetVectorImageTransform(uint32_t aFor)
{
  float x, y, width, height;
  nsSVGImageElement *element = static_cast<nsSVGImageElement*>(mContent);
  element->GetAnimatedLengthValues(&x, &y, &width, &height, nullptr);

  // No viewBoxTM needed here -- our height/width overrides any concept of
  // "native size" that the SVG image has, and it will handle viewBox and
  // preserveAspectRatio on its own once we give it a region to draw into.

  return gfxMatrix().Translate(gfxPoint(x, y)) * GetCanvasTM(aFor);
}

bool
nsSVGImageFrame::TransformContextForPainting(gfxContext* aGfxContext)
{
  gfxMatrix imageTransform;
  if (mImageContainer->GetType() == imgIContainer::TYPE_VECTOR) {
    imageTransform = GetVectorImageTransform(FOR_PAINTING);
  } else {
    int32_t nativeWidth, nativeHeight;
    if (NS_FAILED(mImageContainer->GetWidth(&nativeWidth)) ||
        NS_FAILED(mImageContainer->GetHeight(&nativeHeight)) ||
        nativeWidth == 0 || nativeHeight == 0) {
      return false;
    }
    imageTransform =
      GetRasterImageTransform(nativeWidth, nativeHeight, FOR_PAINTING);
  }

  if (imageTransform.IsSingular()) {
    return false;
  }

  // NOTE: We need to cancel out the effects of Full-Page-Zoom, or else
  // it'll get applied an extra time by DrawSingleUnscaledImage.
  nscoord appUnitsPerDevPx = PresContext()->AppUnitsPerDevPixel();
  gfxFloat pageZoomFactor =
    nsPresContext::AppUnitsToFloatCSSPixels(appUnitsPerDevPx);
  aGfxContext->Multiply(imageTransform.Scale(pageZoomFactor, pageZoomFactor));

  return true;
}

//----------------------------------------------------------------------
// nsISVGChildFrame methods:
NS_IMETHODIMP
nsSVGImageFrame::PaintSVG(nsRenderingContext *aContext,
                          const nsIntRect *aDirtyRect)
{
  nsresult rv = NS_OK;

  if (!GetStyleVisibility()->IsVisible())
    return NS_OK;

  float x, y, width, height;
  nsSVGImageElement *imgElem = static_cast<nsSVGImageElement*>(mContent);
  imgElem->GetAnimatedLengthValues(&x, &y, &width, &height, nullptr);
  NS_ASSERTION(width > 0 && height > 0,
               "Should only be painting things with valid width/height");

  if (!mImageContainer) {
    nsCOMPtr<imgIRequest> currentRequest;
    nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(mContent);
    if (imageLoader)
      imageLoader->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                              getter_AddRefs(currentRequest));

    if (currentRequest)
      currentRequest->GetImage(getter_AddRefs(mImageContainer));
  }

  if (mImageContainer) {
    gfxContext* ctx = aContext->ThebesContext();
    gfxContextAutoSaveRestore autoRestorer(ctx);

    if (GetStyleDisplay()->IsScrollableOverflow()) {
      gfxRect clipRect = nsSVGUtils::GetClipRectForFrame(this, x, y,
                                                         width, height);
      nsSVGUtils::SetClipRect(ctx, GetCanvasTM(FOR_PAINTING), clipRect);
    }

    if (!TransformContextForPainting(ctx)) {
      return NS_ERROR_FAILURE;
    }

    // fill-opacity doesn't affect <image>, so if we're allowed to
    // optimize group opacity, the opacity used for compositing the
    // image into the current canvas is just the group opacity.
    float opacity = 1.0f;
    if (nsSVGUtils::CanOptimizeOpacity(this)) {
      opacity = GetStyleDisplay()->mOpacity;
    }

    if (opacity != 1.0f) {
      ctx->PushGroup(gfxASurface::CONTENT_COLOR_ALPHA);
    }

    nscoord appUnitsPerDevPx = PresContext()->AppUnitsPerDevPixel();
    nsRect dirtyRect; // only used if aDirtyRect is non-null
    if (aDirtyRect) {
      NS_ASSERTION(!NS_SVGDisplayListPaintingEnabled() ||
                   (mState & NS_STATE_SVG_NONDISPLAY_CHILD),
                   "Display lists handle dirty rect intersection test");
      dirtyRect = aDirtyRect->ToAppUnits(appUnitsPerDevPx);
      // Adjust dirtyRect to match our local coordinate system.
      nsRect rootRect =
        nsSVGUtils::TransformFrameRectToOuterSVG(mRect,
                      GetCanvasTM(FOR_PAINTING), PresContext());
      dirtyRect.MoveBy(-rootRect.TopLeft());
    }

    // XXXbholley - I don't think huge images in SVGs are common enough to
    // warrant worrying about the responsiveness impact of doing synchronous
    // decodes. The extra code complexity of determinining when we want to
    // force sync probably just isn't worth it, so always pass FLAG_SYNC_DECODE
    uint32_t drawFlags = imgIContainer::FLAG_SYNC_DECODE;

    if (mImageContainer->GetType() == imgIContainer::TYPE_VECTOR) {
      nsIFrame* imgRootFrame = mImageContainer->GetRootLayoutFrame();
      if (!imgRootFrame) {
        // bad image (e.g. XML parse error in image's SVG file)
        return NS_OK;
      }

      // Grab root node (w/ sanity-check to make sure it exists & is <svg>)
      nsSVGSVGElement* rootSVGElem =
        static_cast<nsSVGSVGElement*>(imgRootFrame->GetContent());
      if (!rootSVGElem || !rootSVGElem->IsSVG(nsGkAtoms::svg)) {
        NS_ABORT_IF_FALSE(false, "missing or non-<svg> root node!!");
        return NS_OK;
      }

      // Override preserveAspectRatio in our helper document
      // XXXdholbert We should technically be overriding the helper doc's clip
      // and overflow properties here, too. See bug 272288 comment 36.
      rootSVGElem->SetImageOverridePreserveAspectRatio(
        imgElem->mPreserveAspectRatio.GetAnimValue());
      nsRect destRect(0, 0,
                      appUnitsPerDevPx * width,
                      appUnitsPerDevPx * height);

      // Note: Can't use DrawSingleUnscaledImage for the TYPE_VECTOR case.
      // That method needs our image to have a fixed native width & height,
      // and that's not always true for TYPE_VECTOR images.
      nsLayoutUtils::DrawSingleImage(
        aContext,
        mImageContainer,
        nsLayoutUtils::GetGraphicsFilterForFrame(this),
        destRect,
        aDirtyRect ? dirtyRect : destRect,
        drawFlags);

      rootSVGElem->ClearImageOverridePreserveAspectRatio();
    } else { // mImageContainer->GetType() == TYPE_RASTER
      nsLayoutUtils::DrawSingleUnscaledImage(
        aContext,
        mImageContainer,
        nsLayoutUtils::GetGraphicsFilterForFrame(this),
        nsPoint(0, 0),
        aDirtyRect ? &dirtyRect : nullptr,
        drawFlags);
    }

    if (opacity != 1.0f) {
      ctx->PopGroupToSource();
      ctx->SetOperator(gfxContext::OPERATOR_OVER);
      ctx->Paint(opacity);
    }
    // gfxContextAutoSaveRestore goes out of scope & cleans up our gfxContext
  }

  return rv;
}

NS_IMETHODIMP_(nsIFrame*)
nsSVGImageFrame::GetFrameForPoint(const nsPoint &aPoint)
{
  // Special case for raster images -- we only want to accept points that fall
  // in the underlying image's (transformed) native bounds.  That region
  // doesn't necessarily map to our <image> element's [x,y,width,height].  So,
  // we have to look up the native image size & our image transform in order
  // to filter out points that fall outside that area.
  if (GetStyleDisplay()->IsScrollableOverflow() && mImageContainer) {
    if (mImageContainer->GetType() == imgIContainer::TYPE_RASTER) {
      int32_t nativeWidth, nativeHeight;
      if (NS_FAILED(mImageContainer->GetWidth(&nativeWidth)) ||
          NS_FAILED(mImageContainer->GetHeight(&nativeHeight)) ||
          nativeWidth == 0 || nativeHeight == 0) {
        return nullptr;
      }

      if (!nsSVGUtils::HitTestRect(
               GetRasterImageTransform(nativeWidth, nativeHeight,
                                       FOR_HIT_TESTING),
               0, 0, nativeWidth, nativeHeight,
               PresContext()->AppUnitsToDevPixels(aPoint.x),
               PresContext()->AppUnitsToDevPixels(aPoint.y))) {
        return nullptr;
      }
    }
    // The special case above doesn't apply to vector images, because they
    // don't limit their drawing to explicit "native bounds" -- they have
    // an infinite canvas on which to place content.  So it's reasonable to
    // just fall back on our <image> element's own bounds here.
  }

  return nsSVGImageFrameBase::GetFrameForPoint(aPoint);
}

nsIAtom *
nsSVGImageFrame::GetType() const
{
  return nsGkAtoms::svgImageFrame;
}

//----------------------------------------------------------------------
// nsSVGPathGeometryFrame methods:

// Lie about our fill/stroke so that covered region and hit detection work properly

void
nsSVGImageFrame::ReflowSVG()
{
  NS_ASSERTION(nsSVGUtils::OuterSVGIsCallingReflowSVG(this),
               "This call is probably a wasteful mistake");

  NS_ABORT_IF_FALSE(!(GetStateBits() & NS_STATE_SVG_NONDISPLAY_CHILD),
                    "ReflowSVG mechanism not designed for this");

  if (!nsSVGUtils::NeedsReflowSVG(this)) {
    return;
  }

  gfxContext tmpCtx(gfxPlatform::GetPlatform()->ScreenReferenceSurface());

  gfxMatrix identity;
  GeneratePath(&tmpCtx, identity);

  gfxRect extent = tmpCtx.GetUserPathExtent();

  if (!extent.IsEmpty()) {
    mRect = nsLayoutUtils::RoundGfxRectToAppRect(extent, 
              PresContext()->AppUnitsPerCSSPixel());
  } else {
    mRect.SetEmpty();
  }

  if (mState & NS_FRAME_FIRST_REFLOW) {
    // Make sure we have our filter property (if any) before calling
    // FinishAndStoreOverflow (subsequent filter changes are handled off
    // nsChangeHint_UpdateEffects):
    nsSVGEffects::UpdateEffects(this);
  }

  nsRect overflow = nsRect(nsPoint(0,0), mRect.Size());
  nsOverflowAreas overflowAreas(overflow, overflow);
  FinishAndStoreOverflow(overflowAreas, mRect.Size());

  mState &= ~(NS_FRAME_FIRST_REFLOW | NS_FRAME_IS_DIRTY |
              NS_FRAME_HAS_DIRTY_CHILDREN);

  // Invalidate, but only if this is not our first reflow (since if it is our
  // first reflow then we haven't had our first paint yet).
  if (!(GetParent()->GetStateBits() & NS_FRAME_FIRST_REFLOW)) {
    InvalidateFrame();
  }
}

uint16_t
nsSVGImageFrame::GetHitTestFlags()
{
  uint16_t flags = 0;

  switch(GetStyleVisibility()->mPointerEvents) {
    case NS_STYLE_POINTER_EVENTS_NONE:
      break;
    case NS_STYLE_POINTER_EVENTS_VISIBLEPAINTED:
    case NS_STYLE_POINTER_EVENTS_AUTO:
      if (GetStyleVisibility()->IsVisible()) {
        /* XXX: should check pixel transparency */
        flags |= SVG_HIT_TEST_FILL;
      }
      break;
    case NS_STYLE_POINTER_EVENTS_VISIBLEFILL:
    case NS_STYLE_POINTER_EVENTS_VISIBLESTROKE:
    case NS_STYLE_POINTER_EVENTS_VISIBLE:
      if (GetStyleVisibility()->IsVisible()) {
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

NS_IMPL_ISUPPORTS1(nsSVGImageListener, imgINotificationObserver)

nsSVGImageListener::nsSVGImageListener(nsSVGImageFrame *aFrame) :  mFrame(aFrame)
{
}

NS_IMETHODIMP
nsSVGImageListener::Notify(imgIRequest *aRequest, int32_t aType, const nsIntRect* aData)
{
  if (!mFrame)
    return NS_ERROR_FAILURE;

  if (aType == imgINotificationObserver::LOAD_COMPLETE) {
    nsSVGUtils::InvalidateBounds(mFrame, false);
    nsSVGUtils::ScheduleReflowSVG(mFrame);
  }

  if (aType == imgINotificationObserver::FRAME_UPDATE) {
    // No new dimensions, so we don't need to call
    // nsSVGUtils::InvalidateAndScheduleBoundsUpdate.
    nsSVGEffects::InvalidateRenderingObservers(mFrame);
    nsSVGUtils::InvalidateBounds(mFrame);
  }

  if (aType == imgINotificationObserver::SIZE_AVAILABLE) {
    // Called once the resource's dimensions have been obtained.
    aRequest->GetImage(getter_AddRefs(mFrame->mImageContainer));
    nsSVGUtils::InvalidateBounds(mFrame, false);
    nsSVGUtils::ScheduleReflowSVG(mFrame);
  }

  return NS_OK;
}

