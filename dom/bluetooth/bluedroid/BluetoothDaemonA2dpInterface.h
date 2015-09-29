/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluedroid_BluetoothDaemonA2dpInterface_h
#define mozilla_dom_bluetooth_bluedroid_BluetoothDaemonA2dpInterface_h

#include "BluetoothDaemonHelpers.h"
#include "BluetoothInterface.h"
#include "mozilla/ipc/DaemonRunnables.h"

BEGIN_BLUETOOTH_NAMESPACE

using mozilla::ipc::DaemonSocketPDU;
using mozilla::ipc::DaemonSocketPDUHeader;
using mozilla::ipc::DaemonSocketResultHandler;

class BluetoothDaemonA2dpModule
{
public:
  enum {
    SERVICE_ID = 0x06
  };

  enum {
    OPCODE_ERROR = 0x00,
    OPCODE_CONNECT = 0x01,
    OPCODE_DISCONNECT = 0x02
  };

  static const int MAX_NUM_CLIENTS;

  virtual nsresult Send(DaemonSocketPDU* aPDU,
                        DaemonSocketResultHandler* aRes) = 0;

  virtual nsresult RegisterModule(uint8_t aId, uint8_t aMode,
                                  uint32_t aMaxNumClients,
                                  BluetoothSetupResultHandler* aRes) = 0;

  virtual nsresult UnregisterModule(uint8_t aId,
                                    BluetoothSetupResultHandler* aRes) = 0;

  void SetNotificationHandler(
    BluetoothA2dpNotificationHandler* aNotificationHandler);

  //
  // Commands
  //

  nsresult ConnectCmd(const BluetoothAddress& aBdAddr,
                      BluetoothA2dpResultHandler* aRes);
  nsresult DisconnectCmd(const BluetoothAddress& aBdAddr,
                         BluetoothA2dpResultHandler* aRes);

protected:
  void HandleSvc(const DaemonSocketPDUHeader& aHeader,
                 DaemonSocketPDU& aPDU, DaemonSocketResultHandler* aRes);

  //
  // Responses
  //

  typedef mozilla::ipc::DaemonResultRunnable0<
    BluetoothA2dpResultHandler, void>
    ResultRunnable;

  typedef mozilla::ipc::DaemonResultRunnable1<
    BluetoothA2dpResultHandler, void, BluetoothStatus, BluetoothStatus>
    ErrorRunnable;

  void ErrorRsp(const DaemonSocketPDUHeader& aHeader,
                DaemonSocketPDU& aPDU,
                BluetoothA2dpResultHandler* aRes);

  void ConnectRsp(const DaemonSocketPDUHeader& aHeader,
                  DaemonSocketPDU& aPDU,
                  BluetoothA2dpResultHandler* aRes);

  void DisconnectRsp(const DaemonSocketPDUHeader& aHeader,
                     DaemonSocketPDU& aPDU,
                     BluetoothA2dpResultHandler* aRes);

  void HandleRsp(const DaemonSocketPDUHeader& aHeader,
                 DaemonSocketPDU& aPDU,
                 DaemonSocketResultHandler* aRes);

  //
  // Notifications
  //

  class NotificationHandlerWrapper;

  typedef mozilla::ipc::DaemonNotificationRunnable2<
    NotificationHandlerWrapper, void,
    BluetoothA2dpConnectionState, BluetoothAddress,
    BluetoothA2dpConnectionState, const BluetoothAddress&>
    ConnectionStateNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable2<
    NotificationHandlerWrapper, void,
    BluetoothA2dpAudioState, BluetoothAddress,
    BluetoothA2dpAudioState, const BluetoothAddress&>
    AudioStateNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable3<
    NotificationHandlerWrapper, void,
    BluetoothAddress, uint32_t, uint8_t,
    const BluetoothAddress&, uint32_t, uint8_t>
    AudioConfigNotification;

  class ConnectionStateInitOp;
  class AudioStateInitOp;
  class AudioConfigInitOp;

  void ConnectionStateNtf(const DaemonSocketPDUHeader& aHeader,
                          DaemonSocketPDU& aPDU);

  void AudioStateNtf(const DaemonSocketPDUHeader& aHeader,
                     DaemonSocketPDU& aPDU);

  void AudioConfigNtf(const DaemonSocketPDUHeader& aHeader,
                      DaemonSocketPDU& aPDU);

  void HandleNtf(const DaemonSocketPDUHeader& aHeader,
                 DaemonSocketPDU& aPDU,
                 DaemonSocketResultHandler* aRes);

  static BluetoothA2dpNotificationHandler* sNotificationHandler;
};

class BluetoothDaemonA2dpInterface final
  : public BluetoothA2dpInterface
{
  class CleanupResultHandler;
  class InitResultHandler;

public:
  BluetoothDaemonA2dpInterface(BluetoothDaemonA2dpModule* aModule);
  ~BluetoothDaemonA2dpInterface();

  void Init(
    BluetoothA2dpNotificationHandler* aNotificationHandler,
    BluetoothA2dpResultHandler* aRes) override;
  void Cleanup(BluetoothA2dpResultHandler* aRes) override;

  /* Connect / Disconnect */

  void Connect(const BluetoothAddress& aBdAddr,
               BluetoothA2dpResultHandler* aRes) override;
  void Disconnect(const BluetoothAddress& aBdAddr,
                  BluetoothA2dpResultHandler* aRes) override;

private:
  void DispatchError(BluetoothA2dpResultHandler* aRes,
                     BluetoothStatus aStatus);
  void DispatchError(BluetoothA2dpResultHandler* aRes, nsresult aRv);

  BluetoothDaemonA2dpModule* mModule;
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_bluedroid_BluetoothDaemonA2dpInterface_h
