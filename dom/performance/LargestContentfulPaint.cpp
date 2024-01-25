/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozilla/dom/Element.h"
#include "nsContentUtils.h"
#include "nsLayoutUtils.h"
#include "nsRFPService.h"
#include "Performance.h"
#include "imgRequest.h"
#include "PerformanceMainThread.h"
#include "LargestContentfulPaint.h"

#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/DOMIntersectionObserver.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"

#include "mozilla/PresShell.h"
#include "mozilla/Logging.h"
#include "mozilla/nsVideoFrame.h"

namespace mozilla::dom {

static LazyLogModule gLCPLogging("LargestContentfulPaint");

#define LOG(...) MOZ_LOG(gLCPLogging, LogLevel::Debug, (__VA_ARGS__))

NS_IMPL_CYCLE_COLLECTION_INHERITED(LargestContentfulPaint, PerformanceEntry,
                                   mPerformance, mURI, mElement)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(LargestContentfulPaint)
NS_INTERFACE_MAP_END_INHERITING(PerformanceEntry)

NS_IMPL_ADDREF_INHERITED(LargestContentfulPaint, PerformanceEntry)
NS_IMPL_RELEASE_INHERITED(LargestContentfulPaint, PerformanceEntry)

static double GetAreaInDoublePixelsFromAppUnits(const nsSize& aSize) {
  return NSAppUnitsToDoublePixels(aSize.Width(), AppUnitsPerCSSPixel()) *
         NSAppUnitsToDoublePixels(aSize.Height(), AppUnitsPerCSSPixel());
}

static double GetAreaInDoublePixelsFromAppUnits(const nsRect& aRect) {
  return NSAppUnitsToDoublePixels(aRect.Width(), AppUnitsPerCSSPixel()) *
         NSAppUnitsToDoublePixels(aRect.Height(), AppUnitsPerCSSPixel());
}

ImagePendingRendering::ImagePendingRendering(
    const LCPImageEntryKey& aLCPImageEntryKey, DOMHighResTimeStamp aLoadTime)
    : mLCPImageEntryKey(aLCPImageEntryKey), mLoadTime(aLoadTime) {}

LargestContentfulPaint::LargestContentfulPaint(
    PerformanceMainThread* aPerformance, const DOMHighResTimeStamp aRenderTime,
    const DOMHighResTimeStamp aLoadTime, const unsigned long aSize,
    nsIURI* aURI, Element* aElement,
    const Maybe<const LCPImageEntryKey>& aLCPImageEntryKey,
    bool aShouldExposeRenderTime)
    : PerformanceEntry(aPerformance->GetParentObject(), u""_ns,
                       kLargestContentfulPaintName),
      mPerformance(aPerformance),
      mRenderTime(aRenderTime),
      mLoadTime(aLoadTime),
      mShouldExposeRenderTime(aShouldExposeRenderTime),
      mSize(aSize),
      mURI(aURI),
      mLCPImageEntryKey(aLCPImageEntryKey) {
  MOZ_ASSERT(mPerformance);
  MOZ_ASSERT(aElement);
  // The element could be a pseudo-element
  if (aElement->ChromeOnlyAccess()) {
    mElement = do_GetWeakReference(Element::FromNodeOrNull(
        aElement->FindFirstNonChromeOnlyAccessContent()));
  } else {
    mElement = do_GetWeakReference(aElement);
  }

  if (const Element* element = GetElement()) {
    mId = element->GetID();
  }
}

JSObject* LargestContentfulPaint::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return LargestContentfulPaint_Binding::Wrap(aCx, this, aGivenProto);
}

Element* LargestContentfulPaint::GetElement() const {
  nsCOMPtr<Element> element = do_QueryReferent(mElement);
  return element ? nsContentUtils::GetAnElementForTiming(
                       element, element->GetComposedDoc(), nullptr)
                 : nullptr;
}

void LargestContentfulPaint::BufferEntryIfNeeded() {
  MOZ_ASSERT(mLCPImageEntryKey.isNothing());
  mPerformance->BufferLargestContentfulPaintEntryIfNeeded(this);
}

/* static*/
bool LCPHelpers::IsQualifiedImageRequest(imgRequest* aRequest,
                                         Element* aContainingElement) {
  MOZ_ASSERT(aContainingElement);
  if (!aRequest) {
    return false;
  }

  if (aRequest->IsChrome()) {
    return false;
  }

  if (!aContainingElement->ChromeOnlyAccess()) {
    return true;
  }

  // Exception: this is a poster image of video element
  if (nsIContent* parent = aContainingElement->GetParent()) {
    nsVideoFrame* videoFrame = do_QueryFrame(parent->GetPrimaryFrame());
    if (videoFrame && videoFrame->GetPosterImage() == aContainingElement) {
      return true;
    }
  }

  // Exception: CSS generated images
  if (aContainingElement->IsInNativeAnonymousSubtree()) {
    if (nsINode* rootParentOrHost =
            aContainingElement
                ->GetClosestNativeAnonymousSubtreeRootParentOrHost()) {
      if (!rootParentOrHost->ChromeOnlyAccess()) {
        return true;
      }
    }
  }
  return false;
}
void LargestContentfulPaint::MaybeProcessImageForElementTiming(
    imgRequestProxy* aRequest, Element* aElement) {
  if (!StaticPrefs::dom_enable_largest_contentful_paint()) {
    return;
  }

  MOZ_ASSERT(aRequest);
  imgRequest* request = aRequest->GetOwner();
  if (!LCPHelpers::IsQualifiedImageRequest(request, aElement)) {
    return;
  }

  Document* document = aElement->GetComposedDoc();
  if (!document) {
    return;
  }

  nsPresContext* pc =
      aElement->GetPresContext(Element::PresContextFor::eForComposedDoc);
  if (!pc) {
    return;
  }

  PerformanceMainThread* performance = pc->GetPerformanceMainThread();
  if (!performance) {
    return;
  }

  if (MOZ_UNLIKELY(MOZ_LOG_TEST(gLCPLogging, LogLevel::Debug))) {
    nsCOMPtr<nsIURI> uri;
    aRequest->GetURI(getter_AddRefs(uri));
    LOG("MaybeProcessImageForElementTiming, Element=%p, URI=%s, "
        "performance=%p ",
        aElement, uri ? uri->GetSpecOrDefault().get() : "", performance);
  }

  const LCPImageEntryKey entryKey = LCPImageEntryKey(aElement, aRequest);
  if (!document->ContentIdentifiersForLCP().EnsureInserted(entryKey)) {
    LOG("  The content identifier existed for element=%p and request=%p, "
        "return.",
        aElement, aRequest);
    return;
  }

#ifdef DEBUG
  uint32_t status = imgIRequest::STATUS_NONE;
  aRequest->GetImageStatus(&status);
  MOZ_ASSERT(status & imgIRequest::STATUS_LOAD_COMPLETE);
#endif

  // Here we are exposing the load time of the image which could be
  // a privacy concern. The spec talks about it at
  // https://wicg.github.io/element-timing/#sec-security
  // TLDR: The similar metric can be obtained by ResourceTiming
  // API and onload handlers already, so this is not exposing anything
  // new.
  DOMHighResTimeStamp nowTime =
      performance->TimeStampToDOMHighResForRendering(TimeStamp::Now());

  // At this point, the loadTime of the image is known, but
  // the renderTime is unknown, so it's added to ImagesPendingRendering
  // as a placeholder, and the corresponding LCP entry will be created
  // when the renderTime is known.
  LOG("  Added a pending image rendering");
  performance->AddImagesPendingRendering(
      ImagePendingRendering{entryKey, nowTime});
}

bool LCPHelpers::CanFinalizeLCPEntry(const nsIFrame* aFrame) {
  if (!StaticPrefs::dom_enable_largest_contentful_paint()) {
    return false;
  }

  if (!aFrame) {
    return false;
  }

  nsPresContext* presContext = aFrame->PresContext();
  return !presContext->HasStoppedGeneratingLCP() &&
         presContext->GetPerformanceMainThread();
}

void LCPHelpers::FinalizeLCPEntryForImage(
    Element* aContainingBlock, imgRequestProxy* aImgRequestProxy,
    const nsRect& aTargetRectRelativeToSelf) {
  LOG("FinalizeLCPEntryForImage element=%p", aContainingBlock);
  if (!aImgRequestProxy) {
    return;
  }

  if (!IsQualifiedImageRequest(aImgRequestProxy->GetOwner(),
                               aContainingBlock)) {
    return;
  }

  nsIFrame* frame = aContainingBlock->GetPrimaryFrame();

  if (!CanFinalizeLCPEntry(frame)) {
    return;
  }

  PerformanceMainThread* performance =
      frame->PresContext()->GetPerformanceMainThread();
  MOZ_ASSERT(performance);

  RefPtr<LargestContentfulPaint> entry =
      performance->GetImageLCPEntry(aContainingBlock, aImgRequestProxy);
  if (!entry) {
    LOG("  No Image Entry");
    return;
  }
  entry->UpdateSize(aContainingBlock, aTargetRectRelativeToSelf, performance,
                    true);
  // If area is less than or equal to document’s largest contentful paint size,
  // return.
  if (!performance->UpdateLargestContentfulPaintSize(entry->Size())) {
    LOG(

        "  This paint(%lu) is not greater than the largest paint (%lf)that "
        "we've "
        "reported so far, return",
        entry->Size(), performance->GetLargestContentfulPaintSize());
    return;
  }

  entry->QueueEntry();
}

DOMHighResTimeStamp LargestContentfulPaint::RenderTime() const {
  if (!mShouldExposeRenderTime) {
    return 0;
  }
  return nsRFPService::ReduceTimePrecisionAsMSecs(
      mRenderTime, mPerformance->GetRandomTimelineSeed(),
      mPerformance->GetRTPCallerType());
}

DOMHighResTimeStamp LargestContentfulPaint::LoadTime() const {
  return nsRFPService::ReduceTimePrecisionAsMSecs(
      mLoadTime, mPerformance->GetRandomTimelineSeed(),
      mPerformance->GetRTPCallerType());
}

DOMHighResTimeStamp LargestContentfulPaint::StartTime() const {
  DOMHighResTimeStamp startTime =
      mShouldExposeRenderTime ? mRenderTime : mLoadTime;
  return nsRFPService::ReduceTimePrecisionAsMSecs(
      startTime, mPerformance->GetRandomTimelineSeed(),
      mPerformance->GetRTPCallerType());
}

/* static */
Element* LargestContentfulPaint::GetContainingBlockForTextFrame(
    const nsTextFrame* aTextFrame) {
  nsIFrame* containingFrame = aTextFrame->GetContainingBlock();
  MOZ_ASSERT(containingFrame);
  return Element::FromNodeOrNull(containingFrame->GetContent());
}

void LargestContentfulPaint::QueueEntry() {
  LOG("QueueEntry entry=%p", this);
  mPerformance->QueueLargestContentfulPaintEntry(this);

  ReportLCPToNavigationTimings();
}

void LargestContentfulPaint::GetUrl(nsAString& aUrl) {
  if (mURI) {
    CopyUTF8toUTF16(mURI->GetSpecOrDefault(), aUrl);
  }
}

void LargestContentfulPaint::UpdateSize(
    const Element* aContainingBlock, const nsRect& aTargetRectRelativeToSelf,
    const PerformanceMainThread* aPerformance, bool aIsImage) {
  nsIFrame* frame = aContainingBlock->GetPrimaryFrame();
  MOZ_ASSERT(frame);

  nsIFrame* rootFrame = frame->PresShell()->GetRootFrame();
  if (!rootFrame) {
    return;
  }

  if (frame->Style()->IsInOpacityZeroSubtree()) {
    LOG("  Opacity:0 return");
    return;
  }

  // The following size computation is based on a pending pull request
  // https://github.com/w3c/largest-contentful-paint/pull/99

  // Let visibleDimensions be concreteDimensions, adjusted for positioning
  // by object-position or background-position and element’s content box.
  const nsRect& visibleDimensions = aTargetRectRelativeToSelf;

  // Let clientContentRect be the smallest DOMRectReadOnly containing
  // visibleDimensions with element’s transforms applied.
  nsRect clientContentRect = nsLayoutUtils::TransformFrameRectToAncestor(
      frame, visibleDimensions, rootFrame);

  // Let intersectionRect be the value returned by the intersection rect
  // algorithm using element as the target and viewport as the root.
  // (From https://wicg.github.io/element-timing/#sec-report-image-element)
  IntersectionInput input = DOMIntersectionObserver::ComputeInput(
      *frame->PresContext()->Document(), rootFrame->GetContent(), nullptr);
  const IntersectionOutput output =
      DOMIntersectionObserver::Intersect(input, *aContainingBlock);

  Maybe<nsRect> intersectionRect = output.mIntersectionRect;

  if (intersectionRect.isNothing()) {
    LOG("  The intersectionRect is nothing for Element=%p. return.",
        aContainingBlock);
    return;
  }

  // Let intersectingClientContentRect be the intersection of clientContentRect
  // with intersectionRect.
  Maybe<nsRect> intersectionWithContentRect =
      clientContentRect.EdgeInclusiveIntersection(intersectionRect.value());

  if (intersectionWithContentRect.isNothing()) {
    LOG("  The intersectionWithContentRect is nothing for Element=%p. return.",
        aContainingBlock);
    return;
  }

  nsRect renderedRect = intersectionWithContentRect.value();

  double area = GetAreaInDoublePixelsFromAppUnits(renderedRect);

  double viewport = GetAreaInDoublePixelsFromAppUnits(input.mRootRect);

  LOG("  Viewport = %f, RenderRect = %f.", viewport, area);
  // We don't want to report things that take the entire viewport.
  if (area >= viewport) {
    LOG("  The renderedRect is at least same as the area of the "
        "viewport for Element=%p, return.",
        aContainingBlock);
    return;
  }

  Maybe<nsSize> intrinsicSize = frame->GetIntrinsicSize().ToSize();
  const bool hasIntrinsicSize = intrinsicSize && !intrinsicSize->IsEmpty();

  if (aIsImage && hasIntrinsicSize) {
    //  Let (naturalWidth, naturalHeight) be imageRequest’s natural dimension.
    //  Let naturalArea be naturalWidth * naturalHeight.
    double naturalArea =
        GetAreaInDoublePixelsFromAppUnits(intrinsicSize.value());

    LOG("  naturalArea = %f", naturalArea);

    // Let boundingClientArea be clientContentRect’s width * clientContentRect’s
    // height.
    double boundingClientArea =
        NSAppUnitsToDoublePixels(clientContentRect.Width(),
                                 AppUnitsPerCSSPixel()) *
        NSAppUnitsToDoublePixels(clientContentRect.Height(),
                                 AppUnitsPerCSSPixel());
    LOG("  boundingClientArea = %f", boundingClientArea);

    // Let scaleFactor be boundingClientArea / naturalArea.
    double scaleFactor = boundingClientArea / naturalArea;
    LOG("  scaleFactor = %f", scaleFactor);

    // If scaleFactor is greater than 1, then divide area by scaleFactor.
    if (scaleFactor > 1) {
      LOG("  area before sacled doown %f", area);
      area = area / scaleFactor;
    }
  }

  MOZ_ASSERT(!mSize);
  mSize = area;
}

void LCPTextFrameHelper::MaybeUnionTextFrame(
    nsTextFrame* aTextFrame, const nsRect& aRelativeToSelfRect) {
  if (!StaticPrefs::dom_enable_largest_contentful_paint() ||
      aTextFrame->PresContext()->HasStoppedGeneratingLCP()) {
    return;
  }

  Element* containingBlock =
      LargestContentfulPaint::GetContainingBlockForTextFrame(aTextFrame);
  if (!containingBlock ||
      // If element is contained in doc’s set of elements with rendered text,
      // continue
      containingBlock->HasFlag(ELEMENT_PROCESSED_BY_LCP_FOR_TEXT) ||
      containingBlock->ChromeOnlyAccess()) {
    return;
  }

  MOZ_ASSERT(containingBlock->GetPrimaryFrame());

  PerformanceMainThread* perf =
      aTextFrame->PresContext()->GetPerformanceMainThread();
  if (!perf) {
    return;
  }

  auto& unionRect = perf->GetTextFrameUnions().LookupOrInsert(containingBlock);
  unionRect = unionRect.Union(aRelativeToSelfRect);
}

void LCPHelpers::CreateLCPEntryForImage(
    PerformanceMainThread* aPerformance, Element* aElement,
    imgRequestProxy* aRequestProxy, const DOMHighResTimeStamp aLoadTime,
    const DOMHighResTimeStamp aRenderTime,
    const LCPImageEntryKey& aImageEntryKey) {
  MOZ_ASSERT(StaticPrefs::dom_enable_largest_contentful_paint());
  MOZ_ASSERT(aRequestProxy);
  if (MOZ_UNLIKELY(MOZ_LOG_TEST(gLCPLogging, LogLevel::Debug))) {
    nsCOMPtr<nsIURI> uri;
    aRequestProxy->GetURI(getter_AddRefs(uri));
    LOG("CreateLCPEntryForImage "
        "Element=%p, aRequestProxy=%p, URI=%s loadTime=%f, "
        "aRenderTime=%f\n",
        aElement, aRequestProxy, uri->GetSpecOrDefault().get(), aLoadTime,
        aRenderTime);
  }
  MOZ_ASSERT(aPerformance);
  MOZ_ASSERT_IF(aRenderTime < aLoadTime, aRenderTime == 0);
  if (aPerformance->HasDispatchedInputEvent() ||
      aPerformance->HasDispatchedScrollEvent()) {
    return;
  }

  // Let url be the empty string.
  // If imageRequest is not null, set url to be imageRequest’s request URL.
  nsCOMPtr<nsIURI> requestURI;
  aRequestProxy->GetURI(getter_AddRefs(requestURI));

  imgRequest* request = aRequestProxy->GetOwner();
  // We should never get here unless request is valid.
  MOZ_ASSERT(request);

  bool taoPassed = request->ShouldReportRenderTimeForLCP() || request->IsData();
  // https://wicg.github.io/element-timing/#report-image-element-timing
  // For TAO failed requests, the renderTime is exposed as 0 for
  // security reasons.
  //
  // At this point, we have all the information about the entry
  // except the size.
  RefPtr<LargestContentfulPaint> entry = new LargestContentfulPaint(
      aPerformance, aRenderTime, aLoadTime, 0, requestURI, aElement,
      Some(aImageEntryKey), taoPassed);

  LOG("  Upsert a LargestContentfulPaint entry=%p to LCPEntryMap.",
      entry.get());
  aPerformance->StoreImageLCPEntry(aElement, aRequestProxy, entry);
}

void LCPHelpers::FinalizeLCPEntryForText(
    PerformanceMainThread* aPerformance, const DOMHighResTimeStamp aRenderTime,
    Element* aContainingBlock, const nsRect& aTargetRectRelativeToSelf,
    const nsPresContext* aPresContext) {
  MOZ_ASSERT(aPerformance);
  LOG("FinalizeLCPEntryForText element=%p", aContainingBlock);

  if (!aContainingBlock->GetPrimaryFrame()) {
    return;
  }
  MOZ_ASSERT(CanFinalizeLCPEntry(aContainingBlock->GetPrimaryFrame()));
  MOZ_ASSERT(!aContainingBlock->HasFlag(ELEMENT_PROCESSED_BY_LCP_FOR_TEXT));
  MOZ_ASSERT(!aContainingBlock->ChromeOnlyAccess());

  aContainingBlock->SetFlags(ELEMENT_PROCESSED_BY_LCP_FOR_TEXT);

  RefPtr<LargestContentfulPaint> entry =
      new LargestContentfulPaint(aPerformance, aRenderTime, 0, 0, nullptr,
                                 aContainingBlock, Nothing(), true);

  entry->UpdateSize(aContainingBlock, aTargetRectRelativeToSelf, aPerformance,
                    false);
  // If area is less than or equal to document’s largest contentful paint size,
  // return.
  if (!aPerformance->UpdateLargestContentfulPaintSize(entry->Size())) {
    LOG("  This paint(%lu) is not greater than the largest paint (%lf)that "
        "we've "
        "reported so far, return",
        entry->Size(), aPerformance->GetLargestContentfulPaintSize());
    return;
  }
  entry->QueueEntry();
}

void LargestContentfulPaint::ReportLCPToNavigationTimings() {
  nsCOMPtr<Element> element = do_QueryReferent(mElement);
  if (!element) {
    return;
  }

  const Document* document = element->OwnerDoc();

  MOZ_ASSERT(document);

  nsDOMNavigationTiming* timing = document->GetNavigationTiming();

  if (MOZ_UNLIKELY(!timing)) {
    return;
  }

  if (document->IsResourceDoc()) {
    return;
  }

  if (BrowsingContext* browsingContext = document->GetBrowsingContext()) {
    if (browsingContext->GetEmbeddedInContentDocument()) {
      return;
    }
  }

  if (!document->IsTopLevelContentDocument()) {
    return;
  }
  timing->NotifyLargestContentfulRenderForRootContentDocument(mRenderTime);
}
}  // namespace mozilla::dom
