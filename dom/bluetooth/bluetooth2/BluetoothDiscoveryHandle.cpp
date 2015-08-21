/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothService.h"
#include "mozilla/dom/BluetoothDeviceEvent.h"
#include "mozilla/dom/BluetoothDiscoveryHandleBinding.h"
#include "mozilla/dom/bluetooth/BluetoothCommon.h"
#include "mozilla/dom/bluetooth/BluetoothDiscoveryHandle.h"
#include "mozilla/dom/bluetooth/BluetoothLeDeviceEvent.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "nsThreadUtils.h"

USING_BLUETOOTH_NAMESPACE

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(BluetoothDiscoveryHandle)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(BluetoothDiscoveryHandle, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(BluetoothDiscoveryHandle, DOMEventTargetHelper)

BluetoothDiscoveryHandle::BluetoothDiscoveryHandle(nsPIDOMWindow* aWindow)
  : DOMEventTargetHelper(aWindow)
{
  MOZ_ASSERT(aWindow);
}

BluetoothDiscoveryHandle::BluetoothDiscoveryHandle(
  nsPIDOMWindow* aWindow,
  const nsTArray<nsString>& aServiceUuids,
  const nsAString& aLeScanUuid)
  : DOMEventTargetHelper(aWindow)
  , mLeScanUuid(aLeScanUuid)
  , mServiceUuids(aServiceUuids)
{
  MOZ_ASSERT(aWindow);
}

BluetoothDiscoveryHandle::~BluetoothDiscoveryHandle()
{
}

// static
already_AddRefed<BluetoothDiscoveryHandle>
BluetoothDiscoveryHandle::Create(nsPIDOMWindow* aWindow)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);

  nsRefPtr<BluetoothDiscoveryHandle> handle =
    new BluetoothDiscoveryHandle(aWindow);
  return handle.forget();
}

already_AddRefed<BluetoothDiscoveryHandle>
BluetoothDiscoveryHandle::Create(
  nsPIDOMWindow* aWindow,
  const nsTArray<nsString>& aServiceUuids,
  const nsAString& aLeScanUuid)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);

  nsRefPtr<BluetoothDiscoveryHandle> handle =
    new BluetoothDiscoveryHandle(aWindow, aServiceUuids, aLeScanUuid);
  return handle.forget();
}

void
BluetoothDiscoveryHandle::DispatchDeviceEvent(BluetoothDevice* aDevice)
{
  MOZ_ASSERT(aDevice);

  BluetoothDeviceEventInit init;
  init.mDevice = aDevice;

  nsRefPtr<BluetoothDeviceEvent> event =
    BluetoothDeviceEvent::Constructor(this,
                                      NS_LITERAL_STRING("devicefound"),
                                      init);
  DispatchTrustedEvent(event);
}

void
BluetoothDiscoveryHandle::DispatchLeDeviceEvent(BluetoothDevice* aLeDevice,
  int32_t aRssi, nsTArray<uint8_t>& aScanRecord)
{
  MOZ_ASSERT(aLeDevice);

  nsTArray<nsString> remoteUuids;
  aLeDevice->GetUuids(remoteUuids);

  bool hasUuidsFilter = !mServiceUuids.IsEmpty();
  bool noAdvertisingUuid  = remoteUuids.IsEmpty();
  // If an LE device doesn't advertise its service UUIDs, it can't possibly pass
  // the UUIDs filter.
  if (hasUuidsFilter && noAdvertisingUuid) {
    return;
  }

  // The web API startLeScan() makes the device's adapter start seeking for
  // remote LE devices advertising given service UUIDs.
  // Since current Bluetooth stack can't filter the results of LeScan by UUIDs,
  // gecko has to filter the results and dispatch what API asked for.
  bool matched = false;
  for (size_t index = 0; index < remoteUuids.Length(); ++index) {
    if (mServiceUuids.Contains(remoteUuids[index])) {
      matched = true;
      break;
    }
  }

  // Dispatch 'devicefound' event only if
  //  - the service UUID in the scan record matches one of the given UUIDs.
  //  - the given UUIDs is empty.
  if (matched || mServiceUuids.IsEmpty()) {
    nsRefPtr<BluetoothLeDeviceEvent> event =
      BluetoothLeDeviceEvent::Constructor(this,
                                          NS_LITERAL_STRING("devicefound"),
                                          aLeDevice,
                                          aRssi,
                                          aScanRecord);
    DispatchTrustedEvent(event);
  }
}

JSObject*
BluetoothDiscoveryHandle::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto)
{
  return BluetoothDiscoveryHandleBinding::Wrap(aCx, this, aGivenProto);
}
