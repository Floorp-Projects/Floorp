/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothpairinglistener_h
#define mozilla_dom_bluetooth_bluetoothpairinglistener_h

#include "BluetoothCommon.h"
#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothDevice;

class BluetoothPairingListener MOZ_FINAL : public DOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  static already_AddRefed<BluetoothPairingListener>
    Create(nsPIDOMWindow* aWindow);

  void DispatchPairingEvent(BluetoothDevice* aDevice,
                            const nsAString& aPasskey,
                            const nsAString& aType);

  nsPIDOMWindow* GetParentObject() const
  {
    return GetOwner();
  }

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  IMPL_EVENT_HANDLER(displaypasskeyreq);
  IMPL_EVENT_HANDLER(enterpincodereq);
  IMPL_EVENT_HANDLER(pairingconfirmationreq);
  IMPL_EVENT_HANDLER(pairingconsentreq);

private:
  BluetoothPairingListener(nsPIDOMWindow* aWindow);
  ~BluetoothPairingListener();
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_bluetoothpairinglistener_h
