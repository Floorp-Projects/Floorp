/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
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

  virtual void SspReply(const nsAString& aBdAddr,
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

protected:
  BluetoothInterface();
  virtual ~BluetoothInterface();
};

END_BLUETOOTH_NAMESPACE

#endif
