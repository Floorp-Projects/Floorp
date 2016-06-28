/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothpbaprequesthandle_h
#define mozilla_dom_bluetooth_bluetoothpbaprequesthandle_h

#include "nsCOMPtr.h"
#include "mozilla/dom/bluetooth/BluetoothCommon.h"
#include "mozilla/dom/DOMRequest.h"

namespace mozilla {
  class ErrorResult;
  namespace dom {
    class Blob;
    class DOMRequest;
  }
}

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothPbapRequestHandle final : public nsISupports
                                       , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(BluetoothPbapRequestHandle)

  static already_AddRefed<BluetoothPbapRequestHandle>
    Create(nsPIDOMWindowInner* aOwner);

  nsPIDOMWindowInner* GetParentObject() const
  {
    return mOwner;
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  already_AddRefed<DOMRequest> ReplyTovCardPulling(Blob& aBlob,
                                                   ErrorResult& aRv);

  already_AddRefed<DOMRequest> ReplyToPhonebookPulling(Blob& aBlob,
                                                       uint16_t phonebookSize,
                                                       ErrorResult& aRv);

  already_AddRefed<DOMRequest> ReplyTovCardListing(Blob& aBlob,
                                                   uint16_t phonebookSize,
                                                   ErrorResult& aRv);

private:
  BluetoothPbapRequestHandle(nsPIDOMWindowInner* aOwner);
  ~BluetoothPbapRequestHandle();

  nsCOMPtr<nsPIDOMWindowInner> mOwner;
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_bluetoothpbaprequesthandle_h
