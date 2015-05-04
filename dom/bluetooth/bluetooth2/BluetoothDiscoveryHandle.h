/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothdiscoveryhandle_h
#define mozilla_dom_bluetooth_bluetoothdiscoveryhandle_h

#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/bluetooth/BluetoothAdapter.h"
#include "mozilla/dom/bluetooth/BluetoothCommon.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "nsISupportsImpl.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothDevice;

class BluetoothDiscoveryHandle final : public DOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(BluetoothDiscoveryHandle,
                                           DOMEventTargetHelper)

  static already_AddRefed<BluetoothDiscoveryHandle>
    Create(nsPIDOMWindow* aWindow);

  static already_AddRefed<BluetoothDiscoveryHandle>
    Create(nsPIDOMWindow* aWindow,
           const nsTArray<nsString>& aServiceUuids,
           const nsAString& aLeScanUuid,
           BluetoothAdapter* aAdapter);

  void DispatchDeviceEvent(BluetoothDevice* aDevice);

  void DispatchLeDeviceEvent(BluetoothDevice* aLeDevice,
                             int32_t aRssi,
                             nsTArray<uint8_t>& aScanRecord);

  IMPL_EVENT_HANDLER(devicefound);

  void GetLeScanUuid(nsString& aLeScanUuid) const
  {
    aLeScanUuid = mLeScanUuid;
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  virtual void DisconnectFromOwner() override;

private:
  BluetoothDiscoveryHandle(nsPIDOMWindow* aWindow);

  BluetoothDiscoveryHandle(nsPIDOMWindow* aWindow,
                           const nsTArray<nsString>& aServiceUuids,
                           const nsAString& aLeScanUuid,
                           BluetoothAdapter* aAdapter);

  ~BluetoothDiscoveryHandle();

  /**
   * Random generated UUID of LE scan
   *
   * This UUID is used only when the handle is built for LE scan.
   * If BluetoothDiscoveryHandle is built for classic discovery, the value would
   * remain empty string during the entire life cycle.
   */
  nsString mLeScanUuid;

  /**
   * A DOMString array of service UUIDs to discover / scan for.
   *
   * This array is only used by LE scan. If BluetoothDiscoveryHandle is built
   * for classic discovery, the array should be empty.
   */
  nsTArray<nsString> mServiceUuids;

  /**
   * The adapter which called startLeScan and created this discovery handle
   *
   * If BluetoothDiscoveryHandle is built for classic discovery, this value
   * should be nullptr.
   */
  nsRefPtr<BluetoothAdapter> mAdapter;
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_bluetoothdiscoveryhandle_h
