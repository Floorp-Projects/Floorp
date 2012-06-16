/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMWifiEvent.h"
#include "nsContentUtils.h"
#include "DictionaryHelpers.h"

// nsDOMMozWifiStatusChangeEvent

DOMCI_DATA(MozWifiStatusChangeEvent, nsDOMMozWifiStatusChangeEvent)

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDOMMozWifiStatusChangeEvent)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsDOMMozWifiStatusChangeEvent, nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mNetwork)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsDOMMozWifiStatusChangeEvent, nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mNetwork)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN(nsDOMMozWifiStatusChangeEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozWifiStatusChangeEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MozWifiStatusChangeEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMPL_ADDREF_INHERITED(nsDOMMozWifiStatusChangeEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMMozWifiStatusChangeEvent, nsDOMEvent)

NS_IMETHODIMP
nsDOMMozWifiStatusChangeEvent::GetNetwork(nsIVariant** aNetwork)
{
  NS_IF_ADDREF(*aNetwork = mNetwork);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMozWifiStatusChangeEvent::GetStatus(nsAString& aStatus)
{
  aStatus = mStatus;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMozWifiStatusChangeEvent::InitMozWifiStatusChangeEvent(const nsAString& aType,
                                                            bool aCanBubble,
                                                            bool aCancelable,
                                                            nsIVariant* aNetwork,
                                                            const nsAString& aStatus)
{
  nsresult rv = nsDOMEvent::InitEvent(aType, aCanBubble, aCancelable);
  NS_ENSURE_SUCCESS(rv, rv);

  mNetwork = aNetwork;
  mStatus = aStatus;

  return NS_OK;
}

nsresult
nsDOMMozWifiStatusChangeEvent::InitFromCtor(const nsAString& aType,
                                            JSContext* aCx, jsval* aVal)
{
  mozilla::dom::MozWifiStatusChangeEventInit d;
  nsresult rv = d.Init(aCx, aVal);
  NS_ENSURE_SUCCESS(rv, rv);
  return InitMozWifiStatusChangeEvent(aType, d.bubbles, d.cancelable, d.network, d.status);
}

nsresult
NS_NewDOMMozWifiStatusChangeEvent(nsIDOMEvent** aInstancePtrResult,
                                  nsPresContext* aPresContext,
                                  nsEvent* aEvent) 
{
  nsDOMMozWifiStatusChangeEvent* e = new nsDOMMozWifiStatusChangeEvent(aPresContext, aEvent);
  return CallQueryInterface(e, aInstancePtrResult);
}

// nsDOMMozWifiConnectionInfoEvent
DOMCI_DATA(MozWifiConnectionInfoEvent, nsDOMMozWifiConnectionInfoEvent)

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDOMMozWifiConnectionInfoEvent)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsDOMMozWifiConnectionInfoEvent, nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mNetwork)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsDOMMozWifiConnectionInfoEvent, nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mNetwork)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN(nsDOMMozWifiConnectionInfoEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozWifiConnectionInfoEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MozWifiConnectionInfoEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMPL_ADDREF_INHERITED(nsDOMMozWifiConnectionInfoEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMMozWifiConnectionInfoEvent, nsDOMEvent)

NS_IMETHODIMP
nsDOMMozWifiConnectionInfoEvent::GetNetwork(nsIVariant** aNetwork)
{
  NS_IF_ADDREF(*aNetwork = mNetwork);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMozWifiConnectionInfoEvent::GetSignalStrength(PRInt16* aSignalStrength)
{
  *aSignalStrength = mSignalStrength;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMozWifiConnectionInfoEvent::GetRelSignalStrength(PRInt16* aRelSignalStrength)
{
  *aRelSignalStrength = mRelSignalStrength;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMozWifiConnectionInfoEvent::GetLinkSpeed(PRInt32* aLinkSpeed)
{
  *aLinkSpeed = mLinkSpeed;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMozWifiConnectionInfoEvent::InitMozWifiConnectionInfoEvent(const nsAString& aType,
                                                                bool aCanBubble,
                                                                bool aCancelable,
                                                                nsIVariant *aNetwork,
                                                                PRInt16 aSignalStrength,
                                                                PRInt16 aRelSignalStrength,
                                                                PRInt32 aLinkSpeed)
{
  nsresult rv = nsDOMEvent::InitEvent(aType, aCanBubble, aCancelable);
  NS_ENSURE_SUCCESS(rv, rv);

  mNetwork = aNetwork;
  mSignalStrength = aSignalStrength;
  mRelSignalStrength = aRelSignalStrength;
  mLinkSpeed = aLinkSpeed;

  return NS_OK;
}

nsresult
nsDOMMozWifiConnectionInfoEvent::InitFromCtor(const nsAString& aType,
                                              JSContext* aCx, jsval* aVal)
{
  mozilla::dom::MozWifiConnectionInfoEventInit d;
  nsresult rv = d.Init(aCx, aVal);
  NS_ENSURE_SUCCESS(rv, rv);
  return InitMozWifiConnectionInfoEvent(aType, d.bubbles, d.cancelable, d.network,
                                        d.signalStrength, d.relSignalStrength, d.linkSpeed);
}

nsresult
NS_NewDOMMozWifiConnectionInfoEvent(nsIDOMEvent** aInstancePtrResult,
                                    nsPresContext* aPresContext,
                                    nsEvent* aEvent) 
{
  nsDOMMozWifiConnectionInfoEvent* e = new nsDOMMozWifiConnectionInfoEvent(aPresContext, aEvent);
  return CallQueryInterface(e, aInstancePtrResult);
}
