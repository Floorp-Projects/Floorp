/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "BluetoothTypes.h"
#include "BluetoothReplyRunnable.h"

USING_BLUETOOTH_NAMESPACE

nsresult
BluetoothReplyRunnable::FireReply(const jsval& aVal)
{
  nsCOMPtr<nsIDOMRequestService> rs =
    do_GetService("@mozilla.org/dom/dom-request-service;1");
  
  if (!rs) {
    NS_WARNING("No DOMRequest Service!");
    return NS_ERROR_FAILURE;
  }
  
  
  return mReply->type() == BluetoothReply::TBluetoothReplySuccess ?
    rs->FireSuccess(mDOMRequest, aVal) :
    rs->FireError(mDOMRequest, mReply->get_BluetoothReplyError().error());
}

nsresult
BluetoothReplyRunnable::FireErrorString()
{
  nsCOMPtr<nsIDOMRequestService> rs =
    do_GetService("@mozilla.org/dom/dom-request-service;1");
  
  if (!rs) {
    NS_WARNING("No DOMRequest Service!");
    return NS_ERROR_FAILURE;
  }
  
  return rs->FireError(mDOMRequest, mErrorString);
}

NS_IMETHODIMP
BluetoothReplyRunnable::Run()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv;

  MOZ_ASSERT(mDOMRequest);

  if (mReply->type() != BluetoothReply::TBluetoothReplySuccess) {
    rv = FireReply(JSVAL_VOID);
  } else {
    jsval v; 
    if (!ParseSuccessfulReply(&v)) {
      rv = FireErrorString();
    } else {
      rv = FireReply(v);
    }
  }

  if (NS_FAILED(rv)) {
    NS_WARNING("Could not fire DOMRequest!");
  }

  ReleaseMembers();
  if (mDOMRequest) {
    NS_WARNING("mDOMRequest still alive! Deriving class should call BluetoothReplyRunnable::ReleaseMembers()!");
  }

  return rv;
}
