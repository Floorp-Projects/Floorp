/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FocusState.h"

#include "mozilla/Logging.h"
#include "mozilla/layers/APZThreadUtils.h"

static mozilla::LazyLogModule sApzFstLog("apz.focusstate");
#define FS_LOG(...) MOZ_LOG(sApzFstLog, LogLevel::Debug, (__VA_ARGS__))

namespace mozilla {
namespace layers {

FocusState::FocusState()
    : mMutex("FocusStateMutex"),
      mLastAPZProcessedEvent(1),
      mLastContentProcessedEvent(0),
      mFocusHasKeyEventListeners(false),
      mReceivedUpdate(false),
      mFocusLayersId{0},
      mFocusHorizontalTarget(ScrollableLayerGuid::NULL_SCROLL_ID),
      mFocusVerticalTarget(ScrollableLayerGuid::NULL_SCROLL_ID) {}

uint64_t FocusState::LastAPZProcessedEvent() const {
  APZThreadUtils::AssertOnControllerThread();
  MutexAutoLock lock(mMutex);

  return mLastAPZProcessedEvent;
}

bool FocusState::IsCurrent(const MutexAutoLock& aProofOfLock) const {
  FS_LOG("Checking IsCurrent() with cseq=%" PRIu64 ", aseq=%" PRIu64 "\n",
         mLastContentProcessedEvent, mLastAPZProcessedEvent);
  MOZ_ASSERT(mLastContentProcessedEvent <= mLastAPZProcessedEvent);
  return mLastContentProcessedEvent == mLastAPZProcessedEvent;
}

void FocusState::ReceiveFocusChangingEvent() {
  APZThreadUtils::AssertOnControllerThread();
  MutexAutoLock lock(mMutex);

  if (!mReceivedUpdate) {
    // In the initial state don't advance mLastAPZProcessedEvent because we
    // might blow away the information that we're in a freshly-restarted GPU
    // process. This information (i.e. that mLastAPZProcessedEvent == 1) needs
    // to be preserved until the first call to Update() which will then advance
    // mLastAPZProcessedEvent to match the content-side sequence number.
    return;
  }
  mLastAPZProcessedEvent += 1;
  FS_LOG("Focus changing event incremented aseq to %" PRIu64 ", (%p)\n",
         mLastAPZProcessedEvent, this);
}

void FocusState::Update(LayersId aRootLayerTreeId,
                        LayersId aOriginatingLayersId,
                        const FocusTarget& aState) {
  // This runs on the updater thread, it's not worth passing around extra raw
  // pointers just to assert it.

  MutexAutoLock lock(mMutex);

  FS_LOG("Update with rlt=%" PRIu64 ", olt=%" PRIu64 ", ft=(%s, %" PRIu64 ")\n",
         aRootLayerTreeId.mId, aOriginatingLayersId.mId, aState.Type(),
         aState.mSequenceNumber);
  mReceivedUpdate = true;

  // Update the focus tree with the latest target
  mFocusTree[aOriginatingLayersId] = aState;

  // Reset our internal state so we can recalculate it
  mFocusHasKeyEventListeners = false;
  mFocusLayersId = aRootLayerTreeId;
  mFocusHorizontalTarget = ScrollableLayerGuid::NULL_SCROLL_ID;
  mFocusVerticalTarget = ScrollableLayerGuid::NULL_SCROLL_ID;

  // To update the focus state for the entire APZCTreeManager, we need
  // to traverse the focus tree to find the current leaf which is the global
  // focus target we can use for async keyboard scrolling
  while (true) {
    auto currentNode = mFocusTree.find(mFocusLayersId);
    if (currentNode == mFocusTree.end()) {
      FS_LOG("Setting target to nil (cannot find lt=%" PRIu64 ")\n",
             mFocusLayersId.mId);
      return;
    }

    const FocusTarget& target = currentNode->second;

    // Accumulate event listener flags on the path to the focus target
    mFocusHasKeyEventListeners |= target.mFocusHasKeyEventListeners;

    // Match on the data stored in mData
    // The match functions return true or false depending on whether the
    // enclosing method, FocusState::Update, should return or continue to the
    // next iteration of the while loop, respectively.
    struct FocusTargetDataMatcher {
      FocusState& mFocusState;
      const uint64_t mSequenceNumber;

      bool operator()(const FocusTarget::NoFocusTarget& aNoFocusTarget) {
        FS_LOG("Setting target to nil (reached a nil target) with seq=%" PRIu64
               ", (%p)\n",
               mSequenceNumber, &mFocusState);

        // Mark what sequence number this target has for debugging purposes so
        // we can always accurately report on whether we are stale or not
        mFocusState.mLastContentProcessedEvent = mSequenceNumber;

        // If this focus state was just created and content has experienced more
        // events then us, then assume we were recreated and sync focus sequence
        // numbers.
        if (mFocusState.mLastAPZProcessedEvent == 1 &&
            mFocusState.mLastContentProcessedEvent >
                mFocusState.mLastAPZProcessedEvent) {
          mFocusState.mLastAPZProcessedEvent =
              mFocusState.mLastContentProcessedEvent;
        }
        return true;
      }

      bool operator()(const LayersId& aRefLayerId) {
        // Guard against infinite loops
        MOZ_ASSERT(mFocusState.mFocusLayersId != aRefLayerId);
        if (mFocusState.mFocusLayersId == aRefLayerId) {
          FS_LOG(
              "Setting target to nil (bailing out of infinite loop, lt=%" PRIu64
              ")\n",
              mFocusState.mFocusLayersId.mId);
          return true;
        }

        FS_LOG("Looking for target in lt=%" PRIu64 "\n", aRefLayerId.mId);

        // The focus target is in a child layer tree
        mFocusState.mFocusLayersId = aRefLayerId;
        return false;
      }

      bool operator()(const FocusTarget::ScrollTargets& aScrollTargets) {
        FS_LOG("Setting target to h=%" PRIu64 ", v=%" PRIu64
               ", and seq=%" PRIu64 "(%p)\n",
               aScrollTargets.mHorizontal, aScrollTargets.mVertical,
               mSequenceNumber, &mFocusState);

        // This is the global focus target
        mFocusState.mFocusHorizontalTarget = aScrollTargets.mHorizontal;
        mFocusState.mFocusVerticalTarget = aScrollTargets.mVertical;

        // Mark what sequence number this target has so we can determine whether
        // it is stale or not
        mFocusState.mLastContentProcessedEvent = mSequenceNumber;

        // If this focus state was just created and content has experienced more
        // events then us, then assume we were recreated and sync focus sequence
        // numbers.
        if (mFocusState.mLastAPZProcessedEvent == 1 &&
            mFocusState.mLastContentProcessedEvent >
                mFocusState.mLastAPZProcessedEvent) {
          mFocusState.mLastAPZProcessedEvent =
              mFocusState.mLastContentProcessedEvent;
        }
        return true;
      }
    };  // struct FocusTargetDataMatcher

    if (target.mData.match(
            FocusTargetDataMatcher{*this, target.mSequenceNumber})) {
      return;
    }
  }
}

void FocusState::RemoveFocusTarget(LayersId aLayersId) {
  // This runs on the updater thread, it's not worth passing around extra raw
  // pointers just to assert it.
  MutexAutoLock lock(mMutex);

  mFocusTree.erase(aLayersId);
}

Maybe<ScrollableLayerGuid> FocusState::GetHorizontalTarget() const {
  APZThreadUtils::AssertOnControllerThread();
  MutexAutoLock lock(mMutex);

  // There is not a scrollable layer to async scroll if
  //   1. We aren't current
  //   2. There are event listeners that could change the focus
  //   3. The target has not been layerized
  if (!IsCurrent(lock) || mFocusHasKeyEventListeners ||
      mFocusHorizontalTarget == ScrollableLayerGuid::NULL_SCROLL_ID) {
    return Nothing();
  }
  return Some(ScrollableLayerGuid(mFocusLayersId, 0, mFocusHorizontalTarget));
}

Maybe<ScrollableLayerGuid> FocusState::GetVerticalTarget() const {
  APZThreadUtils::AssertOnControllerThread();
  MutexAutoLock lock(mMutex);

  // There is not a scrollable layer to async scroll if:
  //   1. We aren't current
  //   2. There are event listeners that could change the focus
  //   3. The target has not been layerized
  if (!IsCurrent(lock) || mFocusHasKeyEventListeners ||
      mFocusVerticalTarget == ScrollableLayerGuid::NULL_SCROLL_ID) {
    return Nothing();
  }
  return Some(ScrollableLayerGuid(mFocusLayersId, 0, mFocusVerticalTarget));
}

bool FocusState::CanIgnoreKeyboardShortcutMisses() const {
  APZThreadUtils::AssertOnControllerThread();
  MutexAutoLock lock(mMutex);

  return IsCurrent(lock) && !mFocusHasKeyEventListeners;
}

void FocusState::Reset() {
  MutexAutoLock lock(mMutex);

  mLastAPZProcessedEvent = 1;
  mLastContentProcessedEvent = 0;
  mFocusHasKeyEventListeners = false;
  mReceivedUpdate = false;
  mFocusLayersId = {0};
  mFocusHorizontalTarget = ScrollableLayerGuid::NULL_SCROLL_ID;
  mFocusVerticalTarget = ScrollableLayerGuid::NULL_SCROLL_ID;
  mFocusTree = {};
}

}  // namespace layers
}  // namespace mozilla
