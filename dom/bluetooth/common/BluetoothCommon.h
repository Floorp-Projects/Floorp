/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_BluetoothCommon_h
#define mozilla_dom_bluetooth_BluetoothCommon_h

#include <algorithm>
#include "mozilla/Compiler.h"
#include "mozilla/Endian.h"
#include "mozilla/Observer.h"
#include "nsAutoPtr.h"
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
 * When receiving an OBEX authenticate challenge request from a remote device,
 * we'll dispatch an event.
 */
#define OBEX_PASSWORD_REQ_ID                 "obexpasswordreq"

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
#define MAP_FOLDER_LISTING_REQ_ID            "mapfolderlistingreq"
#define MAP_MESSAGE_UPDATE_REQ_ID            "mapmessageupdatereq"

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

enum BluetoothAclState {
  ACL_STATE_CONNECTED,
  ACL_STATE_DISCONNECTED
};

enum BluetoothBondState {
  BOND_STATE_NONE,
  BOND_STATE_BONDING,
  BOND_STATE_BONDED
};

enum BluetoothSetupServiceId {
  SETUP_SERVICE_ID_SETUP,
  SETUP_SERVICE_ID_CORE,
  SETUP_SERVICE_ID_SOCKET,
  SETUP_SERVICE_ID_HID,
  SETUP_SERVICE_ID_PAN,
  SETUP_SERVICE_ID_HANDSFREE,
  SETUP_SERVICE_ID_A2DP,
  SETUP_SERVICE_ID_HEALTH,
  SETUP_SERVICE_ID_AVRCP,
  SETUP_SERVICE_ID_GATT,
  SETUP_SERVICE_ID_HANDSFREE_CLIENT,
  SETUP_SERVICE_ID_MAP_CLIENT,
  SETUP_SERVICE_ID_AVRCP_CONTROLLER,
  SETUP_SERVICE_ID_A2DP_SINK
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

/**
 * |BluetoothAddress| stores the 6-byte MAC address of a Bluetooth
 * device. The constants ANY, ALL and LOCAL represent addresses with
 * special meaning.
 */
struct BluetoothAddress {

  static const BluetoothAddress ANY;
  static const BluetoothAddress ALL;
  static const BluetoothAddress LOCAL;

  uint8_t mAddr[6];

  BluetoothAddress()
  {
    Clear(); // assign ANY
  }

  MOZ_IMPLICIT BluetoothAddress(const BluetoothAddress&) = default;

  BluetoothAddress(uint8_t aAddr0, uint8_t aAddr1,
                   uint8_t aAddr2, uint8_t aAddr3,
                   uint8_t aAddr4, uint8_t aAddr5)
  {
    mAddr[0] = aAddr0;
    mAddr[1] = aAddr1;
    mAddr[2] = aAddr2;
    mAddr[3] = aAddr3;
    mAddr[4] = aAddr4;
    mAddr[5] = aAddr5;
  }

  BluetoothAddress& operator=(const BluetoothAddress&) = default;

  bool operator==(const BluetoothAddress& aRhs) const
  {
    return !memcmp(mAddr, aRhs.mAddr, sizeof(mAddr));
  }

  bool operator!=(const BluetoothAddress& aRhs) const
  {
    return !operator==(aRhs);
  }

  /**
   * |Clear| assigns an invalid value (i.e., ANY) to the address.
   */
  void Clear()
  {
    operator=(ANY);
  }

  /**
   * |IsCleared| returns true if the address doesn not contain a
   * specific value (i.e., it contains ANY).
   */
  bool IsCleared() const
  {
    return operator==(ANY);
  }

  /*
   * Getter and setter methods for the address parts. The figure
   * below illustrates the mapping to bytes; from LSB to MSB.
   *
   *    |       LAP       | UAP |    NAP    |
   *    |  0  |  1  |  2  |  3  |  4  |  5  |
   *
   * See Bluetooth Core Spec 2.1, Sec 1.2.
   */

  uint32_t GetLAP() const
  {
    return (static_cast<uint32_t>(mAddr[0])) |
           (static_cast<uint32_t>(mAddr[1]) << 8) |
           (static_cast<uint32_t>(mAddr[2]) << 16);
  }

  void SetLAP(uint32_t aLAP)
  {
    MOZ_ASSERT(!(aLAP & 0xff000000)); // no top-8 bytes in LAP

    mAddr[0] = aLAP;
    mAddr[1] = aLAP >> 8;
    mAddr[2] = aLAP >> 16;
  }

  uint8_t GetUAP() const
  {
    return mAddr[3];
  }

  void SetUAP(uint8_t aUAP)
  {
    mAddr[3] = aUAP;
  }

  uint16_t GetNAP() const
  {
    return LittleEndian::readUint16(&mAddr[4]);
  }

  void SetNAP(uint16_t aNAP)
  {
    LittleEndian::writeUint16(&mAddr[4], aNAP);
  }

};

struct BluetoothConfigurationParameter {
  uint8_t mType;
  uint16_t mLength;
  nsAutoArrayPtr<uint8_t> mValue;
};

/*
 * Service classes and Profile Identifiers
 *
 * Supported Bluetooth services for v1 are listed as below.
 *
 * The value of each service class is defined in "AssignedNumbers/Service
 * Discovery Protocol (SDP)/Service classes and Profile Identifiers" in the
 * Bluetooth Core Specification.
 */
enum BluetoothServiceClass {
  UNKNOWN          = 0x0000,
  OBJECT_PUSH      = 0x1105,
  HEADSET          = 0x1108,
  A2DP_SINK        = 0x110b,
  AVRCP_TARGET     = 0x110c,
  A2DP             = 0x110d,
  AVRCP            = 0x110e,
  AVRCP_CONTROLLER = 0x110f,
  HEADSET_AG       = 0x1112,
  HANDSFREE        = 0x111e,
  HANDSFREE_AG     = 0x111f,
  HID              = 0x1124,
  PBAP_PCE         = 0x112e,
  PBAP_PSE         = 0x112f,
  MAP_MAS          = 0x1132,
  MAP_MNS          = 0x1133
};

struct BluetoothUuid {

  uint8_t mUuid[16];

  BluetoothUuid()
    : BluetoothUuid(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
  { }

  MOZ_IMPLICIT BluetoothUuid(const BluetoothUuid&) = default;

  BluetoothUuid(uint8_t aUuid0, uint8_t aUuid1,
                uint8_t aUuid2, uint8_t aUuid3,
                uint8_t aUuid4, uint8_t aUuid5,
                uint8_t aUuid6, uint8_t aUuid7,
                uint8_t aUuid8, uint8_t aUuid9,
                uint8_t aUuid10, uint8_t aUuid11,
                uint8_t aUuid12, uint8_t aUuid13,
                uint8_t aUuid14, uint8_t aUuid15)
  {
    mUuid[0] = aUuid0;
    mUuid[1] = aUuid1;
    mUuid[2] = aUuid2;
    mUuid[3] = aUuid3;
    mUuid[4] = aUuid4;
    mUuid[5] = aUuid5;
    mUuid[6] = aUuid6;
    mUuid[7] = aUuid7;
    mUuid[8] = aUuid8;
    mUuid[9] = aUuid9;
    mUuid[10] = aUuid10;
    mUuid[11] = aUuid11;
    mUuid[12] = aUuid12;
    mUuid[13] = aUuid13;
    mUuid[14] = aUuid14;
    mUuid[15] = aUuid15;
  }

  explicit BluetoothUuid(uint32_t aUuid32)
  {
    SetUuid32(aUuid32);
  }

  explicit BluetoothUuid(uint16_t aUuid16)
  {
    SetUuid16(aUuid16);
  }

  explicit BluetoothUuid(BluetoothServiceClass aServiceClass)
  {
    SetUuid16(static_cast<uint16_t>(aServiceClass));
  }

  BluetoothUuid& operator=(const BluetoothUuid& aRhs) = default;

  /**
   * |Clear| assigns an invalid value (i.e., zero) to the UUID.
   */
  void Clear()
  {
    operator=(BluetoothUuid());
  }

  /**
   * |IsCleared| returns true if the UUID contains a value of
   * zero.
   */
  bool IsCleared() const
  {
    return operator==(BluetoothUuid());
  }

  bool operator==(const BluetoothUuid& aRhs) const
  {
    return std::equal(aRhs.mUuid,
                      aRhs.mUuid + MOZ_ARRAY_LENGTH(aRhs.mUuid), mUuid);
  }

  bool operator!=(const BluetoothUuid& aRhs) const
  {
    return !operator==(aRhs);
  }

  /*
   * Getter-setter methods for short UUIDS. The first 4 bytes in the
   * UUID are represented by the short notation UUID32, and bytes 3
   * and 4 (indices 2 and 3) are represented by UUID16. The rest of
   * the UUID is filled with the SDP base UUID.
   *
   * Below are helpers for accessing these values.
   */

  void SetUuid32(uint32_t aUuid32)
  {
    BigEndian::writeUint32(&mUuid[0], aUuid32);
    mUuid[4] = 0x00;
    mUuid[5] = 0x00;
    mUuid[6] = 0x10;
    mUuid[7] = 0x00;
    mUuid[8] = 0x80;
    mUuid[9] = 0x00;
    mUuid[10] = 0x00;
    mUuid[11] = 0x80;
    mUuid[12] = 0x5f;
    mUuid[13] = 0x9b;
    mUuid[14] = 0x34;
    mUuid[15] = 0xfb;
  }

  uint32_t GetUuid32() const
  {
    return BigEndian::readUint32(&mUuid[0]);
  }

  void SetUuid16(uint16_t aUuid16)
  {
    SetUuid32(aUuid16); // MSB is 0x0000
  }

  uint16_t GetUuid16() const
  {
    return BigEndian::readUint16(&mUuid[2]);
  }
};

struct BluetoothPinCode {
  uint8_t mPinCode[16]; /* not \0-terminated */
  uint8_t mLength;
};

struct BluetoothServiceName {
  uint8_t mName[255]; /* not \0-terminated */
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

struct BluetoothRemoteName {
  uint8_t mName[248]; /* not \0-terminated */
};

struct BluetoothProperty {
  /* Type */
  BluetoothPropertyType mType;

  /* Value
   */

  /* PROPERTY_BDADDR */
  BluetoothAddress mBdAddress;

  /* PROPERTY_BDNAME
     PROPERTY_REMOTE_FRIENDLY_NAME */
  nsString mString;

  /* PROPERTY_UUIDS */
  nsTArray<BluetoothUuid> mUuidArray;

  /* PROPERTY_ADAPTER_BONDED_DEVICES */
  nsTArray<BluetoothAddress> mBdAddressArray;

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

  BluetoothProperty()
    : mType(PROPERTY_UNKNOWN)
  { }

  explicit BluetoothProperty(BluetoothPropertyType aType,
                             const BluetoothAddress& aBdAddress)
    : mType(aType)
    , mBdAddress(aBdAddress)
  { }

  explicit BluetoothProperty(BluetoothPropertyType aType,
                             const nsAString& aString)
    : mType(aType)
    , mString(aString)
  { }

  explicit BluetoothProperty(BluetoothPropertyType aType,
                             const nsTArray<BluetoothUuid>& aUuidArray)
    : mType(aType)
    , mUuidArray(aUuidArray)
  { }

  explicit BluetoothProperty(BluetoothPropertyType aType,
                             const nsTArray<BluetoothAddress>& aBdAddressArray)
    : mType(aType)
    , mBdAddressArray(aBdAddressArray)
  { }

  explicit BluetoothProperty(BluetoothPropertyType aType, uint32_t aUint32)
    : mType(aType)
    , mUint32(aUint32)
  { }

  explicit BluetoothProperty(BluetoothPropertyType aType, int32_t aInt32)
    : mType(aType)
    , mInt32(aInt32)
  { }

  explicit BluetoothProperty(BluetoothPropertyType aType,
                             BluetoothTypeOfDevice aTypeOfDevice)
    : mType(aType)
    , mTypeOfDevice(aTypeOfDevice)
  { }

  explicit BluetoothProperty(BluetoothPropertyType aType,
                             const BluetoothServiceRecord& aServiceRecord)
    : mType(aType)
    , mServiceRecord(aServiceRecord)
  { }

  explicit BluetoothProperty(BluetoothPropertyType aType,
                             BluetoothScanMode aScanMode)
    : mType(aType)
    , mScanMode(aScanMode)
  { }

  explicit BluetoothProperty(BluetoothPropertyType aType,
                             const BluetoothRemoteInfo& aRemoteInfo)
    : mType(aType)
    , mRemoteInfo(aRemoteInfo)
  { }
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
  BluetoothAddress mBdAddr;
  BluetoothGattServiceId mServiceId;
  BluetoothGattId mCharId;
  uint16_t mLength;
  uint8_t mValue[BLUETOOTH_GATT_MAX_ATTR_LEN];
  bool mIsNotify;
};

struct BluetoothGattTestParam {
  BluetoothAddress mBdAddr;
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
