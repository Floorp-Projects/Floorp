/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothdevice_h__
#define mozilla_dom_bluetooth_bluetoothdevice_h__

#include "mozilla/Attributes.h"
#include "BluetoothCommon.h"
#include "BluetoothPropertyContainer.h"
#include "nsDOMEventTargetHelper.h"
#include "nsIDOMBluetoothDevice.h"
#include "nsString.h"

class nsIDOMDOMRequest;

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothNamedValue;
class BluetoothValue;
class BluetoothSignal;
class BluetoothSocket;

class BluetoothDevice : public nsDOMEventTargetHelper
                      , public nsIDOMBluetoothDevice
                      , public BluetoothSignalObserver
                      , public BluetoothPropertyContainer
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMBLUETOOTHDEVICE

  NS_REALLY_FORWARD_NSIDOMEVENTTARGET(nsDOMEventTargetHelper)

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(BluetoothDevice,
                                                         nsDOMEventTargetHelper)

  static already_AddRefed<BluetoothDevice>
  Create(nsPIDOMWindow* aOwner, const nsAString& aAdapterPath,
         const BluetoothValue& aValue);

  void Notify(const BluetoothSignal& aParam);

  nsISupports*
  ToISupports()
  {
    return static_cast<EventTarget*>(this);
  }

  void SetPropertyByValue(const BluetoothNamedValue& aValue) MOZ_OVERRIDE;

  void Unroot();
private:
  BluetoothDevice(nsPIDOMWindow* aOwner, const nsAString& aAdapterPath,
                  const BluetoothValue& aValue);
  ~BluetoothDevice();
  void Root();

  JSObject* mJsUuids;
  JSObject* mJsServices;

  nsString mAdapterPath;
  nsString mAddress;
  nsString mName;
  nsString mIcon;
  uint32_t mClass;
  bool mConnected;
  bool mPaired;
  bool mIsRooted;
  nsTArray<nsString> mUuids;
  nsTArray<nsString> mServices;

};

END_BLUETOOTH_NAMESPACE

#endif
