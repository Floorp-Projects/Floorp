/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for replaced elements with image data */

#include "nsImageFrame.h"

#include "gfx2DGlue.h"
#include "gfxUtils.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/EventStates.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Helpers.h"
#include "mozilla/gfx/PathHelpers.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/Unused.h"

#include "nsCOMPtr.h"
#include "nsFontMetrics.h"
#include "nsIImageLoadingContent.h"
#include "nsString.h"
#include "nsPrintfCString.h"
#include "nsPresContext.h"
#include "nsRenderingContext.h"
#include "nsIPresShell.h"
#include "nsGkAtoms.h"
#include "nsIDocument.h"
#include "nsContentUtils.h"
#include "nsCSSAnonBoxes.h"
#include "nsStyleContext.h"
#include "nsStyleConsts.h"
#include "nsStyleCoord.h"
#include "nsStyleUtil.h"
#include "nsTransform2D.h"
#include "nsImageMap.h"
#include "nsIIOService.h"
#include "nsILoadGroup.h"
#include "nsISupportsPriority.h"
#include "nsNetUtil.h"
#include "nsNetCID.h"
#include "nsCSSRendering.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsNameSpaceManager.h"
#include <algorithm>
#ifdef ACCESSIBILITY
#include "nsAccessibilityService.h"
#endif
#include "nsIDOMNode.h"
#include "nsLayoutUtils.h"
#include "nsDisplayList.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "FrameLayerBuilder.h"
#include "nsISelectionController.h"
#include "nsISelection.h"

#include "imgIContainer.h"
#include "imgLoader.h"
#include "imgRequestProxy.h"

#include "nsCSSFrameConstructor.h"
#include "nsIDOMRange.h"

#include "nsError.h"
#include "nsBidiUtils.h"
#include "nsBidiPresUtils.h"

#include "gfxRect.h"
#include "ImageLayers.h"
#include "ImageContainer.h"
#include "mozilla/StyleSetHandle.h"
#include "mozilla/StyleSetHandleInlines.h"
#include "nsBlockFrame.h"
#include "nsStyleStructInlines.h"

#include "mozilla/Preferences.h"

#include "mozilla/dom/Link.h"
#include "SVGImageContext.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::image;
using namespace mozilla::layers;

// sizes (pixels) for image icon, padding and border frame
#define ICON_SIZE        (16)
#define ICON_PADDING     (3)
#define ALT_BORDER_WIDTH (1)

//we must add hooks soon
#define IMAGE_EDITOR_CHECK 1

// Default alignment value (so we can tell an unset value from a set value)
#define ALIGN_UNSET uint8_t(-1)

// static icon information
nsImageFrame::IconLoad* nsImageFrame::gIconLoad = nullptr;

// cached IO service for loading icons
nsIIOService* nsImageFrame::sIOService;

// test if the width and height are fixed, looking at the style data
// This is used by nsImageFrame::ShouldCreateImageFrameFor and should
// not be used for layout decisions.
static bool HaveSpecifiedSize(const nsStylePosition* aStylePosition)
{
  // check the width and height values in the reflow state's style struct
  // - if width and height are specified as either coord or percentage, then
  //   the size of the image frame is constrained
  return aStylePosition->mWidth.IsCoordPercentCalcUnit() &&
         aStylePosition->mHeight.IsCoordPercentCalcUnit();
}

// Decide whether we can optimize away reflows that result from the
// image's intrinsic size changing.
inline bool HaveFixedSize(const ReflowInput& aReflowInput)
{
  NS_ASSERTION(aReflowInput.mStylePosition, "crappy reflowInput - null stylePosition");
  // Don't try to make this optimization when an image has percentages
  // in its 'width' or 'height'.  The percentages might be treated like
  // auto (especially for intrinsic width calculations and for heights).
  return aReflowInput.mStylePosition->mHeight.ConvertsToLength() &&
         aReflowInput.mStylePosition->mWidth.ConvertsToLength();
}

nsIFrame*
NS_NewImageFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsImageFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsImageFrame)


nsImageFrame::nsImageFrame(nsStyleContext* aContext) :
  nsAtomicContainerFrame(aContext),
  mComputedSize(0, 0),
  mIntrinsicRatio(0, 0),
  mDisplayingIcon(false),
  mFirstFrameComplete(false),
  mReflowCallbackPosted(false),
  mForceSyncDecoding(false)
{
  EnableVisibilityTracking();

  // We assume our size is not constrained and we haven't gotten an
  // initial reflow yet, so don't touch those flags.
  mIntrinsicSize.width.SetCoordValue(0);
  mIntrinsicSize.height.SetCoordValue(0);
}

nsImageFrame::~nsImageFrame()
{
}

NS_QUERYFRAME_HEAD(nsImageFrame)
  NS_QUERYFRAME_ENTRY(nsImageFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsAtomicContainerFrame)

#ifdef ACCESSIBILITY
a11y::AccType
nsImageFrame::AccessibleType()
{
  // Don't use GetImageMap() to avoid reentrancy into accessibility.
  if (HasImageMap()) {
    return a11y::eHTMLImageMapType;
  }

  return a11y::eImageType;
}
#endif

void
nsImageFrame::DisconnectMap()
{
  if (mImageMap) {
    mImageMap->Destroy();
    mImageMap = nullptr;

#ifdef ACCESSIBILITY
  nsAccessibilityService* accService = GetAccService();
  if (accService) {
    accService->RecreateAccessible(PresContext()->PresShell(), mContent);
  }
#endif
  }
}

void
nsImageFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  if (mReflowCallbackPosted) {
    PresContext()->PresShell()->CancelReflowCallback(this);
    mReflowCallbackPosted = false;
  }

  // Tell our image map, if there is one, to clean up
  // This causes the nsImageMap to unregister itself as
  // a DOM listener.
  DisconnectMap();

  // set the frame to null so we don't send messages to a dead object.
  if (mListener) {
    nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(mContent);
    if (imageLoader) {
      // Notify our image loading content that we are going away so it can
      // deregister with our refresh driver.
      imageLoader->FrameDestroyed(this);

      imageLoader->RemoveObserver(mListener);
    }
    
    reinterpret_cast<nsImageListener*>(mListener.get())->SetFrame(nullptr);
  }
  
  mListener = nullptr;

  // If we were displaying an icon, take ourselves off the list
  if (mDisplayingIcon)
    gIconLoad->RemoveIconObserver(this);

  nsAtomicContainerFrame::DestroyFrom(aDestructRoot);
}

void
nsImageFrame::DidSetStyleContext(nsStyleContext* aOldStyleContext)
{
  nsAtomicContainerFrame::DidSetStyleContext(aOldStyleContext);

  if (!mImage) {
    // We'll pick this change up whenever we do get an image.
    return;
  }

  nsStyleImageOrientation newOrientation = StyleVisibility()->mImageOrientation;

  // We need to update our orientation either if we had no style context before
  // because this is the first time it's been set, or if the image-orientation
  // property changed from its previous value.
  bool shouldUpdateOrientation =
    !aOldStyleContext ||
    aOldStyleContext->StyleVisibility()->mImageOrientation != newOrientation;

  if (shouldUpdateOrientation) {
    nsCOMPtr<imgIContainer> image(mImage->Unwrap());
    mImage = nsLayoutUtils::OrientImage(image, newOrientation);

    UpdateIntrinsicSize(mImage);
    UpdateIntrinsicRatio(mImage);
  }
}

void
nsImageFrame::Init(nsIContent*       aContent,
                   nsContainerFrame* aParent,
                   nsIFrame*         aPrevInFlow)
{
  nsAtomicContainerFrame::Init(aContent, aParent, aPrevInFlow);

  mListener = new nsImageListener(this);

  nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(aContent);
  if (!imageLoader) {
    MOZ_CRASH("Why do we have an nsImageFrame here at all?");
  }

  imageLoader->AddObserver(mListener);

  nsPresContext *aPresContext = PresContext();
  
  if (!gIconLoad)
    LoadIcons(aPresContext);

  // We have a PresContext now, so we need to notify the image content node
  // that it can register images.
  imageLoader->FrameCreated(this);

  // Give image loads associated with an image frame a small priority boost!
  nsCOMPtr<imgIRequest> currentRequest;
  imageLoader->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                          getter_AddRefs(currentRequest));

  if (currentRequest) {
    uint32_t categoryToBoostPriority = imgIRequest::CATEGORY_FRAME_INIT;

    // Increase load priority further if intrinsic size might be important for layout.
    if (!HaveSpecifiedSize(StylePosition())) {
      categoryToBoostPriority |= imgIRequest::CATEGORY_SIZE_QUERY;
    }

    currentRequest->BoostPriority(categoryToBoostPriority);
  }
}

bool
nsImageFrame::UpdateIntrinsicSize(imgIContainer* aImage)
{
  NS_PRECONDITION(aImage, "null image");
  if (!aImage)
    return false;

  IntrinsicSize oldIntrinsicSize = mIntrinsicSize;
  mIntrinsicSize = IntrinsicSize();

  // Set intrinsic size to match aImage's reported intrinsic width & height.
  nsSize intrinsicSize;
  if (NS_SUCCEEDED(aImage->GetIntrinsicSize(&intrinsicSize))) {
    // If the image has no intrinsic width, intrinsicSize.width will be -1, and
    // we can leave mIntrinsicSize.width at its default value of eStyleUnit_None.
    // Otherwise we use intrinsicSize.width. Height works the same way.
    if (intrinsicSize.width != -1)
      mIntrinsicSize.width.SetCoordValue(intrinsicSize.width);
    if (intrinsicSize.height != -1)
      mIntrinsicSize.height.SetCoordValue(intrinsicSize.height);
  } else {
    // Failure means that the image hasn't loaded enough to report a result. We
    // treat this case as if the image's intrinsic size was 0x0.
    mIntrinsicSize.width.SetCoordValue(0);
    mIntrinsicSize.height.SetCoordValue(0);
  }

  return mIntrinsicSize != oldIntrinsicSize;
}

bool
nsImageFrame::UpdateIntrinsicRatio(imgIContainer* aImage)
{
  NS_PRECONDITION(aImage, "null image");

  if (!aImage)
    return false;

  nsSize oldIntrinsicRatio = mIntrinsicRatio;

  // Set intrinsic ratio to match aImage's reported intrinsic ratio.
  if (NS_FAILED(aImage->GetIntrinsicRatio(&mIntrinsicRatio)))
    mIntrinsicRatio.SizeTo(0, 0);

  return mIntrinsicRatio != oldIntrinsicRatio;
}

bool
nsImageFrame::GetSourceToDestTransform(nsTransform2D& aTransform)
{
  // First, figure out destRect (the rect we're rendering into).
  // NOTE: We use mComputedSize instead of just GetInnerArea()'s own size here,
  // because GetInnerArea() might be smaller if we're fragmented, whereas
  // mComputedSize has our full content-box size (which we need for
  // ComputeObjectDestRect to work correctly).
  nsRect constraintRect(GetInnerArea().TopLeft(), mComputedSize);
  constraintRect.y -= GetContinuationOffset();

  nsRect destRect = nsLayoutUtils::ComputeObjectDestRect(constraintRect,
                                                         mIntrinsicSize,
                                                         mIntrinsicRatio,
                                                         StylePosition());
  // Set the translation components, based on destRect
  // XXXbz does this introduce rounding errors because of the cast to
  // float?  Should we just manually add that stuff in every time
  // instead?
  aTransform.SetToTranslate(float(destRect.x),
                            float(destRect.y));

  // Set the scale factors, based on destRect and intrinsic size.
  if (mIntrinsicSize.width.GetUnit() == eStyleUnit_Coord &&
      mIntrinsicSize.width.GetCoordValue() != 0 &&
      mIntrinsicSize.height.GetUnit() == eStyleUnit_Coord &&
      mIntrinsicSize.height.GetCoordValue() != 0 &&
      mIntrinsicSize.width.GetCoordValue() != destRect.width &&
      mIntrinsicSize.height.GetCoordValue() != destRect.height) {

    aTransform.SetScale(float(destRect.width)  /
                        float(mIntrinsicSize.width.GetCoordValue()),
                        float(destRect.height) /
                        float(mIntrinsicSize.height.GetCoordValue()));
    return true;
  }

  return false;
}

// This function checks whether the given request is the current request for our
// mContent.
bool
nsImageFrame::IsPendingLoad(imgIRequest* aRequest) const
{
  // Default to pending load in case of errors
  nsCOMPtr<nsIImageLoadingContent> imageLoader(do_QueryInterface(mContent));
  NS_ASSERTION(imageLoader, "No image loading content?");

  int32_t requestType = nsIImageLoadingContent::UNKNOWN_REQUEST;
  imageLoader->GetRequestType(aRequest, &requestType);

  return requestType != nsIImageLoadingContent::CURRENT_REQUEST;
}

nsRect
nsImageFrame::SourceRectToDest(const nsIntRect& aRect)
{
  // When scaling the image, row N of the source image may (depending on
  // the scaling function) be used to draw any row in the destination image
  // between floor(F * (N-1)) and ceil(F * (N+1)), where F is the
  // floating-point scaling factor.  The same holds true for columns.
  // So, we start by computing that bound without the floor and ceiling.

  nsRect r(nsPresContext::CSSPixelsToAppUnits(aRect.x - 1),
           nsPresContext::CSSPixelsToAppUnits(aRect.y - 1),
           nsPresContext::CSSPixelsToAppUnits(aRect.width + 2),
           nsPresContext::CSSPixelsToAppUnits(aRect.height + 2));

  nsTransform2D sourceToDest;
  if (!GetSourceToDestTransform(sourceToDest)) {
    // Failed to generate transform matrix. Return our whole inner area,
    // to be on the safe side (since this method is used for generating
    // invalidation rects).
    return GetInnerArea();
  }

  sourceToDest.TransformCoord(&r.x, &r.y, &r.width, &r.height);

  // Now, round the edges out to the pixel boundary.
  nscoord scale = nsPresContext::CSSPixelsToAppUnits(1);
  nscoord right = r.x + r.width;
  nscoord bottom = r.y + r.height;

  r.x -= (scale + (r.x % scale)) % scale;
  r.y -= (scale + (r.y % scale)) % scale;
  r.width = right + ((scale - (right % scale)) % scale) - r.x;
  r.height = bottom + ((scale - (bottom % scale)) % scale) - r.y;

  return r;
}

// Note that we treat NS_EVENT_STATE_SUPPRESSED images as "OK".  This means
// that we'll construct image frames for them as needed if their display is
// toggled from "none" (though we won't paint them, unless their visibility
// is changed too).
#define BAD_STATES (NS_EVENT_STATE_BROKEN | NS_EVENT_STATE_USERDISABLED | \
                    NS_EVENT_STATE_LOADING)

// This is a macro so that we don't evaluate the boolean last arg
// unless we have to; it can be expensive
#define IMAGE_OK(_state, _loadingOK)                                           \
   (!(_state).HasAtLeastOneOfStates(BAD_STATES) ||                                    \
    (!(_state).HasAtLeastOneOfStates(NS_EVENT_STATE_BROKEN | NS_EVENT_STATE_USERDISABLED) && \
     (_state).HasState(NS_EVENT_STATE_LOADING) && (_loadingOK)))

/* static */
bool
nsImageFrame::ShouldCreateImageFrameFor(Element* aElement,
                                        nsStyleContext* aStyleContext)
{
  EventStates state = aElement->State();
  if (IMAGE_OK(state,
               HaveSpecifiedSize(aStyleContext->StylePosition()))) {
    // Image is fine; do the image frame thing
    return true;
  }

  // Check if we want to use a placeholder box with an icon or just
  // let the presShell make us into inline text.  Decide as follows:
  //
  //  - if our special "force icons" style is set, show an icon
  //  - else if our "do not show placeholders" pref is set, skip the icon
  //  - else:
  //  - if there is a src attribute, there is no alt attribute,
  //    and this is not an <object> (which could not possibly have
  //    such an attribute), show an icon.
  //  - if QuirksMode, and the IMG has a size show an icon.
  //  - otherwise, skip the icon
  bool useSizedBox;
  
  if (aStyleContext->StyleUIReset()->mForceBrokenImageIcon) {
    useSizedBox = true;
  }
  else if (gIconLoad && gIconLoad->mPrefForceInlineAltText) {
    useSizedBox = false;
  }
  else if (aElement->HasAttr(kNameSpaceID_None, nsGkAtoms::src) &&
           !aElement->HasAttr(kNameSpaceID_None, nsGkAtoms::alt) &&
           !aElement->IsHTMLElement(nsGkAtoms::object) &&
           !aElement->IsHTMLElement(nsGkAtoms::input)) {
    // Use a sized box if we have no alt text.  This means no alt attribute
    // and the node is not an object or an input (since those always have alt
    // text).
    useSizedBox = true;
  }
  else if (aStyleContext->PresContext()->CompatibilityMode() !=
           eCompatibility_NavQuirks) {
    useSizedBox = false;
  }
  else {
    // check whether we have specified size
    useSizedBox = HaveSpecifiedSize(aStyleContext->StylePosition());
  }

  return useSizedBox;
}

nsresult
nsImageFrame::Notify(imgIRequest* aRequest,
                     int32_t aType,
                     const nsIntRect* aRect)
{
  if (aType == imgINotificationObserver::SIZE_AVAILABLE) {
    nsCOMPtr<imgIContainer> image;
    aRequest->GetImage(getter_AddRefs(image));
    return OnSizeAvailable(aRequest, image);
  }

  if (aType == imgINotificationObserver::FRAME_UPDATE) {
    return OnFrameUpdate(aRequest, aRect);
  }

  if (aType == imgINotificationObserver::FRAME_COMPLETE) {
    mFirstFrameComplete = true;
  }

  if (aType == imgINotificationObserver::LOAD_COMPLETE) {
    uint32_t imgStatus;
    aRequest->GetImageStatus(&imgStatus);
    nsresult status =
        imgStatus & imgIRequest::STATUS_ERROR ? NS_ERROR_FAILURE : NS_OK;
    return OnLoadComplete(aRequest, status);
  }

  return NS_OK;
}

static bool
SizeIsAvailable(imgIRequest* aRequest)
{
  if (!aRequest)
    return false;

  uint32_t imageStatus = 0;
  nsresult rv = aRequest->GetImageStatus(&imageStatus);

  return NS_SUCCEEDED(rv) && (imageStatus & imgIRequest::STATUS_SIZE_AVAILABLE); 
}

nsresult
nsImageFrame::OnSizeAvailable(imgIRequest* aRequest, imgIContainer* aImage)
{
  if (!aImage) return NS_ERROR_INVALID_ARG;

  /* Get requested animation policy from the pres context:
   *   normal = 0
   *   one frame = 1
   *   one loop = 2
   */
  nsPresContext *presContext = PresContext();
  aImage->SetAnimationMode(presContext->ImageAnimationMode());

  if (IsPendingLoad(aRequest)) {
    // We don't care
    return NS_OK;
  }

  bool intrinsicSizeChanged = false;
  if (SizeIsAvailable(aRequest)) {
    // This is valid and for the current request, so update our stored image
    // container, orienting according to our style.
    mImage = nsLayoutUtils::OrientImage(aImage, StyleVisibility()->mImageOrientation);

    intrinsicSizeChanged = UpdateIntrinsicSize(mImage);
    intrinsicSizeChanged = UpdateIntrinsicRatio(mImage) || intrinsicSizeChanged;
  } else {
    // We no longer have a valid image, so release our stored image container.
    mImage = mPrevImage = nullptr;

    // Have to size to 0,0 so that GetDesiredSize recalculates the size.
    mIntrinsicSize.width.SetCoordValue(0);
    mIntrinsicSize.height.SetCoordValue(0);
    mIntrinsicRatio.SizeTo(0, 0);
    intrinsicSizeChanged = true;
  }

  if (intrinsicSizeChanged && (mState & IMAGE_GOTINITIALREFLOW)) {
    // Now we need to reflow if we have an unconstrained size and have
    // already gotten the initial reflow
    if (!(mState & IMAGE_SIZECONSTRAINED)) { 
      nsIPresShell *presShell = presContext->GetPresShell();
      NS_ASSERTION(presShell, "No PresShell.");
      if (presShell) { 
        presShell->FrameNeedsReflow(this, nsIPresShell::eStyleChange,
                                    NS_FRAME_IS_DIRTY);
      }
    } else {
      // We've already gotten the initial reflow, and our size hasn't changed,
      // so we're ready to request a decode.
      MaybeDecodeForPredictedSize();
    }

    mPrevImage = nullptr;
  }

  return NS_OK;
}

nsresult
nsImageFrame::OnFrameUpdate(imgIRequest* aRequest, const nsIntRect* aRect)
{
  NS_ENSURE_ARG_POINTER(aRect);

  if (!(mState & IMAGE_GOTINITIALREFLOW)) {
    // Don't bother to do anything; we have a reflow coming up!
    return NS_OK;
  }
  
  if (mFirstFrameComplete && !StyleVisibility()->IsVisible()) {
    return NS_OK;
  }

  if (IsPendingLoad(aRequest)) {
    // We don't care
    return NS_OK;
  }

  nsIntRect layerInvalidRect = mImage
                             ? mImage->GetImageSpaceInvalidationRect(*aRect)
                             : *aRect;

  if (layerInvalidRect.IsEqualInterior(GetMaxSizedIntRect())) {
    // Invalidate our entire area.
    InvalidateSelf(nullptr, nullptr);
    return NS_OK;
  }

  nsRect frameInvalidRect = SourceRectToDest(layerInvalidRect);
  InvalidateSelf(&layerInvalidRect, &frameInvalidRect);
  return NS_OK;
}

void
nsImageFrame::InvalidateSelf(const nsIntRect* aLayerInvalidRect,
                             const nsRect* aFrameInvalidRect)
{
  InvalidateLayer(nsDisplayItem::TYPE_IMAGE,
                  aLayerInvalidRect,
                  aFrameInvalidRect);

  if (!mFirstFrameComplete) {
    InvalidateLayer(nsDisplayItem::TYPE_ALT_FEEDBACK,
                    aLayerInvalidRect,
                    aFrameInvalidRect);
  }
}

nsresult
nsImageFrame::OnLoadComplete(imgIRequest* aRequest, nsresult aStatus)
{
  // Check what request type we're dealing with
  nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(mContent);
  NS_ASSERTION(imageLoader, "Who's notifying us??");
  int32_t loadType = nsIImageLoadingContent::UNKNOWN_REQUEST;
  imageLoader->GetRequestType(aRequest, &loadType);
  if (loadType != nsIImageLoadingContent::CURRENT_REQUEST &&
      loadType != nsIImageLoadingContent::PENDING_REQUEST) {
    return NS_ERROR_FAILURE;
  }

  NotifyNewCurrentRequest(aRequest, aStatus);
  return NS_OK;
}

void
nsImageFrame::NotifyNewCurrentRequest(imgIRequest *aRequest,
                                      nsresult aStatus)
{
  nsCOMPtr<imgIContainer> image;
  aRequest->GetImage(getter_AddRefs(image));
  NS_ASSERTION(image || NS_FAILED(aStatus), "Successful load with no container?");

  // May have to switch sizes here!
  bool intrinsicSizeChanged = true;
  if (NS_SUCCEEDED(aStatus) && image && SizeIsAvailable(aRequest)) {
    // Update our stored image container, orienting according to our style.
    mImage = nsLayoutUtils::OrientImage(image, StyleVisibility()->mImageOrientation);

    intrinsicSizeChanged = UpdateIntrinsicSize(mImage);
    intrinsicSizeChanged = UpdateIntrinsicRatio(mImage) || intrinsicSizeChanged;
  } else {
    // We no longer have a valid image, so release our stored image container.
    mImage = mPrevImage = nullptr;

    // Have to size to 0,0 so that GetDesiredSize recalculates the size
    mIntrinsicSize.width.SetCoordValue(0);
    mIntrinsicSize.height.SetCoordValue(0);
    mIntrinsicRatio.SizeTo(0, 0);
  }

  if (mState & IMAGE_GOTINITIALREFLOW) { // do nothing if we haven't gotten the initial reflow yet
    if (intrinsicSizeChanged) {
      if (!(mState & IMAGE_SIZECONSTRAINED)) {
        nsIPresShell *presShell = PresContext()->GetPresShell();
        if (presShell) {
          presShell->FrameNeedsReflow(this, nsIPresShell::eStyleChange,
                                      NS_FRAME_IS_DIRTY);
        }
      } else {
        // We've already gotten the initial reflow, and our size hasn't changed,
        // so we're ready to request a decode.
        MaybeDecodeForPredictedSize();
      }

      mPrevImage = nullptr;
    }
    // Update border+content to account for image change
    InvalidateFrame();
  }
}

void
nsImageFrame::MaybeDecodeForPredictedSize()
{
  // Check that we're ready to decode.
  if (!mImage) {
    return;  // Nothing to do yet.
  }

  if (mComputedSize.IsEmpty()) {
    return;  // We won't draw anything, so no point in decoding.
  }

  if (GetVisibility() != Visibility::APPROXIMATELY_VISIBLE) {
    return;  // We're not visible, so don't decode.
  }

  // OK, we're ready to decode. Compute the scale to the screen...
  nsIPresShell* presShell = PresContext()->GetPresShell();
  LayoutDeviceToScreenScale2D resolutionToScreen(
      presShell->GetCumulativeResolution()
    * nsLayoutUtils::GetTransformToAncestorScaleExcludingAnimated(this));

  // ...and this frame's content box...
  const nsPoint offset =
    GetOffsetToCrossDoc(nsLayoutUtils::GetReferenceFrame(this));
  const nsRect frameContentBox = GetInnerArea() + offset;

  // ...and our predicted dest rect...
  const int32_t factor = PresContext()->AppUnitsPerDevPixel();
  const LayoutDeviceRect destRect =
    LayoutDeviceRect::FromAppUnits(PredictedDestRect(frameContentBox), factor);

  // ...and use them to compute our predicted size in screen pixels.
  const ScreenSize predictedScreenSize = destRect.Size() * resolutionToScreen;
  const ScreenIntSize predictedScreenIntSize = RoundedToInt(predictedScreenSize);
  if (predictedScreenIntSize.IsEmpty()) {
    return;
  }

  // Determine the optimal image size to use.
  uint32_t flags = imgIContainer::FLAG_HIGH_QUALITY_SCALING
                 | imgIContainer::FLAG_ASYNC_NOTIFY;
  SamplingFilter samplingFilter =
    nsLayoutUtils::GetSamplingFilterForFrame(this);
  gfxSize gfxPredictedScreenSize = gfxSize(predictedScreenIntSize.width,
                                           predictedScreenIntSize.height);
  nsIntSize predictedImageSize =
    mImage->OptimalImageSizeForDest(gfxPredictedScreenSize,
                                    imgIContainer::FRAME_CURRENT,
                                    samplingFilter, flags);

  // Request a decode.
  mImage->RequestDecodeForSize(predictedImageSize, flags);
}

nsRect
nsImageFrame::PredictedDestRect(const nsRect& aFrameContentBox)
{
  // Note: To get the "dest rect", we have to provide the "constraint rect"
  // (which is the content-box, with the effects of fragmentation undone).
  nsRect constraintRect(aFrameContentBox.TopLeft(), mComputedSize);
  constraintRect.y -= GetContinuationOffset();

  return nsLayoutUtils::ComputeObjectDestRect(constraintRect,
                                              mIntrinsicSize,
                                              mIntrinsicRatio,
                                              StylePosition());
}

void
nsImageFrame::EnsureIntrinsicSizeAndRatio()
{
  // If mIntrinsicSize.width and height are 0, then we need to update from the
  // image container.
  if (mIntrinsicSize.width.GetUnit() == eStyleUnit_Coord &&
      mIntrinsicSize.width.GetCoordValue() == 0 &&
      mIntrinsicSize.height.GetUnit() == eStyleUnit_Coord &&
      mIntrinsicSize.height.GetCoordValue() == 0) {

    if (mImage) {
      UpdateIntrinsicSize(mImage);
      UpdateIntrinsicRatio(mImage);
    } else {
      // image request is null or image size not known, probably an
      // invalid image specified
      if (!(GetStateBits() & NS_FRAME_GENERATED_CONTENT)) {
        bool imageBroken = false;
        // check for broken images. valid null images (eg. img src="") are
        // not considered broken because they have no image requests
        nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(mContent);
        if (imageLoader) {
          nsCOMPtr<imgIRequest> currentRequest;
          imageLoader->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                                  getter_AddRefs(currentRequest));
          uint32_t imageStatus;
          imageBroken =
            currentRequest &&
            NS_SUCCEEDED(currentRequest->GetImageStatus(&imageStatus)) &&
            (imageStatus & imgIRequest::STATUS_ERROR);
        }
        // invalid image specified. make the image big enough for the "broken" icon
        if (imageBroken) {
          nscoord edgeLengthToUse =
            nsPresContext::CSSPixelsToAppUnits(
              ICON_SIZE + (2 * (ICON_PADDING + ALT_BORDER_WIDTH)));
          mIntrinsicSize.width.SetCoordValue(edgeLengthToUse);
          mIntrinsicSize.height.SetCoordValue(edgeLengthToUse);
          mIntrinsicRatio.SizeTo(1, 1);
        }
      }
    }
  }
}

/* virtual */
LogicalSize
nsImageFrame::ComputeSize(nsRenderingContext *aRenderingContext,
                          WritingMode aWM,
                          const LogicalSize& aCBSize,
                          nscoord aAvailableISize,
                          const LogicalSize& aMargin,
                          const LogicalSize& aBorder,
                          const LogicalSize& aPadding,
                          ComputeSizeFlags aFlags)
{
  EnsureIntrinsicSizeAndRatio();

  nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(mContent);
  NS_ASSERTION(imageLoader, "No content node??");
  mozilla::IntrinsicSize intrinsicSize(mIntrinsicSize);

  // XXX(seth): We may sometimes find ourselves in the situation where we have
  // mImage, but imageLoader's current request does not have a size yet.
  // This can happen when we load an image speculatively from cache, it fails
  // to validate, and the new image load hasn't fired SIZE_AVAILABLE yet. In
  // this situation we should always use mIntrinsicSize, because
  // GetNaturalWidth/Height will return 0, so we check CurrentRequestHasSize()
  // below. See bug 1019840. We will fix this in bug 1141395.

  // Content may override our default dimensions. This is termed as overriding
  // the intrinsic size by the spec, but all other consumers of mIntrinsic*
  // values are being used to refer to the real/true size of the image data.
  if (imageLoader && imageLoader->CurrentRequestHasSize() && mImage &&
      intrinsicSize.width.GetUnit() == eStyleUnit_Coord &&
      intrinsicSize.height.GetUnit() == eStyleUnit_Coord) {
    uint32_t width;
    uint32_t height;
    if (NS_SUCCEEDED(imageLoader->GetNaturalWidth(&width)) &&
        NS_SUCCEEDED(imageLoader->GetNaturalHeight(&height))) {
      nscoord appWidth = nsPresContext::CSSPixelsToAppUnits((int32_t)width);
      nscoord appHeight = nsPresContext::CSSPixelsToAppUnits((int32_t)height);
      // If this image is rotated, we'll need to transpose the natural
      // width/height.
      bool coordFlip;
      if (StyleVisibility()->mImageOrientation.IsFromImage()) {
        coordFlip = mImage->GetOrientation().SwapsWidthAndHeight();
      } else {
        coordFlip = StyleVisibility()->mImageOrientation.SwapsWidthAndHeight();
      }
      intrinsicSize.width.SetCoordValue(coordFlip ? appHeight : appWidth);
      intrinsicSize.height.SetCoordValue(coordFlip ? appWidth : appHeight);
    }
  }

  return ComputeSizeWithIntrinsicDimensions(aRenderingContext, aWM,
                                            intrinsicSize, mIntrinsicRatio,
                                            aCBSize, aMargin, aBorder, aPadding,
                                            aFlags);
}

// XXXdholbert This function's clients should probably just be calling
// GetContentRectRelativeToSelf() directly.
nsRect 
nsImageFrame::GetInnerArea() const
{
  return GetContentRectRelativeToSelf();
}

Element*
nsImageFrame::GetMapElement() const
{
  nsAutoString usemap;
  if (mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::usemap, usemap)) {
    return mContent->OwnerDoc()->FindImageMap(usemap);
  }
  return nullptr;
}

// get the offset into the content area of the image where aImg starts if it is a continuation.
nscoord 
nsImageFrame::GetContinuationOffset() const
{
  nscoord offset = 0;
  for (nsIFrame *f = GetPrevInFlow(); f; f = f->GetPrevInFlow()) {
    offset += f->GetContentRect().height;
  }
  NS_ASSERTION(offset >= 0, "bogus GetContentRect");
  return offset;
}

/* virtual */ nscoord
nsImageFrame::GetMinISize(nsRenderingContext *aRenderingContext)
{
  // XXX The caller doesn't account for constraints of the height,
  // min-height, and max-height properties.
  DebugOnly<nscoord> result;
  DISPLAY_MIN_WIDTH(this, result);
  EnsureIntrinsicSizeAndRatio();
  return mIntrinsicSize.width.GetUnit() == eStyleUnit_Coord ?
    mIntrinsicSize.width.GetCoordValue() : 0;
}

/* virtual */ nscoord
nsImageFrame::GetPrefISize(nsRenderingContext *aRenderingContext)
{
  // XXX The caller doesn't account for constraints of the height,
  // min-height, and max-height properties.
  DebugOnly<nscoord> result;
  DISPLAY_PREF_WIDTH(this, result);
  EnsureIntrinsicSizeAndRatio();
  // convert from normal twips to scaled twips (printing...)
  return mIntrinsicSize.width.GetUnit() == eStyleUnit_Coord ?
    mIntrinsicSize.width.GetCoordValue() : 0;
}

/* virtual */ IntrinsicSize
nsImageFrame::GetIntrinsicSize()
{
  return mIntrinsicSize;
}

/* virtual */ nsSize
nsImageFrame::GetIntrinsicRatio()
{
  return mIntrinsicRatio;
}

void
nsImageFrame::Reflow(nsPresContext*          aPresContext,
                     ReflowOutput&     aMetrics,
                     const ReflowInput& aReflowInput,
                     nsReflowStatus&          aStatus)
{
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsImageFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aMetrics, aStatus);
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                  ("enter nsImageFrame::Reflow: availSize=%d,%d",
                  aReflowInput.AvailableWidth(), aReflowInput.AvailableHeight()));

  NS_PRECONDITION(mState & NS_FRAME_IN_REFLOW, "frame is not in reflow");

  aStatus.Reset();

  // see if we have a frozen size (i.e. a fixed width and height)
  if (HaveFixedSize(aReflowInput)) {
    mState |= IMAGE_SIZECONSTRAINED;
  } else {
    mState &= ~IMAGE_SIZECONSTRAINED;
  }

  // XXXldb These two bits are almost exact opposites (except in the
  // middle of the initial reflow); remove IMAGE_GOTINITIALREFLOW.
  if (GetStateBits() & NS_FRAME_FIRST_REFLOW) {
    mState |= IMAGE_GOTINITIALREFLOW;
  }

  mComputedSize = 
    nsSize(aReflowInput.ComputedWidth(), aReflowInput.ComputedHeight());

  aMetrics.Width() = mComputedSize.width;
  aMetrics.Height() = mComputedSize.height;

  // add borders and padding
  aMetrics.Width()  += aReflowInput.ComputedPhysicalBorderPadding().LeftRight();
  aMetrics.Height() += aReflowInput.ComputedPhysicalBorderPadding().TopBottom();
  
  if (GetPrevInFlow()) {
    aMetrics.Width() = GetPrevInFlow()->GetSize().width;
    nscoord y = GetContinuationOffset();
    aMetrics.Height() -= y + aReflowInput.ComputedPhysicalBorderPadding().top;
    aMetrics.Height() = std::max(0, aMetrics.Height());
  }


  // we have to split images if we are:
  //  in Paginated mode, we need to have a constrained height, and have a height larger than our available height
  uint32_t loadStatus = imgIRequest::STATUS_NONE;
  nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(mContent);
  NS_ASSERTION(imageLoader, "No content node??");
  if (imageLoader) {
    nsCOMPtr<imgIRequest> currentRequest;
    imageLoader->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                            getter_AddRefs(currentRequest));
    if (currentRequest) {
      currentRequest->GetImageStatus(&loadStatus);
    }
  }
  if (aPresContext->IsPaginated() &&
      ((loadStatus & imgIRequest::STATUS_SIZE_AVAILABLE) || (mState & IMAGE_SIZECONSTRAINED)) &&
      NS_UNCONSTRAINEDSIZE != aReflowInput.AvailableHeight() && 
      aMetrics.Height() > aReflowInput.AvailableHeight()) { 
    // our desired height was greater than 0, so to avoid infinite
    // splitting, use 1 pixel as the min
    aMetrics.Height() = std::max(nsPresContext::CSSPixelsToAppUnits(1), aReflowInput.AvailableHeight());
    aStatus.Reset();
    aStatus.SetIncomplete();
  }

  aMetrics.SetOverflowAreasToDesiredBounds();
  EventStates contentState = mContent->AsElement()->State();
  bool imageOK = IMAGE_OK(contentState, true);

  // Determine if the size is available
  bool haveSize = false;
  if (loadStatus & imgIRequest::STATUS_SIZE_AVAILABLE) {
    haveSize = true;
  }

  if (!imageOK || !haveSize) {
    nsRect altFeedbackSize(0, 0,
                           nsPresContext::CSSPixelsToAppUnits(ICON_SIZE+2*(ICON_PADDING+ALT_BORDER_WIDTH)),
                           nsPresContext::CSSPixelsToAppUnits(ICON_SIZE+2*(ICON_PADDING+ALT_BORDER_WIDTH)));
    // We include the altFeedbackSize in our visual overflow, but not in our
    // scrollable overflow, since it doesn't really need to be scrolled to
    // outside the image.
    static_assert(eOverflowType_LENGTH == 2, "Unknown overflow types?");
    nsRect& visualOverflow = aMetrics.VisualOverflow();
    visualOverflow.UnionRect(visualOverflow, altFeedbackSize);
  } else {
    // We've just reflowed and we should have an accurate size, so we're ready
    // to request a decode.
    MaybeDecodeForPredictedSize();
  }
  FinishAndStoreOverflow(&aMetrics);

  if ((GetStateBits() & NS_FRAME_FIRST_REFLOW) && !mReflowCallbackPosted) {
    nsIPresShell* shell = PresContext()->PresShell();
    mReflowCallbackPosted = true;
    shell->PostReflowCallback(this);
  }

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                  ("exit nsImageFrame::Reflow: size=%d,%d",
                  aMetrics.Width(), aMetrics.Height()));
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aMetrics);
}

bool
nsImageFrame::ReflowFinished()
{
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

void
nsImageFrame::ReflowCallbackCanceled()
{
  mReflowCallbackPosted = false;
}

// Computes the width of the specified string. aMaxWidth specifies the maximum
// width available. Once this limit is reached no more characters are measured.
// The number of characters that fit within the maximum width are returned in
// aMaxFit. NOTE: it is assumed that the fontmetrics have already been selected
// into the rendering context before this is called (for performance). MMP
nscoord
nsImageFrame::MeasureString(const char16_t*     aString,
                            int32_t              aLength,
                            nscoord              aMaxWidth,
                            uint32_t&            aMaxFit,
                            nsRenderingContext& aContext,
                            nsFontMetrics& aFontMetrics)
{
  nscoord totalWidth = 0;
  aFontMetrics.SetTextRunRTL(false);
  nscoord spaceWidth = aFontMetrics.SpaceWidth();

  aMaxFit = 0;
  while (aLength > 0) {
    // Find the next place we can line break
    uint32_t  len = aLength;
    bool      trailingSpace = false;
    for (int32_t i = 0; i < aLength; i++) {
      if (dom::IsSpaceCharacter(aString[i]) && (i > 0)) {
        len = i;  // don't include the space when measuring
        trailingSpace = true;
        break;
      }
    }
  
    // Measure this chunk of text, and see if it fits
    nscoord width =
      nsLayoutUtils::AppUnitWidthOfStringBidi(aString, len, this, aFontMetrics,
                                              aContext);
    bool    fits = (totalWidth + width) <= aMaxWidth;

    // If it fits on the line, or it's the first word we've processed then
    // include it
    if (fits || (0 == totalWidth)) {
      // New piece fits
      totalWidth += width;

      // If there's a trailing space then see if it fits as well
      if (trailingSpace) {
        if ((totalWidth + spaceWidth) <= aMaxWidth) {
          totalWidth += spaceWidth;
        } else {
          // Space won't fit. Leave it at the end but don't include it in
          // the width
          fits = false;
        }

        len++;
      }

      aMaxFit += len;
      aString += len;
      aLength -= len;
    }

    if (!fits) {
      break;
    }
  }
  return totalWidth;
}

// Formats the alt-text to fit within the specified rectangle. Breaks lines
// between words if a word would extend past the edge of the rectangle
void
nsImageFrame::DisplayAltText(nsPresContext*      aPresContext,
                             nsRenderingContext& aRenderingContext,
                             const nsString&      aAltText,
                             const nsRect&        aRect)
{
  // Set font and color
  aRenderingContext.ThebesContext()->
    SetColor(Color::FromABGR(StyleColor()->mColor));
  RefPtr<nsFontMetrics> fm =
    nsLayoutUtils::GetInflatedFontMetricsForFrame(this);

  // Format the text to display within the formatting rect

  nscoord maxAscent = fm->MaxAscent();
  nscoord maxDescent = fm->MaxDescent();
  nscoord lineHeight = fm->MaxHeight(); // line-relative, so an x-coordinate
                                        // length if writing mode is vertical

  WritingMode wm = GetWritingMode();
  bool isVertical = wm.IsVertical();

  fm->SetVertical(isVertical);
  fm->SetTextOrientation(StyleVisibility()->mTextOrientation);

  // XXX It would be nice if there was a way to have the font metrics tell
  // use where to break the text given a maximum width. At a minimum we need
  // to be able to get the break character...
  const char16_t* str = aAltText.get();
  int32_t strLen = aAltText.Length();
  nsPoint pt = wm.IsVerticalRL() ? aRect.TopRight() - nsPoint(lineHeight, 0)
                                 : aRect.TopLeft();
  nscoord iSize = isVertical ? aRect.height : aRect.width;

  if (!aPresContext->BidiEnabled() && HasRTLChars(aAltText)) {
    aPresContext->SetBidiEnabled();
  }

  // Always show the first line, even if we have to clip it below
  bool firstLine = true;
  while (strLen > 0) {
    if (!firstLine) {
      // If we've run out of space, break out of the loop
      if ((!isVertical && (pt.y + maxDescent) >= aRect.YMost()) ||
          (wm.IsVerticalRL() && (pt.x + maxDescent < aRect.x)) ||
          (wm.IsVerticalLR() && (pt.x + maxDescent >= aRect.XMost()))) {
        break;
      }
    }

    // Determine how much of the text to display on this line
    uint32_t  maxFit;  // number of characters that fit
    nscoord strWidth = MeasureString(str, strLen, iSize, maxFit,
                                     aRenderingContext, *fm);

    // Display the text
    nsresult rv = NS_ERROR_FAILURE;

    if (aPresContext->BidiEnabled()) {
      nsBidiDirection dir;
      nscoord x, y;

      if (isVertical) {
        x = pt.x + maxDescent;
        if (wm.IsBidiLTR()) {
          y = aRect.y;
          dir = NSBIDI_LTR;
        } else {
          y = aRect.YMost() - strWidth;
          dir = NSBIDI_RTL;
        }
      } else {
        y = pt.y + maxAscent;
        if (wm.IsBidiLTR()) {
          x = aRect.x;
          dir = NSBIDI_LTR;
        } else {
          x = aRect.XMost() - strWidth;
          dir = NSBIDI_RTL;
        }
      }

      rv = nsBidiPresUtils::RenderText(str, maxFit, dir,
                                       aPresContext, aRenderingContext,
                                       aRenderingContext.GetDrawTarget(),
                                       *fm, x, y);
    }
    if (NS_FAILED(rv)) {
      nsLayoutUtils::DrawUniDirString(str, maxFit,
                                      isVertical
                                        ? nsPoint(pt.x + maxDescent, pt.y)
                                        : nsPoint(pt.x, pt.y + maxAscent),
                                      *fm, aRenderingContext);
    }

    // Move to the next line
    str += maxFit;
    strLen -= maxFit;
    if (wm.IsVerticalRL()) {
      pt.x -= lineHeight;
    } else if (wm.IsVerticalLR()) {
      pt.x += lineHeight;
    } else {
      pt.y += lineHeight;
    }

    firstLine = false;
  }
}

struct nsRecessedBorder : public nsStyleBorder {
  nsRecessedBorder(nscoord aBorderWidth, nsPresContext* aPresContext)
    : nsStyleBorder(aPresContext)
  {
    NS_FOR_CSS_SIDES(side) {
      mBorderColor[side] = StyleComplexColor::FromColor(NS_RGB(0, 0, 0));
      mBorder.Side(side) = aBorderWidth;
      // Note: use SetBorderStyle here because we want to affect
      // mComputedBorder
      SetBorderStyle(side, NS_STYLE_BORDER_STYLE_INSET);
    }
  }
};

class nsDisplayAltFeedback : public nsDisplayItem {
public:
  nsDisplayAltFeedback(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
    : nsDisplayItem(aBuilder, aFrame) {}

  virtual nsDisplayItemGeometry*
  AllocateGeometry(nsDisplayListBuilder* aBuilder) override
  {
    return new nsDisplayItemGenericImageGeometry(this, aBuilder);
  }

  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion) override
  {
    auto geometry =
      static_cast<const nsDisplayItemGenericImageGeometry*>(aGeometry);

    if (aBuilder->ShouldSyncDecodeImages() &&
        geometry->ShouldInvalidateToSyncDecodeImages()) {
      bool snap;
      aInvalidRegion->Or(*aInvalidRegion, GetBounds(aBuilder, &snap));
    }

    nsDisplayItem::ComputeInvalidationRegion(aBuilder, aGeometry, aInvalidRegion);
  }

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) override
  {
    *aSnap = false;
    return mFrame->GetVisualOverflowRectRelativeToSelf() + ToReferenceFrame();
  }

  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext* aCtx) override
  {
    // Always sync decode, because these icons are UI, and since they're not
    // discardable we'll pay the price of sync decoding at most once.
    uint32_t flags = imgIContainer::FLAG_SYNC_DECODE;

    nsImageFrame* f = static_cast<nsImageFrame*>(mFrame);
    DrawResult result =
      f->DisplayAltFeedback(*aCtx,
                            mVisibleRect,
                            ToReferenceFrame(),
                            flags);

    nsDisplayItemGenericImageGeometry::UpdateDrawResult(this, result);
  }

  NS_DISPLAY_DECL_NAME("AltFeedback", TYPE_ALT_FEEDBACK)
};

DrawResult
nsImageFrame::DisplayAltFeedback(nsRenderingContext& aRenderingContext,
                                 const nsRect& aDirtyRect,
                                 nsPoint aPt,
                                 uint32_t aFlags)
{
  // We should definitely have a gIconLoad here.
  MOZ_ASSERT(gIconLoad, "How did we succeed in Init then?");

  // Whether we draw the broken or loading icon.
  bool isLoading = IMAGE_OK(GetContent()->AsElement()->State(), true);

  // Calculate the inner area
  nsRect  inner = GetInnerArea() + aPt;

  // Display a recessed one pixel border
  nscoord borderEdgeWidth = nsPresContext::CSSPixelsToAppUnits(ALT_BORDER_WIDTH);

  // if inner area is empty, then make it big enough for at least the icon
  if (inner.IsEmpty()){
    inner.SizeTo(2*(nsPresContext::CSSPixelsToAppUnits(ICON_SIZE+ICON_PADDING+ALT_BORDER_WIDTH)),
                 2*(nsPresContext::CSSPixelsToAppUnits(ICON_SIZE+ICON_PADDING+ALT_BORDER_WIDTH)));
  }

  // Make sure we have enough room to actually render the border within
  // our frame bounds
  if ((inner.width < 2 * borderEdgeWidth) || (inner.height < 2 * borderEdgeWidth)) {
    return DrawResult::SUCCESS;
  }

  // Paint the border
  if (!isLoading || gIconLoad->mPrefShowLoadingPlaceholder) {
    nsRecessedBorder recessedBorder(borderEdgeWidth, PresContext());

    // Assert that we're not drawing a border-image here; if we were, we
    // couldn't ignore the DrawResult that PaintBorderWithStyleBorder returns.
    MOZ_ASSERT(recessedBorder.mBorderImageSource.GetType() == eStyleImageType_Null);

    Unused <<
      nsCSSRendering::PaintBorderWithStyleBorder(PresContext(), aRenderingContext,
                                                 this, inner, inner,
                                                 recessedBorder, mStyleContext,
                                                 PaintBorderFlags::SYNC_DECODE_IMAGES);
  }

  // Adjust the inner rect to account for the one pixel recessed border,
  // and a six pixel padding on each edge
  inner.Deflate(nsPresContext::CSSPixelsToAppUnits(ICON_PADDING+ALT_BORDER_WIDTH), 
                nsPresContext::CSSPixelsToAppUnits(ICON_PADDING+ALT_BORDER_WIDTH));
  if (inner.IsEmpty()) {
    return DrawResult::SUCCESS;
  }

  DrawTarget* drawTarget = aRenderingContext.GetDrawTarget();
  gfxContext* gfx = aRenderingContext.ThebesContext();

  // Clip so we don't render outside the inner rect
  gfx->Save();
  gfx->Clip(NSRectToSnappedRect(inner, PresContext()->AppUnitsPerDevPixel(),
                                *drawTarget));

  DrawResult result = DrawResult::NOT_READY;

  // Check if we should display image placeholders
  if (!gIconLoad->mPrefShowPlaceholders ||
      (isLoading && !gIconLoad->mPrefShowLoadingPlaceholder)) {
    result = DrawResult::SUCCESS;
  } else {
    nscoord size = nsPresContext::CSSPixelsToAppUnits(ICON_SIZE);

    imgIRequest* request = isLoading
                              ? nsImageFrame::gIconLoad->mLoadingImage
                              : nsImageFrame::gIconLoad->mBrokenImage;

    // If we weren't previously displaying an icon, register ourselves
    // as an observer for load and animation updates and flag that we're
    // doing so now.
    if (request && !mDisplayingIcon) {
      gIconLoad->AddIconObserver(this);
      mDisplayingIcon = true;
    }

    WritingMode wm = GetWritingMode();
    bool flushRight =
      (!wm.IsVertical() && !wm.IsBidiLTR()) || wm.IsVerticalRL();

    // If the icon in question is loaded, draw it.
    uint32_t imageStatus = 0;
    if (request)
      request->GetImageStatus(&imageStatus);
    if (imageStatus & imgIRequest::STATUS_LOAD_COMPLETE) {
      nsCOMPtr<imgIContainer> imgCon;
      request->GetImage(getter_AddRefs(imgCon));
      MOZ_ASSERT(imgCon, "Load complete, but no image container?");
      nsRect dest(flushRight ? inner.XMost() - size : inner.x,
                  inner.y, size, size);
      result = nsLayoutUtils::DrawSingleImage(*gfx, PresContext(), imgCon,
        nsLayoutUtils::GetSamplingFilterForFrame(this), dest, aDirtyRect,
        /* no SVGImageContext */ Nothing(), aFlags);
    }

    // If we could not draw the icon, just draw some graffiti in the mean time.
    if (result == DrawResult::NOT_READY) {
      ColorPattern color(ToDeviceColor(Color(1.f, 0.f, 0.f, 1.f)));

      nscoord iconXPos = flushRight ? inner.XMost() - size : inner.x;

      // stroked rect:
      nsRect rect(iconXPos, inner.y, size, size);
      Rect devPxRect =
        ToRect(nsLayoutUtils::RectToGfxRect(rect, PresContext()->AppUnitsPerDevPixel()));
      drawTarget->StrokeRect(devPxRect, color);

      // filled circle in bottom right quadrant of stroked rect:
      nscoord twoPX = nsPresContext::CSSPixelsToAppUnits(2);
      rect = nsRect(iconXPos + size/2, inner.y + size/2,
                    size/2 - twoPX, size/2 - twoPX);
      devPxRect =
        ToRect(nsLayoutUtils::RectToGfxRect(rect, PresContext()->AppUnitsPerDevPixel()));
      RefPtr<PathBuilder> builder = drawTarget->CreatePathBuilder();
      AppendEllipseToPath(builder, devPxRect.Center(), devPxRect.Size());
      RefPtr<Path> ellipse = builder->Finish();
      drawTarget->Fill(ellipse, color);
    }

    // Reduce the inner rect by the width of the icon, and leave an
    // additional ICON_PADDING pixels for padding
    int32_t paddedIconSize =
      nsPresContext::CSSPixelsToAppUnits(ICON_SIZE + ICON_PADDING);
    if (wm.IsVertical()) {
      inner.y += paddedIconSize;
      inner.height -= paddedIconSize;
    } else {
      if (!flushRight) {
        inner.x += paddedIconSize;
      }
      inner.width -= paddedIconSize;
    }
  }

  // If there's still room, display the alt-text
  if (!inner.IsEmpty()) {
    nsIContent* content = GetContent();
    if (content) {
      nsXPIDLString altText;
      nsCSSFrameConstructor::GetAlternateTextFor(content,
                                                 content->NodeInfo()->NameAtom(),
                                                 altText);
      DisplayAltText(PresContext(), aRenderingContext, altText, inner);
    }
  }

  aRenderingContext.ThebesContext()->Restore();

  return result;
}

#ifdef DEBUG
static void PaintDebugImageMap(nsIFrame* aFrame, DrawTarget* aDrawTarget,
                               const nsRect& aDirtyRect, nsPoint aPt)
{
  nsImageFrame* f = static_cast<nsImageFrame*>(aFrame);
  nsRect inner = f->GetInnerArea() + aPt;
  gfxPoint devPixelOffset =
    nsLayoutUtils::PointToGfxPoint(inner.TopLeft(),
                                   aFrame->PresContext()->AppUnitsPerDevPixel());
  AutoRestoreTransform autoRestoreTransform(aDrawTarget);
  aDrawTarget->SetTransform(
    aDrawTarget->GetTransform().PreTranslate(ToPoint(devPixelOffset)));
  f->GetImageMap()->Draw(aFrame, *aDrawTarget,
                         ColorPattern(ToDeviceColor(Color(0.f, 0.f, 0.f, 1.f))));
}
#endif

void
nsDisplayImage::Paint(nsDisplayListBuilder* aBuilder,
                      nsRenderingContext* aCtx)
{
  uint32_t flags = imgIContainer::FLAG_NONE;
  if (aBuilder->ShouldSyncDecodeImages()) {
    flags |= imgIContainer::FLAG_SYNC_DECODE;
  }
  if (aBuilder->IsPaintingToWindow()) {
    flags |= imgIContainer::FLAG_HIGH_QUALITY_SCALING;
  }

  DrawResult result = static_cast<nsImageFrame*>(mFrame)->
    PaintImage(*aCtx, ToReferenceFrame(), mVisibleRect, mImage, flags);

  if (result == DrawResult::NOT_READY ||
      result == DrawResult::INCOMPLETE ||
      result == DrawResult::TEMPORARY_ERROR) {
    // If the current image failed to paint because it's still loading or
    // decoding, try painting the previous image.
    if (mPrevImage) {
      result = static_cast<nsImageFrame*>(mFrame)->
        PaintImage(*aCtx, ToReferenceFrame(), mVisibleRect, mPrevImage, flags);
    }
  }

  nsDisplayItemGenericImageGeometry::UpdateDrawResult(this, result);
}

nsDisplayItemGeometry*
nsDisplayImage::AllocateGeometry(nsDisplayListBuilder* aBuilder)
{
  return new nsDisplayItemGenericImageGeometry(this, aBuilder);
}

void
nsDisplayImage::ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                          const nsDisplayItemGeometry* aGeometry,
                                          nsRegion* aInvalidRegion)
{
  auto geometry =
    static_cast<const nsDisplayItemGenericImageGeometry*>(aGeometry);

  if (aBuilder->ShouldSyncDecodeImages() &&
      geometry->ShouldInvalidateToSyncDecodeImages()) {
    bool snap;
    aInvalidRegion->Or(*aInvalidRegion, GetBounds(aBuilder, &snap));
  }

  nsDisplayImageContainer::ComputeInvalidationRegion(aBuilder, aGeometry, aInvalidRegion);
}

already_AddRefed<imgIContainer>
nsDisplayImage::GetImage()
{
  nsCOMPtr<imgIContainer> image = mImage;
  return image.forget();
}

nsRect
nsDisplayImage::GetDestRect()
{
  bool snap = true;
  const nsRect frameContentBox = GetBounds(&snap);

  nsImageFrame* imageFrame = static_cast<nsImageFrame*>(mFrame);
  return imageFrame->PredictedDestRect(frameContentBox);
}

LayerState
nsDisplayImage::GetLayerState(nsDisplayListBuilder* aBuilder,
                              LayerManager* aManager,
                              const ContainerLayerParameters& aParameters)
{
  if (!nsDisplayItem::ForceActiveLayers() &&
      !ShouldUseAdvancedLayer(aManager, gfxPrefs::LayersAllowImageLayers)) {
    bool animated = false;
    if (!nsLayoutUtils::AnimatedImageLayersEnabled() ||
        mImage->GetType() != imgIContainer::TYPE_RASTER ||
        NS_FAILED(mImage->GetAnimated(&animated)) ||
        !animated) {
      if (!aManager->IsCompositingCheap() ||
          !nsLayoutUtils::GPUImageScalingEnabled()) {
        return LAYER_NONE;
      }
    }

    if (!animated) {
      int32_t imageWidth;
      int32_t imageHeight;
      mImage->GetWidth(&imageWidth);
      mImage->GetHeight(&imageHeight);

      NS_ASSERTION(imageWidth != 0 && imageHeight != 0, "Invalid image size!");

      const int32_t factor = mFrame->PresContext()->AppUnitsPerDevPixel();
      const LayoutDeviceRect destRect =
        LayoutDeviceRect::FromAppUnits(GetDestRect(), factor);
      const LayerRect destLayerRect = destRect * aParameters.Scale();

      // Calculate the scaling factor for the frame.
      const gfxSize scale = gfxSize(destLayerRect.width / imageWidth,
                                    destLayerRect.height / imageHeight);

      // If we are not scaling at all, no point in separating this into a layer.
      if (scale.width == 1.0f && scale.height == 1.0f) {
        return LAYER_NONE;
      }

      // If the target size is pretty small, no point in using a layer.
      if (destLayerRect.width * destLayerRect.height < 64 * 64) {
        return LAYER_NONE;
      }
    }
  }

  uint32_t flags = aBuilder->ShouldSyncDecodeImages()
                 ? imgIContainer::FLAG_SYNC_DECODE
                 : imgIContainer::FLAG_NONE;

  if (!mImage->IsImageContainerAvailable(aManager, flags)) {
    return LAYER_NONE;
  }

  return LAYER_ACTIVE;
}


/* virtual */ nsRegion
nsDisplayImage::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                bool* aSnap)
{
  *aSnap = false;
  if (mImage && mImage->WillDrawOpaqueNow()) {
    const nsRect frameContentBox = GetBounds(aSnap);
    return GetDestRect().Intersect(frameContentBox);
  }
  return nsRegion();
}

already_AddRefed<Layer>
nsDisplayImage::BuildLayer(nsDisplayListBuilder* aBuilder,
                           LayerManager* aManager,
                           const ContainerLayerParameters& aParameters)
{
  uint32_t flags = imgIContainer::FLAG_ASYNC_NOTIFY;
  if (aBuilder->ShouldSyncDecodeImages()) {
    flags |= imgIContainer::FLAG_SYNC_DECODE;
  }

  RefPtr<ImageContainer> container =
    mImage->GetImageContainer(aManager, flags);
  if (!container) {
    return nullptr;
  }

  RefPtr<ImageLayer> layer = static_cast<ImageLayer*>
    (aManager->GetLayerBuilder()->GetLeafLayerFor(aBuilder, this));
  if (!layer) {
    layer = aManager->CreateImageLayer();
    if (!layer)
      return nullptr;
  }
  layer->SetContainer(container);
  ConfigureLayer(layer, aParameters);
  return layer.forget();
}

DrawResult
nsImageFrame::PaintImage(nsRenderingContext& aRenderingContext, nsPoint aPt,
                         const nsRect& aDirtyRect, imgIContainer* aImage,
                         uint32_t aFlags)
{
  DrawTarget* drawTarget = aRenderingContext.GetDrawTarget();

  // Render the image into our content area (the area inside
  // the borders and padding)
  NS_ASSERTION(GetInnerArea().width == mComputedSize.width, "bad width");

  // NOTE: We use mComputedSize instead of just GetInnerArea()'s own size here,
  // because GetInnerArea() might be smaller if we're fragmented, whereas
  // mComputedSize has our full content-box size (which we need for
  // ComputeObjectDestRect to work correctly).
  nsRect constraintRect(aPt + GetInnerArea().TopLeft(), mComputedSize);
  constraintRect.y -= GetContinuationOffset();

  nsPoint anchorPoint;
  nsRect dest = nsLayoutUtils::ComputeObjectDestRect(constraintRect,
                                                     mIntrinsicSize,
                                                     mIntrinsicRatio,
                                                     StylePosition(),
                                                     &anchorPoint);

  uint32_t flags = aFlags;
  if (mForceSyncDecoding) {
    flags |= imgIContainer::FLAG_SYNC_DECODE;
  }

  Maybe<SVGImageContext> svgContext;
  SVGImageContext::MaybeStoreContextPaint(svgContext, this, aImage);

  DrawResult result =
    nsLayoutUtils::DrawSingleImage(*aRenderingContext.ThebesContext(),
      PresContext(), aImage,
      nsLayoutUtils::GetSamplingFilterForFrame(this), dest, aDirtyRect,
      svgContext, flags, &anchorPoint);

  nsImageMap* map = GetImageMap();
  if (map) {
    gfxPoint devPixelOffset =
      nsLayoutUtils::PointToGfxPoint(dest.TopLeft(),
                                     PresContext()->AppUnitsPerDevPixel());
    AutoRestoreTransform autoRestoreTransform(drawTarget);
    drawTarget->SetTransform(
      drawTarget->GetTransform().PreTranslate(ToPoint(devPixelOffset)));

    // solid white stroke:
    ColorPattern white(ToDeviceColor(Color(1.f, 1.f, 1.f, 1.f)));
    map->Draw(this, *drawTarget, white);

    // then dashed black stroke over the top:
    ColorPattern black(ToDeviceColor(Color(0.f, 0.f, 0.f, 1.f)));
    StrokeOptions strokeOptions;
    nsLayoutUtils::InitDashPattern(strokeOptions, NS_STYLE_BORDER_STYLE_DOTTED);
    map->Draw(this, *drawTarget, black, strokeOptions);
  }

  if (result == DrawResult::SUCCESS) {
    mPrevImage = aImage;
  } else if (result == DrawResult::BAD_IMAGE) {
    mPrevImage = nullptr;
  }

  return result;
}

void
nsImageFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                               const nsRect&           aDirtyRect,
                               const nsDisplayListSet& aLists)
{
  if (!IsVisibleForPainting(aBuilder))
    return;

  DisplayBorderBackgroundOutline(aBuilder, aLists);

  uint32_t clipFlags =
    nsStyleUtil::ObjectPropsMightCauseOverflow(StylePosition()) ?
    0 : DisplayListClipState::ASSUME_DRAWING_RESTRICTED_TO_CONTENT_RECT;

  DisplayListClipState::AutoClipContainingBlockDescendantsToContentBox
    clip(aBuilder, this, clipFlags);

  if (mComputedSize.width != 0 && mComputedSize.height != 0) {
    nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(mContent);
    NS_ASSERTION(imageLoader, "Not an image loading content?");

    nsCOMPtr<imgIRequest> currentRequest;
    if (imageLoader) {
      imageLoader->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                              getter_AddRefs(currentRequest));
    }

    EventStates contentState = mContent->AsElement()->State();
    bool imageOK = IMAGE_OK(contentState, true);

    // XXX(seth): The SizeIsAvailable check here should not be necessary - the
    // intention is that a non-null mImage means we have a size, but there is
    // currently some code that violates this invariant.
    if (!imageOK || !mImage || !SizeIsAvailable(currentRequest)) {
      // No image yet, or image load failed. Draw the alt-text and an icon
      // indicating the status
      aLists.Content()->AppendNewToTop(new (aBuilder)
        nsDisplayAltFeedback(aBuilder, this));

      // This image is visible (we are being asked to paint it) but it's not
      // decoded yet. And we are not going to ask the image to draw, so this
      // may be the only chance to tell it that it should decode.
      if (currentRequest) {
        uint32_t status = 0;
        currentRequest->GetImageStatus(&status);
        if (!(status & imgIRequest::STATUS_DECODE_COMPLETE)) {
          MaybeDecodeForPredictedSize();
        }
      }
    } else {
      aLists.Content()->AppendNewToTop(new (aBuilder)
        nsDisplayImage(aBuilder, this, mImage, mPrevImage));

      // If we were previously displaying an icon, we're not anymore
      if (mDisplayingIcon) {
        gIconLoad->RemoveIconObserver(this);
        mDisplayingIcon = false;
      }

#ifdef DEBUG
      if (GetShowFrameBorders() && GetImageMap()) {
        aLists.Outlines()->AppendNewToTop(new (aBuilder)
          nsDisplayGeneric(aBuilder, this, PaintDebugImageMap, "DebugImageMap",
                           nsDisplayItem::TYPE_DEBUG_IMAGE_MAP));
      }
#endif
    }
  }

  if (ShouldDisplaySelection()) {
    DisplaySelectionOverlay(aBuilder, aLists.Content(),
                            nsISelectionDisplay::DISPLAY_IMAGES);
  }
}

bool
nsImageFrame::ShouldDisplaySelection()
{
  // XXX what on EARTH is this code for?
  nsresult result;
  nsPresContext* presContext = PresContext();
  int16_t displaySelection = presContext->PresShell()->GetSelectionFlags();
  if (!(displaySelection & nsISelectionDisplay::DISPLAY_IMAGES))
    return false;//no need to check the blue border, we cannot be drawn selected
//insert hook here for image selection drawing
#if IMAGE_EDITOR_CHECK
  //check to see if this frame is in an editor context
  //isEditor check. this needs to be changed to have better way to check
  if (displaySelection == nsISelectionDisplay::DISPLAY_ALL) 
  {
    nsCOMPtr<nsISelectionController> selCon;
    result = GetSelectionController(presContext, getter_AddRefs(selCon));
    if (NS_SUCCEEDED(result) && selCon)
    {
      nsCOMPtr<nsISelection> selection;
      result = selCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(selection));
      if (NS_SUCCEEDED(result) && selection)
      {
        int32_t rangeCount;
        selection->GetRangeCount(&rangeCount);
        if (rangeCount == 1) //if not one then let code drop to nsFrame::Paint
        {
          nsCOMPtr<nsIContent> parentContent = mContent->GetParent();
          if (parentContent)
          {
            int32_t thisOffset = parentContent->IndexOf(mContent);
            nsCOMPtr<nsIDOMNode> parentNode = do_QueryInterface(parentContent);
            nsCOMPtr<nsIDOMNode> rangeNode;
            int32_t rangeOffset;
            nsCOMPtr<nsIDOMRange> range;
            selection->GetRangeAt(0,getter_AddRefs(range));
            if (range)
            {
              range->GetStartContainer(getter_AddRefs(rangeNode));
              range->GetStartOffset(&rangeOffset);

              if (parentNode && rangeNode && (rangeNode == parentNode) && rangeOffset == thisOffset)
              {
                range->GetEndContainer(getter_AddRefs(rangeNode));
                range->GetEndOffset(&rangeOffset);
                if ((rangeNode == parentNode) && (rangeOffset == (thisOffset +1))) //+1 since that would mean this whole content is selected only
                  return false; //do not allow nsFrame do draw any further selection
              }
            }
          }
        }
      }
    }
  }
#endif
  return true;
}

nsImageMap*
nsImageFrame::GetImageMap()
{
  if (!mImageMap) {
    nsIContent* map = GetMapElement();
    if (map) {
      mImageMap = new nsImageMap();
      mImageMap->Init(this, map);
    }
  }

  return mImageMap;
}

bool
nsImageFrame::IsServerImageMap()
{
  return mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::ismap);
}

// Translate an point that is relative to our frame
// into a localized pixel coordinate that is relative to the
// content area of this frame (inside the border+padding).
void
nsImageFrame::TranslateEventCoords(const nsPoint& aPoint,
                                   nsIntPoint&     aResult)
{
  nscoord x = aPoint.x;
  nscoord y = aPoint.y;

  // Subtract out border and padding here so that the coordinates are
  // now relative to the content area of this frame.
  nsRect inner = GetInnerArea();
  x -= inner.x;
  y -= inner.y;

  aResult.x = nsPresContext::AppUnitsToIntCSSPixels(x);
  aResult.y = nsPresContext::AppUnitsToIntCSSPixels(y);
}

bool
nsImageFrame::GetAnchorHREFTargetAndNode(nsIURI** aHref, nsString& aTarget,
                                         nsIContent** aNode)
{
  bool status = false;
  aTarget.Truncate();
  *aHref = nullptr;
  *aNode = nullptr;

  // Walk up the content tree, looking for an nsIDOMAnchorElement
  for (nsIContent* content = mContent->GetParent();
       content; content = content->GetParent()) {
    nsCOMPtr<dom::Link> link(do_QueryInterface(content));
    if (link) {
      nsCOMPtr<nsIURI> href = content->GetHrefURI();
      if (href) {
        href->Clone(aHref);
      }
      status = (*aHref != nullptr);

      nsCOMPtr<nsIDOMHTMLAnchorElement> anchor(do_QueryInterface(content));
      if (anchor) {
        anchor->GetTarget(aTarget);
      }
      NS_ADDREF(*aNode = content);
      break;
    }
  }
  return status;
}

nsresult  
nsImageFrame::GetContentForEvent(WidgetEvent* aEvent,
                                 nsIContent** aContent)
{
  NS_ENSURE_ARG_POINTER(aContent);

  nsIFrame* f = nsLayoutUtils::GetNonGeneratedAncestor(this);
  if (f != this) {
    return f->GetContentForEvent(aEvent, aContent);
  }

  // XXX We need to make this special check for area element's capturing the
  // mouse due to bug 135040. Remove it once that's fixed.
  nsIContent* capturingContent =
    aEvent->HasMouseEventMessage() ? nsIPresShell::GetCapturingContent() :
                                     nullptr;
  if (capturingContent && capturingContent->GetPrimaryFrame() == this) {
    *aContent = capturingContent;
    NS_IF_ADDREF(*aContent);
    return NS_OK;
  }

  nsImageMap* map = GetImageMap();

  if (nullptr != map) {
    nsIntPoint p;
    TranslateEventCoords(
      nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent, this), p);
    nsCOMPtr<nsIContent> area = map->GetArea(p.x, p.y);
    if (area) {
      area.forget(aContent);
      return NS_OK;
    }
  }

  *aContent = GetContent();
  NS_IF_ADDREF(*aContent);
  return NS_OK;
}

// XXX what should clicks on transparent pixels do?
nsresult
nsImageFrame::HandleEvent(nsPresContext* aPresContext,
                          WidgetGUIEvent* aEvent,
                          nsEventStatus* aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);

  if ((aEvent->mMessage == eMouseClick &&
       aEvent->AsMouseEvent()->button == WidgetMouseEvent::eLeftButton) ||
      aEvent->mMessage == eMouseMove) {
    nsImageMap* map = GetImageMap();
    bool isServerMap = IsServerImageMap();
    if ((nullptr != map) || isServerMap) {
      nsIntPoint p;
      TranslateEventCoords(
        nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent, this), p);
      bool inside = false;
      // Even though client-side image map triggering happens
      // through content, we need to make sure we're not inside
      // (in case we deal with a case of both client-side and
      // sever-side on the same image - it happens!)
      if (nullptr != map) {
        inside = !!map->GetArea(p.x, p.y);
      }

      if (!inside && isServerMap) {

        // Server side image maps use the href in a containing anchor
        // element to provide the basis for the destination url.
        nsCOMPtr<nsIURI> uri;
        nsAutoString target;
        nsCOMPtr<nsIContent> anchorNode;
        if (GetAnchorHREFTargetAndNode(getter_AddRefs(uri), target,
                                       getter_AddRefs(anchorNode))) {
          // XXX if the mouse is over/clicked in the border/padding area
          // we should probably just pretend nothing happened. Nav4
          // keeps the x,y coordinates positive as we do; IE doesn't
          // bother. Both of them send the click through even when the
          // mouse is over the border.
          if (p.x < 0) p.x = 0;
          if (p.y < 0) p.y = 0;

          nsAutoCString spec;
          nsresult rv = uri->GetSpec(spec);
          NS_ENSURE_SUCCESS(rv, rv);

          spec += nsPrintfCString("?%d,%d", p.x, p.y);
          rv = uri->SetSpec(spec);
          NS_ENSURE_SUCCESS(rv, rv);

          bool clicked = false;
          if (aEvent->mMessage == eMouseClick && !aEvent->DefaultPrevented()) {
            *aEventStatus = nsEventStatus_eConsumeDoDefault;
            clicked = true;
          }
          nsContentUtils::TriggerLink(anchorNode, aPresContext, uri, target,
                                      clicked, true, true);
        }
      }
    }
  }

  return nsAtomicContainerFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
}

nsresult
nsImageFrame::GetCursor(const nsPoint& aPoint,
                        nsIFrame::Cursor& aCursor)
{
  nsImageMap* map = GetImageMap();
  if (nullptr != map) {
    nsIntPoint p;
    TranslateEventCoords(aPoint, p);
    nsCOMPtr<nsIContent> area = map->GetArea(p.x, p.y);
    if (area) {
      // Use the cursor from the style of the *area* element.
      // XXX Using the image as the parent style context isn't
      // technically correct, but it's probably the right thing to do
      // here, since it means that areas on which the cursor isn't
      // specified will inherit the style from the image.
      RefPtr<nsStyleContext> areaStyle = 
        PresContext()->PresShell()->StyleSet()->
          ResolveStyleFor(area->AsElement(), StyleContext(),
                          LazyComputeBehavior::Allow);
      FillCursorInformationFromStyle(areaStyle->StyleUserInterface(),
                                     aCursor);
      if (NS_STYLE_CURSOR_AUTO == aCursor.mCursor) {
        aCursor.mCursor = NS_STYLE_CURSOR_DEFAULT;
      }
      return NS_OK;
    }
  }
  return nsFrame::GetCursor(aPoint, aCursor);
}

nsresult
nsImageFrame::AttributeChanged(int32_t aNameSpaceID,
                               nsIAtom* aAttribute,
                               int32_t aModType)
{
  nsresult rv = nsAtomicContainerFrame::AttributeChanged(aNameSpaceID,
                                                         aAttribute, aModType);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (nsGkAtoms::alt == aAttribute)
  {
    PresContext()->PresShell()->FrameNeedsReflow(this,
                                                 nsIPresShell::eStyleChange,
                                                 NS_FRAME_IS_DIRTY);
  }

  return NS_OK;
}

void
nsImageFrame::OnVisibilityChange(Visibility aNewVisibility,
                                 const Maybe<OnNonvisible>& aNonvisibleAction)
{
  nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(mContent);
  if (!imageLoader) {
    MOZ_ASSERT_UNREACHABLE("Should have an nsIImageLoadingContent");
    nsAtomicContainerFrame::OnVisibilityChange(aNewVisibility, aNonvisibleAction);
    return;
  }

  imageLoader->OnVisibilityChange(aNewVisibility, aNonvisibleAction);

  if (aNewVisibility == Visibility::APPROXIMATELY_VISIBLE) {
    MaybeDecodeForPredictedSize();
  }

  nsAtomicContainerFrame::OnVisibilityChange(aNewVisibility, aNonvisibleAction);
}

nsIAtom*
nsImageFrame::GetType() const
{
  return nsGkAtoms::imageFrame;
}

#ifdef DEBUG_FRAME_DUMP
nsresult
nsImageFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("ImageFrame"), aResult);
}

void
nsImageFrame::List(FILE* out, const char* aPrefix, uint32_t aFlags) const
{
  nsCString str;
  ListGeneric(str, aPrefix, aFlags);

  // output the img src url
  nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(mContent);
  if (imageLoader) {
    nsCOMPtr<imgIRequest> currentRequest;
    imageLoader->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                            getter_AddRefs(currentRequest));
    if (currentRequest) {
      nsCOMPtr<nsIURI> uri;
      currentRequest->GetURI(getter_AddRefs(uri));
      nsAutoCString uristr;
      uri->GetAsciiSpec(uristr);
      str += nsPrintfCString(" [src=%s]", uristr.get());
    }
  }
  fprintf_stderr(out, "%s\n", str.get());
}
#endif

nsIFrame::LogicalSides
nsImageFrame::GetLogicalSkipSides(const ReflowInput* aReflowInput) const
{
  if (MOZ_UNLIKELY(StyleBorder()->mBoxDecorationBreak ==
                     StyleBoxDecorationBreak::Clone)) {
    return LogicalSides();
  }
  LogicalSides skip;
  if (nullptr != GetPrevInFlow()) {
    skip |= eLogicalSideBitsBStart;
  }
  if (nullptr != GetNextInFlow()) {
    skip |= eLogicalSideBitsBEnd;
  }
  return skip;
}

nsresult
nsImageFrame::GetIntrinsicImageSize(nsSize& aSize)
{
  if (mIntrinsicSize.width.GetUnit() == eStyleUnit_Coord &&
      mIntrinsicSize.height.GetUnit() == eStyleUnit_Coord) {
    aSize.SizeTo(mIntrinsicSize.width.GetCoordValue(),
                 mIntrinsicSize.height.GetCoordValue());
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

nsresult
nsImageFrame::LoadIcon(const nsAString& aSpec,
                       nsPresContext *aPresContext,
                       imgRequestProxy** aRequest)
{
  nsresult rv = NS_OK;
  NS_PRECONDITION(!aSpec.IsEmpty(), "What happened??");

  if (!sIOService) {
    rv = CallGetService(NS_IOSERVICE_CONTRACTID, &sIOService);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIURI> realURI;
  SpecToURI(aSpec, sIOService, getter_AddRefs(realURI));

  RefPtr<imgLoader> il =
    nsContentUtils::GetImgLoaderForDocument(aPresContext->Document());

  nsCOMPtr<nsILoadGroup> loadGroup;
  GetLoadGroup(aPresContext, getter_AddRefs(loadGroup));

  // For icon loads, we don't need to merge with the loadgroup flags
  nsLoadFlags loadFlags = nsIRequest::LOAD_NORMAL;
  nsContentPolicyType contentPolicyType = nsIContentPolicy::TYPE_INTERNAL_IMAGE;

  return il->LoadImage(realURI,     /* icon URI */
                       nullptr,      /* initial document URI; this is only
                                       relevant for cookies, so does not
                                       apply to icons. */
                       nullptr,      /* referrer (not relevant for icons) */
                       mozilla::net::RP_Unset,
                       nullptr,      /* principal (not relevant for icons) */
                       loadGroup,
                       gIconLoad,
                       nullptr,      /* No context */
                       nullptr,      /* Not associated with any particular document */
                       loadFlags,
                       nullptr,
                       contentPolicyType,
                       EmptyString(),
                       aRequest);
}

void
nsImageFrame::GetDocumentCharacterSet(nsACString& aCharset) const
{
  if (mContent) {
    NS_ASSERTION(mContent->GetComposedDoc(),
                 "Frame still alive after content removed from document!");
    aCharset = mContent->GetComposedDoc()->GetDocumentCharacterSet();
  }
}

void
nsImageFrame::SpecToURI(const nsAString& aSpec, nsIIOService *aIOService,
                         nsIURI **aURI)
{
  nsCOMPtr<nsIURI> baseURI;
  if (mContent) {
    baseURI = mContent->GetBaseURI();
  }
  nsAutoCString charset;
  GetDocumentCharacterSet(charset);
  NS_NewURI(aURI, aSpec, 
            charset.IsEmpty() ? nullptr : charset.get(), 
            baseURI, aIOService);
}

void
nsImageFrame::GetLoadGroup(nsPresContext *aPresContext, nsILoadGroup **aLoadGroup)
{
  if (!aPresContext)
    return;

  NS_PRECONDITION(nullptr != aLoadGroup, "null OUT parameter pointer");

  nsIPresShell *shell = aPresContext->GetPresShell();

  if (!shell)
    return;

  nsIDocument *doc = shell->GetDocument();
  if (!doc)
    return;

  *aLoadGroup = doc->GetDocumentLoadGroup().take();
}

nsresult nsImageFrame::LoadIcons(nsPresContext *aPresContext)
{
  NS_ASSERTION(!gIconLoad, "called LoadIcons twice");

  NS_NAMED_LITERAL_STRING(loadingSrc,"resource://gre-resources/loading-image.png");
  NS_NAMED_LITERAL_STRING(brokenSrc,"resource://gre-resources/broken-image.png");

  gIconLoad = new IconLoad();
  NS_ADDREF(gIconLoad);

  nsresult rv;
  // create a loader and load the images
  rv = LoadIcon(loadingSrc,
                aPresContext,
                getter_AddRefs(gIconLoad->mLoadingImage));
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = LoadIcon(brokenSrc,
                aPresContext,
                getter_AddRefs(gIconLoad->mBrokenImage));
  if (NS_FAILED(rv)) {
    return rv;
  }

  return rv;
}

NS_IMPL_ISUPPORTS(nsImageFrame::IconLoad, nsIObserver,
                  imgINotificationObserver)

static const char* kIconLoadPrefs[] = {
  "browser.display.force_inline_alttext",
  "browser.display.show_image_placeholders",
  "browser.display.show_loading_image_placeholder",
  nullptr
};

nsImageFrame::IconLoad::IconLoad()
{
  // register observers
  Preferences::AddStrongObservers(this, kIconLoadPrefs);
  GetPrefs();
}

void
nsImageFrame::IconLoad::Shutdown()
{
  Preferences::RemoveObservers(this, kIconLoadPrefs);
  // in case the pref service releases us later
  if (mLoadingImage) {
    mLoadingImage->CancelAndForgetObserver(NS_ERROR_FAILURE);
    mLoadingImage = nullptr;
  }
  if (mBrokenImage) {
    mBrokenImage->CancelAndForgetObserver(NS_ERROR_FAILURE);
    mBrokenImage = nullptr;
  }
}

NS_IMETHODIMP
nsImageFrame::IconLoad::Observe(nsISupports *aSubject, const char* aTopic,
                                const char16_t* aData)
{
  NS_ASSERTION(!nsCRT::strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID),
               "wrong topic");
#ifdef DEBUG
  // assert |aData| is one of our prefs.
  uint32_t i = 0;
  for (; i < ArrayLength(kIconLoadPrefs); ++i) {
    if (NS_ConvertASCIItoUTF16(kIconLoadPrefs[i]) == nsDependentString(aData))
      break;
  }
  MOZ_ASSERT(i < ArrayLength(kIconLoadPrefs));
#endif

  GetPrefs();
  return NS_OK;
}

void nsImageFrame::IconLoad::GetPrefs()
{
  mPrefForceInlineAltText =
    Preferences::GetBool("browser.display.force_inline_alttext");

  mPrefShowPlaceholders =
    Preferences::GetBool("browser.display.show_image_placeholders", true);

  mPrefShowLoadingPlaceholder =
    Preferences::GetBool("browser.display.show_loading_image_placeholder", true);
}

NS_IMETHODIMP
nsImageFrame::IconLoad::Notify(imgIRequest* aRequest,
                               int32_t aType,
                               const nsIntRect* aData)
{
  MOZ_ASSERT(aRequest);

  if (aType != imgINotificationObserver::LOAD_COMPLETE &&
      aType != imgINotificationObserver::FRAME_UPDATE) {
    return NS_OK;
  }

  if (aType == imgINotificationObserver::LOAD_COMPLETE) {
    nsCOMPtr<imgIContainer> image;
    aRequest->GetImage(getter_AddRefs(image));
    if (!image) {
      return NS_ERROR_FAILURE;
    }

    // Retrieve the image's intrinsic size.
    int32_t width = 0;
    int32_t height = 0;
    image->GetWidth(&width);
    image->GetHeight(&height);

    // Request a decode at that size.
    image->RequestDecodeForSize(IntSize(width, height),
                                imgIContainer::DECODE_FLAGS_DEFAULT);
  }

  nsTObserverArray<nsImageFrame*>::ForwardIterator iter(mIconObservers);
  nsImageFrame *frame;
  while (iter.HasMore()) {
    frame = iter.GetNext();
    frame->InvalidateFrame();
  }

  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsImageListener, imgINotificationObserver)

nsImageListener::nsImageListener(nsImageFrame *aFrame) :
  mFrame(aFrame)
{
}

nsImageListener::~nsImageListener()
{
}

NS_IMETHODIMP
nsImageListener::Notify(imgIRequest *aRequest, int32_t aType, const nsIntRect* aData)
{
  if (!mFrame)
    return NS_ERROR_FAILURE;

  return mFrame->Notify(aRequest, aType, aData);
}

static bool
IsInAutoWidthTableCellForQuirk(nsIFrame *aFrame)
{
  if (eCompatibility_NavQuirks != aFrame->PresContext()->CompatibilityMode())
    return false;
  // Check if the parent of the closest nsBlockFrame has auto width.
  nsBlockFrame *ancestor = nsLayoutUtils::FindNearestBlockAncestor(aFrame);
  if (ancestor->StyleContext()->GetPseudo() == nsCSSAnonBoxes::cellContent) {
    // Assume direct parent is a table cell frame.
    nsFrame *grandAncestor = static_cast<nsFrame*>(ancestor->GetParent());
    return grandAncestor &&
      grandAncestor->StylePosition()->mWidth.GetUnit() == eStyleUnit_Auto;
  }
  return false;
}

/* virtual */ void
nsImageFrame::AddInlineMinISize(nsRenderingContext* aRenderingContext,
                                nsIFrame::InlineMinISizeData* aData)
{
  nscoord isize = nsLayoutUtils::IntrinsicForContainer(aRenderingContext,
                    this, nsLayoutUtils::MIN_ISIZE);
  bool canBreak = !IsInAutoWidthTableCellForQuirk(this);
  aData->DefaultAddInlineMinISize(this, isize, canBreak);
}
