/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "base/basictypes.h"

#include "CoalescedWheelData.h"

using namespace mozilla;
using namespace mozilla::dom;

void CoalescedWheelData::Coalesce(const WidgetWheelEvent& aEvent,
                                  const ScrollableLayerGuid& aGuid,
                                  const uint64_t& aInputBlockId) {
  if (IsEmpty()) {
    mCoalescedInputEvent = MakeUnique<WidgetWheelEvent>(aEvent);
    mGuid = aGuid;
    mInputBlockId = aInputBlockId;
  } else {
    MOZ_ASSERT(mGuid == aGuid);
    MOZ_ASSERT(mInputBlockId == aInputBlockId);
    MOZ_ASSERT(mCoalescedInputEvent->mModifiers == aEvent.mModifiers);
    MOZ_ASSERT(mCoalescedInputEvent->mDeltaMode == aEvent.mDeltaMode);
    MOZ_ASSERT(mCoalescedInputEvent->mCanTriggerSwipe ==
               aEvent.mCanTriggerSwipe);
    mCoalescedInputEvent->mDeltaX += aEvent.mDeltaX;
    mCoalescedInputEvent->mDeltaY += aEvent.mDeltaY;
    mCoalescedInputEvent->mDeltaZ += aEvent.mDeltaZ;
    mCoalescedInputEvent->mLineOrPageDeltaX += aEvent.mLineOrPageDeltaX;
    mCoalescedInputEvent->mLineOrPageDeltaY += aEvent.mLineOrPageDeltaY;
    mCoalescedInputEvent->mTimeStamp = aEvent.mTimeStamp;
  }
}

bool CoalescedWheelData::CanCoalesce(const WidgetWheelEvent& aEvent,
                                     const ScrollableLayerGuid& aGuid,
                                     const uint64_t& aInputBlockId) {
  MOZ_ASSERT(!IsEmpty());
  return !mCoalescedInputEvent ||
         (mCoalescedInputEvent->mRefPoint == aEvent.mRefPoint &&
          mCoalescedInputEvent->mModifiers == aEvent.mModifiers &&
          mCoalescedInputEvent->mDeltaMode == aEvent.mDeltaMode &&
          mCoalescedInputEvent->mCanTriggerSwipe == aEvent.mCanTriggerSwipe &&
          mGuid == aGuid && mInputBlockId == aInputBlockId);
}
