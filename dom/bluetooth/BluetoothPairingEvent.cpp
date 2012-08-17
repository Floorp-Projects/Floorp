/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "BluetoothPairingEvent.h"

#include "nsDOMClassInfo.h"

USING_BLUETOOTH_NAMESPACE

// static
already_AddRefed<BluetoothPairingEvent>
BluetoothPairingEvent::Create(const nsAString& aDeviceAddress, const PRUint32& aPasskey)
{
  nsRefPtr<BluetoothPairingEvent> event = new BluetoothPairingEvent();

  event->mPasskey = aPasskey;
  event->mDeviceAddress = aDeviceAddress;

  return event.forget();
}

// static
already_AddRefed<BluetoothPairingEvent>
BluetoothPairingEvent::Create(const nsAString& aDeviceAddress, const nsAString& aUuid)
{
  nsRefPtr<BluetoothPairingEvent> event = new BluetoothPairingEvent();

  event->mUuid = aUuid;
  event->mDeviceAddress = aDeviceAddress;

  return event.forget();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(BluetoothPairingEvent)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(BluetoothPairingEvent,
                                                  nsDOMEvent)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(BluetoothPairingEvent,
                                                nsDOMEvent)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(BluetoothPairingEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMBluetoothPairingEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(BluetoothPairingEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMPL_ADDREF_INHERITED(BluetoothPairingEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(BluetoothPairingEvent, nsDOMEvent)

DOMCI_DATA(BluetoothPairingEvent, BluetoothPairingEvent)

NS_IMETHODIMP
BluetoothPairingEvent::GetPasskey(PRUint32* aPasskey)
{
  *aPasskey = mPasskey;
  return NS_OK;
}

NS_IMETHODIMP
BluetoothPairingEvent::GetUuid(nsAString& aUuid)
{
  aUuid = mUuid;
  return NS_OK;
}

NS_IMETHODIMP
BluetoothPairingEvent::GetDeviceAddress(nsAString& aDeviceAddress)
{
  aDeviceAddress = mDeviceAddress;
  return NS_OK;
}

