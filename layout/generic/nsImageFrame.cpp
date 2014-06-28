/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for replaced elements with bitmap image data */

#include "nsImageFrame.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/EventStates.h"
#include "mozilla/MouseEvents.h"

#include "nsCOMPtr.h"
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
#include "nsTransform2D.h"
#include "nsImageMap.h"
#include "nsIIOService.h"
#include "nsILoadGroup.h"
#include "nsISupportsPriority.h"
#include "nsNetUtil.h"
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
#include "nsStyleSet.h"
#include "nsBlockFrame.h"
#include "nsStyleStructInlines.h"

#include "mozilla/Preferences.h"

#include "mozilla/dom/Link.h"

using namespace mozilla;

// sizes (pixels) for image icon, padding and border frame
#define ICON_SIZE        (16)
#define ICON_PADDING     (3)
#define ALT_BORDER_WIDTH (1)


//we must add hooks soon
#define IMAGE_EDITOR_CHECK 1

// Default alignment value (so we can tell an unset value from a set value)
#define ALIGN_UNSET uint8_t(-1)

using namespace mozilla::layers;
using namespace mozilla::dom;

// static icon information
nsImageFrame::IconLoad* nsImageFrame::gIconLoad = nullptr;

// cached IO service for loading icons
nsIIOService* nsImageFrame::sIOService;

// test if the width and height are fixed, looking at the style data
static bool HaveFixedSize(const nsStylePosition* aStylePosition)
{
  // check the width and height values in the reflow state's style struct
  // - if width and height are specified as either coord or percentage, then
  //   the size of the image frame is constrained
  return aStylePosition->mWidth.IsCoordPercentCalcUnit() &&
         aStylePosition->mHeight.IsCoordPercentCalcUnit();
}
// use the data in the reflow state to decide if the image has a constrained size
// (i.e. width and height that are based on the containing block size and not the image size) 
// so we can avoid animated GIF related reflows
inline bool HaveFixedSize(const nsHTMLReflowState& aReflowState)
{ 
  NS_ASSERTION(aReflowState.mStylePosition, "crappy reflowState - null stylePosition");
  // when an image has percent css style height or width, but ComputedHeight() 
  // or ComputedWidth() of reflow state is  NS_UNCONSTRAINEDSIZE  
  // it needs to return false to cause an incremental reflow later
  // if an image is inside table like bug 156731 simple testcase III, 
  // during pass 1 reflow, ComputedWidth() is NS_UNCONSTRAINEDSIZE
  // in pass 2 reflow, ComputedWidth() is 0, it also needs to return false
  // see bug 156731
  const nsStyleCoord &height = aReflowState.mStylePosition->mHeight;
  const nsStyleCoord &width = aReflowState.mStylePosition->mWidth;
  return ((height.HasPercent() &&
           NS_UNCONSTRAINEDSIZE == aReflowState.ComputedHeight()) ||
          (width.HasPercent() &&
           (NS_UNCONSTRAINEDSIZE == aReflowState.ComputedWidth() ||
            0 == aReflowState.ComputedWidth())))
          ? false
          : HaveFixedSize(aReflowState.mStylePosition); 
}

nsIFrame*
NS_NewImageFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsImageFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsImageFrame)


nsImageFrame::nsImageFrame(nsStyleContext* aContext) :
  ImageFrameSuper(aContext),
  mComputedSize(0, 0),
  mIntrinsicRatio(0, 0),
  mDisplayingIcon(false),
  mFirstFrameComplete(false),
  mReflowCallbackPosted(false)
{
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
NS_QUERYFRAME_TAIL_INHERITING(ImageFrameSuper)

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
    NS_RELEASE(mImageMap);

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

  nsSplittableFrame::DestroyFrom(aDestructRoot);
}

void
nsImageFrame::DidSetStyleContext(nsStyleContext* aOldStyleContext)
{
  ImageFrameSuper::DidSetStyleContext(aOldStyleContext);

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
  nsSplittableFrame::Init(aContent, aParent, aPrevInFlow);

  mListener = new nsImageListener(this);

  nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(aContent);
  if (!imageLoader) {
    NS_RUNTIMEABORT("Why do we have an nsImageFrame here at all?");
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
  nsCOMPtr<nsISupportsPriority> p = do_QueryInterface(currentRequest);
  if (p)
    p->AdjustPriority(-1);

  // If we already have an image container, OnStartContainer won't be called
  if (currentRequest) {
    nsCOMPtr<imgIContainer> image;
    currentRequest->GetImage(getter_AddRefs(image));
    OnStartContainer(currentRequest, image);
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
  // Set the translation components.
  // XXXbz does this introduce rounding errors because of the cast to
  // float?  Should we just manually add that stuff in every time
  // instead?
  nsRect innerArea = GetInnerArea();
  aTransform.SetToTranslate(float(innerArea.x),
                            float(innerArea.y - GetContinuationOffset()));

  // Set the scale factors.
  if (mIntrinsicSize.width.GetUnit() == eStyleUnit_Coord &&
      mIntrinsicSize.width.GetCoordValue() != 0 &&
      mIntrinsicSize.height.GetUnit() == eStyleUnit_Coord &&
      mIntrinsicSize.height.GetCoordValue() != 0 &&
      mIntrinsicSize.width.GetCoordValue() != mComputedSize.width &&
      mIntrinsicSize.height.GetCoordValue() != mComputedSize.height) {

    aTransform.SetScale(float(mComputedSize.width)  /
                        float(mIntrinsicSize.width.GetCoordValue()),
                        float(mComputedSize.height) /
                        float(mIntrinsicSize.height.GetCoordValue()));
    return true;
  }

  return false;
}

/*
 * These two functions basically do the same check.  The first one
 * checks that the given request is the current request for our
 * mContent.  The second checks that the given image container the
 * same as the image container on the current request for our
 * mContent.
 */
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

bool
nsImageFrame::IsPendingLoad(imgIContainer* aContainer) const
{
  //  default to pending load in case of errors
  if (!aContainer) {
    NS_ERROR("No image container!");
    return true;
  }

  nsCOMPtr<nsIImageLoadingContent> imageLoader(do_QueryInterface(mContent));
  NS_ASSERTION(imageLoader, "No image loading content?");
  
  nsCOMPtr<imgIRequest> currentRequest;
  imageLoader->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                          getter_AddRefs(currentRequest));
  if (!currentRequest) {
    NS_ERROR("No current request");
    return true;
  }

  nsCOMPtr<imgIContainer> currentContainer;
  currentRequest->GetImage(getter_AddRefs(currentContainer));

  return currentContainer != aContainer;
  
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
               HaveFixedSize(aStyleContext->StylePosition()))) {
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
           !aElement->IsHTML(nsGkAtoms::object) &&
           !aElement->IsHTML(nsGkAtoms::input)) {
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
    // check whether we have fixed size
    useSizedBox = HaveFixedSize(aStyleContext->StylePosition());
  }

  return useSizedBox;
}

nsresult
nsImageFrame::Notify(imgIRequest* aRequest, int32_t aType, const nsIntRect* aData)
{
  if (aType == imgINotificationObserver::SIZE_AVAILABLE) {
    nsCOMPtr<imgIContainer> image;
    aRequest->GetImage(getter_AddRefs(image));
    return OnStartContainer(aRequest, image);
  }

  if (aType == imgINotificationObserver::FRAME_UPDATE) {
    return OnDataAvailable(aRequest, aData);
  }

  if (aType == imgINotificationObserver::FRAME_COMPLETE) {
    mFirstFrameComplete = true;
  }

  if (aType == imgINotificationObserver::LOAD_COMPLETE) {
    uint32_t imgStatus;
    aRequest->GetImageStatus(&imgStatus);
    nsresult status =
        imgStatus & imgIRequest::STATUS_ERROR ? NS_ERROR_FAILURE : NS_OK;
    return OnStopRequest(aRequest, status);
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
nsImageFrame::OnStartContainer(imgIRequest *aRequest, imgIContainer *aImage)
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
    mImage = nullptr;

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
    }
  }

  return NS_OK;
}

nsresult
nsImageFrame::OnDataAvailable(imgIRequest *aRequest,
                              const nsIntRect *aRect)
{
  if (mFirstFrameComplete) {
    nsCOMPtr<imgIContainer> container;
    aRequest->GetImage(getter_AddRefs(container));
    return FrameChanged(aRequest, container);
  }

  // XXX do we need to make sure that the reflow from the
  // OnStartContainer has been processed before we start calling
  // invalidate?

  NS_ENSURE_ARG_POINTER(aRect);

  if (!(mState & IMAGE_GOTINITIALREFLOW)) {
    // Don't bother to do anything; we have a reflow coming up!
    return NS_OK;
  }
  
  if (IsPendingLoad(aRequest)) {
    // We don't care
    return NS_OK;
  }

  nsIntRect rect = mImage ? mImage->GetImageSpaceInvalidationRect(*aRect)
                          : *aRect;

#ifdef DEBUG_decode
  printf("Source rect (%d,%d,%d,%d)\n",
         aRect->x, aRect->y, aRect->width, aRect->height);
#endif

  if (rect.IsEqualInterior(nsIntRect::GetMaxSizedIntRect())) {
    InvalidateFrame(nsDisplayItem::TYPE_IMAGE);
    InvalidateFrame(nsDisplayItem::TYPE_ALT_FEEDBACK);
  } else {
    nsRect invalid = SourceRectToDest(rect);
    InvalidateFrameWithRect(invalid, nsDisplayItem::TYPE_IMAGE);
    InvalidateFrameWithRect(invalid, nsDisplayItem::TYPE_ALT_FEEDBACK);
  }

  return NS_OK;
}

nsresult
nsImageFrame::OnStopRequest(imgIRequest *aRequest,
                            nsresult aStatus)
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
    mImage = nullptr;

    // Have to size to 0,0 so that GetDesiredSize recalculates the size
    mIntrinsicSize.width.SetCoordValue(0);
    mIntrinsicSize.height.SetCoordValue(0);
    mIntrinsicRatio.SizeTo(0, 0);
  }

  if (mState & IMAGE_GOTINITIALREFLOW) { // do nothing if we haven't gotten the initial reflow yet
    if (!(mState & IMAGE_SIZECONSTRAINED) && intrinsicSizeChanged) {
      nsIPresShell *presShell = PresContext()->GetPresShell();
      if (presShell) { 
        presShell->FrameNeedsReflow(this, nsIPresShell::eStyleChange,
                                    NS_FRAME_IS_DIRTY);
      }
    }
    // Update border+content to account for image change
    InvalidateFrame();
  }
}

nsresult
nsImageFrame::FrameChanged(imgIRequest *aRequest,
                           imgIContainer *aContainer)
{
  if (!StyleVisibility()->IsVisible()) {
    return NS_OK;
  }

  if (IsPendingLoad(aContainer)) {
    // We don't care about it
    return NS_OK;
  }

  InvalidateLayer(nsDisplayItem::TYPE_IMAGE);
  return NS_OK;
}

void
nsImageFrame::EnsureIntrinsicSizeAndRatio(nsPresContext* aPresContext)
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
      // - make the image big enough for the icon (it may not be
      // used if inline alt expansion is used instead)
      if (!(GetStateBits() & NS_FRAME_GENERATED_CONTENT)) {
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

/* virtual */ nsSize
nsImageFrame::ComputeSize(nsRenderingContext *aRenderingContext,
                          nsSize aCBSize, nscoord aAvailableWidth,
                          nsSize aMargin, nsSize aBorder, nsSize aPadding,
                          uint32_t aFlags)
{
  nsPresContext *presContext = PresContext();
  EnsureIntrinsicSizeAndRatio(presContext);

  nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(mContent);
  NS_ASSERTION(imageLoader, "No content node??");
  mozilla::IntrinsicSize intrinsicSize(mIntrinsicSize);

  // Content may override our default dimensions. This is termed as overriding
  // the intrinsic size by the spec, but all other consumers of mIntrinsic*
  // values are being used to refer to the real/true size of the image data.
  if (imageLoader && mImage &&
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

  return nsLayoutUtils::ComputeSizeWithIntrinsicDimensions(
                            aRenderingContext, this,
                            intrinsicSize, mIntrinsicRatio, aCBSize,
                            aMargin, aBorder, aPadding);
}

nsRect 
nsImageFrame::GetInnerArea() const
{
  return GetContentRect() - GetPosition();
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
nsImageFrame::GetMinWidth(nsRenderingContext *aRenderingContext)
{
  // XXX The caller doesn't account for constraints of the height,
  // min-height, and max-height properties.
  DebugOnly<nscoord> result;
  DISPLAY_MIN_WIDTH(this, result);
  nsPresContext *presContext = PresContext();
  EnsureIntrinsicSizeAndRatio(presContext);
  return mIntrinsicSize.width.GetUnit() == eStyleUnit_Coord ?
    mIntrinsicSize.width.GetCoordValue() : 0;
}

/* virtual */ nscoord
nsImageFrame::GetPrefWidth(nsRenderingContext *aRenderingContext)
{
  // XXX The caller doesn't account for constraints of the height,
  // min-height, and max-height properties.
  DebugOnly<nscoord> result;
  DISPLAY_PREF_WIDTH(this, result);
  nsPresContext *presContext = PresContext();
  EnsureIntrinsicSizeAndRatio(presContext);
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
                     nsHTMLReflowMetrics&     aMetrics,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsImageFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aMetrics, aStatus);
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                  ("enter nsImageFrame::Reflow: availSize=%d,%d",
                  aReflowState.AvailableWidth(), aReflowState.AvailableHeight()));

  NS_PRECONDITION(mState & NS_FRAME_IN_REFLOW, "frame is not in reflow");

  aStatus = NS_FRAME_COMPLETE;

  // see if we have a frozen size (i.e. a fixed width and height)
  if (HaveFixedSize(aReflowState)) {
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
    nsSize(aReflowState.ComputedWidth(), aReflowState.ComputedHeight());

  aMetrics.Width() = mComputedSize.width;
  aMetrics.Height() = mComputedSize.height;

  // add borders and padding
  aMetrics.Width()  += aReflowState.ComputedPhysicalBorderPadding().LeftRight();
  aMetrics.Height() += aReflowState.ComputedPhysicalBorderPadding().TopBottom();
  
  if (GetPrevInFlow()) {
    aMetrics.Width() = GetPrevInFlow()->GetSize().width;
    nscoord y = GetContinuationOffset();
    aMetrics.Height() -= y + aReflowState.ComputedPhysicalBorderPadding().top;
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
      NS_UNCONSTRAINEDSIZE != aReflowState.AvailableHeight() && 
      aMetrics.Height() > aReflowState.AvailableHeight()) { 
    // our desired height was greater than 0, so to avoid infinite
    // splitting, use 1 pixel as the min
    aMetrics.Height() = std::max(nsPresContext::CSSPixelsToAppUnits(1), aReflowState.AvailableHeight());
    aStatus = NS_FRAME_NOT_COMPLETE;
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
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aMetrics);
}

bool
nsImageFrame::ReflowFinished()
{
  mReflowCallbackPosted = false;

  nsLayoutUtils::UpdateImageVisibilityForFrame(this);

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
                            nsRenderingContext& aContext)
{
  nscoord totalWidth = 0;
  aContext.SetTextRunRTL(false);
  nscoord spaceWidth = aContext.GetWidth(' ');

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
      nsLayoutUtils::GetStringWidth(this, &aContext, aString, len);
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
  aRenderingContext.SetColor(StyleColor()->mColor);
  nsRefPtr<nsFontMetrics> fm;
  nsLayoutUtils::GetFontMetricsForFrame(this, getter_AddRefs(fm),
    nsLayoutUtils::FontSizeInflationFor(this));
  aRenderingContext.SetFont(fm);

  // Format the text to display within the formatting rect

  nscoord maxAscent = fm->MaxAscent();
  nscoord maxDescent = fm->MaxDescent();
  nscoord height = fm->MaxHeight();

  // XXX It would be nice if there was a way to have the font metrics tell
  // use where to break the text given a maximum width. At a minimum we need
  // to be able to get the break character...
  const char16_t* str = aAltText.get();
  int32_t          strLen = aAltText.Length();
  nscoord          y = aRect.y;

  if (!aPresContext->BidiEnabled() && HasRTLChars(aAltText)) {
    aPresContext->SetBidiEnabled();
  }

  // Always show the first line, even if we have to clip it below
  bool firstLine = true;
  while ((strLen > 0) && (firstLine || (y + maxDescent) < aRect.YMost())) {
    // Determine how much of the text to display on this line
    uint32_t  maxFit;  // number of characters that fit
    nscoord strWidth = MeasureString(str, strLen, aRect.width, maxFit,
                                     aRenderingContext);
    
    // Display the text
    nsresult rv = NS_ERROR_FAILURE;

    if (aPresContext->BidiEnabled()) {
      const nsStyleVisibility* vis = StyleVisibility();
      if (vis->mDirection == NS_STYLE_DIRECTION_RTL)
        rv = nsBidiPresUtils::RenderText(str, maxFit, NSBIDI_RTL,
                                         aPresContext, aRenderingContext,
                                         aRenderingContext,
                                         aRect.XMost() - strWidth, y + maxAscent);
      else
        rv = nsBidiPresUtils::RenderText(str, maxFit, NSBIDI_LTR,
                                         aPresContext, aRenderingContext,
                                         aRenderingContext,
                                         aRect.x, y + maxAscent);
    }
    if (NS_FAILED(rv))
      aRenderingContext.DrawString(str, maxFit, aRect.x, y + maxAscent);

    // Move to the next line
    str += maxFit;
    strLen -= maxFit;
    y += height;
    firstLine = false;
  }
}

struct nsRecessedBorder : public nsStyleBorder {
  nsRecessedBorder(nscoord aBorderWidth, nsPresContext* aPresContext)
    : nsStyleBorder(aPresContext)
  {
    NS_FOR_CSS_SIDES(side) {
      // Note: use SetBorderColor here because we want to make sure
      // the "special" flags are unset.
      SetBorderColor(side, NS_RGB(0, 0, 0));
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

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) MOZ_OVERRIDE
  {
    *aSnap = false;
    return mFrame->GetVisualOverflowRectRelativeToSelf() + ToReferenceFrame();
  }

  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext* aCtx) MOZ_OVERRIDE
  {
    nsImageFrame* f = static_cast<nsImageFrame*>(mFrame);
    EventStates state = f->GetContent()->AsElement()->State();
    f->DisplayAltFeedback(*aCtx,
                          mVisibleRect,
                          IMAGE_OK(state, true)
                             ? nsImageFrame::gIconLoad->mLoadingImage
                             : nsImageFrame::gIconLoad->mBrokenImage,
                          ToReferenceFrame());

  }

  NS_DISPLAY_DECL_NAME("AltFeedback", TYPE_ALT_FEEDBACK)
};

void
nsImageFrame::DisplayAltFeedback(nsRenderingContext& aRenderingContext,
                                 const nsRect&        aDirtyRect,
                                 imgIRequest*         aRequest,
                                 nsPoint              aPt)
{
  // We should definitely have a gIconLoad here.
  NS_ABORT_IF_FALSE(gIconLoad, "How did we succeed in Init then?");

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
    return;
  }

  // Paint the border
  nsRecessedBorder recessedBorder(borderEdgeWidth, PresContext());
  nsCSSRendering::PaintBorderWithStyleBorder(PresContext(), aRenderingContext,
                                             this, inner, inner,
                                             recessedBorder, mStyleContext);

  // Adjust the inner rect to account for the one pixel recessed border,
  // and a six pixel padding on each edge
  inner.Deflate(nsPresContext::CSSPixelsToAppUnits(ICON_PADDING+ALT_BORDER_WIDTH), 
                nsPresContext::CSSPixelsToAppUnits(ICON_PADDING+ALT_BORDER_WIDTH));
  if (inner.IsEmpty()) {
    return;
  }

  // Clip so we don't render outside the inner rect
  aRenderingContext.PushState();
  aRenderingContext.IntersectClip(inner);

  // Check if we should display image placeholders
  if (gIconLoad->mPrefShowPlaceholders) {
    const nsStyleVisibility* vis = StyleVisibility();
    nscoord size = nsPresContext::CSSPixelsToAppUnits(ICON_SIZE);

    bool iconUsed = false;

    // If we weren't previously displaying an icon, register ourselves
    // as an observer for load and animation updates and flag that we're
    // doing so now.
    if (aRequest && !mDisplayingIcon) {
      gIconLoad->AddIconObserver(this);
      mDisplayingIcon = true;
    }


    // If the icon in question is loaded and decoded, draw it
    uint32_t imageStatus = 0;
    if (aRequest)
      aRequest->GetImageStatus(&imageStatus);
    if (imageStatus & imgIRequest::STATUS_FRAME_COMPLETE) {
      nsCOMPtr<imgIContainer> imgCon;
      aRequest->GetImage(getter_AddRefs(imgCon));
      NS_ABORT_IF_FALSE(imgCon, "Frame Complete, but no image container?");
      nsRect dest((vis->mDirection == NS_STYLE_DIRECTION_RTL) ?
                  inner.XMost() - size : inner.x,
                  inner.y, size, size);
      nsLayoutUtils::DrawSingleImage(&aRenderingContext, PresContext(), imgCon,
        nsLayoutUtils::GetGraphicsFilterForFrame(this), dest, aDirtyRect,
        nullptr, imgIContainer::FLAG_NONE);
      iconUsed = true;
    }

    // if we could not draw the icon, flag that we're waiting for it and
    // just draw some graffiti in the mean time
    if (!iconUsed) {
      nscoord iconXPos = (vis->mDirection ==   NS_STYLE_DIRECTION_RTL) ?
                         inner.XMost() - size : inner.x;
      nscoord twoPX = nsPresContext::CSSPixelsToAppUnits(2);
      aRenderingContext.DrawRect(iconXPos, inner.y,size,size);
      aRenderingContext.PushState();
      aRenderingContext.SetColor(NS_RGB(0xFF,0,0));
      aRenderingContext.FillEllipse(size/2 + iconXPos, size/2 + inner.y,
                                    size/2 - twoPX, size/2 - twoPX);
      aRenderingContext.PopState();
    }

    // Reduce the inner rect by the width of the icon, and leave an
    // additional ICON_PADDING pixels for padding
    int32_t iconWidth = nsPresContext::CSSPixelsToAppUnits(ICON_SIZE + ICON_PADDING);
    if (vis->mDirection != NS_STYLE_DIRECTION_RTL)
      inner.x += iconWidth;
    inner.width -= iconWidth;
  }

  // If there's still room, display the alt-text
  if (!inner.IsEmpty()) {
    nsIContent* content = GetContent();
    if (content) {
      nsXPIDLString altText;
      nsCSSFrameConstructor::GetAlternateTextFor(content, content->Tag(),
                                                 altText);
      DisplayAltText(PresContext(), aRenderingContext, altText, inner);
    }
  }

  aRenderingContext.PopState();
}

#ifdef DEBUG
static void PaintDebugImageMap(nsIFrame* aFrame, nsRenderingContext* aCtx,
     const nsRect& aDirtyRect, nsPoint aPt) {
  nsImageFrame* f = static_cast<nsImageFrame*>(aFrame);
  nsRect inner = f->GetInnerArea() + aPt;

  aCtx->SetColor(NS_RGB(0, 0, 0));
  aCtx->PushState();
  aCtx->Translate(inner.TopLeft());
  f->GetImageMap()->Draw(aFrame, *aCtx);
  aCtx->PopState();
}
#endif

void
nsDisplayImage::Paint(nsDisplayListBuilder* aBuilder,
                      nsRenderingContext* aCtx) {
  uint32_t flags = imgIContainer::FLAG_NONE;
  if (aBuilder->ShouldSyncDecodeImages()) {
    flags |= imgIContainer::FLAG_SYNC_DECODE;
  }
  if (aBuilder->IsPaintingToWindow()) {
    flags |= imgIContainer::FLAG_HIGH_QUALITY_SCALING;
  }
  static_cast<nsImageFrame*>(mFrame)->
    PaintImage(*aCtx, ToReferenceFrame(), mVisibleRect, mImage, flags);
}

void
nsDisplayImage::ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                          const nsDisplayItemGeometry* aGeometry,
                                          nsRegion* aInvalidRegion)
{
  if (aBuilder->ShouldSyncDecodeImages() && mImage && !mImage->IsDecoded()) {
    bool snap;
    aInvalidRegion->Or(*aInvalidRegion, GetBounds(aBuilder, &snap));
  }

  nsDisplayImageContainer::ComputeInvalidationRegion(aBuilder, aGeometry, aInvalidRegion);
}

already_AddRefed<ImageContainer>
nsDisplayImage::GetContainer(LayerManager* aManager,
                             nsDisplayListBuilder* aBuilder)
{
  nsRefPtr<ImageContainer> container;
  nsresult rv = mImage->GetImageContainer(aManager, getter_AddRefs(container));
  NS_ENSURE_SUCCESS(rv, nullptr);
  return container.forget();
}

gfxRect
nsDisplayImage::GetDestRect()
{
  int32_t factor = mFrame->PresContext()->AppUnitsPerDevPixel();
  nsImageFrame* imageFrame = static_cast<nsImageFrame*>(mFrame);

  nsRect dest = imageFrame->GetInnerArea() + ToReferenceFrame();
  gfxRect destRect(dest.x, dest.y, dest.width, dest.height);
  destRect.ScaleInverse(factor); 

  return destRect;
}

LayerState
nsDisplayImage::GetLayerState(nsDisplayListBuilder* aBuilder,
                              LayerManager* aManager,
                              const ContainerLayerParameters& aParameters)
{
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

    gfxRect destRect = GetDestRect();

    destRect.width *= aParameters.mXScale;
    destRect.height *= aParameters.mYScale;

    // Calculate the scaling factor for the frame.
    gfxSize scale = gfxSize(destRect.width / imageWidth,
                            destRect.height / imageHeight);

    // If we are not scaling at all, no point in separating this into a layer.
    if (scale.width == 1.0f && scale.height == 1.0f) {
      return LAYER_NONE;
    }

    // If the target size is pretty small, no point in using a layer.
    if (destRect.width * destRect.height < 64 * 64) {
      return LAYER_NONE;
    }
  }

  nsRefPtr<ImageContainer> container;
  mImage->GetImageContainer(aManager, getter_AddRefs(container));
  if (!container) {
    return LAYER_NONE;
  }

  return LAYER_ACTIVE;
}

already_AddRefed<Layer>
nsDisplayImage::BuildLayer(nsDisplayListBuilder* aBuilder,
                           LayerManager* aManager,
                           const ContainerLayerParameters& aParameters)
{
  nsRefPtr<ImageContainer> container;
  nsresult rv = mImage->GetImageContainer(aManager, getter_AddRefs(container));
  NS_ENSURE_SUCCESS(rv, nullptr);

  nsRefPtr<ImageLayer> layer = static_cast<ImageLayer*>
    (aManager->GetLayerBuilder()->GetLeafLayerFor(aBuilder, this));
  if (!layer) {
    layer = aManager->CreateImageLayer();
    if (!layer)
      return nullptr;
  }
  layer->SetContainer(container);
  ConfigureLayer(layer, aParameters.mOffset);
  return layer.forget();
}

void
nsDisplayImage::ConfigureLayer(ImageLayer *aLayer, const nsIntPoint& aOffset)
{
  aLayer->SetFilter(nsLayoutUtils::GetGraphicsFilterForFrame(mFrame));

  int32_t imageWidth;
  int32_t imageHeight;
  mImage->GetWidth(&imageWidth);
  mImage->GetHeight(&imageHeight);

  NS_ASSERTION(imageWidth != 0 && imageHeight != 0, "Invalid image size!");

  const gfxRect destRect = GetDestRect();

  gfx::Matrix transform;
  gfxPoint p = destRect.TopLeft() + aOffset;
  transform.Translate(p.x, p.y);
  transform.Scale(destRect.Width()/imageWidth,
                  destRect.Height()/imageHeight);
  aLayer->SetBaseTransform(gfx::Matrix4x4::From2D(transform));
  aLayer->SetVisibleRegion(nsIntRect(0, 0, imageWidth, imageHeight));
}

void
nsImageFrame::PaintImage(nsRenderingContext& aRenderingContext, nsPoint aPt,
                         const nsRect& aDirtyRect, imgIContainer* aImage,
                         uint32_t aFlags)
{
  // Render the image into our content area (the area inside
  // the borders and padding)
  NS_ASSERTION(GetInnerArea().width == mComputedSize.width, "bad width");
  nsRect inner = GetInnerArea() + aPt;
  nsRect dest(inner.TopLeft(), mComputedSize);
  dest.y -= GetContinuationOffset();

  nsLayoutUtils::DrawSingleImage(&aRenderingContext, PresContext(), aImage,
    nsLayoutUtils::GetGraphicsFilterForFrame(this), dest, aDirtyRect,
    nullptr, aFlags);

  nsImageMap* map = GetImageMap();
  if (nullptr != map) {
    aRenderingContext.PushState();
    aRenderingContext.Translate(inner.TopLeft());
    aRenderingContext.SetColor(NS_RGB(255, 255, 255));
    aRenderingContext.SetLineStyle(nsLineStyle_kSolid);
    map->Draw(this, aRenderingContext);
    aRenderingContext.SetColor(NS_RGB(0, 0, 0));
    aRenderingContext.SetLineStyle(nsLineStyle_kDotted);
    map->Draw(this, aRenderingContext);
    aRenderingContext.PopState();
  }
}

void
nsImageFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                               const nsRect&           aDirtyRect,
                               const nsDisplayListSet& aLists)
{
  if (!IsVisibleForPainting(aBuilder))
    return;

  DisplayBorderBackgroundOutline(aBuilder, aLists);

  DisplayListClipState::AutoClipContainingBlockDescendantsToContentBox
    clip(aBuilder, this, DisplayListClipState::ASSUME_DRAWING_RESTRICTED_TO_CONTENT_RECT);

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
    } else {
      aLists.Content()->AppendNewToTop(new (aBuilder)
        nsDisplayImage(aBuilder, this, mImage));

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
      NS_ADDREF(mImageMap);
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

  if ((aEvent->message == NS_MOUSE_BUTTON_UP && 
       aEvent->AsMouseEvent()->button == WidgetMouseEvent::eLeftButton) ||
      aEvent->message == NS_MOUSE_MOVE) {
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
          uri->GetSpec(spec);
          spec += nsPrintfCString("?%d,%d", p.x, p.y);
          uri->SetSpec(spec);                
          
          bool clicked = false;
          if (aEvent->message == NS_MOUSE_BUTTON_UP) {
            *aEventStatus = nsEventStatus_eConsumeDoDefault; 
            clicked = true;
          }
          nsContentUtils::TriggerLink(anchorNode, aPresContext, uri, target,
                                      clicked, true, true);
        }
      }
    }
  }

  return nsSplittableFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
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
      nsRefPtr<nsStyleContext> areaStyle = 
        PresContext()->PresShell()->StyleSet()->
          ResolveStyleFor(area->AsElement(), StyleContext());
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
  nsresult rv = nsSplittableFrame::AttributeChanged(aNameSpaceID,
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

int
nsImageFrame::GetLogicalSkipSides(const nsHTMLReflowState* aReflowState) const
{
  if (MOZ_UNLIKELY(StyleBorder()->mBoxDecorationBreak ==
                     NS_STYLE_BOX_DECORATION_BREAK_CLONE)) {
    return 0;
  }
  int skip = 0;
  if (nullptr != GetPrevInFlow()) {
    skip |= LOGICAL_SIDE_B_START;
  }
  if (nullptr != GetNextInFlow()) {
    skip |= LOGICAL_SIDE_B_END;
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
 
  nsRefPtr<imgLoader> il =
    nsContentUtils::GetImgLoaderForDocument(aPresContext->Document());

  nsCOMPtr<nsILoadGroup> loadGroup;
  GetLoadGroup(aPresContext, getter_AddRefs(loadGroup));

  // For icon loads, we don't need to merge with the loadgroup flags
  nsLoadFlags loadFlags = nsIRequest::LOAD_NORMAL;

  return il->LoadImage(realURI,     /* icon URI */
                       nullptr,      /* initial document URI; this is only
                                       relevant for cookies, so does not
                                       apply to icons. */
                       nullptr,      /* referrer (not relevant for icons) */
                       nullptr,      /* principal (not relevant for icons) */
                       loadGroup,
                       gIconLoad,
                       nullptr,      /* Not associated with any particular document */
                       loadFlags,
                       nullptr,
                       nullptr,      /* channel policy not needed */
                       EmptyString(),
                       aRequest);
}

void
nsImageFrame::GetDocumentCharacterSet(nsACString& aCharset) const
{
  if (mContent) {
    NS_ASSERTION(mContent->GetDocument(),
                 "Frame still alive after content removed from document!");
    aCharset = mContent->GetDocument()->GetDocumentCharacterSet();
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
  return rv;
}

NS_IMPL_ISUPPORTS(nsImageFrame::IconLoad, nsIObserver,
                  imgINotificationObserver)

static const char* kIconLoadPrefs[] = {
  "browser.display.force_inline_alttext",
  "browser.display.show_image_placeholders",
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
  for (uint32_t i = 0; i < ArrayLength(kIconLoadPrefs) ||
                       (NS_NOTREACHED("wrong pref"), false); ++i)
    if (NS_ConvertASCIItoUTF16(kIconLoadPrefs[i]) == nsDependentString(aData))
      break;
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
}

NS_IMETHODIMP
nsImageFrame::IconLoad::Notify(imgIRequest *aRequest, int32_t aType, const nsIntRect* aData)
{
  if (aType != imgINotificationObserver::LOAD_COMPLETE &&
      aType != imgINotificationObserver::FRAME_UPDATE) {
    return NS_OK;
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
nsImageFrame::AddInlineMinWidth(nsRenderingContext *aRenderingContext,
                                nsIFrame::InlineMinWidthData *aData)
{

  NS_ASSERTION(GetParent(), "Must have a parent if we get here!");
  
  nsIFrame* parent = GetParent();
  bool canBreak =
    !CanContinueTextRun() &&
    parent->StyleText()->WhiteSpaceCanWrap(parent) &&
    !IsInAutoWidthTableCellForQuirk(this);

  if (canBreak)
    aData->OptionallyBreak(aRenderingContext);
 
  aData->trailingWhitespace = 0;
  aData->skipWhitespace = false;
  aData->trailingTextFrame = nullptr;
  aData->currentLine += nsLayoutUtils::IntrinsicForContainer(aRenderingContext,
                            this, nsLayoutUtils::MIN_WIDTH);
  aData->atStartOfLine = false;

  if (canBreak)
    aData->OptionallyBreak(aRenderingContext);

}
