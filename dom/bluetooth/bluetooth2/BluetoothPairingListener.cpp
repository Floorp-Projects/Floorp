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
  , mHasListenedToSignal(false)
{
  MOZ_ASSERT(aWindow);

  TryListeningToBluetoothSignal();
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
BluetoothPairingListener::DispatchPairingEvent(const nsAString& aName,
                                               const nsAString& aAddress,
                                               const nsAString& aPasskey,
                                               const nsAString& aType)
{
  MOZ_ASSERT(!aName.IsEmpty() && !aAddress.IsEmpty() && !aType.IsEmpty());

  nsRefPtr<BluetoothPairingHandle> handle =
    BluetoothPairingHandle::Create(GetOwner(),
                                   aAddress,
                                   aType,
                                   aPasskey);

  BluetoothPairingEventInit init;
  init.mDeviceName = aName;
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

    nsString address = arr[0].value().get_nsString();
    nsString name = arr[1].value().get_nsString();
    nsString passkey = arr[2].value().get_nsString();
    nsString type = arr[3].value().get_nsString();

    // Notify pairing listener of pairing requests
    DispatchPairingEvent(name, address, passkey, type);
  } else {
    BT_WARNING("Not handling pairing listener signal: %s",
               NS_ConvertUTF16toUTF8(aData.name()).get());
  }
}

JSObject*
BluetoothPairingListener::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto)
{
  return BluetoothPairingListenerBinding::Wrap(aCx, this, aGivenProto);
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

void
BluetoothPairingListener::EventListenerAdded(nsIAtom* aType)
{
  DOMEventTargetHelper::EventListenerAdded(aType);

  TryListeningToBluetoothSignal();
}

void
BluetoothPairingListener::TryListeningToBluetoothSignal()
{
  if (mHasListenedToSignal) {
    // We've handled prior pending pairing requests
    return;
  }

  // Listen to bluetooth signal only if all pairing event handlers have been
  // attached. All pending pairing requests queued in BluetoothService would
  // be fired when pairing listener starts listening to bluetooth signal.
  if (!HasListenersFor(nsGkAtoms::ondisplaypasskeyreq) ||
      !HasListenersFor(nsGkAtoms::onenterpincodereq) ||
      !HasListenersFor(nsGkAtoms::onpairingconfirmationreq) ||
      !HasListenersFor(nsGkAtoms::onpairingconsentreq)) {
    BT_LOGR("Pairing listener is not ready to handle pairing requests!");
    return;
  }

  // Start listening to bluetooth signal to handle pairing requests
  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);
  bs->RegisterBluetoothSignalHandler(NS_LITERAL_STRING(KEY_PAIRING_LISTENER),
                                     this);

  mHasListenedToSignal = true;
}
