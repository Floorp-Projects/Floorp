/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothrillistener_h__
#define mozilla_dom_bluetooth_bluetoothrillistener_h__

#include "BluetoothCommon.h"

#include "nsCOMPtr.h"

class nsIIccListener;
class nsIMobileConnectionListener;
class nsITelephonyListener;

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothRilListener
{
public:
  BluetoothRilListener();

  bool StartListening();
  bool StopListening();

  void EnumerateCalls();

private:
  bool StartIccListening();
  bool StopIccListening();

  bool StartMobileConnectionListening();
  bool StopMobileConnectionListening();

  bool StartTelephonyListening();
  bool StopTelephonyListening();

  nsCOMPtr<nsIIccListener> mIccListener;
  nsCOMPtr<nsIMobileConnectionListener> mMobileConnectionListener;
  nsCOMPtr<nsITelephonyListener> mTelephonyListener;
};

END_BLUETOOTH_NAMESPACE

#endif
