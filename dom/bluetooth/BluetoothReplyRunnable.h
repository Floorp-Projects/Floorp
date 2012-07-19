/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothreplyrunnable_h__
#define mozilla_dom_bluetooth_bluetoothreplyrunnable_h__

#include "BluetoothCommon.h"
#include "nsThreadUtils.h"
#include "jsapi.h"

class nsIDOMDOMRequest;

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothReply;

class BluetoothReplyRunnable : public nsRunnable
{
public:
  NS_DECL_NSIRUNNABLE

  BluetoothReplyRunnable(nsIDOMDOMRequest* aReq) :
    mDOMRequest(aReq)
  {
  }

  void SetReply(BluetoothReply* aReply)
  {
    mReply = aReply;
  }

  void SetError(const nsAString& aError)
  {
    mErrorString = aError;
  }

  virtual void ReleaseMembers()
  {
    mDOMRequest = nsnull;
  }

protected:
  virtual ~BluetoothReplyRunnable()
  {
  }
  
  virtual bool ParseSuccessfulReply(jsval* aValue) = 0;

  // This is an autoptr so we don't have to bring the ipdl include into the
  // header. We assume we'll only be running this once and it should die on
  // scope out of Run() anyways.
  nsAutoPtr<BluetoothReply> mReply;

private:
  nsresult FireReply(const jsval& aVal);
  nsresult FireErrorString();
  
  nsCOMPtr<nsIDOMDOMRequest> mDOMRequest;
  nsString mErrorString;
};

class BluetoothVoidReplyRunnable : public BluetoothReplyRunnable
{
public:
  BluetoothVoidReplyRunnable(nsIDOMDOMRequest* aReq) :
    BluetoothReplyRunnable(aReq)
  {
  }

  virtual void ReleaseMembers()
  {
    BluetoothReplyRunnable::ReleaseMembers();
  }
protected:
  virtual bool ParseSuccessfulReply(jsval* aValue)
  {
    *aValue = JSVAL_VOID;
    return true;
  }
};

END_BLUETOOTH_NAMESPACE

#endif
