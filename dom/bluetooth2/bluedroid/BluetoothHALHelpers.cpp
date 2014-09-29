/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothHALHelpers.h"

#define MAX_UUID_SIZE 16

BEGIN_BLUETOOTH_NAMESPACE

//
// Conversion
//

nsresult
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

nsresult
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

nsresult
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

nsresult
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

nsresult
Convert(const uint8_t aIn[16], bt_uuid_t& aOut)
{
  if (sizeof(aOut.uu) != 16) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  memcpy(aOut.uu, aIn, sizeof(aOut.uu));

  return NS_OK;
}

nsresult
Convert(const bt_uuid_t& aIn, BluetoothUuid& aOut)
{
  if (sizeof(aIn.uu) != sizeof(aOut.mUuid)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  memcpy(aOut.mUuid, aIn.uu, sizeof(aOut.mUuid));

  return NS_OK;
}

nsresult
Convert(const BluetoothUuid& aIn, bt_uuid_t& aOut)
{
  if (sizeof(aIn.mUuid) != sizeof(aOut.uu)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  memcpy(aOut.uu, aIn.mUuid, sizeof(aOut.uu));

  return NS_OK;
}

nsresult
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

nsresult
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

nsresult
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

#if ANDROID_VERSION >= 18
nsresult
Convert(const BluetoothAvrcpElementAttribute& aIn, btrc_element_attr_val_t& aOut)
{
  const NS_ConvertUTF16toUTF8 value(aIn.mValue);
  size_t len = std::min<size_t>(strlen(value.get()), sizeof(aOut.text) - 1);

  memcpy(aOut.text, value.get(), len);
  aOut.text[len] = '\0';
  aOut.attr_id = aIn.mId;

  return NS_OK;
}

nsresult
Convert(const btrc_player_settings_t& aIn, BluetoothAvrcpPlayerSettings& aOut)
{
  aOut.mNumAttr = aIn.num_attr;
  memcpy(aOut.mIds, aIn.attr_ids, aIn.num_attr);
  memcpy(aOut.mValues, aIn.attr_values, aIn.num_attr);

  return NS_OK;
}
#endif // ANDROID_VERSION >= 18

nsresult
Convert(const uint8_t* aIn, BluetoothGattAdvData& aOut)
{
  memcpy(aOut.mAdvData, aIn, sizeof(aOut.mAdvData));
  return NS_OK;
}

#if ANDROID_VERSION >= 19
nsresult
Convert(const BluetoothGattId& aIn, btgatt_gatt_id_t& aOut)
{
  aOut.inst_id = aIn.mInstanceId;
  return Convert(aIn.mUuid, aOut.uuid);
}

nsresult
Convert(const btgatt_gatt_id_t& aIn, BluetoothGattId& aOut)
{
  aOut.mInstanceId = aIn.inst_id;
  return Convert(aIn.uuid, aOut.mUuid);
}

nsresult
Convert(const BluetoothGattServiceId& aIn, btgatt_srvc_id_t& aOut)
{
  aOut.is_primary = aIn.mIsPrimary;
  return Convert(aIn.mId, aOut.id);
}

nsresult
Convert(const btgatt_srvc_id_t& aIn, BluetoothGattServiceId& aOut)
{
  aOut.mIsPrimary = aIn.is_primary;
  return Convert(aIn.id, aOut.mId);
}

nsresult
Convert(const btgatt_read_params_t& aIn, BluetoothGattReadParam& aOut)
{
  nsresult rv;

  rv = Convert(aIn.srvc_id, aOut.mServiceId);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = Convert(aIn.char_id, aOut.mCharId);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = Convert(aIn.descr_id, aOut.mDescriptorId);
  NS_ENSURE_SUCCESS(rv, rv);

  memcpy(aOut.mValue, aIn.value.value, aIn.value.len);
  aOut.mValueLength = aIn.value.len;
  aOut.mValueType = aIn.value_type;
  aOut.mStatus = aIn.status;

  return NS_OK;
}

nsresult
Convert(const btgatt_write_params_t& aIn, BluetoothGattWriteParam& aOut)
{
  nsresult rv;

  rv = Convert(aIn.srvc_id, aOut.mServiceId);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = Convert(aIn.char_id, aOut.mCharId);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = Convert(aIn.descr_id, aOut.mDescriptorId);
  NS_ENSURE_SUCCESS(rv, rv);

  aOut.mStatus = aIn.status;

  return NS_OK;
}

nsresult
Convert(const btgatt_notify_params_t& aIn, BluetoothGattNotifyParam& aOut)
{
  nsresult rv;

  rv = Convert(aIn.bda, aOut.mBdAddr);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = Convert(aIn.srvc_id, aOut.mServiceId);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = Convert(aIn.char_id, aOut.mCharId);
  NS_ENSURE_SUCCESS(rv, rv);

  memcpy(aOut.mValue, aIn.value, aIn.len);
  aOut.mLength = aIn.len;
  aOut.mIsNotify = aIn.is_notify;

  return NS_OK;
}
#endif // ANDROID_VERSION >= 19

nsresult
Convert(const ArrayBuffer& aIn, char* aOut) {
  aIn.ComputeLengthAndData();
  memcpy(aOut, aIn.Data(), aIn.Length());
  return NS_OK;
}

nsresult
Convert(const bt_property_t& aIn, BluetoothProperty& aOut)
{
  /* type conversion */

  nsresult rv = Convert(aIn.type, aOut.mType);
  if (NS_FAILED(rv)) {
    return rv;
  }

  /* value conversion */

  switch (aOut.mType) {
    case PROPERTY_UNKNOWN:
      /* Bug 1065999: working around unknown properties */
      break;
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
                   aOut.mTypeOfDevice);
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

END_BLUETOOTH_NAMESPACE
