/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_BluetoothObexAuthHandle_h
#define mozilla_dom_bluetooth_BluetoothObexAuthHandle_h

#include "BluetoothCommon.h"
#include "nsPIDOMWindow.h"
#include "nsWrapperCache.h"

namespace mozilla {
class ErrorResult;
namespace dom {
class Promise;
}
}

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothObexAuthHandle final : public nsISupports
                                    , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(BluetoothObexAuthHandle)

  static already_AddRefed<BluetoothObexAuthHandle>
    Create(nsPIDOMWindowInner* aOwner);

  nsPIDOMWindowInner* GetParentObject() const
  {
    return mOwner;
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  // Set password to the OBEX authentication request
  already_AddRefed<Promise>
    SetPassword(const nsAString& aPassword, ErrorResult& aRv);

  // Reject the OBEX authentication request
  already_AddRefed<Promise> Reject(ErrorResult& aRv);

private:
  BluetoothObexAuthHandle(nsPIDOMWindowInner* aOwner);
  ~BluetoothObexAuthHandle();

  nsCOMPtr<nsPIDOMWindowInner> mOwner;
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_BluetoothObexAuthHandle_h
