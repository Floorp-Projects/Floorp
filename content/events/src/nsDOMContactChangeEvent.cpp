/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMContactChangeEvent.h"
#include "nsContentUtils.h"
#include "DictionaryHelpers.h"

DOMCI_DATA(MozContactChangeEvent, nsDOMMozContactChangeEvent)

NS_INTERFACE_MAP_BEGIN(nsDOMMozContactChangeEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozContactChangeEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MozContactChangeEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMPL_ADDREF_INHERITED(nsDOMMozContactChangeEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMMozContactChangeEvent, nsDOMEvent)

NS_IMETHODIMP
nsDOMMozContactChangeEvent::GetContactID(nsAString& aContactID)
{
  aContactID = mContactID;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMozContactChangeEvent::GetReason(nsAString& aReason)
{
  aReason = mReason;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMozContactChangeEvent::InitMozContactChangeEvent(const nsAString& aType,
                                                      bool aCanBubble,
                                                      bool aCancelable,
                                                      const nsAString& aContactID,
                                                      const nsAString& aReason)
{
  nsresult rv = nsDOMEvent::InitEvent(aType, aCanBubble, aCancelable);
  NS_ENSURE_SUCCESS(rv, rv);

  mContactID = aContactID;
  mReason = aReason;

  return NS_OK;
}

nsresult
nsDOMMozContactChangeEvent::InitFromCtor(const nsAString& aType,
                                         JSContext* aCx, jsval* aVal)
{
  mozilla::dom::MozContactChangeEventInit d;
  nsresult rv = d.Init(aCx, aVal);
  NS_ENSURE_SUCCESS(rv, rv);
  return InitMozContactChangeEvent(aType, d.bubbles, d.cancelable, d.contactID, d.reason);
}

nsresult
NS_NewDOMMozContactChangeEvent(nsIDOMEvent** aInstancePtrResult,
                               nsPresContext* aPresContext,
                               nsEvent* aEvent) 
{
  nsDOMMozContactChangeEvent* e = new nsDOMMozContactChangeEvent(aPresContext, aEvent);
  return CallQueryInterface(e, aInstancePtrResult);
}
