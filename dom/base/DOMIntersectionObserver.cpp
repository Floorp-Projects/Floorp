/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMIntersectionObserver.h"
#include "nsCSSPropertyID.h"
#include "nsIFrame.h"
#include "nsContainerFrame.h"
#include "nsIScrollableFrame.h"
#include "nsContentUtils.h"
#include "nsLayoutUtils.h"
#include "nsRefreshDriver.h"
#include "mozilla/PresShell.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/dom/HTMLImageElement.h"
#include "Units.h"

namespace mozilla::dom {

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMIntersectionObserverEntry)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMIntersectionObserverEntry)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMIntersectionObserverEntry)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(DOMIntersectionObserverEntry, mOwner,
                                      mRootBounds, mBoundingClientRect,
                                      mIntersectionRect, mTarget)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMIntersectionObserver)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(DOMIntersectionObserver)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMIntersectionObserver)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMIntersectionObserver)

NS_IMPL_CYCLE_COLLECTION_CLASS(DOMIntersectionObserver)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(DOMIntersectionObserver)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(DOMIntersectionObserver)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  tmp->Disconnect();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOwner)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocument)
  if (tmp->mCallback.is<RefPtr<dom::IntersectionCallback>>()) {
    ImplCycleCollectionUnlink(
        tmp->mCallback.as<RefPtr<dom::IntersectionCallback>>());
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mRoot)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mQueuedEntries)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(DOMIntersectionObserver)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOwner)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocument)
  if (tmp->mCallback.is<RefPtr<dom::IntersectionCallback>>()) {
    ImplCycleCollectionTraverse(
        cb, tmp->mCallback.as<RefPtr<dom::IntersectionCallback>>(), "mCallback",
        0);
  }
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRoot)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mQueuedEntries)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

DOMIntersectionObserver::DOMIntersectionObserver(
    already_AddRefed<nsPIDOMWindowInner>&& aOwner,
    dom::IntersectionCallback& aCb)
    : mOwner(aOwner),
      mDocument(mOwner->GetExtantDoc()),
      mCallback(RefPtr<dom::IntersectionCallback>(&aCb)),
      mConnected(false) {}

already_AddRefed<DOMIntersectionObserver> DOMIntersectionObserver::Constructor(
    const GlobalObject& aGlobal, dom::IntersectionCallback& aCb,
    ErrorResult& aRv) {
  return Constructor(aGlobal, aCb, IntersectionObserverInit(), aRv);
}

already_AddRefed<DOMIntersectionObserver> DOMIntersectionObserver::Constructor(
    const GlobalObject& aGlobal, dom::IntersectionCallback& aCb,
    const IntersectionObserverInit& aOptions, ErrorResult& aRv) {
  nsCOMPtr<nsPIDOMWindowInner> window =
      do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  RefPtr<DOMIntersectionObserver> observer =
      new DOMIntersectionObserver(window.forget(), aCb);

  if (!aOptions.mRoot.IsNull()) {
    if (aOptions.mRoot.Value().IsElement()) {
      observer->mRoot = aOptions.mRoot.Value().GetAsElement();
    } else {
      MOZ_ASSERT(aOptions.mRoot.Value().IsDocument());
      if (!StaticPrefs::
              dom_IntersectionObserverExplicitDocumentRoot_enabled()) {
        aRv.ThrowTypeError<dom::MSG_DOES_NOT_IMPLEMENT_INTERFACE>(
            "'root' member of IntersectionObserverInit", "Element");
        return nullptr;
      }
      observer->mRoot = aOptions.mRoot.Value().GetAsDocument();
    }
  }

  if (!observer->SetRootMargin(aOptions.mRootMargin)) {
    aRv.ThrowSyntaxError("rootMargin must be specified in pixels or percent.");
    return nullptr;
  }

  if (aOptions.mThreshold.IsDoubleSequence()) {
    const Sequence<double>& thresholds =
        aOptions.mThreshold.GetAsDoubleSequence();
    observer->mThresholds.SetCapacity(thresholds.Length());
    for (const auto& thresh : thresholds) {
      if (thresh < 0.0 || thresh > 1.0) {
        aRv.ThrowRangeError<dom::MSG_THRESHOLD_RANGE_ERROR>();
        return nullptr;
      }
      observer->mThresholds.AppendElement(thresh);
    }
    observer->mThresholds.Sort();
  } else {
    double thresh = aOptions.mThreshold.GetAsDouble();
    if (thresh < 0.0 || thresh > 1.0) {
      aRv.ThrowRangeError<dom::MSG_THRESHOLD_RANGE_ERROR>();
      return nullptr;
    }
    observer->mThresholds.AppendElement(thresh);
  }

  return observer.forget();
}

static void LazyLoadCallback(
    const Sequence<OwningNonNull<DOMIntersectionObserverEntry>>& aEntries) {
  for (const auto& entry : aEntries) {
    MOZ_ASSERT(entry->Target()->IsHTMLElement(nsGkAtoms::img));
    if (entry->IsIntersecting()) {
      static_cast<HTMLImageElement*>(entry->Target())
          ->StopLazyLoading(HTMLImageElement::FromIntersectionObserver::Yes,
                            HTMLImageElement::StartLoading::Yes);
    }
  }
}

static void LazyLoadCallbackReachViewport(
    const Sequence<OwningNonNull<DOMIntersectionObserverEntry>>& aEntries) {
  for (const auto& entry : aEntries) {
    MOZ_ASSERT(entry->Target()->IsHTMLElement(nsGkAtoms::img));
    if (entry->IsIntersecting()) {
      static_cast<HTMLImageElement*>(entry->Target())
          ->LazyLoadImageReachedViewport();
    }
  }
}

static LengthPercentage PrefMargin(float aValue, bool aIsPercentage) {
  return aIsPercentage ? LengthPercentage::FromPercentage(aValue / 100.0f)
                       : LengthPercentage::FromPixels(aValue);
}

DOMIntersectionObserver::DOMIntersectionObserver(Document& aDocument,
                                                 NativeCallback aCallback)
    : mOwner(aDocument.GetInnerWindow()),
      mDocument(&aDocument),
      mCallback(aCallback),
      mConnected(false) {}

already_AddRefed<DOMIntersectionObserver>
DOMIntersectionObserver::CreateLazyLoadObserver(Document& aDocument) {
  RefPtr<DOMIntersectionObserver> observer =
      new DOMIntersectionObserver(aDocument, LazyLoadCallback);
  observer->mThresholds.AppendElement(std::numeric_limits<double>::min());

#define SET_MARGIN(side_, side_lower_)                                 \
  observer->mRootMargin.Get(eSide##side_) = PrefMargin(                \
      StaticPrefs::dom_image_lazy_loading_root_margin_##side_lower_(), \
      StaticPrefs::                                                    \
          dom_image_lazy_loading_root_margin_##side_lower_##_percentage());
  SET_MARGIN(Top, top);
  SET_MARGIN(Right, right);
  SET_MARGIN(Bottom, bottom);
  SET_MARGIN(Left, left);
#undef SET_MARGIN

  return observer.forget();
}

already_AddRefed<DOMIntersectionObserver>
DOMIntersectionObserver::CreateLazyLoadObserverViewport(Document& aDocument) {
  RefPtr<DOMIntersectionObserver> observer =
      new DOMIntersectionObserver(aDocument, LazyLoadCallbackReachViewport);
  observer->mThresholds.AppendElement(std::numeric_limits<double>::min());
  return observer.forget();
}

bool DOMIntersectionObserver::SetRootMargin(const nsACString& aString) {
  return Servo_IntersectionObserverRootMargin_Parse(&aString, &mRootMargin);
}

nsISupports* DOMIntersectionObserver::GetParentObject() const { return mOwner; }

void DOMIntersectionObserver::GetRootMargin(nsACString& aRetVal) {
  Servo_IntersectionObserverRootMargin_ToString(&mRootMargin, &aRetVal);
}

void DOMIntersectionObserver::GetThresholds(nsTArray<double>& aRetVal) {
  aRetVal = mThresholds.Clone();
}

void DOMIntersectionObserver::Observe(Element& aTarget) {
  if (mObservationTargets.Contains(&aTarget)) {
    return;
  }
  aTarget.RegisterIntersectionObserver(this);
  mObservationTargets.AppendElement(&aTarget);
  Connect();
  if (mDocument) {
    if (nsPresContext* pc = mDocument->GetPresContext()) {
      pc->RefreshDriver()->EnsureIntersectionObservationsUpdateHappens();
    }
  }
}

void DOMIntersectionObserver::Unobserve(Element& aTarget) {
  if (!mObservationTargets.Contains(&aTarget)) {
    return;
  }

  if (mObservationTargets.Length() == 1) {
    Disconnect();
    return;
  }

  mObservationTargets.RemoveElement(&aTarget);
  aTarget.UnregisterIntersectionObserver(this);
}

void DOMIntersectionObserver::UnlinkTarget(Element& aTarget) {
  mObservationTargets.RemoveElement(&aTarget);
  if (mObservationTargets.Length() == 0) {
    Disconnect();
  }
}

void DOMIntersectionObserver::Connect() {
  if (mConnected) {
    return;
  }

  mConnected = true;
  if (mDocument) {
    mDocument->AddIntersectionObserver(this);
  }
}

void DOMIntersectionObserver::Disconnect() {
  if (!mConnected) {
    return;
  }

  mConnected = false;
  for (size_t i = 0; i < mObservationTargets.Length(); ++i) {
    Element* target = mObservationTargets.ElementAt(i);
    target->UnregisterIntersectionObserver(this);
  }
  mObservationTargets.Clear();
  if (mDocument) {
    mDocument->RemoveIntersectionObserver(this);
  }
}

void DOMIntersectionObserver::TakeRecords(
    nsTArray<RefPtr<DOMIntersectionObserverEntry>>& aRetVal) {
  aRetVal = std::move(mQueuedEntries);
}

static Maybe<nsRect> EdgeInclusiveIntersection(const nsRect& aRect,
                                               const nsRect& aOtherRect) {
  nscoord left = std::max(aRect.x, aOtherRect.x);
  nscoord top = std::max(aRect.y, aOtherRect.y);
  nscoord right = std::min(aRect.XMost(), aOtherRect.XMost());
  nscoord bottom = std::min(aRect.YMost(), aOtherRect.YMost());
  if (left > right || top > bottom) {
    return Nothing();
  }
  return Some(nsRect(left, top, right - left, bottom - top));
}

enum class BrowsingContextOrigin { Similar, Different };

// NOTE(emilio): Checking docgroup as per discussion in:
// https://github.com/w3c/IntersectionObserver/issues/161
static BrowsingContextOrigin SimilarOrigin(const Element& aTarget,
                                           const nsINode* aRoot) {
  if (!aRoot) {
    return BrowsingContextOrigin::Different;
  }
  return aTarget.OwnerDoc()->GetDocGroup() == aRoot->OwnerDoc()->GetDocGroup()
             ? BrowsingContextOrigin::Similar
             : BrowsingContextOrigin::Different;
}

// NOTE: This returns nullptr if |aDocument| is in another process from the top
// level content document.
static Document* GetTopLevelContentDocumentInThisProcess(Document& aDocument) {
  auto* wc = aDocument.GetTopLevelWindowContext();
  return wc ? wc->GetExtantDoc() : nullptr;
}

// https://w3c.github.io/IntersectionObserver/#compute-the-intersection
//
// TODO(emilio): Proof of this being equivalent to the spec welcome, seems
// reasonably close.
//
// Also, it's unclear to me why the spec talks about browsing context while
// discarding observations of targets of different documents.
//
// Both aRootBounds and the return value are relative to
// nsLayoutUtils::GetContainingBlockForClientRect(aRoot).
//
// In case of out-of-process document, aRemoteDocumentVisibleRect is a rectangle
// in the out-of-process document's coordinate system.
static Maybe<nsRect> ComputeTheIntersection(
    nsIFrame* aTarget, nsIFrame* aRoot, const nsRect& aRootBounds,
    const Maybe<nsRect>& aRemoteDocumentVisibleRect) {
  nsIFrame* target = aTarget;
  // 1. Let intersectionRect be the result of running the
  // getBoundingClientRect() algorithm on the target.
  //
  // `intersectionRect` is kept relative to `target` during the loop.
  Maybe<nsRect> intersectionRect = Some(nsLayoutUtils::GetAllInFlowRectsUnion(
      target, target, nsLayoutUtils::RECTS_ACCOUNT_FOR_TRANSFORMS));

  // 2. Let container be the containing block of the target.
  // (We go through the parent chain and only look at scroll frames)
  //
  // FIXME(emilio): Spec uses containing blocks, we use scroll frames, but we
  // only apply overflow-clipping, not clip-path, so it's ~fine. We do need to
  // apply clip-path.
  //
  // 3. While container is not the intersection root:
  nsIFrame* containerFrame =
      nsLayoutUtils::GetCrossDocParentFrameInProcess(target);
  while (containerFrame && containerFrame != aRoot) {
    // FIXME(emilio): What about other scroll frames that inherit from
    // nsHTMLScrollFrame but have a different type, like nsListControlFrame?
    // This looks bogus in that case, but different bug.
    if (nsIScrollableFrame* scrollFrame = do_QueryFrame(containerFrame)) {
      if (containerFrame->GetParent() == aRoot && !aRoot->GetParent()) {
        // This is subtle: if we're computing the intersection against the
        // viewport (the root frame), and this is its scroll frame, we really
        // want to skip this intersection (because we want to account for the
        // root margin, which is already in aRootBounds).
        break;
      }
      nsRect subFrameRect = scrollFrame->GetScrollPortRect();

      // 3.1 Map intersectionRect to the coordinate space of container.
      nsRect intersectionRectRelativeToContainer =
          nsLayoutUtils::TransformFrameRectToAncestor(
              target, intersectionRect.value(), containerFrame);

      // 3.2 If container has overflow clipping or a css clip-path property,
      // update intersectionRect by applying container's clip.
      //
      // TODO: Apply clip-path.
      //
      // 3.3 is handled, looks like, by this same clipping, given the root
      // scroll-frame cannot escape the viewport, probably?
      //
      intersectionRect = EdgeInclusiveIntersection(
          intersectionRectRelativeToContainer, subFrameRect);
      if (!intersectionRect) {
        return Nothing();
      }
      target = containerFrame;
    }

    containerFrame =
        nsLayoutUtils::GetCrossDocParentFrameInProcess(containerFrame);
  }
  MOZ_ASSERT(intersectionRect);

  // 4. Map intersectionRect to the coordinate space of the intersection root.
  nsRect intersectionRectRelativeToRoot =
      nsLayoutUtils::TransformFrameRectToAncestor(
          target, intersectionRect.value(),
          nsLayoutUtils::GetContainingBlockForClientRect(aRoot));

  // 5.Update intersectionRect by intersecting it with the root intersection
  // rectangle.
  intersectionRect =
      EdgeInclusiveIntersection(intersectionRectRelativeToRoot, aRootBounds);
  if (intersectionRect.isNothing()) {
    return Nothing();
  }
  // 6. Map intersectionRect to the coordinate space of the viewport of the
  // Document containing the target.
  //
  // FIXME(emilio): I think this may not be correct if the root is explicit
  // and in the same document, since then the rectangle may not be relative to
  // the viewport already (but it's in the same document).
  nsRect rect = intersectionRect.value();
  if (aTarget->PresContext() != aRoot->PresContext()) {
    if (nsIFrame* rootScrollFrame =
            aTarget->PresShell()->GetRootScrollFrame()) {
      nsLayoutUtils::TransformRect(aRoot, rootScrollFrame, rect);
    }
  }

  // In out-of-process iframes we need to take an intersection with the remote
  // document visible rect which was already clipped by ancestor document's
  // viewports.
  if (aRemoteDocumentVisibleRect) {
    MOZ_ASSERT(aRoot->PresContext()->IsRootContentDocumentInProcess() &&
               !aRoot->PresContext()->IsRootContentDocumentCrossProcess());

    intersectionRect =
        EdgeInclusiveIntersection(rect, *aRemoteDocumentVisibleRect);
    if (intersectionRect.isNothing()) {
      return Nothing();
    }
    rect = intersectionRect.value();
  }

  return Some(rect);
}

struct OopIframeMetrics {
  nsIFrame* mInProcessRootFrame = nullptr;
  nsRect mInProcessRootRect;
  nsRect mRemoteDocumentVisibleRect;
};

static Maybe<OopIframeMetrics> GetOopIframeMetrics(Document& aDocument,
                                                   Document* aRootDocument) {
  Document* rootDoc =
      nsContentUtils::GetInProcessSubtreeRootDocument(&aDocument);
  MOZ_ASSERT(rootDoc);

  if (rootDoc->IsTopLevelContentDocument()) {
    return Nothing();
  }

  if (aRootDocument &&
      rootDoc ==
          nsContentUtils::GetInProcessSubtreeRootDocument(aRootDocument)) {
    // aRootDoc, if non-null, is either the implicit root
    // (top-level-content-document) or a same-origin document passed explicitly.
    //
    // In the former case, we should've returned above if there are no iframes
    // in between. This condition handles the explicit, same-origin root
    // document, when both are embedded in an OOP iframe.
    return Nothing();
  }

  PresShell* rootPresShell = rootDoc->GetPresShell();
  if (!rootPresShell || rootPresShell->IsDestroying()) {
    return Some(OopIframeMetrics{});
  }

  nsIFrame* inProcessRootFrame = rootPresShell->GetRootFrame();
  if (!inProcessRootFrame) {
    return Some(OopIframeMetrics{});
  }

  BrowserChild* browserChild = BrowserChild::GetFrom(rootDoc->GetDocShell());
  if (!browserChild) {
    return Some(OopIframeMetrics{});
  }
  MOZ_DIAGNOSTIC_ASSERT(!browserChild->IsTopLevel());

  nsRect inProcessRootRect;
  if (nsIScrollableFrame* scrollFrame =
          rootPresShell->GetRootScrollFrameAsScrollable()) {
    inProcessRootRect = scrollFrame->GetScrollPortRect();
  }

  Maybe<LayoutDeviceRect> remoteDocumentVisibleRect =
      browserChild->GetTopLevelViewportVisibleRectInSelfCoords();
  if (!remoteDocumentVisibleRect) {
    return Some(OopIframeMetrics{});
  }

  return Some(OopIframeMetrics{
      inProcessRootFrame,
      inProcessRootRect,
      LayoutDeviceRect::ToAppUnits(
          *remoteDocumentVisibleRect,
          rootPresShell->GetPresContext()->AppUnitsPerDevPixel()),
  });
}

// https://w3c.github.io/IntersectionObserver/#update-intersection-observations-algo
// (step 2)
void DOMIntersectionObserver::Update(Document* aDocument,
                                     DOMHighResTimeStamp time) {
  // 1 - Let rootBounds be observer's root intersection rectangle.
  //  ... but since the intersection rectangle depends on the target, we defer
  //      the inflation until later.
  // NOTE: |rootRect| and |rootFrame| will be root in the same process. In
  // out-of-process iframes, they are NOT root ones of the top level content
  // document.
  nsRect rootRect;
  nsIFrame* rootFrame = nullptr;
  nsINode* root = mRoot;
  Maybe<nsRect> remoteDocumentVisibleRect;
  if (mRoot && mRoot->IsElement()) {
    if ((rootFrame = mRoot->AsElement()->GetPrimaryFrame())) {
      nsRect rootRectRelativeToRootFrame;
      if (nsIScrollableFrame* scrollFrame = do_QueryFrame(rootFrame)) {
        // rootRectRelativeToRootFrame should be the content rect of rootFrame,
        // not including the scrollbars.
        rootRectRelativeToRootFrame = scrollFrame->GetScrollPortRect();
      } else {
        // rootRectRelativeToRootFrame should be the border rect of rootFrame.
        rootRectRelativeToRootFrame = rootFrame->GetRectRelativeToSelf();
      }
      nsIFrame* containingBlock =
          nsLayoutUtils::GetContainingBlockForClientRect(rootFrame);
      rootRect = nsLayoutUtils::TransformFrameRectToAncestor(
          rootFrame, rootRectRelativeToRootFrame, containingBlock);
    }
  } else {
    MOZ_ASSERT(!mRoot || mRoot->IsDocument());
    Document* rootDocument =
        mRoot ? mRoot->AsDocument()
              : GetTopLevelContentDocumentInThisProcess(*aDocument);
    root = rootDocument;

    if (rootDocument) {
      // We're in the same process as the root document, though note that there
      // could be an out-of-process iframe in between us and the root. Grab the
      // root frame and the root rect.
      //
      // Note that the root rect is always good (we assume no DPI changes in
      // between the two documents, and we don't need to convert coordinates).
      //
      // The root frame however we may need to tweak in the block below, if
      // there's any OOP iframe in between `rootDocument` and `aDocument`, to
      // handle the OOP iframe positions.
      if (PresShell* presShell = rootDocument->GetPresShell()) {
        rootFrame = presShell->GetRootFrame();
        // We use the root scrollable frame's scroll port to account the
        // scrollbars in rootRect, if needed.
        if (nsIScrollableFrame* scrollFrame =
                presShell->GetRootScrollFrameAsScrollable()) {
          rootRect = scrollFrame->GetScrollPortRect();
        }
      }
    }

    if (Maybe<OopIframeMetrics> metrics =
            GetOopIframeMetrics(*aDocument, rootDocument)) {
      rootFrame = metrics->mInProcessRootFrame;
      if (!rootDocument) {
        rootRect = metrics->mInProcessRootRect;
      }
      remoteDocumentVisibleRect = Some(metrics->mRemoteDocumentVisibleRect);
    }
  }

  nsMargin rootMargin;  // This root margin is NOT applied in `implicit root`
                        // case, e.g. in out-of-process iframes.
  for (const auto side : mozilla::AllPhysicalSides()) {
    nscoord basis = side == eSideTop || side == eSideBottom ? rootRect.Height()
                                                            : rootRect.Width();
    rootMargin.Side(side) =
        mRootMargin.Get(side).Resolve(basis, NSToCoordRoundWithClamp);
  }

  // 2. For each target in observer’s internal [[ObservationTargets]] slot,
  // processed in the same order that observe() was called on each target:
  for (Element* target : mObservationTargets) {
    nsIFrame* targetFrame = target->GetPrimaryFrame();
    BrowsingContextOrigin origin = SimilarOrigin(*target, root);

    Maybe<nsRect> intersectionRect;
    nsRect targetRect;
    nsRect rootBounds;

    const bool canComputeIntersection = [&] {
      if (!targetFrame || !rootFrame) {
        return false;
      }

      // 2.1. If the intersection root is not the implicit root and target is
      // not a descendant of the intersection root in the containing block
      // chain, skip further processing for target.
      //
      // NOTE(emilio): We don't just "skip further processing" because that
      // violates the invariant that there's at least one observation for a
      // target (though that is also violated by 2.2), but it also causes
      // different behavior when `target` is `display: none`, or not, which is
      // really really odd, see:
      // https://github.com/w3c/IntersectionObserver/issues/457
      //
      // NOTE(emilio): We also do this if target is the implicit root, pending
      // clarification in
      // https://github.com/w3c/IntersectionObserver/issues/456.
      if (rootFrame == targetFrame ||
          !nsLayoutUtils::IsAncestorFrameCrossDoc(rootFrame, targetFrame)) {
        return false;
      }

      // 2.2. If the intersection root is not the implicit root, and target is
      // not in the same Document as the intersection root, skip further
      // processing for target.
      //
      // NOTE(emilio): We don't just "skip further processing", because that
      // doesn't match reality and other browsers, see
      // https://github.com/w3c/IntersectionObserver/issues/457.
      if (mRoot && mRoot->OwnerDoc() != target->OwnerDoc()) {
        return false;
      }

      return true;
    }();

    if (canComputeIntersection) {
      rootBounds = rootRect;
      if (origin == BrowsingContextOrigin::Similar) {
        rootBounds.Inflate(rootMargin);
      }

      // 2.3. Let targetRect be a DOMRectReadOnly obtained by running the
      // getBoundingClientRect() algorithm on target.
      targetRect = targetFrame->GetBoundingClientRect();

      // 2.4. Let intersectionRect be the result of running the compute the
      // intersection algorithm on target.
      intersectionRect = ComputeTheIntersection(
          targetFrame, rootFrame, rootBounds, remoteDocumentVisibleRect);
    }

    // 2.5. Let targetArea be targetRect’s area.
    int64_t targetArea =
        (int64_t)targetRect.Width() * (int64_t)targetRect.Height();
    // 2.6. Let intersectionArea be intersectionRect’s area.
    int64_t intersectionArea = !intersectionRect
                                   ? 0
                                   : (int64_t)intersectionRect->Width() *
                                         (int64_t)intersectionRect->Height();

    // 2.7. Let isIntersecting be true if targetRect and rootBounds intersect or
    // are edge-adjacent, even if the intersection has zero area (because
    // rootBounds or targetRect have zero area); otherwise, let isIntersecting
    // be false.
    const bool isIntersecting = intersectionRect.isSome();

    // 2.8. If targetArea is non-zero, let intersectionRatio be intersectionArea
    // divided by targetArea. Otherwise, let intersectionRatio be 1 if
    // isIntersecting is true, or 0 if isIntersecting is false.
    double intersectionRatio;
    if (targetArea > 0.0) {
      intersectionRatio =
          std::min((double)intersectionArea / (double)targetArea, 1.0);
    } else {
      intersectionRatio = isIntersecting ? 1.0 : 0.0;
    }

    // 2.9 Let thresholdIndex be the index of the first entry in
    // observer.thresholds whose value is greater than intersectionRatio, or the
    // length of observer.thresholds if intersectionRatio is greater than or
    // equal to the last entry in observer.thresholds.
    int32_t thresholdIndex = -1;

    // If not intersecting, we can just shortcut, as we know that the thresholds
    // are always between 0 and 1.
    if (isIntersecting) {
      thresholdIndex = mThresholds.IndexOfFirstElementGt(intersectionRatio);
      if (thresholdIndex == 0) {
        // Per the spec, we should leave threshold at 0 and distinguish between
        // "less than all thresholds and intersecting" and "not intersecting"
        // (queuing observer entries as both cases come to pass). However,
        // neither Chrome nor the WPT tests expect this behavior, so treat these
        // two cases as one.
        //
        // See https://github.com/w3c/IntersectionObserver/issues/432 about
        // this.
        thresholdIndex = -1;
      }
    }

    // Steps 2.10 - 2.15.
    if (target->UpdateIntersectionObservation(this, thresholdIndex)) {
      // See https://github.com/w3c/IntersectionObserver/issues/432 about
      // why we use thresholdIndex > 0 rather than isIntersecting for the
      // entry's isIntersecting value.
      QueueIntersectionObserverEntry(
          target, time,
          origin == BrowsingContextOrigin::Similar ? Some(rootBounds)
                                                   : Nothing(),
          targetRect, intersectionRect, thresholdIndex > 0, intersectionRatio);
    }
  }
}

void DOMIntersectionObserver::QueueIntersectionObserverEntry(
    Element* aTarget, DOMHighResTimeStamp time, const Maybe<nsRect>& aRootRect,
    const nsRect& aTargetRect, const Maybe<nsRect>& aIntersectionRect,
    bool aIsIntersecting, double aIntersectionRatio) {
  RefPtr<DOMRect> rootBounds;
  if (aRootRect.isSome()) {
    rootBounds = new DOMRect(this);
    rootBounds->SetLayoutRect(aRootRect.value());
  }
  RefPtr<DOMRect> boundingClientRect = new DOMRect(this);
  boundingClientRect->SetLayoutRect(aTargetRect);
  RefPtr<DOMRect> intersectionRect = new DOMRect(this);
  if (aIntersectionRect.isSome()) {
    intersectionRect->SetLayoutRect(aIntersectionRect.value());
  }
  RefPtr<DOMIntersectionObserverEntry> entry = new DOMIntersectionObserverEntry(
      this, time, rootBounds.forget(), boundingClientRect.forget(),
      intersectionRect.forget(), aIsIntersecting, aTarget, aIntersectionRatio);
  mQueuedEntries.AppendElement(entry.forget());
}

void DOMIntersectionObserver::Notify() {
  if (!mQueuedEntries.Length()) {
    return;
  }
  Sequence<OwningNonNull<DOMIntersectionObserverEntry>> entries;
  if (entries.SetCapacity(mQueuedEntries.Length(), mozilla::fallible)) {
    for (size_t i = 0; i < mQueuedEntries.Length(); ++i) {
      RefPtr<DOMIntersectionObserverEntry> next = mQueuedEntries[i];
      *entries.AppendElement(mozilla::fallible) = next;
    }
  }
  mQueuedEntries.Clear();

  if (mCallback.is<RefPtr<dom::IntersectionCallback>>()) {
    RefPtr<dom::IntersectionCallback> callback(
        mCallback.as<RefPtr<dom::IntersectionCallback>>());
    callback->Call(this, entries, *this);
  } else {
    mCallback.as<NativeCallback>()(entries);
  }
}

}  // namespace mozilla::dom
