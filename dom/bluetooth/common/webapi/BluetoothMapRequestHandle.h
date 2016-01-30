/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_BluetoothMapRequestHandle_h
#define mozilla_dom_bluetooth_BluetoothMapRequestHandle_h

#include "nsCOMPtr.h"
#include "nsPIDOMWindow.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/File.h"

namespace mozilla {
  class ErrorResult;
  namespace dom {
    class Promise;
  }
}

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothMapRequestHandle final : public nsISupports
                                      , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(BluetoothMapRequestHandle)

  static already_AddRefed<BluetoothMapRequestHandle>
    Create(nsPIDOMWindowInner* aOwner);

  nsPIDOMWindowInner* GetParentObject() const
  {
    return mOwner;
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;
  /**
   * Reply to folder listing
   *
   * @param aMasId         [in]  MAS ID.
   * @param aFolderlists   [in]  MAP folder Name.
   * @param aRv            [out] Error result to set in case of error.
   */
  already_AddRefed<Promise> ReplyToFolderListing(long aMasId,
    const nsAString& aFolderlists, ErrorResult& aRv);

  /**
   * Reply to messages listing
   *
   * @param aMasId         [in]  MAS ID.
   * @param aBlob          [in]  MAP messages listing content.
   * @param aNewMessage    [in]  Whether MSE has received a new message.
   * @param aTimestamp     [in]  The local time basis and UTC offset of the
   *                             MSE. MCE will interpret the timestamps of The
   *                             messages listing entries.
   * @param aSize          [in]  The number of accessible messages in the
   *                             corresponding folder fulfilling.
   * @param aRv            [out] Error result to set in case of error.
   */
  already_AddRefed<Promise> ReplyToMessagesListing(
    long aMasId, Blob& aBlob, bool aNewMessage, const nsAString& aTimestamp,
    int aSize, ErrorResult& aRv);

  /**
   * Reply to get-message request
   *
   * @param aMasId         [in]  MAS ID.
   * @param aBlob          [in]  MAP get messages content.
   * @param aRv            [out] Error result to set in case of error.
   */
  already_AddRefed<Promise> ReplyToGetMessage(long aMasId, Blob& aBlob,
                                              ErrorResult& aRv);

  /**
   * Reply to set-message request
   *
   * @param aMasId         [in]  MAS ID.
   * @param aStatus        [in]  MAP set message result.
   * @param aRv            [out] Error result to set in case of error.
   */
  already_AddRefed<Promise> ReplyToSetMessageStatus(long aMasId, bool aStatus,
                                                    ErrorResult& aRv);

  /**
   * Reply to send-message request
   *
   * @param aMasId         [in]  MAS ID.
   * @param aHandleId      [in]  Handle ID.
   * @param aStatus        [in]  MAP send message result.
   * @param aRv            [out] Error result to set in case of error.
   */
  already_AddRefed<Promise> ReplyToSendMessage(
    long aMasId, const nsAString& aHandleId, bool aStatus, ErrorResult& aRv);

  /**
   * Reply to message update request
   *
   * @param aMasId         [in]  MAS ID.
   * @param aStatus        [in]  Update inbox results.
   * @param aRv            [out] Error result to set in case of error.
   */
  already_AddRefed<Promise> ReplyToMessageUpdate(long aMasId, bool aStatus,
                                                 ErrorResult& aRv);

private:
  BluetoothMapRequestHandle(nsPIDOMWindowInner* aOwner);
  ~BluetoothMapRequestHandle();

  nsCOMPtr<nsPIDOMWindowInner> mOwner;
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_BluetoothMapRequestHandle_h
