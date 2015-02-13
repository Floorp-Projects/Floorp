/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluedroid_bluetoothdaemoninterface_h__
#define mozilla_dom_bluetooth_bluedroid_bluetoothdaemoninterface_h__

#include "BluetoothInterface.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothDaemonListenSocket;
class BluetoothDaemonChannel;
class BluetoothDaemonA2dpInterface;
class BluetoothDaemonAvrcpInterface;
class BluetoothDaemonHandsfreeInterface;
class BluetoothDaemonProtocol;
class BluetoothDaemonSocketInterface;

class BluetoothDaemonInterface MOZ_FINAL : public BluetoothInterface
{
public:
  class CleanupResultHandler;
  class InitResultHandler;

  friend class BluetoothDaemonListenSocket;
  friend class BluetoothDaemonChannel;
  friend class CleanupResultHandler;
  friend class InitResultHandler;

  static BluetoothDaemonInterface* GetInstance();

  void Init(BluetoothNotificationHandler* aNotificationHandler,
            BluetoothResultHandler* aRes);
  void Cleanup(BluetoothResultHandler* aRes);

  void Enable(BluetoothResultHandler* aRes);
  void Disable(BluetoothResultHandler* aRes);

  /* Adapter Properties */

  void GetAdapterProperties(BluetoothResultHandler* aRes);
  void GetAdapterProperty(const nsAString& aName,
                          BluetoothResultHandler* aRes);
  void SetAdapterProperty(const BluetoothNamedValue& aProperty,
                          BluetoothResultHandler* aRes);

  /* Remote Device Properties */

  void GetRemoteDeviceProperties(const nsAString& aRemoteAddr,
                                 BluetoothResultHandler* aRes);
  void GetRemoteDeviceProperty(const nsAString& aRemoteAddr,
                               const nsAString& aName,
                               BluetoothResultHandler* aRes);
  void SetRemoteDeviceProperty(const nsAString& aRemoteAddr,
                               const BluetoothNamedValue& aProperty,
                               BluetoothResultHandler* aRes);

  /* Remote Services */

  void GetRemoteServiceRecord(const nsAString& aRemoteAddr,
                              const uint8_t aUuid[16],
                              BluetoothResultHandler* aRes);
  void GetRemoteServices(const nsAString& aRemoteAddr,
                         BluetoothResultHandler* aRes);

  /* Discovery */

  void StartDiscovery(BluetoothResultHandler* aRes);
  void CancelDiscovery(BluetoothResultHandler* aRes);

  /* Bonds */

  void CreateBond(const nsAString& aBdAddr, BluetoothResultHandler* aRes);
  void RemoveBond(const nsAString& aBdAddr, BluetoothResultHandler* aRes);
  void CancelBond(const nsAString& aBdAddr, BluetoothResultHandler* aRes);

  /* Authentication */

  void PinReply(const nsAString& aBdAddr, bool aAccept,
                const nsAString& aPinCode,
                BluetoothResultHandler* aRes);

  void SspReply(const nsAString& aBdAddr, const nsAString& aVariant,
                bool aAccept, uint32_t aPasskey,
                BluetoothResultHandler* aRes);

  /* DUT Mode */

  void DutModeConfigure(bool aEnable, BluetoothResultHandler* aRes);
  void DutModeSend(uint16_t aOpcode, uint8_t* aBuf, uint8_t aLen,
                   BluetoothResultHandler* aRes);

  /* LE Mode */

  void LeTestMode(uint16_t aOpcode, uint8_t* aBuf, uint8_t aLen,
                  BluetoothResultHandler* aRes);

  /* Profile Interfaces */

  BluetoothSocketInterface* GetBluetoothSocketInterface() MOZ_OVERRIDE;
  BluetoothHandsfreeInterface* GetBluetoothHandsfreeInterface() MOZ_OVERRIDE;
  BluetoothA2dpInterface* GetBluetoothA2dpInterface() MOZ_OVERRIDE;
  BluetoothAvrcpInterface* GetBluetoothAvrcpInterface() MOZ_OVERRIDE;
  BluetoothGattInterface* GetBluetoothGattInterface() MOZ_OVERRIDE;

protected:
  enum Channel {
    LISTEN_SOCKET,
    CMD_CHANNEL,
    NTF_CHANNEL
  };

  BluetoothDaemonInterface();
  ~BluetoothDaemonInterface();

  void OnConnectSuccess(enum Channel aChannel);
  void OnConnectError(enum Channel aChannel);
  void OnDisconnect(enum Channel aChannel);

  nsresult CreateRandomAddressString(const nsACString& aPrefix,
                                     unsigned long aPostfixLength,
                                     nsACString& aAddress);

private:
  void DispatchError(BluetoothResultHandler* aRes, BluetoothStatus aStatus);

  nsCString mListenSocketName;
  nsRefPtr<BluetoothDaemonListenSocket> mListenSocket;
  nsRefPtr<BluetoothDaemonChannel> mCmdChannel;
  nsRefPtr<BluetoothDaemonChannel> mNtfChannel;
  nsAutoPtr<BluetoothDaemonProtocol> mProtocol;

  nsTArray<nsRefPtr<BluetoothResultHandler> > mResultHandlerQ;

  nsAutoPtr<BluetoothDaemonSocketInterface> mSocketInterface;
  nsAutoPtr<BluetoothDaemonHandsfreeInterface> mHandsfreeInterface;
  nsAutoPtr<BluetoothDaemonA2dpInterface> mA2dpInterface;
  nsAutoPtr<BluetoothDaemonAvrcpInterface> mAvrcpInterface;
};

END_BLUETOOTH_NAMESPACE

#endif
