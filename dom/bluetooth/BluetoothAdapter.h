/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothadapter_h__
#define mozilla_dom_bluetooth_bluetoothadapter_h__

#include "BluetoothCommon.h"
#include "BluetoothPropertyContainer.h"
#include "nsCOMPtr.h"
#include "nsDOMEventTargetHelper.h"
#include "nsIDOMBluetoothAdapter.h"

class nsIEventTarget;
class nsIDOMDOMRequest;

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothSignal;
class BluetoothNamedValue;

class BluetoothAdapter : public nsDOMEventTargetHelper
                       , public nsIDOMBluetoothAdapter
                       , public BluetoothSignalObserver
                       , public BluetoothPropertyContainer
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMBLUETOOTHADAPTER

  NS_FORWARD_NSIDOMEVENTTARGET(nsDOMEventTargetHelper::)

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(BluetoothAdapter,
                                                         nsDOMEventTargetHelper)

  static already_AddRefed<BluetoothAdapter>
  Create(nsPIDOMWindow* aOwner, const nsAString& name);

  void Notify(const BluetoothSignal& aParam);

  nsIDOMEventTarget*
  ToIDOMEventTarget() const
  {
    return static_cast<nsDOMEventTargetHelper*>(
      const_cast<BluetoothAdapter*>(this));
  }

  nsISupports*
  ToISupports() const
  {
    return ToIDOMEventTarget();
  }

  void Unroot();
  virtual void SetPropertyByValue(const BluetoothNamedValue& aValue);  
private:
  
  BluetoothAdapter(nsPIDOMWindow* aOwner, const nsAString& aPath);
  ~BluetoothAdapter();

  void Root();
  nsresult StartStopDiscovery(bool aStart, nsIDOMDOMRequest** aRequest);
  
  nsString mAddress;
  nsString mName;
  bool mEnabled;
  bool mDiscoverable;
  bool mDiscovering;
  bool mPairable;
  bool mPowered;
  PRUint32 mPairableTimeout;
  PRUint32 mDiscoverableTimeout;
  PRUint32 mClass;
  nsTArray<nsString> mDeviceAddresses;
  nsTArray<nsString> mUuids;
  JSObject* mJsUuids;
  JSObject* mJsDeviceAddresses;
  bool mIsRooted;
  
  NS_DECL_EVENT_HANDLER(propertychanged)
  NS_DECL_EVENT_HANDLER(devicefound)
  NS_DECL_EVENT_HANDLER(devicedisappeared)
};

END_BLUETOOTH_NAMESPACE

#endif
