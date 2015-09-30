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

  void Init(BluetoothNotificationHandler* aNotificationHandler,
            BluetoothResultHandler* aRes) override;
  void Cleanup(BluetoothResultHandler* aRes) override;

  void Enable(BluetoothResultHandler* aRes) override;
  void Disable(BluetoothResultHandler* aRes) override;

  /* Adapter Properties */

  void GetAdapterProperties(BluetoothResultHandler* aRes) override;
  void GetAdapterProperty(const nsAString& aName,
                          BluetoothResultHandler* aRes) override;
  void SetAdapterProperty(const BluetoothNamedValue& aProperty,
                          BluetoothResultHandler* aRes) override;

  /* Remote Device Properties */

  void GetRemoteDeviceProperties(const BluetoothAddress& aRemoteAddr,
                                 BluetoothResultHandler* aRes) override;
  void GetRemoteDeviceProperty(const BluetoothAddress& aRemoteAddr,
                               const nsAString& aName,
                               BluetoothResultHandler* aRes) override;
  void SetRemoteDeviceProperty(const BluetoothAddress& aRemoteAddr,
                               const BluetoothNamedValue& aProperty,
                               BluetoothResultHandler* aRes) override;

  /* Remote Services */

  void GetRemoteServiceRecord(const BluetoothAddress& aRemoteAddr,
                              const BluetoothUuid& aUuid,
                              BluetoothResultHandler* aRes) override;
  void GetRemoteServices(const BluetoothAddress& aRemoteAddr,
                         BluetoothResultHandler* aRes) override;

  /* Discovery */

  void StartDiscovery(BluetoothResultHandler* aRes) override;
  void CancelDiscovery(BluetoothResultHandler* aRes) override;

  /* Bonds */

  void CreateBond(const BluetoothAddress& aBdAddr,
                  BluetoothTransport aTransport,
                  BluetoothResultHandler* aRes) override;
  void RemoveBond(const BluetoothAddress& aBdAddr,
                  BluetoothResultHandler* aRes) override;
  void CancelBond(const BluetoothAddress& aBdAddr,
                  BluetoothResultHandler* aRes) override;

  /* Connection */

  void GetConnectionState(const BluetoothAddress& aBdAddr,
                          BluetoothResultHandler* aRes) override;

  /* Authentication */

  void PinReply(const BluetoothAddress& aBdAddr, bool aAccept,
                const nsAString& aPinCode,
                BluetoothResultHandler* aRes) override;

  void SspReply(const BluetoothAddress& aBdAddr,
                BluetoothSspVariant aVariant,
                bool aAccept, uint32_t aPasskey,
                BluetoothResultHandler* aRes) override;

  /* DUT Mode */

  void DutModeConfigure(bool aEnable, BluetoothResultHandler* aRes);
  void DutModeSend(uint16_t aOpcode, uint8_t* aBuf, uint8_t aLen,
                   BluetoothResultHandler* aRes) override;

  /* LE Mode */

  void LeTestMode(uint16_t aOpcode, uint8_t* aBuf, uint8_t aLen,
                  BluetoothResultHandler* aRes) override;

  /* Energy Information */

  void ReadEnergyInfo(BluetoothResultHandler* aRes) override;

  /* Profile Interfaces */

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
  void DispatchError(BluetoothResultHandler* aRes, BluetoothStatus aStatus);
  void DispatchError(BluetoothResultHandler* aRes, nsresult aRv);

  nsCString mListenSocketName;
  nsRefPtr<mozilla::ipc::ListenSocket> mListenSocket;
  nsRefPtr<mozilla::ipc::DaemonSocket> mCmdChannel;
  nsRefPtr<mozilla::ipc::DaemonSocket> mNtfChannel;
  nsAutoPtr<BluetoothDaemonProtocol> mProtocol;

  nsTArray<nsRefPtr<BluetoothResultHandler> > mResultHandlerQ;

  nsAutoPtr<BluetoothDaemonSocketInterface> mSocketInterface;
  nsAutoPtr<BluetoothDaemonHandsfreeInterface> mHandsfreeInterface;
  nsAutoPtr<BluetoothDaemonA2dpInterface> mA2dpInterface;
  nsAutoPtr<BluetoothDaemonAvrcpInterface> mAvrcpInterface;
  nsAutoPtr<BluetoothDaemonGattInterface> mGattInterface;
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_bluedroid_BluetoothDaemonInterface_h
