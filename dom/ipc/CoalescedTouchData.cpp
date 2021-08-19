/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CoalescedTouchData.h"
#include "BrowserChild.h"
#include "mozilla/dom/PointerEventHandler.h"

using namespace mozilla;
using namespace mozilla::dom;

static const uint32_t sMaxTouchMoveIdentifiers = 10;

void CoalescedTouchData::CreateCoalescedTouchEvent(
    const WidgetTouchEvent& aEvent) {
  MOZ_ASSERT(IsEmpty());
  mCoalescedInputEvent = MakeUnique<WidgetTouchEvent>(aEvent);
  for (size_t i = 0; i < mCoalescedInputEvent->mTouches.Length(); i++) {
    const RefPtr<Touch>& touch = mCoalescedInputEvent->mTouches[i];
    touch->mCoalescedWidgetEvents = MakeAndAddRef<WidgetPointerEventHolder>();
    // Add an initial event into coalesced events, so
    // the relevant pointer event would at least contain one coalesced event.
    WidgetPointerEvent* event =
        touch->mCoalescedWidgetEvents->mEvents.AppendElement(WidgetPointerEvent(
            aEvent.IsTrusted(), ePointerMove, aEvent.mWidget));
    PointerEventHandler::InitPointerEventFromTouch(*event, aEvent, *touch,
                                                   i == 0);
    event->mFlags.mBubbles = false;
    event->mFlags.mCancelable = false;
  }
}

void CoalescedTouchData::Coalesce(const WidgetTouchEvent& aEvent,
                                  const ScrollableLayerGuid& aGuid,
                                  const uint64_t& aInputBlockId,
                                  const nsEventStatus& aApzResponse) {
  MOZ_ASSERT(aEvent.mMessage == eTouchMove);
  if (IsEmpty()) {
    CreateCoalescedTouchEvent(aEvent);
    mGuid = aGuid;
    mInputBlockId = aInputBlockId;
    mApzResponse = aApzResponse;
  } else {
    MOZ_ASSERT(mGuid == aGuid);
    MOZ_ASSERT(mInputBlockId == aInputBlockId);
    MOZ_ASSERT(mCoalescedInputEvent->mModifiers == aEvent.mModifiers);
    MOZ_ASSERT(mCoalescedInputEvent->mInputSource == aEvent.mInputSource);

    for (size_t i = 0; i < aEvent.mTouches.Length(); i++) {
      const RefPtr<Touch>& touch = aEvent.mTouches[i];
      // Get the same touch in the original event
      RefPtr<Touch> sameTouch = GetTouch(touch->Identifier());
      // The checks in CoalescedTouchData::CanCoalesce ensure it should never
      // be null.
      MOZ_ASSERT(sameTouch);
      MOZ_ASSERT(sameTouch->mCoalescedWidgetEvents);
      MOZ_ASSERT(!sameTouch->mCoalescedWidgetEvents->mEvents.IsEmpty());
      if (!sameTouch->Equals(touch)) {
        sameTouch->SetSameAs(touch);
        WidgetPointerEvent* event =
            sameTouch->mCoalescedWidgetEvents->mEvents.AppendElement(
                WidgetPointerEvent(aEvent.IsTrusted(), ePointerMove,
                                   aEvent.mWidget));
        PointerEventHandler::InitPointerEventFromTouch(*event, aEvent, *touch,
                                                       i == 0);
        event->mFlags.mBubbles = false;
        event->mFlags.mCancelable = false;
      }
    }

    mCoalescedInputEvent->mTimeStamp = aEvent.mTimeStamp;
  }
}

bool CoalescedTouchData::CanCoalesce(const WidgetTouchEvent& aEvent,
                                     const ScrollableLayerGuid& aGuid,
                                     const uint64_t& aInputBlockId,
                                     const nsEventStatus& aApzResponse) {
  MOZ_ASSERT(!IsEmpty());
  if (mGuid != aGuid || mInputBlockId != aInputBlockId ||
      mCoalescedInputEvent->mModifiers != aEvent.mModifiers ||
      mCoalescedInputEvent->mInputSource != aEvent.mInputSource ||
      aEvent.mTouches.Length() > sMaxTouchMoveIdentifiers) {
    return false;
  }

  // Ensures both touchmove events have the same touches
  if (aEvent.mTouches.Length() != mCoalescedInputEvent->mTouches.Length()) {
    return false;
  }
  for (const RefPtr<Touch>& touch : aEvent.mTouches) {
    if (!GetTouch(touch->Identifier())) {
      return false;
    }
  }

  // If one of them is eIgnore and the other one is eConsumeDoDefault,
  // we always coalesce them to eConsumeDoDefault.
  if (mApzResponse != aApzResponse) {
    if (mApzResponse == nsEventStatus::nsEventStatus_eIgnore &&
        aApzResponse == nsEventStatus::nsEventStatus_eConsumeDoDefault) {
      mApzResponse = aApzResponse;
    } else if (mApzResponse != nsEventStatus::nsEventStatus_eConsumeDoDefault ||
               aApzResponse != nsEventStatus::nsEventStatus_eIgnore) {
      return false;
    }
  }

  return true;
}

Touch* CoalescedTouchData::GetTouch(int32_t aIdentifier) {
  for (const RefPtr<Touch>& touch : mCoalescedInputEvent->mTouches) {
    if (touch->Identifier() == aIdentifier) {
      return touch;
    }
  }
  return nullptr;
}

void CoalescedTouchMoveFlusher::WillRefresh(mozilla::TimeStamp aTime) {
  MOZ_ASSERT(mRefreshDriver);
  mBrowserChild->ProcessPendingColaescedTouchData();
}

CoalescedTouchMoveFlusher::CoalescedTouchMoveFlusher(
    BrowserChild* aBrowserChild)
    : CoalescedInputFlusher(aBrowserChild) {}

CoalescedTouchMoveFlusher::~CoalescedTouchMoveFlusher() { RemoveObserver(); }
