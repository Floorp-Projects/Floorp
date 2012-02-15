/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothAdapter.h"
#include "nsDOMClassInfo.h"

USING_BLUETOOTH_NAMESPACE

BluetoothAdapter::BluetoothAdapter() : mPower(false)
{
}

NS_INTERFACE_MAP_BEGIN(BluetoothAdapter)
  NS_INTERFACE_MAP_ENTRY(nsIDOMBluetoothAdapter)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(BluetoothAdapter)
NS_INTERFACE_MAP_END
  
NS_IMPL_ADDREF(BluetoothAdapter)
NS_IMPL_RELEASE(BluetoothAdapter)

DOMCI_DATA(BluetoothAdapter, BluetoothAdapter)
  
NS_IMETHODIMP
BluetoothAdapter::GetPower(bool* aPower)
{
  *aPower = mPower;
  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::SetPower(bool aPower)
{
  mPower = aPower;
  return NS_OK;
}
