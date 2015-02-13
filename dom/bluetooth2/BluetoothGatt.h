/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothgatt_h__
#define mozilla_dom_bluetooth_bluetoothgatt_h__

#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/BluetoothGattBinding.h"
#include "mozilla/dom/bluetooth/BluetoothCommon.h"
#include "nsCOMPtr.h"

namespace mozilla {
namespace dom {
class Promise;
}
}

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothReplyRunnable;
class BluetoothService;
class BluetoothSignal;
class BluetoothValue;

class BluetoothGatt MOZ_FINAL : public DOMEventTargetHelper
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

  /****************************************************************************
   * Event Handlers
   ***************************************************************************/
  IMPL_EVENT_HANDLER(connectionstatechanged);

  /****************************************************************************
   * Methods (Web API Implementation)
   ***************************************************************************/
  already_AddRefed<Promise> Connect(ErrorResult& aRv);
  already_AddRefed<Promise> Disconnect(ErrorResult& aRv);

  /****************************************************************************
   * Others
   ***************************************************************************/
  void Notify(const BluetoothSignal& aParam); // BluetoothSignalObserver

  nsPIDOMWindow* GetParentObject() const
  {
     return GetOwner();
  }

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;
  virtual void DisconnectFromOwner() MOZ_OVERRIDE;

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
   * Generate a random uuid.
   *
   * @param aUuidString [out] String to store the generated uuid.
   */
  void GenerateUuid(nsAString &aUuidString);

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
};

END_BLUETOOTH_NAMESPACE

#endif
