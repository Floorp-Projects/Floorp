/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/FocusState.h"

// #define FS_LOG(...) printf_stderr("FS: " __VA_ARGS__)
#define FS_LOG(...)

namespace mozilla {
namespace layers {

FocusState::FocusState()
  : mLastAPZProcessedEvent(1)
  , mLastContentProcessedEvent(0)
  , mFocusHasKeyEventListeners(false)
  , mFocusLayersId(0)
  , mFocusHorizontalTarget(FrameMetrics::NULL_SCROLL_ID)
  , mFocusVerticalTarget(FrameMetrics::NULL_SCROLL_ID)
{
}

bool
FocusState::IsCurrent() const
{
  FS_LOG("Checking IsCurrent() with cseq=%" PRIu64 ", aseq=%" PRIu64 "\n",
         mLastContentProcessedEvent,
         mLastAPZProcessedEvent);

  MOZ_ASSERT(mLastContentProcessedEvent <= mLastAPZProcessedEvent);
  return mLastContentProcessedEvent == mLastAPZProcessedEvent;
}

void
FocusState::ReceiveFocusChangingEvent()
{
  mLastAPZProcessedEvent += 1;
}

void
FocusState::Update(uint64_t aRootLayerTreeId,
                   uint64_t aOriginatingLayersId,
                   const FocusTarget& aState)
{
  FS_LOG("Update with rlt=%" PRIu64 ", olt=%" PRIu64 ", ft=(%d, %" PRIu64 ")\n",
         aRootLayerTreeId,
         aOriginatingLayersId,
         static_cast<int>(aState.mType),
         aState.mSequenceNumber);

  // Update the focus tree with the latest target
  mFocusTree[aOriginatingLayersId] = aState;

  // Reset our internal state so we can recalculate it
  mFocusHasKeyEventListeners = false;
  mFocusLayersId = aRootLayerTreeId;
  mFocusHorizontalTarget = FrameMetrics::NULL_SCROLL_ID;
  mFocusVerticalTarget = FrameMetrics::NULL_SCROLL_ID;

  // To update the focus state for the entire APZCTreeManager, we need
  // to traverse the focus tree to find the current leaf which is the global
  // focus target we can use for async keyboard scrolling
  while (true) {
    auto currentNode = mFocusTree.find(mFocusLayersId);
    if (currentNode == mFocusTree.end()) {
      FS_LOG("Setting target to nil (cannot find lt=%" PRIu64 ")\n",
             mFocusLayersId);
      return;
    }

    const FocusTarget& target = currentNode->second;

    // Accumulate event listener flags on the path to the focus target
    mFocusHasKeyEventListeners |= target.mFocusHasKeyEventListeners;

    switch (target.mType) {
      case FocusTarget::eRefLayer: {
        // Guard against infinite loops
        MOZ_ASSERT(mFocusLayersId != target.mData.mRefLayerId);
        if (mFocusLayersId == target.mData.mRefLayerId) {
          FS_LOG("Setting target to nil (bailing out of infinite loop, lt=%" PRIu64 ")\n",
                 mFocusLayersId);
          return;
        }

        FS_LOG("Looking for target in lt=%" PRIu64 "\n", target.mData.mRefLayerId);

        // The focus target is in a child layer tree
        mFocusLayersId = target.mData.mRefLayerId;
        break;
      }
      case FocusTarget::eScrollLayer: {
        FS_LOG("Setting target to h=%" PRIu64 ", v=%" PRIu64 ", and seq=%" PRIu64 "\n",
               target.mData.mScrollTargets.mHorizontal,
               target.mData.mScrollTargets.mVertical,
               target.mSequenceNumber);

        // This is the global focus target
        mFocusHorizontalTarget = target.mData.mScrollTargets.mHorizontal;
        mFocusVerticalTarget = target.mData.mScrollTargets.mVertical;

        // Mark what sequence number this target has so we can determine whether
        // it is stale or not
        mLastContentProcessedEvent = target.mSequenceNumber;
        return;
      }
      case FocusTarget::eNone: {
        FS_LOG("Setting target to nil (reached a nil target)\n");

        // Mark what sequence number this target has for debugging purposes so
        // we can always accurately report on whether we are stale or not
        mLastContentProcessedEvent = target.mSequenceNumber;
        return;
      }
    }
  }
}

std::unordered_set<uint64_t>
FocusState::GetFocusTargetLayerIds() const
{
  std::unordered_set<uint64_t> layersIds;
  layersIds.reserve(mFocusTree.size());

  for (const auto& focusNode : mFocusTree) {
    layersIds.insert(focusNode.first);
  }

  return layersIds;
}

void
FocusState::RemoveFocusTarget(uint64_t aLayersId)
{
  mFocusTree.erase(aLayersId);
}

Maybe<ScrollableLayerGuid>
FocusState::GetHorizontalTarget() const
{
  // There is not a scrollable layer to async scroll if
  //   1. We aren't current
  //   2. There are event listeners that could change the focus
  //   3. The target has not been layerized
  if (!IsCurrent() ||
      mFocusHasKeyEventListeners ||
      mFocusHorizontalTarget == FrameMetrics::NULL_SCROLL_ID) {
    return Nothing();
  }
  return Some(ScrollableLayerGuid(mFocusLayersId, 0, mFocusHorizontalTarget));
}

Maybe<ScrollableLayerGuid>
FocusState::GetVerticalTarget() const
{
  // There is not a scrollable layer to async scroll if:
  //   1. We aren't current
  //   2. There are event listeners that could change the focus
  //   3. The target has not been layerized
  if (!IsCurrent() ||
      mFocusHasKeyEventListeners ||
      mFocusVerticalTarget == FrameMetrics::NULL_SCROLL_ID) {
    return Nothing();
  }
  return Some(ScrollableLayerGuid(mFocusLayersId, 0, mFocusVerticalTarget));
}

} // namespace layers
} // namespace mozilla
