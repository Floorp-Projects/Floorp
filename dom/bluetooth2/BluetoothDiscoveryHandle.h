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
#include "nsISupportsImpl.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothDevice;
class BluetoothValue;

class BluetoothDiscoveryHandle MOZ_FINAL : public DOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  static already_AddRefed<BluetoothDiscoveryHandle>
    Create(nsPIDOMWindow* aWindow);

  void DispatchDeviceEvent(BluetoothDevice* aDevice);

  IMPL_EVENT_HANDLER(devicefound);

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

private:
  BluetoothDiscoveryHandle(nsPIDOMWindow* aWindow);
  ~BluetoothDiscoveryHandle();
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_bluetoothdiscoveryhandle_h
