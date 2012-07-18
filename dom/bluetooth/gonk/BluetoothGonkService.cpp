/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothGonkService.h"
#include "BluetoothDBusService.h"

#include "nsDebug.h"
#include "nsError.h"
#include <dlfcn.h>

USING_BLUETOOTH_NAMESPACE

static struct BluedroidFunctions
{
  bool initialized;
  bool tried_initialization;

  BluedroidFunctions() :
    initialized(false),
    tried_initialization(false)
  {
  }

  int (* bt_enable)();
  int (* bt_disable)();
  int (* bt_is_enabled)();
} sBluedroidFunctions;

bool
EnsureBluetoothInit()
{
  if (sBluedroidFunctions.tried_initialization)
  {
    return sBluedroidFunctions.initialized;
  }

  sBluedroidFunctions.initialized = false;
  sBluedroidFunctions.tried_initialization = true;
  
  void* handle = dlopen("libbluedroid.so", RTLD_LAZY);

  if (!handle) {
    NS_ERROR("Failed to open libbluedroid.so, bluetooth cannot run");
    return false;
  }

  sBluedroidFunctions.bt_enable = (int (*)())dlsym(handle, "bt_enable");
  if (!sBluedroidFunctions.bt_enable) {
    NS_ERROR("Failed to attach bt_enable function");
    return false;
  }
  sBluedroidFunctions.bt_disable = (int (*)())dlsym(handle, "bt_disable");
  if (!sBluedroidFunctions.bt_disable) {
    NS_ERROR("Failed to attach bt_disable function");
    return false;
  }
  sBluedroidFunctions.bt_is_enabled = (int (*)())dlsym(handle, "bt_is_enabled");
  if (!sBluedroidFunctions.bt_is_enabled) {
    NS_ERROR("Failed to attach bt_is_enabled function");
    return false;
  }
  sBluedroidFunctions.initialized = true;
  return true;
}

int
IsBluetoothEnabled()
{
  return sBluedroidFunctions.bt_is_enabled();
}

int
EnableBluetooth()
{
  return sBluedroidFunctions.bt_enable();
}

int
DisableBluetooth()
{
  return sBluedroidFunctions.bt_disable();
}

nsresult
StartStopGonkBluetooth(bool aShouldEnable)
{
  bool result;
  
  // Platform specific check for gonk until object is divided in
  // different implementations per platform. Linux doesn't require
  // bluetooth firmware loading, but code should work otherwise.
  if (!EnsureBluetoothInit()) {
    NS_ERROR("Failed to load bluedroid library.\n");
    return NS_ERROR_FAILURE;
  }

  // return 1 if it's enabled, 0 if it's disabled, and -1 on error
  int isEnabled = IsBluetoothEnabled();

  if ((isEnabled == 1 && aShouldEnable) || (isEnabled == 0 && !aShouldEnable)) {
    result = true;
  } else if (isEnabled < 0) {
    result = false;
  } else if (aShouldEnable) {
    result = (EnableBluetooth() == 0) ? true : false;
  } else {
    result = (DisableBluetooth() == 0) ? true : false;
  }
  if (!result) {
    NS_WARNING("Could not set gonk bluetooth firmware!");
    return NS_ERROR_FAILURE;
  }
  
  return NS_OK;
}

nsresult
BluetoothGonkService::StartInternal()
{
  NS_ASSERTION(!NS_IsMainThread(), "This should not run on the main thread!");

  nsresult ret;

  ret = StartStopGonkBluetooth(true);

  if (NS_FAILED(ret)) {
    return ret;    
  }

  return BluetoothDBusService::StartInternal();
}

nsresult
BluetoothGonkService::StopInternal()
{
  NS_ASSERTION(!NS_IsMainThread(), "This should not run on the main thread!");
  nsresult ret;

  ret = StartStopGonkBluetooth(false);

  if (NS_FAILED(ret)) {
    return ret;    
  }

  return BluetoothDBusService::StopInternal();
}

