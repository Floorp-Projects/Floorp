/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothInterface.h"

BEGIN_BLUETOOTH_NAMESPACE

template<class T>
struct interface_traits
{ };

//
// Socket Interface
//

template<>
struct interface_traits<BluetoothSocketInterface>
{
  typedef const btsock_interface_t const_interface_type;

  static const char* profile_id()
  {
    return BT_PROFILE_SOCKETS_ID;
  }
};

bt_status_t
BluetoothSocketInterface::Listen(btsock_type_t aType,
                                 const char* aServiceName,
                                 const uint8_t* aServiceUuid, int aChannel,
                                 int& aSockFd, int aFlags)
{
  return mInterface->listen(aType, aServiceName, aServiceUuid, aChannel,
                           &aSockFd, aFlags);
}

bt_status_t
BluetoothSocketInterface::Connect(const bt_bdaddr_t* aBdAddr,
                                  btsock_type_t aType, const uint8_t* aUuid,
                                  int aChannel, int& aSockFd, int aFlags)
{
  return mInterface->connect(aBdAddr, aType, aUuid, aChannel, &aSockFd,
                             aFlags);
}

BluetoothSocketInterface::BluetoothSocketInterface(
  const btsock_interface_t* aInterface)
: mInterface(aInterface)
{
  MOZ_ASSERT(mInterface);
}

BluetoothSocketInterface::~BluetoothSocketInterface()
{ }

//
// Handsfree Interface
//

template<>
struct interface_traits<BluetoothHandsfreeInterface>
{
  typedef const bthf_interface_t const_interface_type;

  static const char* profile_id()
  {
    return BT_PROFILE_HANDSFREE_ID;
  }
};

BluetoothHandsfreeInterface::BluetoothHandsfreeInterface(
  const bthf_interface_t* aInterface)
: mInterface(aInterface)
{
  MOZ_ASSERT(mInterface);
}

BluetoothHandsfreeInterface::~BluetoothHandsfreeInterface()
{ }

bt_status_t
BluetoothHandsfreeInterface::Init(bthf_callbacks_t* aCallbacks)
{
  return mInterface->init(aCallbacks);
}

void
BluetoothHandsfreeInterface::Cleanup()
{
  mInterface->cleanup();
}

/* Connect / Disconnect */

bt_status_t
BluetoothHandsfreeInterface::Connect(bt_bdaddr_t* aBdAddr)
{
  return mInterface->connect(aBdAddr);
}

bt_status_t
BluetoothHandsfreeInterface::Disconnect(bt_bdaddr_t* aBdAddr)
{
  return mInterface->disconnect(aBdAddr);
}

bt_status_t
BluetoothHandsfreeInterface::ConnectAudio(bt_bdaddr_t* aBdAddr)
{
  return mInterface->connect_audio(aBdAddr);
}

bt_status_t
BluetoothHandsfreeInterface::DisconnectAudio(bt_bdaddr_t* aBdAddr)
{
  return mInterface->disconnect_audio(aBdAddr);
}

/* Voice Recognition */

bt_status_t
BluetoothHandsfreeInterface::StartVoiceRecognition()
{
  return mInterface->start_voice_recognition();
}

bt_status_t
BluetoothHandsfreeInterface::StopVoiceRecognition()
{
  return mInterface->stop_voice_recognition();
}

/* Volume */

bt_status_t
BluetoothHandsfreeInterface::VolumeControl(bthf_volume_type_t aType,
                                           int aVolume)
{
  return mInterface->volume_control(aType, aVolume);
}

/* Device status */

bt_status_t
BluetoothHandsfreeInterface::DeviceStatusNotification(
  bthf_network_state_t aNtkState, bthf_service_type_t aSvcType, int aSignal,
  int aBattChg)
{
  return mInterface->device_status_notification(aNtkState, aSvcType, aSignal,
                                                aBattChg);
}

/* Responses */

bt_status_t
BluetoothHandsfreeInterface::CopsResponse(const char* aCops)
{
  return mInterface->cops_response(aCops);
}

bt_status_t
BluetoothHandsfreeInterface::CindResponse(int aSvc, int aNumActive,
                                          int aNumHeld,
                                          bthf_call_state_t aCallSetupState,
                                          int aSignal, int aRoam, int aBattChg)
{
  return mInterface->cind_response(aSvc, aNumActive, aNumHeld,
                                   aCallSetupState, aSignal, aRoam,
                                   aBattChg);
}

bt_status_t
BluetoothHandsfreeInterface::FormattedAtResponse(const char* aRsp)
{
  return mInterface->formatted_at_response(aRsp);
}

bt_status_t
BluetoothHandsfreeInterface::AtResponse(bthf_at_response_t aResponseCode,
                                        int aErrorCode)
{
  return mInterface->at_response(aResponseCode, aErrorCode);
}

bt_status_t
BluetoothHandsfreeInterface::ClccResponse(int aIndex,
                                          bthf_call_direction_t aDir,
                                          bthf_call_state_t aState,
                                          bthf_call_mode_t aMode,
                                          bthf_call_mpty_type_t aMpty,
                                          const char* aNumber,
                                          bthf_call_addrtype_t aType)
{
  return mInterface->clcc_response(aIndex, aDir, aState, aMode, aMpty,
                                   aNumber, aType);
}

/* Phone State */

bt_status_t
BluetoothHandsfreeInterface::PhoneStateChange(int aNumActive, int aNumHeld,
  bthf_call_state_t aCallSetupState, const char* aNumber,
  bthf_call_addrtype_t aType)
{
  return mInterface->phone_state_change(aNumActive, aNumHeld, aCallSetupState,
                                        aNumber, aType);
}

//
// Bluetooth Advanced Audio Interface
//

template<>
struct interface_traits<BluetoothA2dpInterface>
{
  typedef const btav_interface_t const_interface_type;

  static const char* profile_id()
  {
    return BT_PROFILE_ADVANCED_AUDIO_ID;
  }
};

BluetoothA2dpInterface::BluetoothA2dpInterface(
  const btav_interface_t* aInterface)
: mInterface(aInterface)
{
  MOZ_ASSERT(mInterface);
}

BluetoothA2dpInterface::~BluetoothA2dpInterface()
{ }

bt_status_t
BluetoothA2dpInterface::Init(btav_callbacks_t* aCallbacks)
{
  return mInterface->init(aCallbacks);
}

void
BluetoothA2dpInterface::Cleanup()
{
  mInterface->cleanup();
}

bt_status_t
BluetoothA2dpInterface::Connect(bt_bdaddr_t *aBdAddr)
{
  return mInterface->connect(aBdAddr);
}

bt_status_t
BluetoothA2dpInterface::Disconnect(bt_bdaddr_t *aBdAddr)
{
  return mInterface->disconnect(aBdAddr);
}

//
// Bluetooth AVRCP Interface
//

#if ANDROID_VERSION >= 18
template<>
struct interface_traits<BluetoothAvrcpInterface>
{
  typedef const btrc_interface_t const_interface_type;

  static const char* profile_id()
  {
    return BT_PROFILE_AV_RC_ID;
  }
};

BluetoothAvrcpInterface::BluetoothAvrcpInterface(
  const btrc_interface_t* aInterface)
: mInterface(aInterface)
{
  MOZ_ASSERT(mInterface);
}

BluetoothAvrcpInterface::~BluetoothAvrcpInterface()
{ }

bt_status_t
BluetoothAvrcpInterface::Init(btrc_callbacks_t* aCallbacks)
{
  return mInterface->init(aCallbacks);
}

void
BluetoothAvrcpInterface::Cleanup()
{
  mInterface->cleanup();
}

bt_status_t
BluetoothAvrcpInterface::GetPlayStatusRsp(btrc_play_status_t aPlayStatus,
                                          uint32_t aSongLen, uint32_t aSongPos)
{
  return mInterface->get_play_status_rsp(aPlayStatus, aSongLen, aSongPos);
}

bt_status_t
BluetoothAvrcpInterface::ListPlayerAppAttrRsp(int aNumAttr,
                                              btrc_player_attr_t* aPAttrs)
{
  return mInterface->list_player_app_attr_rsp(aNumAttr, aPAttrs);
}

bt_status_t
BluetoothAvrcpInterface::ListPlayerAppValueRsp(int aNumVal, uint8_t* aPVals)
{
  return mInterface->list_player_app_value_rsp(aNumVal, aPVals);
}

bt_status_t
BluetoothAvrcpInterface::GetPlayerAppValueRsp(btrc_player_settings_t* aPVals)
{
  return mInterface->get_player_app_value_rsp(aPVals);
}

bt_status_t
BluetoothAvrcpInterface::GetPlayerAppAttrTextRsp(int aNumAttr,
  btrc_player_setting_text_t* aPAttrs)
{
  return mInterface->get_player_app_attr_text_rsp(aNumAttr, aPAttrs);
}

bt_status_t
BluetoothAvrcpInterface::GetPlayerAppValueTextRsp(int aNumVal,
  btrc_player_setting_text_t* aPVals)
{
  return mInterface->get_player_app_value_text_rsp(aNumVal, aPVals);
}

bt_status_t
BluetoothAvrcpInterface::GetElementAttrRsp(uint8_t aNumAttr,
                                           btrc_element_attr_val_t* aPAttrs)
{
  return mInterface->get_element_attr_rsp(aNumAttr, aPAttrs);
}

bt_status_t
BluetoothAvrcpInterface::SetPlayerAppValueRsp(btrc_status_t aRspStatus)
{
  return mInterface->set_player_app_value_rsp(aRspStatus);
}

bt_status_t
BluetoothAvrcpInterface::RegisterNotificationRsp(btrc_event_id_t aEventId,
  btrc_notification_type_t aType, btrc_register_notification_t* aPParam)
{
  return mInterface->register_notification_rsp(aEventId, aType, aPParam);
}

bt_status_t
BluetoothAvrcpInterface::SetVolume(uint8_t aVolume)
{
#if ANDROID_VERSION >= 19
  return mInterface->set_volume(aVolume);
#else
  return BT_STATUS_UNSUPPORTED;
#endif
}
#endif // ANDROID_VERSION >= 18

//
// Bluetooth Core Interface
//

/* returns the container structure of a variable; _t is the container's
 * type, _v the name of the variable, and _m is _v's field within _t
 */
#define container(_t, _v, _m) \
  ( (_t*)( ((const unsigned char*)(_v)) - offsetof(_t, _m) ) )

BluetoothInterface*
BluetoothInterface::GetInstance()
{
  static BluetoothInterface* sBluetoothInterface;

  if (sBluetoothInterface) {
    return sBluetoothInterface;
  }

  /* get driver module */

  const hw_module_t* module;
  int err = hw_get_module(BT_HARDWARE_MODULE_ID, &module);
  if (err) {
    BT_WARNING("hw_get_module failed: %s", strerror(err));
    return nullptr;
  }

  /* get device */

  hw_device_t* device;
  err = module->methods->open(module, BT_HARDWARE_MODULE_ID, &device);
  if (err) {
    BT_WARNING("open failed: %s", strerror(err));
    return nullptr;
  }

  const bluetooth_device_t* bt_device =
    container(bluetooth_device_t, device, common);

  /* get interface */

  const bt_interface_t* bt_interface = bt_device->get_bluetooth_interface();
  if (!bt_interface) {
    BT_WARNING("get_bluetooth_interface failed");
    goto err_get_bluetooth_interface;
  }

  if (bt_interface->size != sizeof(*bt_interface)) {
    BT_WARNING("interface of incorrect size");
    goto err_bt_interface_size;
  }

  sBluetoothInterface = new BluetoothInterface(bt_interface);

  return sBluetoothInterface;

err_bt_interface_size:
err_get_bluetooth_interface:
  err = device->close(device);
  if (err) {
    BT_WARNING("close failed: %s", strerror(err));
  }
  return nullptr;
}

BluetoothInterface::BluetoothInterface(const bt_interface_t* aInterface)
: mInterface(aInterface)
{
  MOZ_ASSERT(mInterface);
}

BluetoothInterface::~BluetoothInterface()
{ }

int
BluetoothInterface::Init(bt_callbacks_t* aCallbacks)
{
  return mInterface->init(aCallbacks);
}

void
BluetoothInterface::Cleanup()
{
  mInterface->cleanup();
}

int
BluetoothInterface::Enable()
{
  return mInterface->enable();
}

int
BluetoothInterface::Disable()
{
  return mInterface->disable();
}

/* Adapter Properties */

int
BluetoothInterface::GetAdapterProperties()
{
  return mInterface->get_adapter_properties();
}

int
BluetoothInterface::GetAdapterProperty(bt_property_type_t aType)
{
  return mInterface->get_adapter_property(aType);
}

int
BluetoothInterface::SetAdapterProperty(const bt_property_t* aProperty)
{
  return mInterface->set_adapter_property(aProperty);
}

/* Remote Device Properties */

int
BluetoothInterface::GetRemoteDeviceProperties(bt_bdaddr_t *aRemoteAddr)
{
  return mInterface->get_remote_device_properties(aRemoteAddr);
}

int
BluetoothInterface::GetRemoteDeviceProperty(bt_bdaddr_t* aRemoteAddr,
                                            bt_property_type_t aType)
{
  return mInterface->get_remote_device_property(aRemoteAddr, aType);
}

int
BluetoothInterface::SetRemoteDeviceProperty(bt_bdaddr_t* aRemoteAddr,
                                            const bt_property_t* aProperty)
{
  return mInterface->set_remote_device_property(aRemoteAddr, aProperty);
}

/* Remote Services */

int
BluetoothInterface::GetRemoteServiceRecord(bt_bdaddr_t* aRemoteAddr,
                                           bt_uuid_t* aUuid)
{
  return mInterface->get_remote_service_record(aRemoteAddr, aUuid);
}

int
BluetoothInterface::GetRemoteServices(bt_bdaddr_t* aRemoteAddr)
{
  return mInterface->get_remote_services(aRemoteAddr);
}

/* Discovery */

int
BluetoothInterface::StartDiscovery()
{
  return mInterface->start_discovery();
}

int
BluetoothInterface::CancelDiscovery()
{
  return mInterface->cancel_discovery();
}

/* Bonds */

int
BluetoothInterface::CreateBond(const bt_bdaddr_t* aBdAddr)
{
  return mInterface->create_bond(aBdAddr);
}

int
BluetoothInterface::RemoveBond(const bt_bdaddr_t* aBdAddr)
{
  return mInterface->remove_bond(aBdAddr);
}

int
BluetoothInterface::CancelBond(const bt_bdaddr_t* aBdAddr)
{
  return mInterface->cancel_bond(aBdAddr);
}

/* Authentication */

int
BluetoothInterface::PinReply(const bt_bdaddr_t* aBdAddr, uint8_t aAccept,
                             uint8_t aPinLen, bt_pin_code_t* aPinCode)
{
  return mInterface->pin_reply(aBdAddr, aAccept, aPinLen, aPinCode);
}

int
BluetoothInterface::SspReply(const bt_bdaddr_t* aBdAddr,
                             bt_ssp_variant_t aVariant,
                             uint8_t aAccept, uint32_t aPasskey)
{
  return mInterface->ssp_reply(aBdAddr, aVariant, aAccept, aPasskey);
}

/* DUT Mode */

int
BluetoothInterface::DutModeConfigure(uint8_t aEnable)
{
  return mInterface->dut_mode_configure(aEnable);
}

int
BluetoothInterface::DutModeSend(uint16_t aOpcode, uint8_t* aBuf, uint8_t aLen)
{
  return mInterface->dut_mode_send(aOpcode, aBuf, aLen);
}

/* LE Mode */

int
BluetoothInterface::LeTestMode(uint16_t aOpcode, uint8_t* aBuf, uint8_t aLen)
{
  return mInterface->le_test_mode(aOpcode, aBuf, aLen);
}

/* Profile Interfaces */

template <class T>
T*
BluetoothInterface::GetProfileInterface()
{
  static T* sBluetoothProfileInterface;

  if (sBluetoothProfileInterface) {
    return sBluetoothProfileInterface;
  }

  typename interface_traits<T>::const_interface_type* interface =
    reinterpret_cast<typename interface_traits<T>::const_interface_type*>(
      mInterface->get_profile_interface(interface_traits<T>::profile_id()));

  if (!interface) {
    BT_WARNING("Bluetooth profile '%s' is not supported",
               interface_traits<T>::profile_id());
    return nullptr;
  }

  if (interface->size != sizeof(*interface)) {
    BT_WARNING("interface of incorrect size");
    return nullptr;
  }

  sBluetoothProfileInterface = new T(interface);

  return sBluetoothProfileInterface;
}

BluetoothSocketInterface*
BluetoothInterface::GetBluetoothSocketInterface()
{
  return GetProfileInterface<BluetoothSocketInterface>();
}

BluetoothHandsfreeInterface*
BluetoothInterface::GetBluetoothHandsfreeInterface()
{
  return GetProfileInterface<BluetoothHandsfreeInterface>();
}

BluetoothA2dpInterface*
BluetoothInterface::GetBluetoothA2dpInterface()
{
  return GetProfileInterface<BluetoothA2dpInterface>();
}

BluetoothAvrcpInterface*
BluetoothInterface::GetBluetoothAvrcpInterface()
{
#if ANDROID_VERSION >= 18
  return GetProfileInterface<BluetoothAvrcpInterface>();
#else
  return nullptr;
#endif
}

END_BLUETOOTH_NAMESPACE
