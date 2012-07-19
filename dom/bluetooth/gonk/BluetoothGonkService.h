/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothgonkservice_h__
#define mozilla_dom_bluetooth_bluetoothgonkservice_h__

#include "BluetoothCommon.h"
#include "BluetoothDBusService.h"

BEGIN_BLUETOOTH_NAMESPACE

/**
 * BluetoothService functions are used to dispatch messages to Bluetooth DOM
 * objects on the main thread, as well as provide platform indenpendent access
 * to BT functionality. Tasks for polling for outside messages will usually
 * happen on the IO Thread (see ipc/dbus for instance), and these messages will
 * be encased in runnables that will then be distributed via observers managed
 * here.
 */

class BluetoothGonkService : public BluetoothDBusService
{
public:
  /** 
   * Set up variables and start the platform specific connection. Must
   * be called from main thread.
   *
   * @return NS_OK if connection starts successfully, NS_ERROR_FAILURE
   * otherwise
   */
  virtual nsresult StartInternal();

  /** 
   * Stop the platform specific connection. Must be called from main
   * thread.
   *
   * @return NS_OK if connection starts successfully, NS_ERROR_FAILURE
   * otherwise
   */
  virtual nsresult StopInternal();
};

END_BLUETOOTH_NAMESPACE

#endif
