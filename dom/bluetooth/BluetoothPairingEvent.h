/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_requestpairingevent_h__
#define mozilla_dom_bluetooth_requestpairingevent_h__

#include "BluetoothCommon.h"

#include "nsIDOMBluetoothPairingEvent.h"
#include "nsIDOMEventTarget.h"

#include "nsDOMEvent.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothPairingEvent : public nsDOMEvent
                            , public nsIDOMBluetoothPairingEvent
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_TO_NSDOMEVENT
  NS_DECL_NSIDOMBLUETOOTHPAIRINGEVENT
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(BluetoothPairingEvent, nsDOMEvent)

  static already_AddRefed<BluetoothPairingEvent>
  Create(const nsAString& aDeviceAddress, const PRUint32& aPasskey);
  
  static already_AddRefed<BluetoothPairingEvent>
  Create(const nsAString& aDeviceAddress, const nsAString& aUuid);

  nsresult
  Dispatch(nsIDOMEventTarget* aTarget, const nsAString& aEventType)
  {
    NS_ASSERTION(aTarget, "Null pointer!");
    NS_ASSERTION(!aEventType.IsEmpty(), "Empty event type!");

    nsresult rv = InitEvent(aEventType, false, false);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = SetTrusted(true);
    NS_ENSURE_SUCCESS(rv, rv);

    nsIDOMEvent* thisEvent = static_cast<nsDOMEvent*>(this);

    bool dummy;
    rv = aTarget->DispatchEvent(thisEvent, &dummy);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

private:
  BluetoothPairingEvent()
    : nsDOMEvent(nullptr, nullptr)
  { }

  ~BluetoothPairingEvent()
  { }

  PRUint32 mPasskey;
  nsString mUuid;
  nsString mDeviceAddress;
};

END_BLUETOOTH_NAMESPACE

#endif
