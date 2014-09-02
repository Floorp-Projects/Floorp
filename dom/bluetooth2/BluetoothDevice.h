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

  /****************************************************************************
   * Attribute Getters
   ***************************************************************************/
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

  void GetUuids(nsTArray<nsString>& aUuids) const
  {
    aUuids = mUuids;
  }

  /****************************************************************************
   * Event Handlers
   ***************************************************************************/
  IMPL_EVENT_HANDLER(attributechanged);

  /****************************************************************************
   * Methods (Web API Implementation)
   ***************************************************************************/
  already_AddRefed<Promise> FetchUuids(ErrorResult& aRv);

  /****************************************************************************
   * Others
   ***************************************************************************/
  static already_AddRefed<BluetoothDevice>
    Create(nsPIDOMWindow* aOwner, const BluetoothValue& aValue);

  void Notify(const BluetoothSignal& aParam); // BluetoothSignalObserver
  nsPIDOMWindow* GetParentObject() const
  {
     return GetOwner();
  }

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;
  virtual void DisconnectFromOwner() MOZ_OVERRIDE;

  /**
   * Override operator== for device comparison
   */
  bool operator==(BluetoothDevice& aDevice) const
  {
    nsString address;
    aDevice.GetAddress(address);
    return mAddress.Equals(address);
  }

private:
  BluetoothDevice(nsPIDOMWindow* aOwner, const BluetoothValue& aValue);
  ~BluetoothDevice();

  /**
   * Set device properties according to properties array
   *
   * @param aValue [in] Properties array to set with
   */
  void SetPropertyByValue(const BluetoothNamedValue& aValue);

  /**
   * Handle "PropertyChanged" bluetooth signal.
   *
   * @param aValue [in] Array of changed properties
   */
  void HandlePropertyChanged(const BluetoothValue& aValue);

  /**
   * Fire BluetoothAttributeEvent to trigger onattributechanged event handler.
   */
  void DispatchAttributeEvent(const nsTArray<nsString>& aTypes);

  /**
   * Convert string to BluetoothDeviceAttribute.
   *
   * @param aString [in] String to convert
   */
  BluetoothDeviceAttribute
    ConvertStringToDeviceAttribute(const nsAString& aString);

  /**
   * Check whether value of given device property has changed.
   *
   * @param aType  [in] Device property to check
   * @param aValue [in] New value of the device property
   */
  bool IsDeviceAttributeChanged(BluetoothDeviceAttribute aType,
                                const BluetoothValue& aValue);

  /****************************************************************************
   * Variables
   ***************************************************************************/
  /**
   * BD address of this device.
   */
  nsString mAddress;

  /**
   * Class of device (CoD) that describes this device's capabilities.
   */
  nsRefPtr<BluetoothClassOfDevice> mCod;

  /**
   * Human-readable name of this device.
   */
  nsString mName;

  /**
   * Whether this device is paired or not.
   */
  bool mPaired;

  /**
   * Cached UUID list of services which this device provides.
   */
  nsTArray<nsString> mUuids;
};

END_BLUETOOTH_NAMESPACE

#endif
