/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMSettingsEvent.h"
#include "nsContentUtils.h"
#include "DictionaryHelpers.h"

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDOMMozSettingsEvent)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsDOMMozSettingsEvent, nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mSettingValue)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsDOMMozSettingsEvent, nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mSettingValue)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

DOMCI_DATA(MozSettingsEvent, nsDOMMozSettingsEvent)

NS_INTERFACE_MAP_BEGIN(nsDOMMozSettingsEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozSettingsEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MozSettingsEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMPL_ADDREF_INHERITED(nsDOMMozSettingsEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMMozSettingsEvent, nsDOMEvent)

NS_IMETHODIMP
nsDOMMozSettingsEvent::GetSettingName(nsAString & aSettingName)
{
  aSettingName = mSettingName;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMozSettingsEvent::GetSettingValue(nsIVariant** aSettingValue)
{
  NS_IF_ADDREF(*aSettingValue = mSettingValue);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMozSettingsEvent::InitMozSettingsEvent(const nsAString& aType,
                                            bool aCanBubble,
                                            bool aCancelable,
                                            const nsAString &aSettingName,
                                            nsIVariant *aSettingValue)
{
  nsresult rv = nsDOMEvent::InitEvent(aType, aCanBubble, aCancelable);
  NS_ENSURE_SUCCESS(rv, rv);

  mSettingName = aSettingName;
  mSettingValue = aSettingValue;

  return NS_OK;
}

nsresult
nsDOMMozSettingsEvent::InitFromCtor(const nsAString& aType,
                                    JSContext* aCx, jsval* aVal)
{
  mozilla::dom::MozSettingsEventInit d;
  nsresult rv = d.Init(aCx, aVal);
  NS_ENSURE_SUCCESS(rv, rv);
  return InitMozSettingsEvent(aType, d.bubbles, d.cancelable, d.settingName, d.settingValue);
}

nsresult
NS_NewDOMMozSettingsEvent(nsIDOMEvent** aInstancePtrResult,
                          nsPresContext* aPresContext,
                          nsEvent* aEvent) 
{
  nsDOMMozSettingsEvent* e = new nsDOMMozSettingsEvent(aPresContext, aEvent);
  return CallQueryInterface(e, aInstancePtrResult);
}
