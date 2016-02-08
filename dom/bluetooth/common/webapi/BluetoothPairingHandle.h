/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_BluetoothPairingHandle_h
#define mozilla_dom_bluetooth_BluetoothPairingHandle_h

#include "BluetoothCommon.h"
#include "nsWrapperCache.h"

namespace mozilla {
class ErrorResult;
namespace dom {
class Promise;
}
}

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothPairingHandle final : public nsISupports
                                   , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(BluetoothPairingHandle)

  static already_AddRefed<BluetoothPairingHandle>
    Create(nsPIDOMWindowInner* aOwner,
           const nsAString& aDeviceAddress,
           const nsAString& aType,
           const nsAString& aPasskey);

  nsPIDOMWindowInner* GetParentObject() const
  {
    return mOwner;
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  void GetPasskey(nsString& aPasskey) const
  {
    aPasskey = mPasskey;
  }

  // Reply to the enterpincodereq pairing request
  already_AddRefed<Promise>
    SetPinCode(const nsAString& aPinCode, ErrorResult& aRv);

  // Accept the pairingconfirmationreq or pairingconsentreq pairing request
  already_AddRefed<Promise> Accept(ErrorResult& aRv);

  // Reject the pairing request
  already_AddRefed<Promise> Reject(ErrorResult& aRv);

private:
  BluetoothPairingHandle(nsPIDOMWindowInner* aOwner,
                         const nsAString& aDeviceAddress,
                         const nsAString& aType,
                         const nsAString& aPasskey);
  ~BluetoothPairingHandle();

  /**
   * Map mType into a BluetoothSspVariant enum value.
   *
   * @param aVariant [out] BluetoothSspVariant value mapped from mType.
   * @return a boolean value to indicate whether mType can map into a
   *         BluetoothSspVariant value.
   */
  bool GetSspVariant(BluetoothSspVariant& aVariant);

  nsCOMPtr<nsPIDOMWindowInner> mOwner;
  nsString mDeviceAddress;
  nsString mType;
  nsString mPasskey;
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_BluetoothPairingHandle_h
