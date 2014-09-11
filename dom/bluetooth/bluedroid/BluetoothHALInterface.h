/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluedroid_bluetoothhalinterface_h__
#define mozilla_dom_bluetooth_bluedroid_bluetoothhalinterface_h__

#include <hardware/bluetooth.h>
#include <hardware/bt_sock.h>
#include <hardware/bt_hf.h>
#include <hardware/bt_av.h>
#if ANDROID_VERSION >= 18
#include <hardware/bt_rc.h>
#endif
#include "BluetoothInterface.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothHALInterface;

//
// Socket Interface
//

class BluetoothSocketHALInterface MOZ_FINAL
  : public BluetoothSocketInterface
{
public:
  friend class BluetoothHALInterface;

  void Listen(BluetoothSocketType aType,
              const nsAString& aServiceName,
              const uint8_t aServiceUuid[16],
              int aChannel, bool aEncrypt, bool aAuth,
              BluetoothSocketResultHandler* aRes);

  void Connect(const nsAString& aBdAddr,
               BluetoothSocketType aType,
               const uint8_t aUuid[16],
               int aChannel, bool aEncrypt, bool aAuth,
               BluetoothSocketResultHandler* aRes);

  void Accept(int aFd, BluetoothSocketResultHandler* aRes);

protected:
  BluetoothSocketHALInterface(const btsock_interface_t* aInterface);
  ~BluetoothSocketHALInterface();

private:
  const btsock_interface_t* mInterface;
};

//
// Handsfree Interface
//

class BluetoothHandsfreeHALInterface MOZ_FINAL
  : public BluetoothHandsfreeInterface
{
public:
  friend class BluetoothHALInterface;

  void Init(BluetoothHandsfreeNotificationHandler* aNotificationHandler,
            BluetoothHandsfreeResultHandler* aRes);
  void Cleanup(BluetoothHandsfreeResultHandler* aRes);

  /* Connect / Disconnect */

  void Connect(const nsAString& aBdAddr,
               BluetoothHandsfreeResultHandler* aRes);
  void Disconnect(const nsAString& aBdAddr,
                  BluetoothHandsfreeResultHandler* aRes);
  void ConnectAudio(const nsAString& aBdAddr,
                    BluetoothHandsfreeResultHandler* aRes);
  void DisconnectAudio(const nsAString& aBdAddr,
                       BluetoothHandsfreeResultHandler* aRes);

  /* Voice Recognition */

  void StartVoiceRecognition(BluetoothHandsfreeResultHandler* aRes);
  void StopVoiceRecognition(BluetoothHandsfreeResultHandler* aRes);

  /* Volume */

  void VolumeControl(BluetoothHandsfreeVolumeType aType, int aVolume,
                     BluetoothHandsfreeResultHandler* aRes);

  /* Device status */

  void DeviceStatusNotification(BluetoothHandsfreeNetworkState aNtkState,
                                BluetoothHandsfreeServiceType aSvcType,
                                int aSignal, int aBattChg,
                                BluetoothHandsfreeResultHandler* aRes);

  /* Responses */

  void CopsResponse(const char* aCops,
                    BluetoothHandsfreeResultHandler* aRes);
  void CindResponse(int aSvc, int aNumActive, int aNumHeld,
                    BluetoothHandsfreeCallState aCallSetupState, int aSignal,
                    int aRoam, int aBattChg,
                    BluetoothHandsfreeResultHandler* aRes);
  void FormattedAtResponse(const char* aRsp,
                           BluetoothHandsfreeResultHandler* aRes);
  void AtResponse(BluetoothHandsfreeAtResponse aResponseCode, int aErrorCode,
                  BluetoothHandsfreeResultHandler* aRes);
  void ClccResponse(int aIndex, BluetoothHandsfreeCallDirection aDir,
                    BluetoothHandsfreeCallState aState,
                    BluetoothHandsfreeCallMode aMode,
                    BluetoothHandsfreeCallMptyType aMpty,
                    const nsAString& aNumber,
                    BluetoothHandsfreeCallAddressType aType,
                    BluetoothHandsfreeResultHandler* aRes);

  /* Phone State */

  void PhoneStateChange(int aNumActive, int aNumHeld,
                        BluetoothHandsfreeCallState aCallSetupState,
                        const nsAString& aNumber,
                        BluetoothHandsfreeCallAddressType aType,
                        BluetoothHandsfreeResultHandler* aRes);

protected:
  BluetoothHandsfreeHALInterface(const bthf_interface_t* aInterface);
  ~BluetoothHandsfreeHALInterface();

private:
  const bthf_interface_t* mInterface;
};

//
// Bluetooth Advanced Audio Interface
//

class BluetoothA2dpHALInterface MOZ_FINAL
  : public BluetoothA2dpInterface
{
public:
  friend class BluetoothHALInterface;

  void Init(BluetoothA2dpNotificationHandler* aNotificationHandler,
            BluetoothA2dpResultHandler* aRes);
  void Cleanup(BluetoothA2dpResultHandler* aRes);

  void Connect(const nsAString& aBdAddr,
               BluetoothA2dpResultHandler* aRes);
  void Disconnect(const nsAString& aBdAddr,
                  BluetoothA2dpResultHandler* aRes);

protected:
  BluetoothA2dpHALInterface(const btav_interface_t* aInterface);
  ~BluetoothA2dpHALInterface();

private:
  const btav_interface_t* mInterface;
};

//
// Bluetooth AVRCP Interface
//

class BluetoothAvrcpHALInterface MOZ_FINAL
  : public BluetoothAvrcpInterface
{
public:
  friend class BluetoothHALInterface;

  void Init(BluetoothAvrcpNotificationHandler* aNotificationHandler,
            BluetoothAvrcpResultHandler* aRes);
  void Cleanup(BluetoothAvrcpResultHandler* aRes);

  void GetPlayStatusRsp(ControlPlayStatus aPlayStatus,
                        uint32_t aSongLen, uint32_t aSongPos,
                        BluetoothAvrcpResultHandler* aRes);

  void ListPlayerAppAttrRsp(int aNumAttr,
                            const BluetoothAvrcpPlayerAttribute* aPAttrs,
                            BluetoothAvrcpResultHandler* aRes);
  void ListPlayerAppValueRsp(int aNumVal, uint8_t* aPVals,
                             BluetoothAvrcpResultHandler* aRes);

  /* TODO: redesign this interface once we actually use it */
  void GetPlayerAppValueRsp(uint8_t aNumAttrs,
                            const uint8_t* aIds, const uint8_t* aValues,
                            BluetoothAvrcpResultHandler* aRes);
  /* TODO: redesign this interface once we actually use it */
  void GetPlayerAppAttrTextRsp(int aNumAttr,
                               const uint8_t* aIds, const char** aTexts,
                               BluetoothAvrcpResultHandler* aRes);
  /* TODO: redesign this interface once we actually use it */
  void GetPlayerAppValueTextRsp(int aNumVal,
                                const uint8_t* aIds, const char** aTexts,
                                BluetoothAvrcpResultHandler* aRes);

  void GetElementAttrRsp(uint8_t aNumAttr,
                         const BluetoothAvrcpElementAttribute* aAttr,
                         BluetoothAvrcpResultHandler* aRes);

  void SetPlayerAppValueRsp(BluetoothAvrcpStatus aRspStatus,
                            BluetoothAvrcpResultHandler* aRes);

  void RegisterNotificationRsp(BluetoothAvrcpEvent aEvent,
                               BluetoothAvrcpNotification aType,
                               const BluetoothAvrcpNotificationParam& aParam,
                               BluetoothAvrcpResultHandler* aRes);

  void SetVolume(uint8_t aVolume, BluetoothAvrcpResultHandler* aRes);

protected:
  BluetoothAvrcpHALInterface(
#if ANDROID_VERSION >= 18
    const btrc_interface_t* aInterface
#endif
    );
  ~BluetoothAvrcpHALInterface();

private:
#if ANDROID_VERSION >= 18
  const btrc_interface_t* mInterface;
#endif
};

//
// Bluetooth Core Interface
//

class BluetoothHALInterface MOZ_FINAL : public BluetoothInterface
{
public:
  static BluetoothHALInterface* GetInstance();

  void Init(BluetoothNotificationHandler* aNotificationHandler,
            BluetoothResultHandler* aRes);
  void Cleanup(BluetoothResultHandler* aRes);

  void Enable(BluetoothResultHandler* aRes);
  void Disable(BluetoothResultHandler* aRes);


  /* Adapter Properties */

  void GetAdapterProperties(BluetoothResultHandler* aRes);
  void GetAdapterProperty(const nsAString& aName,
                          BluetoothResultHandler* aRes);
  void SetAdapterProperty(const BluetoothNamedValue& aProperty,
                          BluetoothResultHandler* aRes);

  /* Remote Device Properties */

  void GetRemoteDeviceProperties(const nsAString& aRemoteAddr,
                                 BluetoothResultHandler* aRes);
  void GetRemoteDeviceProperty(const nsAString& aRemoteAddr,
                               const nsAString& aName,
                               BluetoothResultHandler* aRes);
  void SetRemoteDeviceProperty(const nsAString& aRemoteAddr,
                               const BluetoothNamedValue& aProperty,
                               BluetoothResultHandler* aRes);

  /* Remote Services */

  void GetRemoteServiceRecord(const nsAString& aRemoteAddr,
                              const uint8_t aUuid[16],
                              BluetoothResultHandler* aRes);
  void GetRemoteServices(const nsAString& aRemoteAddr,
                         BluetoothResultHandler* aRes);

  /* Discovery */

  void StartDiscovery(BluetoothResultHandler* aRes);
  void CancelDiscovery(BluetoothResultHandler* aRes);

  /* Bonds */

  void CreateBond(const nsAString& aBdAddr, BluetoothResultHandler* aRes);
  void RemoveBond(const nsAString& aBdAddr, BluetoothResultHandler* aRes);
  void CancelBond(const nsAString& aBdAddr, BluetoothResultHandler* aRes);

  /* Authentication */

  void PinReply(const nsAString& aBdAddr, bool aAccept,
                const nsAString& aPinCode,
                BluetoothResultHandler* aRes);

  void SspReply(const nsAString& aBdAddr, const nsAString& aVariant,
                bool aAccept, uint32_t aPasskey,
                BluetoothResultHandler* aRes);

  /* DUT Mode */

  void DutModeConfigure(bool aEnable, BluetoothResultHandler* aRes);
  void DutModeSend(uint16_t aOpcode, uint8_t* aBuf, uint8_t aLen,
                   BluetoothResultHandler* aRes);

  /* LE Mode */

  void LeTestMode(uint16_t aOpcode, uint8_t* aBuf, uint8_t aLen,
                  BluetoothResultHandler* aRes);

  /* Profile Interfaces */

  BluetoothSocketInterface* GetBluetoothSocketInterface();
  BluetoothHandsfreeInterface* GetBluetoothHandsfreeInterface();
  BluetoothA2dpInterface* GetBluetoothA2dpInterface();
  BluetoothAvrcpInterface* GetBluetoothAvrcpInterface();

protected:
  BluetoothHALInterface(const bt_interface_t* aInterface);
  ~BluetoothHALInterface();

private:
  template <class T>
  T* CreateProfileInterface();

  template <class T>
  T* GetProfileInterface();

  const bt_interface_t* mInterface;
};

END_BLUETOOTH_NAMESPACE

#endif
