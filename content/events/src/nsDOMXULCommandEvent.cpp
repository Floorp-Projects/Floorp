/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMXULCommandEvent.h"
#include "prtime.h"

using namespace mozilla;

nsDOMXULCommandEvent::nsDOMXULCommandEvent(mozilla::dom::EventTarget* aOwner,
                                           nsPresContext* aPresContext,
                                           WidgetInputEvent* aEvent)
  : nsDOMUIEvent(aOwner, aPresContext,
                 aEvent ? aEvent : new WidgetInputEvent(false, 0, nullptr))
{
  if (aEvent) {
    mEventIsInternal = false;
  }
  else {
    mEventIsInternal = true;
    mEvent->time = PR_Now();
  }
}

NS_IMPL_ADDREF_INHERITED(nsDOMXULCommandEvent, nsDOMUIEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMXULCommandEvent, nsDOMUIEvent)

NS_IMPL_CYCLE_COLLECTION_INHERITED_1(nsDOMXULCommandEvent, nsDOMUIEvent,
                                     mSourceEvent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsDOMXULCommandEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMXULCommandEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMUIEvent)

NS_IMETHODIMP
nsDOMXULCommandEvent::GetAltKey(bool* aIsDown)
{
  NS_ENSURE_ARG_POINTER(aIsDown);
  *aIsDown = AltKey();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMXULCommandEvent::GetCtrlKey(bool* aIsDown)
{
  NS_ENSURE_ARG_POINTER(aIsDown);
  *aIsDown = CtrlKey();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMXULCommandEvent::GetShiftKey(bool* aIsDown)
{
  NS_ENSURE_ARG_POINTER(aIsDown);
  *aIsDown = ShiftKey();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMXULCommandEvent::GetMetaKey(bool* aIsDown)
{
  NS_ENSURE_ARG_POINTER(aIsDown);
  *aIsDown = MetaKey();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMXULCommandEvent::GetSourceEvent(nsIDOMEvent** aSourceEvent)
{
  NS_ENSURE_ARG_POINTER(aSourceEvent);
  *aSourceEvent = GetSourceEvent().get();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMXULCommandEvent::InitCommandEvent(const nsAString& aType,
                                       bool aCanBubble, bool aCancelable,
                                       nsIDOMWindow* aView,
                                       int32_t aDetail,
                                       bool aCtrlKey, bool aAltKey,
                                       bool aShiftKey, bool aMetaKey,
                                       nsIDOMEvent* aSourceEvent)
{
  nsresult rv = nsDOMUIEvent::InitUIEvent(aType, aCanBubble, aCancelable,
                                          aView, aDetail);
  NS_ENSURE_SUCCESS(rv, rv);

  Event()->InitBasicModifiers(aCtrlKey, aAltKey, aShiftKey, aMetaKey);
  mSourceEvent = aSourceEvent;

  return NS_OK;
}


nsresult NS_NewDOMXULCommandEvent(nsIDOMEvent** aInstancePtrResult,
                                  mozilla::dom::EventTarget* aOwner,
                                  nsPresContext* aPresContext,
                                  WidgetInputEvent* aEvent) 
{
  nsDOMXULCommandEvent* it =
    new nsDOMXULCommandEvent(aOwner, aPresContext, aEvent);
  return CallQueryInterface(it, aInstancePtrResult);
}
