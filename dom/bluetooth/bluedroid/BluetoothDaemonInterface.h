/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluedroid_BluetoothDaemonInterface_h
#define mozilla_dom_bluetooth_bluedroid_BluetoothDaemonInterface_h

#include "BluetoothInterface.h"
#include "mozilla/ipc/DaemonSocketConsumer.h"
#include "mozilla/ipc/ListenSocketConsumer.h"

namespace mozilla {
namespace ipc {

class DaemonSocket;
class ListenSocket;

}
}

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothDaemonA2dpInterface;
class BluetoothDaemonAvrcpInterface;
class BluetoothDaemonGattInterface;
class BluetoothDaemonHandsfreeInterface;
class BluetoothDaemonProtocol;
class BluetoothDaemonSetupInterface;
class BluetoothDaemonSocketInterface;

class BluetoothDaemonInterface final
  : public BluetoothInterface
  , public mozilla::ipc::DaemonSocketConsumer
  , public mozilla::ipc::ListenSocketConsumer
{
public:
  class CleanupResultHandler;
  class InitResultHandler;
  class StartDaemonTask;

  friend class CleanupResultHandler;
  friend class InitResultHandler;
  friend class StartDaemonTask;

  static BluetoothDaemonInterface* GetInstance();

  void SetNotificationHandler(
    BluetoothCoreNotificationHandler* aNotificationHandler) override;

  void Init(BluetoothNotificationHandler* aNotificationHandler,
            BluetoothResultHandler* aRes) override;
  void Cleanup(BluetoothResultHandler* aRes) override;

  void Enable(BluetoothCoreResultHandler* aRes) override;
  void Disable(BluetoothCoreResultHandler* aRes) override;

  /* Adapter Properties */

  void GetAdapterProperties(BluetoothCoreResultHandler* aRes) override;
  void GetAdapterProperty(BluetoothPropertyType aType,
                          BluetoothCoreResultHandler* aRes) override;
  void SetAdapterProperty(const BluetoothProperty& aProperty,
                          BluetoothCoreResultHandler* aRes) override;

  /* Remote Device Properties */

  void GetRemoteDeviceProperties(const BluetoothAddress& aRemoteAddr,
                                 BluetoothCoreResultHandler* aRes) override;
  void GetRemoteDeviceProperty(const BluetoothAddress& aRemoteAddr,
                               BluetoothPropertyType aType,
                               BluetoothCoreResultHandler* aRes) override;
  void SetRemoteDeviceProperty(const BluetoothAddress& aRemoteAddr,
                               const BluetoothProperty& aProperty,
                               BluetoothCoreResultHandler* aRes) override;

  /* Remote Services */

  void GetRemoteServiceRecord(const BluetoothAddress& aRemoteAddr,
                              const BluetoothUuid& aUuid,
                              BluetoothCoreResultHandler* aRes) override;
  void GetRemoteServices(const BluetoothAddress& aRemoteAddr,
                         BluetoothCoreResultHandler* aRes) override;

  /* Discovery */

  void StartDiscovery(BluetoothCoreResultHandler* aRes) override;
  void CancelDiscovery(BluetoothCoreResultHandler* aRes) override;

  /* Bonds */

  void CreateBond(const BluetoothAddress& aBdAddr,
                  BluetoothTransport aTransport,
                  BluetoothCoreResultHandler* aRes) override;
  void RemoveBond(const BluetoothAddress& aBdAddr,
                  BluetoothCoreResultHandler* aRes) override;
  void CancelBond(const BluetoothAddress& aBdAddr,
                  BluetoothCoreResultHandler* aRes) override;

  /* Connection */

  void GetConnectionState(const BluetoothAddress& aBdAddr,
                          BluetoothCoreResultHandler* aRes) override;

  /* Authentication */

  void PinReply(const BluetoothAddress& aBdAddr, bool aAccept,
                const BluetoothPinCode& aPinCode,
                BluetoothCoreResultHandler* aRes) override;

  void SspReply(const BluetoothAddress& aBdAddr,
                BluetoothSspVariant aVariant,
                bool aAccept, uint32_t aPasskey,
                BluetoothCoreResultHandler* aRes) override;

  /* DUT Mode */

  void DutModeConfigure(bool aEnable, BluetoothCoreResultHandler* aRes);
  void DutModeSend(uint16_t aOpcode, uint8_t* aBuf, uint8_t aLen,
                   BluetoothCoreResultHandler* aRes) override;

  /* LE Mode */

  void LeTestMode(uint16_t aOpcode, uint8_t* aBuf, uint8_t aLen,
                  BluetoothCoreResultHandler* aRes) override;

  /* Energy Information */

  void ReadEnergyInfo(BluetoothCoreResultHandler* aRes) override;

  /* Service Interfaces */

  BluetoothSetupInterface* GetBluetoothSetupInterface() override;
  BluetoothSocketInterface* GetBluetoothSocketInterface() override;
  BluetoothHandsfreeInterface* GetBluetoothHandsfreeInterface() override;
  BluetoothA2dpInterface* GetBluetoothA2dpInterface() override;
  BluetoothAvrcpInterface* GetBluetoothAvrcpInterface() override;
  BluetoothGattInterface* GetBluetoothGattInterface() override;

protected:
  enum Channel {
    LISTEN_SOCKET,
    CMD_CHANNEL,
    NTF_CHANNEL
  };

  BluetoothDaemonInterface();
  ~BluetoothDaemonInterface();

  // Methods for |DaemonSocketConsumer| and |ListenSocketConsumer|
  //

  void OnConnectSuccess(int aIndex) override;
  void OnConnectError(int aIndex) override;
  void OnDisconnect(int aIndex) override;

private:
  void DispatchError(BluetoothCoreResultHandler* aRes,
                     BluetoothStatus aStatus);
  void DispatchError(BluetoothCoreResultHandler* aRes,
                     nsresult aRv);

  void DispatchError(BluetoothResultHandler* aRes, BluetoothStatus aStatus);
  void DispatchError(BluetoothResultHandler* aRes, nsresult aRv);

  static BluetoothNotificationHandler* sNotificationHandler;

  nsCString mListenSocketName;
  RefPtr<mozilla::ipc::ListenSocket> mListenSocket;
  RefPtr<mozilla::ipc::DaemonSocket> mCmdChannel;
  RefPtr<mozilla::ipc::DaemonSocket> mNtfChannel;
  nsAutoPtr<BluetoothDaemonProtocol> mProtocol;

  nsTArray<RefPtr<BluetoothResultHandler> > mResultHandlerQ;

  nsAutoPtr<BluetoothDaemonSetupInterface> mSetupInterface;
  nsAutoPtr<BluetoothDaemonSocketInterface> mSocketInterface;
  nsAutoPtr<BluetoothDaemonHandsfreeInterface> mHandsfreeInterface;
  nsAutoPtr<BluetoothDaemonA2dpInterface> mA2dpInterface;
  nsAutoPtr<BluetoothDaemonAvrcpInterface> mAvrcpInterface;
  nsAutoPtr<BluetoothDaemonGattInterface> mGattInterface;
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_bluedroid_BluetoothDaemonInterface_h
