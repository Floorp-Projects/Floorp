/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ResizeObserver.h"

#include "mozilla/dom/Document.h"
#include "nsContentUtils.h"
#include "nsSVGUtils.h"
#include <limits>

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION(ResizeObservation, mTarget)
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(ResizeObservation, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(ResizeObservation, Release)

bool ResizeObservation::IsActive() const {
  nsRect rect = GetTargetRect();
  return (rect.width != mBroadcastWidth || rect.height != mBroadcastHeight);
}

void ResizeObservation::UpdateBroadcastSize(const nsSize& aSize) {
  mBroadcastWidth = aSize.width;
  mBroadcastHeight = aSize.height;
}

nsRect ResizeObservation::GetTargetRect() const {
  nsRect rect;
  nsIFrame* frame = mTarget->GetPrimaryFrame();

  if (frame) {
    if (mTarget->IsSVGElement()) {
      gfxRect bbox = nsSVGUtils::GetBBox(frame);
      rect.width = NSFloatPixelsToAppUnits(bbox.width, AppUnitsPerCSSPixel());
      rect.height = NSFloatPixelsToAppUnits(bbox.height, AppUnitsPerCSSPixel());
    } else {
      // Per the spec, non-replaced inline Elements will always have an empty
      // content rect. Therefore, we don't set rect for non-replaced inline
      // elements here, and their IsActive() will always return false.
      if (frame->IsFrameOfType(nsIFrame::eReplaced) ||
          !frame->IsFrameOfType(nsIFrame::eLineParticipant)) {
        rect = frame->GetContentRectRelativeToSelf();
      }
    }
  }

  return rect;
}

// Only needed for refcounted objects.
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ResizeObserver, mOwner, mCallback,
                                      mActiveTargets, mObservationMap)
NS_IMPL_CYCLE_COLLECTING_ADDREF(ResizeObserver)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ResizeObserver)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ResizeObserver)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

already_AddRefed<ResizeObserver> ResizeObserver::Constructor(
    const GlobalObject& aGlobal, ResizeObserverCallback& aCb,
    ErrorResult& aRv) {
  nsCOMPtr<nsPIDOMWindowInner> window =
      do_QueryInterface(aGlobal.GetAsSupports());

  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  Document* document = window->GetExtantDoc();

  if (!document) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<ResizeObserver> observer = new ResizeObserver(window.forget(), aCb);
  // TODO: Add the new ResizeObserver to document here in the later patch.

  return observer.forget();
}

void ResizeObserver::Observe(Element& aTarget, ErrorResult& aRv) {
  RefPtr<ResizeObservation> observation;

  if (mObservationMap.Get(&aTarget, getter_AddRefs(observation))) {
    // Already observed this target, so return.
    // Note: Based on the spec, we should unobserve it first. However, calling
    // Unobserve() will remove original ResizeObservation and then add a new
    // one, this may cause an unexpected result because ResizeObservation stores
    // the last reported size which should be kept to make sure IsActive()
    // returns the correct result.
    return;
  }

  observation = new ResizeObservation(aTarget);

  mObservationMap.Put(&aTarget, observation);
  mObservationList.insertBack(observation);

  // Per the spec, we need to trigger notification in event loop that
  // contains ResizeObserver observe call even when resize/reflow does
  // not happen.
  // TODO: Implement the notification scheduling in the later patch.
}

void ResizeObserver::Unobserve(Element& aTarget, ErrorResult& aRv) {
  RefPtr<ResizeObservation> observation;
  if (!mObservationMap.Remove(&aTarget, getter_AddRefs(observation))) {
    return;
  }

  MOZ_ASSERT(!mObservationList.isEmpty(),
             "If ResizeObservation found for an element, observation list "
             "must be not empty.");
  observation->remove();
}

void ResizeObserver::Disconnect() {
  mObservationList.clear();
  mObservationMap.Clear();
  mActiveTargets.Clear();
}

void ResizeObserver::GatherActiveObservations(uint32_t aDepth) {
  mActiveTargets.Clear();
  mHasSkippedTargets = false;

  for (auto observation : mObservationList) {
    if (!observation->IsActive()) {
      continue;
    }

    uint32_t targetDepth = nsContentUtils::GetNodeDepth(observation->Target());

    if (targetDepth > aDepth) {
      mActiveTargets.AppendElement(observation);
    } else {
      mHasSkippedTargets = true;
    }
  }
}

uint32_t ResizeObserver::BroadcastActiveObservations() {
  uint32_t shallowestTargetDepth = std::numeric_limits<uint32_t>::max();

  if (!HasActiveObservations()) {
    return shallowestTargetDepth;
  }

  Sequence<OwningNonNull<ResizeObserverEntry>> entries;

  for (auto& observation : mActiveTargets) {
    RefPtr<ResizeObserverEntry> entry =
        new ResizeObserverEntry(this, *observation->Target());

    nsRect rect = observation->GetTargetRect();
    entry->SetContentRect(rect);
    // FIXME: Bug 1545239: Set borderBoxSize and contentBoxSize.

    if (!entries.AppendElement(entry.forget(), fallible)) {
      // Out of memory.
      break;
    }

    // Sync the broadcast size of observation so the next size inspection
    // will be based on the updated size from last delivered observations.
    observation->UpdateBroadcastSize(rect.Size());

    uint32_t targetDepth = nsContentUtils::GetNodeDepth(observation->Target());

    if (targetDepth < shallowestTargetDepth) {
      shallowestTargetDepth = targetDepth;
    }
  }

  RefPtr<ResizeObserverCallback> callback(mCallback);
  callback->Call(this, entries, *this);

  mActiveTargets.Clear();
  mHasSkippedTargets = false;

  return shallowestTargetDepth;
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ResizeObserverEntry, mTarget,
                                      mContentRect, mOwner)
NS_IMPL_CYCLE_COLLECTING_ADDREF(ResizeObserverEntry)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ResizeObserverEntry)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ResizeObserverEntry)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

already_AddRefed<ResizeObserverEntry> ResizeObserverEntry::Constructor(
    const GlobalObject& aGlobal, Element& aTarget, ErrorResult& aRv) {
  RefPtr<ResizeObserverEntry> observerEntry =
      new ResizeObserverEntry(aGlobal.GetAsSupports(), aTarget);
  return observerEntry.forget();
}

void ResizeObserverEntry::SetContentRect(const nsRect& aRect) {
  nsIFrame* frame = mTarget->GetPrimaryFrame();
  Maybe<nsMargin> padding = frame ? Some(frame->GetUsedPadding()) : Nothing();

  // Per the spec, we need to include padding in contentRect of
  // ResizeObserverEntry.
  nsRect rect(padding ? padding->left : aRect.x,
              padding ? padding->top : aRect.y, aRect.width, aRect.height);

  RefPtr<DOMRect> contentRect = new DOMRect(mTarget);
  contentRect->SetLayoutRect(rect);
  mContentRect = contentRect.forget();
}

}  // namespace dom
}  // namespace mozilla
