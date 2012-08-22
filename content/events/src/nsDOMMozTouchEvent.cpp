/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMClassInfoID.h"
#include "nsDOMMozTouchEvent.h"

nsDOMMozTouchEvent::nsDOMMozTouchEvent(nsPresContext* aPresContext, nsMozTouchEvent* aEvent)
  : nsDOMMouseEvent(aPresContext, aEvent ? aEvent : new nsMozTouchEvent(false, 0, nullptr, 0))
{
  NS_ASSERTION(mEvent->eventStructType == NS_MOZTOUCH_EVENT, "event type mismatch NS_MOZTOUCH_EVENT");

  if (aEvent) {
    mEventIsInternal = false;
  } else {
    mEventIsInternal = true;
    mEvent->time = PR_Now();
    mEvent->refPoint.x = mEvent->refPoint.y = 0;
  }
}

nsDOMMozTouchEvent::~nsDOMMozTouchEvent()
{
  if (mEventIsInternal) {
    delete static_cast<nsMozTouchEvent*>(mEvent);
    mEvent = nullptr;
  }
}

NS_IMPL_ADDREF_INHERITED(nsDOMMozTouchEvent, nsDOMMouseEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMMozTouchEvent, nsDOMMouseEvent)

DOMCI_DATA(MozTouchEvent, nsDOMMozTouchEvent)

NS_INTERFACE_MAP_BEGIN(nsDOMMozTouchEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozTouchEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MozTouchEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMMouseEvent)

/* readonly attribute unsigned long steramId; */
NS_IMETHODIMP
nsDOMMozTouchEvent::GetStreamId(uint32_t *aStreamId)
{
  NS_ENSURE_ARG_POINTER(aStreamId);
  *aStreamId = static_cast<nsMozTouchEvent*>(mEvent)->streamId;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMozTouchEvent::InitMozTouchEvent(const nsAString& aTypeArg,
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
                                      uint32_t aStreamId)
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

  nsMozTouchEvent* mozTouchEvent = static_cast<nsMozTouchEvent*>(mEvent);
  mozTouchEvent->streamId = aStreamId;

  return NS_OK;
}

nsresult NS_NewDOMMozTouchEvent(nsIDOMEvent** aInstancePtrResult,
                                     nsPresContext* aPresContext,
                                     nsMozTouchEvent *aEvent)
{
  nsDOMMozTouchEvent *it = new nsDOMMozTouchEvent(aPresContext, aEvent);
  return CallQueryInterface(it, aInstancePtrResult);
}
