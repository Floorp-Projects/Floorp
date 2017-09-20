/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "base/basictypes.h"

#include "CoalescedMouseData.h"
#include "TabChild.h"

using namespace mozilla;
using namespace mozilla::dom;

void
CoalescedMouseData::Coalesce(const WidgetMouseEvent& aEvent,
                             const ScrollableLayerGuid& aGuid,
                             const uint64_t& aInputBlockId)
{
  if (IsEmpty()) {
    mCoalescedInputEvent = MakeUnique<WidgetMouseEvent>(aEvent);
    mGuid = aGuid;
    mInputBlockId = aInputBlockId;
    MOZ_ASSERT(!mCoalescedInputEvent->mCoalescedWidgetEvents);
  } else {
    MOZ_ASSERT(mGuid == aGuid);
    MOZ_ASSERT(mInputBlockId == aInputBlockId);
    MOZ_ASSERT(mCoalescedInputEvent->mModifiers == aEvent.mModifiers);
    MOZ_ASSERT(mCoalescedInputEvent->mReason == aEvent.mReason);
    MOZ_ASSERT(mCoalescedInputEvent->inputSource == aEvent.inputSource);
    MOZ_ASSERT(mCoalescedInputEvent->button == aEvent.button);
    MOZ_ASSERT(mCoalescedInputEvent->buttons == aEvent.buttons);
    mCoalescedInputEvent->mTimeStamp = aEvent.mTimeStamp;
    mCoalescedInputEvent->mRefPoint = aEvent.mRefPoint;
    mCoalescedInputEvent->pressure = aEvent.pressure;
    mCoalescedInputEvent->AssignPointerHelperData(aEvent);
  }

  if (aEvent.mMessage == eMouseMove &&
      PointerEventHandler::IsPointerEventEnabled()) {
    // PointerEvent::getCoalescedEvents is only applied to pointermove events.
    if (!mCoalescedInputEvent->mCoalescedWidgetEvents) {
      mCoalescedInputEvent->mCoalescedWidgetEvents =
        new WidgetPointerEventHolder();
    }
    // Append current event in mCoalescedWidgetEvents. We use them to generate
    // DOM events when content calls PointerEvent::getCoalescedEvents.
    WidgetPointerEvent* event = mCoalescedInputEvent->mCoalescedWidgetEvents
                                  ->mEvents.AppendElement(aEvent);

    event->mFlags.mBubbles = false;
    event->mFlags.mCancelable = false;
  }
}

bool
CoalescedMouseData::CanCoalesce(const WidgetMouseEvent& aEvent,
                             const ScrollableLayerGuid& aGuid,
                             const uint64_t& aInputBlockId)
{
  MOZ_ASSERT(aEvent.mMessage == eMouseMove);
  return !mCoalescedInputEvent ||
         (mCoalescedInputEvent->mModifiers == aEvent.mModifiers &&
          mCoalescedInputEvent->inputSource == aEvent.inputSource &&
          mCoalescedInputEvent->pointerId == aEvent.pointerId &&
          mCoalescedInputEvent->button == aEvent.button &&
          mCoalescedInputEvent->buttons == aEvent.buttons &&
          mGuid == aGuid &&
          mInputBlockId == aInputBlockId);
}


void
CoalescedMouseMoveFlusher::WillRefresh(mozilla::TimeStamp aTime)
{
  MOZ_ASSERT(mRefreshDriver);
  mTabChild->FlushAllCoalescedMouseData();
  mTabChild->ProcessPendingCoalescedMouseDataAndDispatchEvents();
}

void
CoalescedMouseMoveFlusher::StartObserver()
{
  nsRefreshDriver* refreshDriver = GetRefreshDriver();
  if (mRefreshDriver && mRefreshDriver == refreshDriver) {
    // Nothing to do if we already added an observer and it's same refresh driver.
    return;
  }
  RemoveObserver();
  if (refreshDriver) {
    mRefreshDriver = refreshDriver;
    DebugOnly<bool> success =
      mRefreshDriver->AddRefreshObserver(this, FlushType::Event);
    MOZ_ASSERT(success);
  }
}

void
CoalescedMouseMoveFlusher::RemoveObserver()
{
  if (mRefreshDriver) {
    mRefreshDriver->RemoveRefreshObserver(this, FlushType::Event);
    mRefreshDriver = nullptr;
  }
}

nsRefreshDriver*
CoalescedMouseMoveFlusher::GetRefreshDriver()
{
  nsCOMPtr<nsIPresShell> presShell = mTabChild->GetPresShell();
  if (!presShell || !presShell->GetPresContext() ||
      !presShell->GetPresContext()->RefreshDriver()) {
    return nullptr;
  }
  return presShell->GetPresContext()->RefreshDriver();
}
