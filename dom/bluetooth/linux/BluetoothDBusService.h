/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
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
  bool IsReady();

  virtual nsresult StartInternal();

  virtual nsresult StopInternal();

  virtual nsresult GetDefaultAdapterPathInternal(BluetoothReplyRunnable* aRunnable);

  virtual nsresult GetPairedDevicePropertiesInternal(const nsTArray<nsString>& aDeviceAddresses,
                                                     BluetoothReplyRunnable* aRunnable);

  virtual nsresult StartDiscoveryInternal(BluetoothReplyRunnable* aRunnable);

  virtual nsresult StopDiscoveryInternal(BluetoothReplyRunnable* aRunnable);

  virtual nsresult
  GetDevicePropertiesInternal(const BluetoothSignal& aSignal);

  virtual nsresult
  SetProperty(BluetoothObjectType aType,
              const BluetoothNamedValue& aValue,
              BluetoothReplyRunnable* aRunnable);

  virtual bool
  GetDevicePath(const nsAString& aAdapterPath,
                const nsAString& aDeviceAddress,
                nsAString& aDevicePath);

  static bool
  AddServiceRecords(const char* serviceName,
                    unsigned long long uuidMsb,
                    unsigned long long uuidLsb,
                    int channel);

  static bool
  RemoveServiceRecords(const char* serviceName,
                       unsigned long long uuidMsb,
                       unsigned long long uuidLsb,
                       int channel);

  static bool
  AddReservedServicesInternal(const nsTArray<uint32_t>& aServices,
                              nsTArray<uint32_t>& aServiceHandlesContainer);

  static bool
  RemoveReservedServicesInternal(const nsTArray<uint32_t>& aServiceHandles);

  virtual nsresult
  GetScoSocket(const nsAString& aObjectPath,
               bool aAuth,
               bool aEncrypt,
               mozilla::ipc::UnixSocketConsumer* aConsumer);

  virtual nsresult
  GetSocketViaService(const nsAString& aObjectPath,
                      const nsAString& aService,
                      BluetoothSocketType aType,
                      bool aAuth,
                      bool aEncrypt,
                      mozilla::ipc::UnixSocketConsumer* aConsumer,
                      BluetoothReplyRunnable* aRunnable);

  virtual nsresult
  CreatePairedDeviceInternal(const nsAString& aDeviceAddress,
                             int aTimeout,
                             BluetoothReplyRunnable* aRunnable);

  virtual nsresult
  RemoveDeviceInternal(const nsAString& aDeviceObjectPath,
                       BluetoothReplyRunnable* aRunnable);

  virtual bool
  SetPinCodeInternal(const nsAString& aDeviceAddress, const nsAString& aPinCode,
                     BluetoothReplyRunnable* aRunnable);

  virtual bool
  SetPasskeyInternal(const nsAString& aDeviceAddress, uint32_t aPasskey,
                     BluetoothReplyRunnable* aRunnable);

  virtual bool
  SetPairingConfirmationInternal(const nsAString& aDeviceAddress, bool aConfirm,
                                 BluetoothReplyRunnable* aRunnable);

  virtual bool
  SetAuthorizationInternal(const nsAString& aDeviceAddress, bool aAllow,
                           BluetoothReplyRunnable* aRunnable);

  virtual nsresult
  PrepareAdapterInternal();

  virtual void
  Connect(const nsAString& aDeviceAddress,
          const uint16_t aProfileId,
          BluetoothReplyRunnable* aRunnable);

  virtual bool
  IsConnected(uint16_t aProfileId);

  virtual void
  Disconnect(const uint16_t aProfileId, BluetoothReplyRunnable* aRunnable);

  virtual void
  SendFile(const nsAString& aDeviceAddress,
           BlobParent* aBlobParent,
           BlobChild* aBlobChild,
           BluetoothReplyRunnable* aRunnable);

  virtual void
  StopSendingFile(const nsAString& aDeviceAddress,
                  BluetoothReplyRunnable* aRunnable);

  virtual void
  ConfirmReceivingFile(const nsAString& aDeviceAddress, bool aConfirm,
                       BluetoothReplyRunnable* aRunnable);

private:
  nsresult SendGetPropertyMessage(const nsAString& aPath,
                                  const char* aInterface,
                                  void (*aCB)(DBusMessage *, void *),
                                  BluetoothReplyRunnable* aRunnable);
  nsresult SendDiscoveryMessage(const char* aMessageName,
                                BluetoothReplyRunnable* aRunnable);
  nsresult SendSetPropertyMessage(const char* aInterface,
                                  const BluetoothNamedValue& aValue,
                                  BluetoothReplyRunnable* aRunnable);

  void DisconnectAllAcls(const nsAString& aAdapterPath);
};

END_BLUETOOTH_NAMESPACE

#endif
