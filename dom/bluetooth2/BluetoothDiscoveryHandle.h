/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothdiscoveryhandle_h
#define mozilla_dom_bluetooth_bluetoothdiscoveryhandle_h

#include "BluetoothCommon.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/Observer.h"
#include "nsISupportsImpl.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothValue;

class BluetoothDiscoveryHandle MOZ_FINAL : public DOMEventTargetHelper
                                         , public BluetoothSignalObserver
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  static already_AddRefed<BluetoothDiscoveryHandle>
    Create(nsPIDOMWindow* aWindow);

  IMPL_EVENT_HANDLER(devicefound);

  void Notify(const BluetoothSignal& aData); // BluetoothSignalObserver

  virtual void DisconnectFromOwner() MOZ_OVERRIDE;
  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

private:
  BluetoothDiscoveryHandle(nsPIDOMWindow* aWindow);
  ~BluetoothDiscoveryHandle();

  /**
   * Start/Stop listening to bluetooth signal.
   *
   * @param aStart [in] Whether to start or stop listening to bluetooth signal
   */
  void ListenToBluetoothSignal(bool aStart);

  /**
   * Fire BluetoothDeviceEvent to trigger ondevicefound event handler.
   *
   * @param aValue [in] Properties array of the found device
   */
  void DispatchDeviceEvent(const BluetoothValue& aValue);
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_bluetoothdiscoveryhandle_h
