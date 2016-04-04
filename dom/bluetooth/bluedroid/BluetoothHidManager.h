/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluedroid_BluetoothHidManager_h
#define mozilla_dom_bluetooth_bluedroid_BluetoothHidMnagaer_h

#include "BluetoothCommon.h"
#include "BluetoothInterface.h"
#include "BluetoothProfileController.h"
#include "BluetoothProfileManagerBase.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothHidManager : public BluetoothProfileManagerBase
                          , public BluetoothHidNotificationHandler
{
public:
  BT_DECL_PROFILE_MGR_BASE

  static const int MAX_NUM_CLIENTS;

  void OnConnectError();
  void OnDisconnectError();

  virtual void GetName(nsACString& aName)
  {
    aName.AssignLiteral("HID");
  }

  static BluetoothHidManager* Get();
  static void InitHidInterface(BluetoothProfileResultHandler* aRes);
  static void DeinitHidInterface(BluetoothProfileResultHandler* aRes);

  void HandleBackendError();
  void GetReport(const BluetoothHidReportType& aReportType,
                 const uint8_t aReportId,
                 const uint16_t aBufSize);
  void SendData(const uint16_t aDataLen, const uint8_t* aData);
  void SetReport(const BluetoothHidReportType& aReportType,
                 const BluetoothHidReport& aReport);
  void VirtualUnplug();

protected:
  virtual ~BluetoothHidManager();

private:
  class DeinitProfileResultHandlerRunnable;
  class InitProfileResultHandlerRunnable;
  class RegisterModuleResultHandler;
  class UnregisterModuleResultHandler;

  class ConnectResultHandler;
  class DisconnectResultHandler;
  class GetReportResultHandler;
  class SendDataResultHandler;
  class SetReportResultHandler;
  class VirtualUnplugResultHandler;

  BluetoothHidManager();
  void Uninit();
  void HandleShutdown();
  void NotifyConnectionStateChanged(const nsAString& aType);

  //
  // Bluetooth notifications
  //

  void ConnectionStateNotification(
    const BluetoothAddress& aBdAddr,
    BluetoothHidConnectionState aState) override;

  bool mHidConnected;
  BluetoothAddress mDeviceAddress;
  RefPtr<BluetoothProfileController> mController;
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_bluedroid_BluetoothHidManager_h
