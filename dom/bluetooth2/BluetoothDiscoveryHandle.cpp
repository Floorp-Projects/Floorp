/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothDiscoveryHandle.h"
#include "BluetoothService.h"

#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "mozilla/dom/BluetoothDeviceEvent.h"
#include "mozilla/dom/BluetoothDiscoveryHandleBinding.h"
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
  MOZ_ASSERT(IsDOMBinding());
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

JSObject*
BluetoothDiscoveryHandle::WrapObject(JSContext* aCx)
{
  return BluetoothDiscoveryHandleBinding::Wrap(aCx, this);
}
