/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothdbuseventservice_h__
#define mozilla_dom_bluetooth_bluetoothdbuseventservice_h__

#include "BluetoothCommon.h"
#include "mozilla/ipc/RawDBusConnection.h"
#include "BluetoothService.h"

class DBusMessage;

BEGIN_BLUETOOTH_NAMESPACE

/**
 * BluetoothService functions are used to dispatch messages to Bluetooth DOM
 * objects on the main thread, as well as provide platform independent access
 * to BT functionality. Tasks for polling for outside messages will usually
 * happen on the IO Thread (see ipc/dbus for instance), and these messages will
 * be encased in runnables that will then be distributed via observers managed
 * here.
 */

class BluetoothDBusService : public BluetoothService
                           , private mozilla::ipc::RawDBusConnection
{
public:
  /** 
   * Set up variables and start the platform specific connection. Must
   * be called from outside main thread.
   *
   * @return NS_OK if connection starts successfully, NS_ERROR_FAILURE
   * otherwise
   */
  virtual nsresult StartInternal();

  /** 
   * Stop the platform specific connection. Must be called from outside main
   * thread.
   *
   * @return NS_OK if connection starts successfully, NS_ERROR_FAILURE
   * otherwise
   */
  virtual nsresult StopInternal();

  /** 
   * Returns the path of the default adapter, implemented via a platform
   * specific method.
   *
   * @return Default adapter path/name on success, NULL otherwise
   */
  virtual nsresult GetDefaultAdapterPathInternal(BluetoothReplyRunnable* aRunnable);

  /** 
   * Start device discovery (platform specific implementation)
   *
   * @param aAdapterPath Adapter to start discovery on
   *
   * @return NS_OK if discovery stopped correctly, NS_ERROR_FAILURE otherwise
   */
  virtual nsresult StartDiscoveryInternal(const nsAString& aAdapterPath,
                                          BluetoothReplyRunnable* aRunnable);
  /** 
   * Stop device discovery (platform specific implementation)
   *
   * @param aAdapterPath Adapter to stop discovery on
   *
   * @return NS_OK if discovery stopped correctly, NS_ERROR_FAILURE otherwise
   */
  virtual nsresult StopDiscoveryInternal(const nsAString& aAdapterPath,
                                         BluetoothReplyRunnable* aRunnable);

private:
  nsresult SendDiscoveryMessage(const nsAString& aAdapterPath,
                                const char* aMessageName,
                                BluetoothReplyRunnable* aRunnable);
};

END_BLUETOOTH_NAMESPACE

#endif
