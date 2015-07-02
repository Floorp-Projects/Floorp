/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothdaemongattinterface_h
#define mozilla_dom_bluetooth_bluetoothdaemongattinterface_h

#include "BluetoothDaemonHelpers.h"
#include "BluetoothInterface.h"
#include "BluetoothInterfaceHelpers.h"

BEGIN_BLUETOOTH_NAMESPACE

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
    OPCODE_CLIENT_TEST_COMMAND = 0x16
    // TODO: Add server opcodes
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
                             BluetoothGattClientResultHandler* aRes);

  nsresult ClientUnregisterCmd(int aClientIf,
                               BluetoothGattClientResultHandler* aRes);

  /* Start / Stop LE Scan */
  nsresult ClientScanCmd(int aClientIf, bool aStart,
                         BluetoothGattClientResultHandler* aRes);

  /* Connect / Disconnect */
  nsresult ClientConnectCmd(int aClientIf,
                            const nsAString& aBdAddr,
                            bool aIsDirect, /* auto connect */
                            BluetoothTransport aTransport,
                            BluetoothGattClientResultHandler* aRes);

  nsresult ClientDisconnectCmd(int aClientIf,
                               const nsAString& aBdAddr,
                               int aConnId,
                               BluetoothGattClientResultHandler* aRes);

  /* Start / Stop advertisements to listen for incoming connections */
  nsresult ClientListenCmd(int aClientIf,
                           bool aIsStart,
                           BluetoothGattClientResultHandler* aRes);

  /* Clear the attribute cache for a given device*/
  nsresult ClientRefreshCmd(int aClientIf,
                            const nsAString& aBdAddr,
                            BluetoothGattClientResultHandler* aRes);

  /* Enumerate Attributes */
  nsresult ClientSearchServiceCmd(int aConnId,
                                  bool aFiltered,
                                  const BluetoothUuid& aUuid,
                                  BluetoothGattClientResultHandler* aRes);

  nsresult ClientGetIncludedServiceCmd(
    int aConnId,
    const BluetoothGattServiceId& aServiceId,
    bool aContinuation,
    const BluetoothGattServiceId& aStartServiceId,
    BluetoothGattClientResultHandler* aRes);

  nsresult ClientGetCharacteristicCmd(int aConnId,
                                      const BluetoothGattServiceId& aServiceId,
                                      bool aContinuation,
                                      const BluetoothGattId& aStartCharId,
                                      BluetoothGattClientResultHandler* aRes);

  nsresult ClientGetDescriptorCmd(int aConnId,
                                  const BluetoothGattServiceId& aServiceId,
                                  const BluetoothGattId& aCharId,
                                  bool aContinuation,
                                  const BluetoothGattId& aDescriptorId,
                                  BluetoothGattClientResultHandler* aRes);

  /* Read / Write An Attribute */
  nsresult ClientReadCharacteristicCmd(
    int aConnId,
    const BluetoothGattServiceId& aServiceId,
    const BluetoothGattId& aCharId,
    BluetoothGattAuthReq aAuthReq,
    BluetoothGattClientResultHandler* aRes);

  nsresult ClientWriteCharacteristicCmd(
    int aConnId,
    const BluetoothGattServiceId& aServiceId,
    const BluetoothGattId& aCharId,
    BluetoothGattWriteType aWriteType,
    int aLength,
    BluetoothGattAuthReq aAuthReq,
    char* aValue,
    BluetoothGattClientResultHandler* aRes);

  nsresult ClientReadDescriptorCmd(int aConnId,
                                   const BluetoothGattServiceId& aServiceId,
                                   const BluetoothGattId& aCharId,
                                   const BluetoothGattId& aDescriptorId,
                                   BluetoothGattAuthReq aAuthReq,
                                   BluetoothGattClientResultHandler* aRes);

  nsresult ClientWriteDescriptorCmd(int aConnId,
                                    const BluetoothGattServiceId& aServiceId,
                                    const BluetoothGattId& aCharId,
                                    const BluetoothGattId& aDescriptorId,
                                    BluetoothGattWriteType aWriteType,
                                    int aLength,
                                    BluetoothGattAuthReq aAuthReq,
                                    char* aValue,
                                    BluetoothGattClientResultHandler* aRes);

  /* Execute / Abort Prepared Write*/
  nsresult ClientExecuteWriteCmd(int aConnId,
                                 int aIsExecute,
                                 BluetoothGattClientResultHandler* aRes);

  /* Register / Deregister Characteristic Notifications or Indications */
  nsresult ClientRegisterNotificationCmd(
    int aClientIf,
    const nsAString& aBdAddr,
    const BluetoothGattServiceId& aServiceId,
    const BluetoothGattId& aCharId,
    BluetoothGattClientResultHandler* aRes);

  nsresult ClientDeregisterNotificationCmd(
    int aClientIf,
    const nsAString& aBdAddr,
    const BluetoothGattServiceId& aServiceId,
    const BluetoothGattId& aCharId,
    BluetoothGattClientResultHandler* aRes);

  nsresult ClientReadRemoteRssiCmd(int aClientIf,
                                   const nsAString& aBdAddr,
                                   BluetoothGattClientResultHandler* aRes);

  nsresult ClientGetDeviceTypeCmd(const nsAString& aBdAddr,
                                  BluetoothGattClientResultHandler* aRes);

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
                               BluetoothGattClientResultHandler* aRes);

  nsresult ClientTestCommandCmd(int aCommand,
                                const BluetoothGattTestParam& aTestParam,
                                BluetoothGattClientResultHandler* aRes);

  // TODO: Add server commands

protected:
  nsresult Send(DaemonSocketPDU* aPDU,
                BluetoothGattResultHandler* aRes);

  nsresult Send(DaemonSocketPDU* aPDU,
                BluetoothGattClientResultHandler* aRes);

  void HandleSvc(const DaemonSocketPDUHeader& aHeader,
                 DaemonSocketPDU& aPDU, void* aUserData);

  //
  // Responses
  //

  typedef BluetoothResultRunnable0<BluetoothGattClientResultHandler, void>
    ClientResultRunnable;

  typedef BluetoothResultRunnable1<BluetoothGattClientResultHandler, void,
                                   BluetoothTypeOfDevice, BluetoothTypeOfDevice>
    ClientGetDeviceTypeResultRunnable;

  typedef BluetoothResultRunnable0<BluetoothGattResultHandler, void>
    ResultRunnable;

  typedef BluetoothResultRunnable1<BluetoothGattResultHandler, void,
                                   BluetoothStatus, BluetoothStatus>
    ErrorRunnable;

  void ErrorRsp(const DaemonSocketPDUHeader& aHeader,
                DaemonSocketPDU& aPDU,
                BluetoothGattResultHandler* aRes);

  void ClientRegisterRsp(const DaemonSocketPDUHeader& aHeader,
                         DaemonSocketPDU& aPDU,
                         BluetoothGattClientResultHandler* aRes);

  void ClientUnregisterRsp(const DaemonSocketPDUHeader& aHeader,
                           DaemonSocketPDU& aPDU,
                           BluetoothGattClientResultHandler* aRes);

  void ClientScanRsp(const DaemonSocketPDUHeader& aHeader,
                     DaemonSocketPDU& aPDU,
                     BluetoothGattClientResultHandler* aRes);

  void ClientConnectRsp(const DaemonSocketPDUHeader& aHeader,
                        DaemonSocketPDU& aPDU,
                        BluetoothGattClientResultHandler* aRes);

  void ClientDisconnectRsp(const DaemonSocketPDUHeader& aHeader,
                           DaemonSocketPDU& aPDU,
                           BluetoothGattClientResultHandler* aRes);

  void ClientListenRsp(const DaemonSocketPDUHeader& aHeader,
                       DaemonSocketPDU& aPDU,
                       BluetoothGattClientResultHandler* aRes);

  void ClientRefreshRsp(const DaemonSocketPDUHeader& aHeader,
                        DaemonSocketPDU& aPDU,
                        BluetoothGattClientResultHandler* aRes);

  void ClientSearchServiceRsp(const DaemonSocketPDUHeader& aHeader,
                              DaemonSocketPDU& aPDU,
                              BluetoothGattClientResultHandler* aRes);

  void ClientGetIncludedServiceRsp(const DaemonSocketPDUHeader& aHeader,
                                   DaemonSocketPDU& aPDU,
                                   BluetoothGattClientResultHandler* aRes);

  void ClientGetCharacteristicRsp(const DaemonSocketPDUHeader& aHeader,
                                  DaemonSocketPDU& aPDU,
                                  BluetoothGattClientResultHandler* aRes);

  void ClientGetDescriptorRsp(const DaemonSocketPDUHeader& aHeader,
                              DaemonSocketPDU& aPDU,
                              BluetoothGattClientResultHandler* aRes);

  void ClientReadCharacteristicRsp(const DaemonSocketPDUHeader& aHeader,
                                   DaemonSocketPDU& aPDU,
                                   BluetoothGattClientResultHandler* aRes);

  void ClientWriteCharacteristicRsp(const DaemonSocketPDUHeader& aHeader,
                                    DaemonSocketPDU& aPDU,
                                    BluetoothGattClientResultHandler* aRes);

  void ClientReadDescriptorRsp(const DaemonSocketPDUHeader& aHeader,
                               DaemonSocketPDU& aPDU,
                               BluetoothGattClientResultHandler* aRes);

  void ClientWriteDescriptorRsp(const DaemonSocketPDUHeader& aHeader,
                                DaemonSocketPDU& aPDU,
                                BluetoothGattClientResultHandler* aRes);

  void ClientExecuteWriteRsp(const DaemonSocketPDUHeader& aHeader,
                             DaemonSocketPDU& aPDU,
                             BluetoothGattClientResultHandler* aRes);

  void ClientRegisterNotificationRsp(const DaemonSocketPDUHeader& aHeader,
                                     DaemonSocketPDU& aPDU,
                                     BluetoothGattClientResultHandler* aRes);

  void ClientDeregisterNotificationRsp(const DaemonSocketPDUHeader& aHeader,
                                       DaemonSocketPDU& aPDU,
                                       BluetoothGattClientResultHandler* aRes);

  void ClientReadRemoteRssiRsp(const DaemonSocketPDUHeader& aHeader,
                               DaemonSocketPDU& aPDU,
                               BluetoothGattClientResultHandler* aRes);

  void ClientGetDeviceTypeRsp(const DaemonSocketPDUHeader& aHeader,
                              DaemonSocketPDU& aPDU,
                              BluetoothGattClientResultHandler* aRes);

  void ClientSetAdvDataRsp(const DaemonSocketPDUHeader& aHeader,
                           DaemonSocketPDU& aPDU,
                           BluetoothGattClientResultHandler* aRes);

  void ClientTestCommandRsp(const DaemonSocketPDUHeader& aHeader,
                            DaemonSocketPDU& aPDU,
                            BluetoothGattClientResultHandler* aRes);

  void HandleRsp(const DaemonSocketPDUHeader& aHeader,
                 DaemonSocketPDU& aPDU,
                 void* aUserData);

  // TODO: Add Server responses

  //
  // Notifications
  //

  class NotificationHandlerWrapper;
  class ClientNotificationHandlerWrapper;

  // GATT Client Notification
  typedef BluetoothNotificationRunnable3<
    ClientNotificationHandlerWrapper, void,
    BluetoothGattStatus, int, BluetoothUuid,
    BluetoothGattStatus, int, const BluetoothUuid&>
    ClientRegisterNotification;

  typedef BluetoothNotificationRunnable3<
    ClientNotificationHandlerWrapper, void,
    nsString, int, BluetoothGattAdvData,
    const nsAString&, int, const BluetoothGattAdvData&>
    ClientScanResultNotification;

  typedef BluetoothNotificationRunnable4<
    ClientNotificationHandlerWrapper, void,
    int, BluetoothGattStatus, int, nsString,
    int, BluetoothGattStatus, int, const nsAString&>
    ClientConnectNotification;

  typedef BluetoothNotificationRunnable4<
    ClientNotificationHandlerWrapper, void,
    int, BluetoothGattStatus, int, nsString,
    int, BluetoothGattStatus, int, const nsAString&>
    ClientDisconnectNotification;

  typedef BluetoothNotificationRunnable2<
    ClientNotificationHandlerWrapper, void,
    int, BluetoothGattStatus>
    ClientSearchCompleteNotification;

  typedef BluetoothNotificationRunnable2<
    ClientNotificationHandlerWrapper, void,
    int, BluetoothGattServiceId,
    int, const BluetoothGattServiceId&>
    ClientSearchResultNotification;

  typedef BluetoothNotificationRunnable5<
    ClientNotificationHandlerWrapper, void,
    int, BluetoothGattStatus, BluetoothGattServiceId,
    BluetoothGattId, BluetoothGattCharProp,
    int, BluetoothGattStatus, const BluetoothGattServiceId&,
    const BluetoothGattId&, const BluetoothGattCharProp&>
    ClientGetCharacteristicNotification;

  typedef BluetoothNotificationRunnable5<
    ClientNotificationHandlerWrapper, void,
    int, BluetoothGattStatus, BluetoothGattServiceId,
    BluetoothGattId, BluetoothGattId,
    int, BluetoothGattStatus, const BluetoothGattServiceId&,
    const BluetoothGattId&, const BluetoothGattId&>
    ClientGetDescriptorNotification;

  typedef BluetoothNotificationRunnable4<
    ClientNotificationHandlerWrapper, void,
    int, BluetoothGattStatus, BluetoothGattServiceId, BluetoothGattServiceId,
    int, BluetoothGattStatus, const BluetoothGattServiceId&,
    const BluetoothGattServiceId&>
    ClientGetIncludedServiceNotification;

  typedef BluetoothNotificationRunnable5<
    ClientNotificationHandlerWrapper, void,
    int, int, BluetoothGattStatus,
    BluetoothGattServiceId, BluetoothGattId,
    int, int, BluetoothGattStatus,
    const BluetoothGattServiceId&, const BluetoothGattId&>
    ClientRegisterNotificationNotification;

  typedef BluetoothNotificationRunnable2<
    ClientNotificationHandlerWrapper, void,
    int, BluetoothGattNotifyParam,
    int, const BluetoothGattNotifyParam&>
    ClientNotifyNotification;

  typedef BluetoothNotificationRunnable3<
    ClientNotificationHandlerWrapper, void,
    int, BluetoothGattStatus, BluetoothGattReadParam,
    int, BluetoothGattStatus, const BluetoothGattReadParam&>
    ClientReadCharacteristicNotification;

  typedef BluetoothNotificationRunnable3<
    ClientNotificationHandlerWrapper, void,
    int, BluetoothGattStatus, BluetoothGattWriteParam,
    int, BluetoothGattStatus, const BluetoothGattWriteParam&>
    ClientWriteCharacteristicNotification;

  typedef BluetoothNotificationRunnable3<
    ClientNotificationHandlerWrapper, void,
    int, BluetoothGattStatus, BluetoothGattReadParam,
    int, BluetoothGattStatus, const BluetoothGattReadParam&>
    ClientReadDescriptorNotification;

  typedef BluetoothNotificationRunnable3<
    ClientNotificationHandlerWrapper, void,
    int, BluetoothGattStatus, BluetoothGattWriteParam,
    int, BluetoothGattStatus, const BluetoothGattWriteParam&>
    ClientWriteDescriptorNotification;

  typedef BluetoothNotificationRunnable2<
    ClientNotificationHandlerWrapper, void,
    int, BluetoothGattStatus>
    ClientExecuteWriteNotification;

  typedef BluetoothNotificationRunnable4<
    ClientNotificationHandlerWrapper, void,
    int, nsString, int, BluetoothGattStatus,
    int, const nsAString&, int, BluetoothGattStatus>
    ClientReadRemoteRssiNotification;

  typedef BluetoothNotificationRunnable2<
    ClientNotificationHandlerWrapper, void,
    BluetoothGattStatus, int>
    ClientListenNotification;

  class ClientScanResultInitOp;
  class ClientConnectDisconnectInitOp;
  class ClientReadRemoteRssiInitOp;
  class ClientGetDeviceTypeInitOp;

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

  void Init(
    BluetoothGattNotificationHandler* aNotificationHandler,
    BluetoothGattResultHandler* aRes);
  void Cleanup(BluetoothGattResultHandler* aRes);

  BluetoothGattClientInterface* GetBluetoothGattClientInterface();

private:
  void DispatchError(BluetoothGattResultHandler* aRes,
                     BluetoothStatus aStatus);
  void DispatchError(BluetoothGattResultHandler* aRes, nsresult aRv);

  BluetoothDaemonGattModule* mModule;
};

class BluetoothDaemonGattClientInterface final
  : public BluetoothGattClientInterface
{
public:
  BluetoothDaemonGattClientInterface(BluetoothDaemonGattModule* aModule);

  /* Register / Unregister */
  void RegisterClient(const BluetoothUuid& aUuid,
                      BluetoothGattClientResultHandler* aRes);
  void UnregisterClient(int aClientIf,
                        BluetoothGattClientResultHandler* aRes);

  /* Start / Stop LE Scan */
  void Scan(int aClientIf, bool aStart,
            BluetoothGattClientResultHandler* aRes);

  /* Connect / Disconnect */
  void Connect(int aClientIf,
               const nsAString& aBdAddr,
               bool aIsDirect, /* auto connect */
               BluetoothTransport aTransport,
               BluetoothGattClientResultHandler* aRes);
  void Disconnect(int aClientIf,
                  const nsAString& aBdAddr,
                  int aConnId,
                  BluetoothGattClientResultHandler* aRes);

  /* Start / Stop advertisements to listen for incoming connections */
  void Listen(int aClientIf,
              bool aIsStart,
              BluetoothGattClientResultHandler* aRes);

  /* Clear the attribute cache for a given device*/
  void Refresh(int aClientIf,
               const nsAString& aBdAddr,
               BluetoothGattClientResultHandler* aRes);

  /* Enumerate Attributes */
  void SearchService(int aConnId,
                     bool aSearchAll,
                     const BluetoothUuid& aUuid,
                     BluetoothGattClientResultHandler* aRes);
  void GetIncludedService(int aConnId,
                          const BluetoothGattServiceId& aServiceId,
                          bool aFirst,
                          const BluetoothGattServiceId& aStartServiceId,
                          BluetoothGattClientResultHandler* aRes);
  void GetCharacteristic(int aConnId,
                         const BluetoothGattServiceId& aServiceId,
                         bool aFirst,
                         const BluetoothGattId& aStartCharId,
                         BluetoothGattClientResultHandler* aRes);
  void GetDescriptor(int aConnId,
                     const BluetoothGattServiceId& aServiceId,
                     const BluetoothGattId& aCharId,
                     bool aFirst,
                     const BluetoothGattId& aDescriptorId,
                     BluetoothGattClientResultHandler* aRes);

  /* Read / Write An Attribute */
  void ReadCharacteristic(int aConnId,
                          const BluetoothGattServiceId& aServiceId,
                          const BluetoothGattId& aCharId,
                          BluetoothGattAuthReq aAuthReq,
                          BluetoothGattClientResultHandler* aRes);
  void WriteCharacteristic(int aConnId,
                           const BluetoothGattServiceId& aServiceId,
                           const BluetoothGattId& aCharId,
                           BluetoothGattWriteType aWriteType,
                           BluetoothGattAuthReq aAuthReq,
                           const nsTArray<uint8_t>& aValue,
                           BluetoothGattClientResultHandler* aRes);
  void ReadDescriptor(int aConnId,
                      const BluetoothGattServiceId& aServiceId,
                      const BluetoothGattId& aCharId,
                      const BluetoothGattId& aDescriptorId,
                      BluetoothGattAuthReq aAuthReq,
                      BluetoothGattClientResultHandler* aRes);
  void WriteDescriptor(int aConnId,
                       const BluetoothGattServiceId& aServiceId,
                       const BluetoothGattId& aCharId,
                       const BluetoothGattId& aDescriptorId,
                       BluetoothGattWriteType aWriteType,
                       BluetoothGattAuthReq aAuthReq,
                       const nsTArray<uint8_t>& aValue,
                       BluetoothGattClientResultHandler* aRes);

  /* Execute / Abort Prepared Write*/
  void ExecuteWrite(int aConnId,
                    int aIsExecute,
                    BluetoothGattClientResultHandler* aRes);


  /* Register / Deregister Characteristic Notifications or Indications */
  void RegisterNotification(int aClientIf,
                            const nsAString& aBdAddr,
                            const BluetoothGattServiceId& aServiceId,
                            const BluetoothGattId& aCharId,
                            BluetoothGattClientResultHandler* aRes);
  void DeregisterNotification(int aClientIf,
                              const nsAString& aBdAddr,
                              const BluetoothGattServiceId& aServiceId,
                              const BluetoothGattId& aCharId,
                              BluetoothGattClientResultHandler* aRes);

  void ReadRemoteRssi(int aClientIf,
                      const nsAString& aBdAddr,
                      BluetoothGattClientResultHandler* aRes);

  void GetDeviceType(const nsAString& aBdAddr,
                     BluetoothGattClientResultHandler* aRes);

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
                  BluetoothGattClientResultHandler* aRes);

  void TestCommand(int aCommand,
                   const BluetoothGattTestParam& aTestParam,
                   BluetoothGattClientResultHandler* aRes);

private:
  void DispatchError(BluetoothGattClientResultHandler* aRes,
                     BluetoothStatus aStatus);
  void DispatchError(BluetoothGattClientResultHandler* aRes, nsresult aRv);
  BluetoothDaemonGattModule* mModule;
};

// TODO: Add GattServerInterface
END_BLUETOOTH_NAMESPACE

#endif
