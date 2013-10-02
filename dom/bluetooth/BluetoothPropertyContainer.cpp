/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "BluetoothPropertyContainer.h"
#include "BluetoothService.h"
#include "DOMRequest.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "nsServiceManagerUtils.h"

USING_BLUETOOTH_NAMESPACE

already_AddRefed<mozilla::dom::DOMRequest>
BluetoothPropertyContainer::FirePropertyAlreadySet(nsIDOMWindow* aOwner,
                                                   ErrorResult& aRv)
{
  nsCOMPtr<nsIDOMRequestService> rs =
    do_GetService(DOMREQUEST_SERVICE_CONTRACTID);
  if (!rs) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<mozilla::dom::DOMRequest> request = new DOMRequest(aOwner);
  rs->FireSuccess(request, JS::UndefinedValue());

  return request.forget();
}

already_AddRefed<mozilla::dom::DOMRequest>
BluetoothPropertyContainer::SetProperty(nsIDOMWindow* aOwner,
                                        const BluetoothNamedValue& aProperty,
                                        ErrorResult& aRv)
{
  nsRefPtr<mozilla::dom::DOMRequest> request = new DOMRequest(aOwner);
  nsRefPtr<BluetoothReplyRunnable> task =
    new BluetoothVoidReplyRunnable(request);

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    BT_WARNING("Bluetooth service not available!");
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsresult rv = bs->SetProperty(mObjectType, aProperty, task);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}
