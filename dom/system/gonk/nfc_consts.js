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
this.DEBUG_WORKER = false || DEBUG_ALL;
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
  0:'NDEF',
  1:'NDEF_WRITEABLE',
  2:'NDEF_FORMATABLE',
  3:'P2P',
  4:'NFC_A'
};

// TODO: Bug 933595. Fill-in all error codes for Gonk/nfcd protocol
this.GECKO_NFC_ERROR_SUCCESS             = 0;
this.GECKO_NFC_ERROR_GENERIC_FAILURE     = 1;

// NFC powerlevels must match config PDUs.
this.NFC_POWER_LEVEL_UNKNOWN        = -1;
this.NFC_POWER_LEVEL_DISABLED       = 0;
this.NFC_POWER_LEVEL_LOW            = 1;
this.NFC_POWER_LEVEL_ENABLED        = 2;

this.TOPIC_MOZSETTINGS_CHANGED      = "mozsettings-changed";
this.TOPIC_XPCOM_SHUTDOWN           = "xpcom-shutdown";
this.SETTING_NFC_ENABLED            = "nfc.enabled";
this.SETTING_NFC_POWER_LEVEL        = "nfc.powerlevel";

this.NFC_PEER_EVENT_READY = 0x01;
this.NFC_PEER_EVENT_LOST  = 0x02;

// Allow this file to be imported via Components.utils.import().
this.EXPORTED_SYMBOLS = Object.keys(this);
