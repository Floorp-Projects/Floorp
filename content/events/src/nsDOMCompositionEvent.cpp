/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMCompositionEvent.h"
#include "nsDOMClassInfoID.h"

nsDOMCompositionEvent::nsDOMCompositionEvent(mozilla::dom::EventTarget* aOwner,
                                             nsPresContext* aPresContext,
                                             nsCompositionEvent* aEvent)
  : nsDOMUIEvent(aOwner, aPresContext, aEvent ? aEvent :
                 new nsCompositionEvent(false, 0, nullptr))
{
  NS_ASSERTION(mEvent->eventStructType == NS_COMPOSITION_EVENT,
               "event type mismatch");

  if (aEvent) {
    mEventIsInternal = false;
  } else {
    mEventIsInternal = true;
    mEvent->time = PR_Now();

    // XXX compositionstart is cancelable in draft of DOM3 Events.
    //     However, it doesn't make sence for us, we cannot cancel composition
    //     when we sends compositionstart event.
    mEvent->mFlags.mCancelable = false;
  }

  mData = static_cast<nsCompositionEvent*>(mEvent)->data;
  // TODO: Native event should have locale information.

  SetIsDOMBinding();
}

nsDOMCompositionEvent::~nsDOMCompositionEvent()
{
  if (mEventIsInternal) {
    delete static_cast<nsCompositionEvent*>(mEvent);
    mEvent = nullptr;
  }
}

NS_IMPL_ADDREF_INHERITED(nsDOMCompositionEvent, nsDOMUIEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMCompositionEvent, nsDOMUIEvent)

DOMCI_DATA(CompositionEvent, nsDOMCompositionEvent)

NS_INTERFACE_MAP_BEGIN(nsDOMCompositionEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCompositionEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(CompositionEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMUIEvent)

NS_IMETHODIMP
nsDOMCompositionEvent::GetData(nsAString& aData)
{
  aData = mData;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMCompositionEvent::GetLocale(nsAString& aLocale)
{
  aLocale = mLocale;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMCompositionEvent::InitCompositionEvent(const nsAString& aType,
                                            bool aCanBubble,
                                            bool aCancelable,
                                            nsIDOMWindow* aView,
                                            const nsAString& aData,
                                            const nsAString& aLocale)
{
  nsresult rv =
    nsDOMUIEvent::InitUIEvent(aType, aCanBubble, aCancelable, aView, 0);
  NS_ENSURE_SUCCESS(rv, rv);

  mData = aData;
  mLocale = aLocale;
  return NS_OK;
}

nsresult
NS_NewDOMCompositionEvent(nsIDOMEvent** aInstancePtrResult,
                          mozilla::dom::EventTarget* aOwner,
                          nsPresContext* aPresContext,
                          nsCompositionEvent *aEvent)
{
  nsDOMCompositionEvent* event =
    new nsDOMCompositionEvent(aOwner, aPresContext, aEvent);
  return CallQueryInterface(event, aInstancePtrResult);
}
