/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMSimpleGestureEvent.h"
#include "prtime.h"
#include "mozilla/TouchEvents.h"

using namespace mozilla;

nsDOMSimpleGestureEvent::nsDOMSimpleGestureEvent(mozilla::dom::EventTarget* aOwner,
                                                 nsPresContext* aPresContext,
                                                 WidgetSimpleGestureEvent* aEvent)
  : nsDOMMouseEvent(aOwner, aPresContext,
                    aEvent ? aEvent :
                             new WidgetSimpleGestureEvent(false, 0, nullptr,
                                                          0, 0.0))
{
  NS_ASSERTION(mEvent->eventStructType == NS_SIMPLE_GESTURE_EVENT, "event type mismatch");

  if (aEvent) {
    mEventIsInternal = false;
  } else {
    mEventIsInternal = true;
    mEvent->time = PR_Now();
    mEvent->refPoint.x = mEvent->refPoint.y = 0;
    static_cast<WidgetMouseEventBase*>(mEvent)->inputSource =
      nsIDOMMouseEvent::MOZ_SOURCE_UNKNOWN;
  }
}

NS_IMPL_ADDREF_INHERITED(nsDOMSimpleGestureEvent, nsDOMUIEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMSimpleGestureEvent, nsDOMUIEvent)

NS_INTERFACE_MAP_BEGIN(nsDOMSimpleGestureEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSimpleGestureEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMMouseEvent)

/* attribute unsigned long allowedDirections; */
uint32_t
nsDOMSimpleGestureEvent::AllowedDirections()
{
  return mEvent->AsSimpleGestureEvent()->allowedDirections;
}

NS_IMETHODIMP
nsDOMSimpleGestureEvent::GetAllowedDirections(uint32_t *aAllowedDirections)
{
  NS_ENSURE_ARG_POINTER(aAllowedDirections);
  *aAllowedDirections = AllowedDirections();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMSimpleGestureEvent::SetAllowedDirections(uint32_t aAllowedDirections)
{
  mEvent->AsSimpleGestureEvent()->allowedDirections = aAllowedDirections;
  return NS_OK;
}

/* readonly attribute unsigned long direction; */
uint32_t
nsDOMSimpleGestureEvent::Direction()
{
  return mEvent->AsSimpleGestureEvent()->direction;
}

NS_IMETHODIMP
nsDOMSimpleGestureEvent::GetDirection(uint32_t *aDirection)
{
  NS_ENSURE_ARG_POINTER(aDirection);
  *aDirection = Direction();
  return NS_OK;
}

/* readonly attribute float delta; */
double
nsDOMSimpleGestureEvent::Delta()
{
  return mEvent->AsSimpleGestureEvent()->delta;
}

NS_IMETHODIMP
nsDOMSimpleGestureEvent::GetDelta(double *aDelta)
{
  NS_ENSURE_ARG_POINTER(aDelta);
  *aDelta = Delta();
  return NS_OK;
}

/* readonly attribute unsigned long clickCount; */
uint32_t
nsDOMSimpleGestureEvent::ClickCount()
{
  return mEvent->AsSimpleGestureEvent()->clickCount;
}

NS_IMETHODIMP
nsDOMSimpleGestureEvent::GetClickCount(uint32_t *aClickCount)
{
  NS_ENSURE_ARG_POINTER(aClickCount);
  *aClickCount = ClickCount();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMSimpleGestureEvent::InitSimpleGestureEvent(const nsAString& aTypeArg,
                                                bool aCanBubbleArg,
                                                bool aCancelableArg,
                                                nsIDOMWindow* aViewArg,
                                                int32_t aDetailArg,
                                                int32_t aScreenX, 
                                                int32_t aScreenY,
                                                int32_t aClientX,
                                                int32_t aClientY,
                                                bool aCtrlKeyArg,
                                                bool aAltKeyArg,
                                                bool aShiftKeyArg,
                                                bool aMetaKeyArg,
                                                uint16_t aButton,
                                                nsIDOMEventTarget* aRelatedTarget,
                                                uint32_t aAllowedDirectionsArg,
                                                uint32_t aDirectionArg,
                                                double aDeltaArg,
                                                uint32_t aClickCountArg)
{
  nsresult rv = nsDOMMouseEvent::InitMouseEvent(aTypeArg,
                                                aCanBubbleArg,
                                                aCancelableArg,
                                                aViewArg,
                                                aDetailArg,
                                                aScreenX, 
                                                aScreenY,
                                                aClientX,
                                                aClientY,
                                                aCtrlKeyArg,
                                                aAltKeyArg,
                                                aShiftKeyArg,
                                                aMetaKeyArg,
                                                aButton,
                                                aRelatedTarget);
  NS_ENSURE_SUCCESS(rv, rv);

  WidgetSimpleGestureEvent* simpleGestureEvent = mEvent->AsSimpleGestureEvent();
  simpleGestureEvent->allowedDirections = aAllowedDirectionsArg;
  simpleGestureEvent->direction = aDirectionArg;
  simpleGestureEvent->delta = aDeltaArg;
  simpleGestureEvent->clickCount = aClickCountArg;

  return NS_OK;
}

nsresult NS_NewDOMSimpleGestureEvent(nsIDOMEvent** aInstancePtrResult,
                                     mozilla::dom::EventTarget* aOwner,
                                     nsPresContext* aPresContext,
                                     WidgetSimpleGestureEvent* aEvent)
{
  nsDOMSimpleGestureEvent* it =
    new nsDOMSimpleGestureEvent(aOwner, aPresContext, aEvent);
  return CallQueryInterface(it, aInstancePtrResult);
}
