/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MouseEventBinding.h"
#include "mozilla/dom/SimpleGestureEvent.h"
#include "mozilla/TouchEvents.h"
#include "prtime.h"

namespace mozilla {
namespace dom {

SimpleGestureEvent::SimpleGestureEvent(EventTarget* aOwner,
                                       nsPresContext* aPresContext,
                                       WidgetSimpleGestureEvent* aEvent)
    : MouseEvent(
          aOwner, aPresContext,
          aEvent ? aEvent
                 : new WidgetSimpleGestureEvent(false, eVoidEvent, nullptr)) {
  NS_ASSERTION(mEvent->mClass == eSimpleGestureEventClass,
               "event type mismatch");

  if (aEvent) {
    mEventIsInternal = false;
  } else {
    mEventIsInternal = true;
    mEvent->mTime = PR_Now();
    mEvent->mRefPoint = LayoutDeviceIntPoint(0, 0);
    static_cast<WidgetMouseEventBase*>(mEvent)->mInputSource =
        MouseEvent_Binding::MOZ_SOURCE_UNKNOWN;
  }
}

uint32_t SimpleGestureEvent::AllowedDirections() const {
  return mEvent->AsSimpleGestureEvent()->mAllowedDirections;
}

void SimpleGestureEvent::SetAllowedDirections(uint32_t aAllowedDirections) {
  mEvent->AsSimpleGestureEvent()->mAllowedDirections = aAllowedDirections;
}

uint32_t SimpleGestureEvent::Direction() const {
  return mEvent->AsSimpleGestureEvent()->mDirection;
}

double SimpleGestureEvent::Delta() const {
  return mEvent->AsSimpleGestureEvent()->mDelta;
}

uint32_t SimpleGestureEvent::ClickCount() const {
  return mEvent->AsSimpleGestureEvent()->mClickCount;
}

void SimpleGestureEvent::InitSimpleGestureEvent(
    const nsAString& aTypeArg, bool aCanBubbleArg, bool aCancelableArg,
    nsGlobalWindowInner* aViewArg, int32_t aDetailArg, int32_t aScreenX,
    int32_t aScreenY, int32_t aClientX, int32_t aClientY, bool aCtrlKeyArg,
    bool aAltKeyArg, bool aShiftKeyArg, bool aMetaKeyArg, uint16_t aButton,
    EventTarget* aRelatedTarget, uint32_t aAllowedDirectionsArg,
    uint32_t aDirectionArg, double aDeltaArg, uint32_t aClickCountArg) {
  NS_ENSURE_TRUE_VOID(!mEvent->mFlags.mIsBeingDispatched);

  MouseEvent::InitMouseEvent(aTypeArg, aCanBubbleArg, aCancelableArg, aViewArg,
                             aDetailArg, aScreenX, aScreenY, aClientX, aClientY,
                             aCtrlKeyArg, aAltKeyArg, aShiftKeyArg, aMetaKeyArg,
                             aButton, aRelatedTarget);

  WidgetSimpleGestureEvent* simpleGestureEvent = mEvent->AsSimpleGestureEvent();
  simpleGestureEvent->mAllowedDirections = aAllowedDirectionsArg;
  simpleGestureEvent->mDirection = aDirectionArg;
  simpleGestureEvent->mDelta = aDeltaArg;
  simpleGestureEvent->mClickCount = aClickCountArg;
}

}  // namespace dom
}  // namespace mozilla

using namespace mozilla;
using namespace mozilla::dom;

already_AddRefed<SimpleGestureEvent> NS_NewDOMSimpleGestureEvent(
    EventTarget* aOwner, nsPresContext* aPresContext,
    WidgetSimpleGestureEvent* aEvent) {
  RefPtr<SimpleGestureEvent> it =
      new SimpleGestureEvent(aOwner, aPresContext, aEvent);
  return it.forget();
}
