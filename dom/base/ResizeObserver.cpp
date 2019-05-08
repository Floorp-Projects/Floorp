/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ResizeObserver.h"

#include "mozilla/dom/Document.h"
#include "nsIContent.h"
#include "nsSVGUtils.h"
#include <limits>

namespace mozilla {
namespace dom {

/**
 * Returns the length of the parent-traversal path (in terms of the number of
 * nodes) to an unparented/root node from aNode. An unparented/root node is
 * considered to have a depth of 1, its children have a depth of 2, etc.
 * aNode is expected to be non-null.
 * Note: The shadow root is not part of the calculation because the caller,
 * ResizeObserver, doesn't observe the shadow root, and only needs relative
 * depths among all the observed targets. In other words, we calculate the
 * depth of the flattened tree.
 *
 * However, these is a spec issue about how to handle shadow DOM case. We
 * may need to update this function later:
 *   https://github.com/w3c/csswg-drafts/issues/3840
 *
 * https://drafts.csswg.org/resize-observer/#calculate-depth-for-node-h
 */
static uint32_t GetNodeDepth(nsINode* aNode) {
  uint32_t depth = 1;

  MOZ_ASSERT(aNode, "Node shouldn't be null");

  // Use GetFlattenedTreeParentNode to bypass the shadow root and cross the
  // shadow boundary to calculate the node depth without the shadow root.
  while ((aNode = aNode->GetFlattenedTreeParentNode())) {
    ++depth;
  }

  return depth;
}

/**
 * Returns |aTarget|'s size in the form of nsSize.
 * If the target is SVG, width and height are determined from bounding box.
 */
static nsSize GetTargetSize(Element* aTarget, ResizeObserverBoxOptions aBox) {
  nsSize size;
  nsIFrame* frame = aTarget->GetPrimaryFrame();

  if (!frame) {
    return size;
  }

  if (aTarget->IsSVGElement()) {
    // Per the spec, SVG size is always its bounding box size no matter what
    // box option you choose, because SVG elements do not use standard CSS box
    // model.
    gfxRect bbox = nsSVGUtils::GetBBox(frame);
    size.width = NSFloatPixelsToAppUnits(bbox.width, AppUnitsPerCSSPixel());
    size.height = NSFloatPixelsToAppUnits(bbox.height, AppUnitsPerCSSPixel());
  } else {
    // Per the spec, non-replaced inline Elements will always have an empty
    // content rect. Therefore, we always use the same trivially-empty size
    // for non-replaced inline elements here, and their IsActive() will
    // always return false. (So its observation won't be fired.)
    if (!frame->IsFrameOfType(nsIFrame::eReplaced) &&
        frame->IsFrameOfType(nsIFrame::eLineParticipant)) {
      return size;
    }

    switch (aBox) {
      case ResizeObserverBoxOptions::Border_box:
        // GetSize() includes the content area, borders, and padding.
        size = frame->GetSize();
        break;
      case ResizeObserverBoxOptions::Content_box:
      default:
        size = frame->GetContentRectRelativeToSelf().Size();
    }
  }

  return size;
}

NS_IMPL_CYCLE_COLLECTION(ResizeObservation, mTarget)
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(ResizeObservation, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(ResizeObservation, Release)

bool ResizeObservation::IsActive() const {
  nsIFrame* frame = mTarget->GetPrimaryFrame();
  const WritingMode wm = frame ? frame->GetWritingMode() : WritingMode();
  const LogicalSize size(wm, GetTargetSize(mTarget, mObservedBox));
  return mLastReportedSize.ISize(mLastReportedWM) != size.ISize(wm) ||
         mLastReportedSize.BSize(mLastReportedWM) != size.BSize(wm);
}

void ResizeObservation::UpdateLastReportedSize(const nsSize& aSize) {
  nsIFrame* frame = mTarget->GetPrimaryFrame();
  mLastReportedWM = frame ? frame->GetWritingMode() : WritingMode();
  mLastReportedSize = LogicalSize(mLastReportedWM, aSize);
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
  document->AddResizeObserver(observer);

  return observer.forget();
}

void ResizeObserver::Observe(Element& aTarget,
                             const ResizeObserverOptions& aOptions,
                             ErrorResult& aRv) {
  RefPtr<ResizeObservation> observation;

  if (mObservationMap.Get(&aTarget, getter_AddRefs(observation))) {
    if (observation->BoxOptions() == aOptions.mBox) {
      // Already observed this target and the observed box is the same, so
      // return.
      // Note: Based on the spec, we should unobserve it first. However,
      // calling Unobserve() when we observe the same box will remove original
      // ResizeObservation and then add a new one, this may cause an unexpected
      // result because ResizeObservation stores the mLastReportedSize which
      // should be kept to make sure IsActive() returns the correct result.
      return;
    }
    Unobserve(aTarget, aRv);
  }

  nsIFrame* frame = aTarget.GetPrimaryFrame();
  observation = new ResizeObservation(
      aTarget, aOptions.mBox, frame ? frame->GetWritingMode() : WritingMode());

  mObservationMap.Put(&aTarget, observation);
  mObservationList.insertBack(observation);

  // Per the spec, we need to trigger notification in event loop that
  // contains ResizeObserver observe call even when resize/reflow does
  // not happen.
  aTarget.OwnerDoc()->ScheduleResizeObserversNotification();
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

    uint32_t targetDepth = GetNodeDepth(observation->Target());

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
    Element* target = observation->Target();
    RefPtr<ResizeObserverEntry> entry = new ResizeObserverEntry(this, *target);

    nsSize borderBoxSize =
        GetTargetSize(target, ResizeObserverBoxOptions::Border_box);
    entry->SetBorderBoxSize(borderBoxSize);

    nsSize contentBoxSize =
        GetTargetSize(target, ResizeObserverBoxOptions::Content_box);
    entry->SetContentRectAndSize(contentBoxSize);

    if (!entries.AppendElement(entry.forget(), fallible)) {
      // Out of memory.
      break;
    }

    // Sync the broadcast size of observation so the next size inspection
    // will be based on the updated size from last delivered observations.
    switch (observation->BoxOptions()) {
      case ResizeObserverBoxOptions::Border_box:
        observation->UpdateLastReportedSize(borderBoxSize);
        break;
      case ResizeObserverBoxOptions::Content_box:
      default:
        observation->UpdateLastReportedSize(contentBoxSize);
    }

    uint32_t targetDepth = GetNodeDepth(observation->Target());

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

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ResizeObserverEntry, mOwner, mTarget,
                                      mContentRect, mBorderBoxSize,
                                      mContentBoxSize)
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

void ResizeObserverEntry::SetBorderBoxSize(const nsSize& aSize) {
  nsIFrame* frame = mTarget->GetPrimaryFrame();
  const WritingMode wm = frame ? frame->GetWritingMode() : WritingMode();
  mBorderBoxSize = new ResizeObserverSize(this, aSize, wm);
}

void ResizeObserverEntry::SetContentRectAndSize(const nsSize& aSize) {
  nsIFrame* frame = mTarget->GetPrimaryFrame();

  // 1. Update mContentRect.
  nsMargin padding = frame ? frame->GetUsedPadding(): nsMargin();
  // Per the spec, we need to use the top-left padding offset as the origin of
  // our contentRect.
  nsRect rect(nsPoint(padding.left, padding.top), aSize);
  RefPtr<DOMRect> contentRect = new DOMRect(mTarget);
  contentRect->SetLayoutRect(rect);
  mContentRect = contentRect.forget();

  // 2. Update mContentBoxSize.
  const WritingMode wm = frame ? frame->GetWritingMode() : WritingMode();
  mContentBoxSize = new ResizeObserverSize(this, aSize, wm);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ResizeObserverSize, mOwner)
NS_IMPL_CYCLE_COLLECTING_ADDREF(ResizeObserverSize)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ResizeObserverSize)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ResizeObserverSize)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

}  // namespace dom
}  // namespace mozilla
