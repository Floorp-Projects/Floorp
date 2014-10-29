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

// nfcd error codes
this.NFC_SUCCESS = 0;
this.NFC_ERROR_IO = -1;
this.NFC_ERROR_TIMEOUT = -2;
this.NFC_ERROR_BUSY = -3;
this.NFC_ERROR_CONNECT = -4;
this.NFC_ERROR_DISCONNECT = -5;
this.NFC_ERROR_READ = -6;
this.NFC_ERROR_WRITE = -7;
this.NFC_ERROR_INVALID_PARAM = -8;
this.NFC_ERROR_INSUFFICIENT_RESOURCES = -9;
this.NFC_ERROR_SOCKET_CREATION = -10;
this.NFC_ERROR_FAIL_ENABLE_DISCOVERY = -11;
this.NFC_ERROR_FAIL_DISABLE_DISCOVERY = -12;
this.NFC_ERROR_NOT_INITIALIZED = -13;
this.NFC_ERROR_INITIALIZE_FAIL = -14;
this.NFC_ERROR_DEINITIALIZE_FAIL = -15;
this.NFC_ERROR_NOT_SUPPORTED = -16;
this.NFC_ERROR_BAD_SESSION_ID = -17,
this.NFC_ERROR_FAIL_ENABLE_LOW_POWER_MODE = -18;
this.NFC_ERROR_FAIL_DISABLE_LOW_POWER_MODE = -19;

// Gecko specific error codes
this.NFC_GECKO_ERROR_GENERIC_FAILURE = 1;
this.NFC_GECKO_ERROR_P2P_REG_INVALID = 2;
this.NFC_GECKO_ERROR_NOT_ENABLED = 3;
this.NFC_GECKO_ERROR_SEND_FILE_FAILED = 4;

this.NFC_ERROR_MSG = {};
this.NFC_ERROR_MSG[this.NFC_ERROR_IO] = "NfcIoError";
this.NFC_ERROR_MSG[this.NFC_ERROR_TIMEOUT] = "NfcTimeoutError";
this.NFC_ERROR_MSG[this.NFC_ERROR_BUSY] = "NfcBusyError";
this.NFC_ERROR_MSG[this.NFC_ERROR_CONNECT] = "NfcConnectError";
this.NFC_ERROR_MSG[this.NFC_ERROR_DISCONNECT] = "NfcDisconnectError";
this.NFC_ERROR_MSG[this.NFC_ERROR_READ] = "NfcReadError";
this.NFC_ERROR_MSG[this.NFC_ERROR_WRITE] = "NfcWriteError";
this.NFC_ERROR_MSG[this.NFC_ERROR_INVALID_PARAM] = "NfcInvalidParamError";
this.NFC_ERROR_MSG[this.NFC_ERROR_INSUFFICIENT_RESOURCES] = "NfcInsufficentResourcesError";
this.NFC_ERROR_MSG[this.NFC_ERROR_SOCKET_CREATION] = "NfcSocketCreationError";
this.NFC_ERROR_MSG[this.NFC_ERROR_FAIL_ENABLE_DISCOVERY] = "NfcFailEnableDiscoveryError";
this.NFC_ERROR_MSG[this.NFC_ERROR_FAIL_DISABLE_DISCOVERY] = "NfcFailDisableDiscoveryError";
this.NFC_ERROR_MSG[this.NFC_ERROR_NOT_INITIALIZED] = "NfcNotInitializedError";
this.NFC_ERROR_MSG[this.NFC_ERROR_INITIALIZE_FAIL] = "NfcInitializeFailError";
this.NFC_ERROR_MSG[this.NFC_ERROR_DEINITIALIZE_FAIL] = "NfcDeinitializeFailError";
this.NFC_ERROR_MSG[this.NFC_ERROR_NOT_SUPPORTED] = "NfcNotSupportedError";
this.NFC_ERROR_MSG[this.NFC_ERROR_BAD_SESSION_ID] = "NfcBadSessionIdError";
this.NFC_ERROR_MSG[this.NFC_ERROR_FAIL_ENABLE_LOW_POWER_MODE] = "EnableLowPowerModeFail";
this.NFC_ERROR_MSG[this.NFC_ERROR_FAIL_DISABLE_LOW_POWER_MODE] = "DisableLowPowerModeFail";
this.NFC_ERROR_MSG[this.NFC_GECKO_ERROR_GENERIC_FAILURE] = "NfcGenericFailureError";
this.NFC_ERROR_MSG[this.NFC_GECKO_ERROR_P2P_REG_INVALID] = "NfcP2PRegistrationInvalid";
this.NFC_ERROR_MSG[this.NFC_GECKO_ERROR_NOT_ENABLED] = "NfcNotEnabledError";
this.NFC_ERROR_MSG[this.NFC_GECKO_ERROR_SEND_FILE_FAILED] = "NfcSendFileFailed";

// NFC powerlevels must match config PDUs.
this.NFC_POWER_LEVEL_UNKNOWN        = -1;
this.NFC_POWER_LEVEL_DISABLED       = 0;
this.NFC_POWER_LEVEL_LOW            = 1;
this.NFC_POWER_LEVEL_ENABLED        = 2;

this.TOPIC_MOZSETTINGS_CHANGED      = "mozsettings-changed";
this.TOPIC_XPCOM_SHUTDOWN           = "xpcom-shutdown";

this.SETTING_NFC_DEBUG = "nfc.debugging.enabled";

this.PEER_EVENT_READY = 0x01;
this.PEER_EVENT_LOST  = 0x02;
this.TAG_EVENT_FOUND = 0x03;
this.TAG_EVENT_LOST  = 0x04;

// Allow this file to be imported via Components.utils.import().
this.EXPORTED_SYMBOLS = Object.keys(this);
