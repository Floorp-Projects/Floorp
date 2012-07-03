/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMClassInfoID.h"
#include "nsDOMUserProximityEvent.h"
#include "DictionaryHelpers.h"

NS_IMPL_ADDREF_INHERITED(nsDOMUserProximityEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMUserProximityEvent, nsDOMEvent)

DOMCI_DATA(UserProximityEvent, nsDOMUserProximityEvent)

NS_INTERFACE_MAP_BEGIN(nsDOMUserProximityEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMUserProximityEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(UserProximityEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMETHODIMP
nsDOMUserProximityEvent::InitUserProximityEvent(const nsAString & aEventTypeArg,
                                                bool aCanBubbleArg,
                                                bool aCancelableArg,
                                                bool aNear)
{
  nsresult rv = nsDOMEvent::InitEvent(aEventTypeArg, aCanBubbleArg, aCancelableArg);
  NS_ENSURE_SUCCESS(rv, rv);
  mNear = aNear;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMUserProximityEvent::GetNear(bool *aNear)
{
  NS_ENSURE_ARG_POINTER(aNear);
  *aNear = mNear;
  return NS_OK;
}

nsresult
nsDOMUserProximityEvent::InitFromCtor(const nsAString& aType,
                                      JSContext* aCx, jsval* aVal)
{
  mozilla::dom::UserProximityEventInit d;
  nsresult rv = d.Init(aCx, aVal);
  NS_ENSURE_SUCCESS(rv, rv);
  return InitUserProximityEvent(aType, d.bubbles, d.cancelable, d.near);
}

nsresult
NS_NewDOMUserProximityEvent(nsIDOMEvent** aInstancePtrResult,
                            nsPresContext* aPresContext,
                            nsEvent *aEvent) 
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);
  nsDOMUserProximityEvent* it = new nsDOMUserProximityEvent(aPresContext, aEvent);
  return CallQueryInterface(it, aInstancePtrResult);
}
