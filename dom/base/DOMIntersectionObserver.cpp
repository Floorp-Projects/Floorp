/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMIntersectionObserver.h"
#include "nsCSSPropertyID.h"
#include "nsIFrame.h"
#include "nsContentUtils.h"
#include "nsLayoutUtils.h"
#include "mozilla/PresShell.h"
#include "mozilla/ServoBindings.h"

namespace mozilla {
namespace dom {

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
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCallback)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mRoot)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mQueuedEntries)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(DOMIntersectionObserver)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOwner)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocument)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCallback)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRoot)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mQueuedEntries)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

already_AddRefed<DOMIntersectionObserver> DOMIntersectionObserver::Constructor(
    const mozilla::dom::GlobalObject& aGlobal,
    mozilla::dom::IntersectionCallback& aCb, mozilla::ErrorResult& aRv) {
  return Constructor(aGlobal, aCb, IntersectionObserverInit(), aRv);
}

already_AddRefed<DOMIntersectionObserver> DOMIntersectionObserver::Constructor(
    const mozilla::dom::GlobalObject& aGlobal,
    mozilla::dom::IntersectionCallback& aCb,
    const mozilla::dom::IntersectionObserverInit& aOptions,
    mozilla::ErrorResult& aRv) {
  nsCOMPtr<nsPIDOMWindowInner> window =
      do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  RefPtr<DOMIntersectionObserver> observer =
      new DOMIntersectionObserver(window.forget(), aCb);

  observer->mRoot = aOptions.mRoot;

  if (!observer->SetRootMargin(aOptions.mRootMargin)) {
    aRv.ThrowDOMException(
        NS_ERROR_DOM_SYNTAX_ERR,
        NS_LITERAL_CSTRING(
            "rootMargin must be specified in pixels or percent."));
    return nullptr;
  }

  if (aOptions.mThreshold.IsDoubleSequence()) {
    const mozilla::dom::Sequence<double>& thresholds =
        aOptions.mThreshold.GetAsDoubleSequence();
    observer->mThresholds.SetCapacity(thresholds.Length());
    for (const auto& thresh : thresholds) {
      if (thresh < 0.0 || thresh > 1.0) {
        aRv.ThrowTypeError<dom::MSG_THRESHOLD_RANGE_ERROR>();
        return nullptr;
      }
      observer->mThresholds.AppendElement(thresh);
    }
    observer->mThresholds.Sort();
  } else {
    double thresh = aOptions.mThreshold.GetAsDouble();
    if (thresh < 0.0 || thresh > 1.0) {
      aRv.ThrowTypeError<dom::MSG_THRESHOLD_RANGE_ERROR>();
      return nullptr;
    }
    observer->mThresholds.AppendElement(thresh);
  }

  return observer.forget();
}

bool DOMIntersectionObserver::SetRootMargin(const nsAString& aString) {
  return Servo_IntersectionObserverRootMargin_Parse(&aString, &mRootMargin);
}

void DOMIntersectionObserver::GetRootMargin(mozilla::dom::DOMString& aRetVal) {
  nsString& retVal = aRetVal;
  Servo_IntersectionObserverRootMargin_ToString(&mRootMargin, &retVal);
}

void DOMIntersectionObserver::GetThresholds(nsTArray<double>& aRetVal) {
  aRetVal = mThresholds;
}

void DOMIntersectionObserver::Observe(Element& aTarget) {
  if (mObservationTargets.Contains(&aTarget)) {
    return;
  }
  aTarget.RegisterIntersectionObserver(this);
  mObservationTargets.AppendElement(&aTarget);
  Connect();
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
  aRetVal.SwapElements(mQueuedEntries);
  mQueuedEntries.Clear();
}

static bool CheckSimilarOrigin(nsINode* aNode1, nsINode* aNode2) {
  nsIPrincipal* principal1 = aNode1->NodePrincipal();
  nsIPrincipal* principal2 = aNode2->NodePrincipal();
  nsAutoCString baseDomain1;
  nsAutoCString baseDomain2;

  nsresult rv = principal1->GetBaseDomain(baseDomain1);
  if (NS_FAILED(rv)) {
    return principal1 == principal2;
  }

  rv = principal2->GetBaseDomain(baseDomain2);
  if (NS_FAILED(rv)) {
    return principal1 == principal2;
  }

  return baseDomain1 == baseDomain2;
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

enum class BrowsingContextInfo {
  SimilarOriginBrowsingContext,
  DifferentOriginBrowsingContext,
  UnknownBrowsingContext
};

void DOMIntersectionObserver::Update(Document* aDocument,
                                     DOMHighResTimeStamp time) {
  Element* root = nullptr;
  nsIFrame* rootFrame = nullptr;
  nsRect rootRect;

  if (mRoot) {
    root = mRoot;
    rootFrame = root->GetPrimaryFrame();
    if (rootFrame) {
      nsRect rootRectRelativeToRootFrame;
      if (rootFrame->IsScrollFrame()) {
        // rootRectRelativeToRootFrame should be the content rect of rootFrame,
        // not including the scrollbars.
        nsIScrollableFrame* scrollFrame = do_QueryFrame(rootFrame);
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
    RefPtr<PresShell> presShell = aDocument->GetPresShell();
    if (presShell) {
      rootFrame = presShell->GetRootScrollFrame();
      if (rootFrame) {
        nsPresContext* presContext = rootFrame->PresContext();
        while (!presContext->IsRootContentDocument()) {
          presContext = presContext->GetParentPresContext();
          if (!presContext) {
            break;
          }
          nsIFrame* rootScrollFrame =
              presContext->PresShell()->GetRootScrollFrame();
          if (rootScrollFrame) {
            rootFrame = rootScrollFrame;
          } else {
            break;
          }
        }
        root = rootFrame->GetContent()->AsElement();
        nsIScrollableFrame* scrollFrame = do_QueryFrame(rootFrame);
        // If we end up with a null root frame for some reason, we'll proceed
        // with an empty root intersection rect.
        if (scrollFrame) {
          rootRect = scrollFrame->GetScrollPortRect();
        }
      }
    }
  }

  nsMargin rootMargin;
  NS_FOR_CSS_SIDES(side) {
    nscoord basis = side == eSideTop || side == eSideBottom ? rootRect.Height()
                                                            : rootRect.Width();
    rootMargin.Side(side) =
        nsLayoutUtils::ComputeCBDependentValue(basis, mRootMargin.Get(side));
  }

  for (size_t i = 0; i < mObservationTargets.Length(); ++i) {
    Element* target = mObservationTargets.ElementAt(i);
    nsIFrame* targetFrame = target->GetPrimaryFrame();
    nsIFrame* originalTargetFrame = targetFrame;
    nsRect targetRect;
    Maybe<nsRect> intersectionRect;
    bool isSameDoc = root && root->GetComposedDoc() == target->GetComposedDoc();

    if (rootFrame && targetFrame) {
      // If mRoot is set we are testing intersection with a container element
      // instead of the implicit root.
      if (mRoot) {
        // Skip further processing of this target if it is not in the same
        // Document as the intersection root, e.g. if root is an element of
        // the main document and target an element from an embedded iframe.
        if (!isSameDoc) {
          continue;
        }
        // Skip further processing of this target if is not a descendant of the
        // intersection root in the containing block chain. E.g. this would be
        // the case if the target is in a position:absolute element whose
        // containing block is an ancestor of root.
        if (!nsLayoutUtils::IsAncestorFrameCrossDoc(rootFrame, targetFrame)) {
          continue;
        }
      }

      targetRect = nsLayoutUtils::GetAllInFlowRectsUnion(
          targetFrame,
          nsLayoutUtils::GetContainingBlockForClientRect(targetFrame),
          nsLayoutUtils::RECTS_ACCOUNT_FOR_TRANSFORMS);
      intersectionRect = Some(targetFrame->GetRectRelativeToSelf());

      nsIFrame* containerFrame =
          nsLayoutUtils::GetCrossDocParentFrame(targetFrame);
      while (containerFrame && containerFrame != rootFrame) {
        if (containerFrame->IsScrollFrame()) {
          nsIScrollableFrame* scrollFrame = do_QueryFrame(containerFrame);
          nsRect subFrameRect = scrollFrame->GetScrollPortRect();
          nsRect intersectionRectRelativeToContainer =
              nsLayoutUtils::TransformFrameRectToAncestor(
                  targetFrame, intersectionRect.value(), containerFrame);
          intersectionRect = EdgeInclusiveIntersection(
              intersectionRectRelativeToContainer, subFrameRect);
          if (!intersectionRect) {
            break;
          }
          targetFrame = containerFrame;
        }

        // TODO: Apply clip-path.

        containerFrame = nsLayoutUtils::GetCrossDocParentFrame(containerFrame);
      }
    }

    nsRect rootIntersectionRect;
    BrowsingContextInfo isInSimilarOriginBrowsingContext =
        BrowsingContextInfo::UnknownBrowsingContext;

    if (rootFrame && targetFrame) {
      rootIntersectionRect = rootRect;
    }

    if (root && target) {
      isInSimilarOriginBrowsingContext =
          CheckSimilarOrigin(root, target)
              ? BrowsingContextInfo::SimilarOriginBrowsingContext
              : BrowsingContextInfo::DifferentOriginBrowsingContext;
    }

    if (isInSimilarOriginBrowsingContext ==
        BrowsingContextInfo::SimilarOriginBrowsingContext) {
      rootIntersectionRect.Inflate(rootMargin);
    }

    if (intersectionRect.isSome()) {
      nsRect intersectionRectRelativeToRoot =
          nsLayoutUtils::TransformFrameRectToAncestor(
              targetFrame, intersectionRect.value(),
              nsLayoutUtils::GetContainingBlockForClientRect(rootFrame));
      intersectionRect = EdgeInclusiveIntersection(
          intersectionRectRelativeToRoot, rootIntersectionRect);
      if (intersectionRect.isSome() && !isSameDoc) {
        nsRect rect = intersectionRect.value();
        nsPresContext* presContext = originalTargetFrame->PresContext();
        nsIFrame* rootScrollFrame =
            presContext->PresShell()->GetRootScrollFrame();
        if (rootScrollFrame) {
          nsLayoutUtils::TransformRect(rootFrame, rootScrollFrame, rect);
        }
        intersectionRect = Some(rect);
      }
    }

    int64_t targetArea =
        (int64_t)targetRect.Width() * (int64_t)targetRect.Height();
    int64_t intersectionArea = !intersectionRect
                                   ? 0
                                   : (int64_t)intersectionRect->Width() *
                                         (int64_t)intersectionRect->Height();

    double intersectionRatio;
    if (targetArea > 0.0) {
      intersectionRatio =
          std::min((double)intersectionArea / (double)targetArea, 1.0);
    } else {
      intersectionRatio = intersectionRect.isSome() ? 1.0 : 0.0;
    }

    int32_t threshold = -1;
    if (intersectionRect.isSome()) {
      // Spec: "Let thresholdIndex be the index of the first entry in
      // observer.thresholds whose value is greater than intersectionRatio."
      threshold = mThresholds.IndexOfFirstElementGt(intersectionRatio);
      if (threshold == 0) {
        // Per the spec, we should leave threshold at 0 and distinguish between
        // "less than all thresholds and intersecting" and "not intersecting"
        // (queuing observer entries as both cases come to pass). However,
        // neither Chrome nor the WPT tests expect this behavior, so treat these
        // two cases as one.
        threshold = -1;
      }
    }

    if (target->UpdateIntersectionObservation(this, threshold)) {
      QueueIntersectionObserverEntry(
          target, time,
          isInSimilarOriginBrowsingContext ==
                  BrowsingContextInfo::DifferentOriginBrowsingContext
              ? Nothing()
              : Some(rootIntersectionRect),
          targetRect, intersectionRect, intersectionRatio);
    }
  }
}

void DOMIntersectionObserver::QueueIntersectionObserverEntry(
    Element* aTarget, DOMHighResTimeStamp time, const Maybe<nsRect>& aRootRect,
    const nsRect& aTargetRect, const Maybe<nsRect>& aIntersectionRect,
    double aIntersectionRatio) {
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
      intersectionRect.forget(), aIntersectionRect.isSome(), aTarget,
      aIntersectionRatio);
  mQueuedEntries.AppendElement(entry.forget());
}

void DOMIntersectionObserver::Notify() {
  if (!mQueuedEntries.Length()) {
    return;
  }
  mozilla::dom::Sequence<mozilla::OwningNonNull<DOMIntersectionObserverEntry>>
      entries;
  if (entries.SetCapacity(mQueuedEntries.Length(), mozilla::fallible)) {
    for (size_t i = 0; i < mQueuedEntries.Length(); ++i) {
      RefPtr<DOMIntersectionObserverEntry> next = mQueuedEntries[i];
      *entries.AppendElement(mozilla::fallible) = next;
    }
  }
  mQueuedEntries.Clear();
  RefPtr<dom::IntersectionCallback> callback(mCallback);
  callback->Call(this, entries, *this);
}

}  // namespace dom
}  // namespace mozilla
