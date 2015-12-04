/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluedroid_BluetoothDaemonHidInterface_h
#define mozilla_dom_bluetooth_bluedroid_BluetoothDaemonHidInterface_h

#include "BluetoothDaemonHelpers.h"
#include "BluetoothInterface.h"
#include "mozilla/ipc/DaemonRunnables.h"

BEGIN_BLUETOOTH_NAMESPACE

using mozilla::ipc::DaemonSocketPDU;
using mozilla::ipc::DaemonSocketPDUHeader;
using mozilla::ipc::DaemonSocketResultHandler;

class BluetoothDaemonHidModule
{
public:
  enum {
    SERVICE_ID = 0x03
  };

  enum {
    OPCODE_ERROR = 0x00,
    OPCODE_CONNECT = 0x01,
    OPCODE_DISCONNECT = 0x02,
    OPCODE_VIRTUAL_UNPLUG = 0x03,
    OPCODE_SET_INFO = 0x04,
    OPCODE_GET_PROTOCOL = 0x05,
    OPCODE_SET_PROTOCOL = 0x06,
    OPCODE_GET_REPORT = 0x07,
    OPCODE_SET_REPORT = 0x08,
    OPCODE_SEND_DATA = 0x09
  };

  virtual nsresult Send(DaemonSocketPDU* aPDU,
                        DaemonSocketResultHandler* aRes) = 0;

  void SetNotificationHandler(
    BluetoothHidNotificationHandler* aNotificationHandler);

  //
  // Commands
  //

  nsresult ConnectCmd(const BluetoothAddress& aBdAddr,
                      BluetoothHidResultHandler* aRes);
  nsresult DisconnectCmd(const BluetoothAddress& aBdAddr,
                         BluetoothHidResultHandler* aRes);

  /* Virtual Unplug */

  nsresult VirtualUnplugCmd(const BluetoothAddress& aBdAddr,
                            BluetoothHidResultHandler* aRes);

  /* Set Info */

  nsresult SetInfoCmd(
    const BluetoothAddress& aBdAddr,
    const BluetoothHidInfoParam& aHidInfoParam,
    BluetoothHidResultHandler* aRes);

  /* Protocol */

  nsresult GetProtocolCmd(const BluetoothAddress& aBdAddr,
                          BluetoothHidProtocolMode aHidProtocolMode,
                          BluetoothHidResultHandler* aRes);
  nsresult SetProtocolCmd(const BluetoothAddress& aBdAddr,
                          BluetoothHidProtocolMode aHidProtocolMode,
                          BluetoothHidResultHandler* aRes);

  /* Report */

  nsresult GetReportCmd(const BluetoothAddress& aBdAddr,
                        BluetoothHidReportType aType,
                        uint8_t aReportId,
                        uint16_t aBuffSize,
                        BluetoothHidResultHandler* aRes);
  nsresult SetReportCmd(const BluetoothAddress& aBdAddr,
                        BluetoothHidReportType aType,
                        const BluetoothHidReport& aReport,
                        BluetoothHidResultHandler* aRes);

  /* Send Data */

  nsresult SendDataCmd(const BluetoothAddress& aBdAddr,
                       uint16_t aDataLen, const uint8_t* aData,
                       BluetoothHidResultHandler* aRes);

protected:
  void HandleSvc(const DaemonSocketPDUHeader& aHeader,
                 DaemonSocketPDU& aPDU,
                 DaemonSocketResultHandler* aRes);

  //
  // Responses
  //

  typedef mozilla::ipc::DaemonResultRunnable0<
    BluetoothHidResultHandler, void>
    ResultRunnable;

  typedef mozilla::ipc::DaemonResultRunnable1<
    BluetoothHidResultHandler, void, BluetoothStatus, BluetoothStatus>
    ErrorRunnable;

  void ErrorRsp(const DaemonSocketPDUHeader& aHeader,
                DaemonSocketPDU& aPDU,
                BluetoothHidResultHandler* aRes);

  void ConnectRsp(const DaemonSocketPDUHeader& aHeader,
                  DaemonSocketPDU& aPDU,
                  BluetoothHidResultHandler* aRes);

  void DisconnectRsp(const DaemonSocketPDUHeader& aHeader,
                     DaemonSocketPDU& aPDU,
                     BluetoothHidResultHandler* aRes);

  void VirtualUnplugRsp(const DaemonSocketPDUHeader& aHeader,
                        DaemonSocketPDU& aPDU,
                        BluetoothHidResultHandler* aRes);

  void SetInfoRsp(const DaemonSocketPDUHeader& aHeader,
                  DaemonSocketPDU& aPDU,
                  BluetoothHidResultHandler* aRes);

  void GetProtocolRsp(const DaemonSocketPDUHeader& aHeader,
                      DaemonSocketPDU& aPDU,
                      BluetoothHidResultHandler* aRes);

  void SetProtocolRsp(const DaemonSocketPDUHeader& aHeader,
                      DaemonSocketPDU& aPDU,
                      BluetoothHidResultHandler* aRes);

  void GetReportRsp(const DaemonSocketPDUHeader& aHeader,
                    DaemonSocketPDU& aPDU,
                    BluetoothHidResultHandler* aRes);

  void SetReportRsp(const DaemonSocketPDUHeader& aHeader,
                    DaemonSocketPDU& aPDU,
                    BluetoothHidResultHandler* aRes);

  void SendDataRsp(const DaemonSocketPDUHeader& aHeader,
                   DaemonSocketPDU& aPDU,
                   BluetoothHidResultHandler* aRes);

  void HandleRsp(const DaemonSocketPDUHeader& aHeader,
                 DaemonSocketPDU& aPDU,
                 DaemonSocketResultHandler* aRes);

  //
  // Notifications
  //

  class NotificationHandlerWrapper;

  typedef mozilla::ipc::DaemonNotificationRunnable2<
    NotificationHandlerWrapper, void,
    BluetoothAddress, BluetoothHidConnectionState,
    const BluetoothAddress&, BluetoothHidConnectionState>
    ConnectionStateNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable2<
    NotificationHandlerWrapper, void,
    BluetoothAddress, BluetoothHidInfoParam,
    const BluetoothAddress&, const BluetoothHidInfoParam&>
    HidInfoNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable3<
    NotificationHandlerWrapper, void,
    BluetoothAddress, BluetoothHidStatus, BluetoothHidProtocolMode,
    const BluetoothAddress&, BluetoothHidStatus, BluetoothHidProtocolMode>
    ProtocolModeNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable3<
    NotificationHandlerWrapper, void,
    BluetoothAddress, BluetoothHidStatus, uint16_t,
    const BluetoothAddress&, BluetoothHidStatus, uint16_t>
    IdleTimeNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable3<
    NotificationHandlerWrapper, void,
    BluetoothAddress, BluetoothHidStatus, BluetoothHidReport,
    const BluetoothAddress&, BluetoothHidStatus,
    const BluetoothHidReport&>
    GetReportNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable2<
    NotificationHandlerWrapper, void,
    BluetoothAddress, BluetoothHidStatus,
    const BluetoothAddress&, BluetoothHidStatus>
    VirtualUnplugNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable2<
    NotificationHandlerWrapper, void,
    BluetoothAddress, BluetoothHidStatus,
    const BluetoothAddress&, BluetoothHidStatus>
    HandshakeNotification;

  void ConnectionStateNtf(const DaemonSocketPDUHeader& aHeader,
                          DaemonSocketPDU& aPDU);

  void HidInfoNtf(const DaemonSocketPDUHeader& aHeader,
                  DaemonSocketPDU& aPDU);

  void ProtocolModeNtf(const DaemonSocketPDUHeader& aHeader,
                       DaemonSocketPDU& aPDU);

  void IdleTimeNtf(const DaemonSocketPDUHeader& aHeader,
                   DaemonSocketPDU& aPDU);

  void GetReportNtf(const DaemonSocketPDUHeader& aHeader,
                    DaemonSocketPDU& aPDU);

  void VirtualUnplugNtf(const DaemonSocketPDUHeader& aHeader,
                        DaemonSocketPDU& aPDU);

  void HandshakeNtf(const DaemonSocketPDUHeader& aHeader,
                    DaemonSocketPDU& aPDU);

  void HandleNtf(const DaemonSocketPDUHeader& aHeader,
                 DaemonSocketPDU& aPDU,
                 DaemonSocketResultHandler* aRes);

  static BluetoothHidNotificationHandler* sNotificationHandler;
};

class BluetoothDaemonHidInterface final
  : public BluetoothHidInterface
{
public:
  BluetoothDaemonHidInterface(BluetoothDaemonHidModule* aModule);
  ~BluetoothDaemonHidInterface();

  void SetNotificationHandler(
    BluetoothHidNotificationHandler* aNotificationHandler) override;

  /* Connect / Disconnect */

  void Connect(const BluetoothAddress& aBdAddr,
               BluetoothHidResultHandler* aRes) override;
  void Disconnect(const BluetoothAddress& aBdAddr,
                  BluetoothHidResultHandler* aRes) override;

  /* Virtual Unplug */

  void VirtualUnplug(const BluetoothAddress& aBdAddr,
                     BluetoothHidResultHandler* aRes) override;

  /* Set Info */

  void SetInfo(
    const BluetoothAddress& aBdAddr,
    const BluetoothHidInfoParam& aHidInfoParam,
    BluetoothHidResultHandler* aRes) override;

  /* Protocol */

  void GetProtocol(const BluetoothAddress& aBdAddr,
                   BluetoothHidProtocolMode aHidProtoclMode,
                   BluetoothHidResultHandler* aRes) override;

  void SetProtocol(const BluetoothAddress& aBdAddr,
                   BluetoothHidProtocolMode aHidProtocolMode,
                   BluetoothHidResultHandler* aRes) override;

  /* Report */

  void GetReport(const BluetoothAddress& aBdAddr,
                 BluetoothHidReportType aType,
                 uint8_t aReportId, uint16_t aBuffSize,
                 BluetoothHidResultHandler* aRes) override;
  void SetReport(const BluetoothAddress& aBdAddr,
                 BluetoothHidReportType aType,
                 const BluetoothHidReport& aReport,
                 BluetoothHidResultHandler* aRes) override;

  /* Send Data */

  void SendData(const BluetoothAddress& aBdAddr,
                uint16_t aDataLen, const uint8_t* aData,
                BluetoothHidResultHandler* aRes) override;

private:
  void DispatchError(BluetoothHidResultHandler* aRes,
                     BluetoothStatus aStatus);
  void DispatchError(BluetoothHidResultHandler* aRes, nsresult aRv);

  BluetoothDaemonHidModule* mModule;
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_bluedroid_BluetoothDaemonHidInterface_h
