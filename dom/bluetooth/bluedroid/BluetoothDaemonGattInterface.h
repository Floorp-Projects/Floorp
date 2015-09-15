/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluedroid_BluetoothDaemonGattInterface_h
#define mozilla_dom_bluetooth_bluedroid_BluetoothDaemonGattInterface_h

#include "BluetoothDaemonHelpers.h"
#include "BluetoothInterface.h"
#include "mozilla/ipc/DaemonRunnables.h"

BEGIN_BLUETOOTH_NAMESPACE

using mozilla::ipc::DaemonSocketPDU;
using mozilla::ipc::DaemonSocketPDUHeader;

class BluetoothSetupResultHandler;

class BluetoothDaemonGattModule
{
public:
  enum {
    SERVICE_ID = 0x09
  };

  enum {
    OPCODE_ERROR = 0x00,
    OPCODE_CLIENT_REGISTER = 0x01,
    OPCODE_CLIENT_UNREGISTER = 0x02,
    OPCODE_CLIENT_SCAN = 0x03,
    OPCODE_CLIENT_CONNECT = 0x04,
    OPCODE_CLIENT_DISCONNECT = 0x05,
    OPCODE_CLIENT_LISTEN =0x06,
    OPCODE_CLIENT_REFRESH = 0x07,
    OPCODE_CLIENT_SEARCH_SERVICE = 0x08,
    OPCODE_CLIENT_GET_INCLUDED_SERVICE = 0x09,
    OPCODE_CLIENT_GET_CHARACTERISTIC = 0x0a,
    OPCODE_CLIENT_GET_DESCRIPTOR = 0x0b,
    OPCODE_CLIENT_READ_CHARACTERISTIC = 0x0c,
    OPCODE_CLIENT_WRITE_CHARACTERISTIC = 0x0d,
    OPCODE_CLIENT_READ_DESCRIPTOR = 0x0e,
    OPCODE_CLIENT_WRITE_DESCRIPTOR = 0x0f,
    OPCODE_CLIENT_EXECUTE_WRITE = 0x10,
    OPCODE_CLIENT_REGISTER_NOTIFICATION = 0x11,
    OPCODE_CLIENT_DEREGISTER_NOTIFICATION = 0x12,
    OPCODE_CLIENT_READ_REMOTE_RSSI = 0x13,
    OPCODE_CLIENT_GET_DEVICE_TYPE = 0x14,
    OPCODE_CLIENT_SET_ADV_DATA = 0x15,
    OPCODE_CLIENT_TEST_COMMAND = 0x16,
    OPCODE_SERVER_REGISTER = 0x17,
    OPCODE_SERVER_UNREGISTER = 0x18,
    OPCODE_SERVER_CONNECT_PERIPHERAL = 0x19,
    OPCODE_SERVER_DISCONNECT_PERIPHERAL = 0x1a,
    OPCODE_SERVER_ADD_SERVICE = 0x1b,
    OPCODE_SERVER_ADD_INCLUDED_SERVICE = 0x1c,
    OPCODE_SERVER_ADD_CHARACTERISTIC = 0x1d,
    OPCODE_SERVER_ADD_DESCRIPTOR = 0x1e,
    OPCODE_SERVER_START_SERVICE = 0x1f,
    OPCODE_SERVER_STOP_SERVICE = 0x20,
    OPCODE_SERVER_DELETE_SERVICE = 0x21,
    OPCODE_SERVER_SEND_INDICATION = 0x22,
    OPCODE_SERVER_SEND_RESPONSE = 0x23
    // TODO: Add L support
  };

  static const int MAX_NUM_CLIENTS;

  virtual nsresult Send(DaemonSocketPDU* aPDU, void* aUserData) = 0;

  virtual nsresult RegisterModule(uint8_t aId, uint8_t aMode,
                                  uint32_t aMaxNumClients,
                                  BluetoothSetupResultHandler* aRes) = 0;

  virtual nsresult UnregisterModule(uint8_t aId,
                                    BluetoothSetupResultHandler* aRes) = 0;

  void SetNotificationHandler(
    BluetoothGattNotificationHandler* aNotificationHandler);

  //
  // Commands
  //

  /* Register / Unregister */
  nsresult ClientRegisterCmd(const BluetoothUuid& aUuid,
                             BluetoothGattResultHandler* aRes);

  nsresult ClientUnregisterCmd(int aClientIf,
                               BluetoothGattResultHandler* aRes);

  /* Start / Stop LE Scan */
  nsresult ClientScanCmd(int aClientIf, bool aStart,
                         BluetoothGattResultHandler* aRes);

  /* Connect / Disconnect */
  nsresult ClientConnectCmd(int aClientIf,
                            const nsAString& aBdAddr,
                            bool aIsDirect, /* auto connect */
                            BluetoothTransport aTransport,
                            BluetoothGattResultHandler* aRes);

  nsresult ClientDisconnectCmd(int aClientIf,
                               const nsAString& aBdAddr,
                               int aConnId,
                               BluetoothGattResultHandler* aRes);

  /* Start / Stop advertisements to listen for incoming connections */
  nsresult ClientListenCmd(int aClientIf,
                           bool aIsStart,
                           BluetoothGattResultHandler* aRes);

  /* Clear the attribute cache for a given device*/
  nsresult ClientRefreshCmd(int aClientIf,
                            const nsAString& aBdAddr,
                            BluetoothGattResultHandler* aRes);

  /* Enumerate Attributes */
  nsresult ClientSearchServiceCmd(int aConnId,
                                  bool aFiltered,
                                  const BluetoothUuid& aUuid,
                                  BluetoothGattResultHandler* aRes);

  nsresult ClientGetIncludedServiceCmd(
    int aConnId,
    const BluetoothGattServiceId& aServiceId,
    bool aContinuation,
    const BluetoothGattServiceId& aStartServiceId,
    BluetoothGattResultHandler* aRes);

  nsresult ClientGetCharacteristicCmd(int aConnId,
                                      const BluetoothGattServiceId& aServiceId,
                                      bool aContinuation,
                                      const BluetoothGattId& aStartCharId,
                                      BluetoothGattResultHandler* aRes);

  nsresult ClientGetDescriptorCmd(int aConnId,
                                  const BluetoothGattServiceId& aServiceId,
                                  const BluetoothGattId& aCharId,
                                  bool aContinuation,
                                  const BluetoothGattId& aDescriptorId,
                                  BluetoothGattResultHandler* aRes);

  /* Read / Write An Attribute */
  nsresult ClientReadCharacteristicCmd(
    int aConnId,
    const BluetoothGattServiceId& aServiceId,
    const BluetoothGattId& aCharId,
    BluetoothGattAuthReq aAuthReq,
    BluetoothGattResultHandler* aRes);

  nsresult ClientWriteCharacteristicCmd(
    int aConnId,
    const BluetoothGattServiceId& aServiceId,
    const BluetoothGattId& aCharId,
    BluetoothGattWriteType aWriteType,
    int aLength,
    BluetoothGattAuthReq aAuthReq,
    char* aValue,
    BluetoothGattResultHandler* aRes);

  nsresult ClientReadDescriptorCmd(int aConnId,
                                   const BluetoothGattServiceId& aServiceId,
                                   const BluetoothGattId& aCharId,
                                   const BluetoothGattId& aDescriptorId,
                                   BluetoothGattAuthReq aAuthReq,
                                   BluetoothGattResultHandler* aRes);

  nsresult ClientWriteDescriptorCmd(int aConnId,
                                    const BluetoothGattServiceId& aServiceId,
                                    const BluetoothGattId& aCharId,
                                    const BluetoothGattId& aDescriptorId,
                                    BluetoothGattWriteType aWriteType,
                                    int aLength,
                                    BluetoothGattAuthReq aAuthReq,
                                    char* aValue,
                                    BluetoothGattResultHandler* aRes);

  /* Execute / Abort Prepared Write*/
  nsresult ClientExecuteWriteCmd(int aConnId,
                                 int aIsExecute,
                                 BluetoothGattResultHandler* aRes);

  /* Register / Deregister Characteristic Notifications or Indications */
  nsresult ClientRegisterNotificationCmd(
    int aClientIf,
    const nsAString& aBdAddr,
    const BluetoothGattServiceId& aServiceId,
    const BluetoothGattId& aCharId,
    BluetoothGattResultHandler* aRes);

  nsresult ClientDeregisterNotificationCmd(
    int aClientIf,
    const nsAString& aBdAddr,
    const BluetoothGattServiceId& aServiceId,
    const BluetoothGattId& aCharId,
    BluetoothGattResultHandler* aRes);

  nsresult ClientReadRemoteRssiCmd(int aClientIf,
                                   const nsAString& aBdAddr,
                                   BluetoothGattResultHandler* aRes);

  nsresult ClientGetDeviceTypeCmd(const nsAString& aBdAddr,
                                  BluetoothGattResultHandler* aRes);

  /* Set advertising data or scan response data */
  nsresult ClientSetAdvDataCmd(int aServerIf,
                               bool aIsScanRsp,
                               bool aIsNameIncluded,
                               bool aIsTxPowerIncluded,
                               int aMinInterval,
                               int aMaxInterval,
                               int aApperance,
                               uint16_t aManufacturerLen,
                               char* aManufacturerData,
                               uint16_t aServiceDataLen,
                               char* aServiceData,
                               uint16_t aServiceUUIDLen,
                               char* aServiceUUID,
                               BluetoothGattResultHandler* aRes);

  nsresult ClientTestCommandCmd(int aCommand,
                                const BluetoothGattTestParam& aTestParam,
                                BluetoothGattResultHandler* aRes);

  /* Register / Unregister */
  nsresult ServerRegisterCmd(const BluetoothUuid& aUuid,
                             BluetoothGattResultHandler* aRes);

  nsresult ServerUnregisterCmd(int aServerIf,
                               BluetoothGattResultHandler* aRes);

  /* Connect / Disconnect */
  nsresult ServerConnectPeripheralCmd(int aServerIf,
                                      const nsAString& aBdAddr,
                                      bool aIsDirect,
                                      BluetoothTransport aTransport,
                                      BluetoothGattResultHandler* aRes);

  nsresult ServerDisconnectPeripheralCmd(
    int aServerIf,
    const nsAString& aBdAddr,
    int aConnId,
    BluetoothGattResultHandler* aRes);

  /* Add a services / a characteristic / a descriptor */
  nsresult ServerAddServiceCmd(int aServerIf,
                               const BluetoothGattServiceId& aServiceId,
                               int aNumHandles,
                               BluetoothGattResultHandler* aRes);

  nsresult ServerAddIncludedServiceCmd(int aServerIf,
                                       int aServiceHandle,
                                       int aIncludedServiceHandle,
                                       BluetoothGattResultHandler* aRes);

  nsresult ServerAddCharacteristicCmd(int aServerIf,
                                      int aServiceHandle,
                                      const BluetoothUuid& aUuid,
                                      BluetoothGattCharProp aProperties,
                                      BluetoothGattAttrPerm aPermissions,
                                      BluetoothGattResultHandler* aRes);

  nsresult ServerAddDescriptorCmd(int aServerIf,
                                  int aServiceHandle,
                                  const BluetoothUuid& aUuid,
                                  BluetoothGattAttrPerm aPermissions,
                                  BluetoothGattResultHandler* aRes);

  /* Start / Stop / Delete a service */
  nsresult ServerStartServiceCmd(int aServerIf,
                                 int aServiceHandle,
                                 BluetoothTransport aTransport,
                                 BluetoothGattResultHandler* aRes);

  nsresult ServerStopServiceCmd(int aServerIf,
                                int aServiceHandle,
                                BluetoothGattResultHandler* aRes);

  nsresult ServerDeleteServiceCmd(int aServerIf,
                                  int aServiceHandle,
                                  BluetoothGattResultHandler* aRes);

  /* Send an indication or a notification */
  nsresult ServerSendIndicationCmd(int aServerIf,
                                   int aAttributeHandle,
                                   int aConnId,
                                   int aLength,
                                   bool aConfirm,
                                   uint8_t* aValue,
                                   BluetoothGattResultHandler* aRes);

  /* Send a response for an incoming indication */
  nsresult ServerSendResponseCmd(int aConnId,
                                 int aTransId,
                                 BluetoothGattStatus aStatus,
                                 const BluetoothGattResponse& aResponse,
                                 BluetoothGattResultHandler* aRes);
  // TODO: Add L support

protected:
  nsresult Send(DaemonSocketPDU* aPDU,
                BluetoothGattResultHandler* aRes);

  void HandleSvc(const DaemonSocketPDUHeader& aHeader,
                 DaemonSocketPDU& aPDU, void* aUserData);

  //
  // Responses
  //

  typedef mozilla::ipc::DaemonResultRunnable0<
    BluetoothGattResultHandler, void>
    ResultRunnable;

  typedef mozilla::ipc::DaemonResultRunnable1<
    BluetoothGattResultHandler, void,
    BluetoothTypeOfDevice, BluetoothTypeOfDevice>
    ClientGetDeviceTypeResultRunnable;

  typedef mozilla::ipc::DaemonResultRunnable1<
    BluetoothGattResultHandler, void, BluetoothStatus, BluetoothStatus>
    ErrorRunnable;

  void ErrorRsp(const DaemonSocketPDUHeader& aHeader,
                DaemonSocketPDU& aPDU,
                BluetoothGattResultHandler* aRes);

  void ClientRegisterRsp(const DaemonSocketPDUHeader& aHeader,
                         DaemonSocketPDU& aPDU,
                         BluetoothGattResultHandler* aRes);

  void ClientUnregisterRsp(const DaemonSocketPDUHeader& aHeader,
                           DaemonSocketPDU& aPDU,
                           BluetoothGattResultHandler* aRes);

  void ClientScanRsp(const DaemonSocketPDUHeader& aHeader,
                     DaemonSocketPDU& aPDU,
                     BluetoothGattResultHandler* aRes);

  void ClientConnectRsp(const DaemonSocketPDUHeader& aHeader,
                        DaemonSocketPDU& aPDU,
                        BluetoothGattResultHandler* aRes);

  void ClientDisconnectRsp(const DaemonSocketPDUHeader& aHeader,
                           DaemonSocketPDU& aPDU,
                           BluetoothGattResultHandler* aRes);

  void ClientListenRsp(const DaemonSocketPDUHeader& aHeader,
                       DaemonSocketPDU& aPDU,
                       BluetoothGattResultHandler* aRes);

  void ClientRefreshRsp(const DaemonSocketPDUHeader& aHeader,
                        DaemonSocketPDU& aPDU,
                        BluetoothGattResultHandler* aRes);

  void ClientSearchServiceRsp(const DaemonSocketPDUHeader& aHeader,
                              DaemonSocketPDU& aPDU,
                              BluetoothGattResultHandler* aRes);

  void ClientGetIncludedServiceRsp(const DaemonSocketPDUHeader& aHeader,
                                   DaemonSocketPDU& aPDU,
                                   BluetoothGattResultHandler* aRes);

  void ClientGetCharacteristicRsp(const DaemonSocketPDUHeader& aHeader,
                                  DaemonSocketPDU& aPDU,
                                  BluetoothGattResultHandler* aRes);

  void ClientGetDescriptorRsp(const DaemonSocketPDUHeader& aHeader,
                              DaemonSocketPDU& aPDU,
                              BluetoothGattResultHandler* aRes);

  void ClientReadCharacteristicRsp(const DaemonSocketPDUHeader& aHeader,
                                   DaemonSocketPDU& aPDU,
                                   BluetoothGattResultHandler* aRes);

  void ClientWriteCharacteristicRsp(const DaemonSocketPDUHeader& aHeader,
                                    DaemonSocketPDU& aPDU,
                                    BluetoothGattResultHandler* aRes);

  void ClientReadDescriptorRsp(const DaemonSocketPDUHeader& aHeader,
                               DaemonSocketPDU& aPDU,
                               BluetoothGattResultHandler* aRes);

  void ClientWriteDescriptorRsp(const DaemonSocketPDUHeader& aHeader,
                                DaemonSocketPDU& aPDU,
                                BluetoothGattResultHandler* aRes);

  void ClientExecuteWriteRsp(const DaemonSocketPDUHeader& aHeader,
                             DaemonSocketPDU& aPDU,
                             BluetoothGattResultHandler* aRes);

  void ClientRegisterNotificationRsp(const DaemonSocketPDUHeader& aHeader,
                                     DaemonSocketPDU& aPDU,
                                     BluetoothGattResultHandler* aRes);

  void ClientDeregisterNotificationRsp(const DaemonSocketPDUHeader& aHeader,
                                       DaemonSocketPDU& aPDU,
                                       BluetoothGattResultHandler* aRes);

  void ClientReadRemoteRssiRsp(const DaemonSocketPDUHeader& aHeader,
                               DaemonSocketPDU& aPDU,
                               BluetoothGattResultHandler* aRes);

  void ClientGetDeviceTypeRsp(const DaemonSocketPDUHeader& aHeader,
                              DaemonSocketPDU& aPDU,
                              BluetoothGattResultHandler* aRes);

  void ClientSetAdvDataRsp(const DaemonSocketPDUHeader& aHeader,
                           DaemonSocketPDU& aPDU,
                           BluetoothGattResultHandler* aRes);

  void ClientTestCommandRsp(const DaemonSocketPDUHeader& aHeader,
                            DaemonSocketPDU& aPDU,
                            BluetoothGattResultHandler* aRes);

  void ServerRegisterRsp(const DaemonSocketPDUHeader& aHeader,
                         DaemonSocketPDU& aPDU,
                         BluetoothGattResultHandler* aRes);

  void ServerUnregisterRsp(const DaemonSocketPDUHeader& aHeader,
                           DaemonSocketPDU& aPDU,
                           BluetoothGattResultHandler* aRes);

  void ServerConnectPeripheralRsp(const DaemonSocketPDUHeader& aHeader,
                                  DaemonSocketPDU& aPDU,
                                  BluetoothGattResultHandler* aRes);

  void ServerDisconnectPeripheralRsp(const DaemonSocketPDUHeader& aHeader,
                                     DaemonSocketPDU& aPDU,
                                     BluetoothGattResultHandler* aRes);

  void ServerAddServiceRsp(const DaemonSocketPDUHeader& aHeader,
                           DaemonSocketPDU& aPDU,
                           BluetoothGattResultHandler* aRes);

  void ServerAddIncludedServiceRsp(const DaemonSocketPDUHeader& aHeader,
                                   DaemonSocketPDU& aPDU,
                                   BluetoothGattResultHandler* aRes);

  void ServerAddCharacteristicRsp(const DaemonSocketPDUHeader& aHeader,
                                  DaemonSocketPDU& aPDU,
                                  BluetoothGattResultHandler* aRes);

  void ServerAddDescriptorRsp(const DaemonSocketPDUHeader& aHeader,
                              DaemonSocketPDU& aPDU,
                              BluetoothGattResultHandler* aRes);

  void ServerStartServiceRsp(const DaemonSocketPDUHeader& aHeader,
                             DaemonSocketPDU& aPDU,
                             BluetoothGattResultHandler* aRes);

  void ServerStopServiceRsp(const DaemonSocketPDUHeader& aHeader,
                            DaemonSocketPDU& aPDU,
                            BluetoothGattResultHandler* aRes);

  void ServerDeleteServiceRsp(const DaemonSocketPDUHeader& aHeader,
                              DaemonSocketPDU& aPDU,
                              BluetoothGattResultHandler* aRes);

  void ServerSendIndicationRsp(const DaemonSocketPDUHeader& aHeader,
                               DaemonSocketPDU& aPDU,
                               BluetoothGattResultHandler* aRes);

  void ServerSendResponseRsp(const DaemonSocketPDUHeader& aHeader,
                             DaemonSocketPDU& aPDU,
                             BluetoothGattResultHandler* aRes);

  // TODO: Add L support

  void HandleRsp(const DaemonSocketPDUHeader& aHeader,
                 DaemonSocketPDU& aPDU,
                 void* aUserData);

  //
  // Notifications
  //

  class NotificationHandlerWrapper;

  typedef mozilla::ipc::DaemonNotificationRunnable3<
    NotificationHandlerWrapper, void,
    BluetoothGattStatus, int, BluetoothUuid,
    BluetoothGattStatus, int, const BluetoothUuid&>
    ClientRegisterNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable3<
    NotificationHandlerWrapper, void,
    nsString, int, BluetoothGattAdvData,
    const nsAString&, int, const BluetoothGattAdvData&>
    ClientScanResultNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable4<
    NotificationHandlerWrapper, void,
    int, BluetoothGattStatus, int, nsString,
    int, BluetoothGattStatus, int, const nsAString&>
    ClientConnectNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable4<
    NotificationHandlerWrapper, void,
    int, BluetoothGattStatus, int, nsString,
    int, BluetoothGattStatus, int, const nsAString&>
    ClientDisconnectNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable2<
    NotificationHandlerWrapper, void,
    int, BluetoothGattStatus>
    ClientSearchCompleteNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable2<
    NotificationHandlerWrapper, void,
    int, BluetoothGattServiceId,
    int, const BluetoothGattServiceId&>
    ClientSearchResultNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable5<
    NotificationHandlerWrapper, void,
    int, BluetoothGattStatus, BluetoothGattServiceId,
    BluetoothGattId, BluetoothGattCharProp,
    int, BluetoothGattStatus, const BluetoothGattServiceId&,
    const BluetoothGattId&, const BluetoothGattCharProp&>
    ClientGetCharacteristicNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable5<
    NotificationHandlerWrapper, void,
    int, BluetoothGattStatus, BluetoothGattServiceId,
    BluetoothGattId, BluetoothGattId,
    int, BluetoothGattStatus, const BluetoothGattServiceId&,
    const BluetoothGattId&, const BluetoothGattId&>
    ClientGetDescriptorNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable4<
    NotificationHandlerWrapper, void,
    int, BluetoothGattStatus, BluetoothGattServiceId, BluetoothGattServiceId,
    int, BluetoothGattStatus, const BluetoothGattServiceId&,
    const BluetoothGattServiceId&>
    ClientGetIncludedServiceNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable5<
    NotificationHandlerWrapper, void,
    int, int, BluetoothGattStatus,
    BluetoothGattServiceId, BluetoothGattId,
    int, int, BluetoothGattStatus,
    const BluetoothGattServiceId&, const BluetoothGattId&>
    ClientRegisterNotificationNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable2<
    NotificationHandlerWrapper, void,
    int, BluetoothGattNotifyParam,
    int, const BluetoothGattNotifyParam&>
    ClientNotifyNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable3<
    NotificationHandlerWrapper, void,
    int, BluetoothGattStatus, BluetoothGattReadParam,
    int, BluetoothGattStatus, const BluetoothGattReadParam&>
    ClientReadCharacteristicNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable3<
    NotificationHandlerWrapper, void,
    int, BluetoothGattStatus, BluetoothGattWriteParam,
    int, BluetoothGattStatus, const BluetoothGattWriteParam&>
    ClientWriteCharacteristicNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable3<
    NotificationHandlerWrapper, void,
    int, BluetoothGattStatus, BluetoothGattReadParam,
    int, BluetoothGattStatus, const BluetoothGattReadParam&>
    ClientReadDescriptorNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable3<
    NotificationHandlerWrapper, void,
    int, BluetoothGattStatus, BluetoothGattWriteParam,
    int, BluetoothGattStatus, const BluetoothGattWriteParam&>
    ClientWriteDescriptorNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable2<
    NotificationHandlerWrapper, void,
    int, BluetoothGattStatus>
    ClientExecuteWriteNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable4<
    NotificationHandlerWrapper, void,
    int, nsString, int, BluetoothGattStatus,
    int, const nsAString&, int, BluetoothGattStatus>
    ClientReadRemoteRssiNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable2<
    NotificationHandlerWrapper, void,
    BluetoothGattStatus, int>
    ClientListenNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable3<
    NotificationHandlerWrapper, void,
    BluetoothGattStatus, int, BluetoothUuid,
    BluetoothGattStatus, int, const BluetoothUuid&>
    ServerRegisterNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable4<
    NotificationHandlerWrapper, void,
    int, int, bool, nsString,
    int, int, bool, const nsAString&>
    ServerConnectionNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable4<
    NotificationHandlerWrapper, void,
    BluetoothGattStatus, int, BluetoothGattServiceId, int,
    BluetoothGattStatus, int, const BluetoothGattServiceId&, int>
    ServerServiceAddedNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable4<
    NotificationHandlerWrapper, void,
    BluetoothGattStatus, int, int, int>
    ServerIncludedServiceAddedNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable5<
    NotificationHandlerWrapper, void,
    BluetoothGattStatus, int, BluetoothUuid, int, int,
    BluetoothGattStatus, int, const BluetoothUuid&, int, int>
    ServerCharacteristicAddedNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable5<
    NotificationHandlerWrapper, void,
    BluetoothGattStatus, int, BluetoothUuid, int, int,
    BluetoothGattStatus, int, const BluetoothUuid&, int, int>
    ServerDescriptorAddedNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable3<
    NotificationHandlerWrapper, void,
    BluetoothGattStatus, int, int>
    ServerServiceStartedNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable3<
    NotificationHandlerWrapper, void,
    BluetoothGattStatus, int, int>
    ServerServiceStoppedNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable3<
    NotificationHandlerWrapper, void,
    BluetoothGattStatus, int, int>
    ServerServiceDeletedNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable6<
    NotificationHandlerWrapper, void,
    int, int, nsString, int, int, bool,
    int, int, const nsAString&, int, int, bool>
    ServerRequestReadNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable9<
    NotificationHandlerWrapper, void,
    int, int, nsString, int, int, int, nsAutoArrayPtr<uint8_t>, bool, bool,
    int, int, const nsAString&, int, int, int, const uint8_t*, bool, bool>
    ServerRequestWriteNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable4<
    NotificationHandlerWrapper, void,
    int, int, nsString, bool,
    int, int, const nsAString&, bool>
    ServerRequestExecuteWriteNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable2<
    NotificationHandlerWrapper, void,
    BluetoothGattStatus, int>
    ServerResponseConfirmationNotification;

  class ClientScanResultInitOp;
  class ClientConnectDisconnectInitOp;
  class ClientReadRemoteRssiInitOp;
  class ClientGetDeviceTypeInitOp;
  class ServerConnectionInitOp;
  class ServerRequestReadInitOp;
  class ServerRequestWriteInitOp;
  class ServerRequestExecuteWriteInitOp;

  void ClientRegisterNtf(const DaemonSocketPDUHeader& aHeader,
                               DaemonSocketPDU& aPDU);

  void ClientScanResultNtf(const DaemonSocketPDUHeader& aHeader,
                           DaemonSocketPDU& aPDU);

  void ClientConnectNtf(const DaemonSocketPDUHeader& aHeader,
                        DaemonSocketPDU& aPDU);

  void ClientDisconnectNtf(const DaemonSocketPDUHeader& aHeader,
                           DaemonSocketPDU& aPDU);

  void ClientSearchCompleteNtf(const DaemonSocketPDUHeader& aHeader,
                               DaemonSocketPDU& aPDU);

  void ClientSearchResultNtf(const DaemonSocketPDUHeader& aHeader,
                             DaemonSocketPDU& aPDU);

  void ClientGetCharacteristicNtf(const DaemonSocketPDUHeader& aHeader,
                                  DaemonSocketPDU& aPDU);

  void ClientGetDescriptorNtf(const DaemonSocketPDUHeader& aHeader,
                              DaemonSocketPDU& aPDU);

  void ClientGetIncludedServiceNtf(const DaemonSocketPDUHeader& aHeader,
                                   DaemonSocketPDU& aPDU);

  void ClientRegisterNotificationNtf(const DaemonSocketPDUHeader& aHeader,
                                     DaemonSocketPDU& aPDU);

  void ClientNotifyNtf(const DaemonSocketPDUHeader& aHeader,
                       DaemonSocketPDU& aPDU);

  void ClientReadCharacteristicNtf(const DaemonSocketPDUHeader& aHeader,
                                   DaemonSocketPDU& aPDU);

  void ClientWriteCharacteristicNtf(const DaemonSocketPDUHeader& aHeader,
                                    DaemonSocketPDU& aPDU);

  void ClientReadDescriptorNtf(const DaemonSocketPDUHeader& aHeader,
                               DaemonSocketPDU& aPDU);

  void ClientWriteDescriptorNtf(const DaemonSocketPDUHeader& aHeader,
                                DaemonSocketPDU& aPDU);

  void ClientExecuteWriteNtf(const DaemonSocketPDUHeader& aHeader,
                             DaemonSocketPDU& aPDU);

  void ClientReadRemoteRssiNtf(const DaemonSocketPDUHeader& aHeader,
                               DaemonSocketPDU& aPDU);

  void ClientListenNtf(const DaemonSocketPDUHeader& aHeader,
                       DaemonSocketPDU& aPDU);

  void ServerRegisterNtf(const DaemonSocketPDUHeader& aHeader,
                         DaemonSocketPDU& aPDU);

  void ServerConnectionNtf(const DaemonSocketPDUHeader& aHeader,
                           DaemonSocketPDU& aPDU);

  void ServerServiceAddedNtf(const DaemonSocketPDUHeader& aHeader,
                             DaemonSocketPDU& aPDU);

  void ServerIncludedServiceAddedNtf(const DaemonSocketPDUHeader& aHeader,
                                     DaemonSocketPDU& aPDU);

  void ServerCharacteristicAddedNtf(const DaemonSocketPDUHeader& aHeader,
                                    DaemonSocketPDU& aPDU);

  void ServerDescriptorAddedNtf(const DaemonSocketPDUHeader& aHeader,
                                DaemonSocketPDU& aPDU);

  void ServerServiceStartedNtf(const DaemonSocketPDUHeader& aHeader,
                               DaemonSocketPDU& aPDU);

  void ServerServiceStoppedNtf(const DaemonSocketPDUHeader& aHeader,
                               DaemonSocketPDU& aPDU);

  void ServerServiceDeletedNtf(const DaemonSocketPDUHeader& aHeader,
                               DaemonSocketPDU& aPDU);

  void ServerRequestReadNtf(const DaemonSocketPDUHeader& aHeader,
                            DaemonSocketPDU& aPDU);

  void ServerRequestWriteNtf(const DaemonSocketPDUHeader& aHeader,
                             DaemonSocketPDU& aPDU);

  void ServerRequestExecuteWriteNtf(const DaemonSocketPDUHeader& aHeader,
                                    DaemonSocketPDU& aPDU);

  void ServerResponseConfirmationNtf(const DaemonSocketPDUHeader& aHeader,
                                     DaemonSocketPDU& aPDU);

  // TODO: Add L support

  void HandleNtf(const DaemonSocketPDUHeader& aHeader,
                 DaemonSocketPDU& aPDU,
                 void* aUserData);

  static BluetoothGattNotificationHandler* sNotificationHandler;
};

class BluetoothDaemonGattInterface final
  : public BluetoothGattInterface
{
  class CleanupResultHandler;
  class InitResultHandler;

public:
  BluetoothDaemonGattInterface(BluetoothDaemonGattModule* aModule);
  ~BluetoothDaemonGattInterface();

  void Init(BluetoothGattNotificationHandler* aNotificationHandler,
            BluetoothGattResultHandler* aRes) override;
  void Cleanup(BluetoothGattResultHandler* aRes) override;

  /* Register / Unregister */
  void RegisterClient(const BluetoothUuid& aUuid,
                      BluetoothGattResultHandler* aRes) override;
  void UnregisterClient(int aClientIf,
                        BluetoothGattResultHandler* aRes) override;

  /* Start / Stop LE Scan */
  void Scan(int aClientIf, bool aStart,
            BluetoothGattResultHandler* aRes) override;

  /* Connect / Disconnect */
  void Connect(int aClientIf,
               const nsAString& aBdAddr,
               bool aIsDirect, /* auto connect */
               BluetoothTransport aTransport,
               BluetoothGattResultHandler* aRes) override;
  void Disconnect(int aClientIf,
                  const nsAString& aBdAddr,
                  int aConnId,
                  BluetoothGattResultHandler* aRes) override;

  /* Start / Stop advertisements to listen for incoming connections */
  void Listen(int aClientIf,
              bool aIsStart,
              BluetoothGattResultHandler* aRes) override;

  /* Clear the attribute cache for a given device*/
  void Refresh(int aClientIf,
               const nsAString& aBdAddr,
               BluetoothGattResultHandler* aRes) override;

  /* Enumerate Attributes */
  void SearchService(int aConnId,
                     bool aSearchAll,
                     const BluetoothUuid& aUuid,
                     BluetoothGattResultHandler* aRes) override;
  void GetIncludedService(int aConnId,
                          const BluetoothGattServiceId& aServiceId,
                          bool aFirst,
                          const BluetoothGattServiceId& aStartServiceId,
                          BluetoothGattResultHandler* aRes) override;
  void GetCharacteristic(int aConnId,
                         const BluetoothGattServiceId& aServiceId,
                         bool aFirst,
                         const BluetoothGattId& aStartCharId,
                         BluetoothGattResultHandler* aRes) override;
  void GetDescriptor(int aConnId,
                     const BluetoothGattServiceId& aServiceId,
                     const BluetoothGattId& aCharId,
                     bool aFirst,
                     const BluetoothGattId& aDescriptorId,
                     BluetoothGattResultHandler* aRes) override;

  /* Read / Write An Attribute */
  void ReadCharacteristic(int aConnId,
                          const BluetoothGattServiceId& aServiceId,
                          const BluetoothGattId& aCharId,
                          BluetoothGattAuthReq aAuthReq,
                          BluetoothGattResultHandler* aRes) override;
  void WriteCharacteristic(int aConnId,
                           const BluetoothGattServiceId& aServiceId,
                           const BluetoothGattId& aCharId,
                           BluetoothGattWriteType aWriteType,
                           BluetoothGattAuthReq aAuthReq,
                           const nsTArray<uint8_t>& aValue,
                           BluetoothGattResultHandler* aRes) override;
  void ReadDescriptor(int aConnId,
                      const BluetoothGattServiceId& aServiceId,
                      const BluetoothGattId& aCharId,
                      const BluetoothGattId& aDescriptorId,
                      BluetoothGattAuthReq aAuthReq,
                      BluetoothGattResultHandler* aRes) override;
  void WriteDescriptor(int aConnId,
                       const BluetoothGattServiceId& aServiceId,
                       const BluetoothGattId& aCharId,
                       const BluetoothGattId& aDescriptorId,
                       BluetoothGattWriteType aWriteType,
                       BluetoothGattAuthReq aAuthReq,
                       const nsTArray<uint8_t>& aValue,
                       BluetoothGattResultHandler* aRes) override;

  /* Execute / Abort Prepared Write*/
  void ExecuteWrite(int aConnId,
                    int aIsExecute,
                    BluetoothGattResultHandler* aRes) override;


  /* Register / Deregister Characteristic Notifications or Indications */
  void RegisterNotification(int aClientIf,
                            const nsAString& aBdAddr,
                            const BluetoothGattServiceId& aServiceId,
                            const BluetoothGattId& aCharId,
                            BluetoothGattResultHandler* aRes) override;
  void DeregisterNotification(int aClientIf,
                              const nsAString& aBdAddr,
                              const BluetoothGattServiceId& aServiceId,
                              const BluetoothGattId& aCharId,
                              BluetoothGattResultHandler* aRes) override;

  void ReadRemoteRssi(int aClientIf,
                      const nsAString& aBdAddr,
                      BluetoothGattResultHandler* aRes) override;

  void GetDeviceType(const nsAString& aBdAddr,
                     BluetoothGattResultHandler* aRes) override;

  /* Set advertising data or scan response data */
  void SetAdvData(int aServerIf,
                  bool aIsScanRsp,
                  bool aIsNameIncluded,
                  bool aIsTxPowerIncluded,
                  int aMinInterval,
                  int aMaxInterval,
                  int aApperance,
                  uint16_t aManufacturerLen, char* aManufacturerData,
                  uint16_t aServiceDataLen, char* aServiceData,
                  uint16_t aServiceUuidLen, char* aServiceUuid,
                  BluetoothGattResultHandler* aRes) override;

  void TestCommand(int aCommand,
                   const BluetoothGattTestParam& aTestParam,
                   BluetoothGattResultHandler* aRes) override;

  /* Register / Unregister */
  void RegisterServer(const BluetoothUuid& aUuid,
                      BluetoothGattResultHandler* aRes) override;
  void UnregisterServer(int aServerIf,
                        BluetoothGattResultHandler* aRes) override;

  /* Connect / Disconnect */
  void ConnectPeripheral(int aServerIf,
                         const nsAString& aBdAddr,
                         bool aIsDirect, /* auto connect */
                         BluetoothTransport aTransport,
                         BluetoothGattResultHandler* aRes) override;
  void DisconnectPeripheral(int aServerIf,
                            const nsAString& aBdAddr,
                            int aConnId,
                            BluetoothGattResultHandler* aRes) override;

  /* Add a services / a characteristic / a descriptor */
  void AddService(int aServerIf,
                  const BluetoothGattServiceId& aServiceId,
                  int aNumHandles,
                  BluetoothGattResultHandler* aRes) override;
  void AddIncludedService(int aServerIf,
                          int aServiceHandle,
                          int aIncludedServiceHandle,
                          BluetoothGattResultHandler* aRes) override;
  void AddCharacteristic(int aServerIf,
                         int aServiceHandle,
                         const BluetoothUuid& aUuid,
                         BluetoothGattCharProp aProperties,
                         BluetoothGattAttrPerm aPermissions,
                         BluetoothGattResultHandler* aRes) override;
  void AddDescriptor(int aServerIf,
                     int aServiceHandle,
                     const BluetoothUuid& aUuid,
                     BluetoothGattAttrPerm aPermissions,
                     BluetoothGattResultHandler* aRes) override;

  /* Start / Stop / Delete a service */
  void StartService(int aServerIf,
                    int aServiceHandle,
                    BluetoothTransport aTransport,
                    BluetoothGattResultHandler* aRes) override;
  void StopService(int aServerIf,
                   int aServiceHandle,
                   BluetoothGattResultHandler* aRes) override;
  void DeleteService(int aServerIf,
                     int aServiceHandle,
                     BluetoothGattResultHandler* aRes) override;

  /* Send an indication or a notification */
  void SendIndication(
    int aServerIf,
    int aAttributeHandle,
    int aConnId,
    const nsTArray<uint8_t>& aValue,
    bool aConfirm, /* true: indication, false: notification */
    BluetoothGattResultHandler* aRes) override;

  /* Send a response for an incoming indication */
  void SendResponse(int aConnId,
                    int aTransId,
                    BluetoothGattStatus aStatus,
                    const BluetoothGattResponse& aResponse,
                    BluetoothGattResultHandler* aRes) override;

private:
  void DispatchError(BluetoothGattResultHandler* aRes,
                     BluetoothStatus aStatus);
  void DispatchError(BluetoothGattResultHandler* aRes, nsresult aRv);

  BluetoothDaemonGattModule* mModule;
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_bluedroid_BluetoothDaemonGattInterface_h
