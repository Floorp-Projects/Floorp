/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMDeviceProximityEvent.h"
#include "nsContentUtils.h"
#include "DictionaryHelpers.h"

NS_IMPL_ADDREF_INHERITED(nsDOMDeviceProximityEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMDeviceProximityEvent, nsDOMEvent)

DOMCI_DATA(DeviceProximityEvent, nsDOMDeviceProximityEvent)

NS_INTERFACE_MAP_BEGIN(nsDOMDeviceProximityEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDeviceProximityEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(DeviceProximityEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMETHODIMP
nsDOMDeviceProximityEvent::InitDeviceProximityEvent(const nsAString & aEventTypeArg,
                                                    bool aCanBubbleArg,
                                                    bool aCancelableArg,
                                                    double aValue,
                                                    double aMin,
                                                    double aMax)
{
  nsresult rv = nsDOMEvent::InitEvent(aEventTypeArg, aCanBubbleArg, aCancelableArg);
  NS_ENSURE_SUCCESS(rv, rv);
    
  mValue = aValue;
  mMin = aMin;
  mMax = aMax;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDeviceProximityEvent::GetValue(double *aValue)
{
  NS_ENSURE_ARG_POINTER(aValue);

  *aValue = mValue;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDeviceProximityEvent::GetMin(double *aMin)
{
  NS_ENSURE_ARG_POINTER(aMin);

  *aMin = mMin;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDeviceProximityEvent::GetMax(double *aMax)
{
  NS_ENSURE_ARG_POINTER(aMax);

  *aMax = mMax;
  return NS_OK;
}

nsresult
nsDOMDeviceProximityEvent::InitFromCtor(const nsAString& aType,
                                        JSContext* aCx, jsval* aVal)
{
  mozilla::dom::DeviceProximityEventInit d;
  nsresult rv = d.Init(aCx, aVal);
  NS_ENSURE_SUCCESS(rv, rv);
  return InitDeviceProximityEvent(aType, d.bubbles, d.cancelable, d.value, d.min, d.max);
}

nsresult
NS_NewDOMDeviceProximityEvent(nsIDOMEvent** aInstancePtrResult,
                              nsPresContext* aPresContext,
                              nsEvent *aEvent)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);
  nsDOMDeviceProximityEvent* it = new nsDOMDeviceProximityEvent(aPresContext, aEvent);
  return CallQueryInterface(it, aInstancePtrResult);
}
