/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluedroid_bluetoothdaemonsetupinterface_h__
#define mozilla_dom_bluetooth_bluedroid_bluetoothdaemonsetupinterface_h__

#include "BluetoothCommon.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothSetupResultHandler
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(BluetoothSetupResultHandler)

  virtual void OnError(BluetoothStatus aStatus);
  virtual void RegisterModule();
  virtual void UnregisterModule();
  virtual void Configuration();

protected:
  virtual ~BluetoothSetupResultHandler();
};

END_BLUETOOTH_NAMESPACE

#endif
