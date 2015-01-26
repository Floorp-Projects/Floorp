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
this.DEBUG_CONTENT_HELPER = DEBUG_ALL || false;
this.DEBUG_NFC = DEBUG_ALL || false;

// Gecko specific error codes
this.NFC_GECKO_ERROR_P2P_REG_INVALID = 1;
this.NFC_GECKO_ERROR_SEND_FILE_FAILED = 2;

this.NFC_ERROR_MSG = {};
this.NFC_ERROR_MSG[this.NFC_GECKO_ERROR_P2P_REG_INVALID] = "NfcP2PRegistrationInvalid";
this.NFC_ERROR_MSG[this.NFC_GECKO_ERROR_SEND_FILE_FAILED] = "NfcSendFileFailed";

this.NFC_RF_STATE_IDLE = "idle";
this.NFC_RF_STATE_LISTEN = "listen";
this.NFC_RF_STATE_DISCOVERY = "discovery";

this.TOPIC_MOZSETTINGS_CHANGED      = "mozsettings-changed";
this.TOPIC_XPCOM_SHUTDOWN           = "xpcom-shutdown";

this.SETTING_NFC_DEBUG = "nfc.debugging.enabled";

this.PEER_EVENT_READY = 0x01;
this.PEER_EVENT_LOST  = 0x02;
this.TAG_EVENT_FOUND = 0x03;
this.TAG_EVENT_LOST  = 0x04;
this.PEER_EVENT_FOUND = 0x05;
this.RF_EVENT_STATE_CHANGED = 0x06;
this.FOCUS_CHANGED = 0x07;

// This value should sync with |SYSTEM_APP_ID| in nsINfcContentHelper.idl
this.SYSTEM_APP_ID = -1;

// Allow this file to be imported via Components.utils.import().
this.EXPORTED_SYMBOLS = Object.keys(this);
