/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "BluetoothDeviceEvent.h"
#include "BluetoothTypes.h"
#include "BluetoothDevice.h"
#include "nsIDOMDOMRequest.h"

#include "nsDOMClassInfo.h"

USING_BLUETOOTH_NAMESPACE

// static
already_AddRefed<BluetoothDeviceEvent>
BluetoothDeviceEvent::Create(BluetoothDevice* aDevice)
{
  NS_ASSERTION(aDevice, "Null pointer!");

  nsRefPtr<BluetoothDeviceEvent> event = new BluetoothDeviceEvent();

  event->mDevice = aDevice;

  return event.forget();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(BluetoothDeviceEvent)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(BluetoothDeviceEvent,
                                                  nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mDevice)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(BluetoothDeviceEvent,
                                                nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mDevice)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(BluetoothDeviceEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMBluetoothDeviceEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(BluetoothDeviceEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMPL_ADDREF_INHERITED(BluetoothDeviceEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(BluetoothDeviceEvent, nsDOMEvent)

DOMCI_DATA(BluetoothDeviceEvent, BluetoothDeviceEvent)

NS_IMETHODIMP
BluetoothDeviceEvent::GetDevice(nsIDOMBluetoothDevice** aDevice)
{
  nsCOMPtr<nsIDOMBluetoothDevice> device = mDevice.get();
  device.forget(aDevice);
  return NS_OK;
}
