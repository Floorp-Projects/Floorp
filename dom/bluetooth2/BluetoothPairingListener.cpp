/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/bluetooth/BluetoothPairingListener.h"
#include "mozilla/dom/bluetooth/BluetoothPairingHandle.h"
#include "mozilla/dom/BluetoothPairingEvent.h"
#include "mozilla/dom/BluetoothPairingListenerBinding.h"

USING_BLUETOOTH_NAMESPACE

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(BluetoothPairingListener)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(BluetoothPairingListener, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(BluetoothPairingListener, DOMEventTargetHelper)

BluetoothPairingListener::BluetoothPairingListener(nsPIDOMWindow* aWindow)
  : DOMEventTargetHelper(aWindow)
{
  MOZ_ASSERT(aWindow);
}

already_AddRefed<BluetoothPairingListener>
BluetoothPairingListener::Create(nsPIDOMWindow* aWindow)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);

  nsRefPtr<BluetoothPairingListener> handle =
    new BluetoothPairingListener(aWindow);

  return handle.forget();
}

BluetoothPairingListener::~BluetoothPairingListener()
{
}

void
BluetoothPairingListener::DispatchPairingEvent(BluetoothDevice* aDevice,
                                               const nsAString& aPasskey,
                                               const nsAString& aType)
{
  MOZ_ASSERT(aDevice && !aType.IsEmpty());

  nsString address;
  aDevice->GetAddress(address);

  nsRefPtr<BluetoothPairingHandle> handle =
    BluetoothPairingHandle::Create(GetOwner(),
                                   address,
                                   aType,
                                   aPasskey);

  BluetoothPairingEventInit init;
  init.mDevice = aDevice;
  init.mHandle = handle;

  nsRefPtr<BluetoothPairingEvent> event =
    BluetoothPairingEvent::Constructor(this,
                                       aType,
                                       init);

  DispatchTrustedEvent(event);
}

JSObject*
BluetoothPairingListener::WrapObject(JSContext* aCx)
{
  return BluetoothPairingListenerBinding::Wrap(aCx, this);
}
