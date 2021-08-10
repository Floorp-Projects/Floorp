/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ResizeObserver.h"

#include "mozilla/dom/DOMRect.h"
#include "mozilla/dom/Document.h"
#include "mozilla/SVGUtils.h"
#include "nsIContent.h"
#include "nsIContentInlines.h"
#include <limits>

namespace mozilla::dom {

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
    gfxRect bbox = SVGUtils::GetBBox(frame);
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

NS_IMPL_CYCLE_COLLECTION_CLASS(ResizeObservation)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(ResizeObservation)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTarget);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(ResizeObservation)
  tmp->Unlink(RemoveFromObserver::Yes);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(ResizeObservation, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(ResizeObservation, Release)

ResizeObservation::ResizeObservation(Element& aTarget,
                                     ResizeObserver& aObserver,
                                     ResizeObserverBoxOptions aBox,
                                     WritingMode aWm)
    : mTarget(&aTarget),
      mObserver(&aObserver),
      mObservedBox(aBox),
      // This starts us with a 0,0 last-reported-size:
      mLastReportedSize(aWm),
      mLastReportedWM(aWm) {
  aTarget.BindObject(mObserver);
}

void ResizeObservation::Unlink(RemoveFromObserver aRemoveFromObserver) {
  ResizeObserver* observer = std::exchange(mObserver, nullptr);
  nsCOMPtr<Element> target = std::move(mTarget);
  if (observer && target) {
    if (aRemoveFromObserver == RemoveFromObserver::Yes) {
      IgnoredErrorResult rv;
      observer->Unobserve(*target, rv);
      MOZ_DIAGNOSTIC_ASSERT(!rv.Failed(),
                            "How could we keep the observer and target around "
                            "without being in the observation map?");
    }
    target->UnbindObject(observer);
  }
}

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
NS_IMPL_CYCLE_COLLECTION_CLASS(ResizeObserver)

NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(ResizeObserver)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(ResizeObserver)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOwner, mDocument, mCallback,
                                    mActiveTargets, mObservationMap);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(ResizeObserver)
  tmp->Disconnect();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOwner, mDocument, mCallback, mActiveTargets,
                                  mObservationMap);
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

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

  Document* doc = window->GetExtantDoc();
  if (!doc) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return do_AddRef(new ResizeObserver(std::move(window), doc, aCb));
}

void ResizeObserver::Observe(Element& aTarget,
                             const ResizeObserverOptions& aOptions,
                             ErrorResult& aRv) {
  if (MOZ_UNLIKELY(!mDocument)) {
    return aRv.Throw(NS_ERROR_FAILURE);
  }

  // NOTE(emilio): Per spec, this is supposed to happen on construction, but the
  // spec isn't particularly sane here, see
  // https://github.com/w3c/csswg-drafts/issues/4518
  if (mObservationList.isEmpty()) {
    MOZ_ASSERT(mObservationMap.IsEmpty());
    mDocument->AddResizeObserver(*this);
  }

  auto& observation = mObservationMap.LookupOrInsert(&aTarget);
  if (observation) {
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
    // Remove the pre-existing entry, but without unregistering ourselves from
    // the controller.
    observation->remove();
    observation = nullptr;
  }

  // FIXME(emilio): This should probably either flush or not look at the
  // writing-mode or something.
  nsIFrame* frame = aTarget.GetPrimaryFrame();
  observation =
      new ResizeObservation(aTarget, *this, aOptions.mBox,
                            frame ? frame->GetWritingMode() : WritingMode());
  mObservationList.insertBack(observation);

  // Per the spec, we need to trigger notification in event loop that
  // contains ResizeObserver observe call even when resize/reflow does
  // not happen.
  mDocument->ScheduleResizeObserversNotification();
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
  if (mObservationList.isEmpty()) {
    if (MOZ_LIKELY(mDocument)) {
      mDocument->RemoveResizeObserver(*this);
    }
  }
}

void ResizeObserver::Disconnect() {
  const bool registered = !mObservationList.isEmpty();
  while (auto* observation = mObservationList.popFirst()) {
    observation->Unlink(ResizeObservation::RemoveFromObserver::No);
  }
  MOZ_ASSERT(mObservationList.isEmpty());
  mObservationMap.Clear();
  mActiveTargets.Clear();
  if (registered && MOZ_LIKELY(mDocument)) {
    mDocument->RemoveResizeObserver(*this);
  }
}

void ResizeObserver::GatherActiveObservations(uint32_t aDepth) {
  mActiveTargets.Clear();
  mHasSkippedTargets = false;

  for (auto* observation : mObservationList) {
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

    nsSize borderBoxSize =
        GetTargetSize(target, ResizeObserverBoxOptions::Border_box);
    nsSize contentBoxSize =
        GetTargetSize(target, ResizeObserverBoxOptions::Content_box);
    RefPtr<ResizeObserverEntry> entry =
        new ResizeObserverEntry(this, *target, borderBoxSize, contentBoxSize);

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

void ResizeObserverEntry::GetBorderBoxSize(
    nsTArray<RefPtr<ResizeObserverSize>>& aRetVal) const {
  // In the resize-observer-1 spec, there will only be a single
  // ResizeObserverSize returned in the FrozenArray for now.
  //
  // Note: the usage of FrozenArray is to support elements that have multiple
  // fragments, which occur in multi-column scenarios.
  // https://drafts.csswg.org/resize-observer/#resize-observer-entry-interface
  aRetVal.Clear();
  aRetVal.AppendElement(mBorderBoxSize);
}

void ResizeObserverEntry::GetContentBoxSize(
    nsTArray<RefPtr<ResizeObserverSize>>& aRetVal) const {
  // In the resize-observer-1 spec, there will only be a single
  // ResizeObserverSize returned in the FrozenArray for now.
  //
  // Note: the usage of FrozenArray is to support elements that have multiple
  // fragments, which occur in multi-column scenarios.
  // https://drafts.csswg.org/resize-observer/#resize-observer-entry-interface
  aRetVal.Clear();
  aRetVal.AppendElement(mContentBoxSize);
}

void ResizeObserverEntry::SetBorderBoxSize(const nsSize& aSize) {
  nsIFrame* frame = mTarget->GetPrimaryFrame();
  const WritingMode wm = frame ? frame->GetWritingMode() : WritingMode();
  mBorderBoxSize = new ResizeObserverSize(
      this, CSSSize::FromAppUnits(aSize).ToUnknownSize(), wm);
}

void ResizeObserverEntry::SetContentRectAndSize(const nsSize& aSize) {
  nsIFrame* frame = mTarget->GetPrimaryFrame();

  // 1. Update mContentRect.
  nsMargin padding = frame ? frame->GetUsedPadding() : nsMargin();
  // Per the spec, we need to use the top-left padding offset as the origin of
  // our contentRect.
  nsRect rect(nsPoint(padding.left, padding.top), aSize);
  RefPtr<DOMRect> contentRect = new DOMRect(this);
  contentRect->SetLayoutRect(rect);
  mContentRect = std::move(contentRect);

  // 2. Update mContentBoxSize.
  const WritingMode wm = frame ? frame->GetWritingMode() : WritingMode();
  mContentBoxSize = new ResizeObserverSize(
      this, CSSSize::FromAppUnits(aSize).ToUnknownSize(), wm);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ResizeObserverSize, mOwner)
NS_IMPL_CYCLE_COLLECTING_ADDREF(ResizeObserverSize)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ResizeObserverSize)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ResizeObserverSize)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

}  // namespace mozilla::dom
