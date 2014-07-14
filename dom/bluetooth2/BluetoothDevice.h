/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothdevice_h__
#define mozilla_dom_bluetooth_bluetoothdevice_h__

#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/BluetoothDevice2Binding.h"
#include "BluetoothCommon.h"
#include "nsString.h"
#include "nsCOMPtr.h"

namespace mozilla {
namespace dom {
  class Promise;
}
}

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothClassOfDevice;
class BluetoothNamedValue;
class BluetoothValue;
class BluetoothSignal;
class BluetoothSocket;

class BluetoothDevice MOZ_FINAL : public DOMEventTargetHelper
                                , public BluetoothSignalObserver
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(BluetoothDevice,
                                           DOMEventTargetHelper)

  static already_AddRefed<BluetoothDevice>
  Create(nsPIDOMWindow* aOwner, const BluetoothValue& aValue);

  void Notify(const BluetoothSignal& aParam);

  void GetAddress(nsString& aAddress) const
  {
    aAddress = mAddress;
  }

  BluetoothClassOfDevice* Cod() const
  {
    return mCod;
  }

  void GetName(nsString& aName) const
  {
    aName = mName;
  }

  bool Paired() const
  {
    return mPaired;
  }

  void GetUuids(nsTArray<nsString>& aUuids) {
    aUuids = mUuids;
  }

  already_AddRefed<Promise> FetchUuids(ErrorResult& aRv);

  void SetPropertyByValue(const BluetoothNamedValue& aValue);

  BluetoothDeviceAttribute
  ConvertStringToDeviceAttribute(const nsAString& aString);

  bool
  IsDeviceAttributeChanged(BluetoothDeviceAttribute aType,
                           const BluetoothValue& aValue);

  void HandlePropertyChanged(const BluetoothValue& aValue);

  void DispatchAttributeEvent(const nsTArray<nsString>& aTypes);

  IMPL_EVENT_HANDLER(attributechanged);

  nsPIDOMWindow* GetParentObject() const
  {
     return GetOwner();
  }

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;
  virtual void DisconnectFromOwner() MOZ_OVERRIDE;

private:
  BluetoothDevice(nsPIDOMWindow* aOwner, const BluetoothValue& aValue);
  ~BluetoothDevice();

  nsString mAddress;
  nsRefPtr<BluetoothClassOfDevice> mCod;
  nsString mName;
  bool mPaired;
  nsTArray<nsString> mUuids;

};

END_BLUETOOTH_NAMESPACE

#endif
