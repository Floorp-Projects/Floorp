/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothtelephonylistener_h__
#define mozilla_dom_bluetooth_bluetoothtelephonylistener_h__

#include "BluetoothCommon.h"

#include "nsCOMPtr.h"
#include "nsITelephonyProvider.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothTelephonyListener
{
public:
  BluetoothTelephonyListener();

  bool StartListening();
  bool StopListening();

  nsITelephonyListener* GetListener();

private:
  nsCOMPtr<nsITelephonyListener> mTelephonyListener;
};

END_BLUETOOTH_NAMESPACE

#endif
