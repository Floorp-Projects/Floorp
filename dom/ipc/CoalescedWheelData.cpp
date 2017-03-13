/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "base/basictypes.h"

#include "CoalescedWheelData.h"
#include "FrameMetrics.h"

using namespace mozilla;
using namespace mozilla::dom;

void
CoalescedWheelData::Coalesce(const WidgetWheelEvent& aEvent,
                             const ScrollableLayerGuid& aGuid,
                             const uint64_t& aInputBlockId)
{
  if (IsEmpty()) {
    mCoalescedWheelEvent = MakeUnique<WidgetWheelEvent>(aEvent);
    mGuid = aGuid;
    mInputBlockId = aInputBlockId;
  } else {
    MOZ_ASSERT(mGuid == aGuid);
    MOZ_ASSERT(mInputBlockId == aInputBlockId);
    MOZ_ASSERT(mCoalescedWheelEvent->mModifiers == aEvent.mModifiers);
    MOZ_ASSERT(mCoalescedWheelEvent->mDeltaMode == aEvent.mDeltaMode);
    MOZ_ASSERT(mCoalescedWheelEvent->mCanTriggerSwipe ==
               aEvent.mCanTriggerSwipe);
    mCoalescedWheelEvent->mDeltaX += aEvent.mDeltaX;
    mCoalescedWheelEvent->mDeltaY += aEvent.mDeltaY;
    mCoalescedWheelEvent->mDeltaZ += aEvent.mDeltaZ;
    mCoalescedWheelEvent->mLineOrPageDeltaX += aEvent.mLineOrPageDeltaX;
    mCoalescedWheelEvent->mLineOrPageDeltaY += aEvent.mLineOrPageDeltaY;
    mCoalescedWheelEvent->mTimeStamp = aEvent.mTimeStamp;
  }
}

bool
CoalescedWheelData::CanCoalesce(const WidgetWheelEvent& aEvent,
                                const ScrollableLayerGuid& aGuid,
                                const uint64_t& aInputBlockId)
{
  MOZ_ASSERT(!IsEmpty());
  return !mCoalescedWheelEvent ||
         (mCoalescedWheelEvent->mRefPoint == aEvent.mRefPoint &&
          mCoalescedWheelEvent->mModifiers == aEvent.mModifiers &&
          mCoalescedWheelEvent->mDeltaMode == aEvent.mDeltaMode &&
          mCoalescedWheelEvent->mCanTriggerSwipe == aEvent.mCanTriggerSwipe &&
          mGuid == aGuid &&
          mInputBlockId == aInputBlockId);
}
