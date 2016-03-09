/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_BluetoothGattReplyRunnable_h
#define mozilla_dom_bluetooth_BluetoothGattReplyRunnable_h

#include "BluetoothReplyRunnable.h"

class nsIDOMDOMRequest;

namespace mozilla {
namespace dom {
class Promise;
}
}

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothReply;

class BluetoothGattReplyRunnable : public BluetoothReplyRunnable
{
public:
  BluetoothGattReplyRunnable(Promise* aPromise);

protected:
  virtual ~BluetoothGattReplyRunnable() {}

private:
  virtual nsresult FireErrorString() override;

  void GattStatusToDOMStatus(const BluetoothGattStatus aGattStatus,
                             nsresult& aDOMStatus);

  virtual bool IsWrite()
  {
    return false;
  }
};

class BluetoothGattVoidReplyRunnable : public BluetoothGattReplyRunnable
{
public:
  BluetoothGattVoidReplyRunnable(Promise* aPromise)
    : BluetoothGattReplyRunnable(aPromise) {}
  ~BluetoothGattVoidReplyRunnable() {}

protected:
  virtual bool
  ParseSuccessfulReply(JS::MutableHandle<JS::Value> aValue) override
  {
    aValue.setUndefined();
    return true;
  }
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_BluetoothGattReplyRunnable_h
