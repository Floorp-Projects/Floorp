/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothInterface.h"
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include "base/message_loop.h"
#include "nsAutoPtr.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

#if MOZ_IS_GCC && MOZ_GCC_VERSION_AT_LEAST(4, 7, 0)
/* use designated array initializers if supported */
#define CONVERT(in_, out_) \
  [in_] = out_
#else
/* otherwise init array element by position */
#define CONVERT(in_, out_) \
  out_
#endif

#define MAX_UUID_SIZE 16

BEGIN_BLUETOOTH_NAMESPACE

template<class T>
struct interface_traits
{ };

//
// Conversion
//

static nsresult
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

static nsresult
Convert(int aIn, BluetoothStatus& aOut)
{
  return Convert(static_cast<bt_status_t>(aIn), aOut);
}

static nsresult
Convert(const nsAString& aIn, bt_property_type_t& aOut)
{
  if (aIn.EqualsLiteral("Name")) {
    aOut = BT_PROPERTY_BDNAME;
  } else if (aIn.EqualsLiteral("Discoverable")) {
    aOut = BT_PROPERTY_ADAPTER_SCAN_MODE;
  } else if (aIn.EqualsLiteral("DiscoverableTimeout")) {
    aOut = BT_PROPERTY_ADAPTER_DISCOVERY_TIMEOUT;
  } else {
    BT_LOGR("Invalid property name: %s", NS_ConvertUTF16toUTF8(aIn).get());
    return NS_ERROR_ILLEGAL_VALUE;
  }
  return NS_OK;
}

static nsresult
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


static nsresult
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

static nsresult
Convert(ConvertNamedValue& aIn, bt_property_t& aOut)
{
  nsresult rv = Convert(aIn.mNamedValue.name(), aOut.type);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (aIn.mNamedValue.value().type() == BluetoothValue::Tuint32_t) {
    // Set discoverable timeout
    aOut.val =
      reinterpret_cast<void*>(aIn.mNamedValue.value().get_uint32_t());
  } else if (aIn.mNamedValue.value().type() == BluetoothValue::TnsString) {
    // Set name
    aIn.mStringValue =
      NS_ConvertUTF16toUTF8(aIn.mNamedValue.value().get_nsString());
    aOut.val =
      const_cast<void*>(static_cast<const void*>(aIn.mStringValue.get()));
    aOut.len = strlen(static_cast<char*>(aOut.val));
  } else if (aIn.mNamedValue.value().type() == BluetoothValue::Tbool) {
    // Set scan mode
    rv = Convert(aIn.mNamedValue.value().get_bool(), aIn.mScanMode);
    if (NS_FAILED(rv)) {
      return rv;
    }
    aOut.val = &aIn.mScanMode;
    aOut.len = sizeof(aIn.mScanMode);
  } else {
    BT_LOGR("Invalid property value type");
    return NS_ERROR_ILLEGAL_VALUE;
  }

  return NS_OK;
}

static nsresult
Convert(const nsAString& aIn, bt_bdaddr_t& aOut)
{
  NS_ConvertUTF16toUTF8 bdAddressUTF8(aIn);
  const char* str = bdAddressUTF8.get();

  for (size_t i = 0; i < MOZ_ARRAY_LENGTH(aOut.address); ++i, ++str) {
    aOut.address[i] =
      static_cast<uint8_t>(strtoul(str, const_cast<char**>(&str), 16));
  }

  return NS_OK;
}

static nsresult
Convert(const nsAString& aIn, bt_ssp_variant_t& aOut)
{
  if (aIn.EqualsLiteral("PasskeyConfirmation")) {
    aOut = BT_SSP_VARIANT_PASSKEY_CONFIRMATION;
  } else if (aIn.EqualsLiteral("PasskeyEntry")) {
    aOut = BT_SSP_VARIANT_PASSKEY_ENTRY;
  } else if (aIn.EqualsLiteral("Consent")) {
    aOut = BT_SSP_VARIANT_CONSENT;
  } else if (aIn.EqualsLiteral("PasskeyNotification")) {
    aOut = BT_SSP_VARIANT_PASSKEY_NOTIFICATION;
  } else {
    BT_LOGR("Invalid SSP variant name: %s", NS_ConvertUTF16toUTF8(aIn).get());
    aOut = BT_SSP_VARIANT_PASSKEY_CONFIRMATION; // silences compiler warning
    return NS_ERROR_ILLEGAL_VALUE;
  }
  return NS_OK;
}

static nsresult
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

static nsresult
Convert(const bool& aIn, uint8_t& aOut)
{
  // casting converts true/false to either 1 or 0
  aOut = static_cast<uint8_t>(aIn);
  return NS_OK;
}

static nsresult
Convert(const uint8_t aIn[16], bt_uuid_t& aOut)
{
  if (sizeof(aOut.uu) != 16) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  memcpy(aOut.uu, aIn, sizeof(aOut.uu));

  return NS_OK;
}

static nsresult
Convert(const bt_uuid_t& aIn, BluetoothUuid& aOut)
{
  if (sizeof(aIn.uu) != sizeof(aOut.mUuid)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  memcpy(aOut.mUuid, aIn.uu, sizeof(aOut.mUuid));

  return NS_OK;
}

static nsresult
Convert(const nsAString& aIn, bt_pin_code_t& aOut)
{
  if (aIn.Length() > MOZ_ARRAY_LENGTH(aOut.pin)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  NS_ConvertUTF16toUTF8 pinCodeUTF8(aIn);
  const char* str = pinCodeUTF8.get();

  nsAString::size_type i;

  // Fill pin into aOut
  for (i = 0; i < aIn.Length(); ++i, ++str) {
    aOut.pin[i] = static_cast<uint8_t>(*str);
  }

  // Clear remaining bytes in aOut
  size_t ntrailing =
    (MOZ_ARRAY_LENGTH(aOut.pin) - aIn.Length()) * sizeof(aOut.pin[0]);
  memset(aOut.pin + aIn.Length(), 0, ntrailing);

  return NS_OK;
}

static nsresult
Convert(const bt_bdaddr_t& aIn, nsAString& aOut)
{
  char str[BLUETOOTH_ADDRESS_LENGTH + 1];

  int res = snprintf(str, sizeof(str), "%02x:%02x:%02x:%02x:%02x:%02x",
                     static_cast<int>(aIn.address[0]),
                     static_cast<int>(aIn.address[1]),
                     static_cast<int>(aIn.address[2]),
                     static_cast<int>(aIn.address[3]),
                     static_cast<int>(aIn.address[4]),
                     static_cast<int>(aIn.address[5]));
  if (res < 0) {
    return NS_ERROR_ILLEGAL_VALUE;
  } else if ((size_t)res >= sizeof(str)) {
    return NS_ERROR_OUT_OF_MEMORY; /* string buffer too small */
  }

  aOut = NS_ConvertUTF8toUTF16(str);

  return NS_OK;
}

static nsresult
Convert(const bt_bdaddr_t* aIn, nsAString& aOut)
{
  if (!aIn) {
    aOut.AssignLiteral(BLUETOOTH_ADDRESS_NONE);
    return NS_OK;
  }
  return Convert(*aIn, aOut);
}

static nsresult
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

static nsresult
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
    CONVERT(BT_PROPERTY_REMOTE_RSSI, PROPERTY_REMOTE_RSSI),
    CONVERT(BT_PROPERTY_REMOTE_VERSION_INFO,PROPERTY_REMOTE_VERSION_INFO)
  };
  if (aIn == BT_PROPERTY_REMOTE_DEVICE_TIMESTAMP) {
    /* This case is handled separately to not populate
     * |sPropertyType| with empty entries. */
    aOut = PROPERTY_REMOTE_DEVICE_TIMESTAMP;
    return NS_OK;
  }
  if (!aIn || aIn >= MOZ_ARRAY_LENGTH(sPropertyType)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sPropertyType[aIn];
  return NS_OK;
}

static nsresult
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

static nsresult
Convert(const char* aIn, nsACString& aOut)
{
  aOut.Assign(aIn);

  return NS_OK;
}

static nsresult
Convert(const char* aIn, nsAString& aOut)
{
  aOut = NS_ConvertUTF8toUTF16(aIn);

  return NS_OK;
}

static nsresult
Convert(const bt_bdname_t& aIn, nsAString& aOut)
{
  return Convert(reinterpret_cast<const char*>(aIn.name), aOut);
}

static nsresult
Convert(const bt_bdname_t* aIn, nsAString& aOut)
{
  if (!aIn) {
    aOut.Truncate();
    return NS_OK;
  }
  return Convert(*aIn, aOut);
}

static nsresult
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

static nsresult
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

static nsresult
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

static nsresult
Convert(const bt_service_record_t& aIn, BluetoothServiceRecord& aOut)
{
  nsresult rv = Convert(aIn.uuid, aOut.mUuid);
  if (NS_FAILED(rv)) {
    return rv;
  }

  aOut.mChannel = aIn.channel;

  MOZ_ASSERT(sizeof(aIn.name) == sizeof(aOut.mName));
  memcpy(aOut.mName, aIn.name, sizeof(aOut.mName));

  return NS_OK;
}

static nsresult
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

static nsresult
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

static nsresult
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

static nsresult
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

static nsresult
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

static nsresult
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

static nsresult
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

static nsresult
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

static nsresult
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

static nsresult
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

static nsresult
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

static nsresult
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

static nsresult
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

static nsresult
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

static nsresult
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

static nsresult
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

static nsresult
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

static nsresult
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
static nsresult
Convert(const bt_remote_version_t& aIn, BluetoothRemoteInfo& aOut)
{
  aOut.mVerMajor = aIn.version;
  aOut.mVerMinor = aIn.sub_ver;
  aOut.mManufacturer = aIn.manufacturer;

  return NS_OK;
}

static nsresult
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

static nsresult
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

static nsresult
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

static nsresult
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

static nsresult
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

static nsresult
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

static nsresult
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

static nsresult
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

static nsresult
Convert(const BluetoothAvrcpElementAttribute& aIn, btrc_element_attr_val_t& aOut)
{
  const NS_ConvertUTF16toUTF8 value(aIn.mValue);
  size_t len = std::min<size_t>(strlen(value.get()), sizeof(aOut.text) - 1);

  memcpy(aOut.text, value.get(), len);
  aOut.text[len] = '\0';
  aOut.attr_id = aIn.mId;

  return NS_OK;
}

static nsresult
Convert(const btrc_player_settings_t& aIn, BluetoothAvrcpPlayerSettings& aOut)
{
  aOut.mNumAttr = aIn.num_attr;
  memcpy(aOut.mIds, aIn.attr_ids, aIn.num_attr);
  memcpy(aOut.mValues, aIn.attr_values, aIn.num_attr);

  return NS_OK;
}
#endif // ANDROID_VERSION >= 18

#if ANDROID_VERSION >= 19
static nsresult
Convert(btrc_remote_features_t aIn, unsigned long& aOut)
{
  /* The input type's name is misleading. The converted value is
   * actually a bitmask.
   */
  aOut = static_cast<unsigned long>(aIn);
  return NS_OK;
}
#endif // ANDROID_VERSION >= 19

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
static nsresult
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
static nsresult
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
static Tout
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
static nsresult
Convert(const T& aIn, T& aOut)
{
  aOut = aIn;

  return NS_OK;
}

static nsresult
Convert(const bt_property_t& aIn, BluetoothProperty& aOut)
{
  /* type conversion */

  nsresult rv = Convert(aIn.type, aOut.mType);
  if (NS_FAILED(rv)) {
    return rv;
  }

  /* value conversion */

  switch (aOut.mType) {
    case PROPERTY_BDNAME:
      /* fall through */
    case PROPERTY_REMOTE_FRIENDLY_NAME:
      {
        // We construct an nsCString here because bdname
        // returned from Bluedroid is not 0-terminated.
        aOut.mString = NS_ConvertUTF8toUTF16(
          nsCString(static_cast<char*>(aIn.val), aIn.len));
      }
      break;
    case PROPERTY_BDADDR:
      rv = Convert(*static_cast<bt_bdaddr_t*>(aIn.val), aOut.mString);
      break;
    case PROPERTY_UUIDS:
      {
        size_t numUuids = aIn.len / MAX_UUID_SIZE;
        ConvertArray<bt_uuid_t> array(
          static_cast<bt_uuid_t*>(aIn.val), numUuids);
        aOut.mUuidArray.SetLength(numUuids);
        rv = Convert(array, aOut.mUuidArray);
      }
      break;
    case PROPERTY_CLASS_OF_DEVICE:
      /* fall through */
    case PROPERTY_ADAPTER_DISCOVERY_TIMEOUT:
      aOut.mUint32 = *static_cast<uint32_t*>(aIn.val);
      break;
    case PROPERTY_TYPE_OF_DEVICE:
      rv = Convert(*static_cast<bt_device_type_t*>(aIn.val),
                   aOut.mDeviceType);
      break;
    case PROPERTY_SERVICE_RECORD:
      rv = Convert(*static_cast<bt_service_record_t*>(aIn.val),
                   aOut.mServiceRecord);
      break;
    case PROPERTY_ADAPTER_SCAN_MODE:
      rv = Convert(*static_cast<bt_scan_mode_t*>(aIn.val),
                   aOut.mScanMode);
      break;
    case PROPERTY_ADAPTER_BONDED_DEVICES:
      {
        size_t numAddresses = aIn.len / BLUETOOTH_ADDRESS_BYTES;
        ConvertArray<bt_bdaddr_t> array(
          static_cast<bt_bdaddr_t*>(aIn.val), numAddresses);
        aOut.mStringArray.SetLength(numAddresses);
        rv = Convert(array, aOut.mStringArray);
      }
      break;
    case PROPERTY_REMOTE_RSSI:
      aOut.mInt32 = *static_cast<int32_t*>(aIn.val);
      break;
#if ANDROID_VERSION >= 18
    case PROPERTY_REMOTE_VERSION_INFO:
      rv = Convert(*static_cast<bt_remote_version_t*>(aIn.val),
                   aOut.mRemoteInfo);
      break;
#endif
    case PROPERTY_REMOTE_DEVICE_TIMESTAMP:
      /* nothing to do */
      break;
    default:
      /* mismatch with type conversion */
      NS_NOTREACHED("Unhandled property type");
      break;
  }
  if (NS_FAILED(rv)) {
    return rv;
  }
  return NS_OK;
}

//
// Result handling
//

template <typename Obj, typename Res>
class BluetoothInterfaceRunnable0 : public nsRunnable
{
public:
  BluetoothInterfaceRunnable0(Obj* aObj, Res (Obj::*aMethod)())
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
class BluetoothInterfaceRunnable1 : public nsRunnable
{
public:
  BluetoothInterfaceRunnable1(Obj* aObj, Res (Obj::*aMethod)(Arg1),
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
class BluetoothInterfaceRunnable3 : public nsRunnable
{
public:
  BluetoothInterfaceRunnable3(Obj* aObj,
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
class BluetoothNotificationRunnable0 : public nsRunnable
{
public:
  typedef typename ObjectWrapper::ObjectType  ObjectType;
  typedef BluetoothNotificationRunnable0<ObjectWrapper, Res> SelfType;

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
      BT_WARNING("BluetoothNotificationRunnable0::Create failed");
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
  BluetoothNotificationRunnable0(Res (ObjectType::*aMethod)())
  : mMethod(aMethod)
  {
    MOZ_ASSERT(mMethod);
  }

  Res (ObjectType::*mMethod)();
};

template <typename ObjectWrapper, typename Res,
          typename Tin1, typename Arg1=Tin1>
class BluetoothNotificationRunnable1 : public nsRunnable
{
public:
  typedef typename ObjectWrapper::ObjectType  ObjectType;
  typedef BluetoothNotificationRunnable1<ObjectWrapper, Res,
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
      BT_WARNING("BluetoothNotificationRunnable1::Create failed");
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
  BluetoothNotificationRunnable1(Res (ObjectType::*aMethod)(Arg1))
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
class BluetoothNotificationRunnable2 : public nsRunnable
{
public:
  typedef typename ObjectWrapper::ObjectType  ObjectType;
  typedef BluetoothNotificationRunnable2<ObjectWrapper, Res,
                                         Tin1, Tin2, Arg1, Arg2> SelfType;

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
      BT_WARNING("BluetoothNotificationRunnable2::Create failed");
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
  BluetoothNotificationRunnable2(Res (ObjectType::*aMethod)(Arg1, Arg2))
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
class BluetoothNotificationRunnable3 : public nsRunnable
{
public:
  typedef typename ObjectWrapper::ObjectType  ObjectType;
  typedef BluetoothNotificationRunnable3<ObjectWrapper, Res,
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
      BT_WARNING("BluetoothNotificationRunnable3::Create failed");
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
  BluetoothNotificationRunnable3(Res (ObjectType::*aMethod)(Arg1, Arg2, Arg3))
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
class BluetoothNotificationRunnable4 : public nsRunnable
{
public:
  typedef typename ObjectWrapper::ObjectType  ObjectType;
  typedef BluetoothNotificationRunnable4<ObjectWrapper, Res,
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
      BT_WARNING("BluetoothNotificationRunnable4::Create failed");
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
  BluetoothNotificationRunnable4(
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
class BluetoothNotificationRunnable5 : public nsRunnable
{
public:
  typedef typename ObjectWrapper::ObjectType  ObjectType;
  typedef BluetoothNotificationRunnable5<ObjectWrapper, Res,
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
      BT_WARNING("BluetoothNotificationRunnable5::Create failed");
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
  BluetoothNotificationRunnable5(Res (ObjectType::*aMethod)(Arg1, Arg2, Arg3,
                                                            Arg4, Arg5))
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

typedef
  BluetoothInterfaceRunnable1<BluetoothSocketResultHandler, void, int, int>
  BluetoothSocketIntResultRunnable;

typedef
  BluetoothInterfaceRunnable3<BluetoothSocketResultHandler, void,
                              int, const nsString, int,
                              int, const nsAString_internal&, int>
  BluetoothSocketIntStringIntResultRunnable;

typedef
  BluetoothInterfaceRunnable1<BluetoothSocketResultHandler, void,
                              BluetoothStatus, BluetoothStatus>
  BluetoothSocketErrorRunnable;

static nsresult
DispatchBluetoothSocketResult(BluetoothSocketResultHandler* aRes,
                              void (BluetoothSocketResultHandler::*aMethod)(int),
                              int aArg, BluetoothStatus aStatus)
{
  MOZ_ASSERT(aRes);

  nsRunnable* runnable;

  if (aStatus == STATUS_SUCCESS) {
    runnable = new BluetoothSocketIntResultRunnable(aRes, aMethod, aArg);
  } else {
    runnable = new BluetoothSocketErrorRunnable(aRes,
      &BluetoothSocketResultHandler::OnError, aStatus);
  }
  nsresult rv = NS_DispatchToMainThread(runnable);
  if (NS_FAILED(rv)) {
    BT_WARNING("NS_DispatchToMainThread failed: %X", rv);
  }
  return rv;
}

static nsresult
DispatchBluetoothSocketResult(
  BluetoothSocketResultHandler* aRes,
  void (BluetoothSocketResultHandler::*aMethod)(int, const nsAString&, int),
  int aArg1, const nsAString& aArg2, int aArg3, BluetoothStatus aStatus)
{
  MOZ_ASSERT(aRes);

  nsRunnable* runnable;

  if (aStatus == STATUS_SUCCESS) {
    runnable = new BluetoothSocketIntStringIntResultRunnable(aRes, aMethod,
                                                             aArg1, aArg2,
                                                             aArg3);
  } else {
    runnable = new BluetoothSocketErrorRunnable(aRes,
      &BluetoothSocketResultHandler::OnError, aStatus);
  }
  nsresult rv = NS_DispatchToMainThread(runnable);
  if (NS_FAILED(rv)) {
    BT_WARNING("NS_DispatchToMainThread failed: %X", rv);
  }
  return rv;
}

void
BluetoothSocketInterface::Listen(BluetoothSocketType aType,
                                 const nsAString& aServiceName,
                                 const uint8_t aServiceUuid[16],
                                 int aChannel, bool aEncrypt, bool aAuth,
                                 BluetoothSocketResultHandler* aRes)
{
  int fd;
  bt_status_t status;
  btsock_type_t type = BTSOCK_RFCOMM; // silences compiler warning

  if (NS_SUCCEEDED(Convert(aType, type))) {
    status = mInterface->listen(type,
                                NS_ConvertUTF16toUTF8(aServiceName).get(),
                                aServiceUuid, aChannel, &fd,
                                (BTSOCK_FLAG_ENCRYPT * aEncrypt) |
                                (BTSOCK_FLAG_AUTH * aAuth));
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothSocketResult(aRes, &BluetoothSocketResultHandler::Listen,
                                  fd, ConvertDefault(status, STATUS_FAIL));
  }
}

#define CMSGHDR_CONTAINS_FD(_cmsghdr) \
    ( ((_cmsghdr)->cmsg_level == SOL_SOCKET) && \
      ((_cmsghdr)->cmsg_type == SCM_RIGHTS) )

/* |SocketMessageWatcher| receives Bluedroid's socket setup
 * messages on the I/O thread. You need to inherit from this
 * class to make use of it.
 *
 * Bluedroid sends two socket info messages (20 bytes) at
 * the beginning of a connection to both peers.
 *
 *   - 1st message: [channel:4]
 *   - 2nd message: [size:2][bd address:6][channel:4][connection status:4]
 *
 * On the server side, the second message will contain a
 * socket file descriptor for the connection. The client
 * uses the original file descriptor.
 */
class SocketMessageWatcher : public MessageLoopForIO::Watcher
{
public:
  static const unsigned char MSG1_SIZE = 4;
  static const unsigned char MSG2_SIZE = 16;

  static const unsigned char OFF_CHANNEL1 = 0;
  static const unsigned char OFF_SIZE = 4;
  static const unsigned char OFF_BDADDRESS = 6;
  static const unsigned char OFF_CHANNEL2 = 12;
  static const unsigned char OFF_STATUS = 16;

  SocketMessageWatcher(int aFd)
  : mFd(aFd)
  , mClientFd(-1)
  , mLen(0)
  { }

  virtual ~SocketMessageWatcher()
  { }

  virtual void Proceed(BluetoothStatus aStatus) = 0;

  void OnFileCanReadWithoutBlocking(int aFd) MOZ_OVERRIDE
  {
    BluetoothStatus status;

    switch (mLen) {
      case 0:
        status = RecvMsg1();
        break;
      case MSG1_SIZE:
        status = RecvMsg2();
        break;
      default:
        /* message-size error */
        status = STATUS_FAIL;
        break;
    }

    if (IsComplete() || status != STATUS_SUCCESS) {
      mWatcher.StopWatchingFileDescriptor();
      Proceed(status);
    }
  }

  void OnFileCanWriteWithoutBlocking(int aFd) MOZ_OVERRIDE
  { }

  void Watch()
  {
    MessageLoopForIO::current()->WatchFileDescriptor(
      mFd,
      true,
      MessageLoopForIO::WATCH_READ,
      &mWatcher,
      this);
  }

  bool IsComplete() const
  {
    return mLen == (MSG1_SIZE + MSG2_SIZE);
  }

  int GetFd() const
  {
    return mFd;
  }

  int32_t GetChannel1() const
  {
    return ReadInt32(OFF_CHANNEL1);
  }

  int32_t GetSize() const
  {
    return ReadInt16(OFF_SIZE);
  }

  nsString GetBdAddress() const
  {
    nsString bdAddress;
    ReadBdAddress(OFF_BDADDRESS, bdAddress);
    return bdAddress;
  }

  int32_t GetChannel2() const
  {
    return ReadInt32(OFF_CHANNEL2);
  }

  int32_t GetConnectionStatus() const
  {
    return ReadInt32(OFF_STATUS);
  }

  int GetClientFd() const
  {
    return mClientFd;
  }

private:
  BluetoothStatus RecvMsg1()
  {
    struct iovec iv;
    memset(&iv, 0, sizeof(iv));
    iv.iov_base = mBuf;
    iv.iov_len = MSG1_SIZE;

    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = &iv;
    msg.msg_iovlen = 1;

    ssize_t res = TEMP_FAILURE_RETRY(recvmsg(mFd, &msg, MSG_NOSIGNAL));
    if (res < 0) {
      return STATUS_FAIL;
    }

    mLen += res;

    return STATUS_SUCCESS;
  }

  BluetoothStatus RecvMsg2()
  {
    struct iovec iv;
    memset(&iv, 0, sizeof(iv));
    iv.iov_base = mBuf + MSG1_SIZE;
    iv.iov_len = MSG2_SIZE;

    struct msghdr msg;
    struct cmsghdr cmsgbuf[2 * sizeof(cmsghdr) + 0x100];
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = &iv;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsgbuf;
    msg.msg_controllen = sizeof(cmsgbuf);

    ssize_t res = TEMP_FAILURE_RETRY(recvmsg(mFd, &msg, MSG_NOSIGNAL));
    if (res < 0) {
      return STATUS_FAIL;
    }

    mLen += res;

    if (msg.msg_flags & (MSG_CTRUNC | MSG_OOB | MSG_ERRQUEUE)) {
      return STATUS_FAIL;
    }

    struct cmsghdr *cmsgptr = CMSG_FIRSTHDR(&msg);

    // Extract client fd from message header
    for (; cmsgptr; cmsgptr = CMSG_NXTHDR(&msg, cmsgptr)) {
      if (CMSGHDR_CONTAINS_FD(cmsgptr)) {
        // if multiple file descriptors have been sent, we close
        // all but the final one.
        if (mClientFd != -1) {
          TEMP_FAILURE_RETRY(close(mClientFd));
        }
        // retrieve sent client fd
        mClientFd = *(static_cast<int*>(CMSG_DATA(cmsgptr)));
      }
    }

    return STATUS_SUCCESS;
  }

  int16_t ReadInt16(unsigned long aOffset) const
  {
    /* little-endian buffer */
    return (static_cast<int16_t>(mBuf[aOffset + 1]) << 8) |
            static_cast<int16_t>(mBuf[aOffset]);
  }

  int32_t ReadInt32(unsigned long aOffset) const
  {
    /* little-endian buffer */
    return (static_cast<int32_t>(mBuf[aOffset + 3]) << 24) |
           (static_cast<int32_t>(mBuf[aOffset + 2]) << 16) |
           (static_cast<int32_t>(mBuf[aOffset + 1]) << 8) |
            static_cast<int32_t>(mBuf[aOffset]);
  }

  void ReadBdAddress(unsigned long aOffset, nsAString& aBdAddress) const
  {
    const bt_bdaddr_t* bdAddress =
      reinterpret_cast<const bt_bdaddr_t*>(mBuf+aOffset);

    if (NS_FAILED(Convert(*bdAddress, aBdAddress))) {
      aBdAddress.AssignLiteral(BLUETOOTH_ADDRESS_NONE);
    }
  }

  MessageLoopForIO::FileDescriptorWatcher mWatcher;
  int mFd;
  int mClientFd;
  unsigned char mLen;
  uint8_t mBuf[MSG1_SIZE + MSG2_SIZE];
};

/* |SocketMessageWatcherTask| starts a SocketMessageWatcher
 * on the I/O task
 */
class SocketMessageWatcherTask MOZ_FINAL : public Task
{
public:
  SocketMessageWatcherTask(SocketMessageWatcher* aWatcher)
  : mWatcher(aWatcher)
  {
    MOZ_ASSERT(mWatcher);
  }

  void Run() MOZ_OVERRIDE
  {
    mWatcher->Watch();
  }

private:
  SocketMessageWatcher* mWatcher;
};

/* |DeleteTask| deletes a class instance on the I/O thread
 */
template <typename T>
class DeleteTask MOZ_FINAL : public Task
{
public:
  DeleteTask(T* aPtr)
  : mPtr(aPtr)
  { }

  void Run() MOZ_OVERRIDE
  {
    mPtr = nullptr;
  }

private:
  nsAutoPtr<T> mPtr;
};

/* |ConnectWatcher| specializes SocketMessageWatcher for
 * connect operations by reading the socket messages from
 * Bluedroid and forwarding the connected socket to the
 * resource handler.
 */
class ConnectWatcher MOZ_FINAL : public SocketMessageWatcher
{
public:
  ConnectWatcher(int aFd, BluetoothSocketResultHandler* aRes)
  : SocketMessageWatcher(aFd)
  , mRes(aRes)
  { }

  void Proceed(BluetoothStatus aStatus) MOZ_OVERRIDE
  {
    if (mRes) {
      DispatchBluetoothSocketResult(mRes,
                                   &BluetoothSocketResultHandler::Connect,
                                    GetFd(), GetBdAddress(),
                                    GetConnectionStatus(), aStatus);
    }
    MessageLoopForIO::current()->PostTask(
      FROM_HERE, new DeleteTask<ConnectWatcher>(this));
  }

private:
  nsRefPtr<BluetoothSocketResultHandler> mRes;
};

void
BluetoothSocketInterface::Connect(const nsAString& aBdAddr,
                                  BluetoothSocketType aType,
                                  const uint8_t aUuid[16],
                                  int aChannel, bool aEncrypt, bool aAuth,
                                  BluetoothSocketResultHandler* aRes)
{
  int fd;
  bt_status_t status;
  bt_bdaddr_t bdAddr;
  btsock_type_t type = BTSOCK_RFCOMM; // silences compiler warning

  if (NS_SUCCEEDED(Convert(aBdAddr, bdAddr)) &&
      NS_SUCCEEDED(Convert(aType, type))) {
    status = mInterface->connect(&bdAddr, type, aUuid, aChannel, &fd,
                                 (BTSOCK_FLAG_ENCRYPT * aEncrypt) |
                                 (BTSOCK_FLAG_AUTH * aAuth));
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (status == BT_STATUS_SUCCESS) {
    /* receive Bluedroid's socket-setup messages */
    Task* t = new SocketMessageWatcherTask(new ConnectWatcher(fd, aRes));
    XRE_GetIOMessageLoop()->PostTask(FROM_HERE, t);
  } else if (aRes) {
    DispatchBluetoothSocketResult(aRes,
                                  &BluetoothSocketResultHandler::Connect,
                                  -1, EmptyString(), 0,
                                  ConvertDefault(status, STATUS_FAIL));
  }
}

/* Specializes SocketMessageWatcher for Accept operations by
 * reading the socket messages from Bluedroid and forwarding
 * the received client socket to the resource handler. The
 * first message is received immediately. When there's a new
 * connection, Bluedroid sends the 2nd message with the socket
 * info and socket file descriptor.
 */
class AcceptWatcher MOZ_FINAL : public SocketMessageWatcher
{
public:
  AcceptWatcher(int aFd, BluetoothSocketResultHandler* aRes)
  : SocketMessageWatcher(aFd)
  , mRes(aRes)
  {
    /* not supplying a result handler leaks received file descriptor */
    MOZ_ASSERT(mRes);
  }

  void Proceed(BluetoothStatus aStatus) MOZ_OVERRIDE
  {
    if (mRes) {
      DispatchBluetoothSocketResult(mRes,
                                    &BluetoothSocketResultHandler::Accept,
                                    GetClientFd(), GetBdAddress(),
                                    GetConnectionStatus(),
                                    aStatus);
    }
    MessageLoopForIO::current()->PostTask(
      FROM_HERE, new DeleteTask<AcceptWatcher>(this));
  }

private:
  nsRefPtr<BluetoothSocketResultHandler> mRes;
};

void
BluetoothSocketInterface::Accept(int aFd, BluetoothSocketResultHandler* aRes)
{
  /* receive Bluedroid's socket-setup messages and client fd */
  Task* t = new SocketMessageWatcherTask(new AcceptWatcher(aFd, aRes));
  XRE_GetIOMessageLoop()->PostTask(FROM_HERE, t);
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

typedef
  BluetoothInterfaceRunnable0<BluetoothHandsfreeResultHandler, void>
  BluetoothHandsfreeResultRunnable;

typedef
  BluetoothInterfaceRunnable1<BluetoothHandsfreeResultHandler, void,
                              BluetoothStatus, BluetoothStatus>
  BluetoothHandsfreeErrorRunnable;

static nsresult
DispatchBluetoothHandsfreeResult(
  BluetoothHandsfreeResultHandler* aRes,
  void (BluetoothHandsfreeResultHandler::*aMethod)(),
  BluetoothStatus aStatus)
{
  MOZ_ASSERT(aRes);

  nsRunnable* runnable;

  if (aStatus == STATUS_SUCCESS) {
    runnable = new BluetoothHandsfreeResultRunnable(aRes, aMethod);
  } else {
    runnable = new BluetoothHandsfreeErrorRunnable(aRes,
      &BluetoothHandsfreeResultHandler::OnError, aStatus);
  }
  nsresult rv = NS_DispatchToMainThread(runnable);
  if (NS_FAILED(rv)) {
    BT_WARNING("NS_DispatchToMainThread failed: %X", rv);
  }
  return rv;
}

// Notification handling
//

BluetoothHandsfreeNotificationHandler::
  ~BluetoothHandsfreeNotificationHandler()
{ }

static BluetoothHandsfreeNotificationHandler* sHandsfreeNotificationHandler;

struct BluetoothHandsfreeCallback
{
  class HandsfreeNotificationHandlerWrapper
  {
  public:
    typedef BluetoothHandsfreeNotificationHandler ObjectType;

    static ObjectType* GetInstance()
    {
      MOZ_ASSERT(NS_IsMainThread());

      return sHandsfreeNotificationHandler;
    }
  };

  // Notifications

  typedef BluetoothNotificationRunnable2<HandsfreeNotificationHandlerWrapper,
                                         void,
                                         BluetoothHandsfreeConnectionState,
                                         nsString,
                                         BluetoothHandsfreeConnectionState,
                                         const nsAString&>
    ConnectionStateNotification;

  typedef BluetoothNotificationRunnable2<HandsfreeNotificationHandlerWrapper,
                                         void,
                                         BluetoothHandsfreeAudioState,
                                         nsString,
                                         BluetoothHandsfreeAudioState,
                                         const nsAString&>
    AudioStateNotification;

  typedef BluetoothNotificationRunnable1<HandsfreeNotificationHandlerWrapper,
                                         void,
                                         BluetoothHandsfreeVoiceRecognitionState>
    VoiceRecognitionNotification;

  typedef BluetoothNotificationRunnable0<HandsfreeNotificationHandlerWrapper,
                                         void>
    AnswerCallNotification;

  typedef BluetoothNotificationRunnable0<HandsfreeNotificationHandlerWrapper,
                                         void>
    HangupCallNotification;

  typedef BluetoothNotificationRunnable2<HandsfreeNotificationHandlerWrapper,
                                         void,
                                         BluetoothHandsfreeVolumeType, int>
    VolumeNotification;

  typedef BluetoothNotificationRunnable1<HandsfreeNotificationHandlerWrapper,
                                         void, nsString, const nsAString&>
    DialCallNotification;

  typedef BluetoothNotificationRunnable1<HandsfreeNotificationHandlerWrapper,
                                         void, char>
    DtmfNotification;

  typedef BluetoothNotificationRunnable1<HandsfreeNotificationHandlerWrapper,
                                         void,
                                         BluetoothHandsfreeNRECState>
    NRECNotification;

  typedef BluetoothNotificationRunnable1<HandsfreeNotificationHandlerWrapper,
                                         void,
                                         BluetoothHandsfreeCallHoldType>
    CallHoldNotification;

  typedef BluetoothNotificationRunnable0<HandsfreeNotificationHandlerWrapper,
                                         void>
    CnumNotification;

  typedef BluetoothNotificationRunnable0<HandsfreeNotificationHandlerWrapper,
                                         void>
    CindNotification;

  typedef BluetoothNotificationRunnable0<HandsfreeNotificationHandlerWrapper,
                                         void>
    CopsNotification;

  typedef BluetoothNotificationRunnable0<HandsfreeNotificationHandlerWrapper,
                                         void>
    ClccNotification;

  typedef BluetoothNotificationRunnable1<HandsfreeNotificationHandlerWrapper,
                                         void, nsCString, const nsACString&>
    UnknownAtNotification;

  typedef BluetoothNotificationRunnable0<HandsfreeNotificationHandlerWrapper,
                                         void>
    KeyPressedNotification;

  // Bluedroid Handsfree callbacks

  static void
  ConnectionState(bthf_connection_state_t aState, bt_bdaddr_t* aBdAddr)
  {
    ConnectionStateNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::ConnectionStateNotification,
      aState, aBdAddr);
  }

  static void
  AudioState(bthf_audio_state_t aState, bt_bdaddr_t* aBdAddr)
  {
    AudioStateNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::AudioStateNotification,
      aState, aBdAddr);
  }

  static void
  VoiceRecognition(bthf_vr_state_t aState)
  {
    VoiceRecognitionNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::VoiceRecognitionNotification,
      aState);
  }

  static void
  AnswerCall()
  {
    AnswerCallNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::AnswerCallNotification);
  }

  static void
  HangupCall()
  {
    HangupCallNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::HangupCallNotification);
  }

  static void
  Volume(bthf_volume_type_t aType, int aVolume)
  {
    VolumeNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::VolumeNotification,
      aType, aVolume);
  }

  static void
  DialCall(char* aNumber)
  {
    DialCallNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::DialCallNotification, aNumber);
  }

  static void
  Dtmf(char aDtmf)
  {
    DtmfNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::DtmfNotification, aDtmf);
  }

  static void
  NoiseReductionEchoCancellation(bthf_nrec_t aNrec)
  {
    NRECNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::NRECNotification, aNrec);
  }

  static void
  CallHold(bthf_chld_type_t aChld)
  {
    CallHoldNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::CallHoldNotification, aChld);
  }

  static void
  Cnum()
  {
    CnumNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::CnumNotification);
  }

  static void
  Cind()
  {
    CindNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::CindNotification);
  }

  static void
  Cops()
  {
    CopsNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::CopsNotification);
  }

  static void
  Clcc()
  {
    ClccNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::ClccNotification);
  }

  static void
  UnknownAt(char* aAtString)
  {
    UnknownAtNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::UnknownAtNotification,
      aAtString);
  }

  static void
  KeyPressed()
  {
    KeyPressedNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::KeyPressedNotification);
  }
};

// Interface
//

BluetoothHandsfreeInterface::BluetoothHandsfreeInterface(
  const bthf_interface_t* aInterface)
: mInterface(aInterface)
{
  MOZ_ASSERT(mInterface);
}

BluetoothHandsfreeInterface::~BluetoothHandsfreeInterface()
{ }

void
BluetoothHandsfreeInterface::Init(
  BluetoothHandsfreeNotificationHandler* aNotificationHandler,
  BluetoothHandsfreeResultHandler* aRes)
{
  static bthf_callbacks_t sCallbacks = {
    sizeof(sCallbacks),
    BluetoothHandsfreeCallback::ConnectionState,
    BluetoothHandsfreeCallback::AudioState,
    BluetoothHandsfreeCallback::VoiceRecognition,
    BluetoothHandsfreeCallback::AnswerCall,
    BluetoothHandsfreeCallback::HangupCall,
    BluetoothHandsfreeCallback::Volume,
    BluetoothHandsfreeCallback::DialCall,
    BluetoothHandsfreeCallback::Dtmf,
    BluetoothHandsfreeCallback::NoiseReductionEchoCancellation,
    BluetoothHandsfreeCallback::CallHold,
    BluetoothHandsfreeCallback::Cnum,
    BluetoothHandsfreeCallback::Cind,
    BluetoothHandsfreeCallback::Cops,
    BluetoothHandsfreeCallback::Clcc,
    BluetoothHandsfreeCallback::UnknownAt,
    BluetoothHandsfreeCallback::KeyPressed
  };

  sHandsfreeNotificationHandler = aNotificationHandler;

  bt_status_t status = mInterface->init(&sCallbacks);

  if (aRes) {
    DispatchBluetoothHandsfreeResult(aRes,
                                     &BluetoothHandsfreeResultHandler::Init,
                                     ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothHandsfreeInterface::Cleanup(BluetoothHandsfreeResultHandler* aRes)
{
  mInterface->cleanup();

  if (aRes) {
    DispatchBluetoothHandsfreeResult(aRes,
                                     &BluetoothHandsfreeResultHandler::Cleanup,
                                     STATUS_SUCCESS);
  }
}

/* Connect / Disconnect */

void
BluetoothHandsfreeInterface::Connect(const nsAString& aBdAddr,
                                     BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status;
  bt_bdaddr_t bdAddr;

  if (NS_SUCCEEDED(Convert(aBdAddr, bdAddr))) {
    status = mInterface->connect(&bdAddr);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothHandsfreeResult(
      aRes, &BluetoothHandsfreeResultHandler::Connect,
      ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothHandsfreeInterface::Disconnect(
  const nsAString& aBdAddr, BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status;
  bt_bdaddr_t bdAddr;

  if (NS_SUCCEEDED(Convert(aBdAddr, bdAddr))) {
    status = mInterface->disconnect(&bdAddr);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothHandsfreeResult(
      aRes, &BluetoothHandsfreeResultHandler::Disconnect,
      ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothHandsfreeInterface::ConnectAudio(
  const nsAString& aBdAddr, BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status;
  bt_bdaddr_t bdAddr;

  if (NS_SUCCEEDED(Convert(aBdAddr, bdAddr))) {
    status = mInterface->connect_audio(&bdAddr);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothHandsfreeResult(
      aRes, &BluetoothHandsfreeResultHandler::ConnectAudio,
      ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothHandsfreeInterface::DisconnectAudio(
  const nsAString& aBdAddr, BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status;
  bt_bdaddr_t bdAddr;

  if (NS_SUCCEEDED(Convert(aBdAddr, bdAddr))) {
    status = mInterface->disconnect_audio(&bdAddr);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothHandsfreeResult(
      aRes, &BluetoothHandsfreeResultHandler::DisconnectAudio,
      ConvertDefault(status, STATUS_FAIL));
  }
}

/* Voice Recognition */

void
BluetoothHandsfreeInterface::StartVoiceRecognition(
  BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status = mInterface->start_voice_recognition();

  if (aRes) {
    DispatchBluetoothHandsfreeResult(
      aRes, &BluetoothHandsfreeResultHandler::StartVoiceRecognition,
      ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothHandsfreeInterface::StopVoiceRecognition(
  BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status = mInterface->stop_voice_recognition();

  if (aRes) {
    DispatchBluetoothHandsfreeResult(
      aRes, &BluetoothHandsfreeResultHandler::StopVoiceRecognition,
      ConvertDefault(status, STATUS_FAIL));
  }
}

/* Volume */

void
BluetoothHandsfreeInterface::VolumeControl(
  BluetoothHandsfreeVolumeType aType, int aVolume,
  BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status;
  bthf_volume_type_t type = BTHF_VOLUME_TYPE_SPK;

  if (NS_SUCCEEDED(Convert(aType, type))) {
    status = mInterface->volume_control(type, aVolume);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothHandsfreeResult(
      aRes, &BluetoothHandsfreeResultHandler::VolumeControl,
      ConvertDefault(status, STATUS_FAIL));
  }
}

/* Device status */

void
BluetoothHandsfreeInterface::DeviceStatusNotification(
  BluetoothHandsfreeNetworkState aNtkState,
  BluetoothHandsfreeServiceType aSvcType, int aSignal,
  int aBattChg, BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status;
  bthf_network_state_t ntkState = BTHF_NETWORK_STATE_NOT_AVAILABLE;
  bthf_service_type_t svcType = BTHF_SERVICE_TYPE_HOME;

  if (NS_SUCCEEDED(Convert(aNtkState, ntkState)) &&
      NS_SUCCEEDED(Convert(aSvcType, svcType))) {
    status = mInterface->device_status_notification(ntkState, svcType,
                                                    aSignal, aBattChg);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothHandsfreeResult(
      aRes, &BluetoothHandsfreeResultHandler::DeviceStatusNotification,
      ConvertDefault(status, STATUS_FAIL));
  }
}

/* Responses */

void
BluetoothHandsfreeInterface::CopsResponse(
  const char* aCops, BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status = mInterface->cops_response(aCops);

  if (aRes) {
    DispatchBluetoothHandsfreeResult(
      aRes, &BluetoothHandsfreeResultHandler::CopsResponse,
      ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothHandsfreeInterface::CindResponse(
  int aSvc, int aNumActive, int aNumHeld,
  BluetoothHandsfreeCallState aCallSetupState,
  int aSignal, int aRoam, int aBattChg,
  BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status;
  bthf_call_state_t callSetupState = BTHF_CALL_STATE_ACTIVE;

  if (NS_SUCCEEDED(Convert(aCallSetupState, callSetupState))) {
    status = mInterface->cind_response(aSvc, aNumActive, aNumHeld,
                                       callSetupState, aSignal,
                                       aRoam, aBattChg);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothHandsfreeResult(
      aRes, &BluetoothHandsfreeResultHandler::CindResponse,
      ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothHandsfreeInterface::FormattedAtResponse(
  const char* aRsp, BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status = mInterface->formatted_at_response(aRsp);

  if (aRes) {
    DispatchBluetoothHandsfreeResult(
      aRes, &BluetoothHandsfreeResultHandler::FormattedAtResponse,
      ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothHandsfreeInterface::AtResponse(
  BluetoothHandsfreeAtResponse aResponseCode, int aErrorCode,
  BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status;
  bthf_at_response_t responseCode = BTHF_AT_RESPONSE_ERROR;

  if (NS_SUCCEEDED(Convert(aResponseCode, responseCode))) {
    status = mInterface->at_response(responseCode, aErrorCode);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothHandsfreeResult(
      aRes, &BluetoothHandsfreeResultHandler::AtResponse,
      ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothHandsfreeInterface::ClccResponse(
  int aIndex,
  BluetoothHandsfreeCallDirection aDir,
  BluetoothHandsfreeCallState aState,
  BluetoothHandsfreeCallMode aMode,
  BluetoothHandsfreeCallMptyType aMpty,
  const nsAString& aNumber,
  BluetoothHandsfreeCallAddressType aType,
  BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status;
  bthf_call_direction_t dir = BTHF_CALL_DIRECTION_OUTGOING;
  bthf_call_state_t state = BTHF_CALL_STATE_ACTIVE;
  bthf_call_mode_t mode = BTHF_CALL_TYPE_VOICE;
  bthf_call_mpty_type_t mpty = BTHF_CALL_MPTY_TYPE_SINGLE;
  bthf_call_addrtype_t type = BTHF_CALL_ADDRTYPE_UNKNOWN;

  if (NS_SUCCEEDED(Convert(aDir, dir)) &&
      NS_SUCCEEDED(Convert(aState, state)) &&
      NS_SUCCEEDED(Convert(aMode, mode)) &&
      NS_SUCCEEDED(Convert(aMpty, mpty)) &&
      NS_SUCCEEDED(Convert(aType, type))) {
    status = mInterface->clcc_response(aIndex, dir, state, mode, mpty,
                                       NS_ConvertUTF16toUTF8(aNumber).get(),
                                       type);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothHandsfreeResult(
      aRes, &BluetoothHandsfreeResultHandler::ClccResponse,
      ConvertDefault(status, STATUS_FAIL));
  }
}

/* Phone State */

void
BluetoothHandsfreeInterface::PhoneStateChange(int aNumActive, int aNumHeld,
  BluetoothHandsfreeCallState aCallSetupState, const nsAString& aNumber,
  BluetoothHandsfreeCallAddressType aType,
  BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status;
  bthf_call_state_t callSetupState = BTHF_CALL_STATE_ACTIVE;
  bthf_call_addrtype_t type = BTHF_CALL_ADDRTYPE_UNKNOWN;

  if (NS_SUCCEEDED(Convert(aCallSetupState, callSetupState)) &&
      NS_SUCCEEDED(Convert(aType, type))) {
    status = mInterface->phone_state_change(
      aNumActive, aNumHeld, callSetupState,
      NS_ConvertUTF16toUTF8(aNumber).get(), type);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothHandsfreeResult(
      aRes, &BluetoothHandsfreeResultHandler::PhoneStateChange,
      ConvertDefault(status, STATUS_FAIL));
  }
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

typedef
  BluetoothInterfaceRunnable0<BluetoothA2dpResultHandler, void>
  BluetoothA2dpResultRunnable;

typedef
  BluetoothInterfaceRunnable1<BluetoothA2dpResultHandler, void,
                              BluetoothStatus, BluetoothStatus>
  BluetoothA2dpErrorRunnable;

static nsresult
DispatchBluetoothA2dpResult(
  BluetoothA2dpResultHandler* aRes,
  void (BluetoothA2dpResultHandler::*aMethod)(),
  BluetoothStatus aStatus)
{
  MOZ_ASSERT(aRes);

  nsRunnable* runnable;

  if (aStatus == STATUS_SUCCESS) {
    runnable = new BluetoothA2dpResultRunnable(aRes, aMethod);
  } else {
    runnable = new BluetoothA2dpErrorRunnable(aRes,
      &BluetoothA2dpResultHandler::OnError, aStatus);
  }
  nsresult rv = NS_DispatchToMainThread(runnable);
  if (NS_FAILED(rv)) {
    BT_WARNING("NS_DispatchToMainThread failed: %X", rv);
  }
  return rv;
}

// Notification handling
//

BluetoothA2dpNotificationHandler::~BluetoothA2dpNotificationHandler()
{ }

static BluetoothA2dpNotificationHandler* sA2dpNotificationHandler;

struct BluetoothA2dpCallback
{
  class A2dpNotificationHandlerWrapper
  {
  public:
    typedef BluetoothA2dpNotificationHandler ObjectType;

    static ObjectType* GetInstance()
    {
      MOZ_ASSERT(NS_IsMainThread());

      return sA2dpNotificationHandler;
    }
  };

  // Notifications

  typedef BluetoothNotificationRunnable2<A2dpNotificationHandlerWrapper,
                                         void,
                                         BluetoothA2dpConnectionState,
                                         nsString,
                                         BluetoothA2dpConnectionState,
                                         const nsAString&>
    ConnectionStateNotification;

  typedef BluetoothNotificationRunnable2<A2dpNotificationHandlerWrapper,
                                         void,
                                         BluetoothA2dpAudioState,
                                         nsString,
                                         BluetoothA2dpAudioState,
                                         const nsAString&>
    AudioStateNotification;

  // Bluedroid A2DP callbacks

  static void
  ConnectionState(btav_connection_state_t aState, bt_bdaddr_t* aBdAddr)
  {
    ConnectionStateNotification::Dispatch(
      &BluetoothA2dpNotificationHandler::ConnectionStateNotification,
      aState, aBdAddr);
  }

  static void
  AudioState(btav_audio_state_t aState, bt_bdaddr_t* aBdAddr)
  {
    AudioStateNotification::Dispatch(
      &BluetoothA2dpNotificationHandler::AudioStateNotification,
      aState, aBdAddr);
  }
};

// Interface
//

BluetoothA2dpInterface::BluetoothA2dpInterface(
  const btav_interface_t* aInterface)
: mInterface(aInterface)
{
  MOZ_ASSERT(mInterface);
}

BluetoothA2dpInterface::~BluetoothA2dpInterface()
{ }

void
BluetoothA2dpInterface::Init(
  BluetoothA2dpNotificationHandler* aNotificationHandler,
  BluetoothA2dpResultHandler* aRes)
{
  static btav_callbacks_t sCallbacks = {
    sizeof(sCallbacks),
    BluetoothA2dpCallback::ConnectionState,
    BluetoothA2dpCallback::AudioState
  };

  sA2dpNotificationHandler = aNotificationHandler;

  bt_status_t status = mInterface->init(&sCallbacks);

  if (aRes) {
    DispatchBluetoothA2dpResult(aRes, &BluetoothA2dpResultHandler::Init,
                                ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothA2dpInterface::Cleanup(BluetoothA2dpResultHandler* aRes)
{
  mInterface->cleanup();

  if (aRes) {
    DispatchBluetoothA2dpResult(aRes, &BluetoothA2dpResultHandler::Cleanup,
                                STATUS_SUCCESS);
  }
}

void
BluetoothA2dpInterface::Connect(const nsAString& aBdAddr,
                                BluetoothA2dpResultHandler* aRes)
{
  bt_status_t status;
  bt_bdaddr_t bdAddr;

  if (NS_SUCCEEDED(Convert(aBdAddr, bdAddr))) {
    status = mInterface->connect(&bdAddr);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothA2dpResult(aRes, &BluetoothA2dpResultHandler::Connect,
                                ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothA2dpInterface::Disconnect(const nsAString& aBdAddr,
                                   BluetoothA2dpResultHandler* aRes)
{
  bt_status_t status;
  bt_bdaddr_t bdAddr;

  if (NS_SUCCEEDED(Convert(aBdAddr, bdAddr))) {
    status = mInterface->disconnect(&bdAddr);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothA2dpResult(aRes, &BluetoothA2dpResultHandler::Disconnect,
                                ConvertDefault(status, STATUS_FAIL));
  }
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
#endif

typedef
  BluetoothInterfaceRunnable0<BluetoothAvrcpResultHandler, void>
  BluetoothAvrcpResultRunnable;

typedef
  BluetoothInterfaceRunnable1<BluetoothAvrcpResultHandler, void,
                              BluetoothStatus, BluetoothStatus>
  BluetoothAvrcpErrorRunnable;

static nsresult
DispatchBluetoothAvrcpResult(
  BluetoothAvrcpResultHandler* aRes,
  void (BluetoothAvrcpResultHandler::*aMethod)(),
  BluetoothStatus aStatus)
{
  MOZ_ASSERT(aRes);

  nsRunnable* runnable;

  if (aStatus == STATUS_SUCCESS) {
    runnable = new BluetoothAvrcpResultRunnable(aRes, aMethod);
  } else {
    runnable = new BluetoothAvrcpErrorRunnable(aRes,
      &BluetoothAvrcpResultHandler::OnError, aStatus);
  }
  nsresult rv = NS_DispatchToMainThread(runnable);
  if (NS_FAILED(rv)) {
    BT_WARNING("NS_DispatchToMainThread failed: %X", rv);
  }
  return rv;
}

// Notification handling
//

BluetoothAvrcpNotificationHandler::~BluetoothAvrcpNotificationHandler()
{ }

static BluetoothAvrcpNotificationHandler* sAvrcpNotificationHandler;

struct BluetoothAvrcpCallback
{
  class AvrcpNotificationHandlerWrapper
  {
  public:
    typedef BluetoothAvrcpNotificationHandler ObjectType;

    static ObjectType* GetInstance()
    {
      MOZ_ASSERT(NS_IsMainThread());

      return sAvrcpNotificationHandler;
    }
  };

  // Notifications

  typedef BluetoothNotificationRunnable0<AvrcpNotificationHandlerWrapper,
                                         void>
    GetPlayStatusNotification;

  typedef BluetoothNotificationRunnable0<AvrcpNotificationHandlerWrapper,
                                         void>
    ListPlayerAppAttrNotification;

  typedef BluetoothNotificationRunnable1<AvrcpNotificationHandlerWrapper,
                                         void,
                                         BluetoothAvrcpPlayerAttribute>
    ListPlayerAppValuesNotification;

  typedef BluetoothNotificationRunnable2<AvrcpNotificationHandlerWrapper, void,
    uint8_t, nsAutoArrayPtr<BluetoothAvrcpPlayerAttribute>,
    uint8_t, const BluetoothAvrcpPlayerAttribute*>
    GetPlayerAppValueNotification;

  typedef BluetoothNotificationRunnable2<AvrcpNotificationHandlerWrapper, void,
    uint8_t, nsAutoArrayPtr<BluetoothAvrcpPlayerAttribute>,
    uint8_t, const BluetoothAvrcpPlayerAttribute*>
    GetPlayerAppAttrsTextNotification;

  typedef BluetoothNotificationRunnable3<AvrcpNotificationHandlerWrapper,
                                         void,
                                         uint8_t, uint8_t,
                                         nsAutoArrayPtr<uint8_t>,
                                         uint8_t, uint8_t, const uint8_t*>
    GetPlayerAppValuesTextNotification;

  typedef BluetoothNotificationRunnable1<AvrcpNotificationHandlerWrapper,
                                         void,
                                         BluetoothAvrcpPlayerSettings,
                                         const BluetoothAvrcpPlayerSettings&>
    SetPlayerAppValueNotification;

  typedef BluetoothNotificationRunnable2<AvrcpNotificationHandlerWrapper, void,
    uint8_t, nsAutoArrayPtr<BluetoothAvrcpMediaAttribute>,
    uint8_t, const BluetoothAvrcpMediaAttribute*>
    GetElementAttrNotification;

  typedef BluetoothNotificationRunnable2<AvrcpNotificationHandlerWrapper,
                                         void,
                                         BluetoothAvrcpEvent, uint32_t>
    RegisterNotificationNotification;

  typedef BluetoothNotificationRunnable2<AvrcpNotificationHandlerWrapper,
                                         void,
                                         nsString, unsigned long,
                                         const nsAString&>
    RemoteFeatureNotification;

  typedef BluetoothNotificationRunnable2<AvrcpNotificationHandlerWrapper,
                                         void,
                                         uint8_t, uint8_t>
    VolumeChangeNotification;

  typedef BluetoothNotificationRunnable2<AvrcpNotificationHandlerWrapper,
                                         void,
                                         int, int>
    PassthroughCmdNotification;

  // Bluedroid AVRCP callbacks

#if ANDROID_VERSION >= 18
  static void
  GetPlayStatus()
  {
    GetPlayStatusNotification::Dispatch(
      &BluetoothAvrcpNotificationHandler::GetPlayStatusNotification);
  }

  static void
  ListPlayerAppAttr()
  {
    ListPlayerAppAttrNotification::Dispatch(
      &BluetoothAvrcpNotificationHandler::ListPlayerAppAttrNotification);
  }

  static void
  ListPlayerAppValues(btrc_player_attr_t aAttrId)
  {
    ListPlayerAppValuesNotification::Dispatch(
      &BluetoothAvrcpNotificationHandler::ListPlayerAppValuesNotification,
      aAttrId);
  }

  static void
  GetPlayerAppValue(uint8_t aNumAttrs, btrc_player_attr_t* aAttrs)
  {
    GetPlayerAppValueNotification::Dispatch(
      &BluetoothAvrcpNotificationHandler::GetPlayerAppValueNotification,
      aNumAttrs, ConvertArray<btrc_player_attr_t>(aAttrs, aNumAttrs));
  }

  static void
  GetPlayerAppAttrsText(uint8_t aNumAttrs, btrc_player_attr_t* aAttrs)
  {
    GetPlayerAppAttrsTextNotification::Dispatch(
      &BluetoothAvrcpNotificationHandler::GetPlayerAppAttrsTextNotification,
      aNumAttrs, ConvertArray<btrc_player_attr_t>(aAttrs, aNumAttrs));
  }

  static void
  GetPlayerAppValuesText(uint8_t aAttrId, uint8_t aNumVals, uint8_t* aVals)
  {
    GetPlayerAppValuesTextNotification::Dispatch(
      &BluetoothAvrcpNotificationHandler::GetPlayerAppValuesTextNotification,
      aAttrId, aNumVals, ConvertArray<uint8_t>(aVals, aNumVals));
  }

  static void
  SetPlayerAppValue(btrc_player_settings_t* aVals)
  {
    SetPlayerAppValueNotification::Dispatch(
      &BluetoothAvrcpNotificationHandler::SetPlayerAppValueNotification,
      *aVals);
  }

  static void
  GetElementAttr(uint8_t aNumAttrs, btrc_media_attr_t* aAttrs)
  {
    GetElementAttrNotification::Dispatch(
      &BluetoothAvrcpNotificationHandler::GetElementAttrNotification,
      aNumAttrs, ConvertArray<btrc_media_attr_t>(aAttrs, aNumAttrs));
  }

  static void
  RegisterNotification(btrc_event_id_t aEvent, uint32_t aParam)
  {
    RegisterNotificationNotification::Dispatch(
      &BluetoothAvrcpNotificationHandler::RegisterNotificationNotification,
      aEvent, aParam);
  }
#endif // ANDROID_VERSION >= 18

#if ANDROID_VERSION >= 19
  static void
  RemoteFeature(bt_bdaddr_t* aBdAddr, btrc_remote_features_t aFeatures)
  {
    RemoteFeatureNotification::Dispatch(
      &BluetoothAvrcpNotificationHandler::RemoteFeatureNotification,
      aBdAddr, aFeatures);
  }

  static void
  VolumeChange(uint8_t aVolume, uint8_t aCType)
  {
    VolumeChangeNotification::Dispatch(
      &BluetoothAvrcpNotificationHandler::VolumeChangeNotification,
      aVolume, aCType);
  }

  static void
  PassthroughCmd(int aId, int aKeyState)
  {
    PassthroughCmdNotification::Dispatch(
      &BluetoothAvrcpNotificationHandler::PassthroughCmdNotification,
      aId, aKeyState);
  }
#endif // ANDROID_VERSION >= 19
};

// Interface
//

BluetoothAvrcpInterface::BluetoothAvrcpInterface(
#if ANDROID_VERSION >= 18
  const btrc_interface_t* aInterface
#endif
  )
#if ANDROID_VERSION >= 18
: mInterface(aInterface)
#endif
{
#if ANDROID_VERSION >= 18
  MOZ_ASSERT(mInterface);
#endif
}

BluetoothAvrcpInterface::~BluetoothAvrcpInterface()
{ }

void
BluetoothAvrcpInterface::Init(
  BluetoothAvrcpNotificationHandler* aNotificationHandler,
  BluetoothAvrcpResultHandler* aRes)
{
#if ANDROID_VERSION >= 18
  static btrc_callbacks_t sCallbacks = {
    sizeof(sCallbacks),
#if ANDROID_VERSION >= 19
    BluetoothAvrcpCallback::RemoteFeature,
#endif
    BluetoothAvrcpCallback::GetPlayStatus,
    BluetoothAvrcpCallback::ListPlayerAppAttr,
    BluetoothAvrcpCallback::ListPlayerAppValues,
    BluetoothAvrcpCallback::GetPlayerAppValue,
    BluetoothAvrcpCallback::GetPlayerAppAttrsText,
    BluetoothAvrcpCallback::GetPlayerAppValuesText,
    BluetoothAvrcpCallback::SetPlayerAppValue,
    BluetoothAvrcpCallback::GetElementAttr,
    BluetoothAvrcpCallback::RegisterNotification
#if ANDROID_VERSION >= 19
    ,
    BluetoothAvrcpCallback::VolumeChange,
    BluetoothAvrcpCallback::PassthroughCmd
#endif
  };
#endif // ANDROID_VERSION >= 18

  sAvrcpNotificationHandler = aNotificationHandler;

#if ANDROID_VERSION >= 18
  bt_status_t status = mInterface->init(&sCallbacks);
#else
  bt_status_t status = BT_STATUS_UNSUPPORTED;
#endif

  if (aRes) {
    DispatchBluetoothAvrcpResult(aRes, &BluetoothAvrcpResultHandler::Init,
                                 ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothAvrcpInterface::Cleanup(BluetoothAvrcpResultHandler* aRes)
{
#if ANDROID_VERSION >= 18
  mInterface->cleanup();
#endif

  if (aRes) {
    DispatchBluetoothAvrcpResult(aRes, &BluetoothAvrcpResultHandler::Cleanup,
                                 STATUS_SUCCESS);
  }
}

void
BluetoothAvrcpInterface::GetPlayStatusRsp(ControlPlayStatus aPlayStatus,
                                          uint32_t aSongLen, uint32_t aSongPos,
                                          BluetoothAvrcpResultHandler* aRes)
{
  bt_status_t status;

#if ANDROID_VERSION >= 18
  btrc_play_status_t playStatus = BTRC_PLAYSTATE_STOPPED;

  if (!(NS_FAILED(Convert(aPlayStatus, playStatus)))) {
    status = mInterface->get_play_status_rsp(playStatus, aSongLen, aSongPos);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }
#else
  status = BT_STATUS_UNSUPPORTED;
#endif

  if (aRes) {
    DispatchBluetoothAvrcpResult(
      aRes, &BluetoothAvrcpResultHandler::GetPlayStatusRsp,
      ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothAvrcpInterface::ListPlayerAppAttrRsp(
  int aNumAttr, const BluetoothAvrcpPlayerAttribute* aPAttrs,
  BluetoothAvrcpResultHandler* aRes)
{
  bt_status_t status;

#if ANDROID_VERSION >= 18
  ConvertArray<BluetoothAvrcpPlayerAttribute> pAttrsArray(aPAttrs, aNumAttr);
  nsAutoArrayPtr<btrc_player_attr_t> pAttrs;

  if (NS_SUCCEEDED(Convert(pAttrsArray, pAttrs))) {
    status = mInterface->list_player_app_attr_rsp(aNumAttr, pAttrs);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }
#else
  status = BT_STATUS_UNSUPPORTED;
#endif

  if (aRes) {
    DispatchBluetoothAvrcpResult(
      aRes, &BluetoothAvrcpResultHandler::ListPlayerAppAttrRsp,
      ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothAvrcpInterface::ListPlayerAppValueRsp(
  int aNumVal, uint8_t* aPVals, BluetoothAvrcpResultHandler* aRes)
{
#if ANDROID_VERSION >= 18
  bt_status_t status = mInterface->list_player_app_value_rsp(aNumVal, aPVals);
#else
  bt_status_t status = BT_STATUS_UNSUPPORTED;
#endif

  if (aRes) {
    DispatchBluetoothAvrcpResult(
      aRes, &BluetoothAvrcpResultHandler::ListPlayerAppValueRsp,
      ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothAvrcpInterface::GetPlayerAppValueRsp(
  uint8_t aNumAttrs, const uint8_t* aIds, const uint8_t* aValues,
  BluetoothAvrcpResultHandler* aRes)
{
  bt_status_t status;

#if ANDROID_VERSION >= 18
  btrc_player_settings_t pVals;

  /* FIXME: you need to implement the missing conversion functions */
  NS_NOTREACHED("Conversion function missing");

  if (false /* TODO: we don't support any player app values currently */) {
    status = mInterface->get_player_app_value_rsp(&pVals);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }
#else
  status = BT_STATUS_UNSUPPORTED;
#endif

  if (aRes) {
    DispatchBluetoothAvrcpResult(
      aRes, &BluetoothAvrcpResultHandler::GetPlayerAppValueRsp,
      ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothAvrcpInterface::GetPlayerAppAttrTextRsp(
  int aNumAttr, const uint8_t* aIds, const char** aTexts,
  BluetoothAvrcpResultHandler* aRes)
{
  bt_status_t status;

#if ANDROID_VERSION >= 18
  btrc_player_setting_text_t* aPAttrs;

  /* FIXME: you need to implement the missing conversion functions */
  NS_NOTREACHED("Conversion function missing");

  if (false /* TODO: we don't support any attributes currently */) {
    status = mInterface->get_player_app_attr_text_rsp(aNumAttr, aPAttrs);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }
#else
  status = BT_STATUS_UNSUPPORTED;
#endif

  if (aRes) {
    DispatchBluetoothAvrcpResult(
      aRes, &BluetoothAvrcpResultHandler::GetPlayerAppAttrTextRsp,
      ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothAvrcpInterface::GetPlayerAppValueTextRsp(
  int aNumVal, const uint8_t* aIds, const char** aTexts,
  BluetoothAvrcpResultHandler* aRes)
{
  bt_status_t status;

#if ANDROID_VERSION >= 18
  btrc_player_setting_text_t* pVals;

  /* FIXME: you need to implement the missing conversion functions */
  NS_NOTREACHED("Conversion function missing");

  if (false /* TODO: we don't support any values currently */) {
    status = mInterface->get_player_app_value_text_rsp(aNumVal, pVals);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }
#else
  status = BT_STATUS_UNSUPPORTED;
#endif

  if (aRes) {
    DispatchBluetoothAvrcpResult(
      aRes, &BluetoothAvrcpResultHandler::GetPlayerAppValueTextRsp,
      ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothAvrcpInterface::GetElementAttrRsp(
  uint8_t aNumAttr, const BluetoothAvrcpElementAttribute* aAttrs,
  BluetoothAvrcpResultHandler* aRes)
{
  bt_status_t status;

#if ANDROID_VERSION >= 18
  ConvertArray<BluetoothAvrcpElementAttribute> pAttrsArray(aAttrs, aNumAttr);
  nsAutoArrayPtr<btrc_element_attr_val_t> pAttrs;

  if (NS_SUCCEEDED(Convert(pAttrsArray, pAttrs))) {
    status = mInterface->get_element_attr_rsp(aNumAttr, pAttrs);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }
#else
  status = BT_STATUS_UNSUPPORTED;
#endif

  if (aRes) {
    DispatchBluetoothAvrcpResult(
      aRes, &BluetoothAvrcpResultHandler::GetElementAttrRsp,
      ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothAvrcpInterface::SetPlayerAppValueRsp(
  BluetoothAvrcpStatus aRspStatus, BluetoothAvrcpResultHandler* aRes)
{
  bt_status_t status;

#if ANDROID_VERSION >= 18
  btrc_status_t rspStatus = BTRC_STS_BAD_CMD; // silences compiler warning

  if (NS_SUCCEEDED(Convert(aRspStatus, rspStatus))) {
    status = mInterface->set_player_app_value_rsp(rspStatus);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }
#else
  status = BT_STATUS_UNSUPPORTED;
#endif

  if (aRes) {
    DispatchBluetoothAvrcpResult(
      aRes, &BluetoothAvrcpResultHandler::SetPlayerAppValueRsp,
      ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothAvrcpInterface::RegisterNotificationRsp(
  BluetoothAvrcpEvent aEvent, BluetoothAvrcpNotification aType,
  const BluetoothAvrcpNotificationParam& aParam,
  BluetoothAvrcpResultHandler* aRes)
{
  bt_status_t status;

#if ANDROID_VERSION >= 18
  nsresult rv;
  btrc_event_id_t event = { };
  btrc_notification_type_t type = BTRC_NOTIFICATION_TYPE_INTERIM;
  btrc_register_notification_t param;

  switch (aEvent) {
    case AVRCP_EVENT_PLAY_STATUS_CHANGED:
      rv = Convert(aParam.mPlayStatus, param.play_status);
      break;
    case AVRCP_EVENT_TRACK_CHANGE:
      MOZ_ASSERT(sizeof(aParam.mTrack) == sizeof(param.track));
      memcpy(param.track, aParam.mTrack, sizeof(param.track));
      rv = NS_OK;
      break;
    case AVRCP_EVENT_TRACK_REACHED_END:
      NS_NOTREACHED("Unknown conversion");
      rv = NS_ERROR_ILLEGAL_VALUE;
      break;
    case AVRCP_EVENT_TRACK_REACHED_START:
      NS_NOTREACHED("Unknown conversion");
      rv = NS_ERROR_ILLEGAL_VALUE;
      break;
    case AVRCP_EVENT_PLAY_POS_CHANGED:
      param.song_pos = aParam.mSongPos;
      rv = NS_OK;
      break;
    case AVRCP_EVENT_APP_SETTINGS_CHANGED:
      NS_NOTREACHED("Unknown conversion");
      rv = NS_ERROR_ILLEGAL_VALUE;
      break;
    default:
      NS_NOTREACHED("Unknown conversion");
      rv = NS_ERROR_ILLEGAL_VALUE;
      break;
  }

  if (NS_SUCCEEDED(rv) &&
      NS_SUCCEEDED(Convert(aEvent, event)) &&
      NS_SUCCEEDED(Convert(aType, type))) {
    status = mInterface->register_notification_rsp(event, type, &param);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }
#else
  status = BT_STATUS_UNSUPPORTED;
#endif

  if (aRes) {
    DispatchBluetoothAvrcpResult(
      aRes, &BluetoothAvrcpResultHandler::RegisterNotificationRsp,
      ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothAvrcpInterface::SetVolume(uint8_t aVolume,
                                   BluetoothAvrcpResultHandler* aRes)
{
#if ANDROID_VERSION >= 19
  bt_status_t status = mInterface->set_volume(aVolume);
#else
  bt_status_t status = BT_STATUS_UNSUPPORTED;
#endif

  if (aRes) {
    DispatchBluetoothAvrcpResult(
      aRes, &BluetoothAvrcpResultHandler::SetVolume,
      ConvertDefault(status, STATUS_FAIL));
  }
}

//
// Bluetooth Core Interface
//

typedef
  BluetoothInterfaceRunnable0<BluetoothResultHandler, void>
  BluetoothResultRunnable;

typedef
  BluetoothInterfaceRunnable1<BluetoothResultHandler, void,
                              BluetoothStatus, BluetoothStatus>
  BluetoothErrorRunnable;

static nsresult
DispatchBluetoothResult(BluetoothResultHandler* aRes,
                        void (BluetoothResultHandler::*aMethod)(),
                        BluetoothStatus aStatus)
{
  MOZ_ASSERT(aRes);

  nsRunnable* runnable;

  if (aStatus == STATUS_SUCCESS) {
    runnable = new BluetoothResultRunnable(aRes, aMethod);
  } else {
    runnable = new
      BluetoothErrorRunnable(aRes, &BluetoothResultHandler::OnError, aStatus);
  }
  nsresult rv = NS_DispatchToMainThread(runnable);
  if (NS_FAILED(rv)) {
    BT_WARNING("NS_DispatchToMainThread failed: %X", rv);
  }
  return rv;
}

// Notification handling
//

BluetoothNotificationHandler::~BluetoothNotificationHandler()
{ }

static BluetoothNotificationHandler* sNotificationHandler;

struct BluetoothCallback
{
  class NotificationHandlerWrapper
  {
  public:
    typedef BluetoothNotificationHandler  ObjectType;

    static ObjectType* GetInstance()
    {
      MOZ_ASSERT(NS_IsMainThread());

      return sNotificationHandler;
    }
  };

  // Notifications

  typedef BluetoothNotificationRunnable1<NotificationHandlerWrapper, void,
                                         bool>
    AdapterStateChangedNotification;

  typedef BluetoothNotificationRunnable3<NotificationHandlerWrapper, void,
                                         BluetoothStatus, int,
                                         nsAutoArrayPtr<BluetoothProperty>,
                                         BluetoothStatus, int,
                                         const BluetoothProperty*>
    AdapterPropertiesNotification;

  typedef BluetoothNotificationRunnable4<NotificationHandlerWrapper, void,
                                         BluetoothStatus, nsString, int,
                                         nsAutoArrayPtr<BluetoothProperty>,
                                         BluetoothStatus, const nsAString&,
                                         int, const BluetoothProperty*>
    RemoteDevicePropertiesNotification;

  typedef BluetoothNotificationRunnable2<NotificationHandlerWrapper, void,
                                         int,
                                         nsAutoArrayPtr<BluetoothProperty>,
                                         int, const BluetoothProperty*>
    DeviceFoundNotification;

  typedef BluetoothNotificationRunnable1<NotificationHandlerWrapper, void,
                                         bool>
    DiscoveryStateChangedNotification;

  typedef BluetoothNotificationRunnable3<NotificationHandlerWrapper, void,
                                         nsString, nsString, uint32_t,
                                         const nsAString&, const nsAString&>
    PinRequestNotification;

  typedef BluetoothNotificationRunnable5<NotificationHandlerWrapper, void,
                                         nsString, nsString, uint32_t,
                                         nsString, uint32_t,
                                         const nsAString&, const nsAString&,
                                         uint32_t, const nsAString&>
    SspRequestNotification;

  typedef BluetoothNotificationRunnable3<NotificationHandlerWrapper, void,
                                         BluetoothStatus, nsString,
                                         BluetoothBondState,
                                         BluetoothStatus, const nsAString&>
    BondStateChangedNotification;

  typedef BluetoothNotificationRunnable3<NotificationHandlerWrapper, void,
                                         BluetoothStatus, nsString, bool,
                                         BluetoothStatus, const nsAString&>
    AclStateChangedNotification;

  typedef BluetoothNotificationRunnable3<NotificationHandlerWrapper, void,
                                         uint16_t, nsAutoArrayPtr<uint8_t>,
                                         uint8_t, uint16_t, const uint8_t*>
    DutModeRecvNotification;

  typedef BluetoothNotificationRunnable2<NotificationHandlerWrapper, void,
                                         BluetoothStatus, uint16_t>
    LeTestModeNotification;

  // Bluedroid callbacks

  static const bt_property_t*
  AlignedProperties(bt_property_t* aProperties, size_t aNumProperties,
                    nsAutoArrayPtr<bt_property_t>& aPropertiesArray)
  {
    // See Bug 989976: consider aProperties address is not aligned. If
    // it is aligned, we return the pointer directly; otherwise we make
    // an aligned copy. The argument |aPropertiesArray| keeps track of
    // the memory buffer.
    if (!(reinterpret_cast<uintptr_t>(aProperties) % sizeof(void*))) {
      return aProperties;
    }

    bt_property_t* properties = new bt_property_t[aNumProperties];
    memcpy(properties, aProperties, aNumProperties * sizeof(*properties));
    aPropertiesArray = properties;

    return properties;
  }

  static void
  AdapterStateChanged(bt_state_t aStatus)
  {
    AdapterStateChangedNotification::Dispatch(
      &BluetoothNotificationHandler::AdapterStateChangedNotification,
      aStatus);
  }

  static void
  AdapterProperties(bt_status_t aStatus, int aNumProperties,
                    bt_property_t* aProperties)
  {
    nsAutoArrayPtr<bt_property_t> propertiesArray;

    AdapterPropertiesNotification::Dispatch(
      &BluetoothNotificationHandler::AdapterPropertiesNotification,
      ConvertDefault(aStatus, STATUS_FAIL), aNumProperties,
      ConvertArray<bt_property_t>(
        AlignedProperties(aProperties, aNumProperties, propertiesArray),
      aNumProperties));
  }

  static void
  RemoteDeviceProperties(bt_status_t aStatus, bt_bdaddr_t* aBdAddress,
                         int aNumProperties, bt_property_t* aProperties)
  {
    nsAutoArrayPtr<bt_property_t> propertiesArray;

    RemoteDevicePropertiesNotification::Dispatch(
      &BluetoothNotificationHandler::RemoteDevicePropertiesNotification,
      ConvertDefault(aStatus, STATUS_FAIL), aBdAddress, aNumProperties,
      ConvertArray<bt_property_t>(
        AlignedProperties(aProperties, aNumProperties, propertiesArray),
      aNumProperties));
  }

  static void
  DeviceFound(int aNumProperties, bt_property_t* aProperties)
  {
    nsAutoArrayPtr<bt_property_t> propertiesArray;

    DeviceFoundNotification::Dispatch(
      &BluetoothNotificationHandler::DeviceFoundNotification,
      aNumProperties,
      ConvertArray<bt_property_t>(
        AlignedProperties(aProperties, aNumProperties, propertiesArray),
      aNumProperties));
  }

  static void
  DiscoveryStateChanged(bt_discovery_state_t aState)
  {
    DiscoveryStateChangedNotification::Dispatch(
      &BluetoothNotificationHandler::DiscoveryStateChangedNotification,
      aState);
  }

  static void
  PinRequest(bt_bdaddr_t* aRemoteBdAddress,
             bt_bdname_t* aRemoteBdName, uint32_t aRemoteClass)
  {
    PinRequestNotification::Dispatch(
      &BluetoothNotificationHandler::PinRequestNotification,
      aRemoteBdAddress, aRemoteBdName, aRemoteClass);
  }

  static void
  SspRequest(bt_bdaddr_t* aRemoteBdAddress, bt_bdname_t* aRemoteBdName,
             uint32_t aRemoteClass, bt_ssp_variant_t aPairingVariant,
             uint32_t aPasskey)
  {
    SspRequestNotification::Dispatch(
      &BluetoothNotificationHandler::SspRequestNotification,
      aRemoteBdAddress, aRemoteBdName, aRemoteClass,
      aPairingVariant, aPasskey);
  }

  static void
  BondStateChanged(bt_status_t aStatus, bt_bdaddr_t* aRemoteBdAddress,
                   bt_bond_state_t aState)
  {
    BondStateChangedNotification::Dispatch(
      &BluetoothNotificationHandler::BondStateChangedNotification,
      aStatus, aRemoteBdAddress, aState);
  }

  static void
  AclStateChanged(bt_status_t aStatus, bt_bdaddr_t* aRemoteBdAddress,
                  bt_acl_state_t aState)
  {
    AclStateChangedNotification::Dispatch(
      &BluetoothNotificationHandler::AclStateChangedNotification,
      aStatus, aRemoteBdAddress, aState);
  }

  static void
  ThreadEvt(bt_cb_thread_evt evt)
  {
    // This callback maintains internal state and is not exported.
  }

  static void
  DutModeRecv(uint16_t aOpcode, uint8_t* aBuf, uint8_t aLen)
  {
    DutModeRecvNotification::Dispatch(
      &BluetoothNotificationHandler::DutModeRecvNotification,
      aOpcode, ConvertArray<uint8_t>(aBuf, aLen), aLen);
  }

  static void
  LeTestMode(bt_status_t aStatus, uint16_t aNumPackets)
  {
    LeTestModeNotification::Dispatch(
      &BluetoothNotificationHandler::LeTestModeNotification,
      aStatus, aNumPackets);
  }
};

// Interface
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

void
BluetoothInterface::Init(BluetoothNotificationHandler* aNotificationHandler,
                         BluetoothResultHandler* aRes)
{
  static bt_callbacks_t sBluetoothCallbacks = {
    sizeof(sBluetoothCallbacks),
    BluetoothCallback::AdapterStateChanged,
    BluetoothCallback::AdapterProperties,
    BluetoothCallback::RemoteDeviceProperties,
    BluetoothCallback::DeviceFound,
    BluetoothCallback::DiscoveryStateChanged,
    BluetoothCallback::PinRequest,
    BluetoothCallback::SspRequest,
    BluetoothCallback::BondStateChanged,
    BluetoothCallback::AclStateChanged,
    BluetoothCallback::ThreadEvt,
    BluetoothCallback::DutModeRecv,
#if ANDROID_VERSION >= 18
    BluetoothCallback::LeTestMode
#endif
  };

  sNotificationHandler = aNotificationHandler;

  int status = mInterface->init(&sBluetoothCallbacks);

  if (aRes) {
    DispatchBluetoothResult(aRes, &BluetoothResultHandler::Init,
                            ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothInterface::Cleanup(BluetoothResultHandler* aRes)
{
  mInterface->cleanup();

  if (aRes) {
    DispatchBluetoothResult(aRes, &BluetoothResultHandler::Cleanup,
                            STATUS_SUCCESS);
  }

  sNotificationHandler = nullptr;
}

void
BluetoothInterface::Enable(BluetoothResultHandler* aRes)
{
  int status = mInterface->enable();

  if (aRes) {
    DispatchBluetoothResult(aRes, &BluetoothResultHandler::Enable,
                            ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothInterface::Disable(BluetoothResultHandler* aRes)
{
  int status = mInterface->disable();

  if (aRes) {
    DispatchBluetoothResult(aRes, &BluetoothResultHandler::Disable,
                            ConvertDefault(status, STATUS_FAIL));
  }
}

/* Adapter Properties */

void
BluetoothInterface::GetAdapterProperties(BluetoothResultHandler* aRes)
{
  int status = mInterface->get_adapter_properties();

  if (aRes) {
    DispatchBluetoothResult(aRes,
                            &BluetoothResultHandler::GetAdapterProperties,
                            ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothInterface::GetAdapterProperty(const nsAString& aName,
                                       BluetoothResultHandler* aRes)
{
  int status;
  bt_property_type_t type;

  /* FIXME: you need to implement the missing conversion functions */
  NS_NOTREACHED("Conversion function missing");

  if (false /* TODO: we don't support any values for aName currently */) {
    status = mInterface->get_adapter_property(type);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothResult(aRes,
                            &BluetoothResultHandler::GetAdapterProperties,
                            ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothInterface::SetAdapterProperty(const BluetoothNamedValue& aProperty,
                                       BluetoothResultHandler* aRes)
{
  int status;
  ConvertNamedValue convertProperty(aProperty);
  bt_property_t property;

  if (NS_SUCCEEDED(Convert(convertProperty, property))) {
    status = mInterface->set_adapter_property(&property);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothResult(aRes,
                            &BluetoothResultHandler::SetAdapterProperty,
                            ConvertDefault(status, STATUS_FAIL));
  }
}

/* Remote Device Properties */

void
BluetoothInterface::GetRemoteDeviceProperties(const nsAString& aRemoteAddr,
                                              BluetoothResultHandler* aRes)
{
  int status;
  bt_bdaddr_t addr;

  if (NS_SUCCEEDED(Convert(aRemoteAddr, addr))) {
    status = mInterface->get_remote_device_properties(&addr);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothResult(aRes,
                            &BluetoothResultHandler::GetRemoteDeviceProperties,
                            ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothInterface::GetRemoteDeviceProperty(const nsAString& aRemoteAddr,
                                            const nsAString& aName,
                                            BluetoothResultHandler* aRes)
{
  int status;
  bt_bdaddr_t remoteAddr;
  bt_property_type_t name;

  /* FIXME: you need to implement the missing conversion functions */
  NS_NOTREACHED("Conversion function missing");

  if (NS_SUCCEEDED(Convert(aRemoteAddr, remoteAddr)) &&
      false /* TODO: we don't support any values for aName currently */) {
    status = mInterface->get_remote_device_property(&remoteAddr, name);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothResult(aRes,
                            &BluetoothResultHandler::GetRemoteDeviceProperty,
                            ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothInterface::SetRemoteDeviceProperty(const nsAString& aRemoteAddr,
                                            const BluetoothNamedValue& aProperty,
                                            BluetoothResultHandler* aRes)
{
  int status;
  bt_bdaddr_t remoteAddr;
  bt_property_t property;

  /* FIXME: you need to implement the missing conversion functions */
  NS_NOTREACHED("Conversion function missing");

  if (NS_SUCCEEDED(Convert(aRemoteAddr, remoteAddr)) &&
      false /* TODO: we don't support any values for aProperty currently */) {
    status = mInterface->set_remote_device_property(&remoteAddr, &property);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothResult(aRes,
                            &BluetoothResultHandler::SetRemoteDeviceProperty,
                            ConvertDefault(status, STATUS_FAIL));
  }
}

/* Remote Services */

void
BluetoothInterface::GetRemoteServiceRecord(const nsAString& aRemoteAddr,
                                           const uint8_t aUuid[16],
                                           BluetoothResultHandler* aRes)
{
  int status;
  bt_bdaddr_t remoteAddr;
  bt_uuid_t uuid;

  if (NS_SUCCEEDED(Convert(aRemoteAddr, remoteAddr)) &&
      NS_SUCCEEDED(Convert(aUuid, uuid))) {
    status = mInterface->get_remote_service_record(&remoteAddr, &uuid);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothResult(aRes,
                            &BluetoothResultHandler::GetRemoteServiceRecord,
                            ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothInterface::GetRemoteServices(const nsAString& aRemoteAddr,
                                      BluetoothResultHandler* aRes)
{
  int status;
  bt_bdaddr_t remoteAddr;

  if (NS_SUCCEEDED(Convert(aRemoteAddr, remoteAddr))) {
    status = mInterface->get_remote_services(&remoteAddr);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothResult(aRes,
                            &BluetoothResultHandler::GetRemoteServices,
                            ConvertDefault(status, STATUS_FAIL));
  }
}

/* Discovery */

void
BluetoothInterface::StartDiscovery(BluetoothResultHandler* aRes)
{
  int status = mInterface->start_discovery();

  if (aRes) {
    DispatchBluetoothResult(aRes,
                            &BluetoothResultHandler::StartDiscovery,
                            ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothInterface::CancelDiscovery(BluetoothResultHandler* aRes)
{
  int status = mInterface->cancel_discovery();

  if (aRes) {
    DispatchBluetoothResult(aRes,
                            &BluetoothResultHandler::CancelDiscovery,
                            ConvertDefault(status, STATUS_FAIL));
  }
}

/* Bonds */

void
BluetoothInterface::CreateBond(const nsAString& aBdAddr,
                               BluetoothResultHandler* aRes)
{
  bt_bdaddr_t bdAddr;
  int status;

  if (NS_SUCCEEDED(Convert(aBdAddr, bdAddr))) {
    status = mInterface->create_bond(&bdAddr);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothResult(aRes,
                            &BluetoothResultHandler::CreateBond,
                            ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothInterface::RemoveBond(const nsAString& aBdAddr,
                               BluetoothResultHandler* aRes)
{
  bt_bdaddr_t bdAddr;
  int status;

  if (NS_SUCCEEDED(Convert(aBdAddr, bdAddr))) {
    status = mInterface->remove_bond(&bdAddr);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothResult(aRes,
                            &BluetoothResultHandler::RemoveBond,
                            ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothInterface::CancelBond(const nsAString& aBdAddr,
                               BluetoothResultHandler* aRes)
{
  bt_bdaddr_t bdAddr;
  int status;

  if (NS_SUCCEEDED(Convert(aBdAddr, bdAddr))) {
    status = mInterface->cancel_bond(&bdAddr);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothResult(aRes,
                            &BluetoothResultHandler::CancelBond,
                            ConvertDefault(status, STATUS_FAIL));
  }
}

/* Authentication */

void
BluetoothInterface::PinReply(const nsAString& aBdAddr, bool aAccept,
                             const nsAString& aPinCode,
                             BluetoothResultHandler* aRes)
{
  int status;
  bt_bdaddr_t bdAddr;
  uint8_t accept;
  bt_pin_code_t pinCode;

  if (NS_SUCCEEDED(Convert(aBdAddr, bdAddr)) &&
      NS_SUCCEEDED(Convert(aAccept, accept)) &&
      NS_SUCCEEDED(Convert(aPinCode, pinCode))) {
    status = mInterface->pin_reply(&bdAddr, accept, aPinCode.Length(),
                                   &pinCode);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothResult(aRes,
                            &BluetoothResultHandler::PinReply,
                            ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothInterface::SspReply(const nsAString& aBdAddr,
                             const nsAString& aVariant,
                             bool aAccept, uint32_t aPasskey,
                             BluetoothResultHandler* aRes)
{
  int status;
  bt_bdaddr_t bdAddr;
  bt_ssp_variant_t variant;
  uint8_t accept;

  if (NS_SUCCEEDED(Convert(aBdAddr, bdAddr)) &&
      NS_SUCCEEDED(Convert(aVariant, variant)) &&
      NS_SUCCEEDED(Convert(aAccept, accept))) {
    status = mInterface->ssp_reply(&bdAddr, variant, accept, aPasskey);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothResult(aRes,
                            &BluetoothResultHandler::SspReply,
                            ConvertDefault(status, STATUS_FAIL));
  }
}

/* DUT Mode */

void
BluetoothInterface::DutModeConfigure(bool aEnable,
                                     BluetoothResultHandler* aRes)
{
  int status;
  uint8_t enable;

  if (NS_SUCCEEDED(Convert(aEnable, enable))) {
    status = mInterface->dut_mode_configure(enable);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothResult(aRes,
                            &BluetoothResultHandler::DutModeConfigure,
                            ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothInterface::DutModeSend(uint16_t aOpcode, uint8_t* aBuf, uint8_t aLen,
                                BluetoothResultHandler* aRes)
{
  int status = mInterface->dut_mode_send(aOpcode, aBuf, aLen);

  if (aRes) {
    DispatchBluetoothResult(aRes,
                            &BluetoothResultHandler::DutModeSend,
                            ConvertDefault(status, STATUS_FAIL));
  }
}

/* LE Mode */

void
BluetoothInterface::LeTestMode(uint16_t aOpcode, uint8_t* aBuf, uint8_t aLen,
                               BluetoothResultHandler* aRes)
{
#if ANDROID_VERSION >= 18
  int status = mInterface->le_test_mode(aOpcode, aBuf, aLen);
#else
  int status = BT_STATUS_UNSUPPORTED;
#endif

  if (aRes) {
    DispatchBluetoothResult(aRes,
                            &BluetoothResultHandler::LeTestMode,
                            ConvertDefault(status, STATUS_FAIL));
  }
}

/* Profile Interfaces */

template <class T>
T*
BluetoothInterface::CreateProfileInterface()
{
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

  return new T(interface);
}

#if ANDROID_VERSION < 18
/*
 * Bluedroid versions that don't support AVRCP will call this function
 * to create an AVRCP interface. All interface methods will fail with
 * the error constant STATUS_UNSUPPORTED.
 */
template <>
BluetoothAvrcpInterface*
BluetoothInterface::CreateProfileInterface<BluetoothAvrcpInterface>()
{
  BT_WARNING("Bluetooth profile 'avrcp' is not supported");

  return new BluetoothAvrcpInterface();
}
#endif

template <class T>
T*
BluetoothInterface::GetProfileInterface()
{
  static T* sBluetoothProfileInterface;

  if (sBluetoothProfileInterface) {
    return sBluetoothProfileInterface;
  }

  sBluetoothProfileInterface = CreateProfileInterface<T>();

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
  return GetProfileInterface<BluetoothAvrcpInterface>();
}

END_BLUETOOTH_NAMESPACE
