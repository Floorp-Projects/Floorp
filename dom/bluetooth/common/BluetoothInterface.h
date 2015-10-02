/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_BluetoothInterface_h
#define mozilla_dom_bluetooth_BluetoothInterface_h

#include "BluetoothCommon.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "mozilla/ipc/DaemonSocketMessageHandlers.h"

BEGIN_BLUETOOTH_NAMESPACE

//
// Setup Interface
//

class BluetoothSetupResultHandler
  : public mozilla::ipc::DaemonSocketResultHandler
{
public:
  virtual void OnError(BluetoothStatus aStatus);
  virtual void RegisterModule();
  virtual void UnregisterModule();
  virtual void Configuration();

protected:
  virtual ~BluetoothSetupResultHandler() { }
};

//
// Socket Interface
//

class BluetoothSocketResultHandler
  : public mozilla::ipc::DaemonSocketResultHandler
{
public:
  virtual void OnError(BluetoothStatus aStatus);

  virtual void Listen(int aSockFd);
  virtual void Connect(int aSockFd, const BluetoothAddress& aBdAddress,
                       int aConnectionState);
  virtual void Accept(int aSockFd, const BluetoothAddress& aBdAddress,
                      int aConnectionState);

protected:
  virtual ~BluetoothSocketResultHandler() { }
};

class BluetoothSocketInterface
{
public:
  // Init and Cleanup is handled by BluetoothInterface

  virtual void Listen(BluetoothSocketType aType,
                      const BluetoothServiceName& aServiceName,
                      const BluetoothUuid& aServiceUuid,
                      int aChannel, bool aEncrypt, bool aAuth,
                      BluetoothSocketResultHandler* aRes) = 0;

  virtual void Connect(const BluetoothAddress& aBdAddr,
                       BluetoothSocketType aType,
                       const BluetoothUuid& aServiceUuid,
                       int aChannel, bool aEncrypt, bool aAuth,
                       BluetoothSocketResultHandler* aRes) = 0;

  virtual void Accept(int aFd, BluetoothSocketResultHandler* aRes) = 0;

  virtual void Close(BluetoothSocketResultHandler* aRes) = 0;

protected:
  virtual ~BluetoothSocketInterface();
};

//
// Handsfree Interface
//

class BluetoothHandsfreeNotificationHandler
{
public:
  virtual void
  ConnectionStateNotification(BluetoothHandsfreeConnectionState aState,
                              const BluetoothAddress& aBdAddr);

  virtual void
  AudioStateNotification(BluetoothHandsfreeAudioState aState,
                         const BluetoothAddress& aBdAddr);

  virtual void
  VoiceRecognitionNotification(BluetoothHandsfreeVoiceRecognitionState aState,
                               const BluetoothAddress& aBdAddr);

  virtual void
  AnswerCallNotification(const BluetoothAddress& aBdAddr);

  virtual void
  HangupCallNotification(const BluetoothAddress& aBdAddr);

  virtual void
  VolumeNotification(BluetoothHandsfreeVolumeType aType,
                     int aVolume,
                     const BluetoothAddress& aBdAddr);

  virtual void
  DialCallNotification(const nsAString& aNumber,
                       const BluetoothAddress& aBdAddr);

  virtual void
  DtmfNotification(char aDtmf,
                   const BluetoothAddress& aBdAddr);

  virtual void
  NRECNotification(BluetoothHandsfreeNRECState aNrec,
                   const BluetoothAddress& aBdAddr);

  virtual void
  WbsNotification(BluetoothHandsfreeWbsConfig aWbs,
                  const BluetoothAddress& aBdAddr);

  virtual void
  CallHoldNotification(BluetoothHandsfreeCallHoldType aChld,
                       const BluetoothAddress& aBdAddr);

  virtual void
  CnumNotification(const BluetoothAddress& aBdAddr);

  virtual void
  CindNotification(const BluetoothAddress& aBdAddr);

  virtual void
  CopsNotification(const BluetoothAddress& aBdAddr);

  virtual void
  ClccNotification(const BluetoothAddress& aBdAddr);

  virtual void
  UnknownAtNotification(const nsACString& aAtString,
                        const BluetoothAddress& aBdAddr);

  virtual void
  KeyPressedNotification(const BluetoothAddress& aBdAddr);

protected:
  BluetoothHandsfreeNotificationHandler();
  virtual ~BluetoothHandsfreeNotificationHandler();
};

class BluetoothHandsfreeResultHandler
  : public mozilla::ipc::DaemonSocketResultHandler
{
public:
  virtual void OnError(BluetoothStatus aStatus);

  virtual void Init();
  virtual void Cleanup();

  virtual void Connect();
  virtual void Disconnect();
  virtual void ConnectAudio();
  virtual void DisconnectAudio();

  virtual void StartVoiceRecognition();
  virtual void StopVoiceRecognition();

  virtual void VolumeControl();

  virtual void DeviceStatusNotification();

  virtual void CopsResponse();
  virtual void CindResponse();
  virtual void FormattedAtResponse();
  virtual void AtResponse();
  virtual void ClccResponse();
  virtual void PhoneStateChange();

  virtual void ConfigureWbs();

protected:
  virtual ~BluetoothHandsfreeResultHandler() { }
};

class BluetoothHandsfreeInterface
{
public:
  virtual void Init(
    BluetoothHandsfreeNotificationHandler* aNotificationHandler,
    int aMaxNumClients, BluetoothHandsfreeResultHandler* aRes) = 0;
  virtual void Cleanup(BluetoothHandsfreeResultHandler* aRes) = 0;

  /* Connect / Disconnect */

  virtual void Connect(const BluetoothAddress& aBdAddr,
                       BluetoothHandsfreeResultHandler* aRes) = 0;
  virtual void Disconnect(const BluetoothAddress& aBdAddr,
                          BluetoothHandsfreeResultHandler* aRes) = 0;
  virtual void ConnectAudio(const BluetoothAddress& aBdAddr,
                            BluetoothHandsfreeResultHandler* aRes) = 0;
  virtual void DisconnectAudio(const BluetoothAddress& aBdAddr,
                               BluetoothHandsfreeResultHandler* aRes) = 0;

  /* Voice Recognition */

  virtual void StartVoiceRecognition(const BluetoothAddress& aBdAddr,
                                     BluetoothHandsfreeResultHandler* aRes) = 0;
  virtual void StopVoiceRecognition(const BluetoothAddress& aBdAddr,
                                    BluetoothHandsfreeResultHandler* aRes) = 0;

  /* Volume */

  virtual void VolumeControl(BluetoothHandsfreeVolumeType aType, int aVolume,
                             const BluetoothAddress& aBdAddr,
                             BluetoothHandsfreeResultHandler* aRes) = 0;

  /* Device status */

  virtual void DeviceStatusNotification(
    BluetoothHandsfreeNetworkState aNtkState,
    BluetoothHandsfreeServiceType aSvcType,
    int aSignal, int aBattChg, BluetoothHandsfreeResultHandler* aRes) = 0;

  /* Responses */

  virtual void CopsResponse(const char* aCops, const BluetoothAddress& aBdAddr,
                            BluetoothHandsfreeResultHandler* aRes) = 0;
  virtual void CindResponse(int aSvc, int aNumActive, int aNumHeld,
                            BluetoothHandsfreeCallState aCallSetupState,
                            int aSignal, int aRoam, int aBattChg,
                            const BluetoothAddress& aBdAddr,
                            BluetoothHandsfreeResultHandler* aRes) = 0;
  virtual void FormattedAtResponse(const char* aRsp,
                                   const BluetoothAddress& aBdAddr,
                                   BluetoothHandsfreeResultHandler* aRes) = 0;
  virtual void AtResponse(BluetoothHandsfreeAtResponse aResponseCode,
                          int aErrorCode, const BluetoothAddress& aBdAddr,
                          BluetoothHandsfreeResultHandler* aRes) = 0;
  virtual void ClccResponse(int aIndex, BluetoothHandsfreeCallDirection aDir,
                            BluetoothHandsfreeCallState aState,
                            BluetoothHandsfreeCallMode aMode,
                            BluetoothHandsfreeCallMptyType aMpty,
                            const nsAString& aNumber,
                            BluetoothHandsfreeCallAddressType aType,
                            const BluetoothAddress& aBdAddr,
                            BluetoothHandsfreeResultHandler* aRes) = 0;

  /* Phone State */

  virtual void PhoneStateChange(int aNumActive, int aNumHeld,
                                BluetoothHandsfreeCallState aCallSetupState,
                                const nsAString& aNumber,
                                BluetoothHandsfreeCallAddressType aType,
                                BluetoothHandsfreeResultHandler* aRes) = 0;

  /* Wide Band Speech */
  virtual void ConfigureWbs(const BluetoothAddress& aBdAddr,
                            BluetoothHandsfreeWbsConfig aConfig,
                            BluetoothHandsfreeResultHandler* aRes) = 0;

protected:
  BluetoothHandsfreeInterface();
  virtual ~BluetoothHandsfreeInterface();
};

//
// Bluetooth Advanced Audio Interface
//

class BluetoothA2dpNotificationHandler
{
public:
  virtual void
  ConnectionStateNotification(BluetoothA2dpConnectionState aState,
                              const BluetoothAddress& aBdAddr);

  virtual void
  AudioStateNotification(BluetoothA2dpAudioState aState,
                         const BluetoothAddress& aBdAddr);

  virtual void
  AudioConfigNotification(const BluetoothAddress& aBdAddr,
                          uint32_t aSampleRate,
                          uint8_t aChannelCount);

protected:
  BluetoothA2dpNotificationHandler();
  virtual ~BluetoothA2dpNotificationHandler();
};

class BluetoothA2dpResultHandler
  : public mozilla::ipc::DaemonSocketResultHandler
{
public:
  virtual void OnError(BluetoothStatus aStatus);

  virtual void Init();
  virtual void Cleanup();
  virtual void Connect();
  virtual void Disconnect();

protected:
  virtual ~BluetoothA2dpResultHandler() { }
};

class BluetoothA2dpInterface
{
public:
  virtual void Init(BluetoothA2dpNotificationHandler* aNotificationHandler,
                    BluetoothA2dpResultHandler* aRes) = 0;
  virtual void Cleanup(BluetoothA2dpResultHandler* aRes) = 0;

  virtual void Connect(const BluetoothAddress& aBdAddr,
                       BluetoothA2dpResultHandler* aRes) = 0;
  virtual void Disconnect(const BluetoothAddress& aBdAddr,
                          BluetoothA2dpResultHandler* aRes) = 0;

protected:
  BluetoothA2dpInterface();
  virtual ~BluetoothA2dpInterface();
};

//
// Bluetooth AVRCP Interface
//

class BluetoothAvrcpNotificationHandler
{
public:
  virtual void
  GetPlayStatusNotification();

  virtual void
  ListPlayerAppAttrNotification();

  virtual void
  ListPlayerAppValuesNotification(BluetoothAvrcpPlayerAttribute aAttrId);

  virtual void
  GetPlayerAppValueNotification(uint8_t aNumAttrs,
                                const BluetoothAvrcpPlayerAttribute* aAttrs);

  virtual void
  GetPlayerAppAttrsTextNotification(
    uint8_t aNumAttrs, const BluetoothAvrcpPlayerAttribute* aAttrs);

  virtual void
  GetPlayerAppValuesTextNotification(uint8_t aAttrId, uint8_t aNumVals,
                                     const uint8_t* aValues);

  virtual void
  SetPlayerAppValueNotification(
    const BluetoothAvrcpPlayerSettings& aSettings);

  virtual void
  GetElementAttrNotification(uint8_t aNumAttrs,
                             const BluetoothAvrcpMediaAttribute* aAttrs);

  virtual void
  RegisterNotificationNotification(BluetoothAvrcpEvent aEvent,
                                   uint32_t aParam);

  virtual void
  RemoteFeatureNotification(const BluetoothAddress& aBdAddr,
                            unsigned long aFeatures);

  virtual void
  VolumeChangeNotification(uint8_t aVolume, uint8_t aCType);

  virtual void
  PassthroughCmdNotification(int aId, int aKeyState);

protected:
  BluetoothAvrcpNotificationHandler();
  virtual ~BluetoothAvrcpNotificationHandler();
};

class BluetoothAvrcpResultHandler
  : public mozilla::ipc::DaemonSocketResultHandler
{
public:
  virtual void OnError(BluetoothStatus aStatus);

  virtual void Init();
  virtual void Cleanup();

  virtual void GetPlayStatusRsp();

  virtual void ListPlayerAppAttrRsp();
  virtual void ListPlayerAppValueRsp();

  virtual void GetPlayerAppValueRsp();
  virtual void GetPlayerAppAttrTextRsp();
  virtual void GetPlayerAppValueTextRsp();

  virtual void GetElementAttrRsp();

  virtual void SetPlayerAppValueRsp();

  virtual void RegisterNotificationRsp();

  virtual void SetVolume();

protected:
  virtual ~BluetoothAvrcpResultHandler() { }
};

class BluetoothAvrcpInterface
{
public:
  virtual void Init(BluetoothAvrcpNotificationHandler* aNotificationHandler,
                    BluetoothAvrcpResultHandler* aRes) = 0;
  virtual void Cleanup(BluetoothAvrcpResultHandler* aRes) = 0;

  virtual void GetPlayStatusRsp(ControlPlayStatus aPlayStatus,
                                uint32_t aSongLen, uint32_t aSongPos,
                                BluetoothAvrcpResultHandler* aRes) = 0;

  virtual void ListPlayerAppAttrRsp(
    int aNumAttr, const BluetoothAvrcpPlayerAttribute* aPAttrs,
    BluetoothAvrcpResultHandler* aRes) = 0;
  virtual void ListPlayerAppValueRsp(int aNumVal, uint8_t* aPVals,
                                     BluetoothAvrcpResultHandler* aRes) = 0;

  /* TODO: redesign this interface once we actually use it */
  virtual void GetPlayerAppValueRsp(uint8_t aNumAttrs, const uint8_t* aIds,
                                    const uint8_t* aValues,
                                    BluetoothAvrcpResultHandler* aRes) = 0;
  /* TODO: redesign this interface once we actually use it */
  virtual void GetPlayerAppAttrTextRsp(int aNumAttr, const uint8_t* aIds,
                                       const char** aTexts,
                                       BluetoothAvrcpResultHandler* aRes) = 0;
  /* TODO: redesign this interface once we actually use it */
  virtual void GetPlayerAppValueTextRsp(int aNumVal, const uint8_t* aIds,
                                        const char** aTexts,
                                        BluetoothAvrcpResultHandler* aRes) = 0;

  virtual void GetElementAttrRsp(uint8_t aNumAttr,
                                 const BluetoothAvrcpElementAttribute* aAttr,
                                 BluetoothAvrcpResultHandler* aRes) = 0;

  virtual void SetPlayerAppValueRsp(BluetoothAvrcpStatus aRspStatus,
                                    BluetoothAvrcpResultHandler* aRes) = 0;

  virtual void RegisterNotificationRsp(
    BluetoothAvrcpEvent aEvent, BluetoothAvrcpNotification aType,
    const BluetoothAvrcpNotificationParam& aParam,
    BluetoothAvrcpResultHandler* aRes) = 0;

  virtual void SetVolume(uint8_t aVolume,
                         BluetoothAvrcpResultHandler* aRes) = 0;

protected:
  BluetoothAvrcpInterface();
  virtual ~BluetoothAvrcpInterface();
};

//
// GATT Interface
//

class BluetoothGattNotificationHandler
{
public:
  virtual void
  RegisterClientNotification(BluetoothGattStatus aStatus,
                             int aClientIf,
                             const BluetoothUuid& aAppUuid);

  virtual void
  ScanResultNotification(const BluetoothAddress& aBdAddr,
                         int aRssi,
                         const BluetoothGattAdvData& aAdvData);

  virtual void
  ConnectNotification(int aConnId,
                      BluetoothGattStatus aStatus,
                      int aClientIf,
                      const BluetoothAddress& aBdAddr);

  virtual void
  DisconnectNotification(int aConnId,
                         BluetoothGattStatus aStatus,
                         int aClientIf,
                         const BluetoothAddress& aBdAddr);

  virtual void
  SearchCompleteNotification(int aConnId, BluetoothGattStatus aStatus);

  virtual void
  SearchResultNotification(int aConnId,
                           const BluetoothGattServiceId& aServiceId);

  virtual void
  GetCharacteristicNotification(int aConnId,
                                BluetoothGattStatus aStatus,
                                const BluetoothGattServiceId& aServiceId,
                                const BluetoothGattId& aCharId,
                                const BluetoothGattCharProp& aCharProperty);

  virtual void
  GetDescriptorNotification(int aConnId,
                            BluetoothGattStatus aStatus,
                            const BluetoothGattServiceId& aServiceId,
                            const BluetoothGattId& aCharId,
                            const BluetoothGattId& aDescriptorId);

  virtual void
  GetIncludedServiceNotification(int aConnId,
                                 BluetoothGattStatus aStatus,
                                 const BluetoothGattServiceId& aServiceId,
                                 const BluetoothGattServiceId& aIncludedServId);

  virtual void
  RegisterNotificationNotification(int aConnId,
                                   int aIsRegister,
                                   BluetoothGattStatus aStatus,
                                   const BluetoothGattServiceId& aServiceId,
                                   const BluetoothGattId& aCharId);

  virtual void
  NotifyNotification(int aConnId,
                     const BluetoothGattNotifyParam& aNotifyParam);

  virtual void
  ReadCharacteristicNotification(int aConnId,
                                 BluetoothGattStatus aStatus,
                                 const BluetoothGattReadParam& aReadParam);

  virtual void
  WriteCharacteristicNotification(int aConnId,
                                  BluetoothGattStatus aStatus,
                                  const BluetoothGattWriteParam& aWriteParam);

  virtual void
  ReadDescriptorNotification(int aConnId,
                             BluetoothGattStatus aStatus,
                             const BluetoothGattReadParam& aReadParam);

  virtual void
  WriteDescriptorNotification(int aConnId,
                              BluetoothGattStatus aStatus,
                              const BluetoothGattWriteParam& aWriteParam);

  virtual void
  ExecuteWriteNotification(int aConnId, BluetoothGattStatus aStatus);

  virtual void
  ReadRemoteRssiNotification(int aClientIf,
                             const BluetoothAddress& aBdAddr,
                             int aRssi,
                             BluetoothGattStatus aStatus);

  virtual void
  ListenNotification(BluetoothGattStatus aStatus, int aServerIf);

  virtual void
  RegisterServerNotification(BluetoothGattStatus aStatus,
                             int aServerIf,
                             const BluetoothUuid& aAppUuid);

  virtual void
  ConnectionNotification(int aConnId,
                         int aServerIf,
                         bool aConnected,
                         const BluetoothAddress& aBdAddr);

  virtual void
  ServiceAddedNotification(BluetoothGattStatus aStatus,
                           int aServerIf,
                           const BluetoothGattServiceId& aServiceId,
                           const BluetoothAttributeHandle& aServiceHandle);

  virtual void
  IncludedServiceAddedNotification(
    BluetoothGattStatus aStatus,
    int aServerIf,
    const BluetoothAttributeHandle& aServiceHandle,
    const BluetoothAttributeHandle& aIncludedServiceHandle);

  virtual void
  CharacteristicAddedNotification(
    BluetoothGattStatus aStatus,
    int aServerIf,
    const BluetoothUuid& aCharId,
    const BluetoothAttributeHandle& aServiceHandle,
    const BluetoothAttributeHandle& aCharacteristicHandle);

  virtual void
  DescriptorAddedNotification(
    BluetoothGattStatus aStatus,
    int aServerIf,
    const BluetoothUuid& aCharId,
    const BluetoothAttributeHandle& aServiceHandle,
    const BluetoothAttributeHandle& aDescriptorHandle);

  virtual void
  ServiceStartedNotification(BluetoothGattStatus aStatus,
                             int aServerIf,
                             const BluetoothAttributeHandle& aServiceHandle);

  virtual void
  ServiceStoppedNotification(BluetoothGattStatus aStatus,
                             int aServerIf,
                             const BluetoothAttributeHandle& aServiceHandle);

  virtual void
  ServiceDeletedNotification(BluetoothGattStatus aStatus,
                             int aServerIf,
                             const BluetoothAttributeHandle& aServiceHandle);

  virtual void
  RequestReadNotification(int aConnId,
                          int aTransId,
                          const BluetoothAddress& aBdAddr,
                          const BluetoothAttributeHandle& aAttributeHandle,
                          int aOffset,
                          bool aIsLong);

  virtual void
  RequestWriteNotification(int aConnId,
                           int aTransId,
                           const BluetoothAddress& aBdAddr,
                           const BluetoothAttributeHandle& aAttributeHandle,
                           int aOffset,
                           int aLength,
                           const uint8_t* aValue,
                           bool aNeedResponse,
                           bool aIsPrepareWrite);

  virtual void
  RequestExecuteWriteNotification(int aConnId,
                                  int aTransId,
                                  const BluetoothAddress& aBdAddr,
                                  bool aExecute); /* true: execute */
                                                  /* false: cancel */

  virtual void
  ResponseConfirmationNotification(BluetoothGattStatus aStatus,
                                   int aHandle);

  virtual void
  IndicationSentNotification(int aConnId,
                             BluetoothGattStatus aStatus);

  virtual void
  CongestionNotification(int aConnId,
                         bool aCongested);

  virtual void
  MtuChangedNotification(int aConnId,
                         int aMtu);

protected:
  BluetoothGattNotificationHandler();
  virtual ~BluetoothGattNotificationHandler();
};

class BluetoothGattResultHandler
  : public mozilla::ipc::DaemonSocketResultHandler
{
public:
  virtual void OnError(BluetoothStatus aStatus);

  virtual void Init();
  virtual void Cleanup();

  virtual void RegisterClient();
  virtual void UnregisterClient();

  virtual void Scan();

  virtual void Connect();
  virtual void Disconnect();

  virtual void Listen();
  virtual void Refresh();

  virtual void SearchService();
  virtual void GetIncludedService();
  virtual void GetCharacteristic();
  virtual void GetDescriptor();

  virtual void ReadCharacteristic();
  virtual void WriteCharacteristic();
  virtual void ReadDescriptor();
  virtual void WriteDescriptor();

  virtual void ExecuteWrite();

  virtual void RegisterNotification();
  virtual void DeregisterNotification();

  virtual void ReadRemoteRssi();
  virtual void GetDeviceType(BluetoothTypeOfDevice aType);
  virtual void SetAdvData();
  virtual void TestCommand();

  virtual void RegisterServer();
  virtual void UnregisterServer();

  virtual void ConnectPeripheral();
  virtual void DisconnectPeripheral();

  virtual void AddService();
  virtual void AddIncludedService();
  virtual void AddCharacteristic();
  virtual void AddDescriptor();

  virtual void StartService();
  virtual void StopService();
  virtual void DeleteService();

  virtual void SendIndication();

  virtual void SendResponse();

protected:
  virtual ~BluetoothGattResultHandler() { }
};

class BluetoothGattInterface
{
public:
  virtual void Init(BluetoothGattNotificationHandler* aNotificationHandler,
                    BluetoothGattResultHandler* aRes) = 0;
  virtual void Cleanup(BluetoothGattResultHandler* aRes) = 0;

  /* Register / Unregister */
  virtual void RegisterClient(const BluetoothUuid& aUuid,
                              BluetoothGattResultHandler* aRes) = 0;
  virtual void UnregisterClient(int aClientIf,
                                BluetoothGattResultHandler* aRes) = 0;

  /* Start / Stop LE Scan */
  virtual void Scan(int aClientIf, bool aStart,
                    BluetoothGattResultHandler* aRes) = 0;

  /* Connect / Disconnect */
  virtual void Connect(int aClientIf,
                       const BluetoothAddress& aBdAddr,
                       bool aIsDirect, /* auto connect */
                       BluetoothTransport aTransport,
                       BluetoothGattResultHandler* aRes) = 0;
  virtual void Disconnect(int aClientIf,
                          const BluetoothAddress& aBdAddr,
                          int aConnId,
                          BluetoothGattResultHandler* aRes) = 0;

  /* Start / Stop advertisements to listen for incoming connections */
  virtual void Listen(int aClientIf,
                      bool aIsStart,
                      BluetoothGattResultHandler* aRes) = 0;

  /* Clear the attribute cache for a given device*/
  virtual void Refresh(int aClientIf,
                       const BluetoothAddress& aBdAddr,
                       BluetoothGattResultHandler* aRes) = 0;

  /* Enumerate Attributes */
  virtual void SearchService(int aConnId,
                             bool aSearchAll,
                             const BluetoothUuid& aUuid,
                             BluetoothGattResultHandler* aRes) = 0;
  virtual void GetIncludedService(
    int aConnId,
    const BluetoothGattServiceId& aServiceId,
    bool aFirst,
    const BluetoothGattServiceId& aStartServiceId,
    BluetoothGattResultHandler* aRes) = 0;
  virtual void GetCharacteristic(int aConnId,
                                 const BluetoothGattServiceId& aServiceId,
                                 bool aFirst,
                                 const BluetoothGattId& aStartCharId,
                                 BluetoothGattResultHandler* aRes) = 0;
  virtual void GetDescriptor(int aConnId,
                             const BluetoothGattServiceId& aServiceId,
                             const BluetoothGattId& aCharId,
                             bool aFirst,
                             const BluetoothGattId& aDescriptorId,
                             BluetoothGattResultHandler* aRes) = 0;

  /* Read / Write An Attribute */
  virtual void ReadCharacteristic(int aConnId,
                                  const BluetoothGattServiceId& aServiceId,
                                  const BluetoothGattId& aCharId,
                                  BluetoothGattAuthReq aAuthReq,
                                  BluetoothGattResultHandler* aRes) = 0;
  virtual void WriteCharacteristic(int aConnId,
                                   const BluetoothGattServiceId& aServiceId,
                                   const BluetoothGattId& aCharId,
                                   BluetoothGattWriteType aWriteType,
                                   BluetoothGattAuthReq aAuthReq,
                                   const nsTArray<uint8_t>& aValue,
                                   BluetoothGattResultHandler* aRes) = 0;
  virtual void ReadDescriptor(int aConnId,
                              const BluetoothGattServiceId& aServiceId,
                              const BluetoothGattId& aCharId,
                              const BluetoothGattId& aDescriptorId,
                              BluetoothGattAuthReq aAuthReq,
                              BluetoothGattResultHandler* aRes) = 0;
  virtual void WriteDescriptor(int aConnId,
                               const BluetoothGattServiceId& aServiceId,
                               const BluetoothGattId& aCharId,
                               const BluetoothGattId& aDescriptorId,
                               BluetoothGattWriteType aWriteType,
                               BluetoothGattAuthReq aAuthReq,
                               const nsTArray<uint8_t>& aValue,
                               BluetoothGattResultHandler* aRes) = 0;

  /* Execute / Abort Prepared Write*/
  virtual void ExecuteWrite(int aConnId,
                            int aIsExecute,
                            BluetoothGattResultHandler* aRes) = 0;

  /* Register / Deregister Characteristic Notifications or Indications */
  virtual void RegisterNotification(
    int aClientIf,
    const BluetoothAddress& aBdAddr,
    const BluetoothGattServiceId& aServiceId,
    const BluetoothGattId& aCharId,
    BluetoothGattResultHandler* aRes) = 0;
  virtual void DeregisterNotification(
    int aClientIf,
    const BluetoothAddress& aBdAddr,
    const BluetoothGattServiceId& aServiceId,
    const BluetoothGattId& aCharId,
    BluetoothGattResultHandler* aRes) = 0;

  virtual void ReadRemoteRssi(int aClientIf,
                              const BluetoothAddress& aBdAddr,
                              BluetoothGattResultHandler* aRes) = 0;

  virtual void GetDeviceType(const BluetoothAddress& aBdAddr,
                             BluetoothGattResultHandler* aRes) = 0;

  /* Set advertising data or scan response data */
  virtual void SetAdvData(int aServerIf,
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
                          BluetoothGattResultHandler* aRes) = 0;

  virtual void TestCommand(int aCommand,
                           const BluetoothGattTestParam& aTestParam,
                           BluetoothGattResultHandler* aRes) = 0;

  /* Register / Unregister */
  virtual void RegisterServer(const BluetoothUuid& aUuid,
                              BluetoothGattResultHandler* aRes) = 0;
  virtual void UnregisterServer(int aServerIf,
                                BluetoothGattResultHandler* aRes) = 0;

  /* Connect / Disconnect */
  virtual void ConnectPeripheral(int aServerIf,
                                 const BluetoothAddress& aBdAddr,
                                 bool aIsDirect, /* auto connect */
                                 BluetoothTransport aTransport,
                                 BluetoothGattResultHandler* aRes) = 0;
  virtual void DisconnectPeripheral(int aServerIf,
                                    const BluetoothAddress& aBdAddr,
                                    int aConnId,
                                    BluetoothGattResultHandler* aRes) = 0;

  /* Add a services / a characteristic / a descriptor */
  virtual void AddService(int aServerIf,
                          const BluetoothGattServiceId& aServiceId,
                          uint16_t aNumHandles,
                          BluetoothGattResultHandler* aRes) = 0;
  virtual void AddIncludedService(
    int aServerIf,
    const BluetoothAttributeHandle& aServiceHandle,
    const BluetoothAttributeHandle& aIncludedServiceHandle,
    BluetoothGattResultHandler* aRes) = 0;
  virtual void AddCharacteristic(int aServerIf,
                                 const BluetoothAttributeHandle& aServiceHandle,
                                 const BluetoothUuid& aUuid,
                                 BluetoothGattCharProp aProperties,
                                 BluetoothGattAttrPerm aPermissions,
                                 BluetoothGattResultHandler* aRes) = 0;
  virtual void AddDescriptor(int aServerIf,
                             const BluetoothAttributeHandle& aServiceHandle,
                             const BluetoothUuid& aUuid,
                             BluetoothGattAttrPerm aPermissions,
                             BluetoothGattResultHandler* aRes) = 0;

  /* Start / Stop / Delete a service */
  virtual void StartService(int aServerIf,
                            const BluetoothAttributeHandle& aServiceHandle,
                            BluetoothTransport aTransport,
                            BluetoothGattResultHandler* aRes) = 0;
  virtual void StopService(int aServerIf,
                           const BluetoothAttributeHandle& aServiceHandle,
                           BluetoothGattResultHandler* aRes) = 0;
  virtual void DeleteService(int aServerIf,
                             const BluetoothAttributeHandle& aServiceHandle,
                             BluetoothGattResultHandler* aRes) = 0;

  /* Send an indication or a notification */
  virtual void SendIndication(
    int aServerIf,
    const BluetoothAttributeHandle& aCharacteristicHandle,
    int aConnId,
    const nsTArray<uint8_t>& aValue,
    bool aConfirm, /* true: indication; false: notification */
    BluetoothGattResultHandler* aRes) = 0;

  /* Send a response for an incoming indication */
  virtual void SendResponse(int aConnId,
                            int aTransId,
                            uint16_t aStatus,
                            const BluetoothGattResponse& aResponse,
                            BluetoothGattResultHandler* aRes) = 0;

protected:
  BluetoothGattInterface();
  virtual ~BluetoothGattInterface();
};

//
// Bluetooth Core Interface
//

class BluetoothNotificationHandler
{
public:
  virtual void AdapterStateChangedNotification(bool aState);
  virtual void AdapterPropertiesNotification(
    BluetoothStatus aStatus, int aNumProperties,
    const BluetoothProperty* aProperties);

  virtual void RemoteDevicePropertiesNotification(
    BluetoothStatus aStatus, const BluetoothAddress& aBdAddr,
    int aNumProperties, const BluetoothProperty* aProperties);

  virtual void DeviceFoundNotification(
    int aNumProperties, const BluetoothProperty* aProperties);

  virtual void DiscoveryStateChangedNotification(bool aState);

  virtual void PinRequestNotification(const BluetoothAddress& aRemoteBdAddr,
                                      const BluetoothRemoteName& aBdName,
                                      uint32_t aCod);
  virtual void SspRequestNotification(const BluetoothAddress& aRemoteBdAddr,
                                      const BluetoothRemoteName& aBdName,
                                      uint32_t aCod,
                                      BluetoothSspVariant aPairingVariant,
                                      uint32_t aPassKey);

  virtual void BondStateChangedNotification(
    BluetoothStatus aStatus, const BluetoothAddress& aRemoteBdAddr,
    BluetoothBondState aState);
  virtual void AclStateChangedNotification(
    BluetoothStatus aStatus, const BluetoothAddress& aRemoteBdAddr,
    BluetoothAclState aState);

  virtual void DutModeRecvNotification(uint16_t aOpcode,
                                       const uint8_t* aBuf, uint8_t aLen);
  virtual void LeTestModeNotification(BluetoothStatus aStatus,
                                      uint16_t aNumPackets);

  virtual void EnergyInfoNotification(const BluetoothActivityEnergyInfo& aInfo);

  virtual void BackendErrorNotification(bool aCrashed);

protected:
  BluetoothNotificationHandler();
  virtual ~BluetoothNotificationHandler();
};

class BluetoothResultHandler
  : public mozilla::ipc::DaemonSocketResultHandler
{
public:
  virtual void OnError(BluetoothStatus aStatus);

  virtual void Init();
  virtual void Cleanup();
  virtual void Enable();
  virtual void Disable();

  virtual void GetAdapterProperties();
  virtual void GetAdapterProperty();
  virtual void SetAdapterProperty();

  virtual void GetRemoteDeviceProperties();
  virtual void GetRemoteDeviceProperty();
  virtual void SetRemoteDeviceProperty();

  virtual void GetRemoteServiceRecord();
  virtual void GetRemoteServices();

  virtual void StartDiscovery();
  virtual void CancelDiscovery();

  virtual void CreateBond();
  virtual void RemoveBond();
  virtual void CancelBond();

  virtual void GetConnectionState();

  virtual void PinReply();
  virtual void SspReply();

  virtual void DutModeConfigure();
  virtual void DutModeSend();

  virtual void LeTestMode();

  virtual void ReadEnergyInfo();

protected:
  virtual ~BluetoothResultHandler() { }
};

class BluetoothInterface
{
public:
  static BluetoothInterface* GetInstance();

  virtual void Init(BluetoothNotificationHandler* aNotificationHandler,
                    BluetoothResultHandler* aRes) = 0;
  virtual void Cleanup(BluetoothResultHandler* aRes) = 0;

  virtual void Enable(BluetoothResultHandler* aRes) = 0;
  virtual void Disable(BluetoothResultHandler* aRes) = 0;

  /* Adapter Properties */

  virtual void GetAdapterProperties(BluetoothResultHandler* aRes) = 0;
  virtual void GetAdapterProperty(BluetoothPropertyType,
                                  BluetoothResultHandler* aRes) = 0;
  virtual void SetAdapterProperty(const BluetoothProperty& aProperty,
                                  BluetoothResultHandler* aRes) = 0;

  /* Remote Device Properties */

  virtual void GetRemoteDeviceProperties(const BluetoothAddress& aRemoteAddr,
                                         BluetoothResultHandler* aRes) = 0;
  virtual void GetRemoteDeviceProperty(const BluetoothAddress& aRemoteAddr,
                                       BluetoothPropertyType aType,
                                       BluetoothResultHandler* aRes) = 0;
  virtual void SetRemoteDeviceProperty(const BluetoothAddress& aRemoteAddr,
                                       const BluetoothProperty& aProperty,
                                       BluetoothResultHandler* aRes) = 0;

  /* Remote Services */

  virtual void GetRemoteServiceRecord(const BluetoothAddress& aRemoteAddr,
                                      const BluetoothUuid& aUuid,
                                      BluetoothResultHandler* aRes) = 0;
  virtual void GetRemoteServices(const BluetoothAddress& aRemoteAddr,
                                 BluetoothResultHandler* aRes) = 0;

  /* Discovery */

  virtual void StartDiscovery(BluetoothResultHandler* aRes) = 0;
  virtual void CancelDiscovery(BluetoothResultHandler* aRes) = 0;

  /* Bonds */

  virtual void CreateBond(const BluetoothAddress& aBdAddr,
                          BluetoothTransport aTransport,
                          BluetoothResultHandler* aRes) = 0;
  virtual void RemoveBond(const BluetoothAddress& aBdAddr,
                          BluetoothResultHandler* aRes) = 0;
  virtual void CancelBond(const BluetoothAddress& aBdAddr,
                          BluetoothResultHandler* aRes) = 0;

  /* Connection */

  virtual void GetConnectionState(const BluetoothAddress& aBdAddr,
                                  BluetoothResultHandler* aRes) = 0;

  /* Authentication */

  virtual void PinReply(const BluetoothAddress& aBdAddr, bool aAccept,
                        const BluetoothPinCode& aPinCode,
                        BluetoothResultHandler* aRes) = 0;

  virtual void SspReply(const BluetoothAddress& aBdAddr,
                        BluetoothSspVariant aVariant,
                        bool aAccept, uint32_t aPasskey,
                        BluetoothResultHandler* aRes) = 0;

  /* DUT Mode */

  virtual void DutModeConfigure(bool aEnable,
                                BluetoothResultHandler* aRes) = 0;
  virtual void DutModeSend(uint16_t aOpcode, uint8_t* aBuf, uint8_t aLen,
                           BluetoothResultHandler* aRes) = 0;

  /* LE Mode */

  virtual void LeTestMode(uint16_t aOpcode, uint8_t* aBuf, uint8_t aLen,
                          BluetoothResultHandler* aRes) = 0;

  /* Energy Info */

  virtual void ReadEnergyInfo(BluetoothResultHandler* aRes) = 0;

  /* Profile Interfaces */

  virtual BluetoothSocketInterface* GetBluetoothSocketInterface() = 0;
  virtual BluetoothHandsfreeInterface* GetBluetoothHandsfreeInterface() = 0;
  virtual BluetoothA2dpInterface* GetBluetoothA2dpInterface() = 0;
  virtual BluetoothAvrcpInterface* GetBluetoothAvrcpInterface() = 0;
  virtual BluetoothGattInterface* GetBluetoothGattInterface() = 0;

protected:
  BluetoothInterface();
  virtual ~BluetoothInterface();
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_BluetoothInterface_h
