/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMSettingsEvent_h__
#define nsDOMSettingsEvent_h__

#include "nsIDOMSettingsManager.h"
#include "nsDOMEvent.h"

class nsDOMMozSettingsEvent : public nsDOMEvent,
                              public nsIDOMMozSettingsEvent
{
public:
  nsDOMMozSettingsEvent(nsPresContext* aPresContext, nsEvent* aEvent)
    : nsDOMEvent(aPresContext, aEvent) {}
                     
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsDOMMozSettingsEvent, nsDOMEvent)
  // Forward to base class
  NS_FORWARD_TO_NSDOMEVENT

  NS_DECL_NSIDOMMOZSETTINGSEVENT

  virtual nsresult InitFromCtor(const nsAString& aType,
                                JSContext* aCx, jsval* aVal);
private:
  nsString mSettingName;
  nsCOMPtr<nsIVariant> mSettingValue;
};

#endif // nsDOMSettingsEvent_h__
