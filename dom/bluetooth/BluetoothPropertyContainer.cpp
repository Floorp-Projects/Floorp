/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "BluetoothPropertyContainer.h"
#include "BluetoothService.h"
#include "BluetoothTypes.h"
#include "nsIDOMDOMRequest.h"

USING_BLUETOOTH_NAMESPACE

nsresult
BluetoothPropertyContainer::GetProperties()
{
  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    NS_WARNING("Bluetooth service not available!");
    return NS_ERROR_FAILURE;
  }
  nsRefPtr<BluetoothReplyRunnable> task = new GetPropertiesTask(this, NULL);
  return bs->GetProperties(mObjectType, mPath, task);
}

nsresult
BluetoothPropertyContainer::SetProperty(nsIDOMWindow* aOwner,
                                        const BluetoothNamedValue& aProperty,
                                        nsIDOMDOMRequest** aRequest)
{
  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    NS_WARNING("Bluetooth service not available!");
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIDOMRequestService> rs = do_GetService("@mozilla.org/dom/dom-request-service;1");
    
  if (!rs) {
    NS_WARNING("No DOMRequest Service!");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDOMDOMRequest> req;
  nsresult rv = rs->CreateRequest(aOwner, getter_AddRefs(req));
  if (NS_FAILED(rv)) {
    NS_WARNING("Can't create DOMRequest!");
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<BluetoothReplyRunnable> task = new BluetoothVoidReplyRunnable(req);
  
  rv = bs->SetProperty(mObjectType, mPath, aProperty, task);
  NS_ENSURE_SUCCESS(rv, rv);
  
  req.forget(aRequest);
  return NS_OK;
}


bool
BluetoothPropertyContainer::GetPropertiesTask::ParseSuccessfulReply(jsval* aValue)
{
  *aValue = JSVAL_VOID;
  BluetoothValue& v = mReply->get_BluetoothReplySuccess().value();
  if (v.type() != BluetoothValue::TArrayOfBluetoothNamedValue) {
    NS_WARNING("Not a BluetoothNamedValue array!");
    return false;
  }
  const InfallibleTArray<BluetoothNamedValue>& values =
    mReply->get_BluetoothReplySuccess().value().get_ArrayOfBluetoothNamedValue();
  for (uint32_t i = 0; i < values.Length(); ++i) {
    mPropObjPtr->SetPropertyByValue(values[i]);
  }
  return true;
}
