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
Convert(int32_t aIn, BluetoothDeviceType& aOut)
{
  static const BluetoothDeviceType sDeviceType[] = {
    CONVERT(0x00, static_cast<BluetoothDeviceType>(0)), // invalid, required by gcc
    CONVERT(0x01, DEVICE_TYPE_BREDR),
    CONVERT(0x02, DEVICE_TYPE_BLE),
    CONVERT(0x03, DEVICE_TYPE_DUAL)
  };
  if (NS_WARN_IF(!aIn) ||
      NS_WARN_IF(static_cast<size_t>(aIn) >= MOZ_ARRAY_LENGTH(sDeviceType))) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sDeviceType[aIn];
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
Convert(uint8_t aIn, BluetoothSspPairingVariant& aOut)
{
  static const BluetoothSspPairingVariant sSspPairingVariant[] = {
    CONVERT(0x00, SSP_VARIANT_PASSKEY_CONFIRMATION),
    CONVERT(0x01, SSP_VARIANT_PASSKEY_ENTRY),
    CONVERT(0x02, SSP_VARIANT_CONSENT),
    CONVERT(0x03, SSP_VARIANT_PASSKEY_NOTIFICATION)
  };
  if (NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sSspPairingVariant))) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = sSspPairingVariant[aIn];
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
Convert(const nsAString& aIn, BluetoothSspPairingVariant& aOut)
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
Convert(BluetoothSspPairingVariant aIn, uint8_t& aOut)
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
Convert(BluetoothSspPairingVariant aIn, nsAString& aOut)
{
  static const char* const sString[] = {
    CONVERT(SSP_VARIANT_PASSKEY_CONFIRMATION, "PasskeyConfirmation"),
    CONVERT(SSP_VARIANT_PASSKEY_ENTRY, "PasskeyEntry"),
    CONVERT(SSP_VARIANT_CONSENT, "Consent"),
    CONVERT(SSP_VARIANT_PASSKEY_NOTIFICATION, "PasskeyNotification")
  };
  if (NS_WARN_IF(aIn >= MOZ_ARRAY_LENGTH(sString))) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = NS_ConvertUTF8toUTF16(sString[aIn]);
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
PackPDU(BluetoothSspPairingVariant aIn, BluetoothDaemonPDU& aPDU)
{
  return PackPDU(PackConversion<BluetoothSspPairingVariant, uint8_t>(aIn),
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

//
// Unpacking
//

nsresult
UnpackPDU(BluetoothDaemonPDU& aPDU, bool& aOut)
{
  return UnpackPDU(aPDU, UnpackConversion<uint8_t, bool>(aOut));
}

nsresult
UnpackPDU(BluetoothDaemonPDU& aPDU, BluetoothAclState& aOut)
{
  return UnpackPDU(aPDU, UnpackConversion<uint8_t, BluetoothAclState>(aOut));
}

nsresult
UnpackPDU(BluetoothDaemonPDU& aPDU, BluetoothBondState& aOut)
{
  return UnpackPDU(aPDU, UnpackConversion<uint8_t, BluetoothBondState>(aOut));
}

nsresult
UnpackPDU(BluetoothDaemonPDU& aPDU, BluetoothDeviceType& aOut)
{
  return UnpackPDU(
    aPDU, UnpackConversion<int32_t, BluetoothDeviceType>(aOut));
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
      rv = UnpackPDU(aPDU, aOut.mDeviceType);
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
UnpackPDU(BluetoothDaemonPDU& aPDU, BluetoothSspPairingVariant& aOut)
{
  return UnpackPDU(
    aPDU, UnpackConversion<uint8_t, BluetoothSspPairingVariant>(aOut));
}

nsresult
UnpackPDU(BluetoothDaemonPDU& aPDU, BluetoothStatus& aOut)
{
  return UnpackPDU(aPDU, UnpackConversion<uint8_t, BluetoothStatus>(aOut));
}

END_BLUETOOTH_NAMESPACE
