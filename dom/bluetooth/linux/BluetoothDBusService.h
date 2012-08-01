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
 * BluetoothDBusService is the implementation of BluetoothService for DBus on
 * linux/android/B2G. Function comments are in BluetoothService.h
 */

class BluetoothDBusService : public BluetoothService
                           , private mozilla::ipc::RawDBusConnection
{
public:
  virtual nsresult StartInternal();
  virtual nsresult StopInternal();
  virtual nsresult GetDefaultAdapterPathInternal(BluetoothReplyRunnable* aRunnable);
  virtual nsresult StartDiscoveryInternal(const nsAString& aAdapterPath,
                                          BluetoothReplyRunnable* aRunnable);
  virtual nsresult StopDiscoveryInternal(const nsAString& aAdapterPath,
                                         BluetoothReplyRunnable* aRunnable);
  virtual nsresult
  GetProperties(BluetoothObjectType aType,
                const nsAString& aPath,
                BluetoothReplyRunnable* aRunnable);
  virtual nsresult
  SetProperty(BluetoothObjectType aType,
              const nsAString& aPath,
              const BluetoothNamedValue& aValue,
              BluetoothReplyRunnable* aRunnable);
  virtual bool
  GetDevicePath(const nsAString& aAdapterPath,
                const nsAString& aDeviceAddress,
                nsAString& aDevicePath);

private:
  nsresult SendGetPropertyMessage(const nsAString& aPath,
                                  const char* aInterface,
                                  void (*aCB)(DBusMessage *, void *),
                                  BluetoothReplyRunnable* aRunnable);
  nsresult SendDiscoveryMessage(const nsAString& aAdapterPath,
                                const char* aMessageName,
                                BluetoothReplyRunnable* aRunnable);
  nsresult SendSetPropertyMessage(const nsString& aPath, const char* aInterface,
                                  const BluetoothNamedValue& aValue,
                                  BluetoothReplyRunnable* aRunnable);
};

END_BLUETOOTH_NAMESPACE

#endif
