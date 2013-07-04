/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothdbuseventservice_h__
#define mozilla_dom_bluetooth_bluetoothdbuseventservice_h__

#include "mozilla/Attributes.h"
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

  virtual nsresult StartInternal() MOZ_OVERRIDE;

  virtual nsresult StopInternal() MOZ_OVERRIDE;

  virtual bool IsEnabledInternal() MOZ_OVERRIDE;

  virtual nsresult GetDefaultAdapterPathInternal(
                                             BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual nsresult GetConnectedDevicePropertiesInternal(uint16_t aProfileId,
                                             BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual nsresult GetPairedDevicePropertiesInternal(
                                     const nsTArray<nsString>& aDeviceAddresses,
                                     BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual nsresult StartDiscoveryInternal(BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual nsresult StopDiscoveryInternal(BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual nsresult
  SetProperty(BluetoothObjectType aType,
              const BluetoothNamedValue& aValue,
              BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual bool
  GetDevicePath(const nsAString& aAdapterPath,
                const nsAString& aDeviceAddress,
                nsAString& aDevicePath) MOZ_OVERRIDE;

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
               mozilla::ipc::UnixSocketConsumer* aConsumer) MOZ_OVERRIDE;

  virtual nsresult
  GetServiceChannel(const nsAString& aDeviceAddress,
                    const nsAString& aServiceUuid,
                    BluetoothProfileManagerBase* aManager) MOZ_OVERRIDE;

  virtual bool
  UpdateSdpRecords(const nsAString& aDeviceAddress,
                   BluetoothProfileManagerBase* aManager) MOZ_OVERRIDE;

  virtual nsresult
  CreatePairedDeviceInternal(const nsAString& aDeviceAddress,
                             int aTimeout,
                             BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual nsresult
  RemoveDeviceInternal(const nsAString& aDeviceObjectPath,
                       BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual bool
  SetPinCodeInternal(const nsAString& aDeviceAddress, const nsAString& aPinCode,
                     BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual bool
  SetPasskeyInternal(const nsAString& aDeviceAddress, uint32_t aPasskey,
                     BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual bool
  SetPairingConfirmationInternal(const nsAString& aDeviceAddress, bool aConfirm,
                                 BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual bool
  SetAuthorizationInternal(const nsAString& aDeviceAddress, bool aAllow,
                           BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual void
  Connect(const nsAString& aDeviceAddress,
          const uint16_t aProfileId,
          BluetoothReplyRunnable* aRunnable);

  virtual bool
  IsConnected(uint16_t aProfileId) MOZ_OVERRIDE;

  virtual void
  Disconnect(const uint16_t aProfileId, BluetoothReplyRunnable* aRunnable);

  virtual void
  SendFile(const nsAString& aDeviceAddress,
           BlobParent* aBlobParent,
           BlobChild* aBlobChild,
           BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual void
  StopSendingFile(const nsAString& aDeviceAddress,
                  BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual void
  ConfirmReceivingFile(const nsAString& aDeviceAddress, bool aConfirm,
                       BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual void
  ConnectSco(BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual void
  DisconnectSco(BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual void
  IsScoConnected(BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual nsresult
  SendSinkMessage(const nsAString& aDeviceAddresses,
                  const nsAString& aMessage) MOZ_OVERRIDE;

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
