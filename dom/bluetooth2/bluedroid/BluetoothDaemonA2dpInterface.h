/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothdaemona2dpinterface_h
#define mozilla_dom_bluetooth_bluetoothdaemona2dpinterface_h

#include "BluetoothDaemonHelpers.h"
#include "BluetoothInterface.h"
#include "BluetoothInterfaceHelpers.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothSetupResultHandler;

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

  virtual nsresult Send(BluetoothDaemonPDU* aPDU, void* aUserData) = 0;

  virtual nsresult RegisterModule(uint8_t aId, uint8_t aMode,
                                  BluetoothSetupResultHandler* aRes) = 0;

  virtual nsresult UnregisterModule(uint8_t aId,
                                    BluetoothSetupResultHandler* aRes) = 0;

  void SetNotificationHandler(
    BluetoothA2dpNotificationHandler* aNotificationHandler);

  //
  // Commands
  //

  nsresult ConnectCmd(const nsAString& aBdAddr,
                      BluetoothA2dpResultHandler* aRes);
  nsresult DisconnectCmd(const nsAString& aBdAddr,
                         BluetoothA2dpResultHandler* aRes);

protected:
  nsresult Send(BluetoothDaemonPDU* aPDU,
                BluetoothA2dpResultHandler* aRes);

  void HandleSvc(const BluetoothDaemonPDUHeader& aHeader,
                 BluetoothDaemonPDU& aPDU, void* aUserData);

  //
  // Responses
  //

  typedef BluetoothResultRunnable0<BluetoothA2dpResultHandler, void>
    ResultRunnable;

  typedef BluetoothResultRunnable1<BluetoothA2dpResultHandler, void,
                                   BluetoothStatus, BluetoothStatus>
    ErrorRunnable;

  void ErrorRsp(const BluetoothDaemonPDUHeader& aHeader,
                BluetoothDaemonPDU& aPDU,
                BluetoothA2dpResultHandler* aRes);

  void ConnectRsp(const BluetoothDaemonPDUHeader& aHeader,
                  BluetoothDaemonPDU& aPDU,
                  BluetoothA2dpResultHandler* aRes);

  void DisconnectRsp(const BluetoothDaemonPDUHeader& aHeader,
                     BluetoothDaemonPDU& aPDU,
                     BluetoothA2dpResultHandler* aRes);

  void HandleRsp(const BluetoothDaemonPDUHeader& aHeader,
                 BluetoothDaemonPDU& aPDU,
                 void* aUserData);

  //
  // Notifications
  //

  class NotificationHandlerWrapper;

  typedef BluetoothNotificationRunnable2<NotificationHandlerWrapper, void,
                                         BluetoothA2dpConnectionState,
                                         nsString,
                                         BluetoothA2dpConnectionState,
                                         const nsAString&>
    ConnectionStateNotification;

  typedef BluetoothNotificationRunnable2<NotificationHandlerWrapper, void,
                                         BluetoothA2dpAudioState,
                                         nsString,
                                         BluetoothA2dpAudioState,
                                         const nsAString&>
    AudioStateNotification;

  class AudioStateInitOp;
  class ConnectionStateInitOp;

  void ConnectionStateNtf(const BluetoothDaemonPDUHeader& aHeader,
                          BluetoothDaemonPDU& aPDU);

  void AudioStateNtf(const BluetoothDaemonPDUHeader& aHeader,
                     BluetoothDaemonPDU& aPDU);

  void HandleNtf(const BluetoothDaemonPDUHeader& aHeader,
                 BluetoothDaemonPDU& aPDU,
                 void* aUserData);

  static BluetoothA2dpNotificationHandler* sNotificationHandler;
};

class BluetoothDaemonA2dpInterface MOZ_FINAL
  : public BluetoothA2dpInterface
{
  class CleanupResultHandler;
  class InitResultHandler;

public:
  BluetoothDaemonA2dpInterface(BluetoothDaemonA2dpModule* aModule);
  ~BluetoothDaemonA2dpInterface();

  void Init(
    BluetoothA2dpNotificationHandler* aNotificationHandler,
    BluetoothA2dpResultHandler* aRes);
  void Cleanup(BluetoothA2dpResultHandler* aRes);

  /* Connect / Disconnect */

  void Connect(const nsAString& aBdAddr,
               BluetoothA2dpResultHandler* aRes);
  void Disconnect(const nsAString& aBdAddr,
                  BluetoothA2dpResultHandler* aRes);

private:
  void DispatchError(BluetoothA2dpResultHandler* aRes,
                     BluetoothStatus aStatus);

  BluetoothDaemonA2dpModule* mModule;
};

END_BLUETOOTH_NAMESPACE

#endif
