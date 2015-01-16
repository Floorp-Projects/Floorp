/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/bluetooth/BluetoothPairingListener.h"
#include "mozilla/dom/bluetooth/BluetoothPairingHandle.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "mozilla/dom/BluetoothPairingEvent.h"
#include "mozilla/dom/BluetoothPairingListenerBinding.h"
#include "BluetoothService.h"

USING_BLUETOOTH_NAMESPACE

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(BluetoothPairingListener)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(BluetoothPairingListener, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(BluetoothPairingListener, DOMEventTargetHelper)

BluetoothPairingListener::BluetoothPairingListener(nsPIDOMWindow* aWindow)
  : DOMEventTargetHelper(aWindow)
{
  MOZ_ASSERT(aWindow);

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);
  bs->RegisterBluetoothSignalHandler(NS_LITERAL_STRING(KEY_PAIRING_LISTENER),
                                     this);
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
  BluetoothService* bs = BluetoothService::Get();
  // It can be nullptr on shutdown.
  NS_ENSURE_TRUE_VOID(bs);
  bs->UnregisterBluetoothSignalHandler(NS_LITERAL_STRING(KEY_PAIRING_LISTENER),
                                       this);
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

void
BluetoothPairingListener::Notify(const BluetoothSignal& aData)
{
  InfallibleTArray<BluetoothNamedValue> arr;

  BluetoothValue value = aData.value();
  if (aData.name().EqualsLiteral("PairingRequest")) {

    MOZ_ASSERT(value.type() == BluetoothValue::TArrayOfBluetoothNamedValue);

    const InfallibleTArray<BluetoothNamedValue>& arr =
      value.get_ArrayOfBluetoothNamedValue();

    MOZ_ASSERT(arr.Length() == 4 &&
               arr[0].value().type() == BluetoothValue::TnsString && // address
               arr[1].value().type() == BluetoothValue::TnsString && // name
               arr[2].value().type() == BluetoothValue::TnsString && // passkey
               arr[3].value().type() == BluetoothValue::TnsString);  // type

    nsString deviceAddress = arr[0].value().get_nsString();
    nsString deviceName = arr[1].value().get_nsString();
    nsString passkey = arr[2].value().get_nsString();
    nsString type = arr[3].value().get_nsString();

    // Create a temporary device with deviceAddress and deviceName
    InfallibleTArray<BluetoothNamedValue> props;
    BT_APPEND_NAMED_VALUE(props, "Address", deviceAddress);
    BT_APPEND_NAMED_VALUE(props, "Name", deviceName);
    nsRefPtr<BluetoothDevice> device =
      BluetoothDevice::Create(GetOwner(), props);

    // Notify pairing listener of pairing requests
    DispatchPairingEvent(device, passkey, type);
  } else {
    BT_WARNING("Not handling pairing listener signal: %s",
               NS_ConvertUTF16toUTF8(aData.name()).get());
  }
}

JSObject*
BluetoothPairingListener::WrapObject(JSContext* aCx)
{
  return BluetoothPairingListenerBinding::Wrap(aCx, this);
}

void
BluetoothPairingListener::DisconnectFromOwner()
{
  DOMEventTargetHelper::DisconnectFromOwner();

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);
  bs->UnregisterBluetoothSignalHandler(NS_LITERAL_STRING(KEY_PAIRING_LISTENER),
                                       this);
}
