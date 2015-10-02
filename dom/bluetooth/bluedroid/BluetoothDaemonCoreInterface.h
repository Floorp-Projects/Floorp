/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluedroid_BluetoothDaemonCoreInterface_h
#define mozilla_dom_bluetooth_bluedroid_BluetoothDaemonCoreInterface_h

#include "BluetoothDaemonHelpers.h"
#include "BluetoothInterface.h"
#include "mozilla/ipc/DaemonRunnables.h"

BEGIN_BLUETOOTH_NAMESPACE

using mozilla::ipc::DaemonSocketPDU;
using mozilla::ipc::DaemonSocketPDUHeader;
using mozilla::ipc::DaemonSocketResultHandler;

class BluetoothDaemonCoreModule
{
public:
  enum {
    SERVICE_ID = 0x01
  };

  enum {
    OPCODE_ERROR = 0x00,
    OPCODE_ENABLE = 0x01,
    OPCODE_DISABLE = 0x02,
    OPCODE_GET_ADAPTER_PROPERTIES = 0x03,
    OPCODE_GET_ADAPTER_PROPERTY = 0x04,
    OPCODE_SET_ADAPTER_PROPERTY = 0x05,
    OPCODE_GET_REMOTE_DEVICE_PROPERTIES = 0x06,
    OPCODE_GET_REMOTE_DEVICE_PROPERTY = 0x07,
    OPCODE_SET_REMOTE_DEVICE_PROPERTY = 0x08,
    OPCODE_GET_REMOTE_SERVICE_RECORD = 0x09,
    OPCODE_GET_REMOTE_SERVICES = 0x0a,
    OPCODE_START_DISCOVERY = 0x0b,
    OPCODE_CANCEL_DISCOVERY = 0x0c,
    OPCODE_CREATE_BOND = 0x0d,
    OPCODE_REMOVE_BOND = 0x0e,
    OPCODE_CANCEL_BOND = 0x0f,
    OPCODE_PIN_REPLY = 0x10,
    OPCODE_SSP_REPLY = 0x11,
    OPCODE_DUT_MODE_CONFIGURE = 0x12,
    OPCODE_DUT_MODE_SEND = 0x13,
    OPCODE_LE_TEST_MODE = 0x14,
    OPCODE_ADAPTER_STATE_CHANGED_NTF = 0x81,
    OPCODE_ADAPTER_PROPERTIES_NTF = 0x82,
    OPCODE_REMOTE_DEVICE_PROPERTIES_NTF = 0x83,
    OPCODE_DEVICE_FOUND_NTF = 0x84,
    OPCODE_DISCOVERY_STATE_CHANGED_NTF = 0x85,
    OPCODE_PIN_REQUEST_NTF = 0x86,
    OPCODE_SSP_REQUEST_NTF = 0x87,
    OPCODE_BOND_STATE_CHANGED_NTF = 0x88,
    OPCODE_ACL_STATE_CHANGED_NTF = 0x89,
    OPCODE_DUT_MODE_RECV_NTF = 0x8a,
    OPCODE_LE_TEST_MODE_NTF = 0x8b
  };

  static const int MAX_NUM_CLIENTS;

  virtual nsresult Send(DaemonSocketPDU* aPDU,
                        DaemonSocketResultHandler* aRes) = 0;

  void SetNotificationHandler(
    BluetoothNotificationHandler* aNotificationHandler);

  BluetoothNotificationHandler* GetNotificationHandler();

  //
  // Commands
  //

  nsresult EnableCmd(BluetoothResultHandler* aRes);

  nsresult DisableCmd(BluetoothResultHandler* aRes);

  nsresult GetAdapterPropertiesCmd(BluetoothResultHandler* aRes);

  nsresult GetAdapterPropertyCmd(BluetoothPropertyType aType,
                                 BluetoothResultHandler* aRes);

  nsresult SetAdapterPropertyCmd(const BluetoothNamedValue& aProperty,
                                 BluetoothResultHandler* aRes);

  nsresult GetRemoteDevicePropertiesCmd(const BluetoothAddress& aRemoteAddr,
                                        BluetoothResultHandler* aRes);

  nsresult GetRemoteDevicePropertyCmd(const BluetoothAddress& aRemoteAddr,
                                      BluetoothPropertyType aType,
                                      BluetoothResultHandler* aRes);

  nsresult SetRemoteDevicePropertyCmd(const BluetoothAddress& aRemoteAddr,
                                      const BluetoothNamedValue& aProperty,
                                      BluetoothResultHandler* aRes);

  nsresult GetRemoteServiceRecordCmd(const BluetoothAddress& aRemoteAddr,
                                     const BluetoothUuid& aUuid,
                                     BluetoothResultHandler* aRes);

  nsresult GetRemoteServicesCmd(const BluetoothAddress& aRemoteAddr,
                                BluetoothResultHandler* aRes);

  nsresult StartDiscoveryCmd(BluetoothResultHandler* aRes);

  nsresult CancelDiscoveryCmd(BluetoothResultHandler* aRes);

  nsresult CreateBondCmd(const BluetoothAddress& aBdAddr,
                         BluetoothTransport aTransport,
                         BluetoothResultHandler* aRes);

  nsresult RemoveBondCmd(const BluetoothAddress& aBdAddr,
                         BluetoothResultHandler* aRes);

  nsresult CancelBondCmd(const BluetoothAddress& aBdAddr,
                         BluetoothResultHandler* aRes);

  nsresult PinReplyCmd(const BluetoothAddress& aBdAddr, bool aAccept,
                       const BluetoothPinCode& aPinCode,
                       BluetoothResultHandler* aRes);

  nsresult SspReplyCmd(const BluetoothAddress& aBdAddr,
                       BluetoothSspVariant aVariant,
                       bool aAccept, uint32_t aPasskey,
                       BluetoothResultHandler* aRes);

  nsresult DutModeConfigureCmd(bool aEnable, BluetoothResultHandler* aRes);

  nsresult DutModeSendCmd(uint16_t aOpcode, uint8_t* aBuf, uint8_t aLen,
                          BluetoothResultHandler* aRes);

  nsresult LeTestModeCmd(uint16_t aOpcode, uint8_t* aBuf, uint8_t aLen,
                         BluetoothResultHandler* aRes);

protected:
  void HandleSvc(const DaemonSocketPDUHeader& aHeader,
                 DaemonSocketPDU& aPDU, DaemonSocketResultHandler* aRes);

private:

  //
  // Responses
  //

  typedef mozilla::ipc::DaemonResultRunnable0<
    BluetoothResultHandler, void>
    ResultRunnable;

  typedef mozilla::ipc::DaemonResultRunnable1<
    BluetoothResultHandler, void, BluetoothStatus, BluetoothStatus>
    ErrorRunnable;

  void ErrorRsp(const DaemonSocketPDUHeader& aHeader,
                DaemonSocketPDU& aPDU,
                BluetoothResultHandler* aRes);

  void EnableRsp(const DaemonSocketPDUHeader& aHeader,
                 DaemonSocketPDU& aPDU,
                 BluetoothResultHandler* aRes);

  void DisableRsp(const DaemonSocketPDUHeader& aHeader,
                  DaemonSocketPDU& aPDU,
                  BluetoothResultHandler* aRes);

  void GetAdapterPropertiesRsp(const DaemonSocketPDUHeader& aHeader,
                               DaemonSocketPDU& aPDU,
                               BluetoothResultHandler* aRes);

  void GetAdapterPropertyRsp(const DaemonSocketPDUHeader& aHeader,
                             DaemonSocketPDU& aPDU,
                             BluetoothResultHandler* aRes);

  void SetAdapterPropertyRsp(const DaemonSocketPDUHeader& aHeader,
                             DaemonSocketPDU& aPDU,
                             BluetoothResultHandler* aRes);

  void GetRemoteDevicePropertiesRsp(const DaemonSocketPDUHeader& aHeader,
                                    DaemonSocketPDU& aPDU,
                                    BluetoothResultHandler* aRes);

  void
  GetRemoteDevicePropertyRsp(const DaemonSocketPDUHeader& aHeader,
                             DaemonSocketPDU& aPDU,
                             BluetoothResultHandler* aRes);

  void SetRemoteDevicePropertyRsp(const DaemonSocketPDUHeader& aHeader,
                                  DaemonSocketPDU& aPDU,
                                  BluetoothResultHandler* aRes);
  void GetRemoteServiceRecordRsp(const DaemonSocketPDUHeader& aHeader,
                                 DaemonSocketPDU& aPDU,
                                 BluetoothResultHandler* aRes);
  void GetRemoteServicesRsp(const DaemonSocketPDUHeader& aHeader,
                            DaemonSocketPDU& aPDU,
                            BluetoothResultHandler* aRes);

  void StartDiscoveryRsp(const DaemonSocketPDUHeader& aHeader,
                         DaemonSocketPDU& aPDU,
                         BluetoothResultHandler* aRes);
  void CancelDiscoveryRsp(const DaemonSocketPDUHeader& aHeader,
                          DaemonSocketPDU& aPDU,
                          BluetoothResultHandler* aRes);

  void CreateBondRsp(const DaemonSocketPDUHeader& aHeader,
                     DaemonSocketPDU& aPDU,
                     BluetoothResultHandler* aRes);
  void RemoveBondRsp(const DaemonSocketPDUHeader& aHeader,
                     DaemonSocketPDU& aPDU,
                     BluetoothResultHandler* aRes);
  void CancelBondRsp(const DaemonSocketPDUHeader& aHeader,
                     DaemonSocketPDU& aPDU,
                     BluetoothResultHandler* aRes);

  void PinReplyRsp(const DaemonSocketPDUHeader& aHeader,
                   DaemonSocketPDU& aPDU,
                   BluetoothResultHandler* aRes);
  void SspReplyRsp(const DaemonSocketPDUHeader& aHeader,
                   DaemonSocketPDU& aPDU,
                   BluetoothResultHandler* aRes);

  void DutModeConfigureRsp(const DaemonSocketPDUHeader& aHeader,
                           DaemonSocketPDU& aPDU,
                           BluetoothResultHandler* aRes);

  void DutModeSendRsp(const DaemonSocketPDUHeader& aHeader,
                      DaemonSocketPDU& aPDU,
                      BluetoothResultHandler* aRes);

  void LeTestModeRsp(const DaemonSocketPDUHeader& aHeader,
                     DaemonSocketPDU& aPDU,
                     BluetoothResultHandler* aRes);

  void HandleRsp(const DaemonSocketPDUHeader& aHeader,
                 DaemonSocketPDU& aPDU, DaemonSocketResultHandler* aRes);

  //
  // Notifications
  //

  class NotificationHandlerWrapper;

  typedef mozilla::ipc::DaemonNotificationRunnable1<
    NotificationHandlerWrapper, void, bool>
    AdapterStateChangedNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable3<
    NotificationHandlerWrapper, void, BluetoothStatus, int,
    nsAutoArrayPtr<BluetoothProperty>, BluetoothStatus, int,
    const BluetoothProperty*>
    AdapterPropertiesNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable4<
    NotificationHandlerWrapper, void, BluetoothStatus, BluetoothAddress, int,
    nsAutoArrayPtr<BluetoothProperty>, BluetoothStatus,
    const BluetoothAddress&, int, const BluetoothProperty*>
    RemoteDevicePropertiesNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable2<
    NotificationHandlerWrapper, void, int, nsAutoArrayPtr<BluetoothProperty>,
    int, const BluetoothProperty*>
    DeviceFoundNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable1<
    NotificationHandlerWrapper, void, bool>
    DiscoveryStateChangedNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable3<
    NotificationHandlerWrapper, void,
    BluetoothAddress, BluetoothRemoteName, uint32_t,
    const BluetoothAddress&, const BluetoothRemoteName&>
    PinRequestNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable5<
    NotificationHandlerWrapper, void,
    BluetoothAddress, BluetoothRemoteName, uint32_t, BluetoothSspVariant,
    uint32_t,
    const BluetoothAddress&, const BluetoothRemoteName&>
    SspRequestNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable3<
    NotificationHandlerWrapper, void, BluetoothStatus, BluetoothAddress,
    BluetoothBondState, BluetoothStatus, const BluetoothAddress&>
    BondStateChangedNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable3<
    NotificationHandlerWrapper, void,
    BluetoothStatus, BluetoothAddress, BluetoothAclState,
    BluetoothStatus, const BluetoothAddress&>
    AclStateChangedNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable3<
    NotificationHandlerWrapper, void, uint16_t, nsAutoArrayPtr<uint8_t>,
    uint8_t, uint16_t, const uint8_t*>
    DutModeRecvNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable2<
    NotificationHandlerWrapper, void, BluetoothStatus, uint16_t>
    LeTestModeNotification;

  class AclStateChangedInitOp;
  class AdapterPropertiesInitOp;
  class BondStateChangedInitOp;
  class DeviceFoundInitOp;
  class DutModeRecvInitOp;
  class PinRequestInitOp;
  class RemoteDevicePropertiesInitOp;
  class SspRequestInitOp;

  void AdapterStateChangedNtf(const DaemonSocketPDUHeader& aHeader,
                              DaemonSocketPDU& aPDU);

  void AdapterPropertiesNtf(const DaemonSocketPDUHeader& aHeader,
                            DaemonSocketPDU& aPDU);

  void RemoteDevicePropertiesNtf(const DaemonSocketPDUHeader& aHeader,
                                 DaemonSocketPDU& aPDU);

  void DeviceFoundNtf(const DaemonSocketPDUHeader& aHeader,
                      DaemonSocketPDU& aPDU);

  void DiscoveryStateChangedNtf(const DaemonSocketPDUHeader& aHeader,
                                DaemonSocketPDU& aPDU);

  void PinRequestNtf(const DaemonSocketPDUHeader& aHeader,
                     DaemonSocketPDU& aPDU);

  void SspRequestNtf(const DaemonSocketPDUHeader& aHeader,
                     DaemonSocketPDU& aPDU);

  void BondStateChangedNtf(const DaemonSocketPDUHeader& aHeader,
                           DaemonSocketPDU& aPDU);

  void AclStateChangedNtf(const DaemonSocketPDUHeader& aHeader,
                          DaemonSocketPDU& aPDU);

  void DutModeRecvNtf(const DaemonSocketPDUHeader& aHeader,
                      DaemonSocketPDU& aPDU);

  void LeTestModeNtf(const DaemonSocketPDUHeader& aHeader,
                     DaemonSocketPDU& aPDU);

  void HandleNtf(const DaemonSocketPDUHeader& aHeader,
                 DaemonSocketPDU& aPDU, DaemonSocketResultHandler* aRes);

  static BluetoothNotificationHandler* sNotificationHandler;
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_bluedroid_BluetoothDaemonCoreInterface_h
