/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
