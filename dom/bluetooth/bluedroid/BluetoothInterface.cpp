/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include "base/message_loop.h"
#include "BluetoothInterface.h"
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

#if ANDROID_VERSION >= 18
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

#endif

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

BluetoothHandsfreeInterface::BluetoothHandsfreeInterface(
  const bthf_interface_t* aInterface)
: mInterface(aInterface)
{
  MOZ_ASSERT(mInterface);
}

BluetoothHandsfreeInterface::~BluetoothHandsfreeInterface()
{ }

void
BluetoothHandsfreeInterface::Init(bthf_callbacks_t* aCallbacks,
                                  BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status = mInterface->init(aCallbacks);

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

BluetoothA2dpInterface::BluetoothA2dpInterface(
  const btav_interface_t* aInterface)
: mInterface(aInterface)
{
  MOZ_ASSERT(mInterface);
}

BluetoothA2dpInterface::~BluetoothA2dpInterface()
{ }

void
BluetoothA2dpInterface::Init(btav_callbacks_t* aCallbacks,
                             BluetoothA2dpResultHandler* aRes)
{
  bt_status_t status = mInterface->init(aCallbacks);

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

BluetoothAvrcpInterface::BluetoothAvrcpInterface(
  const btrc_interface_t* aInterface)
: mInterface(aInterface)
{
  MOZ_ASSERT(mInterface);
}

BluetoothAvrcpInterface::~BluetoothAvrcpInterface()
{ }

void
BluetoothAvrcpInterface::Init(btrc_callbacks_t* aCallbacks,
                              BluetoothAvrcpResultHandler* aRes)
{
  bt_status_t status = mInterface->init(aCallbacks);

  if (aRes) {
    DispatchBluetoothAvrcpResult(aRes, &BluetoothAvrcpResultHandler::Init,
                                 ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothAvrcpInterface::Cleanup(BluetoothAvrcpResultHandler* aRes)
{
  mInterface->cleanup();

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
  btrc_play_status_t playStatus = BTRC_PLAYSTATE_STOPPED;

  if (!(NS_FAILED(Convert(aPlayStatus, playStatus)))) {
    status = mInterface->get_play_status_rsp(playStatus, aSongLen, aSongPos);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

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
  ConvertArray<BluetoothAvrcpPlayerAttribute> pAttrsArray(aPAttrs, aNumAttr);
  nsAutoArrayPtr<btrc_player_attr_t> pAttrs;

  if (NS_SUCCEEDED(Convert(pAttrsArray, pAttrs))) {
    status = mInterface->list_player_app_attr_rsp(aNumAttr, pAttrs);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

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
  bt_status_t status = mInterface->list_player_app_value_rsp(aNumVal, aPVals);

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
  btrc_player_settings_t pVals;

  /* FIXME: you need to implement the missing conversion functions */
  NS_NOTREACHED("Conversion function missing");

  if (false /* TODO: we don't support any player app values currently */) {
    status = mInterface->get_player_app_value_rsp(&pVals);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

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
  btrc_player_setting_text_t* aPAttrs;

  /* FIXME: you need to implement the missing conversion functions */
  NS_NOTREACHED("Conversion function missing");

  if (false /* TODO: we don't support any attributes currently */) {
    status = mInterface->get_player_app_attr_text_rsp(aNumAttr, aPAttrs);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

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
  btrc_player_setting_text_t* pVals;

  /* FIXME: you need to implement the missing conversion functions */
  NS_NOTREACHED("Conversion function missing");

  if (false /* TODO: we don't support any values currently */) {
    status = mInterface->get_player_app_value_text_rsp(aNumVal, pVals);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

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
  ConvertArray<BluetoothAvrcpElementAttribute> pAttrsArray(aAttrs, aNumAttr);
  nsAutoArrayPtr<btrc_element_attr_val_t> pAttrs;

  if (NS_SUCCEEDED(Convert(pAttrsArray, pAttrs))) {
    status = mInterface->get_element_attr_rsp(aNumAttr, pAttrs);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

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
  btrc_status_t rspStatus = BTRC_STS_BAD_CMD; // silences compiler warning

  if (NS_SUCCEEDED(Convert(aRspStatus, rspStatus))) {
    status = mInterface->set_player_app_value_rsp(rspStatus);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

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
  nsresult rv;
  bt_status_t status;
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
#endif

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
BluetoothInterface::Init(bt_callbacks_t* aCallbacks,
                         BluetoothResultHandler* aRes)
{
  int status = mInterface->init(aCallbacks);

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
