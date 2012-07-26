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

// Set to true to debug all RIL layers
const DEBUG_ALL = false;

// Set individually to debug specific layers
const DEBUG_WORKER = false || DEBUG_ALL;
const DEBUG_CONTENT_HELPER = false || DEBUG_ALL;
const DEBUG_RIL = false || DEBUG_ALL;

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
const REQUEST_VOICE_REGISTRATION_STATE = 20;
const REQUEST_DATA_REGISTRATION_STATE = 21;
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
const REQUEST_CDMA_SET_SUBSCRIPTION_SOURCE = 77;
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
const REQUEST_CDMA_GET_SUBSCRIPTION_SOURCE = 104;
const REQUEST_ISIM_AUTHENTICATION = 105;
const REQUEST_ACKNOWLEDGE_INCOMING_GSM_SMS_WITH_PDU = 106;
const REQUEST_STK_SEND_ENVELOPE_WITH_STATUS = 107;
const REQUEST_DIAL_EMERGENCY_CALL = 10016;

// Akami/Maguro specific parcel types.
const REQUEST_VOICE_RADIO_TECH = 105;
const REQUEST_IMS_REGISTRATION_STATE = 106;
const REQUEST_IMS_SEND_SMS = 107;
const REQUEST_GET_DATA_CALL_PROFILE = 108;
const REQUEST_SET_UICC_SUBSCRIPTION = 109;
const REQUEST_SET_DATA_SUBSCRIPTION = 110;
const REQUEST_GET_UICC_SUBSCRIPTION = 111;
const REQUEST_GET_DATA_SUBSCRIPTION = 112;
const REQUEST_SET_SUBSCRIPTION_MODE = 113;
const REQUEST_SET_TRANSMIT_POWER = 114;
const REQUEST_SETUP_QOS = 115;
const REQUEST_RELEASE_QOS = 116;
const REQUEST_GET_QOS_STATUS = 117;
const REQUEST_MODIFY_QOS = 118;
const REQUEST_SUSPEND_QOS = 119;
const REQUEST_RESUME_QOS = 120;

const RESPONSE_TYPE_SOLICITED = 0;
const RESPONSE_TYPE_UNSOLICITED = 1;

const UNSOLICITED_RESPONSE_BASE = 1000;
const UNSOLICITED_RESPONSE_RADIO_STATE_CHANGED = 1000;
const UNSOLICITED_RESPONSE_CALL_STATE_CHANGED = 1001;
const UNSOLICITED_RESPONSE_VOICE_NETWORK_STATE_CHANGED = 1002;
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
const UNSOLICITED_CDMA_SUBSCRIPTION_SOURCE_CHANGED = 1031;
const UNSOLICITED_CDMA_PRL_CHANGED = 1032;
const UNSOLICITED_EXIT_EMERGENCY_CALLBACK_MODE = 1033;
const UNSOLICITED_RIL_CONNECTED = 1034;

// Akami/Maguro specific parcels.
const UNSOLICITED_VOICE_RADIO_TECH_CHANGED = 1034;
const UNSOLICITED_RESPONSE_IMS_NETWORK_STATE_CHANGED = 1035;
const UNSOLICITED_RESPONSE_TETHERED_MODE_STATE_CHANGED = 1036;
const UNSOLICITED_RESPONSE_DATA_NETWORK_STATE_CHANGED = 1037;
const UNSOLICITED_ON_SS = 1038;
const UNSOLICITED_STK_CC_ALPHA_NOTIFY = 1039;
const UNSOLICITED_UICC_SUBSCRIPTION_STATUS_CHANGED = 1040;
const UNSOLICITED_QOS_STATE_CHANGED_IND = 1041;

const ERROR_SUCCESS = 0;
const ERROR_RADIO_NOT_AVAILABLE = 1;
const ERROR_GENERIC_FAILURE = 2;
const ERROR_PASSWORD_INCORRECT = 3;
const ERROR_SIM_PIN2 = 4;
const ERROR_SIM_PUK2 = 5;
const ERROR_REQUEST_NOT_SUPPORTED = 6;
const ERROR_CANCELLED = 7;
const ERROR_OP_NOT_ALLOWED_DURING_VOICE_CALL = 8;
const ERROR_OP_NOT_ALLOWED_BEFORE_REG_TO_NW = 9;
const ERROR_SMS_SEND_FAIL_RETRY = 10;
const ERROR_SIM_ABSENT = 11;
const ERROR_SUBSCRIPTION_NOT_AVAILABLE = 12;
const ERROR_MODE_NOT_SUPPORTED = 13;
const ERROR_FDN_CHECK_FAILURE = 14;
const ERROR_ILLEGAL_SIM_OR_ME = 15;
const ERROR_DIAL_MODIFIED_TO_USSD = 17;
const ERROR_DIAL_MODIFIED_TO_SS = 18;
const ERROR_DIAL_MODIFIED_TO_DIAL = 19;
const ERROR_USSD_MODIFIED_TO_DIAL = 20;
const ERROR_USSD_MODIFIED_TO_SS = 21;
const ERROR_USSD_MODIFIED_TO_USSD = 22;
const ERROR_SS_MODIFIED_TO_DIAL = 23;
const ERROR_SS_MODIFIED_TO_USSD = 24;
const ERROR_SS_MODIFIED_TO_SS = 25;
const ERROR_SUBSCRIPTION_NOT_SUPPORTED = 26;

const GECKO_ERROR_SUCCESS = null;
const GECKO_ERROR_RADIO_NOT_AVAILABLE = "RadioNotAvailable";
const GECKO_ERROR_GENERIC_FAILURE = "GenericFailure";
const GECKO_ERROR_REQUEST_NOT_SUPPORTED = "RequestNotSupported";
const GECKO_ERROR_ILLEGAL_SIM_OR_ME = "IllegalSIMorME";

const RIL_ERROR_TO_GECKO_ERROR = {};
RIL_ERROR_TO_GECKO_ERROR[ERROR_SUCCESS] = GECKO_ERROR_SUCCESS;
RIL_ERROR_TO_GECKO_ERROR[ERROR_RADIO_NOT_AVAILABLE] = GECKO_ERROR_RADIO_NOT_AVAILABLE;
RIL_ERROR_TO_GECKO_ERROR[ERROR_GENERIC_FAILURE] = GECKO_ERROR_GENERIC_FAILURE;
RIL_ERROR_TO_GECKO_ERROR[ERROR_REQUEST_NOT_SUPPORTED] = GECKO_ERROR_REQUEST_NOT_SUPPORTED;
RIL_ERROR_TO_GECKO_ERROR[ERROR_ILLEGAL_SIM_OR_ME] = GECKO_ERROR_ILLEGAL_SIM_OR_ME;

// 3GPP 23.040 clause 9.2.3.6 TP-Message-Reference(TP-MR):
// The number of times the MS automatically repeats the SMS-SUBMIT shall be in
// the range 1 to 3 but the precise number is an implementation matter.
const SMS_RETRY_MAX = 3;

const RADIO_STATE_OFF = 0;
const RADIO_STATE_UNAVAILABLE = 1;
const RADIO_STATE_ON = 2;

// RIL v5 legacy constants:
const RADIO_STATE_SIM_NOT_READY = 2;
const RADIO_STATE_SIM_LOCKED_OR_ABSENT = 3;
const RADIO_STATE_SIM_READY = 4;
const RADIO_STATE_RUIM_NOT_READY = 5;
const RADIO_STATE_RUIM_READY = 6;
const RADIO_STATE_RUIM_LOCKED_OR_ABSENT = 7;
const RADIO_STATE_NV_NOT_READY = 8;
const RADIO_STATE_NV_READY = 9;

const CARD_STATE_ABSENT = 0;
const CARD_STATE_PRESENT = 1;
const CARD_STATE_ERROR = 2;

const CARD_PERSOSUBSTATE_UNKNOWN = 0;
const CARD_PERSOSUBSTATE_IN_PROGRESS = 1;
const CARD_PERSOSUBSTATE_READY = 2;
const CARD_PERSOSUBSTATE_SIM_NETWORK = 3;
const CARD_PERSOSUBSTATE_SIM_NETWORK_SUBSET = 4;
const CARD_PERSOSUBSTATE_SIM_CORPORATE = 5;
const CARD_PERSOSUBSTATE_SIM_SERVICE_PROVIDER = 6;
const CARD_PERSOSUBSTATE_SIM_SIM = 7;
const CARD_PERSOSUBSTATE_SIM_NETWORK_PUK = 8;
const CARD_PERSOSUBSTATE_SIM_NETWORK_SUBSET_PUK = 9;
const CARD_PERSOSUBSTATE_SIM_CORPORATE_PUK = 10;
const CARD_PERSOSUBSTATE_SIM_SERVICE_PROVIDER_PUK = 11;
const CARD_PERSOSUBSTATE_SIM_SIM_PUK = 12;
const CARD_PERSOSUBSTATE_RUIM_NETWORK1 = 13;
const CARD_PERSOSUBSTATE_RUIM_NETWORK2 = 14;
const CARD_PERSOSUBSTATE_RUIM_HRPD = 15;
const CARD_PERSOSUBSTATE_RUIM_CORPORATE = 16;
const CARD_PERSOSUBSTATE_RUIM_SERVICE_PROVIDER = 17;
const CARD_PERSOSUBSTATE_RUIM_RUIM = 18;
const CARD_PERSOSUBSTATE_RUIM_NETWORK1_PUK = 19;
const CARD_PERSOSUBSTATE_RUIM_NETWORK2_PUK = 20;
const CARD_PERSOSUBSTATE_RUIM_HRPD_PUK = 21;
const CARD_PERSOSUBSTATE_RUIM_CORPORATE_PUK = 22;
const CARD_PERSOSUBSTATE_RUIM_SERVICE_PROVIDER_PUK = 23;
const CARD_PERSOSUBSTATE_RUIM_RUIM_PUK = 24;

const CARD_APPSTATE_ILLEGAL = -1;
const CARD_APPSTATE_UNKNOWN = 0;
const CARD_APPSTATE_DETECTED = 1;
const CARD_APPSTATE_PIN = 2; // If PIN1 or UPin is required.
const CARD_APPSTATE_PUK = 3; // If PUK1 or Puk for UPin is required.
const CARD_APPSTATE_SUBSCRIPTION_PERSO = 4; // perso_substate should be looked
                                            // at when app_state is assigned
                                            // to this value.
const CARD_APPSTATE_READY = 5;

const CARD_PINSTATE_UNKNOWN = 0;
const CARD_PINSTATE_ENABLED_NOT_VERIFIED = 1;
const CARD_PINSTATE_ENABLED_VERIFIED = 2;
const CARD_PINSTATE_DISABLED = 3;
const CARD_PINSTATE_ENABLED_BLOCKED = 4;
const CARD_PINSTATE_ENABLED_PERM_BLOCKED = 5;

const CARD_APPTYPE_UNKNOWN = 0;
const CARD_APPTYPE_SIM = 1;
const CARD_APPTYPE_USIM = 2;
const CARD_APPTYPE_RUIM = 3;
const CARD_APPTYPE_CSIM = 4;
const CARD_APPTYPE_ISIM = 5;

const CARD_MAX_APPS = 8;

const NETWORK_STATE_UNKNOWN = "unknown";
const NETWORK_STATE_AVAILABLE = "available";
const NETWORK_STATE_CONNECTED = "connected";
const NETWORK_STATE_FORBIDDEN = "forbidden";

const NETWORK_SELECTION_MODE_AUTOMATIC = 0;
const NETWORK_SELECTION_MODE_MANUAL = 1;

const NETWORK_INFO_VOICE_REGISTRATION_STATE = "voiceRegistrationState";
const NETWORK_INFO_DATA_REGISTRATION_STATE = "dataRegistrationState";
const NETWORK_INFO_OPERATOR = "operator";
const NETWORK_INFO_NETWORK_SELECTION_MODE = "networkSelectionMode";
const NETWORK_INFO_MESSAGE_TYPES = [
  NETWORK_INFO_VOICE_REGISTRATION_STATE,
  NETWORK_INFO_DATA_REGISTRATION_STATE,
  NETWORK_INFO_OPERATOR,
  NETWORK_INFO_NETWORK_SELECTION_MODE
];

const PREFERRED_NETWORK_TYPE_GSM_WCDMA = 0;
const PREFERRED_NETWORK_TYPE_GSM_ONLY = 1;
const PREFERRED_NETWORK_TYPE_WCDMA = 2;
const PREFERRED_NETWORK_TYPE_GSM_WCDMA_AUTO = 3;
const PREFERRED_NETWORK_TYPE_CDMA_EVDO_AUTO = 4;
const PREFERRED_NETWORK_TYPE_CDMA_ONLY = 5;
const PREFERRED_NETWORK_TYPE_EVDO_ONLY = 6;
const PREFERRED_NETWORK_TYPE_GSM_WCDMA_CDMA_EVDO_AUTO = 7;
const PREFERRED_NETWORK_TYPE_LTE_CDMA_EVDO = 8;
const PREFERRED_NETWORK_TYPE_LTE_GSM_WCDMA = 9;
const PREFERRED_NETWORK_TYPE_LTE_CMDA_EVDO_GSM_WCDMA = 10;
const PREFERRED_NETWORK_TYPE_LTE_ONLY = 11;

// Network registration states. See TS 27.007 7.2
const NETWORK_CREG_STATE_NOT_SEARCHING = 0;
const NETWORK_CREG_STATE_REGISTERED_HOME = 1;
const NETWORK_CREG_STATE_SEARCHING = 2;
const NETWORK_CREG_STATE_DENIED = 3;
const NETWORK_CREG_STATE_UNKNOWN = 4;
const NETWORK_CREG_STATE_REGISTERED_ROAMING = 5;
const NETWORK_CREG_STATE_NOT_SEARCHING_EMERGENCY_CALLS = 10;
const NETWORK_CREG_STATE_SEARCHING_EMERGENCY_CALLS = 12;
const NETWORK_CREG_STATE_DENIED_EMERGENCY_CALLS = 13;
const NETWORK_CREG_STATE_UNKNOWN_EMERGENCY_CALLS = 14;

const NETWORK_CREG_TECH_UNKNOWN = 0;
const NETWORK_CREG_TECH_GPRS = 1;
const NETWORK_CREG_TECH_EDGE = 2;
const NETWORK_CREG_TECH_UMTS = 3;
const NETWORK_CREG_TECH_IS95A = 4;
const NETWORK_CREG_TECH_IS95B = 5;
const NETWORK_CREG_TECH_1XRTT = 6;
const NETWORK_CREG_TECH_EVDO0 = 7;
const NETWORK_CREG_TECH_EVDOA = 8;
const NETWORK_CREG_TECH_HSDPA = 9;
const NETWORK_CREG_TECH_HSUPA = 10;
const NETWORK_CREG_TECH_HSPA = 11;
const NETWORK_CREG_TECH_EVDOB = 12;
const NETWORK_CREG_TECH_EHRPD = 13;
const NETWORK_CREG_TECH_LTE = 14;
const NETWORK_CREG_TECH_HSPAP = 15;

const CALL_STATE_ACTIVE = 0;
const CALL_STATE_HOLDING = 1;
const CALL_STATE_DIALING = 2;
const CALL_STATE_ALERTING = 3;
const CALL_STATE_INCOMING = 4;
const CALL_STATE_WAITING = 5;
const CALL_STATE_BUSY = 6;

const TOA_INTERNATIONAL = 0x91;
const TOA_UNKNOWN = 0x81;

const CALL_PRESENTATION_ALLOWED = 0;
const CALL_PRESENTATION_RESTRICTED = 1;
const CALL_PRESENTATION_UNKNOWN = 2;
const CALL_PRESENTATION_PAYPHONE = 3;

// ICC commands, see TS 27.007 +CRSM commands
const ICC_COMMAND_SEEK = 0xa2;
const ICC_COMMAND_READ_BINARY = 0xb0;
const ICC_COMMAND_READ_RECORD = 0xb2;
const ICC_COMMAND_GET_RESPONSE = 0xc0;
const ICC_COMMAND_UPDATE_BINARY = 0xd6;
const ICC_COMMAND_UPDATE_RECORD = 0xdc;

// ICC constants, GSM SIM file ids from TS 51.011
const ICC_EF_ICCID  = 0x2fe2;
const ICC_EF_IMG    = 0x4f20;
const ICC_EF_PBR    = 0x4f30;
const ICC_EF_SST    = 0x6f38;
const ICC_EF_UST    = 0x6f38; // For USIM
const ICC_EF_ADN    = 0x6f3a;
const ICC_EF_FDN    = 0x6f3b;
const ICC_EF_SMS    = 0x6f3c;
const ICC_EF_MSISDN = 0x6f40;
const ICC_EF_SPN    = 0x6f46;
const ICC_EF_SDN    = 0x6f49;
const ICC_EF_EXT1   = 0x6f4a;
const ICC_EF_EXT2   = 0x6f4b;
const ICC_EF_EXT3   = 0x6f4c;
const ICC_EF_AD     = 0x6fad;
const ICC_EF_PNN    = 0x6fc5;
const ICC_EF_MBDN   = 0x6fc7;
const ICC_EF_EXT6   = 0x6fc8;   // Ext record for EF[MBDN]
const ICC_EF_MBI    = 0x6fc9;
const ICC_EF_MWIS   = 0x6fca;
const ICC_EF_CFIS   = 0x6fcb;
const ICC_EF_SPDI   = 0x6fcd;

// Types of files  TS 11.11 9.3
const TYPE_RFU = 0;
const TYPE_MF  = 1;
const TYPE_DF  = 2;
const TYPE_EF  = 4;

const RESPONSE_DATA_FILE_SIZE = 2;
const RESPONSE_DATA_FILE_ID_1 = 4;
const RESPONSE_DATA_FILE_ID_2 = 5;
const RESPONSE_DATA_FILE_TYPE = 6;
const RESPONSE_DATA_RFU_3 = 7;
const RESPONSE_DATA_ACCESS_CONDITION_1 = 8;
const RESPONSE_DATA_ACCESS_CONDITION_2 = 9;
const RESPONSE_DATA_ACCESS_CONDITION_3 = 10;
const RESPONSE_DATA_FILE_STATUS = 11;
const RESPONSE_DATA_LENGTH = 12;
const RESPONSE_DATA_STRUCTURE = 13;
const RESPONSE_DATA_RECORD_LENGTH = 14;

// Types of files  TS 11.11 9.3
const EF_TYPE_TRANSPARENT = 0;
const EF_TYPE_LINEAR_FIXED = 1;
const EF_TYPE_CYCLIC = 3;

// For retrieving MSISDN, TS 151.011 clause 10.5.5
const MSISDN_FOOTER_SIZE_BYTES = 14;
const MSISDN_MAX_NUMBER_SIZE_BYTES = 10;

// READ_RECORD mode,  TS 102.221
const READ_RECORD_ABSOLUTE_MODE = 4;

// GET_RESPONSE mandatory response size for EF, see TS 51.011 clause 9, 
// 'Response data in case of an EF.'
const GET_RESPONSE_EF_SIZE_BYTES = 15;

// EF path
const EF_PATH_MF_SIM = "3f00";
const EF_PATH_DF_PHONEBOOK = "5f3a";
const EF_PATH_DF_TELECOM = "7f10";
const EF_PATH_DF_GSM = "7f20";

// Status code of sw1 for ICC I/O,
// see GSM11.11 and TS 51.011 clause 9.4, and ISO 7816-4
const ICC_STATUS_NORMAL_ENDING = 0x90;
const ICC_STATUS_NORMAL_ENDING_WITH_EXTRA = 0x91;
const ICC_STATUS_WITH_SIM_DATA = 0x9e;
const ICC_STATUS_WITH_RESPONSE_DATA = 0x9f;
const ICC_STATUS_ERROR_WRONG_LENGTH = 0x67;
const ICC_STATUS_ERROR_COMMAND_NOT_ALLOWED = 0x69;
const ICC_STATUS_ERROR_WRONG_PARAMETERS = 0x6a;

// ICC call barring facility.
// TS 27.007, clause 7.4, +CLCK
const ICC_CB_FACILITY_SIM = "SC";

// ICC service class
// TS 27.007, clause 7.4, +CLCK
const ICC_SERVICE_CLASS_NONE = 0; // no user input
const ICC_SERVICE_CLASS_VOICE = (1 << 0);
const ICC_SERVICE_CLASS_DATA = (1 << 1);
const ICC_SERVICE_CLASS_FAX = (1 << 2);
const ICC_SERVICE_CLASS_SMS = (1 << 3);
const ICC_SERVICE_CLASS_DATA_SYNC = (1 << 4);
const ICC_SERVICE_CLASS_DATA_ASYNC = (1 << 5);
const ICC_SERVICE_CLASS_PACKET = (1 << 6);
const ICC_SERVICE_CLASS_PAD = (1 << 7);
const ICC_SERVICE_CLASS_MAX = (1 << 7); // Max ICC_SERVICE_CLASS value

const ICC_USIM_TYPE1_TAG   = 0xa8;
const ICC_USIM_TYPE2_TAG   = 0xa9;
const ICC_USIM_TYPE3_TAG   = 0xaa;
const ICC_USIM_EFADN_TAG   = 0xc0;
const ICC_USIM_EFIAP_TAG   = 0xc1;
const ICC_USIM_EFEXT1_TAG  = 0xc2;
const ICC_USIM_EFSNE_TAG   = 0xc3;
const ICC_USIM_EFANR_TAG   = 0xc4;
const ICC_USIM_EFPBC_TAG   = 0xc5;
const ICC_USIM_EFGRP_TAG   = 0xc6;
const ICC_USIM_EFAAS_TAG   = 0xc7;
const ICC_USIM_EFGSD_TAG   = 0xc8;
const ICC_USIM_EFUID_TAG   = 0xc9;
const ICC_USIM_EFEMAIL_TAG = 0xca;
const ICC_USIM_EFCCP1_TAG  = 0xcb;

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
const PDU_TOA_SUBSCRIBER    = 0xC0; // Subscriber number. This is used when a
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
const PDU_MTI_SMS_RESERVED        = 0x03;
const PDU_MTI_SMS_STATUS_REPORT   = 0x02;
const PDU_MTI_SMS_COMMAND         = 0x02;
const PDU_MTI_SMS_SUBMIT          = 0x01;
const PDU_MTI_SMS_DELIVER         = 0x00;

// PI - Parameter Indicator
const PDU_PI_EXTENSION           = 0x80;
const PDU_PI_USER_DATA_LENGTH    = 0x04;
const PDU_PI_DATA_CODING_SCHEME  = 0x02;
const PDU_PI_PROTOCOL_IDENTIFIER = 0x01;
const PDU_PI_RESERVED            = 0x78;

// FCS - Failure Cause
const PDU_FCS_OK          = 0x00;
const PDU_FCS_UNSPECIFIED = 0xFF;

// ST - Status
// Bit 7..0 = 000xxxxx, short message transaction completed
const PDU_ST_0_RECEIVED             = 0x00;
const PDU_ST_0_FORWARDED_NO_CONFIRM = 0x01;
const PDU_ST_0_REPLACED_BY_SC       = 0x02;
const PDU_ST_0_RESERVED_BEGIN       = 0x03;
const PDU_ST_0_SC_SPECIFIC_BEGIN    = 0x10;
const PDU_ST_0_SC_SPECIFIC_END      = 0x1F;
// Bit 7..0 = 001xxxxx, temporary error, SC still trying to transfer SM
const PDU_ST_1_CONGESTION        = 0x20;
const PDU_ST_1_SME_BUSY          = 0x21;
const PDU_ST_1_SME_NO_RESPONSE   = 0x22;
const PDU_ST_1_SERVICE_REJECTED  = 0x23;
const PDU_ST_1_QOS_UNAVAILABLE   = 0x24;
const PDU_ST_1_SME_ERROR         = 0x25;
const PDU_ST_1_RESERVED_BEGIN    = 0x26;
const PDU_ST_1_SC_SPECIFIC_BEGIN = 0x30;
const PDU_ST_1_SC_SPECIFIC_END   = 0x3F;
// Bit 7..0 = 010xxxxx, permanent error, SC is not making any more transfer
// attempts
const PDU_ST_2_RPC_ERROR                = 0x40;
const PDU_ST_2_DEST_INCOMPATIBLE        = 0x41;
const PDU_ST_2_CONNECTION_REJECTED      = 0x42;
const PDU_ST_2_NOT_OBTAINABLE           = 0x43;
const PDU_ST_2_QOS_UNAVAILABLE          = 0x44;
const PDU_ST_2_INTERWORKING_UNAVALIABLE = 0x45;
const PDU_ST_2_VALIDITY_EXPIRED         = 0x46;
const PDU_ST_2_DELETED_BY_SME           = 0x47;
const PDU_ST_2_DELETED_BY_SC            = 0x48;
const PDU_ST_2_SM_MISSING               = 0x49;
const PDU_ST_2_RESERVED_BEGIN           = 0x4A;
const PDU_ST_2_SC_SPECIFIC_BEGIN        = 0x50;
const PDU_ST_2_SC_SPECIFIC_END          = 0x5F;
// Bit 7..0 = 011xxxxx, temporary error, SC is not making any more transfer
// attempts
const PDU_ST_3_CONGESTION        = 0x60;
const PDU_ST_3_SME_BUSY          = 0x61;
const PDU_ST_3_SME_NO_RESPONSE   = 0x62;
const PDU_ST_3_SERVICE_REJECTED  = 0x63;
const PDU_ST_3_QOS_UNAVAILABLE   = 0x64;
const PDU_ST_3_SME_ERROR         = 0x65;
const PDU_ST_3_RESERVED_BEGIN    = 0x66;
const PDU_ST_3_SC_SPECIFIC_BEGIN = 0x70;
const PDU_ST_3_SC_SPECIFIC_END   = 0x7F;

// User Data max length in septets
const PDU_MAX_USER_DATA_7BIT = 160;
// User Data max length in octets
const PDU_MAX_USER_DATA_8BIT = 140;
// User Data max length in chars
const PDU_MAX_USER_DATA_UCS2 = 70;

// PID - Protocol Indicator
const PDU_PID_DEFAULT                      = 0x00;
const PDU_PID_TELEMATIC_INTERWORKING       = 0x20;
const PDU_PID_SHORT_MESSAGE_TYPE_0         = 0x40;
const PDU_PID_REPLACE_SHORT_MESSAGE_TYPE_1 = 0x41;
const PDU_PID_REPLACE_SHORT_MESSAGE_TYPE_2 = 0x42;
const PDU_PID_REPLACE_SHORT_MESSAGE_TYPE_3 = 0x43;
const PDU_PID_REPLACE_SHORT_MESSAGE_TYPE_4 = 0x44;
const PDU_PID_REPLACE_SHORT_MESSAGE_TYPE_5 = 0x45;
const PDU_PID_REPLACE_SHORT_MESSAGE_TYPE_6 = 0x46;
const PDU_PID_REPLACE_SHORT_MESSAGE_TYPE_7 = 0x47;
const PDU_PID_ENHANDED_MESSAGE_SERVICE     = 0x5E;
const PDU_PID_RETURN_CALL_MESSAGE          = 0x5F
const PDU_PID_ANSI_136_R_DATA              = 0x7C;
const PDU_PID_ME_DATA_DOWNLOAD             = 0x7D;
const PDU_PID_ME_DEPERSONALIZATION         = 0x7E;
const PDU_PID_USIM_DATA_DOWNLOAD           = 0x7F;

// DCS - Data Coding Scheme
const PDU_DCS_MSG_CODING_7BITS_ALPHABET  = 0x00;
const PDU_DCS_MSG_CODING_8BITS_ALPHABET  = 0x04;
const PDU_DCS_MSG_CODING_16BITS_ALPHABET = 0x08;
const PDU_DCS_MSG_CLASS_ME_SPECIFIC      = 0xF1;
const PDU_DCS_MSG_CLASS_SIM_SPECIFIC     = 0xF2;
const PDU_DCS_MSG_CLASS_TE_SPECIFIC      = 0xF3;
const PDU_DCS_CODING_GROUP_BITS          = 0xF0;
const PDU_DCS_CODING_GROUP_7BITS_DISCARD = 0xC0;
const PDU_DCS_CODING_GROUP_7BITS_STORE   = 0xD0;
const PDU_DCS_CODING_GROUP_16BITS_STORE  = 0xE0;
const PDU_DCS_MWI_ACTIVE_BITS            = 0x08;
const PDU_DCS_MWI_ACTIVE_VALUE           = 0x08;
const PDU_DCS_MWI_TYPE_BITS              = 0x03;
const PDU_DCS_MWI_TYPE_VOICEMAIL         = 0x00;
const PDU_DCS_MWI_TYPE_FAX               = 0x01;
const PDU_DCS_MWI_TYPE_EMAIL             = 0x02;
const PDU_DCS_MWI_TYPE_OTHER             = 0x03;

// Because service center timestamp omit the century. Yay.
const PDU_TIMESTAMP_YEAR_OFFSET = 2000;

// See 9.2.3.24 TP‑User Data (TP‑UD)
const PDU_IEI_CONCATENATED_SHORT_MESSAGES_8BIT         = 0x00;
const PDU_IEI_SPECIAL_SMS_MESSAGE_INDICATION           = 0x01;
const PDU_IEI_APPLICATION_PORT_ADDRESSING_SCHEME_8BIT  = 0x04;
const PDU_IEI_APPLICATION_PORT_ADDRESSING_SCHEME_16BIT = 0x05;
const PDU_IEI_SMSC_CONTROL_PARAMS                      = 0x06;
const PDU_IEI_UDH_SOURCE_INDICATOR                     = 0x07;
const PDU_IEI_CONCATENATED_SHORT_MESSAGES_16BIT        = 0x08;
const PDU_IEI_WIRELESS_CONTROL_MESSAGE_PROTOCOL        = 0x09;
const PDU_IEI_TEXT_FORMATING                           = 0x0A;
const PDU_IEI_PREDEFINED_SOUND                         = 0x0B;
const PDU_IEI_USER_DATA_SOUND                          = 0x0C;
const PDU_IEI_PREDEFINED_ANIMATION                     = 0x0D;
const PDU_IEI_LARGE_ANIMATION                          = 0x0E;
const PDU_IEI_SMALL_ANIMATION                          = 0x0F;
const PDU_IEI_LARGE_PICTURE                            = 0x10;
const PDU_IEI_SMALL_PICTURE                            = 0x11;
const PDU_IEI_VARIABLE_PICTURE                         = 0x12;
const PDU_IEI_USER_PROMPT_INDICATOR                    = 0x13;
const PDU_IEI_EXTENDED_OBJECT                          = 0x14;
const PDU_IEI_REUSED_EXTENDED_OBJECT                   = 0x15;
const PDU_IEI_COMPRESS_CONTROL                         = 0x16;
const PDU_IEI_OBJECT_DISTRIBUTION_INDICATOR            = 0x17;
const PDU_IEI_STANDARD_WVG_OBJECT                      = 0x18;
const PDU_IEI_CHARACTER_SIZE_WVG_OBJECT                = 0x19;
const PDU_IEI_EXTENDED_OBJECT_DATA_REQUEST_COMMAND     = 0x1A;
const PDU_IEI_RFC822_EMAIL_HEADER                      = 0x20;
const PDU_IEI_HYPERLINK_FORMAT_ELEMENT                 = 0x21;
const PDU_IEI_REPLY_ADDRESS_ELEMENT                    = 0x22;
const PDU_IEI_ENHANCED_VOICE_MAIL_INFORMATION          = 0x23;
const PDU_IEI_NATIONAL_LANGUAGE_SINGLE_SHIFT           = 0x24;
const PDU_IEI_NATIONAL_LANGUAGE_LOCKING_SHIFT          = 0x25;

// Application Port Addressing, see 3GPP TS 23.040 9.2.3.24.3
const PDU_APA_RESERVED_8BIT_PORTS = 240;
const PDU_APA_VALID_16BIT_PORTS   = 49152;

// 7bit alphabet escape character. The encoded value of this code point is left
// undefined in official spec. Its code value is internally assigned to \uffff,
// <noncharacter-FFFF> in Unicode basic multilingual plane.
const PDU_NL_EXTENDED_ESCAPE = 0x1B;

// <SP>, <LF>, <CR> are only defined in locking shift tables.
const PDU_NL_SPACE = 0x20;
const PDU_NL_LINE_FEED = 0x0A;
const PDU_NL_CARRIAGE_RETURN = 0x0D;

// 7bit alphabet page break character, only defined in single shift tables.
// The encoded value of this code point is left undefined in official spec, but
// the code point itself maybe be used for example in compressed CBS messages.
// Its code value is internally assigned to \u000c, ASCII form feed, or new page.
const PDU_NL_PAGE_BREAK = 0x0A;
// 7bit alphabet reserved control character, only defined in single shift
// tables. The encoded value of this code point is left undefined in official
// spec. Its code value is internally assigned to \ufffe, <noncharacter-FFFE>
// in Unicode basic multilingual plane.
const PDU_NL_RESERVED_CONTROL = 0x0D;

const PDU_NL_IDENTIFIER_DEFAULT    = 0;
const PDU_NL_IDENTIFIER_TURKISH    = 1;
const PDU_NL_IDENTIFIER_SPANISH    = 2;
const PDU_NL_IDENTIFIER_PORTUGUESE = 3;
const PDU_NL_IDENTIFIER_BENGALI    = 4;
const PDU_NL_IDENTIFIER_GUJARATI   = 5;
const PDU_NL_IDENTIFIER_HINDI      = 6;
const PDU_NL_IDENTIFIER_KANNADA    = 7;
const PDU_NL_IDENTIFIER_MALAYALAM  = 8;
const PDU_NL_IDENTIFIER_ORIYA      = 9;
const PDU_NL_IDENTIFIER_PUNJABI    = 10;
const PDU_NL_IDENTIFIER_TAMIL      = 11;
const PDU_NL_IDENTIFIER_TELUGU     = 12;
const PDU_NL_IDENTIFIER_URDU       = 13;

// National Language Locking Shift Tables, see 3GPP TS 23.038
const PDU_NL_LOCKING_SHIFT_TABLES = [
  /**
   * National Language Identifier: 0x00
   * 6.2.1 GSM 7 bit Default Alphabet
   */
  // 01.....23.....4.....5.....6.....7.....8.....9.....A.B.....C.....D.E.....F.....
    "@\u00a3$\u00a5\u00e8\u00e9\u00f9\u00ec\u00f2\u00c7\n\u00d8\u00f8\r\u00c5\u00e5"
  // 0.....12.....3.....4.....5.....6.....7.....8.....9.....A.....B.....C.....D.....E.....F.....
  + "\u0394_\u03a6\u0393\u039b\u03a9\u03a0\u03a8\u03a3\u0398\u039e\uffff\u00c6\u00e6\u00df\u00c9"
  // 012.34.....56789ABCDEF
  + " !\"#\u00a4%&'()*+,-./"
  // 0123456789ABCDEF
  + "0123456789:;<=>?"
  // 0.....123456789ABCDEF
  + "\u00a1ABCDEFGHIJKLMNO"
  // 0123456789AB.....C.....D.....E.....F.....
  + "PQRSTUVWXYZ\u00c4\u00d6\u00d1\u00dc\u00a7"
  // 0.....123456789ABCDEF
  + "\u00bfabcdefghijklmno"
  // 0123456789AB.....C.....D.....E.....F.....
  + "pqrstuvwxyz\u00e4\u00f6\u00f1\u00fc\u00e0",

  /**
   * National Language Identifier: 0x01
   * A.3.1 Turkish National Language Locking Shift Table
   */
  // 01.....23.....4.....5.....6.....7.....8.....9.....A.B.....C.....D.E.....F.....
    "@\u00a3$\u00a5\u20ac\u00e9\u00f9\u0131\u00f2\u00c7\n\u011e\u011f\r\u00c5\u00e5"
  // 0.....12.....3.....4.....5.....6.....7.....8.....9.....A.....B.....C.....D.....E.....F.....
  + "\u0394_\u03a6\u0393\u039b\u03a9\u03a0\u03a8\u03a3\u0398\u039e\uffff\u015e\u015f\u00df\u00c9"
  // 012.34.....56789ABCDEF
  + " !\"#\u00a4%&'()*+,-./"
  // 0123456789ABCDEF
  + "0123456789:;<=>?"
  // 0.....123456789ABCDEF
  + "\u0130ABCDEFGHIJKLMNO"
  // 0123456789AB.....C.....D.....E.....F.....
  + "PQRSTUVWXYZ\u00c4\u00d6\u00d1\u00dc\u00a7"
  // 0.....123456789ABCDEF
  + "\u00e7abcdefghijklmno"
  // 0123456789AB.....C.....D.....E.....F.....
  + "pqrstuvwxyz\u00e4\u00f6\u00f1\u00fc\u00e0",

  /**
   * National Language Identifier: 0x02
   * A.3.2 Void
   */
  // 0123456789A.BCD.EF
    "          \n  \r  "
  // 0123456789AB.....CDEF
  + "           \uffff    "
  // 0123456789ABCDEF
  + "                "
  // 0123456789ABCDEF
  + "                "
  // 0123456789ABCDEF
  + "                "
  // 0123456789ABCDEF
  + "                "
  // 0123456789ABCDEF
  + "                "
  // 0123456789ABCDEF
  + "                ",

  /**
   * National Language Identifier: 0x03
   * A.3.3 Portuguese National Language Locking Shift Table
   */
  // 01.....23.....4.....5.....6.....7.....8.....9.....A.B.....C.....D.E.....F.....
    "@\u00a3$\u00a5\u00ea\u00e9\u00fa\u00ed\u00f3\u00e7\n\u00d4\u00f4\r\u00c1\u00e1"
  // 0.....12.....3.....4.....5.....67.8.....9.....AB.....C.....D.....E.....F.....
  + "\u0394_\u00aa\u00c7\u00c0\u221e^\\\u20ac\u00d3|\uffff\u00c2\u00e2\u00ca\u00c9"
  // 012.34.....56789ABCDEF
  + " !\"#\u00ba%&'()*+,-./"
  // 0123456789ABCDEF
  + "0123456789:;<=>?"
  // 0.....123456789ABCDEF
  + "\u00cdABCDEFGHIJKLMNO"
  // 0123456789AB.....C.....D.....E.....F.....
  + "PQRSTUVWXYZ\u00c3\u00d5\u00da\u00dc\u00a7"
  // 0123456789ABCDEF
  + "~abcdefghijklmno"
  // 0123456789AB.....C.....DE.....F.....
  + "pqrstuvwxyz\u00e3\u00f5`\u00fc\u00e0",

  /**
   * National Language Identifier: 0x04
   * A.3.4 Bengali National Language Locking Shift Table
   */
  // 0.....1.....2.....3.....4.....5.....6.....7.....8.....9.....A.B.....CD.EF.....
    "\u0981\u0982\u0983\u0985\u0986\u0987\u0988\u0989\u098a\u098b\n\u098c \r \u098f"
  // 0.....123.....4.....5.....6.....7.....8.....9.....A.....B.....C.....D.....E.....F.....
  + "\u0990  \u0993\u0994\u0995\u0996\u0997\u0998\u0999\u099a\uffff\u099b\u099c\u099d\u099e"
  // 012.....3.....4.....5.....6.....7.....89A.....B.....CD.....EF.....
  + " !\u099f\u09a0\u09a1\u09a2\u09a3\u09a4)(\u09a5\u09a6,\u09a7.\u09a8"
  // 0123456789ABCD.....E.....F
  + "0123456789:; \u09aa\u09ab?"
  // 0.....1.....2.....3.....4.....56.....789A.....B.....C.....D.....E.....F.....
  + "\u09ac\u09ad\u09ae\u09af\u09b0 \u09b2   \u09b6\u09b7\u09b8\u09b9\u09bc\u09bd"
  // 0.....1.....2.....3.....4.....5.....6.....789.....A.....BCD.....E.....F.....
  + "\u09be\u09bf\u09c0\u09c1\u09c2\u09c3\u09c4  \u09c7\u09c8  \u09cb\u09cc\u09cd"
  // 0.....123456789ABCDEF
  + "\u09ceabcdefghijklmno"
  // 0123456789AB.....C.....D.....E.....F.....
  + "pqrstuvwxyz\u09d7\u09dc\u09dd\u09f0\u09f1",

  /**
   * National Language Identifier: 0x05
   * A.3.5 Gujarati National Language Locking Shift Table
   */
  // 0.....1.....2.....3.....4.....5.....6.....7.....8.....9.....A.B.....C.....D.EF.....
    "\u0a81\u0a82\u0a83\u0a85\u0a86\u0a87\u0a88\u0a89\u0a8a\u0a8b\n\u0a8c\u0a8d\r \u0a8f"
  // 0.....1.....23.....4.....5.....6.....7.....8.....9.....A.....B.....C.....D.....E.....F.....
  + "\u0a90\u0a91 \u0a93\u0a94\u0a95\u0a96\u0a97\u0a98\u0a99\u0a9a\uffff\u0a9b\u0a9c\u0a9d\u0a9e"
  // 012.....3.....4.....5.....6.....7.....89A.....B.....CD.....EF.....
  + " !\u0a9f\u0aa0\u0aa1\u0aa2\u0aa3\u0aa4)(\u0aa5\u0aa6,\u0aa7.\u0aa8"
  // 0123456789ABCD.....E.....F
  + "0123456789:; \u0aaa\u0aab?"
  // 0.....1.....2.....3.....4.....56.....7.....89.....A.....B.....C.....D.....E.....F.....
  + "\u0aac\u0aad\u0aae\u0aaf\u0ab0 \u0ab2\u0ab3 \u0ab5\u0ab6\u0ab7\u0ab8\u0ab9\u0abc\u0abd"
  // 0.....1.....2.....3.....4.....5.....6.....7.....89.....A.....B.....CD.....E.....F.....
  + "\u0abe\u0abf\u0ac0\u0ac1\u0ac2\u0ac3\u0ac4\u0ac5 \u0ac7\u0ac8\u0ac9 \u0acb\u0acc\u0acd"
  // 0.....123456789ABCDEF
  + "\u0ad0abcdefghijklmno"
  // 0123456789AB.....C.....D.....E.....F.....
  + "pqrstuvwxyz\u0ae0\u0ae1\u0ae2\u0ae3\u0af1",

  /**
   * National Language Identifier: 0x06
   * A.3.6 Hindi National Language Locking Shift Table
   */
  // 0.....1.....2.....3.....4.....5.....6.....7.....8.....9.....A.B.....C.....D.E.....F.....
    "\u0901\u0902\u0903\u0905\u0906\u0907\u0908\u0909\u090a\u090b\n\u090c\u090d\r\u090e\u090f"
  // 0.....1.....2.....3.....4.....5.....6.....7.....8.....9.....A.....B.....C.....D.....E.....F.....
  + "\u0910\u0911\u0912\u0913\u0914\u0915\u0916\u0917\u0918\u0919\u091a\uffff\u091b\u091c\u091d\u091e"
  // 012.....3.....4.....5.....6.....7.....89A.....B.....CD.....EF.....
  + " !\u091f\u0920\u0921\u0922\u0923\u0924)(\u0925\u0926,\u0927.\u0928"
  // 0123456789ABC.....D.....E.....F
  + "0123456789:;\u0929\u092a\u092b?"
  // 0.....1.....2.....3.....4.....5.....6.....7.....8.....9.....A.....B.....C.....D.....E.....F.....
  + "\u092c\u092d\u092e\u092f\u0930\u0931\u0932\u0933\u0934\u0935\u0936\u0937\u0938\u0939\u093c\u093d"
  // 0.....1.....2.....3.....4.....5.....6.....7.....8.....9.....A.....B.....C.....D.....E.....F.....
  + "\u093e\u093f\u0940\u0941\u0942\u0943\u0944\u0945\u0946\u0947\u0948\u0949\u094a\u094b\u094c\u094d"
  // 0.....123456789ABCDEF
  + "\u0950abcdefghijklmno"
  // 0123456789AB.....C.....D.....E.....F.....
  + "pqrstuvwxyz\u0972\u097b\u097c\u097e\u097f",

  /**
   * National Language Identifier: 0x07
   * A.3.7 Kannada National Language Locking Shift Table
   */
  // 01.....2.....3.....4.....5.....6.....7.....8.....9.....A.B.....CD.E.....F.....
    " \u0c82\u0c83\u0c85\u0c86\u0c87\u0c88\u0c89\u0c8a\u0c8b\n\u0c8c \r\u0c8e\u0c8f"
  // 0.....12.....3.....4.....5.....6.....7.....8.....9.....A.....B.....C.....D.....E.....F.....
  + "\u0c90 \u0c92\u0c93\u0c94\u0c95\u0c96\u0c97\u0c98\u0c99\u0c9a\uffff\u0c9b\u0c9c\u0c9d\u0c9e"
  // 012.....3.....4.....5.....6.....7.....89A.....B.....CD.....EF.....
  + " !\u0c9f\u0ca0\u0ca1\u0ca2\u0ca3\u0ca4)(\u0ca5\u0ca6,\u0ca7.\u0ca8"
  // 0123456789ABCD.....E.....F
  + "0123456789:; \u0caa\u0cab?"
  // 0.....1.....2.....3.....4.....5.....6.....7.....89.....A.....B.....C.....D.....E.....F.....
  + "\u0cac\u0cad\u0cae\u0caf\u0cb0\u0cb1\u0cb2\u0cb3 \u0cb5\u0cb6\u0cb7\u0cb8\u0cb9\u0cbc\u0cbd"
  // 0.....1.....2.....3.....4.....5.....6.....78.....9.....A.....BC.....D.....E.....F.....
  + "\u0cbe\u0cbf\u0cc0\u0cc1\u0cc2\u0cc3\u0cc4 \u0cc6\u0cc7\u0cc8 \u0cca\u0ccb\u0ccc\u0ccd"
  // 0.....123456789ABCDEF
  + "\u0cd5abcdefghijklmno"
  // 0123456789AB.....C.....D.....E.....F.....
  + "pqrstuvwxyz\u0cd6\u0ce0\u0ce1\u0ce2\u0ce3",

  /**
   * National Language Identifier: 0x08
   * A.3.8 Malayalam National Language Locking Shift Table
   */
  // 01.....2.....3.....4.....5.....6.....7.....8.....9.....A.B.....CD.E.....F.....
    " \u0d02\u0d03\u0d05\u0d06\u0d07\u0d08\u0d09\u0d0a\u0d0b\n\u0d0c \r\u0d0e\u0d0f"
  // 0.....12.....3.....4.....5.....6.....7.....8.....9.....A.....B.....C.....D.....E.....F.....
  + "\u0d10 \u0d12\u0d13\u0d14\u0d15\u0d16\u0d17\u0d18\u0d19\u0d1a\uffff\u0d1b\u0d1c\u0d1d\u0d1e"
  // 012.....3.....4.....5.....6.....7.....89A.....B.....CD.....EF.....
  + " !\u0d1f\u0d20\u0d21\u0d22\u0d23\u0d24)(\u0d25\u0d26,\u0d27.\u0d28"
  // 0123456789ABCD.....E.....F
  + "0123456789:; \u0d2a\u0d2b?"
  // 0.....1.....2.....3.....4.....5.....6.....7.....8.....9.....A.....B.....C.....D.....EF.....
  + "\u0d2c\u0d2d\u0d2e\u0d2f\u0d30\u0d31\u0d32\u0d33\u0d34\u0d35\u0d36\u0d37\u0d38\u0d39 \u0d3d"
  // 0.....1.....2.....3.....4.....5.....6.....78.....9.....A.....BC.....D.....E.....F.....
  + "\u0d3e\u0d3f\u0d40\u0d41\u0d42\u0d43\u0d44 \u0d46\u0d47\u0d48 \u0d4a\u0d4b\u0d4c\u0d4d"
  // 0.....123456789ABCDEF
  + "\u0d57abcdefghijklmno"
  // 0123456789AB.....C.....D.....E.....F.....
  + "pqrstuvwxyz\u0d60\u0d61\u0d62\u0d63\u0d79",

  /**
   * National Language Identifier: 0x09
   * A.3.9 Oriya National Language Locking Shift Table
   */
  // 0.....1.....2.....3.....4.....5.....6.....7.....8.....9.....A.B.....CD.EF.....
    "\u0b01\u0b02\u0b03\u0b05\u0b06\u0b07\u0b08\u0b09\u0b0a\u0b0b\n\u0b0c \r \u0b0f"
  // 0.....123.....4.....5.....6.....7.....8.....9.....A.....B.....C.....D.....E.....F.....
  + "\u0b10  \u0b13\u0b14\u0b15\u0b16\u0b17\u0b18\u0b19\u0b1a\uffff\u0b1b\u0b1c\u0b1d\u0b1e"
  // 012.....3.....4.....5.....6.....7.....89A.....B.....CD.....EF.....
  + " !\u0b1f\u0b20\u0b21\u0b22\u0b23\u0b24)(\u0b25\u0b26,\u0b27.\u0b28"
  // 0123456789ABCD.....E.....F
  + "0123456789:; \u0b2a\u0b2b?"
  // 0.....1.....2.....3.....4.....56.....7.....89.....A.....B.....C.....D.....E.....F.....
  + "\u0b2c\u0b2d\u0b2e\u0b2f\u0b30 \u0b32\u0b33 \u0b35\u0b36\u0b37\u0b38\u0b39\u0b3c\u0b3d"
  // 0.....1.....2.....3.....4.....5.....6.....789.....A.....BCD.....E.....F.....
  + "\u0b3e\u0b3f\u0b40\u0b41\u0b42\u0b43\u0b44  \u0b47\u0b48  \u0b4b\u0b4c\u0b4d"
  // 0.....123456789ABCDEF
  + "\u0b56abcdefghijklmno"
  // 0123456789AB.....C.....D.....E.....F.....
  + "pqrstuvwxyz\u0b57\u0b60\u0b61\u0b62\u0b63",

  /**
   * National Language Identifier: 0x0A
   * A.3.10 Punjabi National Language Locking Shift Table
   */
  // 0.....1.....2.....3.....4.....5.....6.....7.....8.....9A.BCD.EF.....
    "\u0a01\u0a02\u0a03\u0a05\u0a06\u0a07\u0a08\u0a09\u0a0a \n  \r \u0a0f"
  // 0.....123.....4.....5.....6.....7.....8.....9.....A.....B.....C.....D.....E.....F.....
  + "\u0a10  \u0a13\u0a14\u0a15\u0a16\u0a17\u0a18\u0a19\u0a1a\uffff\u0a1b\u0a1c\u0a1d\u0a1e"
  // 012.....3.....4.....5.....6.....7.....89A.....B.....CD.....EF.....
  + " !\u0a1f\u0a20\u0a21\u0a22\u0a23\u0a24)(\u0a25\u0a26,\u0a27.\u0a28"
  // 0123456789ABCD.....E.....F
  + "0123456789:; \u0a2a\u0a2b?"
  // 0.....1.....2.....3.....4.....56.....7.....89.....A.....BC.....D.....E.....F
  + "\u0a2c\u0a2d\u0a2e\u0a2f\u0a30 \u0a32\u0a33 \u0a35\u0a36 \u0a38\u0a39\u0a3c "
  // 0.....1.....2.....3.....4.....56789.....A.....BCD.....E.....F.....
  + "\u0a3e\u0a3f\u0a40\u0a41\u0a42    \u0a47\u0a48  \u0a4b\u0a4c\u0a4d"
  // 0.....123456789ABCDEF
  + "\u0a51abcdefghijklmno"
  // 0123456789AB.....C.....D.....E.....F.....
  + "pqrstuvwxyz\u0a70\u0a71\u0a72\u0a73\u0a74",

  /**
   * National Language Identifier: 0x0B
   * A.3.11 Tamil National Language Locking Shift Table
   */
  // 01.....2.....3.....4.....5.....6.....7.....8.....9A.BCD.E.....F.....
    " \u0b82\u0b83\u0b85\u0b86\u0b87\u0b88\u0b89\u0b8a \n  \r\u0b8e\u0b8f"
  // 0.....12.....3.....4.....5.....6789.....A.....B.....CD.....EF.....
  + "\u0b90 \u0b92\u0b93\u0b94\u0b95   \u0b99\u0b9a\uffff \u0b9c \u0b9e"
  // 012.....3456.....7.....89ABCDEF.....
  + " !\u0b9f   \u0ba3\u0ba4)(  , .\u0ba8"
  // 0123456789ABC.....D.....EF
  + "0123456789:;\u0ba9\u0baa ?"
  // 012.....3.....4.....5.....6.....7.....8.....9.....A.....B.....C.....D.....EF
  + "  \u0bae\u0baf\u0bb0\u0bb1\u0bb2\u0bb3\u0bb4\u0bb5\u0bb6\u0bb7\u0bb8\u0bb9  "
  // 0.....1.....2.....3.....4.....5678.....9.....A.....BC.....D.....E.....F.....
  + "\u0bbe\u0bbf\u0bc0\u0bc1\u0bc2   \u0bc6\u0bc7\u0bc8 \u0bca\u0bcb\u0bcc\u0bcd"
  // 0.....123456789ABCDEF
  + "\u0bd0abcdefghijklmno"
  // 0123456789AB.....C.....D.....E.....F.....
  + "pqrstuvwxyz\u0bd7\u0bf0\u0bf1\u0bf2\u0bf9",

  /**
   * National Language Identifier: 0x0C
   * A.3.12 Telugu National Language Locking Shift Table
   */
  // 0.....1.....2.....3.....4.....5.....6.....7.....8.....9.....A.B.....CD.E.....F.....
    "\u0c01\u0c02\u0c03\u0c05\u0c06\u0c07\u0c08\u0c09\u0c0a\u0c0b\n\u0c0c \r\u0c0e\u0c0f"
  // 0.....12.....3.....4.....5.....6.....7.....8.....9.....A.....B.....C.....D.....E.....F.....
  + "\u0c10 \u0c12\u0c13\u0c14\u0c15\u0c16\u0c17\u0c18\u0c19\u0c1a\uffff\u0c1b\u0c1c\u0c1d\u0c1e"
  // 012.....3.....4.....5.....6.....7.....89A.....B.....CD.....EF.....
  + " !\u0c1f\u0c20\u0c21\u0c22\u0c23\u0c24)(\u0c25\u0c26,\u0c27.\u0c28"
  // 0123456789ABCD.....E.....F
  + "0123456789:; \u0c2a\u0c2b?"
  // 0.....1.....2.....3.....4.....5.....6.....7.....89.....A.....B.....C.....D.....EF.....
  + "\u0c2c\u0c2d\u0c2e\u0c2f\u0c30\u0c31\u0c32\u0c33 \u0c35\u0c36\u0c37\u0c38\u0c39 \u0c3d"
  // 0.....1.....2.....3.....4.....5.....6.....78.....9.....A.....BC.....D.....E.....F.....
  + "\u0c3e\u0c3f\u0c40\u0c41\u0c42\u0c43\u0c44 \u0c46\u0c47\u0c48 \u0c4a\u0c4b\u0c4c\u0c4d"
  // 0.....123456789ABCDEF
  + "\u0c55abcdefghijklmno"
  // 0123456789AB.....C.....D.....E.....F.....
  + "pqrstuvwxyz\u0c56\u0c60\u0c61\u0c62\u0c63",

  /**
   * National Language Identifier: 0x0D
   * A.3.13 Urdu National Language Locking Shift Table
   */
  // 0.....1.....2.....3.....4.....5.....6.....7.....8.....9.....A.B.....C.....D.E.....F.....
    "\u0627\u0622\u0628\u067b\u0680\u067e\u06a6\u062a\u06c2\u067f\n\u0679\u067d\r\u067a\u067c"
  // 0.....1.....2.....3.....4.....5.....6.....7.....8.....9.....A.....B.....C.....D.....E.....F.....
  + "\u062b\u062c\u0681\u0684\u0683\u0685\u0686\u0687\u062d\u062e\u062f\uffff\u068c\u0688\u0689\u068a"
  // 012.....3.....4.....5.....6.....7.....89A.....B.....CD.....EF.....
  + " !\u068f\u068d\u0630\u0631\u0691\u0693)(\u0699\u0632,\u0696.\u0698"
  // 0123456789ABC.....D.....E.....F
  + "0123456789:;\u069a\u0633\u0634?"
  // 0.....1.....2.....3.....4.....5.....6.....7.....8.....9.....A.....B.....C.....D.....E.....F.....
  + "\u0635\u0636\u0637\u0638\u0639\u0641\u0642\u06a9\u06aa\u06ab\u06af\u06b3\u06b1\u0644\u0645\u0646"
  // 0.....1.....2.....3.....4.....5.....6.....7.....8.....9.....A.....B.....C.....D.....E.....F.....
  + "\u06ba\u06bb\u06bc\u0648\u06c4\u06d5\u06c1\u06be\u0621\u06cc\u06d0\u06d2\u064d\u0650\u064f\u0657"
  // 0.....123456789ABCDEF
  + "\u0654abcdefghijklmno"
  // 0123456789AB.....C.....D.....E.....F.....
  + "pqrstuvwxyz\u0655\u0651\u0653\u0656\u0670"
];

// National Language Single Shift Tables, see 3GPP TS 23.038
const PDU_NL_SINGLE_SHIFT_TABLES = [
  /**
   * National Language Identifier: 0x00
   * 6.2.1.1 GSM 7 bit default alphabet extension table
   */
  // 0123456789A.....BCD.....EF
    "          \u000c  \ufffe  "
  // 0123456789AB.....CDEF
  + "    ^      \uffff    "
  // 0123456789ABCDEF.
  + "        {}     \\"
  // 0123456789ABCDEF
  + "            [~] "
  // 0123456789ABCDEF
  + "|               "
  // 0123456789ABCDEF
  + "                "
  // 012345.....6789ABCDEF
  + "     \u20ac          "
  // 0123456789ABCDEF
  + "                ",

  /**
   * National Language Identifier: 0x01
   * A.2.1 Turkish National Language Single Shift Table
   */
  // 0123456789A.....BCD.....EF
    "          \u000c  \ufffe  "
  // 0123456789AB.....CDEF
  + "    ^      \uffff    "
  // 0123456789ABCDEF.
  + "        {}     \\"
  // 0123456789ABCDEF
  + "            [~] "
  // 01234567.....89.....ABCDEF
  + "|      \u011e \u0130      "
  // 0123.....456789ABCDEF
  + "   \u015e            "
  // 0123.....45.....67.....89.....ABCDEF
  + "   \u00e7 \u20ac \u011f \u0131      "
  // 0123.....456789ABCDEF
  + "   \u015f            ",

  /**
   * National Language Identifier: 0x02
   * A.2.2 Spanish National Language Single Shift Table
   */
  // 0123456789.....A.....BCD.....EF
    "         \u00e7\u000c  \ufffe  "
  // 0123456789AB.....CDEF
  + "    ^      \uffff    "
  // 0123456789ABCDEF.
  + "        {}     \\"
  // 0123456789ABCDEF
  + "            [~] "
  // 01.....23456789.....ABCDEF.....
  + "|\u00c1       \u00cd     \u00d3"
  // 012345.....6789ABCDEF
  + "     \u00da          "
  // 01.....2345.....6789.....ABCDEF.....
  + " \u00e1   \u20ac   \u00ed     \u00f3"
  // 012345.....6789ABCDEF
  + "     \u00fa          ",

  /**
   * National Language Identifier: 0x03
   * A.2.3 Portuguese National Language Single Shift Table
   */
  // 012345.....6789.....A.....B.....C.....D.....E.....F.....
    "     \u00ea   \u00e7\u000c\u00d4\u00f4\ufffe\u00c1\u00e1"
  // 012.....3.....45.....6.....7.....8.....9.....AB.....CDEF.....
  + "  \u03a6\u0393^\u03a9\u03a0\u03a8\u03a3\u0398 \uffff   \u00ca"
  // 0123456789ABCDEF.
  + "        {}     \\"
  // 0123456789ABCDEF
  + "            [~] "
  // 01.....23456789.....ABCDEF.....
  + "|\u00c0       \u00cd     \u00d3"
  // 012345.....6789AB.....C.....DEF
  + "     \u00da     \u00c3\u00d5   "
  // 01.....2345.....6789.....ABCDEF.....
  + " \u00c2   \u20ac   \u00ed     \u00f3"
  // 012345.....6789AB.....C.....DEF.....
  + "     \u00fa     \u00e3\u00f5  \u00e2",

  /**
   * National Language Identifier: 0x04
   * A.2.4 Bengali National Language Single Shift Table
   */
  // 01.....23.....4.....5.6.....789A.....BCD.....EF
    "@\u00a3$\u00a5\u00bf\"\u00a4%&'\u000c*+\ufffe-/"
  // 0123.....45.....6789.....A.....B.....C.....D.....E.....F.....
  + "<=>\u00a1^\u00a1_#*\u09e6\u09e7\uffff\u09e8\u09e9\u09ea\u09eb"
  // 0.....1.....2.....3.....4.....5.....6.....7.....89A.....B.....C.....D.....E.....F.
  + "\u09ec\u09ed\u09ee\u09ef\u09df\u09e0\u09e1\u09e2{}\u09e3\u09f2\u09f3\u09f4\u09f5\\"
  // 0.....1.....2.....3.....4.....56789ABCDEF
  + "\u09f6\u09f7\u09f8\u09f9\u09fa       [~] "
  // 0123456789ABCDEF
  + "|ABCDEFGHIJKLMNO"
  // 0123456789ABCDEF
  + "PQRSTUVWXYZ     "
  // 012345.....6789ABCDEF
  + "     \u20ac          "
  // 0123456789ABCDEF
  + "                ",

  /**
   * National Language Identifier: 0x05
   * A.2.5 Gujarati National Language Single Shift Table
   */
  // 01.....23.....4.....5.6.....789A.....BCD.....EF
    "@\u00a3$\u00a5\u00bf\"\u00a4%&'\u000c*+\ufffe-/"
  // 0123.....45.....6789.....A.....B.....C.....D.....E.....F.....
  + "<=>\u00a1^\u00a1_#*\u0964\u0965\uffff\u0ae6\u0ae7\u0ae8\u0ae9"
  // 0.....1.....2.....3.....4.....5.....6789ABCDEF.
  + "\u0aea\u0aeb\u0aec\u0aed\u0aee\u0aef  {}     \\"
  // 0123456789ABCDEF
  + "            [~] "
  // 0123456789ABCDEF
  + "|ABCDEFGHIJKLMNO"
  // 0123456789ABCDEF
  + "PQRSTUVWXYZ     "
  // 012345.....6789ABCDEF
  + "     \u20ac          "
  // 0123456789ABCDEF
  + "                ",

  /**
   * National Language Identifier: 0x06
   * A.2.6 Hindi National Language Single Shift Table
   */
  // 01.....23.....4.....5.6.....789A.....BCD.....EF
    "@\u00a3$\u00a5\u00bf\"\u00a4%&'\u000c*+\ufffe-/"
  // 0123.....45.....6789.....A.....B.....C.....D.....E.....F.....
  + "<=>\u00a1^\u00a1_#*\u0964\u0965\uffff\u0966\u0967\u0968\u0969"
  // 0.....1.....2.....3.....4.....5.....6.....7.....89A.....B.....C.....D.....E.....F.
  + "\u096a\u096b\u096c\u096d\u096e\u096f\u0951\u0952{}\u0953\u0954\u0958\u0959\u095a\\"
  // 0.....1.....2.....3.....4.....5.....6.....7.....8.....9.....A.....BCDEF
  + "\u095b\u095c\u095d\u095e\u095f\u0960\u0961\u0962\u0963\u0970\u0971 [~] "
  // 0123456789ABCDEF
  + "|ABCDEFGHIJKLMNO"
  // 0123456789ABCDEF
  + "PQRSTUVWXYZ     "
  // 012345.....6789ABCDEF
  + "     \u20ac          "
  // 0123456789ABCDEF
  + "                ",

  /**
   * National Language Identifier: 0x07
   * A.2.7 Kannada National Language Single Shift Table
   */
  // 01.....23.....4.....5.6.....789A.....BCD.....EF
    "@\u00a3$\u00a5\u00bf\"\u00a4%&'\u000c*+\ufffe-/"
  // 0123.....45.....6789.....A.....B.....C.....D.....E.....F.....
  + "<=>\u00a1^\u00a1_#*\u0964\u0965\uffff\u0ce6\u0ce7\u0ce8\u0ce9"
  // 0.....1.....2.....3.....4.....5.....6.....7.....89A.....BCDEF.
  + "\u0cea\u0ceb\u0cec\u0ced\u0cee\u0cef\u0cde\u0cf1{}\u0cf2    \\"
  // 0123456789ABCDEF
  + "            [~] "
  // 0123456789ABCDEF
  + "|ABCDEFGHIJKLMNO"
  // 0123456789ABCDEF
  + "PQRSTUVWXYZ     "
  // 012345.....6789ABCDEF
  + "     \u20ac          "
  // 0123456789ABCDEF
  + "                ",

  /**
   * National Language Identifier: 0x08
   * A.2.8 Malayalam National Language Single Shift Table
   */
  // 01.....23.....4.....5.6.....789A.....BCD.....EF
    "@\u00a3$\u00a5\u00bf\"\u00a4%&'\u000c*+\ufffe-/"
  // 0123.....45.....6789.....A.....B.....C.....D.....E.....F.....
  + "<=>\u00a1^\u00a1_#*\u0964\u0965\uffff\u0d66\u0d67\u0d68\u0d69"
  // 0.....1.....2.....3.....4.....5.....6.....7.....89A.....B.....C.....D.....E.....F.
  + "\u0d6a\u0d6b\u0d6c\u0d6d\u0d6e\u0d6f\u0d70\u0d71{}\u0d72\u0d73\u0d74\u0d75\u0d7a\\"
  // 0.....1.....2.....3.....4.....56789ABCDEF
  + "\u0d7b\u0d7c\u0d7d\u0d7e\u0d7f       [~] "
  // 0123456789ABCDEF
  + "|ABCDEFGHIJKLMNO"
  // 0123456789ABCDEF
  + "PQRSTUVWXYZ     "
  // 012345.....6789ABCDEF
  + "     \u20ac          "
  // 0123456789ABCDEF
  + "                ",

  /**
   * National Language Identifier: 0x09
   * A.2.9 Oriya National Language Single Shift Table
   */
  // 01.....23.....4.....5.6.....789A.....BCD.....EF
    "@\u00a3$\u00a5\u00bf\"\u00a4%&'\u000c*+\ufffe-/"
  // 0123.....45.....6789.....A.....B.....C.....D.....E.....F.....
  + "<=>\u00a1^\u00a1_#*\u0964\u0965\uffff\u0b66\u0b67\u0b68\u0b69"
  // 0.....1.....2.....3.....4.....5.....6.....7.....89A.....B.....C.....DEF.
  + "\u0b6a\u0b6b\u0b6c\u0b6d\u0b6e\u0b6f\u0b5c\u0b5d{}\u0b5f\u0b70\u0b71  \\"
  // 0123456789ABCDEF
  + "            [~] "
  // 0123456789ABCDEF
  + "|ABCDEFGHIJKLMNO"
  // 0123456789ABCDEF
  + "PQRSTUVWXYZ     "
  // 012345.....6789ABCDEF
  + "     \u20ac          "
  // 0123456789ABCDEF
  + "                ",

  /**
   * National Language Identifier: 0x0A
   * A.2.10 Punjabi National Language Single Shift Table
   */
  // 01.....23.....4.....5.6.....789A.....BCD.....EF
    "@\u00a3$\u00a5\u00bf\"\u00a4%&'\u000c*+\ufffe-/"
  // 0123.....45.....6789.....A.....B.....C.....D.....E.....F.....
  + "<=>\u00a1^\u00a1_#*\u0964\u0965\uffff\u0a66\u0a67\u0a68\u0a69"
  // 0.....1.....2.....3.....4.....5.....6.....7.....89A.....B.....C.....D.....EF.
  + "\u0a6a\u0a6b\u0a6c\u0a6d\u0a6e\u0a6f\u0a59\u0a5a{}\u0a5b\u0a5c\u0a5e\u0a75 \\"
  // 0123456789ABCDEF
  + "            [~] "
  // 0123456789ABCDEF
  + "|ABCDEFGHIJKLMNO"
  // 0123456789ABCDEF
  + "PQRSTUVWXYZ     "
  // 012345.....6789ABCDEF
  + "     \u20ac          "
  // 0123456789ABCDEF
  + "                ",

  /**
   * National Language Identifier: 0x0B
   * A.2.11 Tamil National Language Single Shift Table
   */
  // 01.....23.....4.....5.6.....789A.....BCD.....EF
    "@\u00a3$\u00a5\u00bf\"\u00a4%&'\u000c*+\ufffe-/"
  // 0123.....45.....6789.....A.....B.....C.....D.....E.....F.....
  + "<=>\u00a1^\u00a1_#*\u0964\u0965\uffff\u0be6\u0be7\u0be8\u0be9"
  // 0.....1.....2.....3.....4.....5.....6.....7.....89A.....B.....C.....D.....E.....F.
  + "\u0bea\u0beb\u0bec\u0bed\u0bee\u0bef\u0bf3\u0bf4{}\u0bf5\u0bf6\u0bf7\u0bf8\u0bfa\\"
  // 0123456789ABCDEF
  + "            [~] "
  // 0123456789ABCDEF
  + "|ABCDEFGHIJKLMNO"
  // 0123456789ABCDEF
  + "PQRSTUVWXYZ     "
  // 012345.....6789ABCDEF
  + "     \u20ac          "
  // 0123456789ABCDEF
  + "                ",

  /**
   * National Language Identifier: 0x0C
   * A.2.12 Telugu National Language Single Shift Table
   */
  // 01.....23.....4.....5.6.....789A.....BCD.....EF
    "@\u00a3$\u00a5\u00bf\"\u00a4%&'\u000c*+\ufffe-/"
  // 0123.....45.....6789AB.....C.....D.....E.....F.....
  + "<=>\u00a1^\u00a1_#*  \uffff\u0c66\u0c67\u0c68\u0c69"
  // 0.....1.....2.....3.....4.....5.....6.....7.....89A.....B.....C.....D.....E.....F.
  + "\u0c6a\u0c6b\u0c6c\u0c6d\u0c6e\u0c6f\u0c58\u0c59{}\u0c78\u0c79\u0c7a\u0c7b\u0c7c\\"
  // 0.....1.....2.....3456789ABCDEF
  + "\u0c7d\u0c7e\u0c7f         [~] "
  // 0123456789ABCDEF
  + "|ABCDEFGHIJKLMNO"
  // 0123456789ABCDEF
  + "PQRSTUVWXYZ     "
  // 012345.....6789ABCDEF
  + "     \u20ac          "
  // 0123456789ABCDEF
  + "                ",

  /**
   * National Language Identifier: 0x0D
   * A.2.13 Urdu National Language Single Shift Table
   */
  // 01.....23.....4.....5.6.....789A.....BCD.....EF
    "@\u00a3$\u00a5\u00bf\"\u00a4%&'\u000c*+\ufffe-/"
  // 0123.....45.....6789.....A.....B.....C.....D.....E.....F.....
  + "<=>\u00a1^\u00a1_#*\u0600\u0601\uffff\u06f0\u06f1\u06f2\u06f3"
  // 0.....1.....2.....3.....4.....5.....6.....7.....89A.....B.....C.....D.....E.....F.
  + "\u06f4\u06f5\u06f6\u06f7\u06f8\u06f9\u060c\u060d{}\u060e\u060f\u0610\u0611\u0612\\"
  // 0.....1.....2.....3.....4.....5.....6.....7.....8.....9.....A.....B.....CDEF.....
  + "\u0613\u0614\u061b\u061f\u0640\u0652\u0658\u066b\u066c\u0672\u0673\u06cd[~]\u06d4"
  // 0123456789ABCDEF
  + "|ABCDEFGHIJKLMNO"
  // 0123456789ABCDEF
  + "PQRSTUVWXYZ     "
  // 012345.....6789ABCDEF
  + "     \u20ac          "
  // 0123456789ABCDEF
  + "                "
];

// Special SMS Message Indication constants
const PDU_MWI_STORE_TYPE_BIT     = 0x80;
const PDU_MWI_STORE_TYPE_DISCARD = 0x00;
const PDU_MWI_STORE_TYPE_STORE   = 0x80;

const RADIOTECH_FAMILY_3GPP = 1;  // GSM, WCDMA, LTE
const RADIOTECH_FAMILY_3GPP2 = 2; // CDMA, EVDO

const DATACALL_RADIOTECHNOLOGY_CDMA = 0;
const DATACALL_RADIOTECHNOLOGY_GSM = 1;

const DATACALL_AUTH_NONE = 0;
const DATACALL_AUTH_PAP = 1;
const DATACALL_AUTH_CHAP = 2;
const DATACALL_AUTH_PAP_OR_CHAP = 3;

const DATACALL_PROFILE_DEFAULT = 0;
const DATACALL_PROFILE_TETHERED = 1;
const DATACALL_PROFILE_OEM_BASE = 1000;

const DATACALL_DEACTIVATE_NO_REASON = 0;
const DATACALL_DEACTIVATE_RADIO_SHUTDOWN = 1;

const DATACALL_ACTIVE_UNKNOWN = -1;
const DATACALL_INACTIVE = 0;
const DATACALL_ACTIVE_DOWN = 1;
const DATACALL_ACTIVE_UP = 2;

const DATACALL_FAIL_NONE = 0;
const DATACALL_FAIL_OPERATOR_BARRED = 0x08;
const DATACALL_FAIL_INSUFFICIENT_RESOURCES = 0x1A;
const DATACALL_FAIL_MISSING_UKNOWN_APN = 0x1B;
const DATACALL_FAIL_UNKNOWN_PDP_ADDRESS_TYPE = 0x1C;
const DATACALL_FAIL_USER_AUTHENTICATION = 0x1D;
const DATACALL_FAIL_ACTIVATION_REJECT_GGSN = 0x1E;
const DATACALL_FAIL_ACTIVATION_REJECT_UNSPECIFIED = 0x1F;
const DATACALL_FAIL_SERVICE_OPTION_NOT_SUPPORTED = 0x20;
const DATACALL_FAIL_SERVICE_OPTION_NOT_SUBSCRIBED = 0x21;
const DATACALL_FAIL_SERVICE_OPTION_OUT_OF_ORDER = 0x22;
const DATACALL_FAIL_NSAPI_IN_USE = 0x23;
const DATACALL_FAIL_ONLY_IPV4_ALLOWED = 0x32;
const DATACALL_FAIL_ONLY_IPV6_ALLOWED = 0x33;
const DATACALL_FAIL_ONLY_SINGLE_BEARER_ALLOWED = 0x34;
const DATACALL_FAIL_PROTOCOL_ERRORS = 0x6F;
const DATACALL_FAIL_VOICE_REGISTRATION_FAIL = -1;
const DATACALL_FAIL_DATA_REGISTRATION_FAIL = -2;
const DATACALL_FAIL_SIGNAL_LOST = -3;
const DATACALL_FAIL_PREF_RADIO_TECH_CHANGED = -4;
const DATACALL_FAIL_RADIO_POWER_OFF = -5;
const DATACALL_FAIL_TETHERED_CALL_ACTIVE = -6;
const DATACALL_FAIL_ERROR_UNSPECIFIED = 0xffff;

// Keep consistent with nsINetworkManager.NETWORK_STATE_*.
const GECKO_NETWORK_STATE_UNKNOWN = -1;
const GECKO_NETWORK_STATE_CONNECTING = 0;
const GECKO_NETWORK_STATE_CONNECTED = 1;
const GECKO_NETWORK_STATE_SUSPENDED = 2;
const GECKO_NETWORK_STATE_DISCONNECTING = 3;
const GECKO_NETWORK_STATE_DISCONNECTED = 4;

// Used for QUERY_AVAILABLE_NETWORKS status of "unknown"
const GECKO_QAN_STATE_UNKNOWN = null;

const CALL_FAIL_UNOBTAINABLE_NUMBER = 1;
const CALL_FAIL_NORMAL = 16;
const CALL_FAIL_BUSY = 17;
const CALL_FAIL_CONGESTION = 34;
const CALL_FAIL_ACM_LIMIT_EXCEEDED = 68;
const CALL_FAIL_CALL_BARRED = 240;
const CALL_FAIL_FDN_BLOCKED = 241;
const CALL_FAIL_IMSI_UNKNOWN_IN_VLR = 242;
const CALL_FAIL_IMEI_NOT_ACCEPTED = 243;
const CALL_FAIL_ERROR_UNSPECIFIED = 0xffff;

// Other Gecko-specific constants
const GECKO_RADIOSTATE_UNAVAILABLE   = null;
const GECKO_RADIOSTATE_OFF           = "off";
const GECKO_RADIOSTATE_READY         = "ready";

const GECKO_CARDSTATE_UNAVAILABLE    = null;
const GECKO_CARDSTATE_ABSENT         = "absent";
const GECKO_CARDSTATE_PIN_REQUIRED   = "pin_required";
const GECKO_CARDSTATE_PUK_REQUIRED   = "puk_required";
const GECKO_CARDSTATE_NETWORK_LOCKED = "network_locked";
const GECKO_CARDSTATE_NOT_READY      = null;
const GECKO_CARDSTATE_READY          = "ready";

const GECKO_NETWORK_SELECTION_UNKNOWN   = null;
const GECKO_NETWORK_SELECTION_AUTOMATIC = "automatic";
const GECKO_NETWORK_SELECTION_MANUAL    = "manual";

const GECKO_CALL_ERROR_BAD_NUMBER             = "BadNumberError";
const GECKO_CALL_ERROR_NORMAL_CALL_CLEARING   = "NormalCallClearingError";
const GECKO_CALL_ERROR_BUSY                   = "BusyError";
const GECKO_CALL_ERROR_CONGESTION             = "CongestionError";
const GECKO_CALL_ERROR_INCOMING_CALL_EXCEEDED = "IncomingCallExceededError";
const GECKO_CALL_ERROR_BARRED                 = "BarredError";
const GECKO_CALL_ERROR_FDN_BLOCKED            = "FDNBlockedError";
const GECKO_CALL_ERROR_SUBSCRIBER_UNKNOWN     = "SubscriberUnknownError";
const GECKO_CALL_ERROR_DEVICE_NOT_ACCEPTED    = "DeviceNotAcceptedError";
const GECKO_CALL_ERROR_UNSPECIFIED            = "UnspecifiedError";

const RIL_CALL_FAILCAUSE_TO_GECKO_CALL_ERROR = {};
RIL_CALL_FAILCAUSE_TO_GECKO_CALL_ERROR[CALL_FAIL_UNOBTAINABLE_NUMBER] = GECKO_CALL_ERROR_BAD_NUMBER;
RIL_CALL_FAILCAUSE_TO_GECKO_CALL_ERROR[CALL_FAIL_NORMAL]              = GECKO_CALL_ERROR_NORMAL_CALL_CLEARING;
RIL_CALL_FAILCAUSE_TO_GECKO_CALL_ERROR[CALL_FAIL_BUSY]                = GECKO_CALL_ERROR_BUSY;
RIL_CALL_FAILCAUSE_TO_GECKO_CALL_ERROR[CALL_FAIL_CONGESTION]          = GECKO_CALL_ERROR_CONGESTION;
RIL_CALL_FAILCAUSE_TO_GECKO_CALL_ERROR[CALL_FAIL_ACM_LIMIT_EXCEEDED]  = GECKO_CALL_ERROR_INCOMING_CALL_EXCEEDED;
RIL_CALL_FAILCAUSE_TO_GECKO_CALL_ERROR[CALL_FAIL_CALL_BARRED]         = GECKO_CALL_ERROR_BARRED;
RIL_CALL_FAILCAUSE_TO_GECKO_CALL_ERROR[CALL_FAIL_FDN_BLOCKED]         = GECKO_CALL_ERROR_FDN_BLOCKED;
RIL_CALL_FAILCAUSE_TO_GECKO_CALL_ERROR[CALL_FAIL_IMSI_UNKNOWN_IN_VLR] = GECKO_CALL_ERROR_SUBSCRIBER_UNKNOWN;
RIL_CALL_FAILCAUSE_TO_GECKO_CALL_ERROR[CALL_FAIL_IMEI_NOT_ACCEPTED]   = GECKO_CALL_ERROR_DEVICE_NOT_ACCEPTED;
RIL_CALL_FAILCAUSE_TO_GECKO_CALL_ERROR[CALL_FAIL_ERROR_UNSPECIFIED]   = GECKO_CALL_ERROR_UNSPECIFIED;

const GECKO_RADIO_TECH = [
  null,
  "gprs",
  "edge",
  "umts",
  "is95a",
  "is95b",
  "1xrtt",
  "evdo0",
  "evdoa",
  "hsdpa",
  "hsupa",
  "hspa",
  "evdob",
  "ehrpd",
  "lte",
  "hspa+",
];

const GECKO_VOICEMAIL_MESSAGE_COUNT_UNKNOWN = -1;

// Allow this file to be imported via Components.utils.import().
const EXPORTED_SYMBOLS = Object.keys(this);
