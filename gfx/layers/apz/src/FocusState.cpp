/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/FocusState.h"

namespace mozilla {
namespace layers {

FocusState::FocusState()
  : mFocusHasKeyEventListeners(false)
  , mFocusLayersId(0)
  , mFocusHorizontalTarget(FrameMetrics::NULL_SCROLL_ID)
  , mFocusVerticalTarget(FrameMetrics::NULL_SCROLL_ID)
{
}

void
FocusState::Update(uint64_t aRootLayerTreeId,
                   uint64_t aOriginatingLayersId,
                   const FocusTarget& aState)
{
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
          return;
        }

        // The focus target is in a child layer tree
        mFocusLayersId = target.mData.mRefLayerId;
        break;
      }
      case FocusTarget::eScrollLayer: {
        // This is the global focus target
        mFocusHorizontalTarget = target.mData.mScrollTargets.mHorizontal;
        mFocusVerticalTarget = target.mData.mScrollTargets.mVertical;
        return;
      }
      case FocusTarget::eNone: {
        return;
      }
      case FocusTarget::eSentinel: {
        MOZ_ASSERT_UNREACHABLE("Invalid FocusTargetType");
      }
    }
  }
}

std::unordered_set<uint64_t>
FocusState::GetFocusTargetLayerIds() const
{
  std::unordered_set<uint64_t> layersIds;
  layersIds.reserve(mFocusTree.size());

  for (auto focusNode : mFocusTree) {
    layersIds.insert(focusNode.first);
  }

  return layersIds;
}

void
FocusState::RemoveFocusTarget(uint64_t aLayersId)
{
  mFocusTree.erase(aLayersId);
}

} // namespace layers
} // namespace mozilla
