/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for replaced elements with image data */

#include "nsImageFrame.h"

#include "TextDrawTarget.h"
#include "gfx2DGlue.h"
#include "gfxContext.h"
#include "gfxUtils.h"
#include "mozilla/ComputedStyle.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Encoding.h"
#include "mozilla/EventStates.h"
#include "mozilla/HTMLEditor.h"
#include "mozilla/dom/ImageTracker.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Helpers.h"
#include "mozilla/gfx/PathHelpers.h"
#include "mozilla/dom/GeneratedImageContent.h"
#include "mozilla/dom/HTMLAreaElement.h"
#include "mozilla/dom/HTMLImageElement.h"
#include "mozilla/dom/ResponsiveImageSelector.h"
#include "mozilla/layers/RenderRootStateManager.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/PresShell.h"
#include "mozilla/PresShellInlines.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/SVGImageContext.h"
#include "mozilla/Unused.h"

#include "nsCOMPtr.h"
#include "nsFontMetrics.h"
#include "nsIImageLoadingContent.h"
#include "nsImageLoadingContent.h"
#include "nsString.h"
#include "nsPrintfCString.h"
#include "nsPresContext.h"
#include "nsGkAtoms.h"
#include "mozilla/dom/Document.h"
#include "nsContentUtils.h"
#include "nsCSSAnonBoxes.h"
#include "nsStyleConsts.h"
#include "nsStyleUtil.h"
#include "nsTransform2D.h"
#include "nsImageMap.h"
#include "nsILoadGroup.h"
#include "nsNetUtil.h"
#include "nsNetCID.h"
#include "nsCSSRendering.h"
#include "nsNameSpaceManager.h"
#include <algorithm>
#ifdef ACCESSIBILITY
#  include "nsAccessibilityService.h"
#endif
#include "nsLayoutUtils.h"
#include "nsDisplayList.h"
#include "nsIContent.h"
#include "FrameLayerBuilder.h"
#include "mozilla/dom/Selection.h"
#include "nsIURIMutator.h"

#include "imgIContainer.h"
#include "imgLoader.h"
#include "imgRequestProxy.h"

#include "nsCSSFrameConstructor.h"
#include "nsRange.h"

#include "nsError.h"
#include "nsBidiUtils.h"
#include "nsBidiPresUtils.h"

#include "gfxRect.h"
#include "ImageLayers.h"
#include "ImageContainer.h"
#include "mozilla/ServoStyleSet.h"
#include "nsBlockFrame.h"
#include "nsStyleStructInlines.h"

#include "mozilla/Preferences.h"

#include "mozilla/dom/Link.h"
#include "mozilla/dom/HTMLAnchorElement.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::image;
using namespace mozilla::layers;

using mozilla::layout::TextDrawTarget;

// sizes (pixels) for image icon, padding and border frame
#define ICON_SIZE (16)
#define ICON_PADDING (3)
#define ALT_BORDER_WIDTH (1)

// Default alignment value (so we can tell an unset value from a set value)
#define ALIGN_UNSET uint8_t(-1)

// static icon information
StaticRefPtr<nsImageFrame::IconLoad> nsImageFrame::gIconLoad;

// test if the width and height are fixed, looking at the style data
// This is used by nsImageFrame::ShouldCreateImageFrameFor and should
// not be used for layout decisions.
static bool HaveSpecifiedSize(const nsStylePosition* aStylePosition) {
  // check the width and height values in the reflow input's style struct
  // - if width and height are specified as either coord or percentage, then
  //   the size of the image frame is constrained
  return aStylePosition->mWidth.IsLengthPercentage() &&
         aStylePosition->mHeight.IsLengthPercentage();
}

template <typename SizeOrMaxSize>
static bool DependsOnIntrinsicSize(const SizeOrMaxSize& aMinOrMaxSize) {
  auto length = nsIFrame::ToExtremumLength(aMinOrMaxSize);
  if (!length) {
    return false;
  }
  switch (*length) {
    case nsIFrame::ExtremumLength::MinContent:
    case nsIFrame::ExtremumLength::MaxContent:
    case nsIFrame::ExtremumLength::MozFitContent:
      return true;
    case nsIFrame::ExtremumLength::MozAvailable:
      return false;
  }
  MOZ_ASSERT_UNREACHABLE("Unknown sizing keyword?");
  return false;
}

// Decide whether we can optimize away reflows that result from the
// image's intrinsic size changing.
static bool SizeDependsOnIntrinsicSize(const ReflowInput& aReflowInput) {
  const auto& position = *aReflowInput.mStylePosition;
  WritingMode wm = aReflowInput.GetWritingMode();
  // Don't try to make this optimization when an image has percentages
  // in its 'width' or 'height'.  The percentages might be treated like
  // auto (especially for intrinsic width calculations and for heights).
  //
  // min-width: min-content and such can also affect our intrinsic size.
  // but note that those keywords on the block axis behave like auto, so we
  // don't need to check them.
  //
  // Flex item's min-[width|height]:auto resolution depends on intrinsic size.
  return !position.mHeight.ConvertsToLength() ||
         !position.mWidth.ConvertsToLength() ||
         DependsOnIntrinsicSize(position.MinISize(wm)) ||
         DependsOnIntrinsicSize(position.MaxISize(wm)) ||
         aReflowInput.mFrame->IsFlexItem();
}

nsIFrame* NS_NewImageFrame(PresShell* aPresShell, ComputedStyle* aStyle) {
  return new (aPresShell) nsImageFrame(aStyle, aPresShell->GetPresContext(),
                                       nsImageFrame::Kind::ImageElement);
}

nsIFrame* NS_NewImageFrameForContentProperty(PresShell* aPresShell,
                                             ComputedStyle* aStyle) {
  return new (aPresShell) nsImageFrame(aStyle, aPresShell->GetPresContext(),
                                       nsImageFrame::Kind::ContentProperty);
}

nsIFrame* NS_NewImageFrameForGeneratedContentIndex(PresShell* aPresShell,
                                                   ComputedStyle* aStyle) {
  return new (aPresShell)
      nsImageFrame(aStyle, aPresShell->GetPresContext(),
                   nsImageFrame::Kind::ContentPropertyAtIndex);
}

bool nsImageFrame::ShouldShowBrokenImageIcon() const {
  // NOTE(emilio, https://github.com/w3c/csswg-drafts/issues/2832): WebKit and
  // Blink behave differently here for content: url(..), for now adapt to
  // Blink's behavior.
  if (mKind != Kind::ImageElement) {
    return false;
  }

  // <img alt=""> is special, and it shouldn't draw the broken image icon,
  // unlike the no-alt attribute or non-empty-alt-attribute case.
  if (auto* image = HTMLImageElement::FromNode(mContent)) {
    const nsAttrValue* alt = image->GetParsedAttr(nsGkAtoms::alt);
    if (alt && alt->IsEmptyString()) {
      return false;
    }
  }

  // check for broken images. valid null images (eg. img src="") are
  // not considered broken because they have no image requests
  if (nsCOMPtr<imgIRequest> currentRequest = GetCurrentRequest()) {
    uint32_t imageStatus;
    return NS_SUCCEEDED(currentRequest->GetImageStatus(&imageStatus)) &&
           (imageStatus & imgIRequest::STATUS_ERROR);
  }

  nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(mContent);
  MOZ_ASSERT(imageLoader);
  // Show the broken image icon only if we've tried to perform a load at all
  // (that is, if we have a current uri).
  nsCOMPtr<nsIURI> currentURI = imageLoader->GetCurrentURI();
  return !!currentURI;
}

nsImageFrame* nsImageFrame::CreateContinuingFrame(
    mozilla::PresShell* aPresShell, ComputedStyle* aStyle) const {
  return new (aPresShell)
      nsImageFrame(aStyle, aPresShell->GetPresContext(), mKind);
}

NS_IMPL_FRAMEARENA_HELPERS(nsImageFrame)

nsImageFrame::nsImageFrame(ComputedStyle* aStyle, nsPresContext* aPresContext,
                           ClassID aID, Kind aKind)
    : nsAtomicContainerFrame(aStyle, aPresContext, aID),
      mComputedSize(0, 0),
      mIntrinsicSize(0, 0),
      mKind(aKind),
      mContentURLRequestRegistered(false),
      mDisplayingIcon(false),
      mFirstFrameComplete(false),
      mReflowCallbackPosted(false),
      mForceSyncDecoding(false) {
  EnableVisibilityTracking();
}

nsImageFrame::~nsImageFrame() = default;

NS_QUERYFRAME_HEAD(nsImageFrame)
  NS_QUERYFRAME_ENTRY(nsImageFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsAtomicContainerFrame)

#ifdef ACCESSIBILITY
a11y::AccType nsImageFrame::AccessibleType() {
  // Don't use GetImageMap() to avoid reentrancy into accessibility.
  if (HasImageMap()) {
    return a11y::eHTMLImageMapType;
  }

  return a11y::eImageType;
}
#endif

void nsImageFrame::DisconnectMap() {
  if (!mImageMap) {
    return;
  }

  mImageMap->Destroy();
  mImageMap = nullptr;

#ifdef ACCESSIBILITY
  if (nsAccessibilityService* accService = GetAccService()) {
    accService->RecreateAccessible(PresShell(), mContent);
  }
#endif
}

void nsImageFrame::DestroyFrom(nsIFrame* aDestructRoot,
                               PostDestroyData& aPostDestroyData) {
  if (mReflowCallbackPosted) {
    PresShell()->CancelReflowCallback(this);
    mReflowCallbackPosted = false;
  }

  // Tell our image map, if there is one, to clean up
  // This causes the nsImageMap to unregister itself as
  // a DOM listener.
  DisconnectMap();

  MOZ_ASSERT(mListener);

  if (mKind == Kind::ImageElement) {
    MOZ_ASSERT(!mContentURLRequest);
    MOZ_ASSERT(!mContentURLRequestRegistered);
    nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(mContent);
    MOZ_ASSERT(imageLoader);

    // Notify our image loading content that we are going away so it can
    // deregister with our refresh driver.
    imageLoader->FrameDestroyed(this);
    imageLoader->RemoveNativeObserver(mListener);
  } else if (mContentURLRequest) {
    PresContext()->Document()->ImageTracker()->Remove(mContentURLRequest);
    nsLayoutUtils::DeregisterImageRequest(PresContext(), mContentURLRequest,
                                          &mContentURLRequestRegistered);
    mContentURLRequest->Cancel(NS_BINDING_ABORTED);
  }

  // set the frame to null so we don't send messages to a dead object.
  mListener->SetFrame(nullptr);
  mListener = nullptr;

  // If we were displaying an icon, take ourselves off the list
  if (mDisplayingIcon) gIconLoad->RemoveIconObserver(this);

  nsAtomicContainerFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}

void nsImageFrame::MaybeRecordContentUrlOnImageTelemetry() {
  if (mKind != Kind::ImageElement) {
    return;
  }
  const auto& content = *StyleContent();
  if (content.ContentCount() != 1) {
    return;
  }
  const auto& item = content.ContentAt(0);
  if (!item.IsImage()) {
    return;
  }
  PresContext()->Document()->SetUseCounter(
      eUseCounter_custom_ContentUrlOnImageContent);
}

void nsImageFrame::DidSetComputedStyle(ComputedStyle* aOldStyle) {
  nsAtomicContainerFrame::DidSetComputedStyle(aOldStyle);

  MaybeRecordContentUrlOnImageTelemetry();

  auto newOrientation = StyleVisibility()->mImageOrientation;

  // We need to update our orientation either if we had no ComputedStyle before
  // because this is the first time it's been set, or if the image-orientation
  // property changed from its previous value.
  bool shouldUpdateOrientation =
      mImage &&
      (!aOldStyle ||
       aOldStyle->StyleVisibility()->mImageOrientation != newOrientation);

  if (shouldUpdateOrientation) {
    nsCOMPtr<imgIContainer> image(mImage->Unwrap());
    mImage = nsLayoutUtils::OrientImage(image, newOrientation);

    UpdateIntrinsicSize();
    UpdateIntrinsicRatio();
  } else if (!aOldStyle || aOldStyle->StylePosition()->mAspectRatio !=
                               StylePosition()->mAspectRatio) {
    UpdateIntrinsicRatio();
  }
}

static bool SizeIsAvailable(imgIRequest* aRequest) {
  if (!aRequest) {
    return false;
  }

  uint32_t imageStatus = 0;
  nsresult rv = aRequest->GetImageStatus(&imageStatus);
  return NS_SUCCEEDED(rv) && (imageStatus & imgIRequest::STATUS_SIZE_AVAILABLE);
}

void nsImageFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                        nsIFrame* aPrevInFlow) {
  MOZ_ASSERT_IF(aPrevInFlow,
                aPrevInFlow->Type() == Type() &&
                    static_cast<nsImageFrame*>(aPrevInFlow)->mKind == mKind);

  nsAtomicContainerFrame::Init(aContent, aParent, aPrevInFlow);

  mListener = new nsImageListener(this);

  if (!gIconLoad) {
    LoadIcons(PresContext());
  }

  if (mKind == Kind::ImageElement) {
    nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(aContent);
    MOZ_ASSERT(imageLoader);
    imageLoader->AddNativeObserver(mListener);
    // We have a PresContext now, so we need to notify the image content node
    // that it can register images.
    imageLoader->FrameCreated(this);
  } else {
    uint32_t contentIndex = 0;
    const nsStyleContent* styleContent = StyleContent();
    if (mKind == Kind::ContentPropertyAtIndex) {
      MOZ_RELEASE_ASSERT(
          aParent->GetContent()->IsGeneratedContentContainerForMarker() ||
          aParent->GetContent()->IsGeneratedContentContainerForAfter() ||
          aParent->GetContent()->IsGeneratedContentContainerForBefore());
      MOZ_RELEASE_ASSERT(
          aContent->IsHTMLElement(nsGkAtoms::mozgeneratedcontentimage));
      nsIFrame* nonAnonymousParent = aParent;
      while (nonAnonymousParent->Style()->IsAnonBox()) {
        nonAnonymousParent = nonAnonymousParent->GetParent();
      }
      MOZ_RELEASE_ASSERT(aParent->GetContent() ==
                         nonAnonymousParent->GetContent());
      styleContent = nonAnonymousParent->StyleContent();
      contentIndex = static_cast<GeneratedImageContent*>(aContent)->Index();
    }
    MOZ_RELEASE_ASSERT(contentIndex < styleContent->ContentCount());
    MOZ_RELEASE_ASSERT(styleContent->ContentAt(contentIndex).IsImage());
    const StyleImage& image = styleContent->ContentAt(contentIndex).AsImage();
    MOZ_ASSERT(image.IsImageRequestType(),
               "Content image should only parse url() type");
    auto [finalImage, resolution] = image.FinalImageAndResolution();
    Document* doc = PresContext()->Document();
    if (imgRequestProxy* proxy = finalImage->GetImageRequest()) {
      mContentURLRequestResolution = resolution;
      proxy->Clone(mListener, doc, getter_AddRefs(mContentURLRequest));
      SetupForContentURLRequest();
    }
  }

  // Give image loads associated with an image frame a small priority boost.
  if (nsCOMPtr<imgIRequest> currentRequest = GetCurrentRequest()) {
    uint32_t categoryToBoostPriority = imgIRequest::CATEGORY_FRAME_INIT;

    // Increase load priority further if intrinsic size might be important for
    // layout.
    if (!HaveSpecifiedSize(StylePosition())) {
      categoryToBoostPriority |= imgIRequest::CATEGORY_SIZE_QUERY;
    }

    currentRequest->BoostPriority(categoryToBoostPriority);
  }
}

void nsImageFrame::SetupForContentURLRequest() {
  MOZ_ASSERT(mKind != Kind::ImageElement);
  if (!mContentURLRequest) {
    return;
  }

  // We're not using AssociateRequestToFrame for the content property, so we
  // need to add it to the image tracker manually.
  PresContext()->Document()->ImageTracker()->Add(mContentURLRequest);

  uint32_t status = 0;
  nsresult rv = mContentURLRequest->GetImageStatus(&status);
  if (NS_FAILED(rv)) {
    return;
  }

  if (status & imgIRequest::STATUS_SIZE_AVAILABLE) {
    nsCOMPtr<imgIContainer> image;
    mContentURLRequest->GetImage(getter_AddRefs(image));
    OnSizeAvailable(mContentURLRequest, image);
  }

  if (status & imgIRequest::STATUS_FRAME_COMPLETE) {
    mFirstFrameComplete = true;
  }

  if (status & imgIRequest::STATUS_IS_ANIMATED) {
    nsLayoutUtils::RegisterImageRequest(PresContext(), mContentURLRequest,
                                        &mContentURLRequestRegistered);
  }
}

static void ScaleIntrinsicSizeForDensity(IntrinsicSize& aSize,
                                         double aDensity) {
  if (aDensity == 1.0) {
    return;
  }

  if (aSize.width) {
    aSize.width = Some(NSToCoordRound(double(*aSize.width) / aDensity));
  }
  if (aSize.height) {
    aSize.height = Some(NSToCoordRound(double(*aSize.height) / aDensity));
  }
}

static void ScaleIntrinsicSizeForDensity(nsIContent& aContent,
                                         IntrinsicSize& aSize) {
  auto* image = HTMLImageElement::FromNode(aContent);
  if (!image) {
    return;
  }

  ResponsiveImageSelector* selector = image->GetResponsiveImageSelector();
  if (!selector) {
    return;
  }

  double density = selector->GetSelectedImageDensity();
  MOZ_ASSERT(density >= 0.0);
  ScaleIntrinsicSizeForDensity(aSize, density);
}

static IntrinsicSize ComputeIntrinsicSize(imgIContainer* aImage,
                                          bool aUseMappedRatio,
                                          nsImageFrame::Kind aKind,
                                          const nsImageFrame& aFrame) {
  const ComputedStyle& style = *aFrame.Style();
  if (style.StyleDisplay()->IsContainSize()) {
    return IntrinsicSize(0, 0);
  }

  nsSize size;
  if (aImage && NS_SUCCEEDED(aImage->GetIntrinsicSize(&size))) {
    IntrinsicSize intrinsicSize;
    intrinsicSize.width = size.width == -1 ? Nothing() : Some(size.width);
    intrinsicSize.height = size.height == -1 ? Nothing() : Some(size.height);
    if (aKind == nsImageFrame::Kind::ImageElement) {
      ScaleIntrinsicSizeForDensity(*aFrame.GetContent(), intrinsicSize);
    } else {
      ScaleIntrinsicSizeForDensity(intrinsicSize,
                                   aFrame.GetContentURLRequestResolution());
    }
    return intrinsicSize;
  }

  if (aFrame.ShouldShowBrokenImageIcon()) {
    nscoord edgeLengthToUse = nsPresContext::CSSPixelsToAppUnits(
        ICON_SIZE + (2 * (ICON_PADDING + ALT_BORDER_WIDTH)));
    return IntrinsicSize(edgeLengthToUse, edgeLengthToUse);
  }

  if (aUseMappedRatio && style.StylePosition()->mAspectRatio.HasRatio()) {
    return IntrinsicSize();
  }

  return IntrinsicSize(0, 0);
}

// For compat reasons, see bug 1602047, we don't use the intrinsic ratio from
// width="" and height="" for images with no src attribute (no request).
//
// But we shouldn't get fooled by <img loading=lazy>. We do want to apply the
// ratio then...
bool nsImageFrame::ShouldUseMappedAspectRatio() const {
  if (mKind != Kind::ImageElement) {
    return true;
  }
  nsCOMPtr<imgIRequest> currentRequest = GetCurrentRequest();
  if (currentRequest) {
    return true;
  }
  // TODO(emilio): Investigate the compat situation of the above check, maybe we
  // can just check for empty src attribute or something...
  auto* image = HTMLImageElement::FromNode(mContent);
  return image && image->IsAwaitingLoadOrLazyLoading();
}

bool nsImageFrame::UpdateIntrinsicSize() {
  IntrinsicSize oldIntrinsicSize = mIntrinsicSize;
  mIntrinsicSize =
      ComputeIntrinsicSize(mImage, ShouldUseMappedAspectRatio(), mKind, *this);
  return mIntrinsicSize != oldIntrinsicSize;
}

static AspectRatio ComputeIntrinsicRatio(imgIContainer* aImage,
                                         bool aUseMappedRatio,
                                         const nsImageFrame& aFrame) {
  const ComputedStyle& style = *aFrame.Style();
  if (style.StyleDisplay()->IsContainSize()) {
    return AspectRatio();
  }

  if (aImage) {
    if (Maybe<AspectRatio> fromImage = aImage->GetIntrinsicRatio()) {
      return *fromImage;
    }
  }
  if (aUseMappedRatio) {
    const StyleAspectRatio& ratio = style.StylePosition()->mAspectRatio;
    if (ratio.auto_ && ratio.HasRatio()) {
      // Return the mapped intrinsic aspect ratio stored in
      // nsStylePosition::mAspectRatio.
      return ratio.ratio.AsRatio().ToLayoutRatio(UseBoxSizing::Yes);
    }
  }
  if (aFrame.ShouldShowBrokenImageIcon()) {
    return AspectRatio(1.0f);
  }
  return AspectRatio();
}

bool nsImageFrame::UpdateIntrinsicRatio() {
  AspectRatio oldIntrinsicRatio = mIntrinsicRatio;
  mIntrinsicRatio =
      ComputeIntrinsicRatio(mImage, ShouldUseMappedAspectRatio(), *this);
  return mIntrinsicRatio != oldIntrinsicRatio;
}

bool nsImageFrame::GetSourceToDestTransform(nsTransform2D& aTransform) {
  // First, figure out destRect (the rect we're rendering into).
  // NOTE: We use mComputedSize instead of just GetInnerArea()'s own size here,
  // because GetInnerArea() might be smaller if we're fragmented, whereas
  // mComputedSize has our full content-box size (which we need for
  // ComputeObjectDestRect to work correctly).
  nsRect constraintRect(GetInnerArea().TopLeft(), mComputedSize);
  constraintRect.y -= GetContinuationOffset();

  nsRect destRect = nsLayoutUtils::ComputeObjectDestRect(
      constraintRect, mIntrinsicSize, mIntrinsicRatio, StylePosition());
  // Set the translation components, based on destRect
  // XXXbz does this introduce rounding errors because of the cast to
  // float?  Should we just manually add that stuff in every time
  // instead?
  aTransform.SetToTranslate(float(destRect.x), float(destRect.y));

  // NOTE(emilio): This intrinsicSize is not the same as the layout intrinsic
  // size (mIntrinsicSize), which can be scaled due to ResponsiveImageSelector,
  // see ScaleIntrinsicSizeForDensity.
  nsSize intrinsicSize;
  if (!mImage || !NS_SUCCEEDED(mImage->GetIntrinsicSize(&intrinsicSize)) ||
      intrinsicSize.IsEmpty()) {
    return false;
  }

  aTransform.SetScale(float(destRect.width) / float(intrinsicSize.width),
                      float(destRect.height) / float(intrinsicSize.height));
  return true;
}

// This function checks whether the given request is the current request for our
// mContent.
bool nsImageFrame::IsPendingLoad(imgIRequest* aRequest) const {
  // Default to pending load in case of errors
  if (mKind != Kind::ImageElement) {
    MOZ_ASSERT(aRequest == mContentURLRequest);
    return false;
  }

  nsCOMPtr<nsIImageLoadingContent> imageLoader(do_QueryInterface(mContent));
  MOZ_ASSERT(imageLoader);

  int32_t requestType = nsIImageLoadingContent::UNKNOWN_REQUEST;
  imageLoader->GetRequestType(aRequest, &requestType);

  return requestType != nsIImageLoadingContent::CURRENT_REQUEST;
}

nsRect nsImageFrame::SourceRectToDest(const nsIntRect& aRect) {
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

static bool ImageOk(EventStates aState) {
  return !aState.HasState(NS_EVENT_STATE_BROKEN);
}

static bool HasAltText(const Element& aElement) {
  // We always return some alternate text for <input>, see
  // nsCSSFrameConstructor::GetAlternateTextFor.
  if (aElement.IsHTMLElement(nsGkAtoms::input)) {
    return true;
  }

  MOZ_ASSERT(aElement.IsHTMLElement(nsGkAtoms::img));
  return aElement.HasNonEmptyAttr(nsGkAtoms::alt);
}

// Check if we want to use an image frame or just let the frame constructor make
// us into an inline.
/* static */
bool nsImageFrame::ShouldCreateImageFrameFor(const Element& aElement,
                                             ComputedStyle& aStyle) {
  if (ImageOk(aElement.State())) {
    // Image is fine or loading; do the image frame thing
    return true;
  }

  if (aStyle.StyleUIReset()->mMozForceBrokenImageIcon) {
    return true;
  }

  // if our "do not show placeholders" pref is set, skip the icon
  if (gIconLoad && gIconLoad->mPrefForceInlineAltText) {
    return false;
  }

  if (!HasAltText(aElement)) {
    return true;
  }

  if (aElement.OwnerDoc()->GetCompatibilityMode() == eCompatibility_NavQuirks) {
    // FIXME(emilio): We definitely don't reframe when this changes...
    return HaveSpecifiedSize(aStyle.StylePosition());
  }

  return false;
}

void nsImageFrame::Notify(imgIRequest* aRequest, int32_t aType,
                          const nsIntRect* aRect) {
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

  if (aType == imgINotificationObserver::IS_ANIMATED &&
      mKind != Kind::ImageElement) {
    nsLayoutUtils::RegisterImageRequest(PresContext(), mContentURLRequest,
                                        &mContentURLRequestRegistered);
  }

  if (aType == imgINotificationObserver::LOAD_COMPLETE) {
    uint32_t imgStatus;
    aRequest->GetImageStatus(&imgStatus);
    nsresult status =
        imgStatus & imgIRequest::STATUS_ERROR ? NS_ERROR_FAILURE : NS_OK;
    return OnLoadComplete(aRequest, status);
  }
}

void nsImageFrame::OnSizeAvailable(imgIRequest* aRequest,
                                   imgIContainer* aImage) {
  if (!aImage) {
    return;
  }

  /* Get requested animation policy from the pres context:
   *   normal = 0
   *   one frame = 1
   *   one loop = 2
   */
  aImage->SetAnimationMode(PresContext()->ImageAnimationMode());

  if (IsPendingLoad(aRequest)) {
    // We don't care
    return;
  }

  UpdateImage(aRequest, aImage);
}

void nsImageFrame::UpdateImage(imgIRequest* aRequest, imgIContainer* aImage) {
  MOZ_ASSERT(aRequest);
  if (SizeIsAvailable(aRequest)) {
    // This is valid and for the current request, so update our stored image
    // container, orienting according to our style.
    mImage = nsLayoutUtils::OrientImage(aImage,
                                        StyleVisibility()->mImageOrientation);
    MOZ_ASSERT(mImage);
  } else {
    // We no longer have a valid image, so release our stored image container.
    mImage = mPrevImage = nullptr;
  }
  // NOTE(emilio): Intentionally using `|` instead of `||` to avoid
  // short-circuiting.
  bool intrinsicSizeChanged = UpdateIntrinsicSize() | UpdateIntrinsicRatio();
  if (!GotInitialReflow()) {
    return;
  }

  // We're going to need to repaint now either way.
  InvalidateFrame();

  if (intrinsicSizeChanged) {
    // Now we need to reflow if we have an unconstrained size and have
    // already gotten the initial reflow.
    if (!(mState & IMAGE_SIZECONSTRAINED)) {
#ifdef ACCESSIBILITY
      if (nsAccessibilityService* accService = GetAccService()) {
        accService->NotifyOfImageSizeAvailable(PresShell(), mContent);
      }
#endif
      PresShell()->FrameNeedsReflow(this, IntrinsicDirty::StyleChange,
                                    NS_FRAME_IS_DIRTY);
    } else if (PresShell()->IsActive()) {
      // We've already gotten the initial reflow, and our size hasn't changed,
      // so we're ready to request a decode.
      MaybeDecodeForPredictedSize();
    }
  }
}

void nsImageFrame::OnFrameUpdate(imgIRequest* aRequest,
                                 const nsIntRect* aRect) {
  if (NS_WARN_IF(!aRect)) {
    return;
  }

  if (!GotInitialReflow()) {
    // Don't bother to do anything; we have a reflow coming up!
    return;
  }

  if (mFirstFrameComplete && !StyleVisibility()->IsVisible()) {
    return;
  }

  if (IsPendingLoad(aRequest)) {
    // We don't care
    return;
  }

  nsIntRect layerInvalidRect =
      mImage ? mImage->GetImageSpaceInvalidationRect(*aRect) : *aRect;

  if (layerInvalidRect.IsEqualInterior(GetMaxSizedIntRect())) {
    // Invalidate our entire area.
    InvalidateSelf(nullptr, nullptr);
    return;
  }

  nsRect frameInvalidRect = SourceRectToDest(layerInvalidRect);
  InvalidateSelf(&layerInvalidRect, &frameInvalidRect);
}

void nsImageFrame::InvalidateSelf(const nsIntRect* aLayerInvalidRect,
                                  const nsRect* aFrameInvalidRect) {
  // Check if WebRender has interacted with this frame. If it has
  // we need to let it know that things have changed.
  const auto type = DisplayItemType::TYPE_IMAGE;
  const auto producerId =
      mImage ? mImage->GetProducerId() : kContainerProducerID_Invalid;
  if (WebRenderUserData::ProcessInvalidateForImage(this, type, producerId)) {
    return;
  }

  InvalidateLayer(type, aLayerInvalidRect, aFrameInvalidRect);

  if (!mFirstFrameComplete) {
    InvalidateLayer(DisplayItemType::TYPE_ALT_FEEDBACK, aLayerInvalidRect,
                    aFrameInvalidRect);
  }
}

void nsImageFrame::OnLoadComplete(imgIRequest* aRequest, nsresult aStatus) {
  NotifyNewCurrentRequest(aRequest, aStatus);
}

void nsImageFrame::ResponsiveContentDensityChanged() {
  if (!GotInitialReflow()) {
    return;
  }

  if (!mImage) {
    return;
  }

  if (!UpdateIntrinsicSize() && !UpdateIntrinsicRatio()) {
    return;
  }

  PresShell()->FrameNeedsReflow(this, IntrinsicDirty::StyleChange,
                                NS_FRAME_IS_DIRTY);
}

void nsImageFrame::NotifyNewCurrentRequest(imgIRequest* aRequest,
                                           nsresult aStatus) {
  nsCOMPtr<imgIContainer> image;
  aRequest->GetImage(getter_AddRefs(image));
  NS_ASSERTION(image || NS_FAILED(aStatus),
               "Successful load with no container?");
  UpdateImage(aRequest, image);
}

void nsImageFrame::MaybeDecodeForPredictedSize() {
  // Check that we're ready to decode.
  if (!mImage) {
    return;  // Nothing to do yet.
  }

  if (mComputedSize.IsEmpty()) {
    return;  // We won't draw anything, so no point in decoding.
  }

  if (GetVisibility() != Visibility::ApproximatelyVisible) {
    return;  // We're not visible, so don't decode.
  }

  // OK, we're ready to decode. Compute the scale to the screen...
  mozilla::PresShell* presShell = PresContext()->PresShell();
  LayoutDeviceToScreenScale2D resolutionToScreen(
      presShell->GetCumulativeResolution() *
      nsLayoutUtils::GetTransformToAncestorScaleExcludingAnimated(this));

  // ...and this frame's content box...
  const nsPoint offset =
      GetOffsetToCrossDoc(nsLayoutUtils::GetReferenceFrame(this));
  const nsRect frameContentBox = GetInnerArea() + offset;

  // ...and our predicted dest rect...
  const int32_t factor = PresContext()->AppUnitsPerDevPixel();
  const LayoutDeviceRect destRect = LayoutDeviceRect::FromAppUnits(
      PredictedDestRect(frameContentBox), factor);

  // ...and use them to compute our predicted size in screen pixels.
  const ScreenSize predictedScreenSize = destRect.Size() * resolutionToScreen;
  const ScreenIntSize predictedScreenIntSize =
      RoundedToInt(predictedScreenSize);
  if (predictedScreenIntSize.IsEmpty()) {
    return;
  }

  // Determine the optimal image size to use.
  uint32_t flags = imgIContainer::FLAG_HIGH_QUALITY_SCALING |
                   imgIContainer::FLAG_ASYNC_NOTIFY;
  SamplingFilter samplingFilter =
      nsLayoutUtils::GetSamplingFilterForFrame(this);
  gfxSize gfxPredictedScreenSize =
      gfxSize(predictedScreenIntSize.width, predictedScreenIntSize.height);
  nsIntSize predictedImageSize = mImage->OptimalImageSizeForDest(
      gfxPredictedScreenSize, imgIContainer::FRAME_CURRENT, samplingFilter,
      flags);

  // Request a decode.
  mImage->RequestDecodeForSize(predictedImageSize, flags);
}

nsRect nsImageFrame::PredictedDestRect(const nsRect& aFrameContentBox) {
  // Note: To get the "dest rect", we have to provide the "constraint rect"
  // (which is the content-box, with the effects of fragmentation undone).
  nsRect constraintRect(aFrameContentBox.TopLeft(), mComputedSize);
  constraintRect.y -= GetContinuationOffset();

  return nsLayoutUtils::ComputeObjectDestRect(constraintRect, mIntrinsicSize,
                                              mIntrinsicRatio, StylePosition());
}

void nsImageFrame::EnsureIntrinsicSizeAndRatio() {
  if (StyleDisplay()->IsContainSize()) {
    // If we have 'contain:size', then our intrinsic size and ratio are 0,0
    // regardless of what our underlying image may think.
    mIntrinsicSize = IntrinsicSize(0, 0);
    mIntrinsicRatio = AspectRatio();
    return;
  }

  // If mIntrinsicSize.width and height are 0, then we need to update from the
  // image container.
  if (mIntrinsicSize != IntrinsicSize(0, 0)) {
    return;
  }

  UpdateIntrinsicSize();
  UpdateIntrinsicRatio();
}

nsIFrame::SizeComputationResult nsImageFrame::ComputeSize(
    gfxContext* aRenderingContext, WritingMode aWM, const LogicalSize& aCBSize,
    nscoord aAvailableISize, const LogicalSize& aMargin,
    const LogicalSize& aBorderPadding, const StyleSizeOverrides& aSizeOverrides,
    ComputeSizeFlags aFlags) {
  EnsureIntrinsicSizeAndRatio();
  return {ComputeSizeWithIntrinsicDimensions(
              aRenderingContext, aWM, mIntrinsicSize, GetAspectRatio(), aCBSize,
              aMargin, aBorderPadding, aSizeOverrides, aFlags),
          AspectRatioUsage::None};
}

// XXXdholbert This function's clients should probably just be calling
// GetContentRectRelativeToSelf() directly.
nsRect nsImageFrame::GetInnerArea() const {
  return GetContentRectRelativeToSelf();
}

Element* nsImageFrame::GetMapElement() const {
  nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(mContent);
  return imageLoader ? static_cast<nsImageLoadingContent*>(imageLoader.get())
                           ->FindImageMap()
                     : nullptr;
}

// get the offset into the content area of the image where aImg starts if it is
// a continuation.
nscoord nsImageFrame::GetContinuationOffset() const {
  nscoord offset = 0;
  for (nsIFrame* f = GetPrevInFlow(); f; f = f->GetPrevInFlow()) {
    offset += f->GetContentRect().height;
  }
  NS_ASSERTION(offset >= 0, "bogus GetContentRect");
  return offset;
}

nscoord nsImageFrame::GetMinISize(gfxContext* aRenderingContext) {
  // XXX The caller doesn't account for constraints of the block-size,
  // min-block-size, and max-block-size properties.
  DebugOnly<nscoord> result;
  DISPLAY_MIN_INLINE_SIZE(this, result);
  EnsureIntrinsicSizeAndRatio();
  const auto& iSize = GetWritingMode().IsVertical() ? mIntrinsicSize.height
                                                    : mIntrinsicSize.width;
  return iSize.valueOr(0);
}

nscoord nsImageFrame::GetPrefISize(gfxContext* aRenderingContext) {
  // XXX The caller doesn't account for constraints of the block-size,
  // min-block-size, and max-block-size properties.
  DebugOnly<nscoord> result;
  DISPLAY_PREF_INLINE_SIZE(this, result);
  EnsureIntrinsicSizeAndRatio();
  const auto& iSize = GetWritingMode().IsVertical() ? mIntrinsicSize.height
                                                    : mIntrinsicSize.width;
  // convert from normal twips to scaled twips (printing...)
  return iSize.valueOr(0);
}

void nsImageFrame::Reflow(nsPresContext* aPresContext, ReflowOutput& aMetrics,
                          const ReflowInput& aReflowInput,
                          nsReflowStatus& aStatus) {
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsImageFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aMetrics, aStatus);
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");
  NS_FRAME_TRACE(
      NS_FRAME_TRACE_CALLS,
      ("enter nsImageFrame::Reflow: availSize=%d,%d",
       aReflowInput.AvailableWidth(), aReflowInput.AvailableHeight()));

  MOZ_ASSERT(mState & NS_FRAME_IN_REFLOW, "frame is not in reflow");

  // see if we have a frozen size (i.e. a fixed width and height)
  if (!SizeDependsOnIntrinsicSize(aReflowInput)) {
    AddStateBits(IMAGE_SIZECONSTRAINED);
  } else {
    RemoveStateBits(IMAGE_SIZECONSTRAINED);
  }

  mComputedSize =
      nsSize(aReflowInput.ComputedWidth(), aReflowInput.ComputedHeight());

  aMetrics.Width() = mComputedSize.width;
  aMetrics.Height() = mComputedSize.height;

  // add borders and padding
  aMetrics.Width() += aReflowInput.ComputedPhysicalBorderPadding().LeftRight();
  aMetrics.Height() += aReflowInput.ComputedPhysicalBorderPadding().TopBottom();

  if (GetPrevInFlow()) {
    aMetrics.Width() = GetPrevInFlow()->GetSize().width;
    nscoord y = GetContinuationOffset();
    aMetrics.Height() -= y + aReflowInput.ComputedPhysicalBorderPadding().top;
    aMetrics.Height() = std::max(0, aMetrics.Height());
  }

  // we have to split images if we are:
  //  in Paginated mode, we need to have a constrained height, and have a height
  //  larger than our available height
  uint32_t loadStatus = imgIRequest::STATUS_NONE;
  if (nsCOMPtr<imgIRequest> currentRequest = GetCurrentRequest()) {
    currentRequest->GetImageStatus(&loadStatus);
  }

  if (aPresContext->IsPaginated() &&
      ((loadStatus & imgIRequest::STATUS_SIZE_AVAILABLE) ||
       (mState & IMAGE_SIZECONSTRAINED)) &&
      NS_UNCONSTRAINEDSIZE != aReflowInput.AvailableHeight() &&
      aMetrics.Height() > aReflowInput.AvailableHeight()) {
    // our desired height was greater than 0, so to avoid infinite
    // splitting, use 1 pixel as the min
    aMetrics.Height() = std::max(nsPresContext::CSSPixelsToAppUnits(1),
                                 aReflowInput.AvailableHeight());
    aStatus.SetIncomplete();
  }

  aMetrics.SetOverflowAreasToDesiredBounds();
  bool imageOK =
      mKind != Kind::ImageElement || ImageOk(mContent->AsElement()->State());

  // Determine if the size is available
  bool haveSize = false;
  if (loadStatus & imgIRequest::STATUS_SIZE_AVAILABLE) {
    haveSize = true;
  }

  if (!imageOK || !haveSize) {
    nsRect altFeedbackSize(
        0, 0,
        nsPresContext::CSSPixelsToAppUnits(
            ICON_SIZE + 2 * (ICON_PADDING + ALT_BORDER_WIDTH)),
        nsPresContext::CSSPixelsToAppUnits(
            ICON_SIZE + 2 * (ICON_PADDING + ALT_BORDER_WIDTH)));
    // We include the altFeedbackSize in our ink overflow, but not in our
    // scrollable overflow, since it doesn't really need to be scrolled to
    // outside the image.
    nsRect& inkOverflow = aMetrics.InkOverflow();
    inkOverflow.UnionRect(inkOverflow, altFeedbackSize);
  } else if (PresShell()->IsActive()) {
    // We've just reflowed and we should have an accurate size, so we're ready
    // to request a decode.
    MaybeDecodeForPredictedSize();
  }
  FinishAndStoreOverflow(&aMetrics, aReflowInput.mStyleDisplay);

  if (HasAnyStateBits(NS_FRAME_FIRST_REFLOW) && !mReflowCallbackPosted) {
    mReflowCallbackPosted = true;
    PresShell()->PostReflowCallback(this);
  }

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS, ("exit nsImageFrame::Reflow: size=%d,%d",
                                        aMetrics.Width(), aMetrics.Height()));
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aMetrics);
}

bool nsImageFrame::ReflowFinished() {
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

void nsImageFrame::ReflowCallbackCanceled() { mReflowCallbackPosted = false; }

// Computes the width of the specified string. aMaxWidth specifies the maximum
// width available. Once this limit is reached no more characters are measured.
// The number of characters that fit within the maximum width are returned in
// aMaxFit. NOTE: it is assumed that the fontmetrics have already been selected
// into the rendering context before this is called (for performance). MMP
nscoord nsImageFrame::MeasureString(const char16_t* aString, int32_t aLength,
                                    nscoord aMaxWidth, uint32_t& aMaxFit,
                                    gfxContext& aContext,
                                    nsFontMetrics& aFontMetrics) {
  nscoord totalWidth = 0;
  aFontMetrics.SetTextRunRTL(false);
  nscoord spaceWidth = aFontMetrics.SpaceWidth();

  aMaxFit = 0;
  while (aLength > 0) {
    // Find the next place we can line break
    uint32_t len = aLength;
    bool trailingSpace = false;
    for (int32_t i = 0; i < aLength; i++) {
      if (dom::IsSpaceCharacter(aString[i]) && (i > 0)) {
        len = i;  // don't include the space when measuring
        trailingSpace = true;
        break;
      }
    }

    // Measure this chunk of text, and see if it fits
    nscoord width = nsLayoutUtils::AppUnitWidthOfStringBidi(
        aString, len, this, aFontMetrics, aContext);
    bool fits = (totalWidth + width) <= aMaxWidth;

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
void nsImageFrame::DisplayAltText(nsPresContext* aPresContext,
                                  gfxContext& aRenderingContext,
                                  const nsString& aAltText,
                                  const nsRect& aRect) {
  // Set font and color
  aRenderingContext.SetColor(
      sRGBColor::FromABGR(StyleText()->mColor.ToColor()));
  RefPtr<nsFontMetrics> fm =
      nsLayoutUtils::GetInflatedFontMetricsForFrame(this);

  // Format the text to display within the formatting rect

  nscoord maxAscent = fm->MaxAscent();
  nscoord maxDescent = fm->MaxDescent();
  nscoord lineHeight = fm->MaxHeight();  // line-relative, so an x-coordinate
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
    uint32_t maxFit;  // number of characters that fit
    nscoord strWidth =
        MeasureString(str, strLen, iSize, maxFit, aRenderingContext, *fm);

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

      rv = nsBidiPresUtils::RenderText(
          str, maxFit, dir, aPresContext, aRenderingContext,
          aRenderingContext.GetDrawTarget(), *fm, x, y);
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
      : nsStyleBorder(*aPresContext->Document()) {
    for (const auto side : mozilla::AllPhysicalSides()) {
      BorderColorFor(side) = StyleColor::Black();
      mBorder.Side(side) = aBorderWidth;
      // Note: use SetBorderStyle here because we want to affect
      // mComputedBorder
      SetBorderStyle(side, StyleBorderStyle::Inset);
    }
  }
};

class nsDisplayAltFeedback final : public nsPaintedDisplayItem {
 public:
  nsDisplayAltFeedback(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
      : nsPaintedDisplayItem(aBuilder, aFrame) {}

  nsDisplayItemGeometry* AllocateGeometry(
      nsDisplayListBuilder* aBuilder) final {
    return new nsDisplayItemGenericImageGeometry(this, aBuilder);
  }

  void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                 const nsDisplayItemGeometry* aGeometry,
                                 nsRegion* aInvalidRegion) const final {
    auto geometry =
        static_cast<const nsDisplayItemGenericImageGeometry*>(aGeometry);

    if (aBuilder->ShouldSyncDecodeImages() &&
        geometry->ShouldInvalidateToSyncDecodeImages()) {
      bool snap;
      aInvalidRegion->Or(*aInvalidRegion, GetBounds(aBuilder, &snap));
    }

    nsDisplayItem::ComputeInvalidationRegion(aBuilder, aGeometry,
                                             aInvalidRegion);
  }

  nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) const final {
    *aSnap = false;
    return mFrame->InkOverflowRectRelativeToSelf() + ToReferenceFrame();
  }

  void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) final {
    // Always sync decode, because these icons are UI, and since they're not
    // discardable we'll pay the price of sync decoding at most once.
    uint32_t flags = imgIContainer::FLAG_SYNC_DECODE;

    nsImageFrame* f = static_cast<nsImageFrame*>(mFrame);
    ImgDrawResult result =
        f->DisplayAltFeedback(*aCtx, GetPaintRect(), ToReferenceFrame(), flags);

    nsDisplayItemGenericImageGeometry::UpdateDrawResult(this, result);
  }

  bool CreateWebRenderCommands(
      mozilla::wr::DisplayListBuilder& aBuilder,
      mozilla::wr::IpcResourceUpdateQueue& aResources,
      const StackingContextHelper& aSc,
      mozilla::layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) final {
    uint32_t flags = imgIContainer::FLAG_ASYNC_NOTIFY;
    nsImageFrame* f = static_cast<nsImageFrame*>(mFrame);
    ImgDrawResult result = f->DisplayAltFeedbackWithoutLayer(
        this, aBuilder, aResources, aSc, aManager, aDisplayListBuilder,
        ToReferenceFrame(), flags);

    return result == ImgDrawResult::SUCCESS;
  }

  NS_DISPLAY_DECL_NAME("AltFeedback", TYPE_ALT_FEEDBACK)
};

ImgDrawResult nsImageFrame::DisplayAltFeedback(gfxContext& aRenderingContext,
                                               const nsRect& aDirtyRect,
                                               nsPoint aPt, uint32_t aFlags) {
  // We should definitely have a gIconLoad here.
  MOZ_ASSERT(gIconLoad, "How did we succeed in Init then?");

  // Whether we draw the broken or loading icon.
  bool isLoading =
      mKind != Kind::ImageElement || ImageOk(mContent->AsElement()->State());

  // Calculate the inner area
  nsRect inner = GetInnerArea() + aPt;

  // Display a recessed one pixel border
  nscoord borderEdgeWidth =
      nsPresContext::CSSPixelsToAppUnits(ALT_BORDER_WIDTH);

  // if inner area is empty, then make it big enough for at least the icon
  if (inner.IsEmpty()) {
    inner.SizeTo(2 * (nsPresContext::CSSPixelsToAppUnits(
                         ICON_SIZE + ICON_PADDING + ALT_BORDER_WIDTH)),
                 2 * (nsPresContext::CSSPixelsToAppUnits(
                         ICON_SIZE + ICON_PADDING + ALT_BORDER_WIDTH)));
  }

  // Make sure we have enough room to actually render the border within
  // our frame bounds
  if ((inner.width < 2 * borderEdgeWidth) ||
      (inner.height < 2 * borderEdgeWidth)) {
    return ImgDrawResult::SUCCESS;
  }

  // Paint the border
  if (!isLoading || gIconLoad->mPrefShowLoadingPlaceholder) {
    nsRecessedBorder recessedBorder(borderEdgeWidth, PresContext());

    // Assert that we're not drawing a border-image here; if we were, we
    // couldn't ignore the ImgDrawResult that PaintBorderWithStyleBorder
    // returns.
    MOZ_ASSERT(recessedBorder.mBorderImageSource.IsNone());

    Unused << nsCSSRendering::PaintBorderWithStyleBorder(
        PresContext(), aRenderingContext, this, inner, inner, recessedBorder,
        mComputedStyle, PaintBorderFlags::SyncDecodeImages);
  }

  // Adjust the inner rect to account for the one pixel recessed border,
  // and a six pixel padding on each edge
  inner.Deflate(
      nsPresContext::CSSPixelsToAppUnits(ICON_PADDING + ALT_BORDER_WIDTH),
      nsPresContext::CSSPixelsToAppUnits(ICON_PADDING + ALT_BORDER_WIDTH));
  if (inner.IsEmpty()) {
    return ImgDrawResult::SUCCESS;
  }

  DrawTarget* drawTarget = aRenderingContext.GetDrawTarget();

  // Clip so we don't render outside the inner rect
  aRenderingContext.Save();
  aRenderingContext.Clip(NSRectToSnappedRect(
      inner, PresContext()->AppUnitsPerDevPixel(), *drawTarget));

  ImgDrawResult result = ImgDrawResult::NOT_READY;

  // Check if we should display image placeholders
  if (!ShouldShowBrokenImageIcon() || !gIconLoad->mPrefShowPlaceholders ||
      (isLoading && !gIconLoad->mPrefShowLoadingPlaceholder)) {
    result = ImgDrawResult::SUCCESS;
  } else {
    nscoord size = nsPresContext::CSSPixelsToAppUnits(ICON_SIZE);

    imgIRequest* request = isLoading ? nsImageFrame::gIconLoad->mLoadingImage
                                     : nsImageFrame::gIconLoad->mBrokenImage;

    // If we weren't previously displaying an icon, register ourselves
    // as an observer for load and animation updates and flag that we're
    // doing so now.
    if (request && !mDisplayingIcon) {
      gIconLoad->AddIconObserver(this);
      mDisplayingIcon = true;
    }

    WritingMode wm = GetWritingMode();
    bool flushRight = wm.IsPhysicalRTL();

    // If the icon in question is loaded, draw it.
    uint32_t imageStatus = 0;
    if (request) request->GetImageStatus(&imageStatus);
    if (imageStatus & imgIRequest::STATUS_LOAD_COMPLETE &&
        !(imageStatus & imgIRequest::STATUS_ERROR)) {
      nsCOMPtr<imgIContainer> imgCon;
      request->GetImage(getter_AddRefs(imgCon));
      MOZ_ASSERT(imgCon, "Load complete, but no image container?");
      nsRect dest(flushRight ? inner.XMost() - size : inner.x, inner.y, size,
                  size);
      result = nsLayoutUtils::DrawSingleImage(
          aRenderingContext, PresContext(), imgCon, /* aResolution = */ 1.0f,
          nsLayoutUtils::GetSamplingFilterForFrame(this), dest, aDirtyRect,
          /* no SVGImageContext */ Nothing(), aFlags);
    }

    // If we could not draw the icon, just draw some graffiti in the mean time.
    if (result == ImgDrawResult::NOT_READY) {
      ColorPattern color(ToDeviceColor(sRGBColor(1.f, 0.f, 0.f, 1.f)));

      nscoord iconXPos = flushRight ? inner.XMost() - size : inner.x;

      // stroked rect:
      nsRect rect(iconXPos, inner.y, size, size);
      Rect devPxRect = ToRect(nsLayoutUtils::RectToGfxRect(
          rect, PresContext()->AppUnitsPerDevPixel()));
      drawTarget->StrokeRect(devPxRect, color);

      // filled circle in bottom right quadrant of stroked rect:
      nscoord twoPX = nsPresContext::CSSPixelsToAppUnits(2);
      rect = nsRect(iconXPos + size / 2, inner.y + size / 2, size / 2 - twoPX,
                    size / 2 - twoPX);
      devPxRect = ToRect(nsLayoutUtils::RectToGfxRect(
          rect, PresContext()->AppUnitsPerDevPixel()));
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
    nsAutoString altText;
    nsCSSFrameConstructor::GetAlternateTextFor(
        mContent->AsElement(), mContent->NodeInfo()->NameAtom(), altText);
    DisplayAltText(PresContext(), aRenderingContext, altText, inner);
  }

  aRenderingContext.Restore();

  return result;
}

ImgDrawResult nsImageFrame::DisplayAltFeedbackWithoutLayer(
    nsDisplayItem* aItem, mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder, nsPoint aPt, uint32_t aFlags) {
  // We should definitely have a gIconLoad here.
  MOZ_ASSERT(gIconLoad, "How did we succeed in Init then?");

  // Whether we draw the broken or loading icon.
  bool isLoading =
      mKind != Kind::ImageElement || ImageOk(mContent->AsElement()->State());

  // Calculate the inner area
  nsRect inner = GetInnerArea() + aPt;

  // Display a recessed one pixel border
  nscoord borderEdgeWidth =
      nsPresContext::CSSPixelsToAppUnits(ALT_BORDER_WIDTH);

  // if inner area is empty, then make it big enough for at least the icon
  if (inner.IsEmpty()) {
    inner.SizeTo(2 * (nsPresContext::CSSPixelsToAppUnits(
                         ICON_SIZE + ICON_PADDING + ALT_BORDER_WIDTH)),
                 2 * (nsPresContext::CSSPixelsToAppUnits(
                         ICON_SIZE + ICON_PADDING + ALT_BORDER_WIDTH)));
  }

  // Make sure we have enough room to actually render the border within
  // our frame bounds
  if ((inner.width < 2 * borderEdgeWidth) ||
      (inner.height < 2 * borderEdgeWidth)) {
    return ImgDrawResult::SUCCESS;
  }

  // If the TextDrawTarget requires fallback we need to rollback everything we
  // may have added to the display list, but we don't find that out until the
  // end.
  bool textDrawResult = true;
  class AutoSaveRestore {
   public:
    explicit AutoSaveRestore(mozilla::wr::DisplayListBuilder& aBuilder,
                             bool& aTextDrawResult)
        : mBuilder(aBuilder), mTextDrawResult(aTextDrawResult) {
      mBuilder.Save();
    }
    ~AutoSaveRestore() {
      // If we have to use fallback for the text restore the builder and remove
      // anything else we added to the display list, we need to use fallback.
      if (mTextDrawResult) {
        mBuilder.ClearSave();
      } else {
        mBuilder.Restore();
      }
    }

   private:
    mozilla::wr::DisplayListBuilder& mBuilder;
    bool& mTextDrawResult;
  };

  AutoSaveRestore autoSaveRestore(aBuilder, textDrawResult);

  // Paint the border
  if (!isLoading || gIconLoad->mPrefShowLoadingPlaceholder) {
    nsRecessedBorder recessedBorder(borderEdgeWidth, PresContext());
    // Assert that we're not drawing a border-image here; if we were, we
    // couldn't ignore the ImgDrawResult that PaintBorderWithStyleBorder
    // returns.
    MOZ_ASSERT(recessedBorder.mBorderImageSource.IsNone());

    nsRect rect = nsRect(aPt, GetSize());
    Unused << nsCSSRendering::CreateWebRenderCommandsForBorderWithStyleBorder(
        aItem, this, rect, aBuilder, aResources, aSc, aManager,
        aDisplayListBuilder, recessedBorder);
  }

  // Adjust the inner rect to account for the one pixel recessed border,
  // and a six pixel padding on each edge
  inner.Deflate(
      nsPresContext::CSSPixelsToAppUnits(ICON_PADDING + ALT_BORDER_WIDTH),
      nsPresContext::CSSPixelsToAppUnits(ICON_PADDING + ALT_BORDER_WIDTH));
  if (inner.IsEmpty()) {
    return ImgDrawResult::SUCCESS;
  }

  // Clip to this rect so we don't render outside the inner rect
  LayoutDeviceRect bounds = LayoutDeviceRect::FromAppUnits(
      inner, PresContext()->AppUnitsPerDevPixel());
  auto wrBounds = wr::ToLayoutRect(bounds);

  // Check if we should display image placeholders
  if (ShouldShowBrokenImageIcon() && gIconLoad->mPrefShowPlaceholders &&
      (!isLoading || gIconLoad->mPrefShowLoadingPlaceholder)) {
    ImgDrawResult result = ImgDrawResult::NOT_READY;
    nscoord size = nsPresContext::CSSPixelsToAppUnits(ICON_SIZE);
    imgIRequest* request = isLoading ? nsImageFrame::gIconLoad->mLoadingImage
                                     : nsImageFrame::gIconLoad->mBrokenImage;

    // If we weren't previously displaying an icon, register ourselves
    // as an observer for load and animation updates and flag that we're
    // doing so now.
    if (request && !mDisplayingIcon) {
      gIconLoad->AddIconObserver(this);
      mDisplayingIcon = true;
    }

    WritingMode wm = GetWritingMode();
    const bool flushRight = wm.IsPhysicalRTL();

    // If the icon in question is loaded, draw it.
    uint32_t imageStatus = 0;
    if (request) request->GetImageStatus(&imageStatus);
    if (imageStatus & imgIRequest::STATUS_LOAD_COMPLETE &&
        !(imageStatus & imgIRequest::STATUS_ERROR)) {
      nsCOMPtr<imgIContainer> imgCon;
      request->GetImage(getter_AddRefs(imgCon));
      MOZ_ASSERT(imgCon, "Load complete, but no image container?");

      nsRect dest(flushRight ? inner.XMost() - size : inner.x, inner.y, size,
                  size);

      const int32_t factor = PresContext()->AppUnitsPerDevPixel();
      LayoutDeviceRect destRect(LayoutDeviceRect::FromAppUnits(dest, factor));

      Maybe<SVGImageContext> svgContext;
      IntSize decodeSize =
          nsLayoutUtils::ComputeImageContainerDrawingParameters(
              imgCon, this, destRect, aSc, aFlags, svgContext);
      RefPtr<ImageContainer> container;
      result = imgCon->GetImageContainerAtSize(aManager->LayerManager(),
                                               decodeSize, svgContext, aFlags,
                                               getter_AddRefs(container));
      if (container) {
        bool wrResult = aManager->CommandBuilder().PushImage(
            aItem, container, aBuilder, aResources, aSc, destRect, bounds);
        result &= wrResult ? ImgDrawResult::SUCCESS : ImgDrawResult::NOT_READY;
      } else {
        // We don't use &= here because we want the result to be NOT_READY so
        // the next block executes.
        result = ImgDrawResult::NOT_READY;
      }
    }

    // If we could not draw the icon, just draw some graffiti in the mean time.
    if (result == ImgDrawResult::NOT_READY) {
      auto color = wr::ColorF{1.0f, 0.0f, 0.0f, 1.0f};
      bool isBackfaceVisible = !aItem->BackfaceIsHidden();

      nscoord iconXPos = flushRight ? inner.XMost() - size : inner.x;

      // stroked rect:
      nsRect rect(iconXPos, inner.y, size, size);
      auto devPxRect = LayoutDeviceRect::FromAppUnits(
          rect, PresContext()->AppUnitsPerDevPixel());
      auto dest = wr::ToLayoutRect(devPxRect);

      auto borderWidths = wr::ToBorderWidths(1.0, 1.0, 1.0, 1.0);
      wr::BorderSide side = {color, wr::BorderStyle::Solid};
      wr::BorderSide sides[4] = {side, side, side, side};
      Range<const wr::BorderSide> sidesRange(sides, 4);
      aBuilder.PushBorder(dest, wrBounds, isBackfaceVisible, borderWidths,
                          sidesRange, wr::EmptyBorderRadius());

      // filled circle in bottom right quadrant of stroked rect:
      nscoord twoPX = nsPresContext::CSSPixelsToAppUnits(2);
      rect = nsRect(iconXPos + size / 2, inner.y + size / 2, size / 2 - twoPX,
                    size / 2 - twoPX);
      devPxRect = LayoutDeviceRect::FromAppUnits(
          rect, PresContext()->AppUnitsPerDevPixel());
      dest = wr::ToLayoutRect(devPxRect);

      aBuilder.PushRoundedRect(dest, wrBounds, isBackfaceVisible, color);
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

  // Draw text
  if (!inner.IsEmpty()) {
    RefPtr<TextDrawTarget> textDrawer =
        new TextDrawTarget(aBuilder, aResources, aSc, aManager, aItem, inner,
                           /* aCallerDoesSaveRestore = */ true);
    RefPtr<gfxContext> captureCtx = gfxContext::CreateOrNull(textDrawer);

    nsAutoString altText;
    nsCSSFrameConstructor::GetAlternateTextFor(
        mContent->AsElement(), mContent->NodeInfo()->NameAtom(), altText);
    DisplayAltText(PresContext(), *captureCtx.get(), altText, inner);

    textDrawer->TerminateShadows();
    textDrawResult = !textDrawer->CheckHasUnsupportedFeatures();
  }

  // Purposely ignore local DrawResult because we handled it not being success
  // already.
  return textDrawResult ? ImgDrawResult::SUCCESS : ImgDrawResult::NOT_READY;
}

#ifdef DEBUG
static void PaintDebugImageMap(nsIFrame* aFrame, DrawTarget* aDrawTarget,
                               const nsRect& aDirtyRect, nsPoint aPt) {
  nsImageFrame* f = static_cast<nsImageFrame*>(aFrame);
  nsRect inner = f->GetInnerArea() + aPt;
  gfxPoint devPixelOffset = nsLayoutUtils::PointToGfxPoint(
      inner.TopLeft(), aFrame->PresContext()->AppUnitsPerDevPixel());
  AutoRestoreTransform autoRestoreTransform(aDrawTarget);
  aDrawTarget->SetTransform(
      aDrawTarget->GetTransform().PreTranslate(ToPoint(devPixelOffset)));
  f->GetImageMap()->Draw(aFrame, *aDrawTarget,
                         ColorPattern(ToDeviceColor(sRGBColor::OpaqueBlack())));
}
#endif

// We want to sync-decode in this case, as otherwise we either need to flash
// white while waiting to decode the new image, or paint the old image with a
// different aspect-ratio, which would be bad as it'd be stretched.
//
// See bug 1589955.
static bool OldImageHasDifferentRatio(const nsImageFrame& aFrame,
                                      imgIContainer& aImage,
                                      imgIContainer* aPrevImage) {
  if (!aPrevImage || aPrevImage == &aImage) {
    return false;
  }

  // If we don't depend on our intrinsic image size / ratio, we're good.
  //
  // FIXME(emilio): There's the case of the old image being painted
  // intrinsically, and src and styles changing at the same time... Maybe we
  // should keep track of the old GetPaintRect()'s ratio and the image's ratio,
  // instead of checking this bit?
  if (aFrame.HasAnyStateBits(IMAGE_SIZECONSTRAINED)) {
    return false;
  }

  auto currentRatio = aFrame.GetIntrinsicRatio();
  // If we have an image, we need to have a current request.
  // Same if we had an image.
  const bool hasRequest = true;
#ifdef DEBUG
  auto currentRatioRecomputed =
      ComputeIntrinsicRatio(&aImage, hasRequest, aFrame);
  // If the image encounters an error after decoding the size (and we run
  // UpdateIntrinsicRatio) then the image will return the empty AspectRatio and
  // the aspect ratio we compute here will be different from what was computed
  // and stored before the image went into error state. It would be better to
  // check that the image has an error here but we need an imgIRequest for that,
  // not an imgIContainer. In lieu of that we check that
  // aImage.GetIntrinsicRatio() returns Nothing() as it does when the image is
  // in the error state and that the recomputed ratio is the zero ratio.
  MOZ_ASSERT(
      (!currentRatioRecomputed && aImage.GetIntrinsicRatio() == Nothing()) ||
          currentRatio == currentRatioRecomputed,
      "aspect-ratio got out of sync during paint? How?");
#endif
  auto oldRatio = ComputeIntrinsicRatio(aPrevImage, hasRequest, aFrame);
  return oldRatio != currentRatio;
}

void nsDisplayImage::Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) {
  MOZ_ASSERT(mImage);
  auto* frame = static_cast<nsImageFrame*>(mFrame);

  const bool oldImageIsDifferent =
      OldImageHasDifferentRatio(*frame, *mImage, mPrevImage);

  uint32_t flags = imgIContainer::FLAG_NONE;
  if (aBuilder->ShouldSyncDecodeImages() || oldImageIsDifferent) {
    flags |= imgIContainer::FLAG_SYNC_DECODE;
  }
  if (aBuilder->UseHighQualityScaling()) {
    flags |= imgIContainer::FLAG_HIGH_QUALITY_SCALING;
  }

  ImgDrawResult result = frame->PaintImage(*aCtx, ToReferenceFrame(),
                                           GetPaintRect(), mImage, flags);

  if (result == ImgDrawResult::NOT_READY ||
      result == ImgDrawResult::INCOMPLETE ||
      result == ImgDrawResult::TEMPORARY_ERROR) {
    // If the current image failed to paint because it's still loading or
    // decoding, try painting the previous image.
    if (mPrevImage) {
      result = frame->PaintImage(*aCtx, ToReferenceFrame(), GetPaintRect(),
                                 mPrevImage, flags);
    }
  }

  nsDisplayItemGenericImageGeometry::UpdateDrawResult(this, result);
}

nsDisplayItemGeometry* nsDisplayImage::AllocateGeometry(
    nsDisplayListBuilder* aBuilder) {
  return new nsDisplayItemGenericImageGeometry(this, aBuilder);
}

void nsDisplayImage::ComputeInvalidationRegion(
    nsDisplayListBuilder* aBuilder, const nsDisplayItemGeometry* aGeometry,
    nsRegion* aInvalidRegion) const {
  auto geometry =
      static_cast<const nsDisplayItemGenericImageGeometry*>(aGeometry);

  if (aBuilder->ShouldSyncDecodeImages() &&
      geometry->ShouldInvalidateToSyncDecodeImages()) {
    bool snap;
    aInvalidRegion->Or(*aInvalidRegion, GetBounds(aBuilder, &snap));
  }

  nsDisplayImageContainer::ComputeInvalidationRegion(aBuilder, aGeometry,
                                                     aInvalidRegion);
}

already_AddRefed<imgIContainer> nsDisplayImage::GetImage() {
  nsCOMPtr<imgIContainer> image = mImage;
  return image.forget();
}

nsRect nsDisplayImage::GetDestRect() const {
  bool snap = true;
  const nsRect frameContentBox = GetBounds(&snap);

  nsImageFrame* imageFrame = static_cast<nsImageFrame*>(mFrame);
  return imageFrame->PredictedDestRect(frameContentBox);
}

LayerState nsDisplayImage::GetLayerState(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aParameters) {
  if (!nsDisplayItem::ForceActiveLayers()) {
    bool animated = false;
    if (!StaticPrefs::layout_animated_image_layers_enabled() ||
        mImage->GetType() != imgIContainer::TYPE_RASTER ||
        NS_FAILED(mImage->GetAnimated(&animated)) || !animated) {
      if (!aManager->IsCompositingCheap() ||
          !nsLayoutUtils::GPUImageScalingEnabled()) {
        return LayerState::LAYER_NONE;
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
        return LayerState::LAYER_NONE;
      }

      // If the target size is pretty small, no point in using a layer.
      if (destLayerRect.width * destLayerRect.height < 64 * 64) {
        return LayerState::LAYER_NONE;
      }
    }
  }

  if (!CanOptimizeToImageLayer(aManager, aBuilder)) {
    return LayerState::LAYER_NONE;
  }

  // Image layer doesn't support draw focus ring for image map.
  nsImageFrame* f = static_cast<nsImageFrame*>(mFrame);
  if (f->HasImageMap()) {
    return LayerState::LAYER_NONE;
  }

  return LayerState::LAYER_ACTIVE;
}

nsRegion nsDisplayImage::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                         bool* aSnap) const {
  *aSnap = false;
  if (mImage && mImage->WillDrawOpaqueNow()) {
    const nsRect frameContentBox = GetBounds(aSnap);
    return GetDestRect().Intersect(frameContentBox);
  }
  return nsRegion();
}

already_AddRefed<Layer> nsDisplayImage::BuildLayer(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aParameters) {
  uint32_t flags = imgIContainer::FLAG_ASYNC_NOTIFY;
  if (aBuilder->ShouldSyncDecodeImages()) {
    flags |= imgIContainer::FLAG_SYNC_DECODE;
  }

  RefPtr<ImageContainer> container = mImage->GetImageContainer(aManager, flags);
  if (!container || !container->HasCurrentImage()) {
    return nullptr;
  }

  RefPtr<ImageLayer> layer = static_cast<ImageLayer*>(
      aManager->GetLayerBuilder()->GetLeafLayerFor(aBuilder, this));
  if (!layer) {
    layer = aManager->CreateImageLayer();
    if (!layer) return nullptr;
  }
  layer->SetContainer(container);
  ConfigureLayer(layer, aParameters);
  return layer.forget();
}

bool nsDisplayImage::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc, RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  if (!mImage) {
    return false;
  }

  MOZ_ASSERT(mFrame->IsImageFrame() || mFrame->IsImageControlFrame());
  // Image layer doesn't support draw focus ring for image map.
  auto* frame = static_cast<nsImageFrame*>(mFrame);
  if (frame->HasImageMap()) {
    return false;
  }

  const bool oldImageIsDifferent =
      OldImageHasDifferentRatio(*frame, *mImage, mPrevImage);

  uint32_t flags = imgIContainer::FLAG_ASYNC_NOTIFY;
  if (aDisplayListBuilder->ShouldSyncDecodeImages() || oldImageIsDifferent) {
    flags |= imgIContainer::FLAG_SYNC_DECODE;
  }
  if (aDisplayListBuilder->UseHighQualityScaling()) {
    flags |= imgIContainer::FLAG_HIGH_QUALITY_SCALING;
  }

  const int32_t factor = mFrame->PresContext()->AppUnitsPerDevPixel();
  LayoutDeviceRect destRect(
      LayoutDeviceRect::FromAppUnits(GetDestRect(), factor));

  Maybe<SVGImageContext> svgContext;
  IntSize decodeSize = nsLayoutUtils::ComputeImageContainerDrawingParameters(
      mImage, mFrame, destRect, aSc, flags, svgContext);

  RefPtr<layers::ImageContainer> container;
  ImgDrawResult drawResult = mImage->GetImageContainerAtSize(
      aManager->LayerManager(), decodeSize, svgContext, flags,
      getter_AddRefs(container));

  // While we got a container, it may not contain a fully decoded surface. If
  // that is the case, and we have an image we were previously displaying which
  // has a fully decoded surface, then we should prefer the previous image.
  bool updatePrevImage = false;
  switch (drawResult) {
    case ImgDrawResult::NOT_READY:
    case ImgDrawResult::INCOMPLETE:
    case ImgDrawResult::TEMPORARY_ERROR:
      if (mPrevImage && mPrevImage != mImage) {
        RefPtr<ImageContainer> prevContainer;
        ImgDrawResult newDrawResult = mPrevImage->GetImageContainerAtSize(
            aManager->LayerManager(), decodeSize, svgContext, flags,
            getter_AddRefs(prevContainer));
        if (prevContainer && newDrawResult == ImgDrawResult::SUCCESS) {
          drawResult = newDrawResult;
          container = std::move(prevContainer);
          break;
        }

        // Previous image was unusable; we can forget about it.
        updatePrevImage = true;
      }
      break;
    case ImgDrawResult::NOT_SUPPORTED:
      return false;
    default:
      updatePrevImage = mPrevImage != mImage;
      break;
  }

  // The previous image was not used, and is different from the current image.
  // We should forget about it. We need to update the frame as well because the
  // display item may get recreated.
  if (updatePrevImage) {
    mPrevImage = mImage;
    frame->mPrevImage = frame->mImage;
  }

  // If the image container is empty, we don't want to fallback. Any other
  // failure will be due to resource constraints and fallback is unlikely to
  // help us. Hence we can ignore the return value from PushImage.
  if (container) {
    aManager->CommandBuilder().PushImage(this, container, aBuilder, aResources,
                                         aSc, destRect, destRect);
  }

  nsDisplayItemGenericImageGeometry::UpdateDrawResult(this, drawResult);
  return true;
}

ImgDrawResult nsImageFrame::PaintImage(gfxContext& aRenderingContext,
                                       nsPoint aPt, const nsRect& aDirtyRect,
                                       imgIContainer* aImage, uint32_t aFlags) {
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
  nsRect dest = nsLayoutUtils::ComputeObjectDestRect(
      constraintRect, mIntrinsicSize, mIntrinsicRatio, StylePosition(),
      &anchorPoint);

  uint32_t flags = aFlags;
  if (mForceSyncDecoding) {
    flags |= imgIContainer::FLAG_SYNC_DECODE;
  }

  Maybe<SVGImageContext> svgContext;
  SVGImageContext::MaybeStoreContextPaint(svgContext, this, aImage);

  // We've already accounted for resolution via mIntrinsicSize, which influences
  // the dest rect, so we don't need to worry about it here..
  ImgDrawResult result = nsLayoutUtils::DrawSingleImage(
      aRenderingContext, PresContext(), aImage, /* aResolution = */ 1.0f,
      nsLayoutUtils::GetSamplingFilterForFrame(this), dest, aDirtyRect,
      svgContext, flags, &anchorPoint);

  if (nsImageMap* map = GetImageMap()) {
    gfxPoint devPixelOffset = nsLayoutUtils::PointToGfxPoint(
        dest.TopLeft(), PresContext()->AppUnitsPerDevPixel());
    AutoRestoreTransform autoRestoreTransform(drawTarget);
    drawTarget->SetTransform(
        drawTarget->GetTransform().PreTranslate(ToPoint(devPixelOffset)));

    // solid white stroke:
    ColorPattern white(ToDeviceColor(sRGBColor::OpaqueWhite()));
    map->Draw(this, *drawTarget, white);

    // then dashed black stroke over the top:
    ColorPattern black(ToDeviceColor(sRGBColor::OpaqueBlack()));
    StrokeOptions strokeOptions;
    nsLayoutUtils::InitDashPattern(strokeOptions, StyleBorderStyle::Dotted);
    map->Draw(this, *drawTarget, black, strokeOptions);
  }

  if (result == ImgDrawResult::SUCCESS) {
    mPrevImage = aImage;
  } else if (result == ImgDrawResult::BAD_IMAGE) {
    mPrevImage = nullptr;
  }

  return result;
}

already_AddRefed<imgIRequest> nsImageFrame::GetCurrentRequest() const {
  if (mKind != Kind::ImageElement) {
    return do_AddRef(mContentURLRequest);
  }

  MOZ_ASSERT(!mContentURLRequest);

  nsCOMPtr<imgIRequest> request;
  nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(mContent);
  MOZ_ASSERT(imageLoader);
  imageLoader->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                          getter_AddRefs(request));
  return request.forget();
}

void nsImageFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                    const nsDisplayListSet& aLists) {
  if (!IsVisibleForPainting()) return;

  DisplayBorderBackgroundOutline(aBuilder, aLists);

  uint32_t clipFlags =
      nsStyleUtil::ObjectPropsMightCauseOverflow(StylePosition())
          ? 0
          : DisplayListClipState::ASSUME_DRAWING_RESTRICTED_TO_CONTENT_RECT;

  DisplayListClipState::AutoClipContainingBlockDescendantsToContentBox clip(
      aBuilder, this, clipFlags);

  if (mComputedSize.width != 0 && mComputedSize.height != 0) {
    bool imageOK =
        mKind != Kind::ImageElement || ImageOk(mContent->AsElement()->State());

    nsCOMPtr<imgIRequest> currentRequest = GetCurrentRequest();

    // XXX(seth): The SizeIsAvailable check here should not be necessary - the
    // intention is that a non-null mImage means we have a size, but there is
    // currently some code that violates this invariant.
    if (!imageOK || !mImage || !SizeIsAvailable(currentRequest)) {
      // No image yet, or image load failed. Draw the alt-text and an icon
      // indicating the status
      aLists.Content()->AppendNewToTop<nsDisplayAltFeedback>(aBuilder, this);

      // This image is visible (we are being asked to paint it) but it's not
      // decoded yet. And we are not going to ask the image to draw, so this
      // may be the only chance to tell it that it should decode.
      if (currentRequest) {
        uint32_t status = 0;
        currentRequest->GetImageStatus(&status);
        if (!(status & imgIRequest::STATUS_DECODE_COMPLETE)) {
          MaybeDecodeForPredictedSize();
        }
        // Increase loading priority if the image is ready to be displayed.
        if (!(status & imgIRequest::STATUS_LOAD_COMPLETE)) {
          currentRequest->BoostPriority(imgIRequest::CATEGORY_DISPLAY);
        }
      }
    } else {
      aLists.Content()->AppendNewToTop<nsDisplayImage>(aBuilder, this, mImage,
                                                       mPrevImage);

      // If we were previously displaying an icon, we're not anymore
      if (mDisplayingIcon) {
        gIconLoad->RemoveIconObserver(this);
        mDisplayingIcon = false;
      }

#ifdef DEBUG
      if (GetShowFrameBorders() && GetImageMap()) {
        aLists.Outlines()->AppendNewToTop<nsDisplayGeneric>(
            aBuilder, this, PaintDebugImageMap, "DebugImageMap",
            DisplayItemType::TYPE_DEBUG_IMAGE_MAP);
      }
#endif
    }
  }

  if (ShouldDisplaySelection()) {
    DisplaySelectionOverlay(aBuilder, aLists.Content(),
                            nsISelectionDisplay::DISPLAY_IMAGES);
  }
}

bool nsImageFrame::ShouldDisplaySelection() {
  int16_t displaySelection = PresShell()->GetSelectionFlags();
  if (!(displaySelection & nsISelectionDisplay::DISPLAY_IMAGES)) {
    // no need to check the blue border, we cannot be drawn selected.
    return false;
  }

  if (displaySelection != nsISelectionDisplay::DISPLAY_ALL) {
    return true;
  }

  // If the image is currently resize target of the editor, don't draw the
  // selection overlay.
  HTMLEditor* htmlEditor = nsContentUtils::GetHTMLEditor(PresContext());
  if (!htmlEditor) {
    return true;
  }

  return htmlEditor->GetResizerTarget() != mContent;
}

nsImageMap* nsImageFrame::GetImageMap() {
  if (!mImageMap) {
    if (nsIContent* map = GetMapElement()) {
      mImageMap = new nsImageMap();
      mImageMap->Init(this, map);
    }
  }

  return mImageMap;
}

bool nsImageFrame::IsServerImageMap() {
  return mContent->AsElement()->HasAttr(kNameSpaceID_None, nsGkAtoms::ismap);
}

// Translate an point that is relative to our frame
// into a localized pixel coordinate that is relative to the
// content area of this frame (inside the border+padding).
void nsImageFrame::TranslateEventCoords(const nsPoint& aPoint,
                                        nsIntPoint& aResult) {
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

bool nsImageFrame::GetAnchorHREFTargetAndNode(nsIURI** aHref, nsString& aTarget,
                                              nsIContent** aNode) {
  bool status = false;
  aTarget.Truncate();
  *aHref = nullptr;
  *aNode = nullptr;

  // Walk up the content tree, looking for an nsIDOMAnchorElement
  for (nsIContent* content = mContent->GetParent(); content;
       content = content->GetParent()) {
    nsCOMPtr<dom::Link> link(do_QueryInterface(content));
    if (link) {
      nsCOMPtr<nsIURI> href = content->GetHrefURI();
      if (href) {
        href.forget(aHref);
      }
      status = (*aHref != nullptr);

      RefPtr<HTMLAnchorElement> anchor = HTMLAnchorElement::FromNode(content);
      if (anchor) {
        anchor->GetTarget(aTarget);
      }
      NS_ADDREF(*aNode = content);
      break;
    }
  }
  return status;
}

nsresult nsImageFrame::GetContentForEvent(WidgetEvent* aEvent,
                                          nsIContent** aContent) {
  NS_ENSURE_ARG_POINTER(aContent);

  nsIFrame* f = nsLayoutUtils::GetNonGeneratedAncestor(this);
  if (f != this) {
    return f->GetContentForEvent(aEvent, aContent);
  }

  // XXX We need to make this special check for area element's capturing the
  // mouse due to bug 135040. Remove it once that's fixed.
  nsIContent* capturingContent = aEvent->HasMouseEventMessage()
                                     ? PresShell::GetCapturingContent()
                                     : nullptr;
  if (capturingContent && capturingContent->GetPrimaryFrame() == this) {
    *aContent = capturingContent;
    NS_IF_ADDREF(*aContent);
    return NS_OK;
  }

  if (nsImageMap* map = GetImageMap()) {
    nsIntPoint p;
    TranslateEventCoords(
        nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent, RelativeTo{this}),
        p);
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
nsresult nsImageFrame::HandleEvent(nsPresContext* aPresContext,
                                   WidgetGUIEvent* aEvent,
                                   nsEventStatus* aEventStatus) {
  NS_ENSURE_ARG_POINTER(aEventStatus);

  if ((aEvent->mMessage == eMouseClick &&
       aEvent->AsMouseEvent()->mButton == MouseButton::ePrimary) ||
      aEvent->mMessage == eMouseMove) {
    nsImageMap* map = GetImageMap();
    bool isServerMap = IsServerImageMap();
    if (map || isServerMap) {
      nsIntPoint p;
      TranslateEventCoords(nsLayoutUtils::GetEventCoordinatesRelativeTo(
                               aEvent, RelativeTo{this}),
                           p);
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
          rv = NS_MutateURI(uri).SetSpec(spec).Finalize(uri);
          NS_ENSURE_SUCCESS(rv, rv);

          bool clicked = false;
          if (aEvent->mMessage == eMouseClick && !aEvent->DefaultPrevented()) {
            *aEventStatus = nsEventStatus_eConsumeDoDefault;
            clicked = true;
          }
          nsContentUtils::TriggerLink(anchorNode, uri, target, clicked,
                                      /* isTrusted */ true);
        }
      }
    }
  }

  return nsAtomicContainerFrame::HandleEvent(aPresContext, aEvent,
                                             aEventStatus);
}

Maybe<nsIFrame::Cursor> nsImageFrame::GetCursor(const nsPoint& aPoint) {
  nsImageMap* map = GetImageMap();
  if (!map) {
    return nsIFrame::GetCursor(aPoint);
  }
  nsIntPoint p;
  TranslateEventCoords(aPoint, p);
  HTMLAreaElement* area = map->GetArea(p.x, p.y);
  if (!area) {
    return nsIFrame::GetCursor(aPoint);
  }

  // Use the cursor from the style of the *area* element.
  RefPtr<ComputedStyle> areaStyle =
      PresShell()->StyleSet()->ResolveStyleLazily(*area);

  // This is one of the cases, like the <xul:tree> pseudo-style stuff, where we
  // get styles out of the blue and expect to trigger image loads for those.
  areaStyle->StartImageLoads(*PresContext()->Document());

  StyleCursorKind kind = areaStyle->StyleUI()->mCursor.keyword;
  if (kind == StyleCursorKind::Auto) {
    kind = StyleCursorKind::Default;
  }
  return Some(Cursor{kind, AllowCustomCursorImage::Yes, std::move(areaStyle)});
}

nsresult nsImageFrame::AttributeChanged(int32_t aNameSpaceID,
                                        nsAtom* aAttribute, int32_t aModType) {
  nsresult rv = nsAtomicContainerFrame::AttributeChanged(aNameSpaceID,
                                                         aAttribute, aModType);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (nsGkAtoms::alt == aAttribute) {
    PresShell()->FrameNeedsReflow(this, IntrinsicDirty::StyleChange,
                                  NS_FRAME_IS_DIRTY);
  }

  return NS_OK;
}

void nsImageFrame::OnVisibilityChange(
    Visibility aNewVisibility, const Maybe<OnNonvisible>& aNonvisibleAction) {
  if (mKind == Kind::ImageElement) {
    nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(mContent);
    imageLoader->OnVisibilityChange(aNewVisibility, aNonvisibleAction);
  }

  if (aNewVisibility == Visibility::ApproximatelyVisible &&
      PresShell()->IsActive()) {
    MaybeDecodeForPredictedSize();
  }

  nsAtomicContainerFrame::OnVisibilityChange(aNewVisibility, aNonvisibleAction);
}

#ifdef DEBUG_FRAME_DUMP
nsresult nsImageFrame::GetFrameName(nsAString& aResult) const {
  return MakeFrameName(u"ImageFrame"_ns, aResult);
}

void nsImageFrame::List(FILE* out, const char* aPrefix,
                        ListFlags aFlags) const {
  nsCString str;
  ListGeneric(str, aPrefix, aFlags);

  // output the img src url
  if (nsCOMPtr<imgIRequest> currentRequest = GetCurrentRequest()) {
    nsCOMPtr<nsIURI> uri;
    currentRequest->GetURI(getter_AddRefs(uri));
    nsAutoCString uristr;
    uri->GetAsciiSpec(uristr);
    str += nsPrintfCString(" [src=%s]", uristr.get());
  }
  fprintf_stderr(out, "%s\n", str.get());
}
#endif

LogicalSides nsImageFrame::GetLogicalSkipSides() const {
  LogicalSides skip(mWritingMode);
  if (MOZ_UNLIKELY(StyleBorder()->mBoxDecorationBreak ==
                   StyleBoxDecorationBreak::Clone)) {
    return skip;
  }
  if (GetPrevInFlow()) {
    skip |= eLogicalSideBitsBStart;
  }
  if (GetNextInFlow()) {
    skip |= eLogicalSideBitsBEnd;
  }
  return skip;
}

nsresult nsImageFrame::LoadIcon(const nsAString& aSpec,
                                nsPresContext* aPresContext,
                                imgRequestProxy** aRequest) {
  MOZ_ASSERT(!aSpec.IsEmpty(), "What happened??");

  nsCOMPtr<nsIURI> realURI;
  SpecToURI(aSpec, getter_AddRefs(realURI));

  RefPtr<imgLoader> il =
      nsContentUtils::GetImgLoaderForDocument(aPresContext->Document());

  nsCOMPtr<nsILoadGroup> loadGroup;
  GetLoadGroup(aPresContext, getter_AddRefs(loadGroup));

  // For icon loads, we don't need to merge with the loadgroup flags
  nsLoadFlags loadFlags = nsIRequest::LOAD_NORMAL;
  nsContentPolicyType contentPolicyType = nsIContentPolicy::TYPE_INTERNAL_IMAGE;

  return il->LoadImage(
      realURI,                          /* icon URI */
      nullptr,                          /* initial document URI; this is only
                                          relevant for cookies, so does not
                                          apply to icons. */
      nullptr,                          /* referrer (not relevant for icons) */
      nullptr,                          /* principal (not relevant for icons) */
      0, loadGroup, gIconLoad, nullptr, /* No context */
      nullptr, /* Not associated with any particular document */
      loadFlags, nullptr, contentPolicyType, u""_ns,
      false, /* aUseUrgentStartForChannel */
      false, /* aLinkPreload */
      aRequest);
}

void nsImageFrame::GetDocumentCharacterSet(nsACString& aCharset) const {
  if (mContent) {
    NS_ASSERTION(mContent->GetComposedDoc(),
                 "Frame still alive after content removed from document!");
    mContent->GetComposedDoc()->GetDocumentCharacterSet()->Name(aCharset);
  }
}

void nsImageFrame::SpecToURI(const nsAString& aSpec, nsIURI** aURI) {
  nsIURI* baseURI = nullptr;
  if (mContent) {
    baseURI = mContent->GetBaseURI();
  }
  nsAutoCString charset;
  GetDocumentCharacterSet(charset);
  NS_NewURI(aURI, aSpec, charset.IsEmpty() ? nullptr : charset.get(), baseURI);
}

void nsImageFrame::GetLoadGroup(nsPresContext* aPresContext,
                                nsILoadGroup** aLoadGroup) {
  if (!aPresContext) return;

  MOZ_ASSERT(nullptr != aLoadGroup, "null OUT parameter pointer");

  mozilla::PresShell* presShell = aPresContext->GetPresShell();
  if (!presShell) {
    return;
  }

  Document* doc = presShell->GetDocument();
  if (!doc) {
    return;
  }

  *aLoadGroup = doc->GetDocumentLoadGroup().take();
}

nsresult nsImageFrame::LoadIcons(nsPresContext* aPresContext) {
  NS_ASSERTION(!gIconLoad, "called LoadIcons twice");

  constexpr auto loadingSrc = u"resource://gre-resources/loading-image.png"_ns;
  constexpr auto brokenSrc = u"resource://gre-resources/broken-image.png"_ns;

  gIconLoad = new IconLoad();

  nsresult rv;
  // create a loader and load the images
  rv = LoadIcon(loadingSrc, aPresContext,
                getter_AddRefs(gIconLoad->mLoadingImage));
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = LoadIcon(brokenSrc, aPresContext,
                getter_AddRefs(gIconLoad->mBrokenImage));
  if (NS_FAILED(rv)) {
    return rv;
  }

  return rv;
}

NS_IMPL_ISUPPORTS(nsImageFrame::IconLoad, nsIObserver, imgINotificationObserver)

static const char* kIconLoadPrefs[] = {
    "browser.display.force_inline_alttext",
    "browser.display.show_image_placeholders",
    "browser.display.show_loading_image_placeholder", nullptr};

nsImageFrame::IconLoad::IconLoad() {
  // register observers
  Preferences::AddStrongObservers(this, kIconLoadPrefs);
  GetPrefs();
}

void nsImageFrame::IconLoad::Shutdown() {
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
nsImageFrame::IconLoad::Observe(nsISupports* aSubject, const char* aTopic,
                                const char16_t* aData) {
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

void nsImageFrame::IconLoad::GetPrefs() {
  mPrefForceInlineAltText =
      Preferences::GetBool("browser.display.force_inline_alttext");

  mPrefShowPlaceholders =
      Preferences::GetBool("browser.display.show_image_placeholders", true);

  mPrefShowLoadingPlaceholder = Preferences::GetBool(
      "browser.display.show_loading_image_placeholder", true);
}

nsresult nsImageFrame::RestartAnimation() {
  nsCOMPtr<imgIRequest> currentRequest = GetCurrentRequest();

  if (currentRequest) {
    bool deregister = false;
    nsLayoutUtils::RegisterImageRequestIfAnimated(PresContext(), currentRequest,
                                                  &deregister);
  }
  return NS_OK;
}

nsresult nsImageFrame::StopAnimation() {
  nsCOMPtr<imgIRequest> currentRequest = GetCurrentRequest();

  if (currentRequest) {
    bool deregister = true;
    nsLayoutUtils::DeregisterImageRequest(PresContext(), currentRequest,
                                          &deregister);
  }
  return NS_OK;
}

void nsImageFrame::IconLoad::Notify(imgIRequest* aRequest, int32_t aType,
                                    const nsIntRect* aData) {
  MOZ_ASSERT(aRequest);

  if (aType != imgINotificationObserver::LOAD_COMPLETE &&
      aType != imgINotificationObserver::FRAME_UPDATE) {
    return;
  }

  if (aType == imgINotificationObserver::LOAD_COMPLETE) {
    nsCOMPtr<imgIContainer> image;
    aRequest->GetImage(getter_AddRefs(image));
    if (!image) {
      return;
    }

    // Retrieve the image's intrinsic size.
    int32_t width = 0;
    int32_t height = 0;
    image->GetWidth(&width);
    image->GetHeight(&height);

    // Request a decode at that size.
    image->RequestDecodeForSize(IntSize(width, height),
                                imgIContainer::DECODE_FLAGS_DEFAULT |
                                    imgIContainer::FLAG_HIGH_QUALITY_SCALING);
  }

  for (nsImageFrame* frame : mIconObservers.ForwardRange()) {
    frame->InvalidateFrame();
  }
}

NS_IMPL_ISUPPORTS(nsImageListener, imgINotificationObserver)

nsImageListener::nsImageListener(nsImageFrame* aFrame) : mFrame(aFrame) {}

nsImageListener::~nsImageListener() = default;

void nsImageListener::Notify(imgIRequest* aRequest, int32_t aType,
                             const nsIntRect* aData) {
  if (!mFrame) {
    return;
  }

  return mFrame->Notify(aRequest, aType, aData);
}

static bool IsInAutoWidthTableCellForQuirk(nsIFrame* aFrame) {
  if (eCompatibility_NavQuirks != aFrame->PresContext()->CompatibilityMode())
    return false;
  // Check if the parent of the closest nsBlockFrame has auto width.
  nsBlockFrame* ancestor = nsLayoutUtils::FindNearestBlockAncestor(aFrame);
  if (ancestor->Style()->GetPseudoType() == PseudoStyleType::cellContent) {
    // Assume direct parent is a table cell frame.
    nsIFrame* grandAncestor = static_cast<nsIFrame*>(ancestor->GetParent());
    return grandAncestor && grandAncestor->StylePosition()->mWidth.IsAuto();
  }
  return false;
}

void nsImageFrame::AddInlineMinISize(gfxContext* aRenderingContext,
                                     nsIFrame::InlineMinISizeData* aData) {
  nscoord isize = nsLayoutUtils::IntrinsicForContainer(
      aRenderingContext, this, IntrinsicISizeType::MinISize);
  bool canBreak = !IsInAutoWidthTableCellForQuirk(this);
  aData->DefaultAddInlineMinISize(this, isize, canBreak);
}
