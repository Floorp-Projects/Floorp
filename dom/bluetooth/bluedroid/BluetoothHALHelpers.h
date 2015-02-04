/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluedroid_bluetoothhalhelpers_h__
#define mozilla_dom_bluetooth_bluedroid_bluetoothhalhelpers_h__

#include <hardware/bluetooth.h>
#include <hardware/bt_sock.h>
#include <hardware/bt_hf.h>
#include <hardware/bt_av.h>
#if ANDROID_VERSION >= 18
#include <hardware/bt_rc.h>
#endif
#include "BluetoothCommon.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "nsThreadUtils.h"

#if MOZ_IS_GCC && MOZ_GCC_VERSION_AT_LEAST(4, 7, 0)
/* use designated array initializers if supported */
#define CONVERT(in_, out_) \
  [in_] = out_
#else
/* otherwise init array element by position */
#define CONVERT(in_, out_) \
  out_
#endif

BEGIN_BLUETOOTH_NAMESPACE

//
// Conversion
//

inline nsresult
Convert(bt_status_t aIn, BluetoothStatus& aOut)
{
  static const BluetoothStatus sStatus[] = {
    CONVERT(BT_STATUS_SUCCESS, STATUS_SUCCESS),
    CONVERT(BT_STATUS_FAIL, STATUS_FAIL),
    CONVERT(BT_STATUS_NOT_READY, STATUS_NOT_READY),
    CONVERT(BT_STATUS_NOMEM, STATUS_NOMEM),
    CONVERT(BT_STATUS_BUSY, STATUS_BUSY),
    CONVERT(BT_STATUS_DONE, STATUS_DONE),
    CONVERT(BT_STATUS_UNSUPPORTED, STATUS_UNSUPPORTED),
    CONVERT(BT_STATUS_PARM_INVALID, STATUS_PARM_INVALID),
    CONVERT(BT_STATUS_UNHANDLED, STATUS_UNHANDLED),
    CONVERT(BT_STATUS_AUTH_FAILURE, STATUS_AUTH_FAILURE),
    CONVERT(BT_STATUS_RMT_DEV_DOWN, STATUS_RMT_DEV_DOWN)
  };
  if (aIn >= MOZ_ARRAY_LENGTH(sStatus)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sStatus[aIn];
  return NS_OK;
}

inline nsresult
Convert(int aIn, BluetoothStatus& aOut)
{
  return Convert(static_cast<bt_status_t>(aIn), aOut);
}

nsresult
Convert(const nsAString& aIn, bt_property_type_t& aOut);

inline nsresult
Convert(bool aIn, bt_scan_mode_t& aOut)
{
  static const bt_scan_mode_t sScanMode[] = {
    CONVERT(false, BT_SCAN_MODE_CONNECTABLE),
    CONVERT(true, BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE)
  };
  if (aIn >= MOZ_ARRAY_LENGTH(sScanMode)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sScanMode[aIn];
  return NS_OK;
}


inline nsresult
Convert(bt_scan_mode_t aIn, BluetoothScanMode& aOut)
{
  static const BluetoothScanMode sScanMode[] = {
    CONVERT(BT_SCAN_MODE_NONE, SCAN_MODE_NONE),
    CONVERT(BT_SCAN_MODE_CONNECTABLE, SCAN_MODE_CONNECTABLE),
    CONVERT(BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE,
      SCAN_MODE_CONNECTABLE_DISCOVERABLE)
  };
  if (aIn >= MOZ_ARRAY_LENGTH(sScanMode)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sScanMode[aIn];
  return NS_OK;
}

struct ConvertNamedValue
{
  ConvertNamedValue(const BluetoothNamedValue& aNamedValue)
  : mNamedValue(aNamedValue)
  { }

  const BluetoothNamedValue& mNamedValue;

  // temporary fields
  nsCString mStringValue;
  bt_scan_mode_t mScanMode;
};

nsresult
Convert(ConvertNamedValue& aIn, bt_property_t& aOut);

nsresult
Convert(const nsAString& aIn, bt_bdaddr_t& aOut);

nsresult
Convert(const nsAString& aIn, bt_ssp_variant_t& aOut);

inline nsresult
Convert(const bt_ssp_variant_t& aIn, nsAString& aOut)
{
  static const char * const sSspVariant[] = {
    CONVERT(BT_SSP_VARIANT_PASSKEY_CONFIRMATION, "PasskeyConfirmation"),
    CONVERT(BT_SSP_VARIANT_PASSKEY_ENTRY, "PasskeyEntry"),
    CONVERT(BT_SSP_VARIANT_CONSENT, "Consent"),
    CONVERT(BT_SSP_VARIANT_PASSKEY_NOTIFICATION, "PasskeyNotification")
  };
  if (aIn >= MOZ_ARRAY_LENGTH(sSspVariant)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = NS_ConvertUTF8toUTF16(sSspVariant[aIn]);
  return NS_OK;
}

inline nsresult
Convert(const bool& aIn, uint8_t& aOut)
{
  // casting converts true/false to either 1 or 0
  aOut = static_cast<uint8_t>(aIn);
  return NS_OK;
}

nsresult
Convert(const uint8_t aIn[16], bt_uuid_t& aOut);

nsresult
Convert(const bt_uuid_t& aIn, BluetoothUuid& aOut);

nsresult
Convert(const nsAString& aIn, bt_pin_code_t& aOut);

nsresult
Convert(const bt_bdaddr_t& aIn, nsAString& aOut);

inline nsresult
Convert(const bt_bdaddr_t* aIn, nsAString& aOut)
{
  if (!aIn) {
    aOut.AssignLiteral(BLUETOOTH_ADDRESS_NONE);
    return NS_OK;
  }
  return Convert(*aIn, aOut);
}

inline nsresult
Convert(bt_state_t aIn, bool& aOut)
{
  static const bool sState[] = {
    CONVERT(BT_STATE_OFF, false),
    CONVERT(BT_STATE_ON, true)
  };
  if (aIn >= MOZ_ARRAY_LENGTH(sState)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sState[aIn];
  return NS_OK;
}

inline nsresult
Convert(bt_property_type_t aIn, BluetoothPropertyType& aOut)
{
  static const BluetoothPropertyType sPropertyType[] = {
    CONVERT(0, static_cast<BluetoothPropertyType>(0)), // invalid, required by gcc
    CONVERT(BT_PROPERTY_BDNAME, PROPERTY_BDNAME),
    CONVERT(BT_PROPERTY_BDADDR, PROPERTY_BDADDR),
    CONVERT(BT_PROPERTY_UUIDS, PROPERTY_UUIDS),
    CONVERT(BT_PROPERTY_CLASS_OF_DEVICE, PROPERTY_CLASS_OF_DEVICE),
    CONVERT(BT_PROPERTY_TYPE_OF_DEVICE, PROPERTY_TYPE_OF_DEVICE),
    CONVERT(BT_PROPERTY_SERVICE_RECORD, PROPERTY_SERVICE_RECORD),
    CONVERT(BT_PROPERTY_ADAPTER_SCAN_MODE, PROPERTY_ADAPTER_SCAN_MODE),
    CONVERT(BT_PROPERTY_ADAPTER_BONDED_DEVICES,
      PROPERTY_ADAPTER_BONDED_DEVICES),
    CONVERT(BT_PROPERTY_ADAPTER_DISCOVERY_TIMEOUT,
      PROPERTY_ADAPTER_DISCOVERY_TIMEOUT),
    CONVERT(BT_PROPERTY_REMOTE_FRIENDLY_NAME, PROPERTY_REMOTE_FRIENDLY_NAME),
    CONVERT(BT_PROPERTY_REMOTE_RSSI, PROPERTY_REMOTE_RSSI)
#if ANDROID_VERSION >= 18
    ,
    CONVERT(BT_PROPERTY_REMOTE_VERSION_INFO,PROPERTY_REMOTE_VERSION_INFO)
#endif
  };
  if (aIn == BT_PROPERTY_REMOTE_DEVICE_TIMESTAMP) {
    /* This case is handled separately to not populate
     * |sPropertyType| with empty entries. */
    aOut = PROPERTY_REMOTE_DEVICE_TIMESTAMP;
    return NS_OK;
  }
  if (!aIn || aIn >= MOZ_ARRAY_LENGTH(sPropertyType)) {
    /* Bug 1065999: working around unknown properties */
    aOut = PROPERTY_UNKNOWN;
    return NS_OK;
  }
  aOut = sPropertyType[aIn];
  return NS_OK;
}

inline nsresult
Convert(bt_discovery_state_t aIn, bool& aOut)
{
  static const bool sDiscoveryState[] = {
    CONVERT(BT_DISCOVERY_STOPPED, false),
    CONVERT(BT_DISCOVERY_STARTED, true)
  };
  if (aIn >= MOZ_ARRAY_LENGTH(sDiscoveryState)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sDiscoveryState[aIn];
  return NS_OK;
}

inline nsresult
Convert(const char* aIn, nsACString& aOut)
{
  aOut.Assign(aIn);

  return NS_OK;
}

inline nsresult
Convert(const char* aIn, nsAString& aOut)
{
  aOut = NS_ConvertUTF8toUTF16(aIn);

  return NS_OK;
}

inline nsresult
Convert(const bt_bdname_t& aIn, nsAString& aOut)
{
  return Convert(reinterpret_cast<const char*>(aIn.name), aOut);
}

inline nsresult
Convert(const bt_bdname_t* aIn, nsAString& aOut)
{
  if (!aIn) {
    aOut.Truncate();
    return NS_OK;
  }
  return Convert(*aIn, aOut);
}

inline nsresult
Convert(bt_bond_state_t aIn, BluetoothBondState& aOut)
{
  static const BluetoothBondState sBondState[] = {
    CONVERT(BT_BOND_STATE_NONE, BOND_STATE_NONE),
    CONVERT(BT_BOND_STATE_BONDING, BOND_STATE_BONDING),
    CONVERT(BT_BOND_STATE_BONDED, BOND_STATE_BONDED)
  };
  if (aIn >= MOZ_ARRAY_LENGTH(sBondState)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sBondState[aIn];
  return NS_OK;
}

inline nsresult
Convert(bt_acl_state_t aIn, bool& aOut)
{
  static const bool sAclState[] = {
    CONVERT(BT_ACL_STATE_CONNECTED, true),
    CONVERT(BT_ACL_STATE_DISCONNECTED, false)
  };
  if (aIn >= MOZ_ARRAY_LENGTH(sAclState)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sAclState[aIn];
  return NS_OK;
}

inline nsresult
Convert(bt_device_type_t aIn, BluetoothDeviceType& aOut)
{
  static const BluetoothDeviceType sDeviceType[] = {
    CONVERT(0, static_cast<BluetoothDeviceType>(0)), // invalid, required by gcc
    CONVERT(BT_DEVICE_DEVTYPE_BREDR, DEVICE_TYPE_BREDR),
    CONVERT(BT_DEVICE_DEVTYPE_BLE, DEVICE_TYPE_BLE),
    CONVERT(BT_DEVICE_DEVTYPE_DUAL, DEVICE_TYPE_DUAL)
  };
  if (aIn >= MOZ_ARRAY_LENGTH(sDeviceType)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sDeviceType[aIn];
  return NS_OK;
}

nsresult
Convert(const bt_service_record_t& aIn, BluetoothServiceRecord& aOut);

inline nsresult
Convert(BluetoothSocketType aIn, btsock_type_t& aOut)
{
  // FIXME: Array member [0] is currently invalid, but required
  //        by gcc. Start values in |BluetoothSocketType| at index
  //        0 to fix this problem.
  static const btsock_type_t sSocketType[] = {
    CONVERT(0, static_cast<btsock_type_t>(0)), // invalid, [0] required by gcc
    CONVERT(BluetoothSocketType::RFCOMM, BTSOCK_RFCOMM),
    CONVERT(BluetoothSocketType::SCO, BTSOCK_SCO),
    CONVERT(BluetoothSocketType::L2CAP, BTSOCK_L2CAP),
    // EL2CAP is not supported by Bluedroid
  };
  if (aIn == BluetoothSocketType::EL2CAP ||
      aIn >= MOZ_ARRAY_LENGTH(sSocketType) || !sSocketType[aIn]) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sSocketType[aIn];
  return NS_OK;
}

inline nsresult
Convert(BluetoothHandsfreeAtResponse aIn, bthf_at_response_t& aOut)
{
  static const bthf_at_response_t sAtResponse[] = {
    CONVERT(HFP_AT_RESPONSE_ERROR, BTHF_AT_RESPONSE_ERROR),
    CONVERT(HFP_AT_RESPONSE_OK, BTHF_AT_RESPONSE_OK)
  };
  if (aIn >= MOZ_ARRAY_LENGTH(sAtResponse)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sAtResponse[aIn];
  return NS_OK;
}

inline nsresult
Convert(BluetoothHandsfreeCallAddressType aIn, bthf_call_addrtype_t& aOut)
{
  static const bthf_call_addrtype_t sCallAddressType[] = {
    CONVERT(HFP_CALL_ADDRESS_TYPE_UNKNOWN, BTHF_CALL_ADDRTYPE_UNKNOWN),
    CONVERT(HFP_CALL_ADDRESS_TYPE_INTERNATIONAL,
      BTHF_CALL_ADDRTYPE_INTERNATIONAL)
  };
  if (aIn >= MOZ_ARRAY_LENGTH(sCallAddressType)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sCallAddressType[aIn];
  return NS_OK;
}

inline nsresult
Convert(BluetoothHandsfreeCallDirection aIn, bthf_call_direction_t& aOut)
{
  static const bthf_call_direction_t sCallDirection[] = {
    CONVERT(HFP_CALL_DIRECTION_OUTGOING, BTHF_CALL_DIRECTION_OUTGOING),
    CONVERT(HFP_CALL_DIRECTION_INCOMING, BTHF_CALL_DIRECTION_INCOMING)
  };
  if (aIn >= MOZ_ARRAY_LENGTH(sCallDirection)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sCallDirection[aIn];
  return NS_OK;
}

inline nsresult
Convert(BluetoothHandsfreeCallMode aIn, bthf_call_mode_t& aOut)
{
  static const bthf_call_mode_t sCallMode[] = {
    CONVERT(HFP_CALL_MODE_VOICE, BTHF_CALL_TYPE_VOICE),
    CONVERT(HFP_CALL_MODE_DATA, BTHF_CALL_TYPE_DATA),
    CONVERT(HFP_CALL_MODE_FAX, BTHF_CALL_TYPE_FAX)
  };
  if (aIn >= MOZ_ARRAY_LENGTH(sCallMode)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sCallMode[aIn];
  return NS_OK;
}

inline nsresult
Convert(BluetoothHandsfreeCallMptyType aIn, bthf_call_mpty_type_t& aOut)
{
  static const bthf_call_mpty_type_t sCallMptyType[] = {
    CONVERT(HFP_CALL_MPTY_TYPE_SINGLE, BTHF_CALL_MPTY_TYPE_SINGLE),
    CONVERT(HFP_CALL_MPTY_TYPE_MULTI, BTHF_CALL_MPTY_TYPE_MULTI)
  };
  if (aIn >= MOZ_ARRAY_LENGTH(sCallMptyType)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sCallMptyType[aIn];
  return NS_OK;
}

inline nsresult
Convert(BluetoothHandsfreeCallState aIn, bthf_call_state_t& aOut)
{
  static const bthf_call_state_t sCallState[] = {
    CONVERT(HFP_CALL_STATE_ACTIVE, BTHF_CALL_STATE_ACTIVE),
    CONVERT(HFP_CALL_STATE_HELD, BTHF_CALL_STATE_HELD),
    CONVERT(HFP_CALL_STATE_DIALING, BTHF_CALL_STATE_DIALING),
    CONVERT(HFP_CALL_STATE_ALERTING, BTHF_CALL_STATE_ALERTING),
    CONVERT(HFP_CALL_STATE_INCOMING, BTHF_CALL_STATE_INCOMING),
    CONVERT(HFP_CALL_STATE_WAITING, BTHF_CALL_STATE_WAITING),
    CONVERT(HFP_CALL_STATE_IDLE, BTHF_CALL_STATE_IDLE)
  };
  if (aIn >= MOZ_ARRAY_LENGTH(sCallState)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sCallState[aIn];
  return NS_OK;
}

inline nsresult
Convert(BluetoothHandsfreeNetworkState aIn, bthf_network_state_t& aOut)
{
  static const bthf_network_state_t sNetworkState[] = {
    CONVERT(HFP_NETWORK_STATE_NOT_AVAILABLE, BTHF_NETWORK_STATE_NOT_AVAILABLE),
    CONVERT(HFP_NETWORK_STATE_AVAILABLE,  BTHF_NETWORK_STATE_AVAILABLE)
  };
  if (aIn >= MOZ_ARRAY_LENGTH(sNetworkState)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sNetworkState[aIn];
  return NS_OK;
}

inline nsresult
Convert(BluetoothHandsfreeServiceType aIn, bthf_service_type_t& aOut)
{
  static const bthf_service_type_t sServiceType[] = {
    CONVERT(HFP_SERVICE_TYPE_HOME, BTHF_SERVICE_TYPE_HOME),
    CONVERT(HFP_SERVICE_TYPE_ROAMING, BTHF_SERVICE_TYPE_ROAMING)
  };
  if (aIn >= MOZ_ARRAY_LENGTH(sServiceType)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sServiceType[aIn];
  return NS_OK;
}

inline nsresult
Convert(BluetoothHandsfreeVolumeType aIn, bthf_volume_type_t& aOut)
{
  static const bthf_volume_type_t sVolumeType[] = {
    CONVERT(HFP_VOLUME_TYPE_SPEAKER, BTHF_VOLUME_TYPE_SPK),
    CONVERT(HFP_VOLUME_TYPE_MICROPHONE, BTHF_VOLUME_TYPE_MIC)
  };
  if (aIn >= MOZ_ARRAY_LENGTH(sVolumeType)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sVolumeType[aIn];
  return NS_OK;
}

inline nsresult
Convert(bthf_audio_state_t aIn, BluetoothHandsfreeAudioState& aOut)
{
  static const BluetoothHandsfreeAudioState sAudioState[] = {
    CONVERT(BTHF_AUDIO_STATE_DISCONNECTED, HFP_AUDIO_STATE_DISCONNECTED),
    CONVERT(BTHF_AUDIO_STATE_CONNECTING, HFP_AUDIO_STATE_CONNECTING),
    CONVERT(BTHF_AUDIO_STATE_CONNECTED, HFP_AUDIO_STATE_CONNECTED),
    CONVERT(BTHF_AUDIO_STATE_DISCONNECTING, HFP_AUDIO_STATE_DISCONNECTING)
  };
  if (aIn >= MOZ_ARRAY_LENGTH(sAudioState)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sAudioState[aIn];
  return NS_OK;
}

inline nsresult
Convert(bthf_chld_type_t aIn, BluetoothHandsfreeCallHoldType& aOut)
{
  static const BluetoothHandsfreeCallHoldType sCallHoldType[] = {
    CONVERT(BTHF_CHLD_TYPE_RELEASEHELD, HFP_CALL_HOLD_RELEASEHELD),
    CONVERT(BTHF_CHLD_TYPE_RELEASEACTIVE_ACCEPTHELD,
      HFP_CALL_HOLD_RELEASEACTIVE_ACCEPTHELD),
    CONVERT(BTHF_CHLD_TYPE_HOLDACTIVE_ACCEPTHELD,
      HFP_CALL_HOLD_HOLDACTIVE_ACCEPTHELD),
    CONVERT(BTHF_CHLD_TYPE_ADDHELDTOCONF, HFP_CALL_HOLD_ADDHELDTOCONF)
  };
  if (aIn >= MOZ_ARRAY_LENGTH(sCallHoldType)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sCallHoldType[aIn];
  return NS_OK;
}

inline nsresult
Convert(bthf_connection_state_t aIn, BluetoothHandsfreeConnectionState& aOut)
{
  static const BluetoothHandsfreeConnectionState sConnectionState[] = {
    CONVERT(BTHF_CONNECTION_STATE_DISCONNECTED,
      HFP_CONNECTION_STATE_DISCONNECTED),
    CONVERT(BTHF_CONNECTION_STATE_CONNECTING, HFP_CONNECTION_STATE_CONNECTING),
    CONVERT(BTHF_CONNECTION_STATE_CONNECTED, HFP_CONNECTION_STATE_CONNECTED),
    CONVERT(BTHF_CONNECTION_STATE_SLC_CONNECTED,
      HFP_CONNECTION_STATE_SLC_CONNECTED),
    CONVERT(BTHF_CONNECTION_STATE_DISCONNECTING,
      HFP_CONNECTION_STATE_DISCONNECTING)
  };
  if (aIn >= MOZ_ARRAY_LENGTH(sConnectionState)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sConnectionState[aIn];
  return NS_OK;
}

inline nsresult
Convert(bthf_nrec_t aIn, BluetoothHandsfreeNRECState& aOut)
{
  static const BluetoothHandsfreeNRECState sNRECState[] = {
    CONVERT(BTHF_NREC_STOP, HFP_NREC_STOPPED),
    CONVERT(BTHF_NREC_START, HFP_NREC_STARTED)
  };
  if (aIn >= MOZ_ARRAY_LENGTH(sNRECState)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sNRECState[aIn];
  return NS_OK;
}

inline nsresult
Convert(bthf_vr_state_t aIn, BluetoothHandsfreeVoiceRecognitionState& aOut)
{
  static const BluetoothHandsfreeVoiceRecognitionState
    sVoiceRecognitionState[] = {
    CONVERT(BTHF_VR_STATE_STOPPED, HFP_VOICE_RECOGNITION_STOPPED),
    CONVERT(BTHF_VR_STATE_STARTED, HFP_VOICE_RECOGNITION_STARTED)
  };
  if (aIn >= MOZ_ARRAY_LENGTH(sVoiceRecognitionState)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sVoiceRecognitionState[aIn];
  return NS_OK;
}

inline nsresult
Convert(bthf_volume_type_t aIn, BluetoothHandsfreeVolumeType& aOut)
{
  static const BluetoothHandsfreeVolumeType sVolumeType[] = {
    CONVERT(BTHF_VOLUME_TYPE_SPK, HFP_VOLUME_TYPE_SPEAKER),
    CONVERT(BTHF_VOLUME_TYPE_MIC, HFP_VOLUME_TYPE_MICROPHONE)
  };
  if (aIn >= MOZ_ARRAY_LENGTH(sVolumeType)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sVolumeType[aIn];
  return NS_OK;
}

inline nsresult
Convert(btav_connection_state_t aIn, BluetoothA2dpConnectionState& aOut)
{
  static const BluetoothA2dpConnectionState sConnectionState[] = {
    CONVERT(BTAV_CONNECTION_STATE_DISCONNECTED,
      A2DP_CONNECTION_STATE_DISCONNECTED),
    CONVERT(BTAV_CONNECTION_STATE_CONNECTING,
      A2DP_CONNECTION_STATE_CONNECTING),
    CONVERT(BTAV_CONNECTION_STATE_CONNECTED,
      A2DP_CONNECTION_STATE_CONNECTED),
    CONVERT(BTAV_CONNECTION_STATE_DISCONNECTING,
      A2DP_CONNECTION_STATE_DISCONNECTING),
  };
  if (aIn >= MOZ_ARRAY_LENGTH(sConnectionState)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sConnectionState[aIn];
  return NS_OK;
}

inline nsresult
Convert(btav_audio_state_t aIn, BluetoothA2dpAudioState& aOut)
{
  static const BluetoothA2dpAudioState sAudioState[] = {
    CONVERT(BTAV_AUDIO_STATE_REMOTE_SUSPEND, A2DP_AUDIO_STATE_REMOTE_SUSPEND),
    CONVERT(BTAV_AUDIO_STATE_STOPPED, A2DP_AUDIO_STATE_STOPPED),
    CONVERT(BTAV_AUDIO_STATE_STARTED, A2DP_AUDIO_STATE_STARTED),
  };
  if (aIn >= MOZ_ARRAY_LENGTH(sAudioState)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sAudioState[aIn];
  return NS_OK;
}

#if ANDROID_VERSION >= 18
inline nsresult
Convert(const bt_remote_version_t& aIn, BluetoothRemoteInfo& aOut)
{
  aOut.mVerMajor = aIn.version;
  aOut.mVerMinor = aIn.sub_ver;
  aOut.mManufacturer = aIn.manufacturer;

  return NS_OK;
}

inline nsresult
Convert(ControlPlayStatus aIn, btrc_play_status_t& aOut)
{
  static const btrc_play_status_t sPlayStatus[] = {
    CONVERT(PLAYSTATUS_STOPPED, BTRC_PLAYSTATE_STOPPED),
    CONVERT(PLAYSTATUS_PLAYING, BTRC_PLAYSTATE_PLAYING),
    CONVERT(PLAYSTATUS_PAUSED, BTRC_PLAYSTATE_PAUSED),
    CONVERT(PLAYSTATUS_FWD_SEEK, BTRC_PLAYSTATE_FWD_SEEK),
    CONVERT(PLAYSTATUS_REV_SEEK, BTRC_PLAYSTATE_REV_SEEK)
  };
  if (aIn >= MOZ_ARRAY_LENGTH(sPlayStatus)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sPlayStatus[aIn];
  return NS_OK;
}

inline nsresult
Convert(enum BluetoothAvrcpPlayerAttribute aIn, btrc_player_attr_t& aOut)
{
  static const btrc_player_attr_t sPlayerAttr[] = {
    CONVERT(AVRCP_PLAYER_ATTRIBUTE_EQUALIZER, BTRC_PLAYER_ATTR_EQUALIZER),
    CONVERT(AVRCP_PLAYER_ATTRIBUTE_REPEAT, BTRC_PLAYER_ATTR_REPEAT),
    CONVERT(AVRCP_PLAYER_ATTRIBUTE_SHUFFLE, BTRC_PLAYER_ATTR_SHUFFLE),
    CONVERT(AVRCP_PLAYER_ATTRIBUTE_SCAN, BTRC_PLAYER_ATTR_SCAN)
  };
  if (aIn >= MOZ_ARRAY_LENGTH(sPlayerAttr)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sPlayerAttr[aIn];
  return NS_OK;
}

inline nsresult
Convert(btrc_player_attr_t aIn, enum BluetoothAvrcpPlayerAttribute& aOut)
{
  static const BluetoothAvrcpPlayerAttribute sPlayerAttr[] = {
    CONVERT(0, static_cast<BluetoothAvrcpPlayerAttribute>(0)), // invalid, [0] required by gcc
    CONVERT(BTRC_PLAYER_ATTR_EQUALIZER, AVRCP_PLAYER_ATTRIBUTE_EQUALIZER),
    CONVERT(BTRC_PLAYER_ATTR_REPEAT, AVRCP_PLAYER_ATTRIBUTE_REPEAT),
    CONVERT(BTRC_PLAYER_ATTR_SHUFFLE, AVRCP_PLAYER_ATTRIBUTE_SHUFFLE),
    CONVERT(BTRC_PLAYER_ATTR_SCAN, AVRCP_PLAYER_ATTRIBUTE_SCAN)
  };
  if (!aIn || aIn >= MOZ_ARRAY_LENGTH(sPlayerAttr)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sPlayerAttr[aIn];
  return NS_OK;
}

inline nsresult
Convert(enum BluetoothAvrcpStatus aIn, btrc_status_t& aOut)
{
  static const btrc_status_t sStatus[] = {
    CONVERT(AVRCP_STATUS_BAD_COMMAND, BTRC_STS_BAD_CMD),
    CONVERT(AVRCP_STATUS_BAD_PARAMETER, BTRC_STS_BAD_PARAM),
    CONVERT(AVRCP_STATUS_NOT_FOUND, BTRC_STS_NOT_FOUND),
    CONVERT(AVRCP_STATUS_INTERNAL_ERROR, BTRC_STS_INTERNAL_ERR),
    CONVERT(AVRCP_STATUS_SUCCESS, BTRC_STS_NO_ERROR)
  };
  if (aIn >= MOZ_ARRAY_LENGTH(sStatus)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sStatus[aIn];
  return NS_OK;
}

inline nsresult
Convert(enum BluetoothAvrcpEvent aIn, btrc_event_id_t& aOut)
{
  static const btrc_event_id_t sEventId[] = {
    CONVERT(AVRCP_EVENT_PLAY_STATUS_CHANGED, BTRC_EVT_PLAY_STATUS_CHANGED),
    CONVERT(AVRCP_EVENT_TRACK_CHANGE, BTRC_EVT_TRACK_CHANGE),
    CONVERT(AVRCP_EVENT_TRACK_REACHED_END, BTRC_EVT_TRACK_REACHED_END),
    CONVERT(AVRCP_EVENT_TRACK_REACHED_START, BTRC_EVT_TRACK_REACHED_START),
    CONVERT(AVRCP_EVENT_PLAY_POS_CHANGED, BTRC_EVT_PLAY_POS_CHANGED),
    CONVERT(AVRCP_EVENT_APP_SETTINGS_CHANGED, BTRC_EVT_APP_SETTINGS_CHANGED)
  };
  if (aIn >= MOZ_ARRAY_LENGTH(sEventId)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sEventId[aIn];
  return NS_OK;
}

inline nsresult
Convert(btrc_event_id_t aIn, enum BluetoothAvrcpEvent& aOut)
{
  static const BluetoothAvrcpEvent sEventId[] = {
    CONVERT(0, static_cast<BluetoothAvrcpEvent>(0)), // invalid, [0] required by gcc
    CONVERT(BTRC_EVT_PLAY_STATUS_CHANGED, AVRCP_EVENT_PLAY_STATUS_CHANGED),
    CONVERT(BTRC_EVT_TRACK_CHANGE, AVRCP_EVENT_TRACK_CHANGE),
    CONVERT(BTRC_EVT_TRACK_REACHED_END, AVRCP_EVENT_TRACK_REACHED_END),
    CONVERT(BTRC_EVT_TRACK_REACHED_START, AVRCP_EVENT_TRACK_REACHED_START),
    CONVERT(BTRC_EVT_PLAY_POS_CHANGED, AVRCP_EVENT_PLAY_POS_CHANGED),
    CONVERT(6, static_cast<BluetoothAvrcpEvent>(0)), // invalid, [6] required by gcc
    CONVERT(7, static_cast<BluetoothAvrcpEvent>(0)), // invalid, [7] required by gcc
    CONVERT(BTRC_EVT_APP_SETTINGS_CHANGED, AVRCP_EVENT_APP_SETTINGS_CHANGED)
  };
  if (!aIn || aIn >= MOZ_ARRAY_LENGTH(sEventId)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sEventId[aIn];
  return NS_OK;
}

inline nsresult
Convert(btrc_media_attr_t aIn, enum BluetoothAvrcpMediaAttribute& aOut)
{
  static const BluetoothAvrcpMediaAttribute sEventId[] = {
    CONVERT(0, static_cast<BluetoothAvrcpMediaAttribute>(0)), // invalid, [0] required by gcc
    CONVERT(BTRC_MEDIA_ATTR_TITLE, AVRCP_MEDIA_ATTRIBUTE_TITLE),
    CONVERT(BTRC_MEDIA_ATTR_ARTIST, AVRCP_MEDIA_ATTRIBUTE_ARTIST),
    CONVERT(BTRC_MEDIA_ATTR_ALBUM, AVRCP_MEDIA_ATTRIBUTE_ALBUM),
    CONVERT(BTRC_MEDIA_ATTR_TRACK_NUM, AVRCP_MEDIA_ATTRIBUTE_TRACK_NUM),
    CONVERT(BTRC_MEDIA_ATTR_NUM_TRACKS, AVRCP_MEDIA_ATTRIBUTE_NUM_TRACKS),
    CONVERT(BTRC_MEDIA_ATTR_GENRE, AVRCP_MEDIA_ATTRIBUTE_GENRE),
    CONVERT(BTRC_MEDIA_ATTR_PLAYING_TIME, AVRCP_MEDIA_ATTRIBUTE_PLAYING_TIME)
  };
  if (!aIn || aIn >= MOZ_ARRAY_LENGTH(sEventId)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sEventId[aIn];
  return NS_OK;
}

inline nsresult
Convert(enum BluetoothAvrcpNotification aIn, btrc_notification_type_t& aOut)
{
  static const btrc_notification_type_t sNotificationType[] = {
    CONVERT(AVRCP_NTF_INTERIM, BTRC_NOTIFICATION_TYPE_INTERIM),
    CONVERT(AVRCP_NTF_CHANGED, BTRC_NOTIFICATION_TYPE_CHANGED)
  };
  if (aIn >= MOZ_ARRAY_LENGTH(sNotificationType)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sNotificationType[aIn];
  return NS_OK;
}

nsresult
Convert(const BluetoothAvrcpElementAttribute& aIn, btrc_element_attr_val_t& aOut);

nsresult
Convert(const btrc_player_settings_t& aIn, BluetoothAvrcpPlayerSettings& aOut);
#endif // ANDROID_VERSION >= 18

#if ANDROID_VERSION >= 19
inline nsresult
Convert(btrc_remote_features_t aIn, unsigned long& aOut)
{
  /* The input type's name is misleading. The converted value is
   * actually a bitmask.
   */
  aOut = static_cast<unsigned long>(aIn);
  return NS_OK;
}
#endif // ANDROID_VERSION >= 19

#if ANDROID_VERSION >= 21
inline nsresult
Convert(BluetoothTransport aIn, int& aOut)
{
  static const int sTransport[] = {
    CONVERT(TRANSPORT_AUTO, 0),
    CONVERT(TRANSPORT_BREDR, 1),
    CONVERT(TRANSPORT_LE, 2)
  };
  if (NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sTransport))) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sTransport[aIn];
  return NS_OK;
}

nsresult
Convert(const bt_activity_energy_info& aIn, BluetoothActivityEnergyInfo& aOut);

inline nsresult
Convert(bthf_wbs_config_t aIn, BluetoothHandsfreeWbsConfig& aOut)
{
  static const BluetoothHandsfreeWbsConfig sWbsConfig[] = {
    CONVERT(BTHF_WBS_NONE, HFP_WBS_NONE),
    CONVERT(BTHF_WBS_NO, HFP_WBS_NO),
    CONVERT(BTHF_WBS_YES, HFP_WBS_YES)
  };
  if (NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sWbsConfig))) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sWbsConfig[aIn];
  return NS_OK;
}

inline nsresult
Convert(BluetoothHandsfreeWbsConfig aIn, bthf_wbs_config_t& aOut)
{
  static const bthf_wbs_config_t sWbsConfig[] = {
    CONVERT(HFP_WBS_NONE, BTHF_WBS_NONE),
    CONVERT(HFP_WBS_NO, BTHF_WBS_NO),
    CONVERT(HFP_WBS_YES, BTHF_WBS_YES)
  };
  if (NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sWbsConfig))) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sWbsConfig[aIn];
  return NS_OK;
}
#endif // ANDROID_VERSION >= 21

/* |ConvertArray| is a helper for converting arrays. Pass an
 * instance of this structure as the first argument to |Convert|
 * to convert an array. The output type has to support the array
 * subscript operator.
 */
template <typename T>
struct ConvertArray
{
  ConvertArray(const T* aData, unsigned long aLength)
  : mData(aData)
  , mLength(aLength)
  { }

  const T* mData;
  unsigned long mLength;
};

/* This implementation of |Convert| converts the elements of an
 * array one-by-one. The result data structures must have enough
 * memory allocated.
 */
template<typename Tin, typename Tout>
inline nsresult
Convert(const ConvertArray<Tin>& aIn, Tout& aOut)
{
  for (unsigned long i = 0; i < aIn.mLength; ++i) {
    nsresult rv = Convert(aIn.mData[i], aOut[i]);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  return NS_OK;
}

/* This implementation of |Convert| is a helper that automatically
 * allocates enough memory to hold the conversion results. The
 * actual conversion is performed by the array-conversion helper
 * above.
 */
template<typename Tin, typename Tout>
inline nsresult
Convert(const ConvertArray<Tin>& aIn, nsAutoArrayPtr<Tout>& aOut)
{
  aOut = new Tout[aIn.mLength];
  Tout* out = aOut.get();

  return Convert<Tin, Tout*>(aIn, out);
}

/* |ConvertDefault| is a helper function to return the result of a
 * conversion or a default value if the conversion fails.
 */
template<typename Tin, typename Tout>
inline Tout
ConvertDefault(const Tin& aIn, const Tout& aDefault)
{
  Tout out = aDefault; // assignment silences compiler warning
  if (NS_FAILED(Convert(aIn, out))) {
    return aDefault;
  }
  return out;
}

/* This implementation of |Convert| is a helper for copying the
 * input value into the output value. It handles all cases that
 * need no conversion.
 */
template<typename T>
inline nsresult
Convert(const T& aIn, T& aOut)
{
  aOut = aIn;

  return NS_OK;
}

nsresult
Convert(const bt_property_t& aIn, BluetoothProperty& aOut);

//
// Result handling
//

template <typename Obj, typename Res>
class BluetoothHALInterfaceRunnable0 : public nsRunnable
{
public:
  BluetoothHALInterfaceRunnable0(Obj* aObj, Res (Obj::*aMethod)())
  : mObj(aObj)
  , mMethod(aMethod)
  {
    MOZ_ASSERT(mObj);
    MOZ_ASSERT(mMethod);
  }

  NS_METHOD
  Run() MOZ_OVERRIDE
  {
    ((*mObj).*mMethod)();
    return NS_OK;
  }

private:
  nsRefPtr<Obj> mObj;
  void (Obj::*mMethod)();
};

template <typename Obj, typename Res, typename Tin1, typename Arg1>
class BluetoothHALInterfaceRunnable1 : public nsRunnable
{
public:
  BluetoothHALInterfaceRunnable1(Obj* aObj, Res (Obj::*aMethod)(Arg1),
                                 const Arg1& aArg1)
  : mObj(aObj)
  , mMethod(aMethod)
  , mArg1(aArg1)
  {
    MOZ_ASSERT(mObj);
    MOZ_ASSERT(mMethod);
  }

  NS_METHOD
  Run() MOZ_OVERRIDE
  {
    ((*mObj).*mMethod)(mArg1);
    return NS_OK;
  }

private:
  nsRefPtr<Obj> mObj;
  Res (Obj::*mMethod)(Arg1);
  Tin1 mArg1;
};

template <typename Obj, typename Res,
          typename Tin1, typename Tin2, typename Tin3,
          typename Arg1, typename Arg2, typename Arg3>
class BluetoothHALInterfaceRunnable3 : public nsRunnable
{
public:
  BluetoothHALInterfaceRunnable3(Obj* aObj,
                                 Res (Obj::*aMethod)(Arg1, Arg2, Arg3),
                                 const Arg1& aArg1, const Arg2& aArg2,
                                 const Arg3& aArg3)
  : mObj(aObj)
  , mMethod(aMethod)
  , mArg1(aArg1)
  , mArg2(aArg2)
  , mArg3(aArg3)
  {
    MOZ_ASSERT(mObj);
    MOZ_ASSERT(mMethod);
  }

  NS_METHOD
  Run() MOZ_OVERRIDE
  {
    ((*mObj).*mMethod)(mArg1, mArg2, mArg3);
    return NS_OK;
  }

private:
  nsRefPtr<Obj> mObj;
  Res (Obj::*mMethod)(Arg1, Arg2, Arg3);
  Tin1 mArg1;
  Tin2 mArg2;
  Tin3 mArg3;
};

//
// Notification handling
//

template <typename ObjectWrapper, typename Res>
class BluetoothNotificationHALRunnable0 : public nsRunnable
{
public:
  typedef typename ObjectWrapper::ObjectType  ObjectType;
  typedef BluetoothNotificationHALRunnable0<ObjectWrapper, Res> SelfType;

  static already_AddRefed<SelfType> Create(Res (ObjectType::*aMethod)())
  {
    nsRefPtr<SelfType> runnable(new SelfType(aMethod));

    return runnable.forget();
  }

  static void
  Dispatch(Res (ObjectType::*aMethod)())
  {
    nsRefPtr<SelfType> runnable = Create(aMethod);

    if (!runnable) {
      BT_WARNING("BluetoothNotificationHALRunnable0::Create failed");
      return;
    }
    nsresult rv = NS_DispatchToMainThread(runnable);
    if (NS_FAILED(rv)) {
      BT_WARNING("NS_DispatchToMainThread failed: %X", rv);
    }
  }

  NS_METHOD
  Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread());

    ObjectType* obj = ObjectWrapper::GetInstance();

    if (!obj) {
      BT_WARNING("Notification handler not initialized");
    } else {
      ((*obj).*mMethod)();
    }
    return NS_OK;
  }

private:
  BluetoothNotificationHALRunnable0(Res (ObjectType::*aMethod)())
  : mMethod(aMethod)
  {
    MOZ_ASSERT(mMethod);
  }

  Res (ObjectType::*mMethod)();
};

template <typename ObjectWrapper, typename Res,
          typename Tin1, typename Arg1=Tin1>
class BluetoothNotificationHALRunnable1 : public nsRunnable
{
public:
  typedef typename ObjectWrapper::ObjectType  ObjectType;
  typedef BluetoothNotificationHALRunnable1<ObjectWrapper, Res,
                                            Tin1, Arg1> SelfType;

  template <typename T1>
  static already_AddRefed<SelfType> Create(
    Res (ObjectType::*aMethod)(Arg1), const T1& aIn1)
  {
    nsRefPtr<SelfType> runnable(new SelfType(aMethod));

    if (NS_FAILED(runnable->ConvertAndSet(aIn1))) {
      return nullptr;
    }
    return runnable.forget();
  }

  template <typename T1>
  static void
  Dispatch(Res (ObjectType::*aMethod)(Arg1), const T1& aIn1)
  {
    nsRefPtr<SelfType> runnable = Create(aMethod, aIn1);

    if (!runnable) {
      BT_WARNING("BluetoothNotificationHALRunnable1::Create failed");
      return;
    }
    nsresult rv = NS_DispatchToMainThread(runnable);
    if (NS_FAILED(rv)) {
      BT_WARNING("NS_DispatchToMainThread failed: %X", rv);
    }
  }

  NS_METHOD
  Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread());

    ObjectType* obj = ObjectWrapper::GetInstance();

    if (!obj) {
      BT_WARNING("Notification handler not initialized");
    } else {
      ((*obj).*mMethod)(mArg1);
    }
    return NS_OK;
  }

private:
  BluetoothNotificationHALRunnable1(Res (ObjectType::*aMethod)(Arg1))
  : mMethod(aMethod)
  {
    MOZ_ASSERT(mMethod);
  }

  template<typename T1>
  nsresult
  ConvertAndSet(const T1& aIn1)
  {
    nsresult rv = Convert(aIn1, mArg1);
    if (NS_FAILED(rv)) {
      return rv;
    }
    return NS_OK;
  }

  Res (ObjectType::*mMethod)(Arg1);
  Tin1 mArg1;
};

template <typename ObjectWrapper, typename Res,
          typename Tin1, typename Tin2,
          typename Arg1=Tin1, typename Arg2=Tin2>
class BluetoothNotificationHALRunnable2 : public nsRunnable
{
public:
  typedef typename ObjectWrapper::ObjectType  ObjectType;
  typedef BluetoothNotificationHALRunnable2<ObjectWrapper, Res,
                                            Tin1, Tin2,
                                            Arg1, Arg2> SelfType;

  template <typename T1, typename T2>
  static already_AddRefed<SelfType> Create(
    Res (ObjectType::*aMethod)(Arg1, Arg2), const T1& aIn1, const T2& aIn2)
  {
    nsRefPtr<SelfType> runnable(new SelfType(aMethod));

    if (NS_FAILED(runnable->ConvertAndSet(aIn1, aIn2))) {
      return nullptr;
    }
    return runnable.forget();
  }

  template <typename T1, typename T2>
  static void
  Dispatch(Res (ObjectType::*aMethod)(Arg1, Arg2),
           const T1& aIn1, const T2& aIn2)
  {
    nsRefPtr<SelfType> runnable = Create(aMethod, aIn1, aIn2);

    if (!runnable) {
      BT_WARNING("BluetoothNotificationHALRunnable2::Create failed");
      return;
    }
    nsresult rv = NS_DispatchToMainThread(runnable);
    if (NS_FAILED(rv)) {
      BT_WARNING("NS_DispatchToMainThread failed: %X", rv);
    }
  }

  NS_METHOD
  Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread());

    ObjectType* obj = ObjectWrapper::GetInstance();

    if (!obj) {
      BT_WARNING("Notification handler not initialized");
    } else {
      ((*obj).*mMethod)(mArg1, mArg2);
    }
    return NS_OK;
  }

private:
  BluetoothNotificationHALRunnable2(
    Res (ObjectType::*aMethod)(Arg1, Arg2))
  : mMethod(aMethod)
  {
    MOZ_ASSERT(mMethod);
  }

  template<typename T1, typename T2>
  nsresult
  ConvertAndSet(const T1& aIn1, const T2& aIn2)
  {
    nsresult rv = Convert(aIn1, mArg1);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = Convert(aIn2, mArg2);
    if (NS_FAILED(rv)) {
      return rv;
    }
    return NS_OK;
  }

  Res (ObjectType::*mMethod)(Arg1, Arg2);
  Tin1 mArg1;
  Tin2 mArg2;
};

template <typename ObjectWrapper, typename Res,
          typename Tin1, typename Tin2, typename Tin3,
          typename Arg1=Tin1, typename Arg2=Tin2, typename Arg3=Tin3>
class BluetoothNotificationHALRunnable3 : public nsRunnable
{
public:
  typedef typename ObjectWrapper::ObjectType  ObjectType;
  typedef BluetoothNotificationHALRunnable3<ObjectWrapper, Res,
                                            Tin1, Tin2, Tin3,
                                            Arg1, Arg2, Arg3> SelfType;

  template <typename T1, typename T2, typename T3>
  static already_AddRefed<SelfType> Create(
    Res (ObjectType::*aMethod)(Arg1, Arg2, Arg3),
    const T1& aIn1, const T2& aIn2, const T3& aIn3)
  {
    nsRefPtr<SelfType> runnable(new SelfType(aMethod));

    if (NS_FAILED(runnable->ConvertAndSet(aIn1, aIn2, aIn3))) {
      return nullptr;
    }
    return runnable.forget();
  }

  template <typename T1, typename T2, typename T3>
  static void
  Dispatch(Res (ObjectType::*aMethod)(Arg1, Arg2, Arg3),
           const T1& aIn1, const T2& aIn2, const T3& aIn3)
  {
    nsRefPtr<SelfType> runnable = Create(aMethod, aIn1, aIn2, aIn3);

    if (!runnable) {
      BT_WARNING("BluetoothNotificationHALRunnable3::Create failed");
      return;
    }
    nsresult rv = NS_DispatchToMainThread(runnable);
    if (NS_FAILED(rv)) {
      BT_WARNING("NS_DispatchToMainThread failed: %X", rv);
    }
  }

  NS_METHOD
  Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread());

    ObjectType* obj = ObjectWrapper::GetInstance();

    if (!obj) {
      BT_WARNING("Notification handler not initialized");
    } else {
      ((*obj).*mMethod)(mArg1, mArg2, mArg3);
    }
    return NS_OK;
  }

private:
  BluetoothNotificationHALRunnable3(
    Res (ObjectType::*aMethod)(Arg1, Arg2, Arg3))
  : mMethod(aMethod)
  {
    MOZ_ASSERT(mMethod);
  }

  template<typename T1, typename T2, typename T3>
  nsresult
  ConvertAndSet(const T1& aIn1, const T2& aIn2, const T3& aIn3)
  {
    nsresult rv = Convert(aIn1, mArg1);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = Convert(aIn2, mArg2);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = Convert(aIn3, mArg3);
    if (NS_FAILED(rv)) {
      return rv;
    }
    return NS_OK;
  }

  Res (ObjectType::*mMethod)(Arg1, Arg2, Arg3);
  Tin1 mArg1;
  Tin2 mArg2;
  Tin3 mArg3;
};

template <typename ObjectWrapper, typename Res,
          typename Tin1, typename Tin2, typename Tin3, typename Tin4,
          typename Arg1=Tin1, typename Arg2=Tin2,
          typename Arg3=Tin3, typename Arg4=Tin4>
class BluetoothNotificationHALRunnable4 : public nsRunnable
{
public:
  typedef typename ObjectWrapper::ObjectType  ObjectType;
  typedef BluetoothNotificationHALRunnable4<ObjectWrapper, Res,
    Tin1, Tin2, Tin3, Tin4, Arg1, Arg2, Arg3, Arg4> SelfType;

  template <typename T1, typename T2, typename T3, typename T4>
  static already_AddRefed<SelfType> Create(
    Res (ObjectType::*aMethod)(Arg1, Arg2, Arg3, Arg4),
    const T1& aIn1, const T2& aIn2, const T3& aIn3, const T4& aIn4)
  {
    nsRefPtr<SelfType> runnable(new SelfType(aMethod));

    if (NS_FAILED(runnable->ConvertAndSet(aIn1, aIn2, aIn3, aIn4))) {
      return nullptr;
    }
    return runnable.forget();
  }

  template <typename T1, typename T2, typename T3, typename T4>
  static void
  Dispatch(Res (ObjectType::*aMethod)(Arg1, Arg2, Arg3, Arg4),
           const T1& aIn1, const T2& aIn2, const T3& aIn3, const T4& aIn4)
  {
    nsRefPtr<SelfType> runnable = Create(aMethod, aIn1, aIn2, aIn3, aIn4);

    if (!runnable) {
      BT_WARNING("BluetoothNotificationHALRunnable4::Create failed");
      return;
    }
    nsresult rv = NS_DispatchToMainThread(runnable);
    if (NS_FAILED(rv)) {
      BT_WARNING("NS_DispatchToMainThread failed: %X", rv);
    }
  }

  NS_METHOD
  Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread());

    ObjectType* obj = ObjectWrapper::GetInstance();

    if (!obj) {
      BT_WARNING("Notification handler not initialized");
    } else {
      ((*obj).*mMethod)(mArg1, mArg2, mArg3, mArg4);
    }
    return NS_OK;
  }

private:
  BluetoothNotificationHALRunnable4(
    Res (ObjectType::*aMethod)(Arg1, Arg2, Arg3, Arg4))
  : mMethod(aMethod)
  {
    MOZ_ASSERT(mMethod);
  }

  template<typename T1,typename T2, typename T3, typename T4>
  nsresult
  ConvertAndSet(const T1& aIn1, const T2& aIn2,
                const T3& aIn3, const T4& aIn4)
  {
    nsresult rv = Convert(aIn1, mArg1);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = Convert(aIn2, mArg2);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = Convert(aIn3, mArg3);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = Convert(aIn4, mArg4);
    if (NS_FAILED(rv)) {
      return rv;
    }
    return NS_OK;
  }

  Res (ObjectType::*mMethod)(Arg1, Arg2, Arg3, Arg4);
  Tin1 mArg1;
  Tin2 mArg2;
  Tin3 mArg3;
  Tin4 mArg4;
};

template <typename ObjectWrapper, typename Res,
          typename Tin1, typename Tin2, typename Tin3,
          typename Tin4, typename Tin5,
          typename Arg1=Tin1, typename Arg2=Tin2, typename Arg3=Tin3,
          typename Arg4=Tin4, typename Arg5=Tin5>
class BluetoothNotificationHALRunnable5 : public nsRunnable
{
public:
  typedef typename ObjectWrapper::ObjectType  ObjectType;
  typedef BluetoothNotificationHALRunnable5<ObjectWrapper, Res,
    Tin1, Tin2, Tin3, Tin4, Tin5, Arg1, Arg2, Arg3, Arg4, Arg5> SelfType;

  template <typename T1, typename T2, typename T3, typename T4, typename T5>
  static already_AddRefed<SelfType> Create(
    Res (ObjectType::*aMethod)(Arg1, Arg2, Arg3, Arg4, Arg5),
    const T1& aIn1, const T2& aIn2, const T3& aIn3,
    const T4& aIn4, const T5& aIn5)
  {
    nsRefPtr<SelfType> runnable(new SelfType(aMethod));

    if (NS_FAILED(runnable->ConvertAndSet(aIn1, aIn2, aIn3, aIn4, aIn5))) {
      return nullptr;
    }
    return runnable.forget();
  }

  template <typename T1, typename T2, typename T3, typename T4, typename T5>
  static void
  Dispatch(Res (ObjectType::*aMethod)(Arg1, Arg2, Arg3, Arg4, Arg5),
           const T1& aIn1, const T2& aIn2, const T3& aIn3,
           const T4& aIn4, const T5& aIn5)
  {
    nsRefPtr<SelfType> runnable = Create(aMethod,
                                         aIn1, aIn2, aIn3, aIn4, aIn5);
    if (!runnable) {
      BT_WARNING("BluetoothNotificationHALRunnable5::Create failed");
      return;
    }
    nsresult rv = NS_DispatchToMainThread(runnable);
    if (NS_FAILED(rv)) {
      BT_WARNING("NS_DispatchToMainThread failed: %X", rv);
    }
  }

  NS_METHOD
  Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread());

    ObjectType* obj = ObjectWrapper::GetInstance();

    if (!obj) {
      BT_WARNING("Notification handler not initialized");
    } else {
      ((*obj).*mMethod)(mArg1, mArg2, mArg3, mArg4, mArg5);
    }
    return NS_OK;
  }

private:
  BluetoothNotificationHALRunnable5(
    Res (ObjectType::*aMethod)(Arg1, Arg2, Arg3, Arg4, Arg5))
  : mMethod(aMethod)
  {
    MOZ_ASSERT(mMethod);
  }

  template<typename T1, typename T2, typename T3, typename T4, typename T5>
  nsresult
  ConvertAndSet(const T1& aIn1, const T2& aIn2, const T3& aIn3,
                const T4& aIn4, const T5& aIn5)
  {
    nsresult rv = Convert(aIn1, mArg1);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = Convert(aIn2, mArg2);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = Convert(aIn3, mArg3);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = Convert(aIn4, mArg4);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = Convert(aIn5, mArg5);
    if (NS_FAILED(rv)) {
      return rv;
    }
    return NS_OK;
  }

  Res (ObjectType::*mMethod)(Arg1, Arg2, Arg3, Arg4, Arg5);
  Tin1 mArg1;
  Tin2 mArg2;
  Tin3 mArg3;
  Tin4 mArg4;
  Tin5 mArg5;
};

END_BLUETOOTH_NAMESPACE

#endif
