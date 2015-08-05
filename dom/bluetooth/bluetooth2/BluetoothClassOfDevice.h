/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_BluetoothClassOfDevice_h
#define mozilla_dom_bluetooth_BluetoothClassOfDevice_h

#include "BluetoothCommon.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "nsCycleCollectionParticipant.h"
#include "nsPIDOMWindow.h"
#include "nsWrapperCache.h"

struct JSContext;

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothClassOfDevice final : public nsISupports
                                   , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(BluetoothClassOfDevice)

  static already_AddRefed<BluetoothClassOfDevice>
    Create(nsPIDOMWindow* aOwner);

  uint16_t MajorServiceClass() const
  {
    return mMajorServiceClass;
  }

  uint8_t MajorDeviceClass() const
  {
    return mMajorDeviceClass;
  }

  uint8_t MinorDeviceClass() const
  {
    return mMinorDeviceClass;
  }

  /**
   * Compare whether CoD equals to CoD value.
   *
   * @param aValue [in] CoD value to compare
   */
  bool Equals(const uint32_t aValue);

  /**
   * Convert CoD to uint32_t CoD value.
   *
   * TODO: Remove this function once we replace uint32_t cod value with
   *       BluetoothClassOfDevice in BluetoothProfileController.
   */
  uint32_t ToUint32();

  /**
   * Update CoD.
   *
   * @param aValue [in] CoD value to update
   */
  void Update(const uint32_t aValue);

  nsPIDOMWindow* GetParentObject() const
  {
    return mOwnerWindow;
  }
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

private:
  BluetoothClassOfDevice(nsPIDOMWindow* aOwner);
  ~BluetoothClassOfDevice();

  /**
   * Reset CoD to default value.
   */
  void Reset();

  uint16_t mMajorServiceClass;
  uint8_t mMajorDeviceClass;
  uint8_t mMinorDeviceClass;

  nsCOMPtr<nsPIDOMWindow> mOwnerWindow;
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_BluetoothClassOfDevice_h
