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
#include "mozilla/UniquePtr.h"

namespace mozilla {
namespace ipc {

class DaemonSocket;
class ListenSocket;

}
}

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothDaemonA2dpInterface;
class BluetoothDaemonAvrcpInterface;
class BluetoothDaemonCoreInterface;
class BluetoothDaemonGattInterface;
class BluetoothDaemonHandsfreeInterface;
class BluetoothDaemonProtocol;
class BluetoothDaemonSetupInterface;
class BluetoothDaemonSocketInterface;
class BluetoothDaemonHidInterface;

class BluetoothDaemonInterface final
  : public BluetoothInterface
  , public mozilla::ipc::DaemonSocketConsumer
  , public mozilla::ipc::ListenSocketConsumer
{
public:
  class CleanupResultHandler;
  class InitResultHandler;

  friend class CleanupResultHandler;
  friend class InitResultHandler;

  static BluetoothDaemonInterface* GetInstance();

  void Init(BluetoothNotificationHandler* aNotificationHandler,
            BluetoothResultHandler* aRes) override;
  void Cleanup(BluetoothResultHandler* aRes) override;

  /* Service Interfaces */

  BluetoothSetupInterface* GetBluetoothSetupInterface() override;
  BluetoothCoreInterface* GetBluetoothCoreInterface() override;
  BluetoothSocketInterface* GetBluetoothSocketInterface() override;
  BluetoothHandsfreeInterface* GetBluetoothHandsfreeInterface() override;
  BluetoothA2dpInterface* GetBluetoothA2dpInterface() override;
  BluetoothAvrcpInterface* GetBluetoothAvrcpInterface() override;
  BluetoothGattInterface* GetBluetoothGattInterface() override;
  BluetoothHidInterface* GetBluetoothHidInterface() override;

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

  static BluetoothNotificationHandler* sNotificationHandler;

  nsCString mListenSocketName;
  RefPtr<mozilla::ipc::ListenSocket> mListenSocket;
  RefPtr<mozilla::ipc::DaemonSocket> mCmdChannel;
  RefPtr<mozilla::ipc::DaemonSocket> mNtfChannel;
  UniquePtr<BluetoothDaemonProtocol> mProtocol;

  nsTArray<RefPtr<BluetoothResultHandler> > mResultHandlerQ;

  UniquePtr<BluetoothDaemonSetupInterface> mSetupInterface;
  UniquePtr<BluetoothDaemonCoreInterface> mCoreInterface;
  UniquePtr<BluetoothDaemonSocketInterface> mSocketInterface;
  UniquePtr<BluetoothDaemonHidInterface> mHidInterface;
  UniquePtr<BluetoothDaemonHandsfreeInterface> mHandsfreeInterface;
  UniquePtr<BluetoothDaemonA2dpInterface> mA2dpInterface;
  UniquePtr<BluetoothDaemonAvrcpInterface> mAvrcpInterface;
  UniquePtr<BluetoothDaemonGattInterface> mGattInterface;
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_bluedroid_BluetoothDaemonInterface_h
