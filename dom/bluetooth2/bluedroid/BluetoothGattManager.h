/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothgattmanager_h__
#define mozilla_dom_bluetooth_bluetoothgattmanager_h__

#include "BluetoothCommon.h"
#include "BluetoothInterface.h"
#include "BluetoothProfileManagerBase.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothReplyRunnable;

class BluetoothGattManager MOZ_FINAL : public nsIObserver
                                     , public BluetoothGattNotificationHandler
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static BluetoothGattManager* Get();
  static void InitGattInterface(BluetoothProfileResultHandler* aRes);
  static void DeinitGattInterface(BluetoothProfileResultHandler* aRes);
  virtual ~BluetoothGattManager();

  void Connect(const nsAString& aAppUuid,
               const nsAString& aDeviceAddr,
               BluetoothReplyRunnable* aRunnable);

  void Disconnect(const nsAString& aAppUuid,
                  const nsAString& aDeviceAddr,
                  BluetoothReplyRunnable* aRunnable);

  void UnregisterClient(int aClientIf,
                        BluetoothReplyRunnable* aRunnable);

private:
  class CleanupResultHandler;
  class CleanupResultHandlerRunnable;
  class InitGattResultHandler;
  class RegisterClientResultHandler;
  class UnregisterClientResultHandler;
  class ConnectResultHandler;
  class DisconnectResultHandler;

  BluetoothGattManager();

  void HandleShutdown();

  void RegisterClientNotification(int aStatus,
                                  int aClientIf,
                                  const BluetoothUuid& aAppUuid) MOZ_OVERRIDE;

  void ScanResultNotification(
    const nsAString& aBdAddr, int aRssi,
    const BluetoothGattAdvData& aAdvData) MOZ_OVERRIDE;

  void ConnectNotification(int aConnId,
                           int aStatus,
                           int aClientIf,
                           const nsAString& aBdAddr) MOZ_OVERRIDE;

  void DisconnectNotification(int aConnId,
                              int aStatus,
                              int aClientIf,
                              const nsAString& aBdAddr) MOZ_OVERRIDE;

  void SearchCompleteNotification(int aConnId, int aStatus) MOZ_OVERRIDE;

  void SearchResultNotification(int aConnId,
                                const BluetoothGattServiceId& aServiceId)
                                MOZ_OVERRIDE;

  void GetCharacteristicNotification(
    int aConnId, int aStatus,
    const BluetoothGattServiceId& aServiceId,
    const BluetoothGattId& aCharId,
    int aCharProperty) MOZ_OVERRIDE;

  void GetDescriptorNotification(
    int aConnId, int aStatus,
    const BluetoothGattServiceId& aServiceId,
    const BluetoothGattId& aCharId,
    const BluetoothGattId& aDescriptorId) MOZ_OVERRIDE;

  void GetIncludedServiceNotification(
    int aConnId, int aStatus,
    const BluetoothGattServiceId& aServiceId,
    const BluetoothGattServiceId& aIncludedServId) MOZ_OVERRIDE;

  void RegisterNotificationNotification(
    int aConnId, int aIsRegister, int aStatus,
    const BluetoothGattServiceId& aServiceId,
    const BluetoothGattId& aCharId) MOZ_OVERRIDE;

  void NotifyNotification(int aConnId,
                          const BluetoothGattNotifyParam& aNotifyParam)
                          MOZ_OVERRIDE;

  void ReadCharacteristicNotification(int aConnId,
                                      int aStatus,
                                      const BluetoothGattReadParam& aReadParam)
                                      MOZ_OVERRIDE;

  void WriteCharacteristicNotification(
    int aConnId, int aStatus,
    const BluetoothGattWriteParam& aWriteParam) MOZ_OVERRIDE;

  void ReadDescriptorNotification(int aConnId,
                                  int aStatus,
                                  const BluetoothGattReadParam& aReadParam)
                                  MOZ_OVERRIDE;

  void WriteDescriptorNotification(int aConnId,
                                   int aStatus,
                                   const BluetoothGattWriteParam& aWriteParam)
                                   MOZ_OVERRIDE;

  void ExecuteWriteNotification(int aConnId, int aStatus) MOZ_OVERRIDE;

  void ReadRemoteRssiNotification(int aClientIf,
                                  const nsAString& aBdAddr,
                                  int aRssi,
                                  int aStatus) MOZ_OVERRIDE;

  void ListenNotification(int aStatus, int aServerIf) MOZ_OVERRIDE;

  static bool mInShutdown;
};

END_BLUETOOTH_NAMESPACE

#endif
