/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Copyright © 2013, Deutsche Telekom, Inc. */

// Set to true to debug all NFC layers
this.DEBUG_ALL = false;

// Set individually to debug specific layers
this.DEBUG_CONTENT_HELPER = false || DEBUG_ALL;
this.DEBUG_NFC = false || DEBUG_ALL;

// Current version
this.NFC_MAJOR_VERSION = 1;
this.NFC_MINOR_VERSION = 7;

this.NFC_REQUEST_CONFIG = 0;
this.NFC_REQUEST_CONNECT = 1;
this.NFC_REQUEST_CLOSE = 2;
this.NFC_REQUEST_GET_DETAILS = 3;
this.NFC_REQUEST_READ_NDEF = 4;
this.NFC_REQUEST_WRITE_NDEF = 5;
this.NFC_REQUEST_MAKE_NDEF_READ_ONLY = 6;

this.NFC_RESPONSE_GENERAL = 1000;
this.NFC_RESPONSE_CONFIG = 1001;
this.NFC_RESPONSE_READ_NDEF_DETAILS = 1002;
this.NFC_RESPONSE_READ_NDEF = 1003;

this.NFC_NOTIFICATION_INITIALIZED = 2000;
this.NFC_NOTIFICATION_TECH_DISCOVERED = 2001;
this.NFC_NOTIFICATION_TECH_LOST = 2002;

this.NFC_TECHS = {
  0:"NDEF",
  1:"NDEF_WRITEABLE",
  2:"NDEF_FORMATABLE",
  3:"P2P",
  4:"NFC_A",
  5:"NFC_B",
  6:"NFC_F",
  7:"NFC_V",
  8:"NFC_ISO_DEP"
};

// nfcd error codes
this.NFC_SUCCESS = 0;
this.NFC_ERROR_IO = -1;
this.NFC_ERROR_CANCELLED = -2;
this.NFC_ERROR_TIMEOUT = -3;
this.NFC_ERROR_BUSY = -4;
this.NFC_ERROR_CONNECT = -5;
this.NFC_ERROR_DISCONNECT = -6;
this.NFC_ERROR_READ = -7;
this.NFC_ERROR_WRITE = -8;
this.NFC_ERROR_INVALID_PARAM = -9;
this.NFC_ERROR_INSUFFICIENT_RESOURCES = -10;
this.NFC_ERROR_SOCKET_CREATION = -11;
this.NFC_ERROR_SOCKET_NOT_CONNECTED = -12;
this.NFC_ERROR_BUFFER_TOO_SMALL = -13;
this.NFC_ERROR_SAP_USED = -14;
this.NFC_ERROR_SERVICE_NAME_USED = -15;
this.NFC_ERROR_SOCKET_OPTIONS = -16;
this.NFC_ERROR_FAIL_ENABLE_DISCOVERY = -17;
this.NFC_ERROR_FAIL_DISABLE_DISCOVERY = -18;
this.NFC_ERROR_NOT_INITIALIZED = -19;
this.NFC_ERROR_INITIALIZE_FAIL = -20;
this.NFC_ERROR_DEINITIALIZE_FAIL = -21;
this.NFC_ERROR_SE_CONNECTED = -22;
this.NFC_ERROR_NO_SE_CONNECTED = -23;
this.NFC_ERROR_NOT_SUPPORTED = -24;
this.NFC_ERROR_BAD_SESSION_ID = -25;
this.NFC_ERROR_LOST_TECH = -26;
this.NFC_ERROR_BAD_TECH_TYPE = -27;
this.NFC_ERROR_SELECT_SE_FAIL = -28;
this.NFC_ERROR_DESELECT_SE_FAIL = -29;
this.NFC_ERROR_FAIL_ENABLE_LOW_POWER_MODE = -30;
this.NFC_ERROR_FAIL_DISABLE_LOW_POWER_MODE = -31;

// Gecko specific error codes
this.NFC_GECKO_ERROR_GENERIC_FAILURE = 1;
this.NFC_GECKO_ERROR_P2P_REG_INVALID = 2;
this.NFC_GECKO_ERROR_NOT_ENABLED = 3;

this.NFC_ERROR_MSG = {};
this.NFC_ERROR_MSG[this.NFC_ERROR_IO] = "NfcIoError";
this.NFC_ERROR_MSG[this.NFC_ERROR_CANCELLED] = "NfcCancelledError";
this.NFC_ERROR_MSG[this.NFC_ERROR_TIMEOUT] = "NfcTimeoutError";
this.NFC_ERROR_MSG[this.NFC_ERROR_BUSY] = "NfcBusyError";
this.NFC_ERROR_MSG[this.NFC_ERROR_CONNECT] = "NfcConnectError";
this.NFC_ERROR_MSG[this.NFC_ERROR_DISCONNECT] = "NfcDisconnectError";
this.NFC_ERROR_MSG[this.NFC_ERROR_READ] = "NfcReadError";
this.NFC_ERROR_MSG[this.NFC_ERROR_WRITE] = "NfcWriteError";
this.NFC_ERROR_MSG[this.NFC_ERROR_INVALID_PARAM] = "NfcInvalidParamError";
this.NFC_ERROR_MSG[this.NFC_ERROR_INSUFFICIENT_RESOURCES] = "NfcInsufficentResourcesError";
this.NFC_ERROR_MSG[this.NFC_ERROR_SOCKET_CREATION] = "NfcSocketCreationError";
this.NFC_ERROR_MSG[this.NFC_ERROR_SOCKET_NOT_CONNECTED] = "NfcSocketNotConntectedError";
this.NFC_ERROR_MSG[this.NFC_ERROR_BUFFER_TOO_SMALL] = "NfcBufferTooSmallError";
this.NFC_ERROR_MSG[this.NFC_ERROR_SAP_USED] = "NfcSapUsedError";
this.NFC_ERROR_MSG[this.NFC_ERROR_SERVICE_NAME_USED] = "NfcServiceNameUsedError";
this.NFC_ERROR_MSG[this.NFC_ERROR_SOCKET_OPTIONS] = "NfcSocketOptionsError";
this.NFC_ERROR_MSG[this.NFC_ERROR_FAIL_ENABLE_DISCOVERY] = "NfcFailEnableDiscoveryError";
this.NFC_ERROR_MSG[this.NFC_ERROR_FAIL_DISABLE_DISCOVERY] = "NfcFailDisableDiscoveryError";
this.NFC_ERROR_MSG[this.NFC_ERROR_NOT_INITIALIZED] = "NfcNotInitializedError";
this.NFC_ERROR_MSG[this.NFC_ERROR_INITIALIZE_FAIL] = "NfcInitializeFailError";
this.NFC_ERROR_MSG[this.NFC_ERROR_DEINITIALIZE_FAIL] = "NfcDeinitializeFailError";
this.NFC_ERROR_MSG[this.NFC_ERROR_SE_CONNECTED] = "NfcSeConnectedError";
this.NFC_ERROR_MSG[this.NFC_ERROR_NO_SE_CONNECTED] = "NfcNoSeConnectedError";
this.NFC_ERROR_MSG[this.NFC_ERROR_NOT_SUPPORTED] = "NfcNotSupportedError";
this.NFC_ERROR_MSG[this.NFC_ERROR_BAD_SESSION_ID] = "NfcBadSessionIdError";
this.NFC_ERROR_MSG[this.NFC_ERROR_LOST_TECH] = "NfcLostTechError";
this.NFC_ERROR_MSG[this.NFC_ERROR_BAD_TECH_TYPE] = "NfcBadTechTypeError";
this.NFC_ERROR_MSG[this.NFC_ERROR_SELECT_SE_FAIL] = "SelectSecureElementFailed";
this.NFC_ERROR_MSG[this.NFC_ERROR_DESELECT_SE_FAIL] = "DeselectSecureElementFailed";
this.NFC_ERROR_MSG[this.NFC_ERROR_FAIL_ENABLE_LOW_POWER_MODE] = "EnableLowPowerModeFail";
this.NFC_ERROR_MSG[this.NFC_ERROR_FAIL_DISABLE_LOW_POWER_MODE] = "DisableLowPowerModeFail";
this.NFC_ERROR_MSG[this.NFC_GECKO_ERROR_GENERIC_FAILURE] = "NfcGenericFailureError";
this.NFC_ERROR_MSG[this.NFC_GECKO_ERROR_P2P_REG_INVALID] = "NfcP2PRegistrationInvalid";
this.NFC_ERROR_MSG[this.NFC_GECKO_ERROR_NOT_ENABLED] = "NfcNotEnabledError";

// NFC powerlevels must match config PDUs.
this.NFC_POWER_LEVEL_UNKNOWN        = -1;
this.NFC_POWER_LEVEL_DISABLED       = 0;
this.NFC_POWER_LEVEL_LOW            = 1;
this.NFC_POWER_LEVEL_ENABLED        = 2;

this.TOPIC_XPCOM_SHUTDOWN           = "xpcom-shutdown";

this.NFC_PEER_EVENT_READY = 0x01;
this.NFC_PEER_EVENT_LOST  = 0x02;

// Allow this file to be imported via Components.utils.import().
this.EXPORTED_SYMBOLS = Object.keys(this);
