/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothDaemonHelpers.h"
#include <limits>

#define MAX_UUID_SIZE 16

BEGIN_BLUETOOTH_NAMESPACE

//
// Conversion
//

nsresult
Convert(bool aIn, uint8_t& aOut)
{
  static const bool sValue[] = {
    CONVERT(false, 0x00),
    CONVERT(true, 0x01)
  };
  if (NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sValue))) {
    aOut = 0;
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sValue[aIn];
  return NS_OK;
}

nsresult
Convert(bool aIn, BluetoothScanMode& aOut)
{
  static const BluetoothScanMode sScanMode[] = {
    CONVERT(false, SCAN_MODE_CONNECTABLE),
    CONVERT(true, SCAN_MODE_CONNECTABLE_DISCOVERABLE)
  };
  if (NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sScanMode))) {
    aOut = SCAN_MODE_NONE; // silences compiler warning
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sScanMode[aIn];
  return NS_OK;
}

nsresult
Convert(int aIn, uint8_t& aOut)
{
  if (NS_WARN_IF(aIn < std::numeric_limits<uint8_t>::min()) ||
      NS_WARN_IF(aIn > std::numeric_limits<uint8_t>::max())) {
    aOut = 0; // silences compiler warning
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = static_cast<uint8_t>(aIn);
  return NS_OK;
}

nsresult
Convert(int aIn, int16_t& aOut)
{
  if (NS_WARN_IF(aIn < std::numeric_limits<int16_t>::min()) ||
      NS_WARN_IF(aIn > std::numeric_limits<int16_t>::max())) {
    aOut = 0; // silences compiler warning
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = static_cast<int16_t>(aIn);
  return NS_OK;
}

nsresult
Convert(uint8_t aIn, bool& aOut)
{
  static const bool sBool[] = {
    CONVERT(0x00, false),
    CONVERT(0x01, true)
  };
  if (NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sBool))) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sBool[aIn];
  return NS_OK;
}

nsresult
Convert(uint8_t aIn, char& aOut)
{
  aOut = static_cast<char>(aIn);
  return NS_OK;
}

nsresult
Convert(uint8_t aIn, int& aOut)
{
  aOut = static_cast<int>(aIn);
  return NS_OK;
}

nsresult
Convert(uint8_t aIn, unsigned long& aOut)
{
  aOut = static_cast<unsigned long>(aIn);
  return NS_OK;
}

nsresult
Convert(uint8_t aIn, BluetoothA2dpAudioState& aOut)
{
  static const BluetoothA2dpAudioState sAudioState[] = {
    CONVERT(0x00, A2DP_AUDIO_STATE_REMOTE_SUSPEND),
    CONVERT(0x01, A2DP_AUDIO_STATE_STOPPED),
    CONVERT(0x02, A2DP_AUDIO_STATE_STARTED)
  };
  if (NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sAudioState))) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sAudioState[aIn];
  return NS_OK;
}

nsresult
Convert(uint8_t aIn, BluetoothA2dpConnectionState& aOut)
{
  static const BluetoothA2dpConnectionState sConnectionState[] = {
    CONVERT(0x00, A2DP_CONNECTION_STATE_DISCONNECTED),
    CONVERT(0x01, A2DP_CONNECTION_STATE_CONNECTING),
    CONVERT(0x02, A2DP_CONNECTION_STATE_CONNECTED),
    CONVERT(0x03, A2DP_CONNECTION_STATE_DISCONNECTING)
  };
  if (NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sConnectionState))) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sConnectionState[aIn];
  return NS_OK;
}

nsresult
Convert(uint8_t aIn, BluetoothAclState& aOut)
{
  static const BluetoothAclState sAclState[] = {
    CONVERT(0x00, ACL_STATE_CONNECTED),
    CONVERT(0x01, ACL_STATE_DISCONNECTED),
  };
  if (NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sAclState))) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sAclState[aIn];
  return NS_OK;
}

nsresult
Convert(uint8_t aIn, BluetoothAvrcpEvent& aOut)
{
  static const BluetoothAvrcpEvent sAvrcpEvent[] = {
    CONVERT(0x00, static_cast<BluetoothAvrcpEvent>(0)),
    CONVERT(0x01, AVRCP_EVENT_PLAY_STATUS_CHANGED),
    CONVERT(0x02, AVRCP_EVENT_TRACK_CHANGE),
    CONVERT(0x03, AVRCP_EVENT_TRACK_REACHED_END),
    CONVERT(0x04, AVRCP_EVENT_TRACK_REACHED_START),
    CONVERT(0x05, AVRCP_EVENT_PLAY_POS_CHANGED),
    CONVERT(0x06, static_cast<BluetoothAvrcpEvent>(0)),
    CONVERT(0x07, static_cast<BluetoothAvrcpEvent>(0)),
    CONVERT(0x08, AVRCP_EVENT_APP_SETTINGS_CHANGED)
  };
  if (NS_WARN_IF(!aIn) ||
      NS_WARN_IF(aIn == 0x06) ||
      NS_WARN_IF(aIn == 0x07) ||
      NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sAvrcpEvent))) {
    aOut = static_cast<BluetoothAvrcpEvent>(0); // silences compiler warning
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sAvrcpEvent[aIn];
  return NS_OK;
}

nsresult
Convert(uint8_t aIn, BluetoothAvrcpMediaAttribute& aOut)
{
  static const BluetoothAvrcpMediaAttribute sAvrcpMediaAttribute[] = {
    CONVERT(0x00, static_cast<BluetoothAvrcpMediaAttribute>(0)),
    CONVERT(0x01, AVRCP_MEDIA_ATTRIBUTE_TITLE),
    CONVERT(0x02, AVRCP_MEDIA_ATTRIBUTE_ARTIST),
    CONVERT(0x03, AVRCP_MEDIA_ATTRIBUTE_ALBUM),
    CONVERT(0x04, AVRCP_MEDIA_ATTRIBUTE_TRACK_NUM),
    CONVERT(0x05, AVRCP_MEDIA_ATTRIBUTE_NUM_TRACKS),
    CONVERT(0x06, AVRCP_MEDIA_ATTRIBUTE_GENRE),
    CONVERT(0x07, AVRCP_MEDIA_ATTRIBUTE_PLAYING_TIME)
  };
  if (NS_WARN_IF(!aIn) ||
      NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sAvrcpMediaAttribute))) {
    // silences compiler warning
    aOut = static_cast<BluetoothAvrcpMediaAttribute>(0);
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sAvrcpMediaAttribute[aIn];
  return NS_OK;
}

nsresult
Convert(uint8_t aIn, BluetoothAvrcpPlayerAttribute& aOut)
{
  static const BluetoothAvrcpPlayerAttribute sAvrcpPlayerAttribute[] = {
    CONVERT(0x00, static_cast<BluetoothAvrcpPlayerAttribute>(0)),
    CONVERT(0x01, AVRCP_PLAYER_ATTRIBUTE_EQUALIZER),
    CONVERT(0x02, AVRCP_PLAYER_ATTRIBUTE_REPEAT),
    CONVERT(0x03, AVRCP_PLAYER_ATTRIBUTE_SHUFFLE),
    CONVERT(0x04, AVRCP_PLAYER_ATTRIBUTE_SCAN)
  };
  if (NS_WARN_IF(!aIn) ||
      NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sAvrcpPlayerAttribute))) {
    // silences compiler warning
    aOut = static_cast<BluetoothAvrcpPlayerAttribute>(0);
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sAvrcpPlayerAttribute[aIn];
  return NS_OK;
}

nsresult
Convert(uint8_t aIn, BluetoothAvrcpRemoteFeature& aOut)
{
  static const BluetoothAvrcpRemoteFeature sAvrcpRemoteFeature[] = {
    CONVERT(0x00, AVRCP_REMOTE_FEATURE_NONE),
    CONVERT(0x01, AVRCP_REMOTE_FEATURE_METADATA),
    CONVERT(0x02, AVRCP_REMOTE_FEATURE_ABSOLUTE_VOLUME),
    CONVERT(0x03, AVRCP_REMOTE_FEATURE_BROWSE)
  };
  if (NS_WARN_IF(!aIn) ||
      NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sAvrcpRemoteFeature))) {
    // silences compiler warning
    aOut = static_cast<BluetoothAvrcpRemoteFeature>(0);
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sAvrcpRemoteFeature[aIn];
  return NS_OK;
}

nsresult
Convert(uint8_t aIn, BluetoothBondState& aOut)
{
  static const BluetoothBondState sBondState[] = {
    CONVERT(0x00, BOND_STATE_NONE),
    CONVERT(0x01, BOND_STATE_BONDING),
    CONVERT(0x02, BOND_STATE_BONDED)
  };
  if (NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sBondState))) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sBondState[aIn];
  return NS_OK;
}

nsresult
Convert(uint8_t aIn, BluetoothHandsfreeAudioState& aOut)
{
  static const BluetoothHandsfreeAudioState sAudioState[] = {
    CONVERT(0x00, HFP_AUDIO_STATE_DISCONNECTED),
    CONVERT(0x01, HFP_AUDIO_STATE_CONNECTING),
    CONVERT(0x02, HFP_AUDIO_STATE_CONNECTED),
    CONVERT(0x03, HFP_AUDIO_STATE_DISCONNECTING)
  };
  if (NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sAudioState))) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sAudioState[aIn];
  return NS_OK;
}

nsresult
Convert(uint8_t aIn, BluetoothHandsfreeCallHoldType& aOut)
{
  static const BluetoothHandsfreeCallHoldType sCallHoldType[] = {
    CONVERT(0x00, HFP_CALL_HOLD_RELEASEHELD),
    CONVERT(0x01, HFP_CALL_HOLD_RELEASEACTIVE_ACCEPTHELD),
    CONVERT(0x02, HFP_CALL_HOLD_HOLDACTIVE_ACCEPTHELD),
    CONVERT(0x03, HFP_CALL_HOLD_ADDHELDTOCONF)
  };
  if (NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sCallHoldType))) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sCallHoldType[aIn];
  return NS_OK;
}

nsresult
Convert(uint8_t aIn, BluetoothHandsfreeConnectionState& aOut)
{
  static const BluetoothHandsfreeConnectionState sConnectionState[] = {
    CONVERT(0x00, HFP_CONNECTION_STATE_DISCONNECTED),
    CONVERT(0x01, HFP_CONNECTION_STATE_CONNECTING),
    CONVERT(0x02, HFP_CONNECTION_STATE_CONNECTED),
    CONVERT(0x03, HFP_CONNECTION_STATE_SLC_CONNECTED),
    CONVERT(0x04, HFP_CONNECTION_STATE_DISCONNECTING)
  };
  if (NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sConnectionState))) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sConnectionState[aIn];
  return NS_OK;
}

nsresult
Convert(uint8_t aIn, BluetoothHandsfreeNRECState& aOut)
{
  static const BluetoothHandsfreeNRECState sNRECState[] = {
    CONVERT(0x00, HFP_NREC_STOPPED),
    CONVERT(0x01, HFP_NREC_STARTED)
  };
  if (NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sNRECState))) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sNRECState[aIn];
  return NS_OK;
}

nsresult
Convert(uint8_t aIn, BluetoothHandsfreeVoiceRecognitionState& aOut)
{
  static const BluetoothHandsfreeVoiceRecognitionState sState[] = {
    CONVERT(0x00, HFP_VOICE_RECOGNITION_STOPPED),
    CONVERT(0x01, HFP_VOICE_RECOGNITION_STOPPED)
  };
  if (NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sState))) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sState[aIn];
  return NS_OK;
}

nsresult
Convert(uint8_t aIn, BluetoothHandsfreeVolumeType& aOut)
{
  static const BluetoothHandsfreeVolumeType sVolumeType[] = {
    CONVERT(0x00, HFP_VOLUME_TYPE_SPEAKER),
    CONVERT(0x01, HFP_VOLUME_TYPE_MICROPHONE)
  };
  if (NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sVolumeType))) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sVolumeType[aIn];
  return NS_OK;
}

nsresult
Convert(int aIn, int32_t& aOut)
{
  if (NS_WARN_IF(aIn < std::numeric_limits<int32_t>::min()) ||
      NS_WARN_IF(aIn > std::numeric_limits<int32_t>::max())) {
    aOut = 0; // silences compiler warning
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = static_cast<int32_t>(aIn);
  return NS_OK;
}

nsresult
Convert(int32_t aIn, BluetoothTypeOfDevice& aOut)
{
  static const BluetoothTypeOfDevice sTypeOfDevice[] = {
    CONVERT(0x00, static_cast<BluetoothTypeOfDevice>(0)), // invalid, required by gcc
    CONVERT(0x01, TYPE_OF_DEVICE_BREDR),
    CONVERT(0x02, TYPE_OF_DEVICE_BLE),
    CONVERT(0x03, TYPE_OF_DEVICE_DUAL)
  };
  if (NS_WARN_IF(!aIn) ||
      NS_WARN_IF(static_cast<size_t>(aIn) >= MOZ_ARRAY_LENGTH(sTypeOfDevice))) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sTypeOfDevice[aIn];
  return NS_OK;
}

nsresult
Convert(uint8_t aIn, BluetoothPropertyType& aOut)
{
  static const BluetoothPropertyType sPropertyType[] = {
    CONVERT(0x00, static_cast<BluetoothPropertyType>(0)), // invalid, required by gcc
    CONVERT(0x01, PROPERTY_BDNAME),
    CONVERT(0x02, PROPERTY_BDADDR),
    CONVERT(0x03, PROPERTY_UUIDS),
    CONVERT(0x04, PROPERTY_CLASS_OF_DEVICE),
    CONVERT(0x05, PROPERTY_TYPE_OF_DEVICE),
    CONVERT(0x06, PROPERTY_SERVICE_RECORD),
    CONVERT(0x07, PROPERTY_ADAPTER_SCAN_MODE),
    CONVERT(0x08, PROPERTY_ADAPTER_BONDED_DEVICES),
    CONVERT(0x09, PROPERTY_ADAPTER_DISCOVERY_TIMEOUT),
    CONVERT(0x0a, PROPERTY_REMOTE_FRIENDLY_NAME),
    CONVERT(0x0b, PROPERTY_REMOTE_RSSI),
    CONVERT(0x0c, PROPERTY_REMOTE_VERSION_INFO)
  };
  if (aIn == 0xff) {
    /* This case is handled separately to not populate
     * |sPropertyType| with empty entries. */
    aOut = PROPERTY_REMOTE_DEVICE_TIMESTAMP;
    return NS_OK;
  }
  if (NS_WARN_IF(!aIn) ||
      NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sPropertyType))) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sPropertyType[aIn];
  return NS_OK;
}

nsresult
Convert(BluetoothSocketType aIn, uint8_t& aOut)
{
  static const uint8_t sSocketType[] = {
    CONVERT(0, 0), // silences compiler warning
    CONVERT(BluetoothSocketType::RFCOMM, 0x01),
    CONVERT(BluetoothSocketType::SCO, 0x02),
    CONVERT(BluetoothSocketType::L2CAP, 0x03)
    // EL2CAP not supported
  };
  if (NS_WARN_IF(aIn == BluetoothSocketType::EL2CAP) ||
      NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sSocketType)) ||
      NS_WARN_IF(!sSocketType[aIn])) {
    aOut = 0; // silences compiler warning
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sSocketType[aIn];
  return NS_OK;
}

nsresult
Convert(int32_t aIn, BluetoothScanMode& aOut)
{
  static const BluetoothScanMode sScanMode[] = {
    CONVERT(0x00, SCAN_MODE_NONE),
    CONVERT(0x01, SCAN_MODE_CONNECTABLE),
    CONVERT(0x02, SCAN_MODE_CONNECTABLE_DISCOVERABLE)
  };
  if (NS_WARN_IF(aIn < 0) ||
      NS_WARN_IF(static_cast<size_t>(aIn) >= MOZ_ARRAY_LENGTH(sScanMode))) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sScanMode[aIn];
  return NS_OK;
}

nsresult
Convert(uint8_t aIn, BluetoothSspVariant& aOut)
{
  static const BluetoothSspVariant sSspVariant[] = {
    CONVERT(0x00, SSP_VARIANT_PASSKEY_CONFIRMATION),
    CONVERT(0x01, SSP_VARIANT_PASSKEY_ENTRY),
    CONVERT(0x02, SSP_VARIANT_CONSENT),
    CONVERT(0x03, SSP_VARIANT_PASSKEY_NOTIFICATION)
  };
  if (NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sSspVariant))) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sSspVariant[aIn];
  return NS_OK;
}

nsresult
Convert(uint8_t aIn, BluetoothStatus& aOut)
{
  static const BluetoothStatus sStatus[] = {
    CONVERT(0x00, STATUS_SUCCESS),
    CONVERT(0x01, STATUS_FAIL),
    CONVERT(0x02, STATUS_NOT_READY),
    CONVERT(0x03, STATUS_NOMEM),
    CONVERT(0x04, STATUS_BUSY),
    CONVERT(0x05, STATUS_DONE),
    CONVERT(0x06, STATUS_UNSUPPORTED),
    CONVERT(0x07, STATUS_PARM_INVALID),
    CONVERT(0x08, STATUS_UNHANDLED),
    CONVERT(0x09, STATUS_AUTH_FAILURE),
    CONVERT(0x0a, STATUS_RMT_DEV_DOWN)
  };
  if (NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sStatus))) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sStatus[aIn];
  return NS_OK;
}

nsresult
Convert(uint32_t aIn, int& aOut)
{
  aOut = static_cast<int>(aIn);
  return NS_OK;
}

nsresult
Convert(uint32_t aIn, uint8_t& aOut)
{
  if (NS_WARN_IF(aIn < std::numeric_limits<uint8_t>::min()) ||
      NS_WARN_IF(aIn > std::numeric_limits<uint8_t>::max())) {
    aOut = 0; // silences compiler warning
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = static_cast<uint8_t>(aIn);
  return NS_OK;
}

nsresult
Convert(size_t aIn, uint16_t& aOut)
{
  if (NS_WARN_IF(aIn >= (1ul << 16))) {
    aOut = 0; // silences compiler warning
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = static_cast<uint16_t>(aIn);
  return NS_OK;
}

nsresult
Convert(const nsAString& aIn, BluetoothAddress& aOut)
{
  NS_ConvertUTF16toUTF8 bdAddressUTF8(aIn);
  const char* str = bdAddressUTF8.get();

  for (size_t i = 0; i < MOZ_ARRAY_LENGTH(aOut.mAddr); ++i, ++str) {
    aOut.mAddr[i] =
      static_cast<uint8_t>(strtoul(str, const_cast<char**>(&str), 16));
  }

  return NS_OK;
}

nsresult
Convert(const nsAString& aIn, BluetoothPinCode& aOut)
{
  if (NS_WARN_IF(aIn.Length() > MOZ_ARRAY_LENGTH(aOut.mPinCode))) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  NS_ConvertUTF16toUTF8 pinCodeUTF8(aIn);
  const char* str = pinCodeUTF8.get();

  nsAString::size_type i;

  // Fill pin into aOut
  for (i = 0; i < aIn.Length(); ++i, ++str) {
    aOut.mPinCode[i] = static_cast<uint8_t>(*str);
  }

  // Clear remaining bytes in aOut
  size_t ntrailing = (MOZ_ARRAY_LENGTH(aOut.mPinCode) - aIn.Length()) *
                     sizeof(aOut.mPinCode[0]);
  memset(aOut.mPinCode + aIn.Length(), 0, ntrailing);

  aOut.mLength = aIn.Length();

  return NS_OK;
}

nsresult
Convert(const nsAString& aIn, BluetoothPropertyType& aOut)
{
  if (aIn.EqualsLiteral("Name")) {
    aOut = PROPERTY_BDNAME;
  } else if (aIn.EqualsLiteral("Discoverable")) {
    aOut = PROPERTY_ADAPTER_SCAN_MODE;
  } else if (aIn.EqualsLiteral("DiscoverableTimeout")) {
    aOut = PROPERTY_ADAPTER_DISCOVERY_TIMEOUT;
  } else {
    BT_LOGR("Invalid property name: %s", NS_ConvertUTF16toUTF8(aIn).get());
    aOut = static_cast<BluetoothPropertyType>(0); // silences compiler warning
    return NS_ERROR_ILLEGAL_VALUE;
  }
  return NS_OK;
}

nsresult
Convert(const nsAString& aIn, BluetoothServiceName& aOut)
{
  NS_ConvertUTF16toUTF8 serviceNameUTF8(aIn);
  const char* str = serviceNameUTF8.get();
  size_t len = strlen(str);

  if (NS_WARN_IF(len > sizeof(aOut.mName))) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  memcpy(aOut.mName, str, len);
  memset(aOut.mName + len, 0, sizeof(aOut.mName) - len);

  return NS_OK;
}

nsresult
Convert(const nsAString& aIn, BluetoothSspVariant& aOut)
{
  if (aIn.EqualsLiteral("PasskeyConfirmation")) {
    aOut = SSP_VARIANT_PASSKEY_CONFIRMATION;
  } else if (aIn.EqualsLiteral("PasskeyEntry")) {
    aOut = SSP_VARIANT_PASSKEY_ENTRY;
  } else if (aIn.EqualsLiteral("Consent")) {
    aOut = SSP_VARIANT_CONSENT;
  } else if (aIn.EqualsLiteral("PasskeyNotification")) {
    aOut = SSP_VARIANT_PASSKEY_NOTIFICATION;
  } else {
    BT_LOGR("Invalid SSP variant name: %s", NS_ConvertUTF16toUTF8(aIn).get());
    aOut = SSP_VARIANT_PASSKEY_CONFIRMATION; // silences compiler warning
    return NS_ERROR_ILLEGAL_VALUE;
  }
  return NS_OK;
}

nsresult
Convert(BluetoothAclState aIn, bool& aOut)
{
  static const bool sBool[] = {
    CONVERT(ACL_STATE_CONNECTED, true),
    CONVERT(ACL_STATE_DISCONNECTED, false)
  };
  if (NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sBool))) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sBool[aIn];
  return NS_OK;
}

nsresult
Convert(const BluetoothAddress& aIn, nsAString& aOut)
{
  char str[BLUETOOTH_ADDRESS_LENGTH + 1];

  int res = snprintf(str, sizeof(str), "%02x:%02x:%02x:%02x:%02x:%02x",
                     static_cast<int>(aIn.mAddr[0]),
                     static_cast<int>(aIn.mAddr[1]),
                     static_cast<int>(aIn.mAddr[2]),
                     static_cast<int>(aIn.mAddr[3]),
                     static_cast<int>(aIn.mAddr[4]),
                     static_cast<int>(aIn.mAddr[5]));
  if (NS_WARN_IF(res < 0)) {
    return NS_ERROR_ILLEGAL_VALUE;
  } else if (NS_WARN_IF((size_t)res >= sizeof(str))) {
    return NS_ERROR_OUT_OF_MEMORY; /* string buffer too small */
  }

  aOut = NS_ConvertUTF8toUTF16(str);

  return NS_OK;
}

nsresult
Convert(BluetoothAvrcpEvent aIn, uint8_t& aOut)
{
  static const uint8_t sValue[] = {
    CONVERT(AVRCP_EVENT_PLAY_STATUS_CHANGED, 0x01),
    CONVERT(AVRCP_EVENT_TRACK_CHANGE, 0x02),
    CONVERT(AVRCP_EVENT_TRACK_REACHED_END, 0x03),
    CONVERT(AVRCP_EVENT_TRACK_REACHED_START, 0x04),
    CONVERT(AVRCP_EVENT_PLAY_POS_CHANGED, 0x05),
    CONVERT(AVRCP_EVENT_APP_SETTINGS_CHANGED, 0x08)
  };
  if (NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sValue))) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sValue[aIn];
  return NS_OK;
}

nsresult
Convert(BluetoothAvrcpNotification aIn, uint8_t& aOut)
{
  static const bool sValue[] = {
    CONVERT(AVRCP_NTF_INTERIM, 0x00),
    CONVERT(AVRCP_NTF_CHANGED, 0x01)
  };
  if (NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sValue))) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sValue[aIn];
  return NS_OK;
}

nsresult
Convert(BluetoothAvrcpPlayerAttribute aIn, uint8_t& aOut)
{
  static const uint8_t sValue[] = {
    CONVERT(AVRCP_PLAYER_ATTRIBUTE_EQUALIZER, 0x01),
    CONVERT(AVRCP_PLAYER_ATTRIBUTE_REPEAT, 0x02),
    CONVERT(AVRCP_PLAYER_ATTRIBUTE_SHUFFLE, 0x03),
    CONVERT(AVRCP_PLAYER_ATTRIBUTE_SCAN, 0x04)
  };
  if (NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sValue))) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sValue[aIn];
  return NS_OK;
}

nsresult
Convert(BluetoothAvrcpRemoteFeature aIn, unsigned long& aOut)
{
  if (NS_WARN_IF(aIn < std::numeric_limits<unsigned long>::min()) ||
      NS_WARN_IF(aIn > std::numeric_limits<unsigned long>::max())) {
    aOut = 0; // silences compiler warning
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = static_cast<unsigned long>(aIn);
  return NS_OK;
}

nsresult
Convert(BluetoothAvrcpStatus aIn, uint8_t& aOut)
{
  static const uint8_t sValue[] = {
    CONVERT(AVRCP_STATUS_BAD_COMMAND, 0x00),
    CONVERT(AVRCP_STATUS_BAD_PARAMETER, 0x01),
    CONVERT(AVRCP_STATUS_NOT_FOUND, 0x02),
    CONVERT(AVRCP_STATUS_INTERNAL_ERROR, 0x03),
    CONVERT(AVRCP_STATUS_SUCCESS, 0x04)
  };
  if (NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sValue))) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sValue[aIn];
  return NS_OK;
}

nsresult
Convert(BluetoothHandsfreeAtResponse aIn, uint8_t& aOut)
{
  static const uint8_t sAtResponse[] = {
    CONVERT(HFP_AT_RESPONSE_ERROR, 0x00),
    CONVERT(HFP_AT_RESPONSE_OK, 0x01)
  };
  if (NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sAtResponse))) {
    aOut = 0x00; // silences compiler warning
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sAtResponse[aIn];
  return NS_OK;
}

nsresult
Convert(BluetoothHandsfreeCallAddressType aIn, uint8_t& aOut)
{
  static const uint8_t sCallAddressType[] = {
    CONVERT(HFP_CALL_ADDRESS_TYPE_UNKNOWN, 0x81),
    CONVERT(HFP_CALL_ADDRESS_TYPE_INTERNATIONAL, 0x91)
  };
  if (NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sCallAddressType))) {
    aOut = 0x00; // silences compiler warning
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sCallAddressType[aIn];
  return NS_OK;
}

nsresult
Convert(BluetoothHandsfreeCallDirection aIn, uint8_t& aOut)
{
  static const uint8_t sCallDirection[] = {
    CONVERT(HFP_CALL_DIRECTION_OUTGOING, 0x00),
    CONVERT(HFP_CALL_DIRECTION_INCOMING, 0x01)
  };
  if (NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sCallDirection))) {
    aOut = 0x00; // silences compiler warning
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sCallDirection[aIn];
  return NS_OK;
}

nsresult
Convert(BluetoothHandsfreeCallState aIn, uint8_t& aOut)
{
  static const uint8_t sCallState[] = {
    CONVERT(HFP_CALL_STATE_ACTIVE, 0x00),
    CONVERT(HFP_CALL_STATE_HELD, 0x01),
    CONVERT(HFP_CALL_STATE_DIALING, 0x02),
    CONVERT(HFP_CALL_STATE_ALERTING, 0x03),
    CONVERT(HFP_CALL_STATE_INCOMING, 0x04),
    CONVERT(HFP_CALL_STATE_WAITING, 0x05),
    CONVERT(HFP_CALL_STATE_IDLE, 0x06)
  };
  if (NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sCallState))) {
    aOut = 0x00; // silences compiler warning
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sCallState[aIn];
  return NS_OK;
}

nsresult
Convert(BluetoothHandsfreeCallMode aIn, uint8_t& aOut)
{
  static const uint8_t sCallMode[] = {
    CONVERT(HFP_CALL_MODE_VOICE, 0x00),
    CONVERT(HFP_CALL_MODE_DATA, 0x01),
    CONVERT(HFP_CALL_MODE_FAX, 0x02)
  };
  if (NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sCallMode))) {
    aOut = 0x00; // silences compiler warning
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sCallMode[aIn];
  return NS_OK;
}

nsresult
Convert(BluetoothHandsfreeCallMptyType aIn, uint8_t& aOut)
{
  static const uint8_t sCallMptyType[] = {
    CONVERT(HFP_CALL_MPTY_TYPE_SINGLE, 0x00),
    CONVERT(HFP_CALL_MPTY_TYPE_MULTI, 0x01)
  };
  if (NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sCallMptyType))) {
    aOut = 0x00; // silences compiler warning
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sCallMptyType[aIn];
  return NS_OK;
}

nsresult
Convert(BluetoothHandsfreeNetworkState aIn, uint8_t& aOut)
{
  static const uint8_t sNetworkState[] = {
    CONVERT(HFP_NETWORK_STATE_NOT_AVAILABLE, 0x00),
    CONVERT(HFP_NETWORK_STATE_AVAILABLE, 0x01)
  };
  if (NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sNetworkState))) {
    aOut = 0x00; // silences compiler warning
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sNetworkState[aIn];
  return NS_OK;
}

nsresult
Convert(BluetoothHandsfreeServiceType aIn, uint8_t& aOut)
{
  static const uint8_t sServiceType[] = {
    CONVERT(HFP_SERVICE_TYPE_HOME, 0x00),
    CONVERT(HFP_SERVICE_TYPE_ROAMING, 0x01)
  };
  if (NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sServiceType))) {
    aOut = 0x00; // silences compiler warning
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sServiceType[aIn];
  return NS_OK;
}

nsresult
Convert(BluetoothHandsfreeVolumeType aIn, uint8_t& aOut)
{
  static const uint8_t sVolumeType[] = {
    CONVERT(HFP_VOLUME_TYPE_SPEAKER, 0x00),
    CONVERT(HFP_VOLUME_TYPE_MICROPHONE, 0x01)
  };
  if (NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sVolumeType))) {
    aOut = 0x00; // silences compiler warning
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sVolumeType[aIn];
  return NS_OK;
}

nsresult
Convert(BluetoothPropertyType aIn, uint8_t& aOut)
{
  static const uint8_t sPropertyType[] = {
    CONVERT(PROPERTY_UNKNOWN, 0x00),
    CONVERT(PROPERTY_BDNAME, 0x01),
    CONVERT(PROPERTY_BDADDR, 0x02),
    CONVERT(PROPERTY_UUIDS, 0x03),
    CONVERT(PROPERTY_CLASS_OF_DEVICE, 0x04),
    CONVERT(PROPERTY_TYPE_OF_DEVICE, 0x05),
    CONVERT(PROPERTY_SERVICE_RECORD, 0x06),
    CONVERT(PROPERTY_ADAPTER_SCAN_MODE, 0x07),
    CONVERT(PROPERTY_ADAPTER_BONDED_DEVICES, 0x08),
    CONVERT(PROPERTY_ADAPTER_DISCOVERY_TIMEOUT, 0x09),
    CONVERT(PROPERTY_REMOTE_FRIENDLY_NAME, 0x0a),
    CONVERT(PROPERTY_REMOTE_RSSI, 0x0b),
    CONVERT(PROPERTY_REMOTE_VERSION_INFO, 0x0c),
    CONVERT(PROPERTY_REMOTE_DEVICE_TIMESTAMP, 0xff)
  };
  if (NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sPropertyType))) {
    aOut = 0x00; // silences compiler warning
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sPropertyType[aIn];
  return NS_OK;
}

nsresult
Convert(const BluetoothRemoteName& aIn, nsAString& aOut)
{
  // We construct an nsCString here because the string
  // returned from the PDU is not 0-terminated.
  aOut = NS_ConvertUTF8toUTF16(
    nsCString(reinterpret_cast<const char*>(aIn.mName), sizeof(aIn.mName)));
  return NS_OK;
}

nsresult
Convert(BluetoothScanMode aIn, int32_t& aOut)
{
  static const int32_t sScanMode[] = {
    CONVERT(SCAN_MODE_NONE, 0x00),
    CONVERT(SCAN_MODE_CONNECTABLE, 0x01),
    CONVERT(SCAN_MODE_CONNECTABLE_DISCOVERABLE, 0x02)
  };
  if (NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sScanMode))) {
    aOut = 0; // silences compiler warning
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sScanMode[aIn];
  return NS_OK;
}

nsresult
Convert(BluetoothSspVariant aIn, uint8_t& aOut)
{
  static const uint8_t sValue[] = {
    CONVERT(SSP_VARIANT_PASSKEY_CONFIRMATION, 0x00),
    CONVERT(SSP_VARIANT_PASSKEY_ENTRY, 0x01),
    CONVERT(SSP_VARIANT_CONSENT, 0x02),
    CONVERT(SSP_VARIANT_PASSKEY_NOTIFICATION, 0x03)
  };
  if (NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sValue))) {
    aOut = 0; // silences compiler warning
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sValue[aIn];
  return NS_OK;
}

nsresult
Convert(ControlPlayStatus aIn, uint8_t& aOut)
{
  static const uint8_t sValue[] = {
    CONVERT(PLAYSTATUS_STOPPED, 0x00),
    CONVERT(PLAYSTATUS_PLAYING, 0x01),
    CONVERT(PLAYSTATUS_PAUSED, 0x02),
    CONVERT(PLAYSTATUS_FWD_SEEK, 0x03),
    CONVERT(PLAYSTATUS_REV_SEEK, 0x04)
  };
  if (aIn == PLAYSTATUS_ERROR) {
    /* This case is handled separately to not populate
     * |sValue| with empty entries. */
    aOut = 0xff;
    return NS_OK;
  }
  if (NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sValue))) {
    aOut = 0; // silences compiler warning
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sValue[aIn];
  return NS_OK;
}

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

//
// Packing
//

nsresult
PackPDU(bool aIn, BluetoothDaemonPDU& aPDU)
{
  return PackPDU(PackConversion<bool, uint8_t>(aIn), aPDU);
}

nsresult
PackPDU(const BluetoothAddress& aIn, BluetoothDaemonPDU& aPDU)
{
  return PackPDU(PackArray<uint8_t>(aIn.mAddr, sizeof(aIn.mAddr)), aPDU);
}

nsresult
PackPDU(const BluetoothAvrcpAttributeTextPairs& aIn,
        BluetoothDaemonPDU& aPDU)
{
  size_t i;

  for (i = 0; i < aIn.mLength; ++i) {
    nsresult rv = PackPDU(aIn.mAttr[i], aPDU);
    if (NS_FAILED(rv)) {
      return rv;
    }

    uint8_t len;
    const uint8_t* str;

    if (aIn.mText[i]) {
      str = reinterpret_cast<const uint8_t*>(aIn.mText[i]);
      len = strlen(aIn.mText[i]) + 1;
    } else {
      /* write \0 character for NULL strings */
      str = reinterpret_cast<const uint8_t*>("\0");
      len = 1;
    }

    rv = PackPDU(len, aPDU);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = PackPDU(PackArray<uint8_t>(str, len), aPDU);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  return NS_OK;
}

nsresult
PackPDU(const BluetoothAvrcpAttributeValuePairs& aIn,
        BluetoothDaemonPDU& aPDU)
{
  size_t i;

  for (i = 0; i < aIn.mLength; ++i) {
    nsresult rv = PackPDU(aIn.mAttr[i], aPDU);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = PackPDU(aIn.mValue[i], aPDU);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  return NS_OK;
}

nsresult
PackPDU(const BluetoothAvrcpElementAttribute& aIn, BluetoothDaemonPDU& aPDU)
{
  nsresult rv = PackPDU(PackConversion<uint32_t, uint8_t>(aIn.mId), aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }

  const NS_ConvertUTF16toUTF8 cstr(aIn.mValue);

  if (NS_WARN_IF(cstr.Length() == PR_UINT32_MAX)) {
    return NS_ERROR_ILLEGAL_VALUE; /* integer overflow detected */
  }

  PRUint32 clen = cstr.Length() + 1; /* include \0 character */

  rv = PackPDU(PackConversion<PRUint32, uint8_t>(clen), aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return PackPDU(
    PackArray<uint8_t>(reinterpret_cast<const uint8_t*>(cstr.get()), clen),
    aPDU);
}

nsresult
PackPDU(BluetoothAvrcpEvent aIn, BluetoothDaemonPDU& aPDU)
{
  return PackPDU(PackConversion<BluetoothAvrcpEvent, uint8_t>(aIn), aPDU);
}

nsresult
PackPDU(const BluetoothAvrcpEventParamPair& aIn, BluetoothDaemonPDU& aPDU)
{
  nsresult rv;

  switch (aIn.mEvent) {
    case AVRCP_EVENT_PLAY_STATUS_CHANGED:
      rv = PackPDU(aIn.mParam.mPlayStatus, aPDU);
      break;
    case AVRCP_EVENT_TRACK_CHANGE:
      rv = PackPDU(PackArray<uint8_t>(aIn.mParam.mTrack,
                                      MOZ_ARRAY_LENGTH(aIn.mParam.mTrack)),
                   aPDU);
      break;
    case AVRCP_EVENT_TRACK_REACHED_END:
      /* fall through */
    case AVRCP_EVENT_TRACK_REACHED_START:
      /* no data to pack */
      rv = NS_OK;
      break;
    case AVRCP_EVENT_PLAY_POS_CHANGED:
      rv = PackPDU(aIn.mParam.mSongPos, aPDU);
      break;
    case AVRCP_EVENT_APP_SETTINGS_CHANGED:
      /* pack number of attribute-value pairs */
      rv = PackPDU(aIn.mParam.mNumAttr, aPDU);
      if (NS_FAILED(rv)) {
        return rv;
      }
      /* pack attribute-value pairs */
      rv = PackPDU(BluetoothAvrcpAttributeValuePairs(aIn.mParam.mIds,
                                                     aIn.mParam.mValues,
                                                     aIn.mParam.mNumAttr),
                   aPDU);
      break;
    default:
      rv = NS_ERROR_ILLEGAL_VALUE;
      break;
  }
  return rv;
}

nsresult
PackPDU(BluetoothAvrcpNotification aIn, BluetoothDaemonPDU& aPDU)
{
  return PackPDU(
    PackConversion<BluetoothAvrcpNotification, uint8_t>(aIn), aPDU);
}

nsresult
PackPDU(BluetoothAvrcpPlayerAttribute aIn, BluetoothDaemonPDU& aPDU)
{
  return PackPDU(
    PackConversion<BluetoothAvrcpPlayerAttribute, uint8_t>(aIn), aPDU);
}

nsresult
PackPDU(BluetoothAvrcpStatus aIn, BluetoothDaemonPDU& aPDU)
{
  return PackPDU(PackConversion<BluetoothAvrcpStatus, uint8_t>(aIn), aPDU);
}

nsresult
PackPDU(const BluetoothConfigurationParameter& aIn, BluetoothDaemonPDU& aPDU)
{
  return PackPDU(aIn.mType, aIn.mLength,
                 PackArray<uint8_t>(aIn.mValue.get(), aIn.mLength), aPDU);
}

nsresult
PackPDU(const BluetoothDaemonPDUHeader& aIn, BluetoothDaemonPDU& aPDU)
{
  return PackPDU(aIn.mService, aIn.mOpcode, aIn.mLength, aPDU);
}

nsresult
PackPDU(const BluetoothHandsfreeAtResponse& aIn, BluetoothDaemonPDU& aPDU)
{
  return PackPDU(
    PackConversion<BluetoothHandsfreeAtResponse, uint8_t>(aIn), aPDU);
}

nsresult
PackPDU(const BluetoothHandsfreeCallAddressType& aIn, BluetoothDaemonPDU& aPDU)
{
  return PackPDU(
    PackConversion<BluetoothHandsfreeCallAddressType, uint8_t>(aIn), aPDU);
}

nsresult
PackPDU(const BluetoothHandsfreeCallDirection& aIn, BluetoothDaemonPDU& aPDU)
{
  return PackPDU(
    PackConversion<BluetoothHandsfreeCallDirection, uint8_t>(aIn), aPDU);
}

nsresult
PackPDU(const BluetoothHandsfreeCallMode& aIn, BluetoothDaemonPDU& aPDU)
{
  return PackPDU(
    PackConversion<BluetoothHandsfreeCallMode, uint8_t>(aIn), aPDU);
}

nsresult
PackPDU(const BluetoothHandsfreeCallMptyType& aIn, BluetoothDaemonPDU& aPDU)
{
  return PackPDU(
    PackConversion<BluetoothHandsfreeCallMptyType, uint8_t>(aIn), aPDU);
}

nsresult
PackPDU(const BluetoothHandsfreeCallState& aIn, BluetoothDaemonPDU& aPDU)
{
  return PackPDU(
    PackConversion<BluetoothHandsfreeCallState, uint8_t>(aIn), aPDU);
}

nsresult
PackPDU(const BluetoothHandsfreeNetworkState& aIn, BluetoothDaemonPDU& aPDU)
{
  return PackPDU(
    PackConversion<BluetoothHandsfreeNetworkState, uint8_t>(aIn), aPDU);
}

nsresult
PackPDU(const BluetoothHandsfreeServiceType& aIn, BluetoothDaemonPDU& aPDU)
{
  return PackPDU(
    PackConversion<BluetoothHandsfreeServiceType, uint8_t>(aIn), aPDU);
}

nsresult
PackPDU(const BluetoothHandsfreeVolumeType& aIn, BluetoothDaemonPDU& aPDU)
{
  return PackPDU(
    PackConversion<BluetoothHandsfreeVolumeType, uint8_t>(aIn), aPDU);
}

nsresult
PackPDU(const BluetoothNamedValue& aIn, BluetoothDaemonPDU& aPDU)
{
  nsresult rv = PackPDU(
    PackConversion<nsString, BluetoothPropertyType>(aIn.name()), aPDU);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (aIn.value().type() == BluetoothValue::Tuint32_t) {
    // Set discoverable timeout
    rv = PackPDU(static_cast<uint16_t>(sizeof(uint32_t)),
                 aIn.value().get_uint32_t(), aPDU);
  } else if (aIn.value().type() == BluetoothValue::TnsString) {
    // Set name
    const nsCString value =
      NS_ConvertUTF16toUTF8(aIn.value().get_nsString());

    rv = PackPDU(PackConversion<size_t, uint16_t>(value.Length()),
                 PackArray<uint8_t>(
                   reinterpret_cast<const uint8_t*>(value.get()),
                   value.Length()),
                 aPDU);
  } else if (aIn.value().type() == BluetoothValue::Tbool) {
    // Set scan mode
    bool value = aIn.value().get_bool();

    rv = PackPDU(static_cast<uint16_t>(sizeof(int32_t)),
                 PackConversion<bool, BluetoothScanMode>(value), aPDU);
  } else {
    BT_LOGR("Invalid property value type");
    rv = NS_ERROR_ILLEGAL_VALUE;
  }
  return rv;
}

nsresult
PackPDU(const BluetoothPinCode& aIn, BluetoothDaemonPDU& aPDU)
{
  return PackPDU(aIn.mLength,
                 PackArray<uint8_t>(aIn.mPinCode, sizeof(aIn.mPinCode)),
                 aPDU);
}

nsresult
PackPDU(BluetoothPropertyType aIn, BluetoothDaemonPDU& aPDU)
{
  return PackPDU(PackConversion<BluetoothPropertyType, uint8_t>(aIn), aPDU);
}

nsresult
PackPDU(BluetoothSspVariant aIn, BluetoothDaemonPDU& aPDU)
{
  return PackPDU(PackConversion<BluetoothSspVariant, uint8_t>(aIn),
                 aPDU);
}

nsresult
PackPDU(BluetoothScanMode aIn, BluetoothDaemonPDU& aPDU)
{
  return PackPDU(PackConversion<BluetoothScanMode, int32_t>(aIn), aPDU);
}

nsresult
PackPDU(const BluetoothServiceName& aIn, BluetoothDaemonPDU& aPDU)
{
  return PackPDU(PackArray<uint8_t>(aIn.mName, sizeof(aIn.mName)), aPDU);
}

nsresult
PackPDU(BluetoothSocketType aIn, BluetoothDaemonPDU& aPDU)
{
  return PackPDU(PackConversion<BluetoothSocketType, uint8_t>(aIn), aPDU);
}

nsresult
PackPDU(ControlPlayStatus aIn, BluetoothDaemonPDU& aPDU)
{
  return PackPDU(PackConversion<ControlPlayStatus, uint8_t>(aIn), aPDU);
}

//
// Unpacking
//

nsresult
UnpackPDU(BluetoothDaemonPDU& aPDU, bool& aOut)
{
  return UnpackPDU(aPDU, UnpackConversion<uint8_t, bool>(aOut));
}

nsresult
UnpackPDU(BluetoothDaemonPDU& aPDU, char& aOut)
{
  return UnpackPDU(aPDU, UnpackConversion<uint8_t, char>(aOut));
}

nsresult
UnpackPDU(BluetoothDaemonPDU& aPDU, BluetoothA2dpAudioState& aOut)
{
  return UnpackPDU(
    aPDU, UnpackConversion<uint8_t, BluetoothA2dpAudioState>(aOut));
}

nsresult
UnpackPDU(BluetoothDaemonPDU& aPDU, BluetoothA2dpConnectionState& aOut)
{
  return UnpackPDU(
    aPDU, UnpackConversion<uint8_t, BluetoothA2dpConnectionState>(aOut));
}

nsresult
UnpackPDU(BluetoothDaemonPDU& aPDU, BluetoothAclState& aOut)
{
  return UnpackPDU(aPDU, UnpackConversion<uint8_t, BluetoothAclState>(aOut));
}

nsresult
UnpackPDU(BluetoothDaemonPDU& aPDU, BluetoothAvrcpEvent& aOut)
{
  return UnpackPDU(
    aPDU, UnpackConversion<uint8_t, BluetoothAvrcpEvent>(aOut));
}

nsresult
UnpackPDU(BluetoothDaemonPDU& aPDU, BluetoothAvrcpMediaAttribute& aOut)
{
  return UnpackPDU(
    aPDU, UnpackConversion<uint8_t, BluetoothAvrcpMediaAttribute>(aOut));
}

nsresult
UnpackPDU(BluetoothDaemonPDU& aPDU, BluetoothAvrcpPlayerAttribute& aOut)
{
  return UnpackPDU(
    aPDU, UnpackConversion<uint8_t, BluetoothAvrcpPlayerAttribute>(aOut));
}

nsresult
UnpackPDU(BluetoothDaemonPDU& aPDU, BluetoothAvrcpPlayerSettings& aOut)
{
  /* Read number of attribute-value pairs */
  nsresult rv = UnpackPDU(aPDU, aOut.mNumAttr);
  if (NS_FAILED(rv)) {
    return rv;
  }
  /* Read attribute-value pairs */
  for (uint8_t i = 0; i < aOut.mNumAttr; ++i) {
    nsresult rv = UnpackPDU(aPDU, aOut.mIds[i]);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = UnpackPDU(aPDU, aOut.mValues[i]);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  return NS_OK;
}

nsresult
UnpackPDU(BluetoothDaemonPDU& aPDU, BluetoothAvrcpRemoteFeature& aOut)
{
  return UnpackPDU(
    aPDU, UnpackConversion<uint8_t, BluetoothAvrcpRemoteFeature>(aOut));
}

nsresult
UnpackPDU(BluetoothDaemonPDU& aPDU, BluetoothBondState& aOut)
{
  return UnpackPDU(aPDU, UnpackConversion<uint8_t, BluetoothBondState>(aOut));
}

nsresult
UnpackPDU(BluetoothDaemonPDU& aPDU, BluetoothTypeOfDevice& aOut)
{
  return UnpackPDU(
    aPDU, UnpackConversion<int32_t, BluetoothTypeOfDevice>(aOut));
}

nsresult
UnpackPDU(BluetoothDaemonPDU& aPDU, BluetoothHandsfreeAudioState& aOut)
{
  return UnpackPDU(
    aPDU, UnpackConversion<uint8_t, BluetoothHandsfreeAudioState>(aOut));
}

nsresult
UnpackPDU(BluetoothDaemonPDU& aPDU, BluetoothHandsfreeCallHoldType& aOut)
{
  return UnpackPDU(
    aPDU, UnpackConversion<uint8_t, BluetoothHandsfreeCallHoldType>(aOut));
}

nsresult
UnpackPDU(BluetoothDaemonPDU& aPDU, BluetoothHandsfreeConnectionState& aOut)
{
  return UnpackPDU(
    aPDU, UnpackConversion<uint8_t, BluetoothHandsfreeConnectionState>(aOut));
}

nsresult
UnpackPDU(BluetoothDaemonPDU& aPDU, BluetoothHandsfreeNRECState& aOut)
{
  return UnpackPDU(
    aPDU, UnpackConversion<uint8_t, BluetoothHandsfreeNRECState>(aOut));
}

nsresult
UnpackPDU(BluetoothDaemonPDU& aPDU,
          BluetoothHandsfreeVoiceRecognitionState& aOut)
{
  return UnpackPDU(
    aPDU,
    UnpackConversion<uint8_t, BluetoothHandsfreeVoiceRecognitionState>(aOut));
}

nsresult
UnpackPDU(BluetoothDaemonPDU& aPDU, BluetoothHandsfreeVolumeType& aOut)
{
  return UnpackPDU(
    aPDU, UnpackConversion<uint8_t, BluetoothHandsfreeVolumeType>(aOut));
}

nsresult
UnpackPDU(BluetoothDaemonPDU& aPDU, BluetoothProperty& aOut)
{
  nsresult rv = UnpackPDU(aPDU, aOut.mType);
  if (NS_FAILED(rv)) {
    return rv;
  }
  uint16_t len;
  rv = UnpackPDU(aPDU, len);
  if (NS_FAILED(rv)) {
    return rv;
  }

  switch (aOut.mType) {
    case PROPERTY_BDNAME:
      /* fall through */
    case PROPERTY_REMOTE_FRIENDLY_NAME: {
        const uint8_t* data = aPDU.Consume(len);
        if (NS_WARN_IF(!data)) {
          return NS_ERROR_ILLEGAL_VALUE;
        }
        // We construct an nsCString here because the string
        // returned from the PDU is not 0-terminated.
        aOut.mString = NS_ConvertUTF8toUTF16(
          nsCString(reinterpret_cast<const char*>(data), len));
      }
      break;
    case PROPERTY_BDADDR:
      rv = UnpackPDU<BluetoothAddress>(
        aPDU, UnpackConversion<BluetoothAddress, nsAString>(aOut.mString));
      break;
    case PROPERTY_UUIDS: {
        size_t numUuids = len / MAX_UUID_SIZE;
        aOut.mUuidArray.SetLength(numUuids);
        rv = UnpackPDU(aPDU, aOut.mUuidArray);
      }
      break;
    case PROPERTY_CLASS_OF_DEVICE:
      /* fall through */
    case PROPERTY_ADAPTER_DISCOVERY_TIMEOUT:
      rv = UnpackPDU(aPDU, aOut.mUint32);
      break;
    case PROPERTY_TYPE_OF_DEVICE:
      rv = UnpackPDU(aPDU, aOut.mTypeOfDevice);
      break;
    case PROPERTY_SERVICE_RECORD:
      rv = UnpackPDU(aPDU, aOut.mServiceRecord);
      break;
    case PROPERTY_ADAPTER_SCAN_MODE:
      rv = UnpackPDU(aPDU, aOut.mScanMode);
      break;
    case PROPERTY_ADAPTER_BONDED_DEVICES: {
        /* unpack addresses */
        size_t numAddresses = len / BLUETOOTH_ADDRESS_BYTES;
        nsAutoArrayPtr<BluetoothAddress> addresses;
        UnpackArray<BluetoothAddress> addressArray(addresses, numAddresses);
        rv = UnpackPDU(aPDU, addressArray);
        if (NS_FAILED(rv)) {
          return rv;
        }
        /* convert addresses to strings */
        aOut.mStringArray.SetLength(numAddresses);
        ConvertArray<BluetoothAddress> convertArray(addressArray.mData,
                                                    addressArray.mLength);
        rv = Convert(convertArray, aOut.mStringArray);
      }
      break;
    case PROPERTY_REMOTE_RSSI: {
        int8_t rssi;
        rv = UnpackPDU(aPDU, rssi);
        aOut.mInt32 = rssi;
      }
      break;
    case PROPERTY_REMOTE_VERSION_INFO:
      rv = UnpackPDU(aPDU, aOut.mRemoteInfo);
      break;
    case PROPERTY_REMOTE_DEVICE_TIMESTAMP:
      /* nothing to do */
      break;
    default:
      break;
  }
  return rv;
}

nsresult
UnpackPDU(BluetoothDaemonPDU& aPDU, BluetoothPropertyType& aOut)
{
  return UnpackPDU(
    aPDU, UnpackConversion<uint8_t, BluetoothPropertyType>(aOut));
}

nsresult
UnpackPDU(BluetoothDaemonPDU& aPDU, BluetoothRemoteInfo& aOut)
{
  nsresult rv = UnpackPDU(aPDU,
                          UnpackConversion<uint32_t, int>(aOut.mVerMajor));
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = UnpackPDU(aPDU, UnpackConversion<uint32_t, int>(aOut.mVerMinor));
  if (NS_FAILED(rv)) {
    return rv;
  }
  return UnpackPDU(aPDU, UnpackConversion<uint32_t, int>(aOut.mManufacturer));
}

nsresult
UnpackPDU(BluetoothDaemonPDU& aPDU, BluetoothScanMode& aOut)
{
  return UnpackPDU(aPDU, UnpackConversion<int32_t, BluetoothScanMode>(aOut));
}

nsresult
UnpackPDU(BluetoothDaemonPDU& aPDU, BluetoothServiceRecord& aOut)
{
  /* unpack UUID */
  nsresult rv = UnpackPDU(aPDU, aOut.mUuid);
  if (NS_FAILED(rv)) {
    return rv;
  }
  /* unpack channel */
  rv = UnpackPDU(aPDU, aOut.mChannel);
  if (NS_FAILED(rv)) {
    return rv;
  }
  /* unpack name */
  return aPDU.Read(aOut.mName, sizeof(aOut.mName));
}

nsresult
UnpackPDU(BluetoothDaemonPDU& aPDU, BluetoothSspVariant& aOut)
{
  return UnpackPDU(
    aPDU, UnpackConversion<uint8_t, BluetoothSspVariant>(aOut));
}

nsresult
UnpackPDU(BluetoothDaemonPDU& aPDU, BluetoothStatus& aOut)
{
  return UnpackPDU(aPDU, UnpackConversion<uint8_t, BluetoothStatus>(aOut));
}

nsresult
UnpackPDU(BluetoothDaemonPDU& aPDU, nsDependentCString& aOut)
{
  // We get a pointer to the first character in the PDU, a length
  // of 1 ensures we consume the \0 byte. With 'str' pointing to
  // the string in the PDU, we can copy the actual bytes.

  const char* str = reinterpret_cast<const char*>(aPDU.Consume(1));
  if (NS_WARN_IF(!str)) {
    return NS_ERROR_ILLEGAL_VALUE; // end of PDU
  }

  const char* end = static_cast<char*>(memchr(str, '\0', aPDU.GetSize()));
  if (NS_WARN_IF(!end)) {
    return NS_ERROR_ILLEGAL_VALUE; // no string terminator
  }

  ptrdiff_t len = end - str;

  const uint8_t* rest = aPDU.Consume(len);
  if (NS_WARN_IF(!rest)) {
    // We couldn't consume bytes that should have been there.
    return NS_ERROR_ILLEGAL_VALUE;
  }

  aOut.Rebind(str, len);

  return NS_OK;
}

nsresult
UnpackPDU(BluetoothDaemonPDU& aPDU, const UnpackCString0& aOut)
{
  nsDependentCString cstring;

  nsresult rv = UnpackPDU(aPDU, cstring);
  if (NS_FAILED(rv)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  aOut.mString->AssignASCII(cstring.get(), cstring.Length());

  return NS_OK;
}

nsresult
UnpackPDU(BluetoothDaemonPDU& aPDU, const UnpackString0& aOut)
{
  nsDependentCString cstring;

  nsresult rv = UnpackPDU(aPDU, cstring);
  if (NS_FAILED(rv)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  *aOut.mString = NS_ConvertUTF8toUTF16(cstring);

  return NS_OK;
}

END_BLUETOOTH_NAMESPACE
