/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothinterface_h__
#define mozilla_dom_bluetooth_bluetoothinterface_h__

#include "BluetoothCommon.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"

BEGIN_BLUETOOTH_NAMESPACE

//
// Socket Interface
//

class BluetoothSocketResultHandler
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(BluetoothSocketResultHandler)

  virtual void OnError(BluetoothStatus aStatus)
  {
    BT_WARNING("Received error code %d", (int)aStatus);
  }

  virtual void Listen(int aSockFd) { }
  virtual void Connect(int aSockFd, const nsAString& aBdAddress,
                       int aConnectionState) { }
  virtual void Accept(int aSockFd, const nsAString& aBdAddress,
                      int aConnectionState) { }

protected:
  virtual ~BluetoothSocketResultHandler() { }
};

class BluetoothSocketInterface
{
public:
  // Init and Cleanup is handled by BluetoothInterface

  virtual void Listen(BluetoothSocketType aType,
                      const nsAString& aServiceName,
                      const uint8_t aServiceUuid[16],
                      int aChannel, bool aEncrypt, bool aAuth,
                      BluetoothSocketResultHandler* aRes) = 0;

  virtual void Connect(const nsAString& aBdAddr,
                       BluetoothSocketType aType,
                       const uint8_t aUuid[16],
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
                              const nsAString& aBdAddr)
  { }

  virtual void
  AudioStateNotification(BluetoothHandsfreeAudioState aState,
                         const nsAString& aBdAddr)
  { }

  virtual void
  VoiceRecognitionNotification(BluetoothHandsfreeVoiceRecognitionState aState,
                               const nsAString& aBdAddr)
  { }

  virtual void
  AnswerCallNotification(const nsAString& aBdAddr)
  { }

  virtual void
  HangupCallNotification(const nsAString& aBdAddr)
  { }

  virtual void
  VolumeNotification(BluetoothHandsfreeVolumeType aType,
                     int aVolume,
                     const nsAString& aBdAddr)
  { }

  virtual void
  DialCallNotification(const nsAString& aNumber,
                       const nsAString& aBdAddr)
  { }

  virtual void
  DtmfNotification(char aDtmf,
                   const nsAString& aBdAddr)
  { }

  virtual void
  NRECNotification(BluetoothHandsfreeNRECState aNrec,
                   const nsAString& aBdAddr)
  { }

  virtual void
  WbsNotification(BluetoothHandsfreeWbsConfig aWbs,
                  const nsAString& aBdAddr)
  { }

  virtual void
  CallHoldNotification(BluetoothHandsfreeCallHoldType aChld,
                       const nsAString& aBdAddr)
  { }

  virtual void
  CnumNotification(const nsAString& aBdAddr)
  { }

  virtual void
  CindNotification(const nsAString& aBdAddr)
  { }

  virtual void
  CopsNotification(const nsAString& aBdAddr)
  { }

  virtual void
  ClccNotification(const nsAString& aBdAddr)
  { }

  virtual void
  UnknownAtNotification(const nsACString& aAtString,
                        const nsAString& aBdAddr)
  { }

  virtual void
  KeyPressedNotification(const nsAString& aBdAddr)
  { }

protected:
  BluetoothHandsfreeNotificationHandler()
  { }

  virtual ~BluetoothHandsfreeNotificationHandler();
};

class BluetoothHandsfreeResultHandler
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(BluetoothHandsfreeResultHandler)

  virtual void OnError(BluetoothStatus aStatus)
  {
    BT_WARNING("Received error code %d", (int)aStatus);
  }

  virtual void Init() { }
  virtual void Cleanup() { }

  virtual void Connect() { }
  virtual void Disconnect() { }
  virtual void ConnectAudio() { }
  virtual void DisconnectAudio() { }

  virtual void StartVoiceRecognition() { }
  virtual void StopVoiceRecognition() { }

  virtual void VolumeControl() { }

  virtual void DeviceStatusNotification() { }

  virtual void CopsResponse() { }
  virtual void CindResponse() { }
  virtual void FormattedAtResponse() { }
  virtual void AtResponse() { }
  virtual void ClccResponse() { }
  virtual void PhoneStateChange() { }

  virtual void ConfigureWbs() { }

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

  virtual void Connect(const nsAString& aBdAddr,
                       BluetoothHandsfreeResultHandler* aRes) = 0;
  virtual void Disconnect(const nsAString& aBdAddr,
                          BluetoothHandsfreeResultHandler* aRes) = 0;
  virtual void ConnectAudio(const nsAString& aBdAddr,
                            BluetoothHandsfreeResultHandler* aRes) = 0;
  virtual void DisconnectAudio(const nsAString& aBdAddr,
                               BluetoothHandsfreeResultHandler* aRes) = 0;

  /* Voice Recognition */

  virtual void StartVoiceRecognition(const nsAString& aBdAddr,
                                     BluetoothHandsfreeResultHandler* aRes) = 0;
  virtual void StopVoiceRecognition(const nsAString& aBdAddr,
                                    BluetoothHandsfreeResultHandler* aRes) = 0;

  /* Volume */

  virtual void VolumeControl(BluetoothHandsfreeVolumeType aType, int aVolume,
                             const nsAString& aBdAddr,
                             BluetoothHandsfreeResultHandler* aRes) = 0;

  /* Device status */

  virtual void DeviceStatusNotification(
    BluetoothHandsfreeNetworkState aNtkState,
    BluetoothHandsfreeServiceType aSvcType,
    int aSignal, int aBattChg, BluetoothHandsfreeResultHandler* aRes) = 0;

  /* Responses */

  virtual void CopsResponse(const char* aCops, const nsAString& aBdAddr,
                            BluetoothHandsfreeResultHandler* aRes) = 0;
  virtual void CindResponse(int aSvc, int aNumActive, int aNumHeld,
                            BluetoothHandsfreeCallState aCallSetupState,
                            int aSignal, int aRoam, int aBattChg,
                            const nsAString& aBdAddr,
                            BluetoothHandsfreeResultHandler* aRes) = 0;
  virtual void FormattedAtResponse(const char* aRsp, const nsAString& aBdAddr,
                                   BluetoothHandsfreeResultHandler* aRes) = 0;
  virtual void AtResponse(BluetoothHandsfreeAtResponse aResponseCode,
                          int aErrorCode, const nsAString& aBdAddr,
                          BluetoothHandsfreeResultHandler* aRes) = 0;
  virtual void ClccResponse(int aIndex, BluetoothHandsfreeCallDirection aDir,
                            BluetoothHandsfreeCallState aState,
                            BluetoothHandsfreeCallMode aMode,
                            BluetoothHandsfreeCallMptyType aMpty,
                            const nsAString& aNumber,
                            BluetoothHandsfreeCallAddressType aType,
                            const nsAString& aBdAddr,
                            BluetoothHandsfreeResultHandler* aRes) = 0;

  /* Phone State */

  virtual void PhoneStateChange(int aNumActive, int aNumHeld,
                                BluetoothHandsfreeCallState aCallSetupState,
                                const nsAString& aNumber,
                                BluetoothHandsfreeCallAddressType aType,
                                BluetoothHandsfreeResultHandler* aRes) = 0;

  /* Wide Band Speech */
  virtual void ConfigureWbs(const nsAString& aBdAddr,
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
                              const nsAString& aBdAddr)
  { }

  virtual void
  AudioStateNotification(BluetoothA2dpAudioState aState,
                         const nsAString& aBdAddr)
  { }

  virtual void
  AudioConfigNotification(const nsAString& aBdAddr,
                          uint32_t aSampleRate,
                          uint8_t aChannelCount)
  { }

protected:
  BluetoothA2dpNotificationHandler()
  { }

  virtual ~BluetoothA2dpNotificationHandler();
};

class BluetoothA2dpResultHandler
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(BluetoothA2dpResultHandler)

  virtual void OnError(BluetoothStatus aStatus)
  {
    BT_WARNING("Received error code %d", (int)aStatus);
  }

  virtual void Init() { }
  virtual void Cleanup() { }
  virtual void Connect() { }
  virtual void Disconnect() { }

protected:
  virtual ~BluetoothA2dpResultHandler() { }
};

class BluetoothA2dpInterface
{
public:
  virtual void Init(BluetoothA2dpNotificationHandler* aNotificationHandler,
                    BluetoothA2dpResultHandler* aRes) = 0;
  virtual void Cleanup(BluetoothA2dpResultHandler* aRes) = 0;

  virtual void Connect(const nsAString& aBdAddr,
                       BluetoothA2dpResultHandler* aRes) = 0;
  virtual void Disconnect(const nsAString& aBdAddr,
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
  GetPlayStatusNotification()
  { }

  virtual void
  ListPlayerAppAttrNotification()
  { }

  virtual void
  ListPlayerAppValuesNotification(BluetoothAvrcpPlayerAttribute aAttrId)
  { }

  virtual void
  GetPlayerAppValueNotification(uint8_t aNumAttrs,
                                const BluetoothAvrcpPlayerAttribute* aAttrs)
  { }

  virtual void
  GetPlayerAppAttrsTextNotification(uint8_t aNumAttrs,
                                    const BluetoothAvrcpPlayerAttribute* aAttrs)
  { }

  virtual void
  GetPlayerAppValuesTextNotification(uint8_t aAttrId, uint8_t aNumVals,
                                     const uint8_t* aValues)
  { }

  virtual void
  SetPlayerAppValueNotification(const BluetoothAvrcpPlayerSettings& aSettings)
  { }

  virtual void
  GetElementAttrNotification(uint8_t aNumAttrs,
                             const BluetoothAvrcpMediaAttribute* aAttrs)
  { }

  virtual void
  RegisterNotificationNotification(BluetoothAvrcpEvent aEvent,
                                   uint32_t aParam)
  { }

  virtual void
  RemoteFeatureNotification(const nsAString& aBdAddr, unsigned long aFeatures)
  { }

  virtual void
  VolumeChangeNotification(uint8_t aVolume, uint8_t aCType)
  { }

  virtual void
  PassthroughCmdNotification(int aId, int aKeyState)
  { }

protected:
  BluetoothAvrcpNotificationHandler()
  { }

  virtual ~BluetoothAvrcpNotificationHandler();
};

class BluetoothAvrcpResultHandler
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(BluetoothAvrcpResultHandler)

  virtual void OnError(BluetoothStatus aStatus)
  {
    BT_WARNING("Received error code %d", (int)aStatus);
  }

  virtual void Init() { }
  virtual void Cleanup() { }

  virtual void GetPlayStatusRsp() { }

  virtual void ListPlayerAppAttrRsp() { }
  virtual void ListPlayerAppValueRsp() { }

  virtual void GetPlayerAppValueRsp() { }
  virtual void GetPlayerAppAttrTextRsp() { }
  virtual void GetPlayerAppValueTextRsp() { }

  virtual void GetElementAttrRsp() { }

  virtual void SetPlayerAppValueRsp() { }

  virtual void RegisterNotificationRsp() { }

  virtual void SetVolume() { }

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

  virtual void SetVolume(uint8_t aVolume, BluetoothAvrcpResultHandler* aRes) = 0;

protected:
  BluetoothAvrcpInterface();
  virtual ~BluetoothAvrcpInterface();
};

//
// GATT Interface
//

class BluetoothGattClientNotificationHandler
{
public:
  virtual void
  RegisterClientNotification(BluetoothGattStatus aStatus,
                             int aClientIf,
                             const BluetoothUuid& aAppUuid)
  { }

  virtual void
  ScanResultNotification(const nsAString& aBdAddr,
                         int aRssi,
                         const BluetoothGattAdvData& aAdvData)
  { }

  virtual void
  ConnectNotification(int aConnId,
                      BluetoothGattStatus aStatus,
                      int aClientIf,
                      const nsAString& aBdAddr)
  { }

  virtual void
  DisconnectNotification(int aConnId,
                         BluetoothGattStatus aStatus,
                         int aClientIf,
                         const nsAString& aBdAddr)
  { }

  virtual void
  SearchCompleteNotification(int aConnId, BluetoothGattStatus aStatus) { }

  virtual void
  SearchResultNotification(int aConnId,
                           const BluetoothGattServiceId& aServiceId)
  { }

  virtual void
  GetCharacteristicNotification(int aConnId,
                                BluetoothGattStatus aStatus,
                                const BluetoothGattServiceId& aServiceId,
                                const BluetoothGattId& aCharId,
                                const BluetoothGattCharProp& aCharProperty)
  { }

  virtual void
  GetDescriptorNotification(int aConnId,
                            BluetoothGattStatus aStatus,
                            const BluetoothGattServiceId& aServiceId,
                            const BluetoothGattId& aCharId,
                            const BluetoothGattId& aDescriptorId)
  { }

  virtual void
  GetIncludedServiceNotification(int aConnId,
                                 BluetoothGattStatus aStatus,
                                 const BluetoothGattServiceId& aServiceId,
                                 const BluetoothGattServiceId& aIncludedServId)
  { }

  virtual void
  RegisterNotificationNotification(int aConnId,
                                   int aIsRegister,
                                   BluetoothGattStatus aStatus,
                                   const BluetoothGattServiceId& aServiceId,
                                   const BluetoothGattId& aCharId)
  { }

  virtual void
  NotifyNotification(int aConnId, const BluetoothGattNotifyParam& aNotifyParam)
  { }

  virtual void
  ReadCharacteristicNotification(int aConnId,
                                 BluetoothGattStatus aStatus,
                                 const BluetoothGattReadParam& aReadParam)
  { }

  virtual void
  WriteCharacteristicNotification(int aConnId,
                                  BluetoothGattStatus aStatus,
                                  const BluetoothGattWriteParam& aWriteParam)
  { }

  virtual void
  ReadDescriptorNotification(int aConnId,
                             BluetoothGattStatus aStatus,
                             const BluetoothGattReadParam& aReadParam)
  { }

  virtual void
  WriteDescriptorNotification(int aConnId,
                              BluetoothGattStatus aStatus,
                              const BluetoothGattWriteParam& aWriteParam)
  { }

  virtual void
  ExecuteWriteNotification(int aConnId, BluetoothGattStatus aStatus) { }

  virtual void
  ReadRemoteRssiNotification(int aClientIf,
                             const nsAString& aBdAddr,
                             int aRssi,
                             BluetoothGattStatus aStatus)
  { }

  virtual void
  ListenNotification(BluetoothGattStatus aStatus, int aServerIf) { }

protected:
  BluetoothGattClientNotificationHandler()
  { }

  virtual ~BluetoothGattClientNotificationHandler();
};

class BluetoothGattServerNotificationHandler
{
public:
  virtual void
  RegisterServerNotification(BluetoothGattStatus aStatus,
                             int aServerIf,
                             const BluetoothUuid& aAppUuid)
  { }

  virtual void
  ConnectionNotification(int aConnId,
                         int aServerIf,
                         bool aConnected,
                         const nsAString& aBdAddr)
  { }

  virtual void
  ServiceAddedNotification(BluetoothGattStatus aStatus,
                           int aServerIf,
                           const BluetoothGattServiceId& aServiceId,
                           int aServiceHandle)
  { }

  virtual void
  IncludedServiceAddedNotification(BluetoothGattStatus aStatus,
                                   int aServerIf,
                                   int aServiceHandle,
                                   int aIncludedServiceHandle)
  { }

  virtual void
  CharacteristicAddedNotification(BluetoothGattStatus aStatus,
                                  int aServerIf,
                                  const BluetoothUuid& aCharId,
                                  int aServiceHandle,
                                  int aCharacteristicHandle)
  { }

  virtual void
  DescriptorAddedNotification(BluetoothGattStatus aStatus,
                              int aServerIf,
                              const BluetoothUuid& aCharId,
                              int aServiceHandle,
                              int aDescriptorHandle)
  { }

  virtual void
  ServiceStartedNotification(BluetoothGattStatus aStatus,
                             int aServerIf,
                             int aServiceHandle)
  { }

  virtual void
  ServiceStoppedNotification(BluetoothGattStatus aStatus,
                             int aServerIf,
                             int aServiceHandle)
  { }

  virtual void
  ServiceDeletedNotification(BluetoothGattStatus aStatus,
                             int aServerIf,
                             int aServiceHandle)
  { }

  virtual void
  RequestReadNotification(int aConnId,
                          int aTransId,
                          const nsAString& aBdAddr,
                          int aAttributeHandle,
                          int aOffset,
                          bool aIsLong)
  { }

  virtual void
  RequestWriteNotification(int aConnId,
                           int aTransId,
                           const nsAString& aBdAddr,
                           int aAttributeHandle,
                           int aOffset,
                           int aLength,
                           const uint8_t* aValue,
                           bool aNeedResponse,
                           bool aIsPrepareWrite)
  { }

  virtual void
  RequestExecuteWriteNotification(int aConnId,
                                  int aTransId,
                                  const nsAString& aBdAddr,
                                  bool aExecute) /* true: execute */
                                                 /* false: cancel */
  { }

  virtual void
  ResponseConfirmationNotification(BluetoothGattStatus aStatus,
                                   int aHandle)
  { }

  virtual void
  IndicationSentNotification(int aConnId,
                             BluetoothGattStatus aStatus)
  { }

  virtual void
  CongestionNotification(int aConnId,
                         bool aCongested)
  { }

  virtual void
  MtuChangedNotification(int aConnId,
                         int aMtu)
  { }

protected:
  BluetoothGattServerNotificationHandler()
  { }

  virtual ~BluetoothGattServerNotificationHandler();
};

class BluetoothGattNotificationHandler
  : public BluetoothGattClientNotificationHandler
  , public BluetoothGattServerNotificationHandler
{
public:
  virtual ~BluetoothGattNotificationHandler();

protected:
  BluetoothGattNotificationHandler()
  { }
};

class BluetoothGattResultHandler
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(BluetoothGattResultHandler)

  virtual void OnError(BluetoothStatus aStatus)
  {
    BT_WARNING("Received error code %d", (int)aStatus);
  }

  virtual void Init() { }
  virtual void Cleanup() { }

protected:
  virtual ~BluetoothGattResultHandler() { }
};

class BluetoothGattClientResultHandler : public BluetoothGattResultHandler
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(BluetoothGattClientResultHandler)

  virtual void RegisterClient() { }
  virtual void UnregisterClient() { }

  virtual void Scan() { }

  virtual void Connect() { }
  virtual void Disconnect() { }

  virtual void Listen() { }
  virtual void Refresh() { }

  virtual void SearchService() { }
  virtual void GetIncludedService() { }
  virtual void GetCharacteristic() { }
  virtual void GetDescriptor() { }

  virtual void ReadCharacteristic() { }
  virtual void WriteCharacteristic() { }
  virtual void ReadDescriptor() { }
  virtual void WriteDescriptor() { }

  virtual void ExecuteWrite() { }

  virtual void RegisterNotification() { }
  virtual void DeregisterNotification() { }

  virtual void ReadRemoteRssi() { }
  virtual void GetDeviceType(BluetoothTypeOfDevice type) { }
  virtual void SetAdvData() { }
  virtual void TestCommand() { }

protected:
  virtual ~BluetoothGattClientResultHandler() { }
};

class BluetoothGattServerResultHandler : public BluetoothGattResultHandler
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(BluetoothGattServerResultHandler)

  virtual void RegisterServer() { }
  virtual void UnregisterServer() { }

  virtual void ConnectPeripheral() { }
  virtual void DisconnectPeripheral() { }

  virtual void AddService() { }
  virtual void AddIncludedService() { }
  virtual void AddCharacteristic() { }
  virtual void AddDescriptor() { }

  virtual void StartService() { }
  virtual void StopService() { }
  virtual void DeleteService() { }

  virtual void SendIndication() { }

  virtual void SendResponse() { }

protected:
  virtual ~BluetoothGattServerResultHandler() { }
};

class BluetoothGattClientInterface
{
public:
  /* Register / Unregister */
  virtual void RegisterClient(const BluetoothUuid& aUuid,
                              BluetoothGattClientResultHandler* aRes) = 0;
  virtual void UnregisterClient(int aClientIf,
                                BluetoothGattClientResultHandler* aRes) = 0;

  /* Start / Stop LE Scan */
  virtual void Scan(int aClientIf, bool aStart,
                    BluetoothGattClientResultHandler* aRes) = 0;

  /* Connect / Disconnect */
  virtual void Connect(int aClientIf,
                       const nsAString& aBdAddr,
                       bool aIsDirect, /* auto connect */
                       BluetoothTransport aTransport,
                       BluetoothGattClientResultHandler* aRes) = 0;
  virtual void Disconnect(int aClientIf,
                          const nsAString& aBdAddr,
                          int aConnId,
                          BluetoothGattClientResultHandler* aRes) = 0;

  /* Start / Stop advertisements to listen for incoming connections */
  virtual void Listen(int aClientIf,
                      bool aIsStart,
                      BluetoothGattClientResultHandler* aRes) = 0;

  /* Clear the attribute cache for a given device*/
  virtual void Refresh(int aClientIf,
                       const nsAString& aBdAddr,
                       BluetoothGattClientResultHandler* aRes) = 0;

  /* Enumerate Attributes */
  virtual void SearchService(int aConnId,
                             bool aSearchAll,
                             const BluetoothUuid& aUuid,
                             BluetoothGattClientResultHandler* aRes) = 0;
  virtual void GetIncludedService(
    int aConnId,
    const BluetoothGattServiceId& aServiceId,
    bool aFirst,
    const BluetoothGattServiceId& aStartServiceId,
    BluetoothGattClientResultHandler* aRes) = 0;
  virtual void GetCharacteristic(int aConnId,
                                 const BluetoothGattServiceId& aServiceId,
                                 bool aFirst,
                                 const BluetoothGattId& aStartCharId,
                                 BluetoothGattClientResultHandler* aRes) = 0;
  virtual void GetDescriptor(int aConnId,
                             const BluetoothGattServiceId& aServiceId,
                             const BluetoothGattId& aCharId,
                             bool aFirst,
                             const BluetoothGattId& aDescriptorId,
                             BluetoothGattClientResultHandler* aRes) = 0;

  /* Read / Write An Attribute */
  virtual void ReadCharacteristic(int aConnId,
                                  const BluetoothGattServiceId& aServiceId,
                                  const BluetoothGattId& aCharId,
                                  BluetoothGattAuthReq aAuthReq,
                                  BluetoothGattClientResultHandler* aRes) = 0;
  virtual void WriteCharacteristic(int aConnId,
                                   const BluetoothGattServiceId& aServiceId,
                                   const BluetoothGattId& aCharId,
                                   BluetoothGattWriteType aWriteType,
                                   BluetoothGattAuthReq aAuthReq,
                                   const nsTArray<uint8_t>& aValue,
                                   BluetoothGattClientResultHandler* aRes) = 0;
  virtual void ReadDescriptor(int aConnId,
                              const BluetoothGattServiceId& aServiceId,
                              const BluetoothGattId& aCharId,
                              const BluetoothGattId& aDescriptorId,
                              BluetoothGattAuthReq aAuthReq,
                              BluetoothGattClientResultHandler* aRes) = 0;
  virtual void WriteDescriptor(int aConnId,
                               const BluetoothGattServiceId& aServiceId,
                               const BluetoothGattId& aCharId,
                               const BluetoothGattId& aDescriptorId,
                               BluetoothGattWriteType aWriteType,
                               BluetoothGattAuthReq aAuthReq,
                               const nsTArray<uint8_t>& aValue,
                               BluetoothGattClientResultHandler* aRes) = 0;

  /* Execute / Abort Prepared Write*/
  virtual void ExecuteWrite(int aConnId,
                            int aIsExecute,
                            BluetoothGattClientResultHandler* aRes) = 0;


  /* Register / Deregister Characteristic Notifications or Indications */
  virtual void RegisterNotification(
    int aClientIf,
    const nsAString& aBdAddr,
    const BluetoothGattServiceId& aServiceId,
    const BluetoothGattId& aCharId,
    BluetoothGattClientResultHandler* aRes) = 0;
  virtual void DeregisterNotification(
    int aClientIf,
    const nsAString& aBdAddr,
    const BluetoothGattServiceId& aServiceId,
    const BluetoothGattId& aCharId,
    BluetoothGattClientResultHandler* aRes) = 0;

  virtual void ReadRemoteRssi(int aClientIf,
                              const nsAString& aBdAddr,
                              BluetoothGattClientResultHandler* aRes) = 0;

  virtual void GetDeviceType(const nsAString& aBdAddr,
                             BluetoothGattClientResultHandler* aRes) = 0;

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
                          BluetoothGattClientResultHandler* aRes) = 0;

  virtual void TestCommand(int aCommand,
                           const BluetoothGattTestParam& aTestParam,
                           BluetoothGattClientResultHandler* aRes) = 0;

protected:
  BluetoothGattClientInterface();
  virtual ~BluetoothGattClientInterface();
};

class BluetoothGattServerInterface
{
public:
  /* Register / Unregister */
  virtual void RegisterServer(const BluetoothUuid& aUuid,
                              BluetoothGattServerResultHandler* aRes) = 0;
  virtual void UnregisterServer(int aServerIf,
                                BluetoothGattServerResultHandler* aRes) = 0;

  /* Connect / Disconnect */
  virtual void ConnectPeripheral(int aServerIf,
                                 const nsAString& aBdAddr,
                                 bool aIsDirect, /* auto connect */
                                 BluetoothTransport aTransport,
                                 BluetoothGattServerResultHandler* aRes) = 0;
  virtual void DisconnectPeripheral(int aServerIf,
                                    const nsAString& aBdAddr,
                                    int aConnId,
                                    BluetoothGattServerResultHandler* aRes) = 0;

  /* Add a services / a characteristic / a descriptor */
  virtual void AddService(int aServerIf,
                          const BluetoothGattServiceId& aServiceId,
                          int aNumHandles,
                          BluetoothGattServerResultHandler* aRes) = 0;
  virtual void AddIncludedService(int aServerIf,
                                  int aServiceHandle,
                                  int aIncludedServiceHandle,
                                  BluetoothGattServerResultHandler* aRes) = 0;
  virtual void AddCharacteristic(int aServerIf,
                                 int aServiceHandle,
                                 const BluetoothUuid& aUuid,
                                 BluetoothGattCharProp aProperties,
                                 BluetoothGattAttrPerm aPermissions,
                                 BluetoothGattServerResultHandler* aRes) = 0;
  virtual void AddDescriptor(int aServerIf,
                             int aServiceHandle,
                             const BluetoothUuid& aUuid,
                             BluetoothGattAttrPerm aPermissions,
                             BluetoothGattServerResultHandler* aRes) = 0;

  /* Start / Stop / Delete a service */
  virtual void StartService(int aServerIf,
                            int aServiceHandle,
                            BluetoothTransport aTransport,
                            BluetoothGattServerResultHandler* aRes) = 0;
  virtual void StopService(int aServerIf,
                           int aServiceHandle,
                           BluetoothGattServerResultHandler* aRes) = 0;
  virtual void DeleteService(int aServerIf,
                             int aServiceHandle,
                             BluetoothGattServerResultHandler* aRes) = 0;

  /* Send an indication or a notification */
  virtual void SendIndication(int aServerIf,
                              int aAttributeHandle,
                              int aConnId,
                              const nsTArray<uint8_t>& aValue,
                              bool aConfirm, /* true: indication */
                                             /* false: notification */
                              BluetoothGattServerResultHandler* aRes) = 0;

  /* Send a response for an incoming indication */
  virtual void SendResponse(int aConnId,
                            int aTransId,
                            BluetoothGattStatus aStatus,
                            const BluetoothGattResponse& aResponse,
                            BluetoothGattServerResultHandler* aRes) = 0;

protected:
  BluetoothGattServerInterface();
  virtual ~BluetoothGattServerInterface();
};

class BluetoothGattInterface
{
public:
  virtual void Init(BluetoothGattNotificationHandler* aNotificationHandler,
                    BluetoothGattResultHandler* aRes) = 0;
  virtual void Cleanup(BluetoothGattResultHandler* aRes) = 0;

  virtual BluetoothGattClientInterface* GetBluetoothGattClientInterface() = 0;
  virtual BluetoothGattServerInterface* GetBluetoothGattServerInterface() = 0;

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
  virtual void AdapterStateChangedNotification(bool aState) { }
  virtual void AdapterPropertiesNotification(
    BluetoothStatus aStatus, int aNumProperties,
    const BluetoothProperty* aProperties) { }

  virtual void RemoteDevicePropertiesNotification(
    BluetoothStatus aStatus, const nsAString& aBdAddr,
    int aNumProperties, const BluetoothProperty* aProperties) { }

  virtual void DeviceFoundNotification(
    int aNumProperties, const BluetoothProperty* aProperties) { }

  virtual void DiscoveryStateChangedNotification(bool aState) { }

  virtual void PinRequestNotification(const nsAString& aRemoteBdAddr,
                                      const nsAString& aBdName, uint32_t aCod) { }
  virtual void SspRequestNotification(const nsAString& aRemoteBdAddr,
                                      const nsAString& aBdName,
                                      uint32_t aCod,
                                      BluetoothSspVariant aPairingVariant,
                                      uint32_t aPassKey) { }

  virtual void BondStateChangedNotification(BluetoothStatus aStatus,
                                            const nsAString& aRemoteBdAddr,
                                            BluetoothBondState aState) { }
  virtual void AclStateChangedNotification(BluetoothStatus aStatus,
                                           const nsAString& aRemoteBdAddr,
                                           bool aState) { }

  virtual void DutModeRecvNotification(uint16_t aOpcode,
                                       const uint8_t* aBuf, uint8_t aLen) { }
  virtual void LeTestModeNotification(BluetoothStatus aStatus,
                                      uint16_t aNumPackets) { }

  virtual void EnergyInfoNotification(const BluetoothActivityEnergyInfo& aInfo)
  { }

  virtual void BackendErrorNotification(bool aCrashed)
  { }

protected:
  BluetoothNotificationHandler()
  { }

  virtual ~BluetoothNotificationHandler();
};

class BluetoothResultHandler
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(BluetoothResultHandler)

  virtual void OnError(BluetoothStatus aStatus)
  {
    BT_LOGR("Received error code %d", aStatus);
  }

  virtual void Init() { }
  virtual void Cleanup() { }
  virtual void Enable() { }
  virtual void Disable() { }

  virtual void GetAdapterProperties() { }
  virtual void GetAdapterProperty() { }
  virtual void SetAdapterProperty() { }

  virtual void GetRemoteDeviceProperties() { }
  virtual void GetRemoteDeviceProperty() { }
  virtual void SetRemoteDeviceProperty() { }

  virtual void GetRemoteServiceRecord() { }
  virtual void GetRemoteServices() { }

  virtual void StartDiscovery() { }
  virtual void CancelDiscovery() { }

  virtual void CreateBond() { }
  virtual void RemoveBond() { }
  virtual void CancelBond() { }

  virtual void GetConnectionState() { }

  virtual void PinReply() { }
  virtual void SspReply() { }

  virtual void DutModeConfigure() { }
  virtual void DutModeSend() { }

  virtual void LeTestMode() { }

  virtual void ReadEnergyInfo() { }

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
  virtual void GetAdapterProperty(const nsAString& aName,
                                  BluetoothResultHandler* aRes) = 0;
  virtual void SetAdapterProperty(const BluetoothNamedValue& aProperty,
                                  BluetoothResultHandler* aRes) = 0;

  /* Remote Device Properties */

  virtual void GetRemoteDeviceProperties(const nsAString& aRemoteAddr,
                                         BluetoothResultHandler* aRes) = 0;
  virtual void GetRemoteDeviceProperty(const nsAString& aRemoteAddr,
                                       const nsAString& aName,
                                       BluetoothResultHandler* aRes) = 0;
  virtual void SetRemoteDeviceProperty(const nsAString& aRemoteAddr,
                                       const BluetoothNamedValue& aProperty,
                                       BluetoothResultHandler* aRes) = 0;

  /* Remote Services */

  virtual void GetRemoteServiceRecord(const nsAString& aRemoteAddr,
                                      const uint8_t aUuid[16],
                                      BluetoothResultHandler* aRes) = 0;
  virtual void GetRemoteServices(const nsAString& aRemoteAddr,
                                 BluetoothResultHandler* aRes) = 0;

  /* Discovery */

  virtual void StartDiscovery(BluetoothResultHandler* aRes) = 0;
  virtual void CancelDiscovery(BluetoothResultHandler* aRes) = 0;

  /* Bonds */

  virtual void CreateBond(const nsAString& aBdAddr,
                          BluetoothTransport aTransport,
                          BluetoothResultHandler* aRes) = 0;
  virtual void RemoveBond(const nsAString& aBdAddr,
                          BluetoothResultHandler* aRes) = 0;
  virtual void CancelBond(const nsAString& aBdAddr,
                          BluetoothResultHandler* aRes) = 0;

  /* Connection */

  virtual void GetConnectionState(const nsAString& aBdAddr,
                                  BluetoothResultHandler* aRes) = 0;

  /* Authentication */

  virtual void PinReply(const nsAString& aBdAddr, bool aAccept,
                        const nsAString& aPinCode,
                        BluetoothResultHandler* aRes) = 0;

  virtual void SspReply(const nsAString& aBdAddr, BluetoothSspVariant aVariant,
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

#endif
