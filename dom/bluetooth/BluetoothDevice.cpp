/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothDevice.h"
#include "nsDOMClassInfo.h"

USING_BLUETOOTH_NAMESPACE

DOMCI_DATA(BluetoothDevice, BluetoothDevice)

NS_IMPL_CYCLE_COLLECTION_CLASS(BluetoothDevice)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(BluetoothDevice,
                                                  nsDOMEventTargetHelper)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(propertychanged)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(disconnectrequested)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(BluetoothDevice,
                                                nsDOMEventTargetHelper)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(propertychanged)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(disconnectrequested)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(BluetoothDevice)
  NS_INTERFACE_MAP_ENTRY(nsIDOMBluetoothDevice)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMBluetoothDevice)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(BluetoothDevice)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(BluetoothDevice, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(BluetoothDevice, nsDOMEventTargetHelper)

BluetoothDevice::BluetoothDevice()
{
}

nsresult
BluetoothDevice::GetAdapter(nsAString& aAdapter)
{
  aAdapter = mAdapter;
  return NS_OK;
}

nsresult
BluetoothDevice::GetAddress(nsAString& aAddress)
{
  aAddress = mAddress;
  return NS_OK;
}

nsresult
BluetoothDevice::GetName(nsAString& aName)
{
  aName = mName;
  return NS_OK;
}

nsresult
BluetoothDevice::GetClass(PRUint32* aClass)
{
  *aClass = mClass;
  return NS_OK;
}

nsresult
BluetoothDevice::GetConnected(bool* aConnected)
{
  *aConnected = mConnected;
  return NS_OK;
}

nsresult
BluetoothDevice::GetPaired(bool* aPaired)
{
  *aPaired = mPaired;
  return NS_OK;
}

nsresult
BluetoothDevice::GetLegacyPairing(bool* aLegacyPairing)
{
  *aLegacyPairing = mLegacyPairing;
  return NS_OK;
}

nsresult
BluetoothDevice::GetTrusted(bool* aTrusted)
{
  *aTrusted = mTrusted;
  return NS_OK;
}

nsresult
BluetoothDevice::SetTrusted(bool aTrusted)
{
  mTrusted = aTrusted;
  return NS_OK;
}

nsresult
BluetoothDevice::GetAlias(nsAString& aAlias)
{
  aAlias = mAlias;
  return NS_OK;
}

nsresult
BluetoothDevice::SetAlias(const nsAString& aAlias)
{
  mAlias = aAlias;
  return NS_OK;
}

nsresult
BluetoothDevice::GetUuids(jsval* aUuids)
{
  //TODO: convert mUuids to jsval and assign to aUuids;
  return NS_OK;
}

nsresult
BluetoothDevice::Disconnect()
{
  return NS_OK;
}

nsresult
BluetoothDevice::GetProperties(jsval* aProperties)
{
  return NS_OK;
}

nsresult
BluetoothDevice::SetProperty(const nsAString& aName, const nsAString& aValue)
{
  return NS_OK;
}

nsresult
BluetoothDevice::DiscoverServices(const nsAString& aPattern, jsval* aServices)
{
  return NS_OK;
}

nsresult
BluetoothDevice::CancelDiscovery()
{
  return NS_OK;
}

NS_IMPL_EVENT_HANDLER(BluetoothDevice, propertychanged)
NS_IMPL_EVENT_HANDLER(BluetoothDevice, disconnectrequested)
