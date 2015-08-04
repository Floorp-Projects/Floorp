/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_BluetoothPairingListener_h
#define mozilla_dom_bluetooth_BluetoothPairingListener_h

#include "BluetoothCommon.h"
#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothSignal;

class BluetoothPairingListener final : public DOMEventTargetHelper
                                     , public BluetoothSignalObserver
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  static already_AddRefed<BluetoothPairingListener>
    Create(nsPIDOMWindow* aWindow);

  void DispatchPairingEvent(const nsAString& aName,
                            const nsAString& aAddress,
                            const nsAString& aPasskey,
                            const nsAString& aType);

  void Notify(const BluetoothSignal& aParam); // BluetoothSignalObserver

  nsPIDOMWindow* GetParentObject() const
  {
    return GetOwner();
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;
  virtual void DisconnectFromOwner() override;
  virtual void EventListenerAdded(nsIAtom* aType) override;

  IMPL_EVENT_HANDLER(displaypasskeyreq);
  IMPL_EVENT_HANDLER(enterpincodereq);
  IMPL_EVENT_HANDLER(pairingconfirmationreq);
  IMPL_EVENT_HANDLER(pairingconsentreq);

private:
  BluetoothPairingListener(nsPIDOMWindow* aWindow);
  ~BluetoothPairingListener();

  /**
   * Listen to bluetooth signal if all pairing event handlers are ready.
   *
   * Listen to bluetooth signal only if all pairing event handlers have been
   * attached. All pending pairing requests queued in BluetoothService would be
   * fired when pairing listener starts listening to bluetooth signal.
   */
  void TryListeningToBluetoothSignal();

  /**
   * Indicate whether or not this pairing listener has started listening to
   * Bluetooth signal.
   */
  bool mHasListenedToSignal;
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_BluetoothPairingListener_h
