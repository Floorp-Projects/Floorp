/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMClassInfoID.h"
#include "nsDOMHashChangeEvent.h"
#include "DictionaryHelpers.h"

NS_IMPL_ADDREF_INHERITED(nsDOMHashChangeEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMHashChangeEvent, nsDOMEvent)

DOMCI_DATA(HashChangeEvent, nsDOMHashChangeEvent)

NS_INTERFACE_MAP_BEGIN(nsDOMHashChangeEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHashChangeEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(HashChangeEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

nsDOMHashChangeEvent::~nsDOMHashChangeEvent()
{
}

NS_IMETHODIMP
nsDOMHashChangeEvent::GetOldURL(nsAString &aURL)
{
  aURL.Assign(mOldURL);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMHashChangeEvent::GetNewURL(nsAString &aURL)
{
  aURL.Assign(mNewURL);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMHashChangeEvent::InitHashChangeEvent(const nsAString &aTypeArg,
                                          bool aCanBubbleArg,
                                          bool aCancelableArg,
                                          const nsAString &aOldURL,
                                          const nsAString &aNewURL)
{
  nsresult rv = nsDOMEvent::InitEvent(aTypeArg, aCanBubbleArg, aCancelableArg);
  NS_ENSURE_SUCCESS(rv, rv);

  mOldURL.Assign(aOldURL);
  mNewURL.Assign(aNewURL);
  return NS_OK;
}

nsresult
nsDOMHashChangeEvent::InitFromCtor(const nsAString& aType,
                                   JSContext* aCx, jsval* aVal)
{
  mozilla::dom::HashChangeEventInit d;
  nsresult rv = d.Init(aCx, aVal);
  NS_ENSURE_SUCCESS(rv, rv);
  return InitHashChangeEvent(aType, d.bubbles, d.cancelable, d.oldURL, d.newURL);
}

nsresult NS_NewDOMHashChangeEvent(nsIDOMEvent** aInstancePtrResult,
                                nsPresContext* aPresContext,
                                nsEvent* aEvent)
{
  nsDOMHashChangeEvent* event =
    new nsDOMHashChangeEvent(aPresContext, aEvent);

  return CallQueryInterface(event, aInstancePtrResult);
}
