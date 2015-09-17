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

  nsresult GetAdapterPropertyCmd(const nsAString& aName,
                                 BluetoothResultHandler* aRes);

  nsresult SetAdapterPropertyCmd(const BluetoothNamedValue& aProperty,
                                 BluetoothResultHandler* aRes);

  nsresult GetRemoteDevicePropertiesCmd(const nsAString& aRemoteAddr,
                                        BluetoothResultHandler* aRes);

  nsresult GetRemoteDevicePropertyCmd(const nsAString& aRemoteAddr,
                                      const nsAString& aName,
                                      BluetoothResultHandler* aRes);

  nsresult SetRemoteDevicePropertyCmd(const nsAString& aRemoteAddr,
                                      const BluetoothNamedValue& aProperty,
                                      BluetoothResultHandler* aRes);

  nsresult GetRemoteServiceRecordCmd(const nsAString& aRemoteAddr,
                                     const uint8_t aUuid[16],
                                     BluetoothResultHandler* aRes);

  nsresult GetRemoteServicesCmd(const nsAString& aRemoteAddr,
                                BluetoothResultHandler* aRes);

  nsresult StartDiscoveryCmd(BluetoothResultHandler* aRes);

  nsresult CancelDiscoveryCmd(BluetoothResultHandler* aRes);

  nsresult CreateBondCmd(const nsAString& aBdAddr,
                         BluetoothTransport aTransport,
                         BluetoothResultHandler* aRes);

  nsresult RemoveBondCmd(const nsAString& aBdAddr,
                         BluetoothResultHandler* aRes);

  nsresult CancelBondCmd(const nsAString& aBdAddr,
                         BluetoothResultHandler* aRes);

  nsresult PinReplyCmd(const nsAString& aBdAddr, bool aAccept,
                       const nsAString& aPinCode,
                       BluetoothResultHandler* aRes);

  nsresult SspReplyCmd(const nsAString& aBdAddr, BluetoothSspVariant aVariant,
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
    NotificationHandlerWrapper, void, BluetoothStatus, nsString, int,
    nsAutoArrayPtr<BluetoothProperty>, BluetoothStatus, const nsAString&,
    int, const BluetoothProperty*>
    RemoteDevicePropertiesNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable2<
    NotificationHandlerWrapper, void, int, nsAutoArrayPtr<BluetoothProperty>,
    int, const BluetoothProperty*>
    DeviceFoundNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable1<
    NotificationHandlerWrapper, void, bool>
    DiscoveryStateChangedNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable3<
    NotificationHandlerWrapper, void, nsString, nsString, uint32_t,
    const nsAString&, const nsAString&>
    PinRequestNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable5<
    NotificationHandlerWrapper, void, nsString, nsString, uint32_t,
    BluetoothSspVariant, uint32_t, const nsAString&, const nsAString&>
    SspRequestNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable3<
    NotificationHandlerWrapper, void, BluetoothStatus, nsString,
    BluetoothBondState, BluetoothStatus, const nsAString&>
    BondStateChangedNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable3<
    NotificationHandlerWrapper, void, BluetoothStatus, nsString, bool,
    BluetoothStatus, const nsAString&>
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
