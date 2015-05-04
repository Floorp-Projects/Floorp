/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothgatt_h__
#define mozilla_dom_bluetooth_bluetoothgatt_h__

#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/BluetoothGattBinding.h"
#include "mozilla/dom/bluetooth/BluetoothCommon.h"
#include "mozilla/dom/bluetooth/BluetoothGattService.h"
#include "nsCOMPtr.h"

namespace mozilla {
namespace dom {
class Promise;
}
}

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothSignal;
class BluetoothValue;

class BluetoothGatt final : public DOMEventTargetHelper
                          , public BluetoothSignalObserver
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(BluetoothGatt, DOMEventTargetHelper)

  /****************************************************************************
   * Attribute Getters
   ***************************************************************************/
  BluetoothConnectionState ConnectionState() const
  {
    return mConnectionState;
  }

  void GetServices(nsTArray<nsRefPtr<BluetoothGattService>>& aServices) const
  {
    aServices = mServices;
  }

  /****************************************************************************
   * Event Handlers
   ***************************************************************************/
  IMPL_EVENT_HANDLER(characteristicchanged);
  IMPL_EVENT_HANDLER(connectionstatechanged);

  /****************************************************************************
   * Methods (Web API Implementation)
   ***************************************************************************/
  already_AddRefed<Promise> Connect(ErrorResult& aRv);
  already_AddRefed<Promise> Disconnect(ErrorResult& aRv);
  already_AddRefed<Promise> DiscoverServices(ErrorResult& aRv);
  already_AddRefed<Promise> ReadRemoteRssi(ErrorResult& aRv);

  /****************************************************************************
   * Others
   ***************************************************************************/
  void Notify(const BluetoothSignal& aParam); // BluetoothSignalObserver

  nsPIDOMWindow* GetParentObject() const
  {
     return GetOwner();
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;
  virtual void DisconnectFromOwner() override;

  BluetoothGatt(nsPIDOMWindow* aOwner,
                const nsAString& aDeviceAddr);

private:
  ~BluetoothGatt();

  /**
   * Update mConnectionState to aState and fire
   * connectionstatechanged event to the application.
   *
   * @param aState [in] New connection state
   */
  void UpdateConnectionState(BluetoothConnectionState aState);

  /**
   * Add newly discovered GATT services into mServices and update the cache
   * value of mServices.
   *
   * @param aValue [in] BluetoothValue which contains an array of
   *                    BluetoothGattServiceId of all discovered services.
   */
  void HandleServicesDiscovered(const BluetoothValue& aValue);

  /**
   * The value of a GATT characteristic has changed. In the mean time, the
   * cached value of this GATT characteristic has already been updated. An
   * 'characteristicchanged' event will be fired by this function.
   *
   * @param aValue [in] BluetoothValue which contains an array of
   *                    BluetoothNamedValue. There are exact two elements in
   *                    the array. The first element uses 'serviceId' as the
   *                    name and uses BluetoothGattServiceId as the value. The
   *                    second element uses 'charId' as the name and uses
   *                    BluetoothGattId as the value.
   */
  void HandleCharacteristicChanged(const BluetoothValue& aValue);

  /****************************************************************************
   * Variables
   ***************************************************************************/
  /**
   * Random generated UUID of this GATT client.
   */
  nsString mAppUuid;

  /**
   * Id of the GATT client interface given by bluetooth stack.
   * 0 if the client is not registered yet, nonzero otherwise.
   */
  int mClientIf;

  /**
   * Connection state of this remote device.
   */
  BluetoothConnectionState mConnectionState;

  /**
   * Address of the remote device.
   */
  nsString mDeviceAddr;

  /**
   * Array of discovered services from the remote GATT server.
   */
  nsTArray<nsRefPtr<BluetoothGattService>> mServices;

  /**
   * Indicate whether there is ongoing discoverServices request or not.
   */
  bool mDiscoveringServices;
};

END_BLUETOOTH_NAMESPACE

#endif
