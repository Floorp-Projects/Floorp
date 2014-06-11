/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothdevice_h__
#define mozilla_dom_bluetooth_bluetoothdevice_h__

#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "BluetoothCommon.h"
#include "BluetoothPropertyContainer.h"
#include "nsString.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothNamedValue;
class BluetoothValue;
class BluetoothSignal;
class BluetoothSocket;

class BluetoothDevice : public DOMEventTargetHelper
                      , public BluetoothSignalObserver
                      , public BluetoothPropertyContainer
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(BluetoothDevice,
                                                         DOMEventTargetHelper)

  static already_AddRefed<BluetoothDevice>
  Create(nsPIDOMWindow* aOwner, const nsAString& aAdapterPath,
         const BluetoothValue& aValue);

  void Notify(const BluetoothSignal& aParam);

  void GetAddress(nsString& aAddress) const
  {
    aAddress = mAddress;
  }

  void GetName(nsString& aName) const
  {
    aName = mName;
  }

  void GetIcon(nsString& aIcon) const
  {
    aIcon = mIcon;
  }

  uint32_t Class() const
  {
    return mClass;
  }

  bool Paired() const
  {
    return mPaired;
  }

  bool Connected() const
  {
    return mConnected;
  }

  void GetUuids(JSContext* aContext, JS::MutableHandle<JS::Value> aUuids,
                ErrorResult& aRv);
  void GetServices(JSContext* aContext, JS::MutableHandle<JS::Value> aServices,
                   ErrorResult& aRv);

  nsISupports*
  ToISupports()
  {
    return static_cast<EventTarget*>(this);
  }

  void SetPropertyByValue(const BluetoothNamedValue& aValue) MOZ_OVERRIDE;

  void Unroot();

  nsPIDOMWindow* GetParentObject() const
  {
     return GetOwner();
  }

  virtual JSObject*
    WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  virtual void DisconnectFromOwner() MOZ_OVERRIDE;

private:
  BluetoothDevice(nsPIDOMWindow* aOwner, const nsAString& aAdapterPath,
                  const BluetoothValue& aValue);
  ~BluetoothDevice();
  void Root();

  JS::Heap<JSObject*> mJsUuids;
  JS::Heap<JSObject*> mJsServices;

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
