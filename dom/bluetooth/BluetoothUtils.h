/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothutils_h__
#define mozilla_dom_bluetooth_bluetoothutils_h__

#include "BluetoothCommon.h"

BEGIN_BLUETOOTH_NAMESPACE

/**
 * BluetoothUtil functions are used to dispatch messages to Bluetooth DOM
 * objects on the main thread, as well as provide platform indenpendent access
 * to BT functionality. Tasks for polling for outside messages will usually
 * happen on the IO Thread (see ipc/dbus for instance), and these messages will
 * be encased in runnables that will then be distributed via observers managed
 * here.
 */

/** 
 * Add a message handler object from message distribution observer.
 * Must be called from the main thread.
 *
 * @param aNodeName Node name of the object
 * @param aMsgHandler Weak pointer to the object
 *
 * @return NS_OK on successful addition to observer, NS_ERROR_FAILED
 * otherwise
 */
nsresult RegisterBluetoothEventHandler(const nsCString& aNodeName,
                                       BluetoothEventObserver *aMsgHandler);

/** 
 * Remove a message handler object from message distribution observer.
 * Must be called from the main thread.
 *
 * @param aNodeName Node name of the object
 * @param aMsgHandler Weak pointer to the object
 *
 * @return NS_OK on successful removal from observer service,
 * NS_ERROR_FAILED otherwise
 */
nsresult UnregisterBluetoothEventHandler(const nsCString& aNodeName,
                                         BluetoothEventObserver *aMsgHandler);

/** 
 * Returns the path of the default adapter, implemented via a platform
 * specific method.
 *
 * @return Default adapter path/name on success, NULL otherwise
 */
nsresult GetDefaultAdapterPathInternal(nsCString& aAdapterPath);

/** 
 * Set up variables and start the platform specific connection. Must
 * be called from main thread.
 *
 * @return NS_OK if connection starts successfully, NS_ERROR_FAILURE
 * otherwise
 */
nsresult StartBluetoothConnection();

/** 
 * Stop the platform specific connection. Must be called from main
 * thread.
 *
 * @return NS_OK if connection starts successfully, NS_ERROR_FAILURE
 * otherwise
 */
nsresult StopBluetoothConnection();

END_BLUETOOTH_NAMESPACE

#endif
