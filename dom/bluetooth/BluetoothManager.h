/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothmanager_h__
#define mozilla_dom_bluetooth_bluetoothmanager_h__

#include "BluetoothCommon.h"
#include "nsDOMEventTargetHelper.h"
#include "nsIDOMBluetoothManager.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothAdapter;

class BluetoothManager : public nsDOMEventTargetHelper
                       , public nsIDOMBluetoothManager                         
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMBLUETOOTHMANAGER

  NS_FORWARD_NSIDOMEVENTTARGET(nsDOMEventTargetHelper::)

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(BluetoothManager,
                                           nsDOMEventTargetHelper)

  BluetoothManager(nsPIDOMWindow*);
  inline void SetEnabledInternal(bool aEnabled) {mEnabled = aEnabled;}
  
private:
  bool mEnabled;

  NS_DECL_EVENT_HANDLER(enabled)

  nsCOMPtr<nsIEventTarget> mToggleBtThread;
};

END_BLUETOOTH_NAMESPACE

nsresult NS_NewBluetoothManager(nsPIDOMWindow* aWindow, nsIDOMBluetoothManager** aBluetoothManager);

#endif
