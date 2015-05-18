/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Copyright © 2014, Deutsche Telekom, Inc. */

// Set to true to debug SecureElement (SE) stack
this.DEBUG_ALL = false;

// Set individually to debug specific layers
this.DEBUG_CONNECTOR = DEBUG_ALL || false;
this.DEBUG_ACE = DEBUG_ALL || false ;
this.DEBUG_SE = DEBUG_ALL || false ;

// Maximun logical channels per session.
// For 'uicc' SE type this value is 3, as opening a basic channel' : 0
// is not allowed for security reasons. In such scenarios, possible
// supplementary logical channels available are : [1, 2, or 3].
// However,Other SE types may support upto max 4 (including '0').
this.MAX_CHANNELS_ALLOWED_PER_SESSION = 4;

this.BASIC_CHANNEL = 0;

// According GPCardSpec 2.2
this.MAX_APDU_LEN = 255; // including APDU header

// CLA (1 byte) + INS (1 byte) + P1 (1 byte) + P2 (1 byte)
this.APDU_HEADER_LEN = 4;

this.LOGICAL_CHANNEL_NUMBER_LIMIT = 4;
this.SUPPLEMENTARY_LOGICAL_CHANNEL_NUMBER_LIMIT = 20;

this.MIN_AID_LEN = 5;
this.MAX_AID_LEN = 16;

this.CLA_GET_RESPONSE = 0x00;

this.INS_SELECT = 0xA4;
this.INS_MANAGE_CHANNEL = 0x70;
this.INS_GET_RESPONSE = 0xC0;

// Match the following errors with SecureElement.webidl's SEError enum values
this.ERROR_NONE               = "";
this.ERROR_SECURITY           = "SESecurityError";
this.ERROR_IO                 = "SEIoError";
this.ERROR_BADSTATE           = "SEBadStateError";
this.ERROR_INVALIDCHANNEL     = "SEInvalidChannelError";
this.ERROR_INVALIDAPPLICATION = "SEInvalidApplicationError";
this.ERROR_GENERIC            = "SEGenericError";
this.ERROR_NOTPRESENT         = "SENotPresentError";

this.TYPE_UICC = "uicc";
this.TYPE_ESE = "eSE";

// Allow this file to be imported via Components.utils.import().
this.EXPORTED_SYMBOLS = Object.keys(this);
