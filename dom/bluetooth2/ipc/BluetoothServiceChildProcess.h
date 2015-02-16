/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_ipc_bluetoothservicechildprocess_h__
#define mozilla_dom_bluetooth_ipc_bluetoothservicechildprocess_h__

#include "BluetoothService.h"

namespace mozilla {
namespace ipc {
class UnixSocketConsumer;
}
namespace dom {
namespace bluetooth {

class BluetoothChild;

} // namespace bluetooth
} // namespace dom
} // namespace mozilla


BEGIN_BLUETOOTH_NAMESPACE

class BluetoothServiceChildProcess : public BluetoothService
{
  friend class mozilla::dom::bluetooth::BluetoothChild;

public:
  static BluetoothServiceChildProcess*
  Create();

  virtual void
  RegisterBluetoothSignalHandler(const nsAString& aNodeName,
                                 BluetoothSignalObserver* aMsgHandler)
                                 MOZ_OVERRIDE;

  virtual void
  UnregisterBluetoothSignalHandler(const nsAString& aNodeName,
                                   BluetoothSignalObserver* aMsgHandler)
                                   MOZ_OVERRIDE;

  virtual nsresult
  GetAdaptersInternal(BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual nsresult
  StartInternal(BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual nsresult
  StopInternal(BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual nsresult
  GetPairedDevicePropertiesInternal(const nsTArray<nsString>& aDeviceAddresses,
                                    BluetoothReplyRunnable* aRunnable)
                                    MOZ_OVERRIDE;

  virtual nsresult
  GetConnectedDevicePropertiesInternal(uint16_t aServiceUuid,
                                       BluetoothReplyRunnable* aRunnable)
                                       MOZ_OVERRIDE;
  virtual nsresult
  FetchUuidsInternal(const nsAString& aDeviceAddress,
                     BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual nsresult
  StopDiscoveryInternal(BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual nsresult
  StartDiscoveryInternal(BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual nsresult
  SetProperty(BluetoothObjectType aType,
              const BluetoothNamedValue& aValue,
              BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual nsresult
  CreatePairedDeviceInternal(const nsAString& aAddress,
                             int aTimeout,
                             BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual nsresult
  RemoveDeviceInternal(const nsAString& aObjectPath,
                       BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual nsresult
  GetServiceChannel(const nsAString& aDeviceAddress,
                    const nsAString& aServiceUuid,
                    BluetoothProfileManagerBase* aManager) MOZ_OVERRIDE;

  virtual bool
  UpdateSdpRecords(const nsAString& aDeviceAddress,
                   BluetoothProfileManagerBase* aManager) MOZ_OVERRIDE;

  virtual void
  SetPinCodeInternal(const nsAString& aDeviceAddress,
                     const nsAString& aPinCode,
                     BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual void
  SetPasskeyInternal(const nsAString& aDeviceAddress,
                     uint32_t aPasskey,
                     BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual void
  SetPairingConfirmationInternal(const nsAString& aDeviceAddress,
                                 bool aConfirm,
                                 BluetoothReplyRunnable* aRunnable)
                                 MOZ_OVERRIDE;

  virtual void
  Connect(const nsAString& aDeviceAddress,
          uint32_t aCod,
          uint16_t aServiceUuid,
          BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual void
  Disconnect(const nsAString& aDeviceAddress,
             uint16_t aServiceUuid,
             BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual bool
  IsConnected(uint16_t aServiceUuid) MOZ_OVERRIDE;

  virtual void
  SendFile(const nsAString& aDeviceAddress,
           BlobParent* aBlobParent,
           BlobChild* aBlobChild,
           BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual void
  SendFile(const nsAString& aDeviceAddress,
           nsIDOMBlob* aBlob,
           BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual void
  StopSendingFile(const nsAString& aDeviceAddress,
                  BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual void
  ConfirmReceivingFile(const nsAString& aDeviceAddress,
                       bool aConfirm,
                       BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual void
  ConnectSco(BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual void
  DisconnectSco(BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual void
  IsScoConnected(BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

#ifdef MOZ_B2G_RIL
  virtual void
  AnswerWaitingCall(BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual void
  IgnoreWaitingCall(BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual void
  ToggleCalls(BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;
#endif

  virtual void
  SendMetaData(const nsAString& aTitle,
               const nsAString& aArtist,
               const nsAString& aAlbum,
               int64_t aMediaNumber,
               int64_t aTotalMediaCount,
               int64_t aDuration,
               BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual void
  SendPlayStatus(int64_t aDuration,
                 int64_t aPosition,
                 const nsAString& aPlayStatus,
                 BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual void
  UpdatePlayStatus(uint32_t aDuration,
                   uint32_t aPosition,
                   ControlPlayStatus aPlayStatus) MOZ_OVERRIDE;

  virtual nsresult
  SendSinkMessage(const nsAString& aDeviceAddresses,
                  const nsAString& aMessage) MOZ_OVERRIDE;

  virtual nsresult
  SendInputMessage(const nsAString& aDeviceAddresses,
                   const nsAString& aMessage) MOZ_OVERRIDE;

  virtual void
  ConnectGattClientInternal(const nsAString& aAppUuid,
                            const nsAString& aDeviceAddress,
                            BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual void
  DisconnectGattClientInternal(const nsAString& aAppUuid,
                               const nsAString& aDeviceAddress,
                               BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual void
  UnregisterGattClientInternal(int aClientIf,
                               BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

protected:
  BluetoothServiceChildProcess();
  virtual ~BluetoothServiceChildProcess();

  void
  NoteDeadActor();

  void
  NoteShutdownInitiated();

  virtual nsresult
  HandleStartup() MOZ_OVERRIDE;

  virtual nsresult
  HandleShutdown() MOZ_OVERRIDE;

private:
  bool
  IsSignalRegistered(const nsAString& aNodeName) {
    return !!mBluetoothSignalObserverTable.Get(aNodeName);
  }
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_ipc_bluetoothservicechildprocess_h__
