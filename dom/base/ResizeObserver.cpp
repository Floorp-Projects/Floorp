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
#include "nsIScrollableFrame.h"
#include "nsLayoutUtils.h"
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

static nsSize GetContentRectSize(const nsIFrame& aFrame) {
  if (const nsIScrollableFrame* f = do_QueryFrame(&aFrame)) {
    // We return the scrollport rect for compat with other UAs, see bug 1733042.
    // But the scrollPort includes padding (but not border!), so remove it.
    nsRect scrollPort = f->GetScrollPortRect();
    nsMargin padding =
        aFrame.GetUsedPadding().ApplySkipSides(aFrame.GetSkipSides());
    scrollPort.Deflate(padding);
    // This can break in some edge cases like when layout overflows sizes or
    // what not.
    NS_ASSERTION(
        !aFrame.PresContext()->UseOverlayScrollbars() ||
            scrollPort.Size() == aFrame.GetContentRectRelativeToSelf().Size(),
        "Wrong scrollport?");
    return scrollPort.Size();
  }
  return aFrame.GetContentRectRelativeToSelf().Size();
}

/**
 * Returns |aTarget|'s size in the form of gfx::Size (in pixels).
 * If the target is an SVG that does not participate in CSS layout,
 * its width and height are determined from bounding box.
 *
 * https://www.w3.org/TR/resize-observer-1/#calculate-box-size
 */
static AutoTArray<LogicalPixelSize, 1> CalculateBoxSize(
    Element* aTarget, ResizeObserverBoxOptions aBox,
    const ResizeObserver& aObserver) {
  nsIFrame* frame = aTarget->GetPrimaryFrame();

  if (!frame) {
    // TODO: Should this return an empty array instead?
    // https://github.com/w3c/csswg-drafts/issues/7734
    return {LogicalPixelSize()};
  }

  if (frame->HasAnyStateBits(NS_FRAME_SVG_LAYOUT)) {
    // Per the spec, this target's SVG size is always its bounding box size no
    // matter what box option you choose, because SVG elements do not use
    // standard CSS box model.
    // TODO: what if the SVG is fragmented?
    // https://github.com/w3c/csswg-drafts/issues/7736
    const gfxRect bbox = SVGUtils::GetBBox(frame);
    gfx::Size size(static_cast<float>(bbox.width),
                   static_cast<float>(bbox.height));
    const WritingMode wm = frame->GetWritingMode();
    if (aBox == ResizeObserverBoxOptions::Device_pixel_content_box) {
      // Per spec, we calculate the inline/block sizes to target’s bounding box
      // {inline|block} length, in integral device pixels, so we round the final
      // result.
      // https://drafts.csswg.org/resize-observer/#dom-resizeobserverboxoptions-device-pixel-content-box
      const LayoutDeviceIntSize snappedSize =
          RoundedToInt(CSSSize::FromUnknownSize(size) *
                       frame->PresContext()->CSSToDevPixelScale());
      return {LogicalPixelSize(wm, gfx::Size(snappedSize.ToUnknownSize()))};
    }
    return {LogicalPixelSize(wm, size)};
  }

  // Per the spec, non-replaced inline Elements will always have an empty
  // content rect. Therefore, we always use the same trivially-empty size
  // for non-replaced inline elements here, and their IsActive() will
  // always return false. (So its observation won't be fired.)
  // TODO: Should we use an empty array instead?
  // https://github.com/w3c/csswg-drafts/issues/7734
  if (!frame->IsFrameOfType(nsIFrame::eReplaced) &&
      frame->IsFrameOfType(nsIFrame::eLineParticipant)) {
    return {LogicalPixelSize()};
  }

  auto GetFrameSize = [aBox](nsIFrame* aFrame) {
    switch (aBox) {
      case ResizeObserverBoxOptions::Border_box:
        return CSSPixel::FromAppUnits(aFrame->GetSize()).ToUnknownSize();
      case ResizeObserverBoxOptions::Device_pixel_content_box: {
        // Simply converting from app units to device units is insufficient - we
        // need to take subpixel snapping into account. Subpixel snapping
        // happens with respect to the reference frame, so do the dev pixel
        // conversion with our rectangle positioned relative to the reference
        // frame, then get the size from there.
        const auto* referenceFrame = nsLayoutUtils::GetReferenceFrame(aFrame);
        // GetOffsetToCrossDoc version handles <iframe>s in addition to normal
        // cases. We don't expect this to tight loop for additional checks to
        // matter.
        const auto offset = aFrame->GetOffsetToCrossDoc(referenceFrame);
        const auto contentSize = GetContentRectSize(*aFrame);
        // Casting to double here is deliberate to minimize rounding error in
        // upcoming operations.
        const auto appUnitsPerDevPixel =
            static_cast<double>(aFrame->PresContext()->AppUnitsPerDevPixel());
        // Calculation here is a greatly simplified version of
        // `NSRectToSnappedRect` as 1) we're not actually drawing (i.e. no draw
        // target), and 2) transform does not need to be taken into account.
        gfx::Rect rect{gfx::Float(offset.X() / appUnitsPerDevPixel),
                       gfx::Float(offset.Y() / appUnitsPerDevPixel),
                       gfx::Float(contentSize.Width() / appUnitsPerDevPixel),
                       gfx::Float(contentSize.Height() / appUnitsPerDevPixel)};
        gfx::Point tl = rect.TopLeft().Round();
        gfx::Point br = rect.BottomRight().Round();

        rect.SizeTo(gfx::Size(br.x - tl.x, br.y - tl.y));
        rect.NudgeToIntegers();
        return rect.Size().ToUnknownSize();
      }
      case ResizeObserverBoxOptions::Content_box:
      default:
        break;
    }
    return CSSPixel::FromAppUnits(GetContentRectSize(*aFrame)).ToUnknownSize();
  };
  if (!StaticPrefs::dom_resize_observer_support_fragments() &&
      !aObserver.HasNativeCallback()) {
    return {LogicalPixelSize(frame->GetWritingMode(), GetFrameSize(frame))};
  }
  AutoTArray<LogicalPixelSize, 1> size;
  for (nsIFrame* cur = frame; cur; cur = cur->GetNextContinuation()) {
    const WritingMode wm = cur->GetWritingMode();
    size.AppendElement(LogicalPixelSize(wm, GetFrameSize(cur)));
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

ResizeObservation::ResizeObservation(Element& aTarget,
                                     ResizeObserver& aObserver,
                                     ResizeObserverBoxOptions aBox)
    : mTarget(&aTarget),
      mObserver(&aObserver),
      mObservedBox(aBox),
      mLastReportedSize({LogicalPixelSize(WritingMode(), gfx::Size(-1, -1))}) {
  aTarget.BindObject(mObserver);
}

void ResizeObservation::Unlink(RemoveFromObserver aRemoveFromObserver) {
  ResizeObserver* observer = std::exchange(mObserver, nullptr);
  nsCOMPtr<Element> target = std::move(mTarget);
  if (observer && target) {
    if (aRemoveFromObserver == RemoveFromObserver::Yes) {
      observer->Unobserve(*target);
    }
    target->UnbindObject(observer);
  }
}

bool ResizeObservation::IsActive() const {
  // As detailed in the css-contain specification, if the target is hidden by
  // `content-visibility` it should not call its ResizeObservation callbacks.
  nsIFrame* frame = mTarget->GetPrimaryFrame();
  if (frame && frame->IsHiddenByContentVisibilityOnAnyAncestor()) {
    return false;
  }

  return mLastReportedSize !=
         CalculateBoxSize(mTarget, mObservedBox, *mObserver);
}

void ResizeObservation::UpdateLastReportedSize(
    const nsTArray<LogicalPixelSize>& aSize) {
  mLastReportedSize.Assign(aSize);
}

// Only needed for refcounted objects.
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(ResizeObserver)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(ResizeObserver)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOwner, mDocument, mActiveTargets,
                                    mObservationMap);
  if (tmp->mCallback.is<RefPtr<ResizeObserverCallback>>()) {
    ImplCycleCollectionTraverse(
        cb, tmp->mCallback.as<RefPtr<ResizeObserverCallback>>(), "mCallback",
        0);
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(ResizeObserver)
  tmp->Disconnect();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOwner, mDocument, mActiveTargets,
                                  mObservationMap);
  if (tmp->mCallback.is<RefPtr<ResizeObserverCallback>>()) {
    ImplCycleCollectionUnlink(
        tmp->mCallback.as<RefPtr<ResizeObserverCallback>>());
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(ResizeObserver)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ResizeObserver)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ResizeObserver)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

ResizeObserver::ResizeObserver(Document& aDocument, NativeCallback aCallback)
    : mOwner(aDocument.GetInnerWindow()),
      mDocument(&aDocument),
      mCallback(aCallback) {
  MOZ_ASSERT(mOwner, "Need a non-null owner window");
  MOZ_ASSERT(mDocument == mOwner->GetExtantDoc());
}

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
                             const ResizeObserverOptions& aOptions) {
  if (MOZ_UNLIKELY(!mDocument)) {
    MOZ_ASSERT_UNREACHABLE("How did we call observe() after unlink?");
    return;
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

  observation = new ResizeObservation(aTarget, *this, aOptions.mBox);
  mObservationList.insertBack(observation);

  // Per the spec, we need to trigger notification in event loop that
  // contains ResizeObserver observe call even when resize/reflow does
  // not happen.
  mDocument->ScheduleResizeObserversNotification();
}

void ResizeObserver::Unobserve(Element& aTarget) {
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

    auto borderBoxSize =
        CalculateBoxSize(target, ResizeObserverBoxOptions::Border_box, *this);
    auto contentBoxSize =
        CalculateBoxSize(target, ResizeObserverBoxOptions::Content_box, *this);
    auto devicePixelContentBoxSize = CalculateBoxSize(
        target, ResizeObserverBoxOptions::Device_pixel_content_box, *this);
    RefPtr<ResizeObserverEntry> entry =
        new ResizeObserverEntry(mOwner, *target, borderBoxSize, contentBoxSize,
                                devicePixelContentBoxSize);

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
      case ResizeObserverBoxOptions::Device_pixel_content_box:
        observation->UpdateLastReportedSize(devicePixelContentBoxSize);
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

  if (mCallback.is<RefPtr<ResizeObserverCallback>>()) {
    RefPtr<ResizeObserverCallback> callback(
        mCallback.as<RefPtr<ResizeObserverCallback>>());
    callback->Call(this, entries, *this);
  } else {
    mCallback.as<NativeCallback>()(entries, *this);
  }

  mActiveTargets.Clear();
  mHasSkippedTargets = false;

  return shallowestTargetDepth;
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ResizeObserverEntry, mOwner, mTarget,
                                      mContentRect, mBorderBoxSize,
                                      mContentBoxSize,
                                      mDevicePixelContentBoxSize)
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
  aRetVal.Assign(mBorderBoxSize);
}

void ResizeObserverEntry::GetContentBoxSize(
    nsTArray<RefPtr<ResizeObserverSize>>& aRetVal) const {
  // In the resize-observer-1 spec, there will only be a single
  // ResizeObserverSize returned in the FrozenArray for now.
  //
  // Note: the usage of FrozenArray is to support elements that have multiple
  // fragments, which occur in multi-column scenarios.
  // https://drafts.csswg.org/resize-observer/#resize-observer-entry-interface
  aRetVal.Assign(mContentBoxSize);
}

void ResizeObserverEntry::GetDevicePixelContentBoxSize(
    nsTArray<RefPtr<ResizeObserverSize>>& aRetVal) const {
  // In the resize-observer-1 spec, there will only be a single
  // ResizeObserverSize returned in the FrozenArray for now.
  //
  // Note: the usage of FrozenArray is to support elements that have multiple
  // fragments, which occur in multi-column scenarios.
  // https://drafts.csswg.org/resize-observer/#resize-observer-entry-interface
  aRetVal.Assign(mDevicePixelContentBoxSize);
}

void ResizeObserverEntry::SetBorderBoxSize(
    const nsTArray<LogicalPixelSize>& aSize) {
  mBorderBoxSize.Clear();
  mBorderBoxSize.SetCapacity(aSize.Length());
  for (const LogicalPixelSize& size : aSize) {
    mBorderBoxSize.AppendElement(new ResizeObserverSize(mOwner, size));
  }
}

void ResizeObserverEntry::SetContentRectAndSize(
    const nsTArray<LogicalPixelSize>& aSize) {
  nsIFrame* frame = mTarget->GetPrimaryFrame();

  // 1. Update mContentRect.
  nsMargin padding = frame ? frame->GetUsedPadding() : nsMargin();
  // Per the spec, we need to use the top-left padding offset as the origin of
  // our contentRect.
  gfx::Size sizeForRect;
  MOZ_DIAGNOSTIC_ASSERT(!aSize.IsEmpty());
  if (!aSize.IsEmpty()) {
    const WritingMode wm = frame ? frame->GetWritingMode() : WritingMode();
    sizeForRect = aSize[0].PhysicalSize(wm);
  }
  nsRect rect(nsPoint(padding.left, padding.top),
              CSSPixel::ToAppUnits(CSSSize::FromUnknownSize(sizeForRect)));
  RefPtr<DOMRect> contentRect = new DOMRect(mOwner);
  contentRect->SetLayoutRect(rect);
  mContentRect = std::move(contentRect);

  // 2. Update mContentBoxSize.
  mContentBoxSize.Clear();
  mContentBoxSize.SetCapacity(aSize.Length());
  for (const LogicalPixelSize& size : aSize) {
    mContentBoxSize.AppendElement(new ResizeObserverSize(mOwner, size));
  }
}

void ResizeObserverEntry::SetDevicePixelContentSize(
    const nsTArray<LogicalPixelSize>& aSize) {
  mDevicePixelContentBoxSize.Clear();
  mDevicePixelContentBoxSize.SetCapacity(aSize.Length());
  for (const LogicalPixelSize& size : aSize) {
    mDevicePixelContentBoxSize.AppendElement(
        new ResizeObserverSize(mOwner, size));
  }
}

static void LastRememberedSizeCallback(
    const Sequence<OwningNonNull<ResizeObserverEntry>>& aEntries,
    ResizeObserver& aObserver) {
  for (const auto& entry : aEntries) {
    Element* target = entry->Target();
    if (!target->IsInComposedDoc()) {
      aObserver.Unobserve(*target);
      target->RemoveLastRememberedBSize();
      target->RemoveLastRememberedISize();
      continue;
    }
    nsIFrame* frame = target->GetPrimaryFrame();
    if (!frame) {
      aObserver.Unobserve(*target);
      continue;
    }
    MOZ_ASSERT(!frame->IsFrameOfType(nsIFrame::eLineParticipant) ||
                   frame->IsFrameOfType(nsIFrame::eReplaced),
               "Should have unobserved non-replaced inline.");
    MOZ_ASSERT(!frame->HidesContent(),
               "Should have unobserved element skipping its contents.");
    const nsStylePosition* stylePos = frame->StylePosition();
    const WritingMode wm = frame->GetWritingMode();
    bool canUpdateBSize = stylePos->ContainIntrinsicBSize(wm).HasAuto();
    bool canUpdateISize = stylePos->ContainIntrinsicISize(wm).HasAuto();
    MOZ_ASSERT(canUpdateBSize || !target->HasLastRememberedBSize(),
               "Should have removed the last remembered block size.");
    MOZ_ASSERT(canUpdateISize || !target->HasLastRememberedISize(),
               "Should have removed the last remembered inline size.");
    MOZ_ASSERT(canUpdateBSize || canUpdateISize,
               "Should have unobserved if we can't update any size.");
    AutoTArray<RefPtr<ResizeObserverSize>, 1> contentSizeList;
    entry->GetContentBoxSize(contentSizeList);
    MOZ_ASSERT(!contentSizeList.IsEmpty());
    if (canUpdateBSize) {
      float bSize = 0;
      for (const auto& current : contentSizeList) {
        bSize += current->BlockSize();
      }
      target->SetLastRememberedBSize(bSize);
    }
    if (canUpdateISize) {
      float iSize = 0;
      for (const auto& current : contentSizeList) {
        iSize = std::max(iSize, current->InlineSize());
      }
      target->SetLastRememberedISize(iSize);
    }
  }
}

/* static */ already_AddRefed<ResizeObserver>
ResizeObserver::CreateLastRememberedSizeObserver(Document& aDocument) {
  return do_AddRef(new ResizeObserver(aDocument, LastRememberedSizeCallback));
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ResizeObserverSize, mOwner)
NS_IMPL_CYCLE_COLLECTING_ADDREF(ResizeObserverSize)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ResizeObserverSize)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ResizeObserverSize)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

}  // namespace mozilla::dom
