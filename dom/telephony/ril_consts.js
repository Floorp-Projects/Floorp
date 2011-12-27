/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is RIL JS Worker.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Kyle Machulis <kyle@nonpolynomial.com>
 *   Philipp von Weitershausen <philipp@weitershausen.de>
 *   Fernando Jimenez <ferjmoreno@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const REQUEST_GET_SIM_STATUS = 1;
const REQUEST_ENTER_SIM_PIN = 2;
const REQUEST_ENTER_SIM_PUK = 3;
const REQUEST_ENTER_SIM_PIN2 = 4;
const REQUEST_ENTER_SIM_PUK2 = 5;
const REQUEST_CHANGE_SIM_PIN = 6;
const REQUEST_CHANGE_SIM_PIN2 = 7;
const REQUEST_ENTER_NETWORK_DEPERSONALIZATION = 8;
const REQUEST_GET_CURRENT_CALLS = 9;
const REQUEST_DIAL = 10;
const REQUEST_GET_IMSI = 11;
const REQUEST_HANGUP = 12;
const REQUEST_HANGUP_WAITING_OR_BACKGROUND = 13;
const REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND = 14;
const REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE = 15;
const REQUEST_SWITCH_HOLDING_AND_ACTIVE = 15;
const REQUEST_CONFERENCE = 16;
const REQUEST_UDUB = 17;
const REQUEST_LAST_CALL_FAIL_CAUSE = 18;
const REQUEST_SIGNAL_STRENGTH = 19;
const REQUEST_REGISTRATION_STATE = 20;
const REQUEST_GPRS_REGISTRATION_STATE = 21;
const REQUEST_OPERATOR = 22;
const REQUEST_RADIO_POWER = 23;
const REQUEST_DTMF = 24;
const REQUEST_SEND_SMS = 25;
const REQUEST_SEND_SMS_EXPECT_MORE = 26;
const REQUEST_SETUP_DATA_CALL = 27;
const REQUEST_SIM_IO = 28;
const REQUEST_SEND_USSD = 29;
const REQUEST_CANCEL_USSD = 30;
const REQUEST_GET_CLIR = 31;
const REQUEST_SET_CLIR = 32;
const REQUEST_QUERY_CALL_FORWARD_STATUS = 33;
const REQUEST_SET_CALL_FORWARD = 34;
const REQUEST_QUERY_CALL_WAITING = 35;
const REQUEST_SET_CALL_WAITING = 36;
const REQUEST_SMS_ACKNOWLEDGE = 37;
const REQUEST_GET_IMEI = 38;
const REQUEST_GET_IMEISV = 39;
const REQUEST_ANSWER = 40;
const REQUEST_DEACTIVATE_DATA_CALL = 41;
const REQUEST_QUERY_FACILITY_LOCK = 42;
const REQUEST_SET_FACILITY_LOCK = 43;
const REQUEST_CHANGE_BARRING_PASSWORD = 44;
const REQUEST_QUERY_NETWORK_SELECTION_MODE = 45;
const REQUEST_SET_NETWORK_SELECTION_AUTOMATIC = 46;
const REQUEST_SET_NETWORK_SELECTION_MANUAL = 47;
const REQUEST_QUERY_AVAILABLE_NETWORKS = 48;
const REQUEST_DTMF_START = 49;
const REQUEST_DTMF_STOP = 50;
const REQUEST_BASEBAND_VERSION = 51;
const REQUEST_SEPARATE_CONNECTION = 52;
const REQUEST_SET_MUTE = 53;
const REQUEST_GET_MUTE = 54;
const REQUEST_QUERY_CLIP = 55;
const REQUEST_LAST_DATA_CALL_FAIL_CAUSE = 56;
const REQUEST_DATA_CALL_LIST = 57;
const REQUEST_RESET_RADIO = 58;
const REQUEST_OEM_HOOK_RAW = 59;
const REQUEST_OEM_HOOK_STRINGS = 60;
const REQUEST_SCREEN_STATE = 61;
const REQUEST_SET_SUPP_SVC_NOTIFICATION = 62;
const REQUEST_WRITE_SMS_TO_SIM = 63;
const REQUEST_DELETE_SMS_ON_SIM = 64;
const REQUEST_SET_BAND_MODE = 65;
const REQUEST_QUERY_AVAILABLE_BAND_MODE = 66;
const REQUEST_STK_GET_PROFILE = 67;
const REQUEST_STK_SET_PROFILE = 68;
const REQUEST_STK_SEND_ENVELOPE_COMMAND = 69;
const REQUEST_STK_SEND_TERMINAL_RESPONSE = 70;
const REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM = 71;
const REQUEST_EXPLICIT_CALL_TRANSFER = 72;
const REQUEST_SET_PREFERRED_NETWORK_TYPE = 73;
const REQUEST_GET_PREFERRED_NETWORK_TYPE = 74;
const REQUEST_GET_NEIGHBORING_CELL_IDS = 75;
const REQUEST_SET_LOCATION_UPDATES = 76;
const REQUEST_CDMA_SET_SUBSCRIPTION = 77;
const REQUEST_CDMA_SET_ROAMING_PREFERENCE = 78;
const REQUEST_CDMA_QUERY_ROAMING_PREFERENCE = 79;
const REQUEST_SET_TTY_MODE = 80;
const REQUEST_QUERY_TTY_MODE = 81;
const REQUEST_CDMA_SET_PREFERRED_VOICE_PRIVACY_MODE = 82;
const REQUEST_CDMA_QUERY_PREFERRED_VOICE_PRIVACY_MODE = 83;
const REQUEST_CDMA_FLASH = 84;
const REQUEST_CDMA_BURST_DTMF = 85;
const REQUEST_CDMA_VALIDATE_AND_WRITE_AKEY = 86;
const REQUEST_CDMA_SEND_SMS = 87;
const REQUEST_CDMA_SMS_ACKNOWLEDGE = 88;
const REQUEST_GSM_GET_BROADCAST_SMS_CONFIG = 89;
const REQUEST_GSM_SET_BROADCAST_SMS_CONFIG = 90;
const REQUEST_GSM_SMS_BROADCAST_ACTIVATION = 91;
const REQUEST_CDMA_GET_BROADCAST_SMS_CONFIG = 92;
const REQUEST_CDMA_SET_BROADCAST_SMS_CONFIG = 93;
const REQUEST_CDMA_SMS_BROADCAST_ACTIVATION = 94;
const REQUEST_CDMA_SUBSCRIPTION = 95;
const REQUEST_CDMA_WRITE_SMS_TO_RUIM = 96;
const REQUEST_CDMA_DELETE_SMS_ON_RUIM = 97;
const REQUEST_DEVICE_IDENTITY = 98;
const REQUEST_EXIT_EMERGENCY_CALLBACK_MODE = 99;
const REQUEST_GET_SMSC_ADDRESS = 100;
const REQUEST_SET_SMSC_ADDRESS = 101;
const REQUEST_REPORT_SMS_MEMORY_STATUS = 102;
const REQUEST_REPORT_STK_SERVICE_IS_RUNNING = 103;

const RESPONSE_TYPE_SOLICITED = 0;
const RESPONSE_TYPE_UNSOLICITED = 1;

const UNSOLICITED_RESPONSE_BASE = 1000;
const UNSOLICITED_RESPONSE_RADIO_STATE_CHANGED = 1000;
const UNSOLICITED_RESPONSE_CALL_STATE_CHANGED = 1001;
const UNSOLICITED_RESPONSE_NETWORK_STATE_CHANGED = 1002;
const UNSOLICITED_RESPONSE_NEW_SMS = 1003;
const UNSOLICITED_RESPONSE_NEW_SMS_STATUS_REPORT = 1004;
const UNSOLICITED_RESPONSE_NEW_SMS_ON_SIM = 1005;
const UNSOLICITED_ON_USSD = 1006;
const UNSOLICITED_ON_USSD_REQUEST = 1007;
const UNSOLICITED_NITZ_TIME_RECEIVED = 1008;
const UNSOLICITED_SIGNAL_STRENGTH = 1009;
const UNSOLICITED_DATA_CALL_LIST_CHANGED = 1010;
const UNSOLICITED_SUPP_SVC_NOTIFICATION = 1011;
const UNSOLICITED_STK_SESSION_END = 1012;
const UNSOLICITED_STK_PROACTIVE_COMMAND = 1013;
const UNSOLICITED_STK_EVENT_NOTIFY = 1014;
const UNSOLICITED_STK_CALL_SETUP = 1015;
const UNSOLICITED_SIM_SMS_STORAGE_FULL = 1016;
const UNSOLICITED_SIM_REFRESH = 1017;
const UNSOLICITED_CALL_RING = 1018;
const UNSOLICITED_RESPONSE_SIM_STATUS_CHANGED = 1019;
const UNSOLICITED_RESPONSE_CDMA_NEW_SMS = 1020;
const UNSOLICITED_RESPONSE_NEW_BROADCAST_SMS = 1021;
const UNSOLICITED_CDMA_RUIM_SMS_STORAGE_FULL = 1022;
const UNSOLICITED_RESTRICTED_STATE_CHANGED = 1023;
const UNSOLICITED_ENTER_EMERGENCY_CALLBACK_MODE = 1024;
const UNSOLICITED_CDMA_CALL_WAITING = 1025;
const UNSOLICITED_CDMA_OTA_PROVISION_STATUS = 1026;
const UNSOLICITED_CDMA_INFO_REC = 1027;
const UNSOLICITED_OEM_HOOK_RAW = 1028;
const UNSOLICITED_RINGBACK_TONE = 1029;
const UNSOLICITED_RESEND_INCALL_MUTE = 1030;

const RADIO_STATE_OFF = 0;
const RADIO_STATE_UNAVAILABLE = 1;
const RADIO_STATE_SIM_NOT_READY = 2;
const RADIO_STATE_SIM_LOCKED_OR_ABSENT = 3;
const RADIO_STATE_SIM_READY = 4;
const RADIO_STATE_RUIM_NOT_READY = 5;
const RADIO_STATE_RUIM_READY = 6;
const RADIO_STATE_RUIM_LOCKED_OR_ABSENT = 7;
const RADIO_STATE_NV_NOT_READY = 8;
const RADIO_STATE_NV_READY = 9;

const CARD_MAX_APPS = 8;

const CALL_STATE_ACTIVE = 0;
const CALL_STATE_HOLDING = 1;
const CALL_STATE_DIALING = 2;
const CALL_STATE_ALERTING = 3;
const CALL_STATE_INCOMING = 4;
const CALL_STATE_WAITING = 5;

const TOA_INTERNATIONAL = 0x91;
const TOA_UNKNOWN = 0x81;

const CALL_PRESENTATION_ALLOWED = 0;
const CALL_PRESENTATION_RESTRICTED = 1;
const CALL_PRESENTATION_UNKNOWN = 2;
const CALL_PRESENTATION_PAYPHONE = 3;

const SMS_HANDLED = 0;


/**
 * DOM constants
 */

const DOM_RADIOSTATE_UNAVAILABLE   = "unavailable";
const DOM_RADIOSTATE_OFF           = "off";
const DOM_RADIOSTATE_READY         = "ready";

const DOM_CARDSTATE_UNAVAILABLE    = "unavailable";
const DOM_CARDSTATE_ABSENT         = "absent";
const DOM_CARDSTATE_PIN_REQUIRED   = "pin_required";
const DOM_CARDSTATE_PUK_REQUIRED   = "puk_required";
const DOM_CARDSTATE_NETWORK_LOCKED = "network_locked";
const DOM_CARDSTATE_NOT_READY      = "not_ready";
const DOM_CARDSTATE_READY          = "ready";

const DOM_CALL_READYSTATE_DIALING        = "dialing";
const DOM_CALL_READYSTATE_RINGING        = "ringing";
const DOM_CALL_READYSTATE_BUSY           = "busy";
const DOM_CALL_READYSTATE_CONNECTING     = "connecting";
const DOM_CALL_READYSTATE_CONNECTED      = "connected";
const DOM_CALL_READYSTATE_DISCONNECTING  = "disconnecting";
const DOM_CALL_READYSTATE_DISCONNECTED   = "disconnected";
const DOM_CALL_READYSTATE_INCOMING       = "incoming";
const DOM_CALL_READYSTATE_HOLDING        = "holding";
const DOM_CALL_READYSTATE_HELD           = "held";

const RIL_TO_DOM_CALL_STATE = [
  DOM_CALL_READYSTATE_CONNECTED, // CALL_READYSTATE_ACTIVE
  DOM_CALL_READYSTATE_HELD,      // CALL_READYSTATE_HOLDING
  DOM_CALL_READYSTATE_DIALING,   // CALL_READYSTATE_DIALING
  DOM_CALL_READYSTATE_RINGING,   // CALL_READYSTATE_ALERTING
  DOM_CALL_READYSTATE_INCOMING,  // CALL_READYSTATE_INCOMING
  DOM_CALL_READYSTATE_HELD       // CALL_READYSTATE_WAITING (XXX is this right?)
];


/**
 * GSM PDU constants
 */

// PDU TYPE-OF-ADDRESS
const PDU_TOA_UNKNOWN       = 0x80; // Unknown. This is used when the user or
                                    // network has no a priori information
                                    // about the numbering plan.
const PDU_TOA_ISDN          = 0x81; // ISDN/Telephone numbering
const PDU_TOA_DATA_NUM      = 0x83; // Data numbering plan
const PDU_TOA_TELEX_NUM     = 0x84; // Telex numbering plan
const PDU_TOA_NATIONAL_NUM  = 0x88; // National numbering plan
const PDU_TOA_PRIVATE_NUM   = 0x89; // Private numbering plan
const PDU_TOA_ERMES_NUM     = 0x8A; // Ermes numbering plan
const PDU_TOA_INTERNATIONAL = 0x90; // International number
const PDU_TOA_NATIONAL      = 0xA0; // National number. Prefix or escape digits
                                    // shall not be included
const PDU_TOA_NETWORK_SPEC  = 0xB0; // Network specific number This is used to
                                    // indicate administration/service number
                                    // specific to the serving network
const PDU_TOA_SUSCRIBER     = 0xC0; // Suscriber number. This is used when a
                                    // specific short number representation is
                                    // stored in one or more SCs as part of a
                                    // higher layer application
const PDU_TOA_ALPHANUMERIC  = 0xD0; // Alphanumeric, (coded according to GSM TS
                                    // 03.38 7-bit default alphabet)
const PDU_TOA_ABBREVIATED   = 0xE0; // Abbreviated number

/**
 * First octet of the SMS-DELIVER PDU
 *
 * RP:     0   Reply Path parameter is not set in this PDU
 *         1   Reply Path parameter is set in this PDU
 *
 * UDHI:   0   The UD field contains only the short message
 *         1   The beginning of the UD field contains a header in addition of
 *             the short message
 *
 * SRI: (is only set by the SMSC)
 *         0    A status report will not be returned to the SME
 *         1    A status report will be returned to the SME
 *
 * MMS: (is only set by the SMSC)
 *         0    More messages are waiting for the MS in the SMSC
 *         1    No more messages are waiting for the MS in the SMSC
 *
 * MTI:   bit1    bit0    Message type
 *         0       0       SMS-DELIVER (SMSC ==> MS)
 *         0       0       SMS-DELIVER REPORT (MS ==> SMSC, is generated
 *                         automatically by the M20, after receiving a
 *                         SMS-DELIVER)
 *         0       1       SMS-SUBMIT (MS ==> SMSC)
 *         0       1       SMS-SUBMIT REPORT (SMSC ==> MS)
 *         1       0       SMS-STATUS REPORT (SMSC ==> MS)
 *         1       0       SMS-COMMAND (MS ==> SMSC)
 *         1       1       Reserved
 */
const PDU_RP    = 0x80;       // Reply path. Parameter indicating that
                              // reply path exists.
const PDU_UDHI  = 0x40;       // User data header indicator. This bit is
                              // set to 1 if the User Data field starts
                              // with a header
const PDU_SRI_SRR = 0x20;     // Status report indication (SMS-DELIVER)
                              // or request (SMS-SUBMIT)
const PDU_VPF_ABSOLUTE = 0x18;// Validity period aboslute format
                              // (SMS-SUBMIT only)
const PDU_VPF_RELATIVE = 0x10;// Validity period relative format
                              // (SMS-SUBMIT only)
const PDU_VPF_ENHANCED = 0x8; // Validity period enhance format
                              // (SMS-SUBMIT only)
const PDU_MMS_RD       = 0x04;// More messages to send. (SMS-DELIVER only) or
                              // Reject duplicates (SMS-SUBMIT only)

// MTI - Message Type Indicator
const PDU_MTI_SMS_STATUS_COMMAND  = 0x02;
const PDU_MTI_SMS_SUBMIT          = 0x01;
const PDU_MTI_SMS_DELIVER         = 0x00;

// User Data max length in octets
const PDU_MAX_USER_DATA_7BIT = 160;

// DCS - Data Coding Scheme
const PDU_DCS_MSG_CODING_7BITS_ALPHABET = 0x00;
const PDU_DCS_MSG_CODING_8BITS_ALPHABET = 0x04;
const PDU_DCS_MSG_CODING_16BITS_ALPHABET= 0x08;
const PDU_DCS_MSG_CLASS_ME_SPECIFIC     = 0xF1;
const PDU_DCS_MSG_CLASS_SIM_SPECIFIC    = 0xF2;
const PDU_DCS_MSG_CLASS_TE_SPECIFIC     = 0xF3;

// Because service center timestamp omit the century. Yay.
const PDU_TIMESTAMP_YEAR_OFFSET = 2000;

// 7bit Default Alphabet
//TODO: maybe convert this to a string? might be faster/cheaper
const PDU_ALPHABET_7BIT_DEFAULT = [
  "@",      // COMMERCIAL AT
  "\xa3",   // POUND SIGN
  "$",      // DOLLAR SIGN
  "\xa5",   // YEN SIGN
  "\xe8",   // LATIN SMALL LETTER E WITH GRAVE
  "\xe9",   // LATIN SMALL LETTER E WITH ACUTE
  "\xf9",   // LATIN SMALL LETTER U WITH GRAVE
  "\xec",   // LATIN SMALL LETTER I WITH GRAVE
  "\xf2",   // LATIN SMALL LETTER O WITH GRAVE
  "\xc7",   // LATIN CAPITAL LETTER C WITH CEDILLA
  "\n",     // LINE FEED
  "\xd8",   // LATIN CAPITAL LETTER O WITH STROKE
  "\xf8",   // LATIN SMALL LETTER O WITH STROKE
  "\r",     // CARRIAGE RETURN
  "\xc5",   // LATIN CAPITAL LETTER A WITH RING ABOVE
  "\xe5",   // LATIN SMALL LETTER A WITH RING ABOVE
  "\u0394", // GREEK CAPITAL LETTER DELTA
  "_",      // LOW LINE
  "\u03a6", // GREEK CAPITAL LETTER PHI
  "\u0393", // GREEK CAPITAL LETTER GAMMA
  "\u039b", // GREEK CAPITAL LETTER LAMBDA
  "\u03a9", // GREEK CAPITAL LETTER OMEGA
  "\u03a0", // GREEK CAPITAL LETTER PI
  "\u03a8", // GREEK CAPITAL LETTER PSI
  "\u03a3", // GREEK CAPITAL LETTER SIGMA
  "\u0398", // GREEK CAPITAL LETTER THETA
  "\u039e", // GREEK CAPITAL LETTER XI
  "\u20ac", // (escape to extension table)
  "\xc6",   // LATIN CAPITAL LETTER AE
  "\xe6",   // LATIN SMALL LETTER AE
  "\xdf",   // LATIN SMALL LETTER SHARP S (German)
  "\xc9",   // LATIN CAPITAL LETTER E WITH ACUTE
  " ",      // SPACE
  "!",      // EXCLAMATION MARK
  "\"",     // QUOTATION MARK
  "#",      // NUMBER SIGN
  "\xa4",   // CURRENCY SIGN
  "%",      // PERCENT SIGN
  "&",      // AMPERSAND
  "'",      // APOSTROPHE
  "(",      // LEFT PARENTHESIS
  ")",      // RIGHT PARENTHESIS
  "*",      // ASTERISK
  "+",      // PLUS SIGN
  ",",      // COMMA
  "-",      // HYPHEN-MINUS
  ".",      // FULL STOP
  "/",      // SOLIDUS (SLASH)
  "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
  ":",      // COLON
  ";",      // SEMICOLON
  "<",      // LESS-THAN SIGN
  "=",      // EQUALS SIGN
  ">",      // GREATER-THAN SIGN
  "?",      // QUESTION MARK
  "\xa1",   // INVERTED EXCLAMATION MARK
  "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
  "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
  "\xc4",   // LATIN CAPITAL LETTER A WITH DIAERESIS
  "\xd6",   // LATIN CAPITAL LETTER O WITH DIAERESIS
  "\xd1",   // LATIN CAPITAL LETTER N WITH TILDE
  "\xdc",   // LATIN CAPITAL LETTER U WITH DIAERESIS
  "\xa7",   // SECTION SIGN
  "\xbf",   // INVERTED QUESTION MARK
  "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m",
  "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z",
  "\xe4",   // LATIN SMALL LETTER A WITH DIAERESIS
  "\xf6",   // LATIN SMALL LETTER O WITH DIAERESIS
  "\xf1",   // LATIN SMALL LETTER N WITH TILDE
  "\xfc",   // LATIN SMALL LETTER U WITH DIAERESIS
  "\xe0"    // LATIN SMALL LETTER A WITH GRAVE
];
