/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMClassInfoID.h"
#include "nsDOMDeviceLightEvent.h"
#include "DictionaryHelpers.h"

NS_IMPL_ADDREF_INHERITED(nsDOMDeviceLightEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMDeviceLightEvent, nsDOMEvent)

DOMCI_DATA(DeviceLightEvent, nsDOMDeviceLightEvent)

NS_INTERFACE_MAP_BEGIN(nsDOMDeviceLightEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDeviceLightEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(DeviceLightEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMETHODIMP
nsDOMDeviceLightEvent::InitDeviceLightEvent(const nsAString & aEventTypeArg,
                                            bool aCanBubbleArg,
                                            bool aCancelableArg,
                                            double aValue)
{
  nsresult rv = nsDOMEvent::InitEvent(aEventTypeArg, aCanBubbleArg, aCancelableArg);
  NS_ENSURE_SUCCESS(rv, rv);

  mValue = aValue;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDeviceLightEvent::GetValue(double *aValue)
{
  NS_ENSURE_ARG_POINTER(aValue);
  *aValue = mValue;
  return NS_OK;
}

nsresult
nsDOMDeviceLightEvent::InitFromCtor(const nsAString& aType,
                                    JSContext* aCx, jsval* aVal)
{
  mozilla::dom::DeviceLightEventInit d;
  nsresult rv = d.Init(aCx, aVal);
  NS_ENSURE_SUCCESS(rv, rv);
  return InitDeviceLightEvent(aType, d.bubbles, d.cancelable, d.value);
}

nsresult
NS_NewDOMDeviceLightEvent(nsIDOMEvent** aInstancePtrResult,
                          nsPresContext* aPresContext,
                          nsEvent *aEvent) 
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);
  nsDOMDeviceLightEvent* it = new nsDOMDeviceLightEvent(aPresContext, aEvent);
  return CallQueryInterface(it, aInstancePtrResult);
}
