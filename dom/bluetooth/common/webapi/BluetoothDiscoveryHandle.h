/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_BluetoothDiscoveryHandle_h
#define mozilla_dom_bluetooth_BluetoothDiscoveryHandle_h

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

  static already_AddRefed<BluetoothDiscoveryHandle>
    Create(nsPIDOMWindowInner* aWindow);

  static already_AddRefed<BluetoothDiscoveryHandle>
    Create(nsPIDOMWindowInner* aWindow,
           const nsTArray<BluetoothUuid>& aServiceUuids,
           const BluetoothUuid& aLeScanUuid);

  void DispatchDeviceEvent(BluetoothDevice* aDevice);

  void DispatchLeDeviceEvent(BluetoothDevice* aLeDevice,
                             int32_t aRssi,
                             nsTArray<uint8_t>& aScanRecord);

  IMPL_EVENT_HANDLER(devicefound);

  void GetLeScanUuid(BluetoothUuid& aLeScanUuid) const
  {
    aLeScanUuid = mLeScanUuid;
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

private:
  BluetoothDiscoveryHandle(nsPIDOMWindowInner* aWindow);

  BluetoothDiscoveryHandle(nsPIDOMWindowInner* aWindow,
                           const nsTArray<BluetoothUuid>& aServiceUuids,
                           const BluetoothUuid& aLeScanUuid);

  ~BluetoothDiscoveryHandle();

  /**
   * Random generated UUID of LE scan
   *
   * This UUID is used only when the handle is built for LE scan.
   * If BluetoothDiscoveryHandle is built for classic discovery, the value would
   * remain empty string during the entire life cycle.
   */
  BluetoothUuid mLeScanUuid;

  /**
   * A BluetoothUuid array of service UUIDs to discover / scan for.
   *
   * This array is only used by LE scan. If BluetoothDiscoveryHandle is built
   * for classic discovery, the array should be empty.
   */
  nsTArray<BluetoothUuid> mServiceUuids;
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_BluetoothDiscoveryHandle_h
