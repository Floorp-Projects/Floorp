/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothreplyrunnable_h__
#define mozilla_dom_bluetooth_bluetoothreplyrunnable_h__

#include "mozilla/Attributes.h"
#include "BluetoothCommon.h"
#include "nsThreadUtils.h"
#include "js/Value.h"

class nsIDOMDOMRequest;

namespace mozilla {
namespace dom {
class Promise;
}
}

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothReply;

class BluetoothReplyRunnable : public nsRunnable
{
public:
  NS_DECL_NSIRUNNABLE

  BluetoothReplyRunnable(nsIDOMDOMRequest* aReq,
                         Promise* aPromise = nullptr,
                         const nsAString& aName = EmptyString());

  void SetReply(BluetoothReply* aReply);

  void SetError(const nsAString& aError)
  {
    mErrorString = aError;
  }

  virtual void ReleaseMembers();

protected:
  virtual ~BluetoothReplyRunnable();

  virtual bool ParseSuccessfulReply(JS::MutableHandle<JS::Value> aValue) = 0;

  // This is an autoptr so we don't have to bring the ipdl include into the
  // header. We assume we'll only be running this once and it should die on
  // scope out of Run() anyways.
  nsAutoPtr<BluetoothReply> mReply;

private:
  nsresult FireReplySuccess(JS::Handle<JS::Value> aVal);
  nsresult FireErrorString();

  /**
   * Either mDOMRequest or mPromise is not nullptr to reply applications
   * success or error string. One special case is internal IPC that require
   * neither mDOMRequest nor mPromise to reply applications.
   * E.g., GetAdaptersTask triggered by BluetoothManager
   *
   * TODO: remove mDOMRequest once all methods adopt Promise.
   */
  nsCOMPtr<nsIDOMDOMRequest> mDOMRequest;
  nsRefPtr<Promise> mPromise;

  nsString mErrorString;
  nsString mName; // for debugging
};

class BluetoothVoidReplyRunnable : public BluetoothReplyRunnable
{
public:
  BluetoothVoidReplyRunnable(nsIDOMDOMRequest* aReq,
                             Promise* aPromise = nullptr,
                             const nsAString& aName = EmptyString());
 ~BluetoothVoidReplyRunnable();

protected:
  virtual bool
  ParseSuccessfulReply(JS::MutableHandle<JS::Value> aValue) MOZ_OVERRIDE
  {
    aValue.setUndefined();
    return true;
  }
};

END_BLUETOOTH_NAMESPACE

#endif
