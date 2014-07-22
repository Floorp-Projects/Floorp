/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluedroid_bluetoothinterface_h__
#define mozilla_dom_bluetooth_bluedroid_bluetoothinterface_h__

#include <hardware/bluetooth.h>
#include <hardware/bt_sock.h>
#include <hardware/bt_hf.h>
#include <hardware/bt_av.h>
#if ANDROID_VERSION >= 18
#include <hardware/bt_rc.h>
#endif
#include "BluetoothCommon.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothInterface;

//
// Socket Interface
//

class BluetoothSocketResultHandler
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(BluetoothSocketResultHandler)

  virtual ~BluetoothSocketResultHandler() { }

  virtual void OnError(bt_status_t aStatus)
  {
    BT_WARNING("Received error code %d", (int)aStatus);
  }

  virtual void Listen(int aSockFd) { }
  virtual void Connect(int aSockFd, const nsAString& aBdAddress,
                       int aConnectionState) { }
  virtual void Accept(int aSockFd, const nsAString& aBdAddress,
                      int aConnectionState) { }
};

class BluetoothSocketInterface
{
public:
  friend class BluetoothInterface;

  // Init and Cleanup is handled by BluetoothInterface

  void Listen(btsock_type_t aType,
              const char* aServiceName, const uint8_t* aServiceUuid,
              int aChannel, int aFlags, BluetoothSocketResultHandler* aRes);

  void Connect(const bt_bdaddr_t* aBdAddr, btsock_type_t aType,
               const uint8_t* aUuid, int aChannel, int aFlags,
               BluetoothSocketResultHandler* aRes);

  void Accept(int aFd, BluetoothSocketResultHandler* aRes);

protected:
  BluetoothSocketInterface(const btsock_interface_t* aInterface);
  ~BluetoothSocketInterface();

private:
  const btsock_interface_t* mInterface;
};

//
// Handsfree Interface
//

class BluetoothHandsfreeResultHandler
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(BluetoothHandsfreeResultHandler)

  virtual ~BluetoothHandsfreeResultHandler() { }

  virtual void OnError(bt_status_t aStatus)
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
};

class BluetoothHandsfreeInterface
{
public:
  friend class BluetoothInterface;

  void Init(bthf_callbacks_t* aCallbacks,
            BluetoothHandsfreeResultHandler* aRes);
  void Cleanup(BluetoothHandsfreeResultHandler* aRes);

  /* Connect / Disconnect */

  void Connect(bt_bdaddr_t* aBdAddr,
               BluetoothHandsfreeResultHandler* aRes);
  void Disconnect(bt_bdaddr_t* aBdAddr,
                  BluetoothHandsfreeResultHandler* aRes);
  void ConnectAudio(bt_bdaddr_t* aBdAddr,
                    BluetoothHandsfreeResultHandler* aRes);
  void DisconnectAudio(bt_bdaddr_t* aBdAddr,
                       BluetoothHandsfreeResultHandler* aRes);

  /* Voice Recognition */

  bt_status_t StartVoiceRecognition();
  bt_status_t StopVoiceRecognition();

  /* Volume */

  bt_status_t VolumeControl(bthf_volume_type_t aType, int aVolume);

  /* Device status */

  bt_status_t DeviceStatusNotification(bthf_network_state_t aNtkState,
                                       bthf_service_type_t aSvcType,
                                       int aSignal, int aBattChg);

  /* Responses */

  bt_status_t CopsResponse(const char* aCops);
  bt_status_t CindResponse(int aSvc, int aNumActive, int aNumHeld,
                           bthf_call_state_t aCallSetupState, int aSignal,
                           int aRoam, int aBattChg);
  bt_status_t FormattedAtResponse(const char* aRsp);
  bt_status_t AtResponse(bthf_at_response_t aResponseCode, int aErrorCode);
  bt_status_t ClccResponse(int aIndex, bthf_call_direction_t aDir,
                           bthf_call_state_t aState, bthf_call_mode_t aMode,
                           bthf_call_mpty_type_t aMpty, const char* aNumber,
                           bthf_call_addrtype_t aType);

  /* Phone State */

  bt_status_t PhoneStateChange(int aNumActive, int aNumHeld,
                               bthf_call_state_t aCallSetupState,
                               const char* aNumber,
                               bthf_call_addrtype_t aType);

protected:
  BluetoothHandsfreeInterface(const bthf_interface_t* aInterface);
  ~BluetoothHandsfreeInterface();

private:
  const bthf_interface_t* mInterface;
};

//
// Bluetooth Advanced Audio Interface
//

class BluetoothA2dpInterface
{
public:
  friend class BluetoothInterface;

  bt_status_t Init(btav_callbacks_t *aCallbacks);
  void        Cleanup();

  bt_status_t Connect(bt_bdaddr_t *aBdAddr);
  bt_status_t Disconnect(bt_bdaddr_t *aBdAddr);

protected:
  BluetoothA2dpInterface(const btav_interface_t* aInterface);
  ~BluetoothA2dpInterface();

private:
  const btav_interface_t* mInterface;
};

//
// Bluetooth AVRCP Interface
//

class BluetoothAvrcpInterface
{
#if ANDROID_VERSION >= 18
public:
  friend class BluetoothInterface;

  bt_status_t Init(btrc_callbacks_t* aCallbacks);
  void        Cleanup();

  bt_status_t GetPlayStatusRsp(btrc_play_status_t aPlayStatus,
                               uint32_t aSongLen, uint32_t aSongPos);

  bt_status_t ListPlayerAppAttrRsp(int aNumAttr, btrc_player_attr_t* aPAttrs);
  bt_status_t ListPlayerAppValueRsp(int aNumVal, uint8_t* aPVals);

  bt_status_t GetPlayerAppValueRsp(btrc_player_settings_t* aPVals);
  bt_status_t GetPlayerAppAttrTextRsp(int aNumAttr,
                                      btrc_player_setting_text_t* aPAttrs);
  bt_status_t GetPlayerAppValueTextRsp(int aNumVal,
                                       btrc_player_setting_text_t* aPVals);

  bt_status_t GetElementAttrRsp(uint8_t aNumAttr,
                                btrc_element_attr_val_t* aPAttrs);

  bt_status_t SetPlayerAppValueRsp(btrc_status_t aRspStatus);

  bt_status_t RegisterNotificationRsp(btrc_event_id_t aEventId,
                                      btrc_notification_type_t aType,
                                      btrc_register_notification_t* aPParam);

  bt_status_t SetVolume(uint8_t aVolume);

protected:
  BluetoothAvrcpInterface(const btrc_interface_t* aInterface);
  ~BluetoothAvrcpInterface();

private:
  const btrc_interface_t* mInterface;
#endif
};

//
// Bluetooth Core Interface
//

class BluetoothResultHandler
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(BluetoothResultHandler)

  virtual ~BluetoothResultHandler() { }

  virtual void OnError(int aStatus)
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

  virtual void PinReply() { }
  virtual void SspReply() { }

  virtual void DutModeConfigure() { }
  virtual void DutModeSend() { }

  virtual void LeTestMode() { }
};

class BluetoothInterface
{
public:
  static BluetoothInterface* GetInstance();

  void Init(bt_callbacks_t* aCallbacks, BluetoothResultHandler* aRes);
  void Cleanup(BluetoothResultHandler* aRes);

  void Enable(BluetoothResultHandler* aRes);
  void Disable(BluetoothResultHandler* aRes);


  /* Adapter Properties */

  void GetAdapterProperties(BluetoothResultHandler* aRes);
  void GetAdapterProperty(bt_property_type_t aType,
                          BluetoothResultHandler* aRes);
  void SetAdapterProperty(const bt_property_t* aProperty,
                          BluetoothResultHandler* aRes);

  /* Remote Device Properties */

  void GetRemoteDeviceProperties(bt_bdaddr_t *aRemoteAddr,
                                 BluetoothResultHandler* aRes);
  void GetRemoteDeviceProperty(bt_bdaddr_t* aRemoteAddr,
                               bt_property_type_t aType,
                               BluetoothResultHandler* aRes);
  void SetRemoteDeviceProperty(bt_bdaddr_t* aRemoteAddr,
                               const bt_property_t* aProperty,
                               BluetoothResultHandler* aRes);

  /* Remote Services */

  void GetRemoteServiceRecord(bt_bdaddr_t* aRemoteAddr,
                              bt_uuid_t* aUuid,
                              BluetoothResultHandler* aRes);
  void GetRemoteServices(bt_bdaddr_t* aRemoteAddr,
                         BluetoothResultHandler* aRes);

  /* Discovery */

  void StartDiscovery(BluetoothResultHandler* aRes);
  void CancelDiscovery(BluetoothResultHandler* aRes);

  /* Bonds */

  void CreateBond(const bt_bdaddr_t* aBdAddr, BluetoothResultHandler* aRes);
  void RemoveBond(const bt_bdaddr_t* aBdAddr, BluetoothResultHandler* aRes);
  void CancelBond(const bt_bdaddr_t* aBdAddr, BluetoothResultHandler* aRes);

  /* Authentication */

  void PinReply(const bt_bdaddr_t* aBdAddr, uint8_t aAccept,
                uint8_t aPinLen, bt_pin_code_t* aPinCode,
                BluetoothResultHandler* aRes);

  void SspReply(const bt_bdaddr_t* aBdAddr, bt_ssp_variant_t aVariant,
                uint8_t aAccept, uint32_t aPasskey,
                BluetoothResultHandler* aRes);

  /* DUT Mode */

  void DutModeConfigure(uint8_t aEnable, BluetoothResultHandler* aRes);
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
  BluetoothInterface(const bt_interface_t* aInterface);
  ~BluetoothInterface();

private:
  template <class T>
  T* GetProfileInterface();

  const bt_interface_t* mInterface;
};

END_BLUETOOTH_NAMESPACE

#endif
