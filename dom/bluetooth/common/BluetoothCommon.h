/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_BluetoothCommon_h
#define mozilla_dom_bluetooth_BluetoothCommon_h

#include "mozilla/Compiler.h"
#include "mozilla/Observer.h"
#include "nsPrintfCString.h"
#include "nsString.h"
#include "nsTArray.h"

extern bool gBluetoothDebugFlag;

#define SWITCH_BT_DEBUG(V) (gBluetoothDebugFlag = V)

#undef BT_LOG
#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>

/**
 * Prints 'D'EBUG build logs, which show in DEBUG build only when
 * developer setting 'Bluetooth output in adb' is enabled.
 */
#define BT_LOGD(msg, ...)                                            \
  do {                                                               \
    if (gBluetoothDebugFlag) {                                       \
      __android_log_print(ANDROID_LOG_INFO, "GeckoBluetooth",        \
                          "%s: " msg, __FUNCTION__, ##__VA_ARGS__);  \
    }                                                                \
  } while(0)

/**
 * Prints 'R'ELEASE build logs, which show in both RELEASE and DEBUG builds.
 */
#define BT_LOGR(msg, ...)                                            \
  __android_log_print(ANDROID_LOG_INFO, "GeckoBluetooth",            \
                      "%s: " msg, __FUNCTION__, ##__VA_ARGS__)

/**
 * Prints DEBUG build warnings, which show in DEBUG build only.
 */
#define BT_WARNING(args...)                                          \
  NS_WARNING(nsPrintfCString(args).get())

#else
#define BT_LOGD(msg, ...)                                            \
  do {                                                               \
    if (gBluetoothDebugFlag) {                                       \
      printf("%s: " msg, __FUNCTION__, ##__VA_ARGS__);               \
    }                                                                \
  } while(0)

#define BT_LOGR(msg, ...) printf("%s: " msg, __FUNCTION__, ##__VA_ARGS__)
#define BT_WARNING(msg, ...) printf("%s: " msg, __FUNCTION__, ##__VA_ARGS__)
#endif

/**
 * Convert an enum value to string and append it to a fallible array.
 */
#define BT_APPEND_ENUM_STRING_FALLIBLE(array, enumType, enumValue)   \
  do {                                                               \
    uint32_t index = uint32_t(enumValue);                            \
    nsAutoString name;                                               \
    name.AssignASCII(enumType##Values::strings[index].value,         \
                     enumType##Values::strings[index].length);       \
    array.AppendElement(name, mozilla::fallible);                    \
  } while(0)

/**
 * Ensure success of system message broadcast with void return.
 */
#define BT_ENSURE_TRUE_VOID_BROADCAST_SYSMSG(type, parameters)       \
  do {                                                               \
    if (!BroadcastSystemMessage(type, parameters)) {                 \
      BT_WARNING("Failed to broadcast [%s]",                         \
                 NS_ConvertUTF16toUTF8(type).get());                 \
      return;                                                        \
    }                                                                \
  } while(0)

/**
 * Resolve |promise| with |ret| if |x| is false.
 */
#define BT_ENSURE_TRUE_RESOLVE(x, promise, ret)                      \
  do {                                                               \
    if (MOZ_UNLIKELY(!(x))) {                                        \
      BT_LOGR("BT_ENSURE_TRUE_RESOLVE(" #x ") failed");              \
      (promise)->MaybeResolve(ret);                                  \
      return (promise).forget();                                     \
    }                                                                \
  } while(0)

/**
 * Reject |promise| with |ret| if |x| is false.
 */
#define BT_ENSURE_TRUE_REJECT(x, promise, ret)                       \
  do {                                                               \
    if (MOZ_UNLIKELY(!(x))) {                                        \
      BT_LOGR("BT_ENSURE_TRUE_REJECT(" #x ") failed");               \
      (promise)->MaybeReject(ret);                                   \
      return (promise).forget();                                     \
    }                                                                \
  } while(0)

/**
 * Reject |promise| with |ret| if nsresult |rv| is not successful.
 */
#define BT_ENSURE_SUCCESS_REJECT(rv, promise, ret)                   \
  BT_ENSURE_TRUE_REJECT(NS_SUCCEEDED(rv), promise, ret)

/**
 * Resolve |promise| with |value| and return |ret| if |x| is false.
 */
#define BT_ENSURE_TRUE_RESOLVE_RETURN(x, promise, value, ret)        \
  do {                                                               \
    if (MOZ_UNLIKELY(!(x))) {                                        \
      BT_LOGR("BT_ENSURE_TRUE_RESOLVE_RETURN(" #x ") failed");       \
      (promise)->MaybeResolve(value);                                \
      return ret;                                                    \
    }                                                                \
  } while(0)

/**
 * Resolve |promise| with |value| and return if |x| is false.
 */
#define BT_ENSURE_TRUE_RESOLVE_VOID(x, promise, value)               \
  do {                                                               \
    if (MOZ_UNLIKELY(!(x))) {                                        \
      BT_LOGR("BT_ENSURE_TRUE_RESOLVE_VOID(" #x ") failed");         \
      (promise)->MaybeResolve(value);                                \
      return;                                                        \
    }                                                                \
  } while(0)

/**
 * Reject |promise| with |value| and return |ret| if |x| is false.
 */
#define BT_ENSURE_TRUE_REJECT_RETURN(x, promise, value, ret)         \
  do {                                                               \
    if (MOZ_UNLIKELY(!(x))) {                                        \
      BT_LOGR("BT_ENSURE_TRUE_REJECT_RETURN(" #x ") failed");        \
      (promise)->MaybeReject(value);                                 \
      return ret;                                                    \
    }                                                                \
  } while(0)

/**
 * Reject |promise| with |value| and return if |x| is false.
 */
#define BT_ENSURE_TRUE_REJECT_VOID(x, promise, value)                \
  do {                                                               \
    if (MOZ_UNLIKELY(!(x))) {                                        \
      BT_LOGR("BT_ENSURE_TRUE_REJECT_VOID(" #x ") failed");          \
      (promise)->MaybeReject(value);                                 \
      return;                                                        \
    }                                                                \
  } while(0)

/**
 * Reject |promise| with |value| and return |ret| if nsresult |rv|
 * is not successful.
 */
#define BT_ENSURE_SUCCESS_REJECT_RETURN(rv, promise, value, ret)     \
  BT_ENSURE_TRUE_REJECT_RETURN(NS_SUCCEEDED(rv), promise, value, ret)

/**
 * Reject |promise| with |value| and return if nsresult |rv|
 * is not successful.
 */
#define BT_ENSURE_SUCCESS_REJECT_VOID(rv, promise, value)            \
  BT_ENSURE_TRUE_REJECT_VOID(NS_SUCCEEDED(rv), promise, value)

#define BEGIN_BLUETOOTH_NAMESPACE \
  namespace mozilla { namespace dom { namespace bluetooth {
#define END_BLUETOOTH_NAMESPACE \
  } /* namespace bluetooth */ } /* namespace dom */ } /* namespace mozilla */
#define USING_BLUETOOTH_NAMESPACE \
  using namespace mozilla::dom::bluetooth;

#define KEY_LOCAL_AGENT       "/B2G/bluetooth/agent"
#define KEY_REMOTE_AGENT      "/B2G/bluetooth/remote_device_agent"
#define KEY_MANAGER           "/B2G/bluetooth/manager"
#define KEY_ADAPTER           "/B2G/bluetooth/adapter"
#define KEY_PAIRING_LISTENER  "/B2G/bluetooth/pairing_listener"

/**
 * When the connection status of a Bluetooth profile is changed, we'll notify
 * observers which register the following topics.
 */
#define BLUETOOTH_A2DP_STATUS_CHANGED_ID "bluetooth-a2dp-status-changed"
#define BLUETOOTH_HFP_STATUS_CHANGED_ID  "bluetooth-hfp-status-changed"
#define BLUETOOTH_HFP_NREC_STATUS_CHANGED_ID  "bluetooth-hfp-nrec-status-changed"
#define BLUETOOTH_HID_STATUS_CHANGED_ID  "bluetooth-hid-status-changed"
#define BLUETOOTH_SCO_STATUS_CHANGED_ID  "bluetooth-sco-status-changed"

/**
 * When the connection status of a Bluetooth profile is changed, we'll
 * dispatch one of the following events.
 */
#define A2DP_STATUS_CHANGED_ID               "a2dpstatuschanged"
#define HFP_STATUS_CHANGED_ID                "hfpstatuschanged"
#define SCO_STATUS_CHANGED_ID                "scostatuschanged"

/**
 * Types of pairing requests for constructing BluetoothPairingEvent and
 * BluetoothPairingHandle.
 */
#define PAIRING_REQ_TYPE_DISPLAYPASSKEY       "displaypasskeyreq"
#define PAIRING_REQ_TYPE_ENTERPINCODE         "enterpincodereq"
#define PAIRING_REQ_TYPE_CONFIRMATION         "pairingconfirmationreq"
#define PAIRING_REQ_TYPE_CONSENT              "pairingconsentreq"

/**
 * System message to launch bluetooth app if no pairing listener is ready to
 * receive pairing requests.
 */
#define SYS_MSG_BT_PAIRING_REQ                "bluetooth-pairing-request"

/**
 * The preference name of bluetooth app origin of bluetooth app. The default
 * value is defined in b2g/app/b2g.js.
 */
#define PREF_BLUETOOTH_APP_ORIGIN             "dom.bluetooth.app-origin"

/**
 * When a remote device gets paired / unpaired with local bluetooth adapter or
 * pairing process is aborted, we'll dispatch an event.
 */
#define DEVICE_PAIRED_ID                     "devicepaired"
#define DEVICE_UNPAIRED_ID                   "deviceunpaired"
#define PAIRING_ABORTED_ID                   "pairingaborted"

/**
 * When receiving a query about current play status from remote device, we'll
 * dispatch an event.
 */
#define REQUEST_MEDIA_PLAYSTATUS_ID          "requestmediaplaystatus"

/**
 * When receiving a PBAP request from a remote device, we'll dispatch an event.
 */
#define PULL_PHONEBOOK_REQ_ID                "pullphonebookreq"
#define PULL_VCARD_ENTRY_REQ_ID              "pullvcardentryreq"
#define PULL_VCARD_LISTING_REQ_ID            "pullvcardlistingreq"

/**
 * When receiving a MAP request from a remote device,
 * we'll dispatch an event.
 */
#define MAP_MESSAGES_LISTING_REQ_ID          "mapmessageslistingreq"
#define MAP_GET_MESSAGE_REQ_ID               "mapgetmessagereq"
#define MAP_SET_MESSAGE_STATUS_REQ_ID        "mapsetmessagestatusreq"
#define MAP_PUSH_MESSAGE_REQ_ID              "mappushmessagereq"

/**
 * When the value of a characteristic of a remote BLE device changes, we'll
 * dispatch an event
 */
#define GATT_CHARACTERISTIC_CHANGED_ID       "characteristicchanged"

/**
 * When a remote BLE device gets connected / disconnected, we'll dispatch an
 * event.
 */
#define GATT_CONNECTION_STATE_CHANGED_ID     "connectionstatechanged"

/**
 * When attributes of BluetoothManager, BluetoothAdapter, or BluetoothDevice
 * are changed, we'll dispatch an event.
 */
#define ATTRIBUTE_CHANGED_ID                 "attributechanged"

/**
 * When the local GATT server received attribute read/write requests, we'll
 * dispatch an event.
 */
#define ATTRIBUTE_READ_REQUEST               "attributereadreq"
#define ATTRIBUTE_WRITE_REQUEST              "attributewritereq"

// Bluetooth address format: xx:xx:xx:xx:xx:xx (or xx_xx_xx_xx_xx_xx)
#define BLUETOOTH_ADDRESS_LENGTH 17
#define BLUETOOTH_ADDRESS_NONE   "00:00:00:00:00:00"
#define BLUETOOTH_ADDRESS_BYTES  6

// Bluetooth stack internal error, such as I/O error
#define ERR_INTERNAL_ERROR "InternalError"

/**
 * BT specification v4.1 defines the maximum attribute length as 512 octets.
 * Currently use 600 here to conform to bluedroid's BTGATT_MAX_ATTR_LEN.
 */
#define BLUETOOTH_GATT_MAX_ATTR_LEN 600

BEGIN_BLUETOOTH_NAMESPACE

enum BluetoothStatus {
  STATUS_SUCCESS,
  STATUS_FAIL,
  STATUS_NOT_READY,
  STATUS_NOMEM,
  STATUS_BUSY,
  STATUS_DONE,
  STATUS_UNSUPPORTED,
  STATUS_PARM_INVALID,
  STATUS_UNHANDLED,
  STATUS_AUTH_FAILURE,
  STATUS_RMT_DEV_DOWN,
  NUM_STATUS
};

enum BluetoothBondState {
  BOND_STATE_NONE,
  BOND_STATE_BONDING,
  BOND_STATE_BONDED
};

/* Physical transport for GATT connections to remote dual-mode devices */
enum BluetoothTransport {
  TRANSPORT_AUTO,   /* No preference of physical transport */
  TRANSPORT_BREDR,  /* Prefer BR/EDR transport */
  TRANSPORT_LE      /* Prefer LE transport */
};

enum BluetoothTypeOfDevice {
  TYPE_OF_DEVICE_BREDR,
  TYPE_OF_DEVICE_BLE,
  TYPE_OF_DEVICE_DUAL
};

enum BluetoothPropertyType {
  PROPERTY_UNKNOWN,
  PROPERTY_BDNAME,
  PROPERTY_BDADDR,
  PROPERTY_UUIDS,
  PROPERTY_CLASS_OF_DEVICE,
  PROPERTY_TYPE_OF_DEVICE,
  PROPERTY_SERVICE_RECORD,
  PROPERTY_ADAPTER_SCAN_MODE,
  PROPERTY_ADAPTER_BONDED_DEVICES,
  PROPERTY_ADAPTER_DISCOVERY_TIMEOUT,
  PROPERTY_REMOTE_FRIENDLY_NAME,
  PROPERTY_REMOTE_RSSI,
  PROPERTY_REMOTE_VERSION_INFO,
  PROPERTY_REMOTE_DEVICE_TIMESTAMP
};

enum BluetoothScanMode {
  SCAN_MODE_NONE,
  SCAN_MODE_CONNECTABLE,
  SCAN_MODE_CONNECTABLE_DISCOVERABLE
};

enum BluetoothSspVariant {
  SSP_VARIANT_PASSKEY_CONFIRMATION,
  SSP_VARIANT_PASSKEY_ENTRY,
  SSP_VARIANT_CONSENT,
  SSP_VARIANT_PASSKEY_NOTIFICATION,
  NUM_SSP_VARIANT
};

struct BluetoothActivityEnergyInfo {
  uint8_t mStatus;
  uint8_t mStackState;  /* stack reported state */
  uint64_t mTxTime;     /* in ms */
  uint64_t mRxTime;     /* in ms */
  uint64_t mIdleTime;   /* in ms */
  uint64_t mEnergyUsed; /* a product of mA, V and ms */
};

struct BluetoothUuid {
  uint8_t mUuid[16];

  bool operator==(const BluetoothUuid& aOther) const
  {
    for (uint8_t i = 0; i < sizeof(mUuid); i++) {
      if (mUuid[i] != aOther.mUuid[i]) {
        return false;
      }
    }
    return true;
  }

  bool operator!=(const BluetoothUuid& aOther) const
  {
    return !(*this == aOther);
  }
};

struct BluetoothServiceRecord {
  BluetoothUuid mUuid;
  uint16_t mChannel;
  char mName[256];
};

struct BluetoothRemoteInfo {
  int mVerMajor;
  int mVerMinor;
  int mManufacturer;
};

struct BluetoothProperty {
  /* Type */
  BluetoothPropertyType mType;

  /* Value
   */

  /* PROPERTY_BDNAME
     PROPERTY_BDADDR
     PROPERTY_REMOTE_FRIENDLY_NAME */
  nsString mString;

  /* PROPERTY_UUIDS */
  nsTArray<BluetoothUuid> mUuidArray;

  /* PROPERTY_ADAPTER_BONDED_DEVICES */
  nsTArray<nsString> mStringArray;

  /* PROPERTY_CLASS_OF_DEVICE
     PROPERTY_ADAPTER_DISCOVERY_TIMEOUT */
  uint32_t mUint32;

  /* PROPERTY_RSSI_VALUE */
  int32_t mInt32;

  /* PROPERTY_TYPE_OF_DEVICE */
  BluetoothTypeOfDevice mTypeOfDevice;

  /* PROPERTY_SERVICE_RECORD */
  BluetoothServiceRecord mServiceRecord;

  /* PROPERTY_SCAN_MODE */
  BluetoothScanMode mScanMode;

  /* PROPERTY_REMOTE_VERSION_INFO */
  BluetoothRemoteInfo mRemoteInfo;
};

enum BluetoothSocketType {
  RFCOMM = 1,
  SCO    = 2,
  L2CAP  = 3,
  EL2CAP = 4
};

enum BluetoothHandsfreeAtResponse {
  HFP_AT_RESPONSE_ERROR,
  HFP_AT_RESPONSE_OK
};

enum BluetoothHandsfreeAudioState {
  HFP_AUDIO_STATE_DISCONNECTED,
  HFP_AUDIO_STATE_CONNECTING,
  HFP_AUDIO_STATE_CONNECTED,
  HFP_AUDIO_STATE_DISCONNECTING,
};

enum BluetoothHandsfreeCallAddressType {
  HFP_CALL_ADDRESS_TYPE_UNKNOWN,
  HFP_CALL_ADDRESS_TYPE_INTERNATIONAL
};

enum BluetoothHandsfreeCallDirection {
  HFP_CALL_DIRECTION_OUTGOING,
  HFP_CALL_DIRECTION_INCOMING
};

enum BluetoothHandsfreeCallHoldType {
  HFP_CALL_HOLD_RELEASEHELD,
  HFP_CALL_HOLD_RELEASEACTIVE_ACCEPTHELD,
  HFP_CALL_HOLD_HOLDACTIVE_ACCEPTHELD,
  HFP_CALL_HOLD_ADDHELDTOCONF
};

enum BluetoothHandsfreeCallMode {
  HFP_CALL_MODE_VOICE,
  HFP_CALL_MODE_DATA,
  HFP_CALL_MODE_FAX
};

enum BluetoothHandsfreeCallMptyType {
  HFP_CALL_MPTY_TYPE_SINGLE,
  HFP_CALL_MPTY_TYPE_MULTI
};

enum BluetoothHandsfreeCallState {
  HFP_CALL_STATE_ACTIVE,
  HFP_CALL_STATE_HELD,
  HFP_CALL_STATE_DIALING,
  HFP_CALL_STATE_ALERTING,
  HFP_CALL_STATE_INCOMING,
  HFP_CALL_STATE_WAITING,
  HFP_CALL_STATE_IDLE
};

enum BluetoothHandsfreeConnectionState
{
  HFP_CONNECTION_STATE_DISCONNECTED,
  HFP_CONNECTION_STATE_CONNECTING,
  HFP_CONNECTION_STATE_CONNECTED,
  HFP_CONNECTION_STATE_SLC_CONNECTED,
  HFP_CONNECTION_STATE_DISCONNECTING
};

enum BluetoothHandsfreeNetworkState {
  HFP_NETWORK_STATE_NOT_AVAILABLE,
  HFP_NETWORK_STATE_AVAILABLE
};

enum BluetoothHandsfreeNRECState {
  HFP_NREC_STOPPED,
  HFP_NREC_STARTED
};

enum BluetoothHandsfreeServiceType {
  HFP_SERVICE_TYPE_HOME,
  HFP_SERVICE_TYPE_ROAMING
};

enum BluetoothHandsfreeVoiceRecognitionState {
  HFP_VOICE_RECOGNITION_STOPPED,
  HFP_VOICE_RECOGNITION_STARTED
};

enum BluetoothHandsfreeVolumeType {
  HFP_VOLUME_TYPE_SPEAKER,
  HFP_VOLUME_TYPE_MICROPHONE
};

enum BluetoothHandsfreeWbsConfig {
  HFP_WBS_NONE, /* Neither CVSD nor mSBC codec, but other optional codec.*/
  HFP_WBS_NO,   /* CVSD */
  HFP_WBS_YES   /* mSBC */
};

class BluetoothSignal;

class BluetoothSignalObserver : public mozilla::Observer<BluetoothSignal>
{
public:
  BluetoothSignalObserver() : mSignalRegistered(false)
  { }

  void SetSignalRegistered(bool aSignalRegistered)
  {
    mSignalRegistered = aSignalRegistered;
  }

protected:
  bool mSignalRegistered;
};

// Enums for object types, currently used for shared function lookups
// (get/setproperty, etc...). Possibly discernable via dbus paths, but this
// method is future-proofed for platform independence.
enum BluetoothObjectType {
  TYPE_MANAGER = 0,
  TYPE_ADAPTER = 1,
  TYPE_DEVICE = 2,
  NUM_TYPE
};

enum BluetoothA2dpAudioState {
  A2DP_AUDIO_STATE_REMOTE_SUSPEND,
  A2DP_AUDIO_STATE_STOPPED,
  A2DP_AUDIO_STATE_STARTED,
};

enum BluetoothA2dpConnectionState {
  A2DP_CONNECTION_STATE_DISCONNECTED,
  A2DP_CONNECTION_STATE_CONNECTING,
  A2DP_CONNECTION_STATE_CONNECTED,
  A2DP_CONNECTION_STATE_DISCONNECTING
};

enum ControlPlayStatus {
  PLAYSTATUS_STOPPED  = 0x00,
  PLAYSTATUS_PLAYING  = 0x01,
  PLAYSTATUS_PAUSED   = 0x02,
  PLAYSTATUS_FWD_SEEK = 0x03,
  PLAYSTATUS_REV_SEEK = 0x04,
  PLAYSTATUS_UNKNOWN,
  PLAYSTATUS_ERROR    = 0xFF,
};

enum {
  AVRCP_UID_SIZE = 8
};

enum BluetoothAvrcpMediaAttribute {
  AVRCP_MEDIA_ATTRIBUTE_TITLE = 0x01,
  AVRCP_MEDIA_ATTRIBUTE_ARTIST = 0x02,
  AVRCP_MEDIA_ATTRIBUTE_ALBUM = 0x03,
  AVRCP_MEDIA_ATTRIBUTE_TRACK_NUM = 0x04,
  AVRCP_MEDIA_ATTRIBUTE_NUM_TRACKS = 0x05,
  AVRCP_MEDIA_ATTRIBUTE_GENRE = 0x06,
  AVRCP_MEDIA_ATTRIBUTE_PLAYING_TIME = 0x07
};

enum BluetoothAvrcpPlayerAttribute {
  AVRCP_PLAYER_ATTRIBUTE_EQUALIZER,
  AVRCP_PLAYER_ATTRIBUTE_REPEAT,
  AVRCP_PLAYER_ATTRIBUTE_SHUFFLE,
  AVRCP_PLAYER_ATTRIBUTE_SCAN
};

enum BluetoothAvrcpPlayerRepeatValue {
  AVRCP_PLAYER_VAL_OFF_REPEAT = 0x01,
  AVRCP_PLAYER_VAL_SINGLE_REPEAT = 0x02,
  AVRCP_PLAYER_VAL_ALL_REPEAT = 0x03,
  AVRCP_PLAYER_VAL_GROUP_REPEAT = 0x04
};

enum BluetoothAvrcpPlayerShuffleValue {
  AVRCP_PLAYER_VAL_OFF_SHUFFLE = 0x01,
  AVRCP_PLAYER_VAL_ALL_SHUFFLE = 0x02,
  AVRCP_PLAYER_VAL_GROUP_SHUFFLE = 0x03
};

enum BluetoothAvrcpStatus {
  AVRCP_STATUS_BAD_COMMAND,
  AVRCP_STATUS_BAD_PARAMETER,
  AVRCP_STATUS_NOT_FOUND,
  AVRCP_STATUS_INTERNAL_ERROR,
  AVRCP_STATUS_SUCCESS
};

enum BluetoothAvrcpEvent {
  AVRCP_EVENT_PLAY_STATUS_CHANGED,
  AVRCP_EVENT_TRACK_CHANGE,
  AVRCP_EVENT_TRACK_REACHED_END,
  AVRCP_EVENT_TRACK_REACHED_START,
  AVRCP_EVENT_PLAY_POS_CHANGED,
  AVRCP_EVENT_APP_SETTINGS_CHANGED
};

enum BluetoothAvrcpNotification {
  AVRCP_NTF_INTERIM,
  AVRCP_NTF_CHANGED
};

enum BluetoothAvrcpRemoteFeatureBits {
  AVRCP_REMOTE_FEATURE_NONE,
  AVRCP_REMOTE_FEATURE_METADATA = 0x01,
  AVRCP_REMOTE_FEATURE_ABSOLUTE_VOLUME = 0x02,
  AVRCP_REMOTE_FEATURE_BROWSE = 0x04
};

struct BluetoothAvrcpElementAttribute {
  uint32_t mId;
  nsString mValue;
};

struct BluetoothAvrcpNotificationParam {
  ControlPlayStatus mPlayStatus;
  uint8_t mTrack[8];
  uint32_t mSongPos;
  uint8_t mNumAttr;
  uint8_t mIds[256];
  uint8_t mValues[256];
};

struct BluetoothAvrcpPlayerSettings {
  uint8_t mNumAttr;
  uint8_t mIds[256];
  uint8_t mValues[256];
};

enum BluetoothAttRole {
  ATT_SERVER_ROLE,
  ATT_CLIENT_ROLE
};

enum BluetoothGattStatus {
  GATT_STATUS_SUCCESS,
  GATT_STATUS_INVALID_HANDLE,
  GATT_STATUS_READ_NOT_PERMITTED,
  GATT_STATUS_WRITE_NOT_PERMITTED,
  GATT_STATUS_INVALID_PDU,
  GATT_STATUS_INSUFFICIENT_AUTHENTICATION,
  GATT_STATUS_REQUEST_NOT_SUPPORTED,
  GATT_STATUS_INVALID_OFFSET,
  GATT_STATUS_INSUFFICIENT_AUTHORIZATION,
  GATT_STATUS_PREPARE_QUEUE_FULL,
  GATT_STATUS_ATTRIBUTE_NOT_FOUND,
  GATT_STATUS_ATTRIBUTE_NOT_LONG,
  GATT_STATUS_INSUFFICIENT_ENCRYPTION_KEY_SIZE,
  GATT_STATUS_INVALID_ATTRIBUTE_LENGTH,
  GATT_STATUS_UNLIKELY_ERROR,
  GATT_STATUS_INSUFFICIENT_ENCRYPTION,
  GATT_STATUS_UNSUPPORTED_GROUP_TYPE,
  GATT_STATUS_INSUFFICIENT_RESOURCES,
  GATT_STATUS_UNKNOWN_ERROR
};

enum BluetoothGattAuthReq {
  GATT_AUTH_REQ_NONE,
  GATT_AUTH_REQ_NO_MITM,
  GATT_AUTH_REQ_MITM,
  GATT_AUTH_REQ_SIGNED_NO_MITM,
  GATT_AUTH_REQ_SIGNED_MITM,
  GATT_AUTH_REQ_END_GUARD
};

enum BluetoothGattWriteType {
  GATT_WRITE_TYPE_NO_RESPONSE,
  GATT_WRITE_TYPE_NORMAL,
  GATT_WRITE_TYPE_PREPARE,
  GATT_WRITE_TYPE_SIGNED,
  GATT_WRITE_TYPE_END_GUARD
};

/*
 * Bluetooth GATT Characteristic Properties bit field
 */
enum BluetoothGattCharPropBit {
  GATT_CHAR_PROP_BIT_BROADCAST            = (1 << 0),
  GATT_CHAR_PROP_BIT_READ                 = (1 << 1),
  GATT_CHAR_PROP_BIT_WRITE_NO_RESPONSE    = (1 << 2),
  GATT_CHAR_PROP_BIT_WRITE                = (1 << 3),
  GATT_CHAR_PROP_BIT_NOTIFY               = (1 << 4),
  GATT_CHAR_PROP_BIT_INDICATE             = (1 << 5),
  GATT_CHAR_PROP_BIT_SIGNED_WRITE         = (1 << 6),
  GATT_CHAR_PROP_BIT_EXTENDED_PROPERTIES  = (1 << 7)
};

/*
 * BluetoothGattCharProp is used to store a bit mask value which contains
 * each corresponding bit value of each BluetoothGattCharPropBit.
 */
typedef uint8_t BluetoothGattCharProp;
#define BLUETOOTH_EMPTY_GATT_CHAR_PROP  static_cast<BluetoothGattCharProp>(0x00)

/*
 * Bluetooth GATT Attribute Permissions bit field
 */
enum BluetoothGattAttrPermBit {
  GATT_ATTR_PERM_BIT_READ                 = (1 << 0),
  GATT_ATTR_PERM_BIT_READ_ENCRYPTED       = (1 << 1),
  GATT_ATTR_PERM_BIT_READ_ENCRYPTED_MITM  = (1 << 2),
  GATT_ATTR_PERM_BIT_WRITE                = (1 << 4),
  GATT_ATTR_PERM_BIT_WRITE_ENCRYPTED      = (1 << 5),
  GATT_ATTR_PERM_BIT_WRITE_ENCRYPTED_MITM = (1 << 6),
  GATT_ATTR_PERM_BIT_WRITE_SIGNED         = (1 << 7),
  GATT_ATTR_PERM_BIT_WRITE_SIGNED_MITM    = (1 << 8)
};

/*
 * BluetoothGattAttrPerm is used to store a bit mask value which contains
 * each corresponding bit value of each BluetoothGattAttrPermBit.
 */
typedef int32_t BluetoothGattAttrPerm;
#define BLUETOOTH_EMPTY_GATT_ATTR_PERM  static_cast<BluetoothGattAttrPerm>(0x00)

struct BluetoothGattAdvData {
  uint8_t mAdvData[62];
};

struct BluetoothGattId {
  BluetoothUuid mUuid;
  uint8_t mInstanceId;

  bool operator==(const BluetoothGattId& aOther) const
  {
    return mUuid == aOther.mUuid && mInstanceId == aOther.mInstanceId;
  }
};

struct BluetoothGattServiceId {
  BluetoothGattId mId;
  uint8_t mIsPrimary;

  bool operator==(const BluetoothGattServiceId& aOther) const
  {
    return mId == aOther.mId && mIsPrimary == aOther.mIsPrimary;
  }
};

struct BluetoothGattCharAttribute {
  BluetoothGattId mId;
  BluetoothGattCharProp mProperties;
  BluetoothGattWriteType mWriteType;

  bool operator==(const BluetoothGattCharAttribute& aOther) const
  {
    return mId == aOther.mId &&
           mProperties == aOther.mProperties &&
           mWriteType == aOther.mWriteType;
  }
};

struct BluetoothGattReadParam {
  BluetoothGattServiceId mServiceId;
  BluetoothGattId mCharId;
  BluetoothGattId mDescriptorId;
  uint16_t mValueType;
  uint16_t mValueLength;
  uint8_t mValue[BLUETOOTH_GATT_MAX_ATTR_LEN];
  uint8_t mStatus;
};

struct BluetoothGattWriteParam {
  BluetoothGattServiceId mServiceId;
  BluetoothGattId mCharId;
  BluetoothGattId mDescriptorId;
  uint8_t mStatus;
};

struct BluetoothGattNotifyParam {
  nsString mBdAddr;
  BluetoothGattServiceId mServiceId;
  BluetoothGattId mCharId;
  uint16_t mLength;
  uint8_t mValue[BLUETOOTH_GATT_MAX_ATTR_LEN];
  bool mIsNotify;
};

struct BluetoothGattTestParam {
  nsString mBdAddr;
  BluetoothUuid mUuid;
  uint16_t mU1;
  uint16_t mU2;
  uint16_t mU3;
  uint16_t mU4;
  uint16_t mU5;
};

struct BluetoothAttributeHandle {
  uint16_t mHandle;

  BluetoothAttributeHandle()
    : mHandle(0x0000)
  { }

  bool operator==(const BluetoothAttributeHandle& aOther) const
  {
    return mHandle == aOther.mHandle;
  }
};

struct BluetoothGattResponse {
  BluetoothAttributeHandle mHandle;
  uint16_t mOffset;
  uint16_t mLength;
  BluetoothGattAuthReq mAuthReq;
  uint8_t mValue[BLUETOOTH_GATT_MAX_ATTR_LEN];

  bool operator==(const BluetoothGattResponse& aOther) const
  {
    return mHandle == aOther.mHandle &&
           mOffset == aOther.mOffset &&
           mLength == aOther.mLength &&
           mAuthReq == aOther.mAuthReq &&
           !memcmp(mValue, aOther.mValue, mLength);
  }
};

/**
 * EIR Data Type, Advertising Data Type (AD Type) and OOB Data Type Definitions
 * Please refer to https://www.bluetooth.org/en-us/specification/\
 * assigned-numbers/generic-access-profile
 */
enum BluetoothGapDataType {
  GAP_INCOMPLETE_UUID16  = 0X02, // Incomplete List of 16-bit Service Class UUIDs
  GAP_COMPLETE_UUID16    = 0X03, // Complete List of 16-bit Service Class UUIDs
  GAP_INCOMPLETE_UUID32  = 0X04, // Incomplete List of 32-bit Service Class UUIDs
  GAP_COMPLETE_UUID32    = 0X05, // Complete List of 32-bit Service Class UUIDs
  GAP_INCOMPLETE_UUID128 = 0X06, // Incomplete List of 128-bit Service Class UUIDs
  GAP_COMPLETE_UUID128   = 0X07, // Complete List of 128-bit Service Class UUIDs
  GAP_SHORTENED_NAME     = 0X08, // Shortened Local Name
  GAP_COMPLETE_NAME      = 0X09, // Complete Local Name
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_BluetoothCommon_h
