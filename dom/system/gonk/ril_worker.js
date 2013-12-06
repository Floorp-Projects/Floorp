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

/**
 * This file implements the RIL worker thread. It communicates with
 * the main thread to provide a high-level API to the phone's RIL
 * stack, and with the RIL IPC thread to communicate with the RIL
 * device itself. These communication channels use message events as
 * known from Web Workers:
 *
 * - postMessage()/"message" events for main thread communication
 *
 * - postRILMessage()/"RILMessageEvent" events for RIL IPC thread
 *   communication.
 *
 * The two main objects in this file represent individual parts of this
 * communication chain:
 *
 * - RILMessageEvent -> Buf -> RIL -> postMessage() -> nsIRadioInterfaceLayer
 * - nsIRadioInterfaceLayer -> postMessage() -> RIL -> Buf -> postRILMessage()
 *
 * Note: The code below is purposely lean on abstractions to be as lean in
 * terms of object allocations. As a result, it may look more like C than
 * JavaScript, and that's intended.
 */

"use strict";

importScripts("ril_consts.js", "systemlibs.js");
importScripts("resource://gre/modules/workers/require.js");

// set to true in ril_consts.js to see debug messages
let DEBUG = DEBUG_WORKER;
let CLIENT_ID = -1;
let GLOBAL = this;

if (!this.debug) {
  // Debugging stub that goes nowhere.
  this.debug = function debug(message) {
    dump("RIL Worker[" + CLIENT_ID + "]: " + message + "\n");
  };
}

const DEFAULT_EMERGENCY_NUMBERS = ["112", "911"];

// Timeout value for emergency callback mode.
const EMERGENCY_CB_MODE_TIMEOUT_MS = 300000;  // 5 mins = 300000 ms.

const ICC_MAX_LINEAR_FIXED_RECORDS = 0xfe;

// MMI match groups
const MMI_MATCH_GROUP_FULL_MMI = 1;
const MMI_MATCH_GROUP_MMI_PROCEDURE = 2;
const MMI_MATCH_GROUP_SERVICE_CODE = 3;
const MMI_MATCH_GROUP_SIA = 5;
const MMI_MATCH_GROUP_SIB = 7;
const MMI_MATCH_GROUP_SIC = 9;
const MMI_MATCH_GROUP_PWD_CONFIRM = 11;
const MMI_MATCH_GROUP_DIALING_NUMBER = 12;

const MMI_MAX_LENGTH_SHORT_CODE = 2;

const MMI_END_OF_USSD = "#";

let RILQUIRKS_CALLSTATE_EXTRA_UINT32 = libcutils.property_get("ro.moz.ril.callstate_extra_int", "false") === "true";
// This may change at runtime since in RIL v6 and later, we get the version
// number via the UNSOLICITED_RIL_CONNECTED parcel.
let RILQUIRKS_V5_LEGACY = libcutils.property_get("ro.moz.ril.v5_legacy", "true") === "true";
let RILQUIRKS_REQUEST_USE_DIAL_EMERGENCY_CALL = libcutils.property_get("ro.moz.ril.dial_emergency_call", "false") === "true";
let RILQUIRKS_SIM_APP_STATE_EXTRA_FIELDS = libcutils.property_get("ro.moz.ril.simstate_extra_field", "false") === "true";
// Needed for call-waiting on Peak device
let RILQUIRKS_EXTRA_UINT32_2ND_CALL = libcutils.property_get("ro.moz.ril.extra_int_2nd_call", "false") == "true";
// On the emulator we support querying the number of lock retries
let RILQUIRKS_HAVE_QUERY_ICC_LOCK_RETRY_COUNT = libcutils.property_get("ro.moz.ril.query_icc_count", "false") == "true";

// Ril quirk to Send STK Profile Download
let RILQUIRKS_SEND_STK_PROFILE_DOWNLOAD = libcutils.property_get("ro.moz.ril.send_stk_profile_dl", "false") == "true";

// Marker object.
let PENDING_NETWORK_TYPE = {};

let Buf = {
  __proto__: (function(){
    return require("resource://gre/modules/workers/worker_buf.js").Buf;
  })(),

  mToken: 0,
  mTokenRequestMap: null,

  init: function init() {
    this._init();

    // This gets incremented each time we send out a parcel.
    this.mToken = 1;

    // Maps tokens we send out with requests to the request type, so that
    // when we get a response parcel back, we know what request it was for.
    this.mTokenRequestMap = {};
  },

  /**
   * Process one parcel.
   */
  processParcel: function processParcel() {
    let response_type = this.readInt32();

    let request_type, options;
    if (response_type == RESPONSE_TYPE_SOLICITED) {
      let token = this.readInt32();
      let error = this.readInt32();

      options = this.mTokenRequestMap[token];
      if (!options) {
        if (DEBUG) {
          debug("Suspicious uninvited request found: " + token + ". Ignored!");
        }
        return;
      }

      delete this.mTokenRequestMap[token];
      request_type = options.rilRequestType;

      options.rilRequestError = error;
      if (DEBUG) {
        debug("Solicited response for request type " + request_type +
              ", token " + token + ", error " + error);
      }
    } else if (response_type == RESPONSE_TYPE_UNSOLICITED) {
      request_type = this.readInt32();
      if (DEBUG) debug("Unsolicited response for request type " + request_type);
    } else {
      if (DEBUG) debug("Unknown response type: " + response_type);
      return;
    }

    RIL.handleParcel(request_type, this.readAvailable, options);
  },

  /**
   * Start a new outgoing parcel.
   *
   * @param type
   *        Integer specifying the request type.
   * @param options [optional]
   *        Object containing information about the request, e.g. the
   *        original main thread message object that led to the RIL request.
   */
  newParcel: function newParcel(type, options) {
    if (DEBUG) debug("New outgoing parcel of type " + type);

    // We're going to leave room for the parcel size at the beginning.
    this.outgoingIndex = this.PARCEL_SIZE_SIZE;
    this.writeInt32(type);
    this.writeInt32(this.mToken);

    if (!options) {
      options = {};
    }
    options.rilRequestType = type;
    options.rilRequestError = null;
    this.mTokenRequestMap[this.mToken] = options;
    this.mToken++;
    return this.mToken;
  },

  simpleRequest: function simpleRequest(type, options) {
    this.newParcel(type, options);
    this.sendParcel();
  },

  onSendParcel: function onSendParcel(parcel) {
    postRILMessage(CLIENT_ID, parcel);
  }
};

/**
 * The RIL state machine.
 *
 * This object communicates with rild via parcels and with the main thread
 * via post messages. It maintains state about the radio, ICC, calls, etc.
 * and acts upon state changes accordingly.
 */
let RIL = {
  /**
   * Valid calls.
   */
  currentCalls: {},

  /**
   * Existing conference call and its participants.
   */
  currentConference: {state: null, participants: {}},

  /**
   * Existing data calls.
   */
  currentDataCalls: {},

  /**
   * Hash map for received multipart sms fragments. Messages are hashed with
   * its sender address and concatenation reference number. Three additional
   * attributes `segmentMaxSeq`, `receivedSegments`, `segments` are inserted.
   */
  _receivedSmsSegmentsMap: {},

  /**
   * Outgoing messages waiting for SMS-STATUS-REPORT.
   */
  _pendingSentSmsMap: {},

  /**
   * Index of the RIL_PREFERRED_NETWORK_TYPE_TO_GECKO. Its value should be
   * preserved over rild reset.
   */
  preferredNetworkType: null,

  /**
   * Global Cell Broadcast switch.
   */
  cellBroadcastDisabled: false,

  /**
   * Global CLIR mode settings.
   */
  clirMode: CLIR_DEFAULT,

  /**
   * Parsed Cell Broadcast search lists.
   * cellBroadcastConfigs.MMI should be preserved over rild reset.
   */
  cellBroadcastConfigs: null,
  mergedCellBroadcastConfig: null,

  _receivedSmsCbPagesMap: {},

  initRILState: function initRILState() {
    /**
     * One of the RADIO_STATE_* constants.
     */
    this.radioState = GECKO_RADIOSTATE_UNAVAILABLE;
    this._isInitialRadioState = true;

    /**
     * True if we are on CDMA network.
     */
    this._isCdma = false;

    /**
     * True if we are in emergency callback mode.
     */
    this._isInEmergencyCbMode = false;

    /**
     * Set when radio is ready but radio tech is unknown. That is, we are
     * waiting for REQUEST_VOICE_RADIO_TECH
     */
    this._waitingRadioTech = false;

    /**
     * ICC status. Keeps a reference of the data response to the
     * getICCStatus request.
     */
    this.iccStatus = null;

    /**
     * Card state
     */
    this.cardState = GECKO_CARDSTATE_UNDETECTED;

    /**
     * Strings
     */
    this.IMEI = null;
    this.IMEISV = null;
    this.ESN = null;
    this.MEID = null;
    this.SMSC = null;

    /**
     * ICC information that is not exposed to Gaia.
     */
    this.iccInfoPrivate = {};

    /**
     * ICC information, such as MSISDN, MCC, MNC, SPN...etc.
     */
    this.iccInfo = {};

    /**
     * CDMA specific information. ex. CDMA Network ID, CDMA System ID... etc.
     */
    this.cdmaHome = null;

    /**
     * Application identification for apps in ICC.
     */
    this.aid = null;

    /**
     * Application type for apps in ICC.
     */
    this.appType = null;

    this.networkSelectionMode = null;

    this.voiceRegistrationState = {};
    this.dataRegistrationState = {};

    /**
     * List of strings identifying the network operator.
     */
    this.operator = null;

    /**
     * String containing the baseband version.
     */
    this.basebandVersion = null;

    // Clean up this.currentCalls: rild might have restarted.
    for each (let currentCall in this.currentCalls) {
      delete this.currentCalls[currentCall.callIndex];
      this._handleDisconnectedCall(currentCall);
    }

    // Deactivate this.currentDataCalls: rild might have restarted.
    for each (let datacall in this.currentDataCalls) {
      this.deactivateDataCall(datacall);
    }

    // Don't clean up this._receivedSmsSegmentsMap or this._pendingSentSmsMap
    // because on rild restart: we may continue with the pending segments.

    /**
     * Whether or not the multiple requests in requestNetworkInfo() are currently
     * being processed
     */
    this._processingNetworkInfo = false;

    /**
     * Multiple requestNetworkInfo() in a row before finishing the first
     * request, hence we need to fire requestNetworkInfo() again after
     * gathering all necessary stuffs. This is to make sure that ril_worker
     * gets precise network information.
     */
    this._needRepollNetworkInfo = false;

    /**
     * Pending messages to be send in batch from requestNetworkInfo()
     */
    this._pendingNetworkInfo = {rilMessageType: "networkinfochanged"};

    /**
     * USSD session flag.
     * Only one USSD session may exist at a time, and the session is assumed
     * to exist until:
     *    a) There's a call to cancelUSSD()
     *    b) The implementation sends a UNSOLICITED_ON_USSD with a type code
     *       of "0" (USSD-Notify/no further action) or "2" (session terminated)
     */
    this._ussdSession = null;

   /**
    * Regular expresion to parse MMI codes.
    */
    this._mmiRegExp = null;

    /**
     * Cell Broadcast Search Lists.
     */
    let cbmmi = this.cellBroadcastConfigs && this.cellBroadcastConfigs.MMI;
    this.cellBroadcastConfigs = {
      MMI: cbmmi || null
    };
    this.mergedCellBroadcastConfig = null;
  },

  /**
   * Parse an integer from a string, falling back to a default value
   * if the the provided value is not a string or does not contain a valid
   * number.
   *
   * @param string
   *        String to be parsed.
   * @param defaultValue [optional]
   *        Default value to be used.
   * @param radix [optional]
   *        A number that represents the numeral system to be used. Default 10.
   */
  parseInt: function RIL_parseInt(string, defaultValue, radix) {
    let number = parseInt(string, radix || 10);
    if (!isNaN(number)) {
      return number;
    }
    if (defaultValue === undefined) {
      defaultValue = null;
    }
    return defaultValue;
  },


  /**
   * Outgoing requests to the RIL. These can be triggered from the
   * main thread via messages that look like this:
   *
   *   {rilMessageType: "methodName",
   *    extra:          "parameters",
   *    go:             "here"}
   *
   * So if one of the following methods takes arguments, it takes only one,
   * an object, which then contains all of the parameters as attributes.
   * The "@param" documentation is to be interpreted accordingly.
   */

  /**
   * Retrieve the ICC's status.
   */
  getICCStatus: function getICCStatus() {
    Buf.simpleRequest(REQUEST_GET_SIM_STATUS);
  },

  /**
   * Helper function for unlocking ICC locks.
   */
  iccUnlockCardLock: function iccUnlockCardLock(options) {
    switch (options.lockType) {
      case GECKO_CARDLOCK_PIN:
        this.enterICCPIN(options);
        break;
      case GECKO_CARDLOCK_PIN2:
        this.enterICCPIN2(options);
        break;
      case GECKO_CARDLOCK_PUK:
        this.enterICCPUK(options);
        break;
      case GECKO_CARDLOCK_PUK2:
        this.enterICCPUK2(options);
        break;
      case GECKO_CARDLOCK_NCK:
      case GECKO_CARDLOCK_CCK: // Fall through.
      case GECKO_CARDLOCK_SPCK: {
        let type = GECKO_PERSO_LOCK_TO_CARD_PERSO_LOCK[options.lockType];
        this.enterDepersonalization(type, options.pin, options);
        break;
      }
      case GECKO_CARDLOCK_NCK_PUK:
      case GECKO_CARDLOCK_CCK_PUK: // Fall through.
      case GECKO_CARDLOCK_SPCK_PUK: {
        let type = GECKO_PERSO_LOCK_TO_CARD_PERSO_LOCK[options.lockType];
        this.enterDepersonalization(type, options.puk, options);
        break;
      }
      default:
        options.errorMsg = "Unsupported Card Lock.";
        options.success = false;
        this.sendChromeMessage(options);
    }
  },

  /**
   * Enter a PIN to unlock the ICC.
   *
   * @param pin
   *        String containing the PIN.
   * @param [optional] aid
   *        AID value.
   */
  enterICCPIN: function enterICCPIN(options) {
    Buf.newParcel(REQUEST_ENTER_SIM_PIN, options);
    Buf.writeInt32(RILQUIRKS_V5_LEGACY ? 1 : 2);
    Buf.writeString(options.pin);
    if (!RILQUIRKS_V5_LEGACY) {
      Buf.writeString(options.aid || this.aid);
    }
    Buf.sendParcel();
  },

  /**
   * Enter a PIN2 to unlock the ICC.
   *
   * @param pin
   *        String containing the PIN2.
   * @param [optional] aid
   *        AID value.
   */
  enterICCPIN2: function enterICCPIN2(options) {
    Buf.newParcel(REQUEST_ENTER_SIM_PIN2, options);
    Buf.writeInt32(RILQUIRKS_V5_LEGACY ? 1 : 2);
    Buf.writeString(options.pin);
    if (!RILQUIRKS_V5_LEGACY) {
      Buf.writeString(options.aid || this.aid);
    }
    Buf.sendParcel();
  },

  /**
   * Requests a network personalization be deactivated.
   *
   * @param type
   *        Integer indicating the network personalization be deactivated.
   * @param password
   *        String containing the password.
   */
  enterDepersonalization: function enterDepersonalization(type, password, options) {
    Buf.newParcel(REQUEST_ENTER_NETWORK_DEPERSONALIZATION_CODE, options);
    Buf.writeInt32(type);
    Buf.writeString(password);
    Buf.sendParcel();
  },

  /**
   * Helper function for changing ICC locks.
   */
  iccSetCardLock: function iccSetCardLock(options) {
    if (options.newPin !== undefined) { // Change PIN lock.
      switch (options.lockType) {
        case GECKO_CARDLOCK_PIN:
          this.changeICCPIN(options);
          break;
        case GECKO_CARDLOCK_PIN2:
          this.changeICCPIN2(options);
          break;
        default:
          options.errorMsg = "Unsupported Card Lock.";
          options.success = false;
          this.sendChromeMessage(options);
      }
    } else { // Enable/Disable lock.
      switch (options.lockType) {
        case GECKO_CARDLOCK_PIN:
          options.facility = ICC_CB_FACILITY_SIM;
          options.password = options.pin;
          break;
        case GECKO_CARDLOCK_FDN:
          options.facility = ICC_CB_FACILITY_FDN;
          options.password = options.pin2;
          break;
        default:
          options.errorMsg = "Unsupported Card Lock.";
          options.success = false;
          this.sendChromeMessage(options);
          return;
      }
      options.enabled = options.enabled;
      options.serviceClass = ICC_SERVICE_CLASS_VOICE |
                             ICC_SERVICE_CLASS_DATA  |
                             ICC_SERVICE_CLASS_FAX;
      this.setICCFacilityLock(options);
    }
  },

  /**
   * Change the current ICC PIN number.
   *
   * @param pin
   *        String containing the old PIN value
   * @param newPin
   *        String containing the new PIN value
   * @param [optional] aid
   *        AID value.
   */
  changeICCPIN: function changeICCPIN(options) {
    Buf.newParcel(REQUEST_CHANGE_SIM_PIN, options);
    Buf.writeInt32(RILQUIRKS_V5_LEGACY ? 2 : 3);
    Buf.writeString(options.pin);
    Buf.writeString(options.newPin);
    if (!RILQUIRKS_V5_LEGACY) {
      Buf.writeString(options.aid || this.aid);
    }
    Buf.sendParcel();
  },

  /**
   * Change the current ICC PIN2 number.
   *
   * @param pin
   *        String containing the old PIN2 value
   * @param newPin
   *        String containing the new PIN2 value
   * @param [optional] aid
   *        AID value.
   */
  changeICCPIN2: function changeICCPIN2(options) {
    Buf.newParcel(REQUEST_CHANGE_SIM_PIN2, options);
    Buf.writeInt32(RILQUIRKS_V5_LEGACY ? 2 : 3);
    Buf.writeString(options.pin);
    Buf.writeString(options.newPin);
    if (!RILQUIRKS_V5_LEGACY) {
      Buf.writeString(options.aid || this.aid);
    }
    Buf.sendParcel();
  },
  /**
   * Supplies ICC PUK and a new PIN to unlock the ICC.
   *
   * @param puk
   *        String containing the PUK value.
   * @param newPin
   *        String containing the new PIN value.
   * @param [optional] aid
   *        AID value.
   */
   enterICCPUK: function enterICCPUK(options) {
     Buf.newParcel(REQUEST_ENTER_SIM_PUK, options);
     Buf.writeInt32(RILQUIRKS_V5_LEGACY ? 2 : 3);
     Buf.writeString(options.puk);
     Buf.writeString(options.newPin);
     if (!RILQUIRKS_V5_LEGACY) {
       Buf.writeString(options.aid || this.aid);
     }
     Buf.sendParcel();
   },

  /**
   * Supplies ICC PUK2 and a new PIN2 to unlock the ICC.
   *
   * @param puk
   *        String containing the PUK2 value.
   * @param newPin
   *        String containing the new PIN2 value.
   * @param [optional] aid
   *        AID value.
   */
   enterICCPUK2: function enterICCPUK2(options) {
     Buf.newParcel(REQUEST_ENTER_SIM_PUK2, options);
     Buf.writeInt32(RILQUIRKS_V5_LEGACY ? 2 : 3);
     Buf.writeString(options.puk);
     Buf.writeString(options.newPin);
     if (!RILQUIRKS_V5_LEGACY) {
       Buf.writeString(options.aid || this.aid);
     }
     Buf.sendParcel();
   },

  /**
   * Helper function for fetching the state of ICC locks.
   */
  iccGetCardLockState: function iccGetCardLockState(options) {
    switch (options.lockType) {
      case GECKO_CARDLOCK_PIN:
        options.facility = ICC_CB_FACILITY_SIM;
        break;
      case GECKO_CARDLOCK_FDN:
        options.facility = ICC_CB_FACILITY_FDN;
        break;
      default:
        options.errorMsg = "Unsupported Card Lock.";
        options.success = false;
        this.sendChromeMessage(options);
        return;
    }

    options.password = ""; // For query no need to provide pin.
    options.serviceClass = ICC_SERVICE_CLASS_VOICE |
                           ICC_SERVICE_CLASS_DATA  |
                           ICC_SERVICE_CLASS_FAX;
    this.queryICCFacilityLock(options);
  },

  /**
   * Helper function for fetching the number of unlock retries of ICC locks.
   *
   * We only query the retry count when we're on the emulator. The phones do
   * not support the request id and their rild doesn't return an error.
   */
  iccGetCardLockRetryCount: function iccGetCardLockRetryCount(options) {
    var selCode = {
      pin: ICC_SEL_CODE_SIM_PIN,
      puk: ICC_SEL_CODE_SIM_PUK,
      pin2: ICC_SEL_CODE_SIM_PIN2,
      puk2: ICC_SEL_CODE_SIM_PUK2,
      nck: ICC_SEL_CODE_PH_NET_PIN,
      cck: ICC_SEL_CODE_PH_CORP_PIN,
      spck: ICC_SEL_CODE_PH_SP_PIN
    };

    if (typeof(selCode[options.lockType]) === 'undefined') {
      /* unknown lock type */
      options.errorMsg = GECKO_ERROR_GENERIC_FAILURE;
      options.success = false;
      this.sendChromeMessage(options);
      return;
    }

    if (RILQUIRKS_HAVE_QUERY_ICC_LOCK_RETRY_COUNT) {
      /* Only the emulator supports this request, ... */
      options.selCode = selCode[options.lockType];
      this.queryICCLockRetryCount(options);
    } else {
      /* ... while the phones do not. */
      options.errorMsg = GECKO_ERROR_REQUEST_NOT_SUPPORTED;
      options.success = false;
      this.sendChromeMessage(options);
    }
  },

  /**
   * Query ICC lock retry count.
   *
   * @param selCode
   *        One of ICC_SEL_CODE_*.
   * @param serviceClass
   *        One of ICC_SERVICE_CLASS_*.
   */
  queryICCLockRetryCount: function queryICCLockRetryCount(options) {
    Buf.newParcel(REQUEST_GET_UNLOCK_RETRY_COUNT, options);
    Buf.writeInt32(1);
    Buf.writeString(options.selCode);
    Buf.sendParcel();
  },

  /**
   * Query ICC facility lock.
   *
   * @param facility
   *        One of ICC_CB_FACILITY_*.
   * @param password
   *        Password for the facility, or "" if not required.
   * @param serviceClass
   *        One of ICC_SERVICE_CLASS_*.
   * @param [optional] aid
   *        AID value.
   */
  queryICCFacilityLock: function queryICCFacilityLock(options) {
    Buf.newParcel(REQUEST_QUERY_FACILITY_LOCK, options);
    Buf.writeInt32(RILQUIRKS_V5_LEGACY ? 3 : 4);
    Buf.writeString(options.facility);
    Buf.writeString(options.password);
    Buf.writeString(options.serviceClass.toString());
    if (!RILQUIRKS_V5_LEGACY) {
      Buf.writeString(options.aid || this.aid);
    }
    Buf.sendParcel();
  },

  /**
   * Set ICC facility lock.
   *
   * @param facility
   *        One of ICC_CB_FACILITY_*.
   * @param enabled
   *        true to enable, false to disable.
   * @param password
   *        Password for the facility, or "" if not required.
   * @param serviceClass
   *        One of ICC_SERVICE_CLASS_*.
   * @param [optional] aid
   *        AID value.
   */
  setICCFacilityLock: function setICCFacilityLock(options) {
    Buf.newParcel(REQUEST_SET_FACILITY_LOCK, options);
    Buf.writeInt32(RILQUIRKS_V5_LEGACY ? 4 : 5);
    Buf.writeString(options.facility);
    Buf.writeString(options.enabled ? "1" : "0");
    Buf.writeString(options.password);
    Buf.writeString(options.serviceClass.toString());
    if (!RILQUIRKS_V5_LEGACY) {
      Buf.writeString(options.aid || this.aid);
    }
    Buf.sendParcel();
  },

  /**
   *  Request an ICC I/O operation.
   *
   *  See TS 27.007 "restricted SIM" operation, "AT Command +CRSM".
   *  The sequence is in the same order as how libril reads this parcel,
   *  see the struct RIL_SIM_IO_v5 or RIL_SIM_IO_v6 defined in ril.h
   *
   *  @param command
   *         The I/O command, one of the ICC_COMMAND_* constants.
   *  @param fileId
   *         The file to operate on, one of the ICC_EF_* constants.
   *  @param pathId
   *         String type, check the 'pathid' parameter from TS 27.007 +CRSM.
   *  @param p1, p2, p3
   *         Arbitrary integer parameters for the command.
   *  @param [optional] dataWriter
   *         The function for writing string parameter for the ICC_COMMAND_UPDATE_RECORD.
   *  @param [optional] pin2
   *         String containing the PIN2.
   *  @param [optional] aid
   *         AID value.
   */
  iccIO: function iccIO(options) {
    Buf.newParcel(REQUEST_SIM_IO, options);
    Buf.writeInt32(options.command);
    Buf.writeInt32(options.fileId);
    Buf.writeString(options.pathId);
    Buf.writeInt32(options.p1);
    Buf.writeInt32(options.p2);
    Buf.writeInt32(options.p3);

    // Write data.
    if (options.command == ICC_COMMAND_UPDATE_RECORD &&
        options.dataWriter) {
      options.dataWriter(options.p3);
    } else {
      Buf.writeString(null);
    }

    // Write pin2.
    if (options.command == ICC_COMMAND_UPDATE_RECORD &&
        options.pin2) {
      Buf.writeString(options.pin2);
    } else {
      Buf.writeString(null);
    }

    if (!RILQUIRKS_V5_LEGACY) {
      Buf.writeString(options.aid || this.aid);
    }
    Buf.sendParcel();
  },

  /**
   * Get IMSI.
   *
   * @param [optional] aid
   *        AID value.
   */
  getIMSI: function getIMSI(aid) {
    if (RILQUIRKS_V5_LEGACY) {
      Buf.simpleRequest(REQUEST_GET_IMSI);
      return;
    }
    Buf.newParcel(REQUEST_GET_IMSI);
    Buf.writeInt32(1);
    Buf.writeString(aid || this.aid);
    Buf.sendParcel();
  },

  /**
   * Read UICC Phonebook contacts.
   *
   * @param contactType
   *        "adn" or "fdn".
   * @param requestId
   *        Request id from RadioInterfaceLayer.
   */
  readICCContacts: function readICCContacts(options) {
    if (!this.appType) {
      options.errorMsg = CONTACT_ERR_REQUEST_NOT_SUPPORTED;
      this.sendChromeMessage(options);
      return;
    }

    ICCContactHelper.readICCContacts(
      this.appType,
      options.contactType,
      function onsuccess(contacts) {
        for (let i = 0; i < contacts.length; i++) {
          let contact = contacts[i];
          let pbrIndex = contact.pbrIndex || 0;
          let recordIndex = pbrIndex * ICC_MAX_LINEAR_FIXED_RECORDS + contact.recordId;
          contact.contactId = this.iccInfo.iccid + recordIndex;
        }
        // Reuse 'options' to get 'requestId' and 'contactType'.
        options.contacts = contacts;
        RIL.sendChromeMessage(options);
      }.bind(this),
      function onerror(errorMsg) {
        options.errorMsg = errorMsg;
        RIL.sendChromeMessage(options);
      }.bind(this));
  },

  /**
   * Update UICC Phonebook.
   *
   * @param contactType   "adn" or "fdn".
   * @param contact       The contact will be updated.
   * @param pin2          PIN2 is required for updating FDN.
   * @param requestId     Request id from RadioInterfaceLayer.
   */
  updateICCContact: function updateICCContact(options) {
    let onsuccess = function onsuccess() {
      let recordIndex =
        contact.pbrIndex * ICC_MAX_LINEAR_FIXED_RECORDS + contact.recordId;
      contact.contactId = this.iccInfo.iccid + recordIndex;
      // Reuse 'options' to get 'requestId' and 'contactType'.
      RIL.sendChromeMessage(options);
    }.bind(this);

    let onerror = function onerror(errorMsg) {
      options.errorMsg = errorMsg;
      RIL.sendChromeMessage(options);
    }.bind(this);

    if (!this.appType || !options.contact) {
      onerror(CONTACT_ERR_REQUEST_NOT_SUPPORTED );
      return;
    }

    let contact = options.contact;
    let iccid = RIL.iccInfo.iccid;
    let isValidRecordId = false;
    if (typeof contact.contactId === "string" &&
        contact.contactId.startsWith(iccid)) {
      let recordIndex = contact.contactId.substring(iccid.length);
      contact.pbrIndex = Math.floor(recordIndex / ICC_MAX_LINEAR_FIXED_RECORDS);
      contact.recordId = recordIndex % ICC_MAX_LINEAR_FIXED_RECORDS;
      isValidRecordId = contact.recordId > 0 && contact.recordId < 0xff;
    }

    if (DEBUG) {
      debug("Update ICC Contact " + JSON.stringify(contact));
    }

    // If contact has 'recordId' property, updates corresponding record.
    // If not, inserts the contact into a free record.
    if (isValidRecordId) {
      ICCContactHelper.updateICCContact(
        this.appType, options.contactType, contact, options.pin2, onsuccess, onerror);
    } else {
      ICCContactHelper.addICCContact(
        this.appType, options.contactType, contact, options.pin2, onsuccess, onerror);
    }
  },

  /**
   * Request the phone's radio to be enabled or disabled.
   *
   * @param enabled
   *        Boolean indicating the desired state.
   */
  setRadioEnabled: function setRadioEnabled(options) {
    Buf.newParcel(REQUEST_RADIO_POWER, options);
    Buf.writeInt32(1);
    Buf.writeInt32(options.enabled ? 1 : 0);
    Buf.sendParcel();
  },

  /**
   * Query call waiting status via MMI.
   */
  _handleQueryMMICallWaiting: function _handleQueryMMICallWaiting(options) {
    function callback(options) {
      options.length = Buf.readInt32();
      options.enabled = (Buf.readInt32() === 1);
      let services = Buf.readInt32();
      if (options.enabled) {
        options.statusMessage = MMI_SM_KS_SERVICE_ENABLED_FOR;
        let serviceClass = [];
        for (let serviceClassMask = 1;
             serviceClassMask <= ICC_SERVICE_CLASS_MAX;
             serviceClassMask <<= 1) {
          if ((serviceClassMask & services) !== 0) {
            serviceClass.push(MMI_KS_SERVICE_CLASS_MAPPING[serviceClassMask]);
          }
        }
        options.additionalInformation = serviceClass;
      } else {
        options.statusMessage = MMI_SM_KS_SERVICE_DISABLED;
      }

      // Prevent DataCloneError when sending chrome messages.
      delete options.callback;
      this.sendChromeMessage(options);
    }

    options.callback = callback;
    this.queryCallWaiting(options);
  },

  /**
   * Set call waiting status via MMI.
   */
  _handleSetMMICallWaiting: function _handleSetMMICallWaiting(options) {
    function callback(options) {
      if (options.enabled) {
        options.statusMessage = MMI_SM_KS_SERVICE_ENABLED;
      } else {
        options.statusMessage = MMI_SM_KS_SERVICE_DISABLED;
      }

      // Prevent DataCloneError when sending chrome messages.
      delete options.callback;
      this.sendChromeMessage(options);
    }

    options.callback = callback;
    this.setCallWaiting(options);
  },

  /**
   * Query call waiting status.
   *
   */
  queryCallWaiting: function queryCallWaiting(options) {
    Buf.newParcel(REQUEST_QUERY_CALL_WAITING, options);
    Buf.writeInt32(1);
    // As per 3GPP TS 24.083, section 1.6 UE doesn't need to send service
    // class parameter in call waiting interrogation  to network
    Buf.writeInt32(ICC_SERVICE_CLASS_NONE);
    Buf.sendParcel();
  },

  /**
   * Set call waiting status.
   *
   * @param on
   *        Boolean indicating the desired waiting status.
   */
  setCallWaiting: function setCallWaiting(options) {
    Buf.newParcel(REQUEST_SET_CALL_WAITING, options);
    Buf.writeInt32(2);
    Buf.writeInt32(options.enabled ? 1 : 0);
    Buf.writeInt32(options.serviceClass !== undefined ?
                    options.serviceClass : ICC_SERVICE_CLASS_VOICE);
    Buf.sendParcel();
  },

  /**
   * Queries current CLIP status.
   *
   * (MMI request for code "*#30#")
   *
   */
  queryCLIP: function queryCLIP(options) {
    Buf.simpleRequest(REQUEST_QUERY_CLIP, options);
  },

  /**
   * Queries current CLIR status.
   *
   */
  getCLIR: function getCLIR(options) {
    Buf.simpleRequest(REQUEST_GET_CLIR, options);
  },

  /**
   * Enables or disables the presentation of the calling line identity (CLI) to
   * the called party when originating a call.
   *
   * @param options.clirMode
   *        Is one of the CLIR_* constants in
   *        nsIDOMMozMobileConnection interface.
   */
  setCLIR: function setCLIR(options) {
    if (options) {
      this.clirMode = options.clirMode;
    }
    Buf.newParcel(REQUEST_SET_CLIR, options);
    Buf.writeInt32(1);
    Buf.writeInt32(this.clirMode);
    Buf.sendParcel();
  },

  /**
   * Set screen state.
   *
   * @param on
   *        Boolean indicating whether the screen should be on or off.
   */
  setScreenState: function setScreenState(options) {
    Buf.newParcel(REQUEST_SCREEN_STATE);
    Buf.writeInt32(1);
    Buf.writeInt32(options.on ? 1 : 0);
    Buf.sendParcel();
  },

  getVoiceRegistrationState: function getVoiceRegistrationState() {
    Buf.simpleRequest(REQUEST_VOICE_REGISTRATION_STATE);
  },

  getVoiceRadioTechnology: function getVoiceRadioTechnology() {
    Buf.simpleRequest(REQUEST_VOICE_RADIO_TECH);
  },

  getDataRegistrationState: function getDataRegistrationState() {
    Buf.simpleRequest(REQUEST_DATA_REGISTRATION_STATE);
  },

  getOperator: function getOperator() {
    Buf.simpleRequest(REQUEST_OPERATOR);
  },

  /**
   * Set the preferred network type.
   *
   * @param options An object contains a valid index of
   *                RIL_PREFERRED_NETWORK_TYPE_TO_GECKO as its `networkType`
   *                attribute, or undefined to set current preferred network
   *                type.
   */
  setPreferredNetworkType: function setPreferredNetworkType(options) {
    if (options) {
      this.preferredNetworkType = options.networkType;
    }
    if (this.preferredNetworkType == null) {
      return;
    }

    Buf.newParcel(REQUEST_SET_PREFERRED_NETWORK_TYPE, options);
    Buf.writeInt32(1);
    Buf.writeInt32(this.preferredNetworkType);
    Buf.sendParcel();
  },

  /**
   * Get the preferred network type.
   */
  getPreferredNetworkType: function getPreferredNetworkType() {
    Buf.simpleRequest(REQUEST_GET_PREFERRED_NETWORK_TYPE);
  },

  /**
   * Request various states about the network.
   */
  requestNetworkInfo: function requestNetworkInfo() {
    if (this._processingNetworkInfo) {
      if (DEBUG) debug("Network info requested, but we're already requesting network info.");
      this._needRepollNetworkInfo = true;
      return;
    }

    if (DEBUG) debug("Requesting network info");

    this._processingNetworkInfo = true;
    this.getVoiceRegistrationState();
    this.getDataRegistrationState(); //TODO only GSM
    this.getOperator();
    this.getNetworkSelectionMode();
    this.getSignalStrength();
  },

  /**
   * Get the available networks
   */
  getAvailableNetworks: function getAvailableNetworks(options) {
    if (DEBUG) debug("Getting available networks");
    Buf.newParcel(REQUEST_QUERY_AVAILABLE_NETWORKS, options);
    Buf.sendParcel();
  },

  /**
   * Request the radio's network selection mode
   */
  getNetworkSelectionMode: function getNetworkSelectionMode() {
    if (DEBUG) debug("Getting network selection mode");
    Buf.simpleRequest(REQUEST_QUERY_NETWORK_SELECTION_MODE);
  },

  /**
   * Tell the radio to automatically choose a voice/data network
   */
  selectNetworkAuto: function selectNetworkAuto(options) {
    if (DEBUG) debug("Setting automatic network selection");
    Buf.simpleRequest(REQUEST_SET_NETWORK_SELECTION_AUTOMATIC, options);
  },

  /**
   * Set the roaming preference mode
   */
  setRoamingPreference: function setRoamingPreference(options) {
    let roamingMode = CDMA_ROAMING_PREFERENCE_TO_GECKO.indexOf(options.mode);

    if (roamingMode === -1) {
      options.errorMsg = GECKO_ERROR_INVALID_PARAMETER;
      this.sendChromeMessage(options);
      return;
    }

    Buf.newParcel(REQUEST_CDMA_SET_ROAMING_PREFERENCE, options);
    Buf.writeInt32(1);
    Buf.writeInt32(roamingMode);
    Buf.sendParcel();
  },

  /**
   * Get the roaming preference mode
   */
  queryRoamingPreference: function getRoamingPreference(options) {
    Buf.simpleRequest(REQUEST_CDMA_QUERY_ROAMING_PREFERENCE, options);
  },

  /**
   * Set the voice privacy mode
   */
  setVoicePrivacyMode: function setVoicePrivacyMode(options) {
    Buf.newParcel(REQUEST_CDMA_SET_PREFERRED_VOICE_PRIVACY_MODE, options);
    Buf.writeInt32(1);
    Buf.writeInt32(options.enabled ? 1 : 0);
    Buf.sendParcel();
  },

  /**
   * Get the voice privacy mode
   */
  queryVoicePrivacyMode: function getVoicePrivacyMode(options) {
    Buf.simpleRequest(REQUEST_CDMA_QUERY_PREFERRED_VOICE_PRIVACY_MODE, options);
  },

  /**
   * Open Logical UICC channel (aid) for Secure Element access
   */
  iccOpenChannel: function iccOpenChannel(options) {
    if (DEBUG) {
      debug("iccOpenChannel: " + JSON.stringify(options));
    }

    Buf.newParcel(REQUEST_SIM_OPEN_CHANNEL, options);
    Buf.writeString(options.aid);
    Buf.sendParcel();
  },

/**
   * Exchange APDU data on an open Logical UICC channel
   */
  iccExchangeAPDU: function iccExchangeAPDU(options) {
    if (DEBUG) debug("iccExchangeAPDU: " + JSON.stringify(options));

    let cla = options.apdu.cla;
    let command = options.apdu.command;
    let channel = options.channel;
    let path = options.apdu.path || "";
    let data = options.apdu.data || "";
    let data2 = options.apdu.data2 || "";

    let p1 = options.apdu.p1;
    let p2 = options.apdu.p2;
    let p3 = options.apdu.p3; // Extra

    Buf.newParcel(REQUEST_SIM_ACCESS_CHANNEL, options);
    Buf.writeInt32(cla);
    Buf.writeInt32(command);
    Buf.writeInt32(channel);
    Buf.writeString(path); // path
    Buf.writeInt32(p1);
    Buf.writeInt32(p2);
    Buf.writeInt32(p3);
    Buf.writeString(data); // generic data field.
    Buf.writeString(data2);

    Buf.sendParcel();
  },

  /**
   * Close Logical UICC channel
   */
  iccCloseChannel: function iccCloseChannel(options) {
    if (DEBUG) debug("iccCloseChannel: " + JSON.stringify(options));

    Buf.newParcel(REQUEST_SIM_CLOSE_CHANNEL, options);
    Buf.writeInt32(1);
    Buf.writeInt32(options.channel);
    Buf.sendParcel();
  },

  /**
   * Tell the radio to choose a specific voice/data network
   */
  selectNetwork: function selectNetwork(options) {
    if (DEBUG) {
      debug("Setting manual network selection: " + options.mcc + ", " + options.mnc);
    }

    let numeric = (options.mcc && options.mnc) ? options.mcc + options.mnc : null;
    Buf.newParcel(REQUEST_SET_NETWORK_SELECTION_MANUAL, options);
    Buf.writeString(numeric);
    Buf.sendParcel();
  },

  /**
   * Get current calls.
   */
  getCurrentCalls: function getCurrentCalls() {
    Buf.simpleRequest(REQUEST_GET_CURRENT_CALLS);
  },

  /**
   * Get the signal strength.
   */
  getSignalStrength: function getSignalStrength() {
    Buf.simpleRequest(REQUEST_SIGNAL_STRENGTH);
  },

  getIMEI: function getIMEI(options) {
    Buf.simpleRequest(REQUEST_GET_IMEI, options);
  },

  getIMEISV: function getIMEISV() {
    Buf.simpleRequest(REQUEST_GET_IMEISV);
  },

  getDeviceIdentity: function getDeviceIdentity() {
    Buf.simpleRequest(REQUEST_DEVICE_IDENTITY);
  },

  getBasebandVersion: function getBasebandVersion() {
    Buf.simpleRequest(REQUEST_BASEBAND_VERSION);
  },

  sendExitEmergencyCbModeRequest: function sendExitEmergencyCbModeRequest(options) {
    Buf.simpleRequest(REQUEST_EXIT_EMERGENCY_CALLBACK_MODE, options);
  },

  getCdmaSubscription: function getCdmaSubscription() {
    Buf.simpleRequest(REQUEST_CDMA_SUBSCRIPTION);
  },

  exitEmergencyCbMode: function exitEmergencyCbMode(options) {
    // The function could be called by an API from RadioInterfaceLayer or by
    // ril_worker itself. From ril_worker, we won't pass the parameter
    // 'options'. In this case, it is marked as internal.
    if (!options) {
      options = {internal: true};
    }
    this._cancelEmergencyCbModeTimeout();
    this.sendExitEmergencyCbModeRequest(options);
  },

  /**
   * Cache the request for making an emergency call when radio is off. The
   * request shall include two types of callback functions. 'callback' is
   * called when radio is ready, and 'onerror' is called when turning radio
   * on fails.
   */
  cachedDialRequest : null,

  /**
   * Dial the phone.
   *
   * @param number
   *        String containing the number to dial.
   * @param clirMode
   *        Integer for showing/hidding the caller Id to the called party.
   * @param uusInfo
   *        Integer doing something XXX TODO
   */
  dial: function dial(options) {
    let onerror = (function onerror(errorMsg) {
      this._sendCallError(-1, errorMsg);
    }).bind(this);

    if (this._isEmergencyNumber(options.number)) {
      this.dialEmergencyNumber(options, onerror);
    } else {
      if (!this._isCdma) {
        // TODO: Both dial() and sendMMI() functions should be unified at some
        // point in the future. In the mean time we handle temporary CLIR MMI
        // commands through the dial() function. Please see bug 889737.
        let mmi = this._parseMMI(options.number);
        if (mmi && this._isTemporaryModeCLIR(mmi)) {
          options.number = mmi.dialNumber;
          // In temporary mode, MMI_PROCEDURE_ACTIVATION means allowing CLI
          // presentation, i.e. CLIR_SUPPRESSION. See TS 22.030, Annex B.
          options.clirMode = mmi.procedure == MMI_PROCEDURE_ACTIVATION ?
                             CLIR_SUPPRESSION : CLIR_INVOCATION;
        }
      }
      this.dialNonEmergencyNumber(options, onerror);
    }
  },

  dialNonEmergencyNumber: function dialNonEmergencyNumber(options, onerror) {
    if (this.radioState == GECKO_RADIOSTATE_OFF) {
      // Notify error in establishing the call without radio.
      onerror(GECKO_ERROR_RADIO_NOT_AVAILABLE);
      return;
    }

    if (this.voiceRegistrationState.emergencyCallsOnly ||
        options.isDialEmergency) {
      onerror(RIL_CALL_FAILCAUSE_TO_GECKO_CALL_ERROR[CALL_FAIL_UNOBTAINABLE_NUMBER]);
      return;
    }

    // Exit emergency callback mode when user dial a non-emergency call.
    if (this._isInEmergencyCbMode) {
      this.exitEmergencyCbMode();
    }

    options.request = REQUEST_DIAL;
    this.sendDialRequest(options);
  },

  dialEmergencyNumber: function dialEmergencyNumber(options, onerror) {
    options.request = RILQUIRKS_REQUEST_USE_DIAL_EMERGENCY_CALL ?
                      REQUEST_DIAL_EMERGENCY_CALL : REQUEST_DIAL;

    if (this.radioState == GECKO_RADIOSTATE_OFF) {
      if (DEBUG) debug("Automatically enable radio for an emergency call.");

      if (!this.cachedDialRequest) {
        this.cachedDialRequest = {};
      }
      this.cachedDialRequest.onerror = onerror;
      this.cachedDialRequest.callback = this.sendDialRequest.bind(this, options);
      this.setRadioEnabled({enabled: true});
      return;
    }

    this.sendDialRequest(options);
  },

  sendDialRequest: function sendDialRequest(options) {
    Buf.newParcel(options.request);
    Buf.writeString(options.number);
    Buf.writeInt32(options.clirMode || 0);
    Buf.writeInt32(options.uusInfo || 0);
    // TODO Why do we need this extra 0? It was put it in to make this
    // match the format of the binary message.
    Buf.writeInt32(0);
    Buf.sendParcel();
  },

  /**
   * Hang up the phone.
   *
   * @param callIndex
   *        Call index (1-based) as reported by REQUEST_GET_CURRENT_CALLS.
   */
  hangUp: function hangUp(options) {
    let call = this.currentCalls[options.callIndex];
    if (!call) {
      return;
    }

    switch (call.state) {
      case CALL_STATE_ACTIVE:
      case CALL_STATE_DIALING:
      case CALL_STATE_ALERTING:
        Buf.newParcel(REQUEST_HANGUP);
        Buf.writeInt32(1);
        Buf.writeInt32(options.callIndex);
        Buf.sendParcel();
        break;
      case CALL_STATE_HOLDING:
        Buf.simpleRequest(REQUEST_HANGUP_WAITING_OR_BACKGROUND);
        break;
    }
  },

  /**
   * Mute or unmute the radio.
   *
   * @param mute
   *        Boolean to indicate whether to mute or unmute the radio.
   */
  setMute: function setMute(options) {
    Buf.newParcel(REQUEST_SET_MUTE);
    Buf.writeInt32(1);
    Buf.writeInt32(options.muted ? 1 : 0);
    Buf.sendParcel();
  },

  /**
   * Answer an incoming/waiting call.
   *
   * @param callIndex
   *        Call index of the call to answer.
   */
  answerCall: function answerCall(options) {
    // Check for races. Since we dispatched the incoming/waiting call
    // notification the incoming/waiting call may have changed. The main
    // thread thinks that it is answering the call with the given index,
    // so only answer if that is still incoming/waiting.
    let call = this.currentCalls[options.callIndex];
    if (!call) {
      return;
    }

    switch (call.state) {
      case CALL_STATE_INCOMING:
        Buf.simpleRequest(REQUEST_ANSWER);
        break;
      case CALL_STATE_WAITING:
        // Answer the waiting (second) call, and hold the first call.
        Buf.simpleRequest(REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE);
        break;
    }
  },

  /**
   * Reject an incoming/waiting call.
   *
   * @param callIndex
   *        Call index of the call to reject.
   */
  rejectCall: function rejectCall(options) {
    // Check for races. Since we dispatched the incoming/waiting call
    // notification the incoming/waiting call may have changed. The main
    // thread thinks that it is rejecting the call with the given index,
    // so only reject if that is still incoming/waiting.
    let call = this.currentCalls[options.callIndex];
    if (!call) {
      return;
    }

    if (this._isCdma) {
      // AT+CHLD=0 means "release held or UDUB."
      Buf.simpleRequest(REQUEST_HANGUP_WAITING_OR_BACKGROUND);
      return;
    }

    switch (call.state) {
      case CALL_STATE_INCOMING:
        Buf.simpleRequest(REQUEST_UDUB);
        break;
      case CALL_STATE_WAITING:
        // Reject the waiting (second) call, and remain the first call.
        Buf.simpleRequest(REQUEST_HANGUP_WAITING_OR_BACKGROUND);
        break;
    }
  },

  holdCall: function holdCall(options) {
    let call = this.currentCalls[options.callIndex];
    if (call && call.state == CALL_STATE_ACTIVE) {
      if (this._isCdma) {
        Buf.newParcel(REQUEST_CDMA_FLASH);
        Buf.writeString("");
        Buf.sendParcel();
      } else {
        Buf.simpleRequest(REQUEST_SWITCH_HOLDING_AND_ACTIVE);
      }
    }
  },

  resumeCall: function resumeCall(options) {
    let call = this.currentCalls[options.callIndex];
    if (call && call.state == CALL_STATE_HOLDING) {
      Buf.simpleRequest(REQUEST_SWITCH_HOLDING_AND_ACTIVE);
    }
  },

  // Flag indicating whether user has requested making a conference call.
  _hasConferenceRequest: false,

  conferenceCall: function conferenceCall(options) {
    this._hasConferenceRequest = true;
    Buf.simpleRequest(REQUEST_CONFERENCE, options);
  },

  separateCall: function separateCall(options) {
    Buf.newParcel(REQUEST_SEPARATE_CONNECTION, options);
    Buf.writeInt32(1);
    Buf.writeInt32(options.callIndex);
    Buf.sendParcel();
  },

  holdConference: function holdConference() {
    Buf.simpleRequest(REQUEST_SWITCH_HOLDING_AND_ACTIVE);
  },

  resumeConference: function resumeConference() {
    Buf.simpleRequest(REQUEST_SWITCH_HOLDING_AND_ACTIVE);
  },

  /**
   * Send an SMS.
   *
   * The `options` parameter object should contain the following attributes:
   *
   * @param number
   *        String containing the recipient number.
   * @param body
   *        String containing the message text.
   * @param envelopeId
   *        Numeric value identifying the sms request.
   */
  sendSMS: function sendSMS(options) {
    options.langIndex = options.langIndex || PDU_NL_IDENTIFIER_DEFAULT;
    options.langShiftIndex = options.langShiftIndex || PDU_NL_IDENTIFIER_DEFAULT;

    if (!options.retryCount) {
      options.retryCount = 0;
    }

    if (!options.segmentSeq) {
      // Fist segment to send
      options.segmentSeq = 1;
      options.body = options.segments[0].body;
      options.encodedBodyLength = options.segments[0].encodedBodyLength;
    }

    if (this._isCdma) {
      Buf.newParcel(REQUEST_CDMA_SEND_SMS, options);
      CdmaPDUHelper.writeMessage(options);
    } else {
      Buf.newParcel(REQUEST_SEND_SMS, options);
      Buf.writeInt32(2);
      Buf.writeString(options.SMSC);
      GsmPDUHelper.writeMessage(options);
    }
    Buf.sendParcel();
  },

  /**
   * Acknowledge the receipt and handling of an SMS.
   *
   * @param success
   *        Boolean indicating whether the message was successfuly handled.
   * @param cause
   *        SMS_* constant indicating the reason for unsuccessful handling.
   */
  acknowledgeGsmSms: function acknowledgeGsmSms(success, cause) {
    Buf.newParcel(REQUEST_SMS_ACKNOWLEDGE);
    Buf.writeInt32(2);
    Buf.writeInt32(success ? 1 : 0);
    Buf.writeInt32(cause);
    Buf.sendParcel();
  },

  /**
   * Acknowledge the receipt and handling of an SMS.
   *
   * @param success
   *        Boolean indicating whether the message was successfuly handled.
   */
  ackSMS: function ackSMS(options) {
    if (options.result == PDU_FCS_RESERVED) {
      return;
    }
    if (this._isCdma) {
      this.acknowledgeCdmaSms(options.result == PDU_FCS_OK, options.result);
    } else {
      this.acknowledgeGsmSms(options.result == PDU_FCS_OK, options.result);
    }
  },

  /**
   * Acknowledge the receipt and handling of a CDMA SMS.
   *
   * @param success
   *        Boolean indicating whether the message was successfuly handled.
   * @param cause
   *        SMS_* constant indicating the reason for unsuccessful handling.
   */
  acknowledgeCdmaSms: function acknowledgeCdmaSms(success, cause) {
    Buf.newParcel(REQUEST_CDMA_SMS_ACKNOWLEDGE);
    Buf.writeInt32(success ? 0 : 1);
    Buf.writeInt32(cause);
    Buf.sendParcel();
  },

  setCellBroadcastDisabled: function setCellBroadcastDisabled(options) {
    this.cellBroadcastDisabled = options.disabled;

    // If |this.mergedCellBroadcastConfig| is null, either we haven't finished
    // reading required SIM files, or no any channel is ever configured.  In
    // the former case, we'll call |this.updateCellBroadcastConfig()| later
    // with correct configs; in the latter case, we don't bother resetting CB
    // to disabled again.
    if (this.mergedCellBroadcastConfig) {
      this.updateCellBroadcastConfig();
    }
  },

  setCellBroadcastSearchList: function setCellBroadcastSearchList(options) {
    try {
      let str = options.searchListStr;
      this.cellBroadcastConfigs.MMI = this._convertCellBroadcastSearchList(str);
      options.success = true;
    } catch (e) {
      if (DEBUG) {
        debug("Invalid Cell Broadcast search list: " + e);
      }
      options.success = false;
    }

    this.sendChromeMessage(options);
    if (!options.success) {
      return;
    }

    this._mergeAllCellBroadcastConfigs();
  },

  updateCellBroadcastConfig: function updateCellBroadcastConfig() {
    let activate = !this.cellBroadcastDisabled &&
                   (this.mergedCellBroadcastConfig != null) &&
                   (this.mergedCellBroadcastConfig.length > 0);
    if (activate) {
      this.setSmsBroadcastConfig(this.mergedCellBroadcastConfig);
    } else {
      // It's unnecessary to set config first if we're deactivating.
      this.setSmsBroadcastActivation(false);
    }
  },

  setGsmSmsBroadcastConfig: function setGsmSmsBroadcastConfig(config) {
    Buf.newParcel(REQUEST_GSM_SET_BROADCAST_SMS_CONFIG);

    let numConfigs = config ? config.length / 2 : 0;
    Buf.writeInt32(numConfigs);
    for (let i = 0; i < config.length;) {
      Buf.writeInt32(config[i++]);
      Buf.writeInt32(config[i++]);
      Buf.writeInt32(0x00);
      Buf.writeInt32(0xFF);
      Buf.writeInt32(1);
    }

    Buf.sendParcel();
  },

  /**
   * Send CDMA SMS broadcast config.
   *
   * @see 3GPP2 C.R1001 Sec. 9.2 and 9.3
   */
  setCdmaSmsBroadcastConfig: function setCdmaSmsBroadcastConfig(config) {
    // |config| is an array of half-closed range: [[from, to), [from, to), ...].
    // It will be further decomposed, ex: [1, 4) => 1, 2, 3.
    Buf.newParcel(REQUEST_CDMA_SET_BROADCAST_SMS_CONFIG);

    let numConfigs = 0;
    for (let i = 0; i < config.length; i += 2) {
      numConfigs += (config[i+1] - config[i]);
    }

    Buf.writeInt32(numConfigs);
    for (let i = 0; i < config.length;) {
      let begin = config[i++];
      let end = config[i++];

      for (let j = begin; j < end; ++j) {
        Buf.writeInt32(j);
        Buf.writeInt32(0);  // Language Indicator: Unknown or unspecified.
        Buf.writeInt32(1);
      }
    }

    Buf.sendParcel();
  },

  setSmsBroadcastConfig: function setSmsBroadcastConfig(config) {
    if (this._isCdma) {
      this.setCdmaSmsBroadcastConfig(config);
    } else {
      this.setGsmSmsBroadcastConfig(config);
    }
  },

  setSmsBroadcastActivation: function setSmsBroadcastActivation(activate) {
    let parcelType = this._isCdma ? REQUEST_CDMA_SMS_BROADCAST_ACTIVATION :
                                    REQUEST_GSM_SMS_BROADCAST_ACTIVATION;
    Buf.newParcel(parcelType);
    Buf.writeInt32(1);
    // See hardware/ril/include/telephony/ril.h, 0 - Activate, 1 - Turn off.
    Buf.writeInt32(activate ? 0 : 1);
    Buf.sendParcel();
  },

  /**
   * Start a DTMF Tone.
   *
   * @param dtmfChar
   *        DTMF signal to send, 0-9, *, +
   */
  startTone: function startTone(options) {
    Buf.newParcel(REQUEST_DTMF_START);
    Buf.writeString(options.dtmfChar);
    Buf.sendParcel();
  },

  stopTone: function stopTone() {
    Buf.simpleRequest(REQUEST_DTMF_STOP);
  },

  /**
   * Send a DTMF tone.
   *
   * @param dtmfChar
   *        DTMF signal to send, 0-9, *, +
   */
  sendTone: function sendTone(options) {
    Buf.newParcel(REQUEST_DTMF);
    Buf.writeString(options.dtmfChar);
    Buf.sendParcel();
  },

  /**
   * Get the Short Message Service Center address.
   */
  getSmscAddress: function getSmscAddress(options) {
    if (!this.SMSC) {
      Buf.simpleRequest(REQUEST_GET_SMSC_ADDRESS, options);
      return;
    }

    if (!options || options.rilMessageType !== "getSmscAddress") {
      return;
    }

    options.smscAddress = this.SMSC;
    options.errorMsg = RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError];
    this.sendChromeMessage(options);
  },

  /**
   * Set the Short Message Service Center address.
   *
   * @param smscAddress
   *        Short Message Service Center address in PDU format.
   */
  setSmscAddress: function setSmscAddress(options) {
    Buf.newParcel(REQUEST_SET_SMSC_ADDRESS, options);
    Buf.writeString(options.smscAddress);
    Buf.sendParcel();
  },

  /**
   * Setup a data call.
   *
   * @param radioTech
   *        Integer to indicate radio technology.
   *        DATACALL_RADIOTECHNOLOGY_CDMA => CDMA.
   *        DATACALL_RADIOTECHNOLOGY_GSM  => GSM.
   * @param apn
   *        String containing the name of the APN to connect to.
   * @param user
   *        String containing the username for the APN.
   * @param passwd
   *        String containing the password for the APN.
   * @param chappap
   *        Integer containing CHAP/PAP auth type.
   *        DATACALL_AUTH_NONE        => PAP and CHAP is never performed.
   *        DATACALL_AUTH_PAP         => PAP may be performed.
   *        DATACALL_AUTH_CHAP        => CHAP may be performed.
   *        DATACALL_AUTH_PAP_OR_CHAP => PAP / CHAP may be performed.
   * @param pdptype
   *        String containing PDP type to request. ("IP", "IPV6", ...)
   */
  setupDataCall: function setupDataCall(options) {
    // From ./hardware/ril/include/telephony/ril.h:
    // ((const char **)data)[0] Radio technology to use: 0-CDMA, 1-GSM/UMTS, 2...
    // for values above 2 this is RIL_RadioTechnology + 2.
    //
    // From frameworks/base/telephony/java/com/android/internal/telephony/DataConnection.java:
    // if the mRilVersion < 6, radio technology must be GSM/UMTS or CDMA.
    // Otherwise, it must be + 2
    //
    // See also bug 901232 and 867873
    let radioTech;
    if (RILQUIRKS_V5_LEGACY) {
      radioTech = this._isCdma ? DATACALL_RADIOTECHNOLOGY_CDMA
                               : DATACALL_RADIOTECHNOLOGY_GSM;
    } else {
      radioTech = options.radioTech + 2;
    }
    let token = Buf.newParcel(REQUEST_SETUP_DATA_CALL, options);
    Buf.writeInt32(7);
    Buf.writeString(radioTech.toString());
    Buf.writeString(DATACALL_PROFILE_DEFAULT.toString());
    Buf.writeString(options.apn);
    Buf.writeString(options.user);
    Buf.writeString(options.passwd);
    Buf.writeString(options.chappap.toString());
    Buf.writeString(options.pdptype);
    Buf.sendParcel();
    return token;
  },

  /**
   * Deactivate a data call.
   *
   * @param cid
   *        String containing CID.
   * @param reason
   *        One of DATACALL_DEACTIVATE_* constants.
   */
  deactivateDataCall: function deactivateDataCall(options) {
    let datacall = this.currentDataCalls[options.cid];
    if (!datacall) {
      return;
    }

    Buf.newParcel(REQUEST_DEACTIVATE_DATA_CALL, options);
    Buf.writeInt32(2);
    Buf.writeString(options.cid);
    Buf.writeString(options.reason || DATACALL_DEACTIVATE_NO_REASON);
    Buf.sendParcel();

    datacall.state = GECKO_NETWORK_STATE_DISCONNECTING;
    this.sendChromeMessage(datacall);
  },

  /**
   * Get a list of data calls.
   */
  getDataCallList: function getDataCallList() {
    Buf.simpleRequest(REQUEST_DATA_CALL_LIST);
  },

  /**
   * Get failure casue code for the most recently failed PDP context.
   */
  getFailCauseCode: function getFailCauseCode(options) {
    Buf.simpleRequest(REQUEST_LAST_CALL_FAIL_CAUSE, options);
  },

  /**
   * Helper to parse MMI/USSD string. TS.22.030 Figure 3.5.3.2.
   */
  _parseMMI: function _parseMMI(mmiString) {
    if (!mmiString || !mmiString.length) {
      return null;
    }

    let matches = this._matchMMIRegexp(mmiString);
    if (matches) {
      // After successfully executing the regular expresion over the MMI string,
      // the following match groups should contain:
      // 1 = full MMI string that might be used as a USSD request.
      // 2 = MMI procedure.
      // 3 = Service code.
      // 5 = SIA.
      // 7 = SIB.
      // 9 = SIC.
      // 11 = Password registration.
      // 12 = Dialing number.
      return {
        fullMMI: matches[MMI_MATCH_GROUP_FULL_MMI],
        procedure: matches[MMI_MATCH_GROUP_MMI_PROCEDURE],
        serviceCode: matches[MMI_MATCH_GROUP_SERVICE_CODE],
        sia: matches[MMI_MATCH_GROUP_SIA],
        sib: matches[MMI_MATCH_GROUP_SIB],
        sic: matches[MMI_MATCH_GROUP_SIC],
        pwd: matches[MMI_MATCH_GROUP_PWD_CONFIRM],
        dialNumber: matches[MMI_MATCH_GROUP_DIALING_NUMBER]
      };
    }

    if (this._isPoundString(mmiString) ||
        this._isMMIShortString(mmiString)) {
      return {
        fullMMI: mmiString
      };
    }

    return null;
  },

  /**
   * Helper to parse MMI string via regular expression. TS.22.030 Figure
   * 3.5.3.2.
   */
  _matchMMIRegexp: function _matchMMIRegexp(mmiString) {
    // Regexp to parse and process the MMI code.
    if (this._mmiRegExp == null) {
      // The first group of the regexp takes the whole MMI string.
      // The second group takes the MMI procedure that can be:
      //    - Activation (*SC*SI#).
      //    - Deactivation (#SC*SI#).
      //    - Interrogation (*#SC*SI#).
      //    - Registration (**SC*SI#).
      //    - Erasure (##SC*SI#).
      //  where SC = Service Code (2 or 3 digits) and SI = Supplementary Info
      //  (variable length).
      let pattern = "((\\*[*#]?|##?)";

      // Third group of the regexp looks for the MMI Service code, which is a
      // 2 or 3 digits that uniquely specifies the Supplementary Service
      // associated with the MMI code.
      pattern += "(\\d{2,3})";

      // Groups from 4 to 9 looks for the MMI Supplementary Information SIA,
      // SIB and SIC. SIA may comprise e.g. a PIN code or Directory Number,
      // SIB may be used to specify the tele or bearer service and SIC to
      // specify the value of the "No Reply Condition Timer". Where a particular
      // service request does not require any SI, "*SI" is not entered. The use
      // of SIA, SIB and SIC is optional and shall be entered in any of the
      // following formats:
      //    - *SIA*SIB*SIC#
      //    - *SIA*SIB#
      //    - *SIA**SIC#
      //    - *SIA#
      //    - **SIB*SIC#
      //    - ***SISC#
      pattern += "(\\*([^*#]*)(\\*([^*#]*)(\\*([^*#]*)";

      // The eleventh group takes the password for the case of a password
      // registration procedure.
      pattern += "(\\*([^*#]*))?)?)?)?#)";

      // The last group takes the dial string after the #.
      pattern += "([^#]*)";

      this._mmiRegExp = new RegExp(pattern);
    }

    // Regex only applys for those well-defined MMI strings (refer to TS.22.030
    // Annex B), otherwise, null should be the expected return value.
    return this._mmiRegExp.exec(mmiString);
  },

  /**
   * Helper to parse # string. TS.22.030 Figure 3.5.3.2.
   */
  _isPoundString: function _isPoundString(mmiString) {
    return (mmiString.charAt(mmiString.length - 1) === MMI_END_OF_USSD);
  },

  /**
   * Helper to parse short string. TS.22.030 Figure 3.5.3.2.
   */
  _isMMIShortString: function _isMMIShortString(mmiString) {
    if (mmiString.length > 2) {
      return false;
    }

    if (this._isEmergencyNumber(mmiString)) {
      return false;
    }

    // In a call case.
    if (Object.getOwnPropertyNames(this.currentCalls).length > 0) {
      return true;
    }

    if ((mmiString.length != 2) || (mmiString.charAt(0) !== '1')) {
      return true;
    }

    return false;
  },

  sendMMI: function sendMMI(options) {
    if (DEBUG) {
      debug("SendMMI " + JSON.stringify(options));
    }
    let mmiString = options.mmi;
    let mmi = this._parseMMI(mmiString);

    let _sendMMIError = (function _sendMMIError(errorMsg, mmiServiceCode) {
      options.success = false;
      options.errorMsg = errorMsg;
      if (mmiServiceCode) {
        options.mmiServiceCode = mmiServiceCode;
      }
      this.sendChromeMessage(options);
    }).bind(this);

    function _isValidPINPUKRequest(mmiServiceCode) {
      // The only allowed MMI procedure for ICC PIN, PIN2, PUK and PUK2 handling
      // is "Registration" (**).
      if (!mmi.procedure || mmi.procedure != MMI_PROCEDURE_REGISTRATION ) {
        _sendMMIError(MMI_ERROR_KS_INVALID_ACTION, mmiServiceCode);
        return false;
      }

      if (!mmi.sia || !mmi.sia.length || !mmi.sib || !mmi.sib.length ||
          !mmi.sic || !mmi.sic.length) {
        _sendMMIError(MMI_ERROR_KS_ERROR, mmiServiceCode);
        return false;
      }

      if (mmi.sib != mmi.sic) {
        _sendMMIError(MMI_ERROR_KS_MISMATCH_PIN, mmiServiceCode);
        return false;
      }

      if (mmi.sia.length < 4 || mmi.sia.length > 8 ||
          mmi.sib.length < 4 || mmi.sib.length > 8 ||
          mmi.sic.length < 4 || mmi.sic.length > 8) {
        _sendMMIError(MMI_ERROR_KS_INVALID_PIN, mmiServiceCode);
        return false;
      }

      return true;
    }

    function _isRadioAvailable(mmiServiceCode) {
      if (RIL.radioState !== GECKO_RADIOSTATE_READY) {
        _sendMMIError(GECKO_ERROR_RADIO_NOT_AVAILABLE, mmiServiceCode);
        return false;
      }
      return true;
    }

    // If we couldn't parse the MMI code, we'll send it as an USSD request.
    if (mmi === null) {
      if (this._ussdSession) {
        if (!_isRadioAvailable(MMI_KS_SC_USSD)) {
          return;
        }
        options.ussd = mmiString;
        this.sendUSSD(options);
        return;
      }

      _sendMMIError(MMI_ERROR_KS_ERROR);
      return;
    }

    if (DEBUG) {
      debug("MMI " + JSON.stringify(mmi));
    }

    // We check if the MMI service code is supported and in that case we
    // trigger the appropriate RIL request if possible.
    let sc = mmi.serviceCode;
    switch (sc) {
      // Call forwarding
      case MMI_SC_CFU:
      case MMI_SC_CF_BUSY:
      case MMI_SC_CF_NO_REPLY:
      case MMI_SC_CF_NOT_REACHABLE:
      case MMI_SC_CF_ALL:
      case MMI_SC_CF_ALL_CONDITIONAL:
        if (!_isRadioAvailable(MMI_KS_SC_CALL_FORWARDING)) {
          return;
        }
        // Call forwarding requires at least an action, given by the MMI
        // procedure, and a reason, given by the MMI service code, but there
        // is no way that we get this far without a valid procedure or service
        // code.
        options.mmiServiceCode = MMI_KS_SC_CALL_FORWARDING;
        options.action = MMI_PROC_TO_CF_ACTION[mmi.procedure];
        options.reason = MMI_SC_TO_CF_REASON[sc];
        options.number = mmi.sia;
        options.serviceClass = this._siToServiceClass(mmi.sib);
        if (options.action == CALL_FORWARD_ACTION_QUERY_STATUS) {
          this.queryCallForwardStatus(options);
          return;
        }

        options.isSetCallForward = true;
        options.timeSeconds = mmi.sic;
        this.setCallForward(options);
        return;

      // Change the current ICC PIN number.
      case MMI_SC_PIN:
        // As defined in TS.122.030 6.6.2 to change the ICC PIN we should expect
        // an MMI code of the form **04*OLD_PIN*NEW_PIN*NEW_PIN#, where old PIN
        // should be entered as the SIA parameter and the new PIN as SIB and
        // SIC.
        if (!_isRadioAvailable(MMI_KS_SC_PIN) ||
            !_isValidPINPUKRequest(MMI_KS_SC_PIN)) {
          return;
        }

        options.mmiServiceCode = MMI_KS_SC_PIN;
        options.pin = mmi.sia;
        options.newPin = mmi.sib;
        this.changeICCPIN(options);
        return;

      // Change the current ICC PIN2 number.
      case MMI_SC_PIN2:
        // As defined in TS.122.030 6.6.2 to change the ICC PIN2 we should
        // enter and MMI code of the form **042*OLD_PIN2*NEW_PIN2*NEW_PIN2#,
        // where the old PIN2 should be entered as the SIA parameter and the
        // new PIN2 as SIB and SIC.
        if (!_isRadioAvailable(MMI_KS_SC_PIN2) ||
            !_isValidPINPUKRequest(MMI_KS_SC_PIN2)) {
          return;
        }

        options.mmiServiceCode = MMI_KS_SC_PIN2;
        options.pin = mmi.sia;
        options.newPin = mmi.sib;
        this.changeICCPIN2(options);
        return;

      // Unblock ICC PIN.
      case MMI_SC_PUK:
        // As defined in TS.122.030 6.6.3 to unblock the ICC PIN we should
        // enter an MMI code of the form **05*PUK*NEW_PIN*NEW_PIN#, where PUK
        // should be entered as the SIA parameter and the new PIN as SIB and
        // SIC.
        if (!_isRadioAvailable(MMI_KS_SC_PUK) ||
            !_isValidPINPUKRequest(MMI_KS_SC_PUK)) {
          return;
        }

        options.mmiServiceCode = MMI_KS_SC_PUK;
        options.puk = mmi.sia;
        options.newPin = mmi.sib;
        this.enterICCPUK(options);
        return;

      // Unblock ICC PIN2.
      case MMI_SC_PUK2:
        // As defined in TS.122.030 6.6.3 to unblock the ICC PIN2 we should
        // enter an MMI code of the form **052*PUK2*NEW_PIN2*NEW_PIN2#, where
        // PUK2 should be entered as the SIA parameter and the new PIN2 as SIB
        // and SIC.
        if (!_isRadioAvailable(MMI_KS_SC_PUK2) ||
            !_isValidPINPUKRequest(MMI_KS_SC_PUK2)) {
          return;
        }

        options.mmiServiceCode = MMI_KS_SC_PUK2;
        options.puk = mmi.sia;
        options.newPin = mmi.sib;
        this.enterICCPUK2(options);
        return;

      // IMEI
      case MMI_SC_IMEI:
        // A device's IMEI can't change, so we only need to request it once.
        if (this.IMEI == null) {
          this.getIMEI(options);
          return;
        }
        // If we already had the device's IMEI, we just send it to chrome.
        options.mmiServiceCode = MMI_KS_SC_IMEI;
        options.success = true;
        options.statusMessage = this.IMEI;
        this.sendChromeMessage(options);
        return;

      // CLIP
      case MMI_SC_CLIP:
        options.mmiServiceCode = MMI_KS_SC_CLIP;
        options.procedure = mmi.procedure;
        if (options.procedure === MMI_PROCEDURE_INTERROGATION) {
          this.queryCLIP(options);
        } else {
          _sendMMIError(MMI_ERROR_KS_NOT_SUPPORTED, MMI_KS_SC_CLIP);
        }
        return;

      // CLIR (non-temporary ones)
      // TODO: Both dial() and sendMMI() functions should be unified at some
      // point in the future. In the mean time we handle temporary CLIR MMI
      // commands through the dial() function. Please see bug 889737.
      case MMI_SC_CLIR:
        options.mmiServiceCode = MMI_KS_SC_CLIR;
        options.procedure = mmi.procedure;
        switch (options.procedure) {
          case MMI_PROCEDURE_INTERROGATION:
            this.getCLIR(options);
            return;
          case MMI_PROCEDURE_ACTIVATION:
            options.clirMode = CLIR_INVOCATION;
            break;
          case MMI_PROCEDURE_DEACTIVATION:
            options.clirMode = CLIR_SUPPRESSION;
            break;
          default:
            _sendMMIError(MMI_ERROR_KS_NOT_SUPPORTED, MMI_KS_SC_CLIR);
            return;
        }
        options.isSetCLIR = true;
        this.setCLIR(options);
        return;

      // Call barring
      case MMI_SC_BAOC:
      case MMI_SC_BAOIC:
      case MMI_SC_BAOICxH:
      case MMI_SC_BAIC:
      case MMI_SC_BAICr:
      case MMI_SC_BA_ALL:
      case MMI_SC_BA_MO:
      case MMI_SC_BA_MT:
        options.mmiServiceCode = MMI_KS_SC_CALL_BARRING;
        options.password = mmi.sia || "";
        options.serviceClass = this._siToServiceClass(mmi.sib);
        options.facility = MMI_SC_TO_CB_FACILITY[sc];
        options.procedure = mmi.procedure;
        if (mmi.procedure === MMI_PROCEDURE_INTERROGATION) {
          this.queryICCFacilityLock(options);
          return;
        }
        if (mmi.procedure === MMI_PROCEDURE_ACTIVATION) {
          options.enabled = 1;
        } else if (mmi.procedure === MMI_PROCEDURE_DEACTIVATION) {
          options.enabled = 0;
        } else {
          _sendMMIError(MMI_ERROR_KS_NOT_SUPPORTED, MMI_KS_SC_CALL_BARRING);
          return;
        }
        this.setICCFacilityLock(options);
        return;

      // Call waiting
      case MMI_SC_CALL_WAITING:
        if (!_isRadioAvailable(MMI_KS_SC_CALL_WAITING)) {
          return;
        }

        options.mmiServiceCode = MMI_KS_SC_CALL_WAITING;

        if (mmi.procedure === MMI_PROCEDURE_INTERROGATION) {
          this._handleQueryMMICallWaiting(options);
          return;
        }

        if (mmi.procedure === MMI_PROCEDURE_ACTIVATION) {
          options.enabled = true;
        } else if (mmi.procedure === MMI_PROCEDURE_DEACTIVATION) {
          options.enabled = false;
        } else {
          _sendMMIError(MMI_ERROR_KS_NOT_SUPPORTED, MMI_KS_SC_CALL_WAITING);
          return;
        }

        options.serviceClass = this._siToServiceClass(mmi.sia);
        this._handleSetMMICallWaiting(options);
        return;
    }

    // If the MMI code is not a known code and is a recognized USSD request,
    // it shall still be sent as a USSD request.
    if (mmi.fullMMI) {
      if (!_isRadioAvailable(MMI_KS_SC_USSD)) {
        return;
      }

      options.ussd = mmi.fullMMI;
      options.mmiServiceCode = MMI_KS_SC_USSD;
      this.sendUSSD(options);
      return;
    }

    // At this point, the MMI string is considered as not valid MMI code and
    // not valid USSD code.
    _sendMMIError(MMI_ERROR_KS_ERROR);
  },

  /**
   * Send USSD.
   *
   * @param ussd
   *        String containing the USSD code.
   *
   */
   sendUSSD: function sendUSSD(options) {
     Buf.newParcel(REQUEST_SEND_USSD, options);
     Buf.writeString(options.ussd);
     Buf.sendParcel();
   },

  /**
   * Cancel pending USSD.
   */
   cancelUSSD: function cancelUSSD(options) {
     options.mmiServiceCode = MMI_KS_SC_USSD;
     Buf.simpleRequest(REQUEST_CANCEL_USSD, options);
   },

  /**
   * Queries current call forward rules.
   *
   * @param reason
   *        One of nsIDOMMozMobileCFInfo.CALL_FORWARD_REASON_* constants.
   * @param serviceClass
   *        One of ICC_SERVICE_CLASS_* constants.
   * @param number
   *        Phone number of forwarding address.
   */
  queryCallForwardStatus: function queryCallForwardStatus(options) {
    Buf.newParcel(REQUEST_QUERY_CALL_FORWARD_STATUS, options);
    Buf.writeInt32(CALL_FORWARD_ACTION_QUERY_STATUS);
    Buf.writeInt32(options.reason);
    Buf.writeInt32(options.serviceClass || ICC_SERVICE_CLASS_NONE);
    Buf.writeInt32(this._toaFromString(options.number));
    Buf.writeString(options.number);
    Buf.writeInt32(0);
    Buf.sendParcel();
  },

  /**
   * Configures call forward rule.
   *
   * @param action
   *        One of nsIDOMMozMobileCFInfo.CALL_FORWARD_ACTION_* constants.
   * @param reason
   *        One of nsIDOMMozMobileCFInfo.CALL_FORWARD_REASON_* constants.
   * @param serviceClass
   *        One of ICC_SERVICE_CLASS_* constants.
   * @param number
   *        Phone number of forwarding address.
   * @param timeSeconds
   *        Time in seconds to wait beforec all is forwarded.
   */
  setCallForward: function setCallForward(options) {
    Buf.newParcel(REQUEST_SET_CALL_FORWARD, options);
    Buf.writeInt32(options.action);
    Buf.writeInt32(options.reason);
    Buf.writeInt32(options.serviceClass);
    Buf.writeInt32(this._toaFromString(options.number));
    Buf.writeString(options.number);
    Buf.writeInt32(options.timeSeconds);
    Buf.sendParcel();
  },

  /**
   * Queries current call barring rules.
   *
   * @param program
   *        One of nsIDOMMozMobileConnection.CALL_BARRING_PROGRAM_* constants.
   * @param serviceClass
   *        One of ICC_SERVICE_CLASS_* constants.
   */
  queryCallBarringStatus: function queryCallBarringStatus(options) {
    options.facility = CALL_BARRING_PROGRAM_TO_FACILITY[options.program];
    options.password = ""; // For query no need to provide it.
    this.queryICCFacilityLock(options);
  },

  /**
   * Configures call barring rule.
   *
   * @param program
   *        One of nsIDOMMozMobileConnection.CALL_BARRING_PROGRAM_* constants.
   * @param enabled
   *        Enable or disable the call barring.
   * @param password
   *        Barring password.
   * @param serviceClass
   *        One of ICC_SERVICE_CLASS_* constants.
   */
  setCallBarring: function setCallBarring(options) {
    options.facility = CALL_BARRING_PROGRAM_TO_FACILITY[options.program];
    this.setICCFacilityLock(options);
  },

  /**
   * Change call barring facility password.
   *
   * @param pin
   *        Old password.
   * @param newPin
   *        New password.
   */
  changeCallBarringPassword: function changeCallBarringPassword(options) {
    Buf.newParcel(REQUEST_CHANGE_BARRING_PASSWORD, options);
    Buf.writeInt32(3);
    // Set facility to ICC_CB_FACILITY_BA_ALL by following TS.22.030 clause
    // 6.5.4 and Table B.1.
    Buf.writeString(ICC_CB_FACILITY_BA_ALL);
    Buf.writeString(options.pin);
    Buf.writeString(options.newPin);
    Buf.sendParcel();
  },

  /**
   * Handle STK CALL_SET_UP request.
   *
   * @param hasConfirmed
   *        Does use have confirmed the call requested from ICC?
   */
  stkHandleCallSetup: function stkHandleCallSetup(options) {
     Buf.newParcel(REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM);
     Buf.writeInt32(1);
     Buf.writeInt32(options.hasConfirmed ? 1 : 0);
     Buf.sendParcel();
  },

  /**
   * Send STK Profile Download.
   *
   * @param profile Profile supported by ME.
   */
  sendStkTerminalProfile: function sendStkTerminalProfile(profile) {
    Buf.newParcel(REQUEST_STK_SET_PROFILE);
    Buf.writeInt32(profile.length * 2);
    for (let i = 0; i < profile.length; i++) {
      GsmPDUHelper.writeHexOctet(profile[i]);
    }
    Buf.writeInt32(0);
    Buf.sendParcel();
  },

  /**
   * Send STK terminal response.
   *
   * @param command
   * @param deviceIdentities
   * @param resultCode
   * @param [optional] itemIdentifier
   * @param [optional] input
   * @param [optional] isYesNo
   * @param [optional] hasConfirmed
   * @param [optional] localInfo
   * @param [optional] timer
   */
  sendStkTerminalResponse: function sendStkTerminalResponse(response) {
    if (response.hasConfirmed !== undefined) {
      this.stkHandleCallSetup(response);
      return;
    }

    let command = response.command;
    Buf.newParcel(REQUEST_STK_SEND_TERMINAL_RESPONSE);

    // 1st mark for Parcel size
    Buf.startCalOutgoingSize(function(size) {
      // Parcel size is in string length, which costs 2 uint8 per char.
      Buf.writeInt32(size / 2);
    });

    // Command Details
    GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_COMMAND_DETAILS |
                               COMPREHENSIONTLV_FLAG_CR);
    GsmPDUHelper.writeHexOctet(3);
    if (response.command) {
      GsmPDUHelper.writeHexOctet(command.commandNumber);
      GsmPDUHelper.writeHexOctet(command.typeOfCommand);
      GsmPDUHelper.writeHexOctet(command.commandQualifier);
    } else {
      GsmPDUHelper.writeHexOctet(0x00);
      GsmPDUHelper.writeHexOctet(0x00);
      GsmPDUHelper.writeHexOctet(0x00);
    }

    // Device Identifier
    // According to TS102.223/TS31.111 section 6.8 Structure of
    // TERMINAL RESPONSE, "For all SIMPLE-TLV objects with Min=N,
    // the ME should set the CR(comprehension required) flag to
    // comprehension not required.(CR=0)"
    // Since DEVICE_IDENTITIES and DURATION TLVs have Min=N,
    // the CR flag is not set.
    GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_DEVICE_ID);
    GsmPDUHelper.writeHexOctet(2);
    GsmPDUHelper.writeHexOctet(STK_DEVICE_ID_ME);
    GsmPDUHelper.writeHexOctet(STK_DEVICE_ID_SIM);

    // Result
    GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_RESULT |
                               COMPREHENSIONTLV_FLAG_CR);
    GsmPDUHelper.writeHexOctet(1);
    GsmPDUHelper.writeHexOctet(response.resultCode);

    // Item Identifier
    if (response.itemIdentifier != null) {
      GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_ITEM_ID |
                                 COMPREHENSIONTLV_FLAG_CR);
      GsmPDUHelper.writeHexOctet(1);
      GsmPDUHelper.writeHexOctet(response.itemIdentifier);
    }

    // No need to process Text data if user requests help information.
    if (response.resultCode != STK_RESULT_HELP_INFO_REQUIRED) {
      let text;
      if (response.isYesNo !== undefined) {
        // GET_INKEY
        // When the ME issues a successful TERMINAL RESPONSE for a GET INKEY
        // ("Yes/No") command with command qualifier set to "Yes/No", it shall
        // supply the value '01' when the answer is "positive" and the value
        // '00' when the answer is "negative" in the Text string data object.
        text = response.isYesNo ? 0x01 : 0x00;
      } else {
        text = response.input;
      }

      if (text) {
        GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_TEXT_STRING |
                                   COMPREHENSIONTLV_FLAG_CR);

        // 2nd mark for text length
        Buf.startCalOutgoingSize(function(size) {
          // Text length is in number of hexOctets, which costs 4 uint8 per hexOctet.
          GsmPDUHelper.writeHexOctet(size / 4);
        });

        let coding = command.options.isUCS2 ?
                       STK_TEXT_CODING_UCS2 :
                       (command.options.isPacked ?
                          STK_TEXT_CODING_GSM_7BIT_PACKED :
                          STK_TEXT_CODING_GSM_8BIT);
        GsmPDUHelper.writeHexOctet(coding);

        // Write Text String.
        switch (coding) {
          case STK_TEXT_CODING_UCS2:
            GsmPDUHelper.writeUCS2String(text);
            break;
          case STK_TEXT_CODING_GSM_7BIT_PACKED:
            GsmPDUHelper.writeStringAsSeptets(text, 0, 0, 0);
            break;
          case STK_TEXT_CODING_GSM_8BIT:
            for (let i = 0; i < text.length; i++) {
              GsmPDUHelper.writeHexOctet(text.charCodeAt(i));
            }
            break;
        }

        // Calculate and write text length to 2nd mark
        Buf.stopCalOutgoingSize();
      }
    }

    // Local Information
    if (response.localInfo) {
      let localInfo = response.localInfo;

      // Location Infomation
      if (localInfo.locationInfo) {
        ComprehensionTlvHelper.writeLocationInfoTlv(localInfo.locationInfo);
      }

      // IMEI
      if (localInfo.imei != null) {
        let imei = localInfo.imei;
        if(imei.length == 15) {
          imei = imei + "0";
        }

        GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_IMEI);
        GsmPDUHelper.writeHexOctet(8);
        for (let i = 0; i < imei.length / 2; i++) {
          GsmPDUHelper.writeHexOctet(parseInt(imei.substr(i * 2, 2), 16));
        }
      }

      // Date and Time Zone
      if (localInfo.date != null) {
        ComprehensionTlvHelper.writeDateTimeZoneTlv(localInfo.date);
      }

      // Language
      if (localInfo.language) {
        ComprehensionTlvHelper.writeLanguageTlv(localInfo.language);
      }
    }

    // Timer
    if (response.timer) {
      let timer = response.timer;

      if (timer.timerId) {
        GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_TIMER_IDENTIFIER);
        GsmPDUHelper.writeHexOctet(1);
        GsmPDUHelper.writeHexOctet(timer.timerId);
      }

      if (timer.timerValue) {
        ComprehensionTlvHelper.writeTimerValueTlv(timer.timerValue, false);
      }
    }

    // Calculate and write Parcel size to 1st mark
    Buf.stopCalOutgoingSize();

    Buf.writeInt32(0);
    Buf.sendParcel();
  },

  /**
   * Send STK Envelope(Menu Selection) command.
   *
   * @param itemIdentifier
   * @param helpRequested
   */
  sendStkMenuSelection: function sendStkMenuSelection(command) {
    command.tag = BER_MENU_SELECTION_TAG;
    command.deviceId = {
      sourceId :STK_DEVICE_ID_KEYPAD,
      destinationId: STK_DEVICE_ID_SIM
    };
    this.sendICCEnvelopeCommand(command);
  },

  /**
   * Send STK Envelope(Timer Expiration) command.
   *
   * @param timer
   */
  sendStkTimerExpiration: function sendStkTimerExpiration(command) {
    command.tag = BER_TIMER_EXPIRATION_TAG;
    command.deviceId = {
      sourceId: STK_DEVICE_ID_ME,
      destinationId: STK_DEVICE_ID_SIM
    };
    command.timerId = command.timer.timerId;
    command.timerValue = command.timer.timerValue;
    this.sendICCEnvelopeCommand(command);
  },

  /**
   * Send STK Envelope(Event Download) command.
   * @param event
   */
  sendStkEventDownload: function sendStkEventDownload(command) {
    command.tag = BER_EVENT_DOWNLOAD_TAG;
    command.eventList = command.event.eventType;
    switch (command.eventList) {
      case STK_EVENT_TYPE_LOCATION_STATUS:
        command.deviceId = {
          sourceId :STK_DEVICE_ID_ME,
          destinationId: STK_DEVICE_ID_SIM
        };
        command.locationStatus = command.event.locationStatus;
        // Location info should only be provided when locationStatus is normal.
        if (command.locationStatus == STK_SERVICE_STATE_NORMAL) {
          command.locationInfo = command.event.locationInfo;
        }
        break;
      case STK_EVENT_TYPE_MT_CALL:
        command.deviceId = {
          sourceId: STK_DEVICE_ID_NETWORK,
          destinationId: STK_DEVICE_ID_SIM
        };
        command.transactionId = 0;
        command.address = command.event.number;
        break;
      case STK_EVENT_TYPE_CALL_DISCONNECTED:
        command.cause = command.event.error;
        // Fall through.
      case STK_EVENT_TYPE_CALL_CONNECTED:
        command.deviceId = {
          sourceId: (command.event.isIssuedByRemote ?
                     STK_DEVICE_ID_NETWORK : STK_DEVICE_ID_ME),
          destinationId: STK_DEVICE_ID_SIM
        };
        command.transactionId = 0;
        break;
      case STK_EVENT_TYPE_USER_ACTIVITY:
        command.deviceId = {
          sourceId: STK_DEVICE_ID_ME,
          destinationId: STK_DEVICE_ID_SIM
        };
        break;
      case STK_EVENT_TYPE_IDLE_SCREEN_AVAILABLE:
        command.deviceId = {
          sourceId: STK_DEVICE_ID_DISPLAY,
          destinationId: STK_DEVICE_ID_SIM
        };
        break;
      case STK_EVENT_TYPE_LANGUAGE_SELECTION:
        command.deviceId = {
          sourceId: STK_DEVICE_ID_ME,
          destinationId: STK_DEVICE_ID_SIM
        };
        command.language = command.event.language;
        break;
      case STK_EVENT_TYPE_BROWSER_TERMINATION:
        command.deviceId = {
          sourceId: STK_DEVICE_ID_ME,
          destinationId: STK_DEVICE_ID_SIM
        };
        command.terminationCause = command.event.terminationCause;
        break;
    }
    this.sendICCEnvelopeCommand(command);
  },

  /**
   * Send REQUEST_STK_SEND_ENVELOPE_COMMAND to ICC.
   *
   * @param tag
   * @patam deviceId
   * @param [optioanl] itemIdentifier
   * @param [optional] helpRequested
   * @param [optional] eventList
   * @param [optional] locationStatus
   * @param [optional] locationInfo
   * @param [optional] address
   * @param [optional] transactionId
   * @param [optional] cause
   * @param [optional] timerId
   * @param [optional] timerValue
   * @param [optional] terminationCause
   */
  sendICCEnvelopeCommand: function sendICCEnvelopeCommand(options) {
    if (DEBUG) {
      debug("Stk Envelope " + JSON.stringify(options));
    }
    Buf.newParcel(REQUEST_STK_SEND_ENVELOPE_COMMAND);

    // 1st mark for Parcel size
    Buf.startCalOutgoingSize(function(size) {
      // Parcel size is in string length, which costs 2 uint8 per char.
      Buf.writeInt32(size / 2);
    });

    // Write a BER-TLV
    GsmPDUHelper.writeHexOctet(options.tag);
    // 2nd mark for BER length
    Buf.startCalOutgoingSize(function(size) {
      // BER length is in number of hexOctets, which costs 4 uint8 per hexOctet.
      GsmPDUHelper.writeHexOctet(size / 4);
    });

    // Device Identifies
    GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_DEVICE_ID |
                               COMPREHENSIONTLV_FLAG_CR);
    GsmPDUHelper.writeHexOctet(2);
    GsmPDUHelper.writeHexOctet(options.deviceId.sourceId);
    GsmPDUHelper.writeHexOctet(options.deviceId.destinationId);

    // Item Identifier
    if (options.itemIdentifier != null) {
      GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_ITEM_ID |
                                 COMPREHENSIONTLV_FLAG_CR);
      GsmPDUHelper.writeHexOctet(1);
      GsmPDUHelper.writeHexOctet(options.itemIdentifier);
    }

    // Help Request
    if (options.helpRequested) {
      GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_HELP_REQUEST |
                                 COMPREHENSIONTLV_FLAG_CR);
      GsmPDUHelper.writeHexOctet(0);
      // Help Request doesn't have value
    }

    // Event List
    if (options.eventList != null) {
      GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_EVENT_LIST |
                                 COMPREHENSIONTLV_FLAG_CR);
      GsmPDUHelper.writeHexOctet(1);
      GsmPDUHelper.writeHexOctet(options.eventList);
    }

    // Location Status
    if (options.locationStatus != null) {
      let len = options.locationStatus.length;
      GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_LOCATION_STATUS |
                                 COMPREHENSIONTLV_FLAG_CR);
      GsmPDUHelper.writeHexOctet(1);
      GsmPDUHelper.writeHexOctet(options.locationStatus);
    }

    // Location Info
    if (options.locationInfo) {
      ComprehensionTlvHelper.writeLocationInfoTlv(options.locationInfo);
    }

    // Transaction Id
    if (options.transactionId != null) {
      GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_TRANSACTION_ID |
                                 COMPREHENSIONTLV_FLAG_CR);
      GsmPDUHelper.writeHexOctet(1);
      GsmPDUHelper.writeHexOctet(options.transactionId);
    }

    // Address
    if (options.address) {
      GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_ADDRESS |
                                 COMPREHENSIONTLV_FLAG_CR);
      ComprehensionTlvHelper.writeLength(
        Math.ceil(options.address.length/2) + 1 // address BCD + TON
      );
      ICCPDUHelper.writeDiallingNumber(options.address);
    }

    // Cause of disconnection.
    if (options.cause != null) {
      ComprehensionTlvHelper.writeCauseTlv(options.cause);
    }

    // Timer Identifier
    if (options.timerId != null) {
        GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_TIMER_IDENTIFIER |
                                   COMPREHENSIONTLV_FLAG_CR);
        GsmPDUHelper.writeHexOctet(1);
        GsmPDUHelper.writeHexOctet(options.timerId);
    }

    // Timer Value
    if (options.timerValue != null) {
        ComprehensionTlvHelper.writeTimerValueTlv(options.timerValue, true);
    }

    // Language
    if (options.language) {
      ComprehensionTlvHelper.writeLanguageTlv(options.language);
    }

    // Browser Termination
    if (options.terminationCause != null) {
      GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_BROWSER_TERMINATION_CAUSE |
                                 COMPREHENSIONTLV_FLAG_CR);
      GsmPDUHelper.writeHexOctet(1);
      GsmPDUHelper.writeHexOctet(options.terminationCause);
    }

    // Calculate and write BER length to 2nd mark
    Buf.stopCalOutgoingSize();

    // Calculate and write Parcel size to 1st mark
    Buf.stopCalOutgoingSize();

    Buf.writeInt32(0);
    Buf.sendParcel();
  },

  /**
   * Check a given number against the list of emergency numbers provided by the RIL.
   *
   * @param number
   *        The number to look up.
   */
   _isEmergencyNumber: function _isEmergencyNumber(number) {
     // Check read-write ecclist property first.
     let numbers = libcutils.property_get("ril.ecclist");
     if (!numbers) {
       // Then read-only ecclist property since others RIL only uses this.
       numbers = libcutils.property_get("ro.ril.ecclist");
     }

     if (numbers) {
       numbers = numbers.split(",");
     } else {
       // No ecclist system property, so use our own list.
       numbers = DEFAULT_EMERGENCY_NUMBERS;
     }

     return numbers.indexOf(number) != -1;
   },

   /**
    * Checks whether to temporarily suppress caller id for the call.
    *
    * @param mmi
    *        MMI full object.
    */
   _isTemporaryModeCLIR: function _isTemporaryModeCLIR(mmi) {
     return (mmi &&
             mmi.serviceCode == MMI_SC_CLIR &&
             mmi.dialNumber &&
             (mmi.procedure == MMI_PROCEDURE_ACTIVATION ||
              mmi.procedure == MMI_PROCEDURE_DEACTIVATION));
   },

  /**
   * Report STK Service is running.
   */
  reportStkServiceIsRunning: function reportStkServiceIsRunning() {
    Buf.simpleRequest(REQUEST_REPORT_STK_SERVICE_IS_RUNNING);
  },

  /**
   * Process ICC status.
   */
  _processICCStatus: function _processICCStatus(iccStatus) {
    this.iccStatus = iccStatus;
    let newCardState;
    let index = this._isCdma ? iccStatus.cdmaSubscriptionAppIndex :
                               iccStatus.gsmUmtsSubscriptionAppIndex;
    let app = iccStatus.apps[index];

    // When |iccStatus.cardState| is not CARD_STATE_PRESENT or have incorrect
    // app information, we can not get iccId. So treat ICC as undetected.
    if (iccStatus.cardState !== CARD_STATE_PRESENT || !app) {
      if (this.cardState !== GECKO_CARDSTATE_UNDETECTED) {
        this.operator = null;
        // We should send |cardstatechange| before |iccinfochange|, otherwise we
        // may lost cardstatechange event when icc card becomes undetected.
        this.cardState = GECKO_CARDSTATE_UNDETECTED;
        this.sendChromeMessage({rilMessageType: "cardstatechange",
                                cardState: this.cardState});

        this.iccInfo = {iccType: null};
        ICCUtilsHelper.handleICCInfoChange();
      }
      return;
    }

    // fetchICCRecords will need to read aid, so read aid here.
    this.aid = app.aid;
    this.appType = app.app_type;
    this.iccInfo.iccType = GECKO_CARD_TYPE[this.appType];
    // Try to get iccId only when cardState left GECKO_CARDSTATE_UNDETECTED.
    if (this.cardState === GECKO_CARDSTATE_UNDETECTED &&
        iccStatus.cardState === CARD_STATE_PRESENT) {
      ICCRecordHelper.readICCID();
    }

    switch (app.app_state) {
      case CARD_APPSTATE_ILLEGAL:
        newCardState = GECKO_CARDSTATE_ILLEGAL;
        break;
      case CARD_APPSTATE_PIN:
        newCardState = GECKO_CARDSTATE_PIN_REQUIRED;
        break;
      case CARD_APPSTATE_PUK:
        newCardState = GECKO_CARDSTATE_PUK_REQUIRED;
        break;
      case CARD_APPSTATE_SUBSCRIPTION_PERSO:
        newCardState = PERSONSUBSTATE[app.perso_substate];
        break;
      case CARD_APPSTATE_READY:
        newCardState = GECKO_CARDSTATE_READY;
        break;
      case CARD_APPSTATE_UNKNOWN:
      case CARD_APPSTATE_DETECTED:
        // Fall through.
      default:
        newCardState = GECKO_CARDSTATE_UNKNOWN;
    }

    let pin1State = app.pin1_replaced ? iccStatus.universalPINState :
                                        app.pin1;
    if (pin1State === CARD_PINSTATE_ENABLED_PERM_BLOCKED) {
      newCardState = GECKO_CARDSTATE_PERMANENT_BLOCKED;
    }

    if (this.cardState == newCardState) {
      return;
    }

    // This was moved down from CARD_APPSTATE_READY
    this.requestNetworkInfo();
    if (newCardState == GECKO_CARDSTATE_READY) {
      // For type SIM, we need to check EF_phase first.
      // Other types of ICC we can send Terminal_Profile immediately.
      if (this.appType == CARD_APPTYPE_SIM) {
        SimRecordHelper.readSimPhase();
      } else if (RILQUIRKS_SEND_STK_PROFILE_DOWNLOAD) {
        this.sendStkTerminalProfile(STK_SUPPORTED_TERMINAL_PROFILE);
      }

      ICCRecordHelper.fetchICCRecords();
    }

    this.cardState = newCardState;
    this.sendChromeMessage({rilMessageType: "cardstatechange",
                            cardState: this.cardState});
  },

   /**
   * Helper for processing responses of functions such as enterICC* and changeICC*.
   */
  _processEnterAndChangeICCResponses:
    function _processEnterAndChangeICCResponses(length, options) {
    options.success = (options.rilRequestError === 0);
    if (!options.success) {
      options.errorMsg = RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError];
    }
    options.retryCount = length ? Buf.readInt32List()[0] : -1;
    if (options.rilMessageType != "sendMMI") {
      this.sendChromeMessage(options);
      return;
    }

    let mmiServiceCode = options.mmiServiceCode;

    if (options.success) {
      switch (mmiServiceCode) {
        case MMI_KS_SC_PIN:
          options.statusMessage = MMI_SM_KS_PIN_CHANGED;
          break;
        case MMI_KS_SC_PIN2:
          options.statusMessage = MMI_SM_KS_PIN2_CHANGED;
          break;
        case MMI_KS_SC_PUK:
          options.statusMessage = MMI_SM_KS_PIN_UNBLOCKED;
          break;
        case MMI_KS_SC_PUK2:
          options.statusMessage = MMI_SM_KS_PIN2_UNBLOCKED;
          break;
      }
    } else {
      if (options.retryCount <= 0) {
        if (mmiServiceCode === MMI_KS_SC_PUK) {
          options.errorMsg = MMI_ERROR_KS_SIM_BLOCKED;
        } else if (mmiServiceCode === MMI_KS_SC_PIN) {
          options.errorMsg = MMI_ERROR_KS_NEEDS_PUK;
        }
      } else {
        if (mmiServiceCode === MMI_KS_SC_PIN ||
            mmiServiceCode === MMI_KS_SC_PIN2) {
          options.errorMsg = MMI_ERROR_KS_BAD_PIN;
        } else if (mmiServiceCode === MMI_KS_SC_PUK ||
                   mmiServiceCode === MMI_KS_SC_PUK2) {
          options.errorMsg = MMI_ERROR_KS_BAD_PUK;
        }
        if (options.retryCount !== undefined) {
          options.additionalInformation = options.retryCount;
        }
      }
    }

    this.sendChromeMessage(options);
  },

  // We combine all of the NETWORK_INFO_MESSAGE_TYPES into one "networkinfochange"
  // message to the RadioInterfaceLayer, so we can avoid sending multiple
  // VoiceInfoChanged events for both operator / voice_data_registration
  //
  // State management here is a little tricky. We need to know both:
  // 1. Whether or not a response was received for each of the
  //    NETWORK_INFO_MESSAGE_TYPES
  // 2. The outbound message that corresponds with that response -- but this
  //    only happens when internal state changes (i.e. it isn't guaranteed)
  //
  // To collect this state, each message response function first calls
  // _receivedNetworkInfo, to mark the response as received. When the
  // final response is received, a call to _sendPendingNetworkInfo is placed
  // on the next tick of the worker thread.
  //
  // Since the original call to _receivedNetworkInfo happens at the top
  // of the response handler, this gives the final handler a chance to
  // queue up it's "changed" message by calling _sendNetworkInfoMessage if/when
  // the internal state has actually changed.
  _sendNetworkInfoMessage: function _sendNetworkInfoMessage(type, message) {
    if (!this._processingNetworkInfo) {
      // We only combine these messages in the case of the combined request
      // in requestNetworkInfo()
      this.sendChromeMessage(message);
      return;
    }

    if (DEBUG) debug("Queuing " + type + " network info message: " + JSON.stringify(message));
    this._pendingNetworkInfo[type] = message;
  },

  _receivedNetworkInfo: function _receivedNetworkInfo(type) {
    if (DEBUG) debug("Received " + type + " network info.");
    if (!this._processingNetworkInfo) {
      return;
    }

    let pending = this._pendingNetworkInfo;

    // We still need to track states for events that aren't fired.
    if (!(type in pending)) {
      pending[type] = PENDING_NETWORK_TYPE;
    }

    // Pending network info is ready to be sent when no more messages
    // are waiting for responses, but the combined payload hasn't been sent.
    for (let i = 0; i < NETWORK_INFO_MESSAGE_TYPES.length; i++) {
      let msgType = NETWORK_INFO_MESSAGE_TYPES[i];
      if (!(msgType in pending)) {
        if (DEBUG) debug("Still missing some more network info, not notifying main thread.");
        return;
      }
    }

    // Do a pass to clean up the processed messages that didn't create
    // a response message, so we don't have unused keys in the outbound
    // networkinfochanged message.
    for (let key in pending) {
      if (pending[key] == PENDING_NETWORK_TYPE) {
        delete pending[key];
      }
    }

    if (DEBUG) debug("All pending network info has been received: " + JSON.stringify(pending));

    // Send the message on the next tick of the worker's loop, so we give the
    // last message a chance to call _sendNetworkInfoMessage first.
    setTimeout(this._sendPendingNetworkInfo, 0);
  },

  _sendPendingNetworkInfo: function _sendPendingNetworkInfo() {
    RIL.sendChromeMessage(RIL._pendingNetworkInfo);

    RIL._processingNetworkInfo = false;
    for (let i = 0; i < NETWORK_INFO_MESSAGE_TYPES.length; i++) {
      delete RIL._pendingNetworkInfo[NETWORK_INFO_MESSAGE_TYPES[i]];
    }

    if (RIL._needRepollNetworkInfo) {
      RIL._needRepollNetworkInfo = false;
      RIL.requestNetworkInfo();
    }
  },

  /**
   * Normalize the signal strength in dBm to the signal level from 0 to 100.
   *
   * @param signal
   *        The signal strength in dBm to normalize.
   * @param min
   *        The signal strength in dBm maps to level 0.
   * @param max
   *        The signal strength in dBm maps to level 100.
   *
   * @return level
   *         The signal level from 0 to 100.
   */
  _processSignalLevel: function _processSignalLevel(signal, min, max) {
    if (signal <= min) {
      return 0;
    }

    if (signal >= max) {
      return 100;
    }

    return Math.floor((signal - min) * 100 / (max - min));
  },

  _processSignalStrength: function _processSignalStrength(signal) {
    let info = {
      voice: {
        signalStrength:    null,
        relSignalStrength: null
      },
      data: {
        signalStrength:    null,
        relSignalStrength: null
      }
    };

    if (this._isCdma) {
      // CDMA RSSI.
      // Valid values are positive integers. This value is the actual RSSI value
      // multiplied by -1. Example: If the actual RSSI is -75, then this
      // response value will be 75.
      if (signal.cdmaDBM && signal.cdmaDBM > 0) {
        let signalStrength = -1 * signal.cdmaDBM;
        info.voice.signalStrength = signalStrength;

        // -105 and -70 are referred to AOSP's implementation. These values are
        // not constants and can be customized based on different requirement.
        let signalLevel = this._processSignalLevel(signalStrength, -105, -70);
        info.voice.relSignalStrength = signalLevel;
      }

      // EVDO RSSI.
      // Valid values are positive integers. This value is the actual RSSI value
      // multiplied by -1. Example: If the actual RSSI is -75, then this
      // response value will be 75.
      if (signal.evdoDBM && signal.evdoDBM > 0) {
        let signalStrength = -1 * signal.evdoDBM;
        info.data.signalStrength = signalStrength;

        // -105 and -70 are referred to AOSP's implementation. These values are
        // not constants and can be customized based on different requirement.
        let signalLevel = this._processSignalLevel(signalStrength, -105, -70);
        info.data.relSignalStrength = signalLevel;
      }
    } else {
      // GSM signal strength.
      // Valid values are 0-31 as defined in TS 27.007 8.5.
      // 0     : -113 dBm or less
      // 1     : -111 dBm
      // 2...30: -109...-53 dBm
      // 31    : -51 dBm
      if (signal.gsmSignalStrength &&
          signal.gsmSignalStrength >= 0 &&
          signal.gsmSignalStrength <= 31) {
        let signalStrength = -113 + 2 * signal.gsmSignalStrength;
        info.voice.signalStrength = info.data.signalStrength = signalStrength;

        // -115 and -85 are referred to AOSP's implementation. These values are
        // not constants and can be customized based on different requirement.
        let signalLevel = this._processSignalLevel(signalStrength, -110, -85);
        info.voice.relSignalStrength = info.data.relSignalStrength = signalLevel;
      }
    }

    info.rilMessageType = "signalstrengthchange";
    this._sendNetworkInfoMessage(NETWORK_INFO_SIGNAL, info);

    if (this.cachedDialRequest && info.voice.signalStrength) {
      // Radio is ready for making the cached emergency call.
      this.cachedDialRequest.callback();
      this.cachedDialRequest = null;
    }
  },

  /**
   * Process the network registration flags.
   *
   * @return true if the state changed, false otherwise.
   */
  _processCREG: function _processCREG(curState, newState) {
    let changed = false;

    let regState = RIL.parseInt(newState[0], NETWORK_CREG_STATE_UNKNOWN);
    if (curState.regState === undefined || curState.regState !== regState) {
      changed = true;
      curState.regState = regState;

      curState.state = NETWORK_CREG_TO_GECKO_MOBILE_CONNECTION_STATE[regState];
      curState.connected = regState == NETWORK_CREG_STATE_REGISTERED_HOME ||
                           regState == NETWORK_CREG_STATE_REGISTERED_ROAMING;
      curState.roaming = regState == NETWORK_CREG_STATE_REGISTERED_ROAMING;
      curState.emergencyCallsOnly = !curState.connected;
    }

    if (!curState.cell) {
      curState.cell = {};
    }

    // From TS 23.003, 0000 and 0xfffe are indicated that no valid LAI exists
    // in MS. So we still need to report the '0000' as well.
    let lac = RIL.parseInt(newState[1], -1, 16);
    if (curState.cell.gsmLocationAreaCode === undefined ||
        curState.cell.gsmLocationAreaCode !== lac) {
      curState.cell.gsmLocationAreaCode = lac;
      changed = true;
    }

    let cid = RIL.parseInt(newState[2], -1, 16);
    if (curState.cell.gsmCellId === undefined ||
        curState.cell.gsmCellId !== cid) {
      curState.cell.gsmCellId = cid;
      changed = true;
    }

    let radioTech = (newState[3] === undefined ?
                     NETWORK_CREG_TECH_UNKNOWN :
                     RIL.parseInt(newState[3], NETWORK_CREG_TECH_UNKNOWN));
    if (curState.radioTech === undefined || curState.radioTech !== radioTech) {
      changed = true;
      curState.radioTech = radioTech;
      curState.type = GECKO_RADIO_TECH[radioTech] || null;
    }
    return changed;
  },

  _processVoiceRegistrationState: function _processVoiceRegistrationState(state) {
    let rs = this.voiceRegistrationState;
    let stateChanged = this._processCREG(rs, state);
    if (stateChanged && rs.connected) {
      RIL.getSmscAddress();
    }

    let cell = rs.cell;
    if (this._isCdma) {
      // Some variables below are not used. Comment them instead of removing to
      // keep the information about state[x].
      let cdmaBaseStationId = RIL.parseInt(state[4], -1);
      let cdmaBaseStationLatitude = RIL.parseInt(state[5], -2147483648);
      let cdmaBaseStationLongitude = RIL.parseInt(state[6], -2147483648);
      // let cssIndicator = RIL.parseInt(state[7]);
      let cdmaSystemId = RIL.parseInt(state[8], -1);
      let cdmaNetworkId = RIL.parseInt(state[9], -1);
      // let roamingIndicator = RIL.parseInt(state[10]);
      // let systemIsInPRL = RIL.parseInt(state[11]);
      // let defaultRoamingIndicator = RIL.parseInt(state[12]);
      // let reasonForDenial = RIL.parseInt(state[13]);

      if (cell.cdmaBaseStationId !== cdmaBaseStationId ||
          cell.cdmaBaseStationLatitude !== cdmaBaseStationLatitude ||
          cell.cdmaBaseStationLongitude !== cdmaBaseStationLongitude ||
          cell.cdmaSystemId !== cdmaSystemId ||
          cell.cdmaNetworkId !== cdmaNetworkId) {
        stateChanged = true;
        cell.cdmaBaseStationId = cdmaBaseStationId;
        cell.cdmaBaseStationLatitude = cdmaBaseStationLatitude;
        cell.cdmaBaseStationLongitude = cdmaBaseStationLongitude;
        cell.cdmaSystemId = cdmaSystemId;
        cell.cdmaNetworkId = cdmaNetworkId;
      }
    }

    if (stateChanged) {
      rs.rilMessageType = "voiceregistrationstatechange";
      this._sendNetworkInfoMessage(NETWORK_INFO_VOICE_REGISTRATION_STATE, rs);
    }
  },

  _processDataRegistrationState: function _processDataRegistrationState(state) {
    let rs = this.dataRegistrationState;
    let stateChanged = this._processCREG(rs, state);
    if (stateChanged) {
      rs.rilMessageType = "dataregistrationstatechange";
      this._sendNetworkInfoMessage(NETWORK_INFO_DATA_REGISTRATION_STATE, rs);
    }
  },

  _processOperator: function _processOperator(operatorData) {
    if (operatorData.length < 3) {
      if (DEBUG) {
        debug("Expected at least 3 strings for operator.");
      }
    }

    if (!this.operator) {
      this.operator = {
        rilMessageType: "operatorchange",
        longName: null,
        shortName: null
      };
    }

    let [longName, shortName, networkTuple] = operatorData;
    let thisTuple = (this.operator.mcc || "") + (this.operator.mnc || "");

    if (this.operator.longName !== longName ||
        this.operator.shortName !== shortName ||
        thisTuple !== networkTuple) {

      this.operator.mcc = null;
      this.operator.mnc = null;

      if (networkTuple) {
        try {
          this._processNetworkTuple(networkTuple, this.operator);
        } catch (e) {
          if (DEBUG) debug("Error processing operator tuple: " + e);
        }
      } else {
        // According to ril.h, the operator fields will be NULL when the operator
        // is not currently registered. We can avoid trying to parse the numeric
        // tuple in that case.
        if (DEBUG) {
          debug("Operator is currently unregistered");
        }
      }

      let networkName;
      // We won't get network name using PNN and OPL if voice registration isn't ready
      if (this.voiceRegistrationState.cell &&
          this.voiceRegistrationState.cell.gsmLocationAreaCode != -1) {
        networkName = ICCUtilsHelper.getNetworkNameFromICC(
          this.operator.mcc,
          this.operator.mnc,
          this.voiceRegistrationState.cell.gsmLocationAreaCode);
      }

      if (networkName) {
        if (DEBUG) {
          debug("Operator names will be overriden: " +
                "longName = " + networkName.fullName + ", " +
                "shortName = " + networkName.shortName);
        }

        this.operator.longName = networkName.fullName;
        this.operator.shortName = networkName.shortName;
      } else {
        this.operator.longName = longName;
        this.operator.shortName = shortName;
      }

      if (ICCUtilsHelper.updateDisplayCondition()) {
        ICCUtilsHelper.handleICCInfoChange();
      }
      this._sendNetworkInfoMessage(NETWORK_INFO_OPERATOR, this.operator);
    }
  },

  /**
   * Helpers for processing call state and handle the active call.
   */
  _processCalls: function _processCalls(newCalls) {
    let conferenceChanged = false;
    let clearConferenceRequest = false;

    // Go through the calls we currently have on file and see if any of them
    // changed state. Remove them from the newCalls map as we deal with them
    // so that only new calls remain in the map after we're done.
    for each (let currentCall in this.currentCalls) {
      let newCall;
      if (newCalls) {
        newCall = newCalls[currentCall.callIndex];
        delete newCalls[currentCall.callIndex];
      }

      if (!newCall) {
        // Call is no longer reported by the radio. Remove from our map and
        // send disconnected state change.

        if (this.currentConference.participants[currentCall.callIndex]) {
          conferenceChanged = true;
          currentCall.isConference = false;
          delete this.currentConference.participants[currentCall.callIndex];
          delete this.currentCalls[currentCall.callIndex];
          // We don't query the fail cause here as it triggers another asynchrouns
          // request that leads to a problem of updating all conferece participants
          // in one task.
          this._handleDisconnectedCall(currentCall);
        } else {
          delete this.currentCalls[currentCall.callIndex];
          this.getFailCauseCode(currentCall);
        }
        continue;
      }

      // Call is still valid.
      if (newCall.state == currentCall.state &&
          newCall.isMpty == currentCall.isMpty) {
        continue;
      }

      // State has changed.
      if (newCall.state == CALL_STATE_INCOMING &&
          currentCall.state == CALL_STATE_WAITING) {
        // Update the call internally but we don't notify chrome since these two
        // states are viewed as the same one there.
        currentCall.state = newCall.state;
        continue;
      }

      if (!currentCall.started && newCall.state == CALL_STATE_ACTIVE) {
        currentCall.started = new Date().getTime();
      }

      if (currentCall.isMpty == newCall.isMpty &&
          newCall.state != currentCall.state) {
        currentCall.state = newCall.state;
        if (currentCall.isConference) {
          conferenceChanged = true;
        }
        this._handleChangedCallState(currentCall);
        continue;
      }

      // '.isMpty' becomes false when the conference call is put on hold.
      // We need to introduce additional 'isConference' to correctly record the
      // real conference status

      // Update a possible conference participant when .isMpty changes.
      if (!currentCall.isMpty && newCall.isMpty) {
        if (this._hasConferenceRequest) {
          conferenceChanged = true;
          clearConferenceRequest = true;
          currentCall.state = newCall.state;
          currentCall.isMpty = newCall.isMpty;
          currentCall.isConference = true;
          this.currentConference.participants[currentCall.callIndex] = currentCall;
          this._handleChangedCallState(currentCall);
        } else if (currentCall.isConference) {
          // The case happens when resuming a held conference call.
          conferenceChanged = true;
          currentCall.state = newCall.state;
          currentCall.isMpty = newCall.isMpty;
          this.currentConference.participants[currentCall.callIndex] = currentCall;
          this._handleChangedCallState(currentCall);
        } else {
          // Weird. This sometimes happens when we switch two calls, but it is
          // not a conference call.
          currentCall.state = newCall.state;
          this._handleChangedCallState(currentCall);
        }
      } else if (currentCall.isMpty && !newCall.isMpty) {
        if (!this.currentConference.participants[newCall.callIndex]) {
          continue;
        }

        // '.isMpty' of a conference participant is set to false by rild when
        // the conference call is put on hold. We don't actually know if the call
        // still attends the conference until updating all calls finishes. We
        // cache it for further determination.
        if (newCall.state != CALL_STATE_HOLDING) {
          delete this.currentConference.participants[newCall.callIndex];
          currentCall.state = newCall.state;
          currentCall.isMpty = newCall.isMpty;
          currentCall.isConference = false;
          conferenceChanged = true;
          this._handleChangedCallState(currentCall);
          continue;
        }

        if (!this.currentConference.cache) {
          this.currentConference.cache = {};
        }
        this.currentConference.cache[currentCall.callIndex] = newCall;
        currentCall.state = newCall.state;
        currentCall.isMpty = newCall.isMpty;
        conferenceChanged = true;
      }
    }

    // Go through any remaining calls that are new to us.
    for each (let newCall in newCalls) {
      if (newCall.isVoice) {
        // Format international numbers appropriately.
        if (newCall.number &&
            newCall.toa == TOA_INTERNATIONAL &&
            newCall.number[0] != "+") {
          newCall.number = "+" + newCall.number;
        }

        if (newCall.state == CALL_STATE_INCOMING) {
          newCall.isOutgoing = false;
        } else if (newCall.state == CALL_STATE_DIALING) {
          newCall.isOutgoing = true;
        }

        // Set flag for outgoing emergency call.
        if (newCall.isOutgoing && this._isEmergencyNumber(newCall.number)) {
          newCall.isEmergency = true;
        } else {
          newCall.isEmergency = false;
        }

        // Add to our map.
        if (newCall.isMpty) {
          conferenceChanged = true;
          newCall.isConference = true;
          this.currentConference.participants[newCall.callIndex] = newCall;
        } else {
          newCall.isConference = false;
        }
        this._handleChangedCallState(newCall);
        this.currentCalls[newCall.callIndex] = newCall;
      }
    }

    if (clearConferenceRequest) {
      this._hasConferenceRequest = false;
    }
    if (conferenceChanged) {
      this._ensureConference();
    }
  },

  _ensureConference: function _ensureConference() {
    let oldState = this.currentConference.state;
    let remaining = Object.keys(this.currentConference.participants);

    if (remaining.length == 1) {
      // Remove that if only does one remain in a conference call.
      let call = this.currentCalls[remaining[0]];
      call.isConference = false;
      this._handleChangedCallState(call);
      delete this.currentConference.participants[call.callIndex];
    } else if (remaining.length > 1) {
      for each (let call in this.currentConference.cache) {
        call.isConference = true;
        this.currentConference.participants[call.callIndex] = call;
        this.currentCalls[call.callIndex] = call;
        this._handleChangedCallState(call);
      }
    }
    delete this.currentConference.cache;

    // Update the conference call's state.
    let state = CALL_STATE_UNKNOWN;
    for each (let call in this.currentConference.participants) {
      if (state != CALL_STATE_UNKNOWN && state != call.state) {
        // Each participant should have the same state, otherwise something
        // wrong happens.
        state = CALL_STATE_UNKNOWN;
        break;
      }
      state = call.state;
    }
    if (oldState != state) {
      this.currentConference.state = state;
      let message = {rilMessageType: "conferenceCallStateChanged",
                     state: state};
      this.sendChromeMessage(message);
    }
  },

  _handleChangedCallState: function _handleChangedCallState(changedCall) {
    let message = {rilMessageType: "callStateChange",
                   call: changedCall};
    this.sendChromeMessage(message);
  },

  _handleDisconnectedCall: function _handleDisconnectedCall(disconnectedCall) {
    let message = {rilMessageType: "callDisconnected",
                   call: disconnectedCall};
    this.sendChromeMessage(message);
  },

  _sendCallError: function _sendCallError(callIndex, errorMsg) {
    this.sendChromeMessage({rilMessageType: "callError",
                           callIndex: callIndex,
                           errorMsg: errorMsg});
  },

  _sendDataCallError: function _sendDataCallError(message, errorCode) {
    // Should not include token for unsolicited response.
    delete message.rilMessageToken;
    message.rilMessageType = "datacallerror";
    if (errorCode == ERROR_GENERIC_FAILURE) {
      message.errorMsg = RIL_ERROR_TO_GECKO_ERROR[errorCode];
    } else {
      message.errorMsg = RIL_DATACALL_FAILCAUSE_TO_GECKO_DATACALL_ERROR[errorCode];
    }
    this.sendChromeMessage(message);
  },

  _compareDataCallLink: function _compareDataCallLink(source, target) {
    if (source.ifname != target.ifname ||
        source.ipaddr != target.ipaddr ||
        source.gw != target.gw) {
      return false;
    }

    // Compare <datacall>.dns.
    let sdns = source.dns, tdns = target.dns;
    if (sdns.length != tdns.length) {
      return false;
    }
    for (let i = 0; i < sdns.length; i++) {
      if (sdns[i] != tdns[i]) {
        return false;
      }
    }

    return true;
  },

  _processDataCallList: function _processDataCallList(datacalls, newDataCallOptions) {
    // Check for possible PDP errors: We check earlier because the datacall
    // can be removed if is the same as the current one.
    for each (let newDataCall in datacalls) {
      if (newDataCall.status != DATACALL_FAIL_NONE) {
        if (newDataCallOptions) {
          newDataCall.apn = newDataCallOptions.apn;
        }
        this._sendDataCallError(newDataCall, newDataCall.status);
      }
    }

    for each (let currentDataCall in this.currentDataCalls) {
      let updatedDataCall;
      if (datacalls) {
        updatedDataCall = datacalls[currentDataCall.cid];
        delete datacalls[currentDataCall.cid];
      }

      if (!updatedDataCall) {
        // If datacalls list is coming from REQUEST_SETUP_DATA_CALL response,
        // we do not change state for any currentDataCalls not in datacalls list.
        if (!newDataCallOptions) {
          delete this.currentDataCalls[currentDataCall.cid];
          currentDataCall.state = GECKO_NETWORK_STATE_DISCONNECTED;
          currentDataCall.rilMessageType = "datacallstatechange";
          this.sendChromeMessage(currentDataCall);
        }
        continue;
      }

      if (updatedDataCall && !updatedDataCall.ifname) {
        delete this.currentDataCalls[currentDataCall.cid];
        currentDataCall.state = GECKO_NETWORK_STATE_UNKNOWN;
        currentDataCall.rilMessageType = "datacallstatechange";
        this.sendChromeMessage(currentDataCall);
        continue;
      }

      this._setDataCallGeckoState(updatedDataCall);
      if (updatedDataCall.state != currentDataCall.state) {
        if (updatedDataCall.state == GECKO_NETWORK_STATE_DISCONNECTED) {
          delete this.currentDataCalls[currentDataCall.cid];
        }
        currentDataCall.status = updatedDataCall.status;
        currentDataCall.active = updatedDataCall.active;
        currentDataCall.state = updatedDataCall.state;
        currentDataCall.rilMessageType = "datacallstatechange";
        this.sendChromeMessage(currentDataCall);
        continue;
      }

      // State not changed, now check links.
      if (this._compareDataCallLink(updatedDataCall, currentDataCall)) {
        if(DEBUG) debug("No changes in data call.");
        continue;
      }
      if ((updatedDataCall.ifname != currentDataCall.ifname) ||
          (updatedDataCall.ipaddr != currentDataCall.ipaddr)) {
        if(DEBUG) debug("Data link changed, cleanup.");
        this.deactivateDataCall(currentDataCall);
        continue;
      }
      // Minor change, just update and notify.
      if(DEBUG) debug("Data link minor change, just update and notify.");
      currentDataCall.gw = updatedDataCall.gw;
      if (updatedDataCall.dns) {
        currentDataCall.dns[0] = updatedDataCall.dns[0];
        currentDataCall.dns[1] = updatedDataCall.dns[1];
      }
      currentDataCall.rilMessageType = "datacallstatechange";
      this.sendChromeMessage(currentDataCall);
    }

    for each (let newDataCall in datacalls) {
      if (!newDataCall.ifname) {
        continue;
      }

      if (!newDataCallOptions) {
        if (DEBUG) debug("Unexpected new data call: " + JSON.stringify(newDataCall));
        continue;
      }

      this.currentDataCalls[newDataCall.cid] = newDataCall;
      this._setDataCallGeckoState(newDataCall);

      newDataCall.radioTech = newDataCallOptions.radioTech;
      newDataCall.apn = newDataCallOptions.apn;
      newDataCall.user = newDataCallOptions.user;
      newDataCall.passwd = newDataCallOptions.passwd;
      newDataCall.chappap = newDataCallOptions.chappap;
      newDataCall.pdptype = newDataCallOptions.pdptype;
      newDataCallOptions = null;

      newDataCall.rilMessageType = "datacallstatechange";
      this.sendChromeMessage(newDataCall);
    }
  },

  _setDataCallGeckoState: function _setDataCallGeckoState(datacall) {
    switch (datacall.active) {
      case DATACALL_INACTIVE:
        datacall.state = GECKO_NETWORK_STATE_DISCONNECTED;
        break;
      case DATACALL_ACTIVE_DOWN:
      case DATACALL_ACTIVE_UP:
        datacall.state = GECKO_NETWORK_STATE_CONNECTED;
        break;
    }
  },

  _processSuppSvcNotification: function _processSuppSvcNotification(info) {
    debug("handle supp svc notification: " + JSON.stringify(info));

    let notification = null;
    let callIndex = -1;

    if (info.notificationType === 0) {
      // MO intermediate result code. Refer to code1 defined in 3GPP 27.007
      // 7.17.
    } else if (info.notificationType === 1) {
      // MT unsolicited result code. Refer to code2 defined in 3GPP 27.007 7.17.
      switch (info.code) {
        case SUPP_SVC_NOTIFICATION_CODE2_PUT_ON_HOLD:
        case SUPP_SVC_NOTIFICATION_CODE2_RETRIEVED:
          notification = GECKO_SUPP_SVC_NOTIFICATION_FROM_CODE2[info.code];
          break;
        default:
          // Notification type not supported.
          return;
      }

      // Get the target call object for this notification.
      let currentCallIndexes = Object.keys(this.currentCalls);
      if (currentCallIndexes.length === 1) {
        // Only one call exists. This should be the target.
        callIndex = currentCallIndexes[0];
      } else {
        // Find the call in |currentCalls| by the given number.
        if (info.number) {
          for each (let currentCall in this.currentCalls) {
            if (currentCall.number == info.number) {
              callIndex = currentCall.callIndex;
              break;
            }
          }
        }
      }
    }

    let message = {rilMessageType: "suppSvcNotification",
                   notification: notification,
                   callIndex: callIndex};
    this.sendChromeMessage(message);
  },

  _cancelEmergencyCbModeTimeout: function _cancelEmergencyCbModeTimeout() {
    if (this._exitEmergencyCbModeTimeoutID) {
      clearTimeout(this._exitEmergencyCbModeTimeoutID);
      this._exitEmergencyCbModeTimeoutID = null;
    }
  },

  _handleChangedEmergencyCbMode: function _handleChangedEmergencyCbMode(active) {
    this._isInEmergencyCbMode = active;

    // Clear the existed timeout event.
    this._cancelEmergencyCbModeTimeout();

    // Start a new timeout event when entering the mode.
    if (active) {
      this._exitEmergencyCbModeTimeoutID = setTimeout(
          this.exitEmergencyCbMode.bind(this), EMERGENCY_CB_MODE_TIMEOUT_MS);
    }

    let message = {rilMessageType: "emergencyCbModeChange",
                   active: active,
                   timeoutMs: EMERGENCY_CB_MODE_TIMEOUT_MS};
    this.sendChromeMessage(message);
  },

  _processNetworks: function _processNetworks() {
    let strings = Buf.readStringList();
    let networks = [];

    for (let i = 0; i < strings.length; i += 4) {
      let network = {
        longName: strings[i],
        shortName: strings[i + 1],
        mcc: null,
        mnc: null,
        state: null
      };

      let networkTuple = strings[i + 2];
      try {
        this._processNetworkTuple(networkTuple, network);
      } catch (e) {
        if (DEBUG) debug("Error processing operator tuple: " + e);
      }

      let state = strings[i + 3];
      if (state === NETWORK_STATE_UNKNOWN) {
        // TODO: looks like this might conflict in style with
        // GECKO_NETWORK_STYLE_UNKNOWN / nsINetworkManager
        state = GECKO_QAN_STATE_UNKNOWN;
      }

      network.state = state;
      networks.push(network);
    }
    return networks;
  },

  /**
   * The "numeric" portion of the operator info is a tuple
   * containing MCC (country code) and MNC (network code).
   * AFAICT, MCC should always be 3 digits, making the remaining
   * portion the MNC.
   */
  _processNetworkTuple: function _processNetworkTuple(networkTuple, network) {
    let tupleLen = networkTuple.length;

    if (tupleLen == 5 || tupleLen == 6) {
      network.mcc = networkTuple.substr(0, 3);
      network.mnc = networkTuple.substr(3);
    } else {
      network.mcc = null;
      network.mnc = null;

      throw new Error("Invalid network tuple (should be 5 or 6 digits): " + networkTuple);
    }
  },

  /**
   * Process radio technology change.
   */
  _processRadioTech: function _processRadioTech(radioTech) {
    let isCdma = true;
    this.radioTech = radioTech;

    switch(radioTech) {
      case NETWORK_CREG_TECH_GPRS:
      case NETWORK_CREG_TECH_EDGE:
      case NETWORK_CREG_TECH_UMTS:
      case NETWORK_CREG_TECH_HSDPA:
      case NETWORK_CREG_TECH_HSUPA:
      case NETWORK_CREG_TECH_HSPA:
      case NETWORK_CREG_TECH_LTE:
      case NETWORK_CREG_TECH_HSPAP:
      case NETWORK_CREG_TECH_GSM:
        isCdma = false;
    }

    if (DEBUG) {
      debug("Radio tech is set to: " + GECKO_RADIO_TECH[radioTech] +
            ", it is a " + (isCdma?"cdma":"gsm") + " technology");
    }

    // We should request SIM information when
    //  1. Radio state has been changed, so we are waiting for radioTech or
    //  2. isCdma is different from this._isCdma.
    if (this._waitingRadioTech || isCdma != this._isCdma) {
      this._isCdma = isCdma;
      this._waitingRadioTech = false;
      if (this._isCdma) {
        this.getDeviceIdentity();
      } else {
        this.getIMEI();
        this.getIMEISV();
      }
       this.getICCStatus();
    }
  },

  /**
   * Helper for returning the TOA for the given dial string.
   */
  _toaFromString: function _toaFromString(number) {
    let toa = TOA_UNKNOWN;
    if (number && number.length > 0 && number[0] == '+') {
      toa = TOA_INTERNATIONAL;
    }
    return toa;
  },

  /**
   * Helper for translating basic service group to call forwarding service class
   * parameter.
   */
  _siToServiceClass: function _siToServiceClass(si) {
    if (!si) {
      return ICC_SERVICE_CLASS_NONE;
    }

    let serviceCode = parseInt(si, 10);
    switch (serviceCode) {
      case 10:
        return ICC_SERVICE_CLASS_SMS + ICC_SERVICE_CLASS_FAX  + ICC_SERVICE_CLASS_VOICE;
      case 11:
        return ICC_SERVICE_CLASS_VOICE;
      case 12:
        return ICC_SERVICE_CLASS_SMS + ICC_SERVICE_CLASS_FAX;
      case 13:
        return ICC_SERVICE_CLASS_FAX;
      case 16:
        return ICC_SERVICE_CLASS_SMS;
      case 19:
        return ICC_SERVICE_CLASS_FAX + ICC_SERVICE_CLASS_VOICE;
      case 21:
        return ICC_SERVICE_CLASS_PAD + ICC_SERVICE_CLASS_DATA_ASYNC;
      case 22:
        return ICC_SERVICE_CLASS_PACKET + ICC_SERVICE_CLASS_DATA_SYNC;
      case 25:
        return ICC_SERVICE_CLASS_DATA_ASYNC;
      case 26:
        return ICC_SERVICE_CLASS_DATA_SYNC + SERVICE_CLASS_VOICE;
      case 99:
        return ICC_SERVICE_CLASS_PACKET;
      default:
        return ICC_SERVICE_CLASS_NONE;
    }
  },

  /**
   * @param message A decoded SMS-DELIVER message.
   *
   * @see 3GPP TS 31.111 section 7.1.1
   */
  dataDownloadViaSMSPP: function dataDownloadViaSMSPP(message) {
    let options = {
      pid: message.pid,
      dcs: message.dcs,
      encoding: message.encoding,
    };
    Buf.newParcel(REQUEST_STK_SEND_ENVELOPE_WITH_STATUS, options);

    Buf.seekIncoming(-1 * (Buf.getCurrentParcelSize() - Buf.getReadAvailable()
                           - 2 * Buf.UINT32_SIZE)); // Skip response_type & request_type.
    let messageStringLength = Buf.readInt32(); // In semi-octets
    let smscLength = GsmPDUHelper.readHexOctet(); // In octets, inclusive of TOA
    let tpduLength = (messageStringLength / 2) - (smscLength + 1); // In octets

    // Device identities: 4 bytes
    // Address: 0 or (2 + smscLength)
    // SMS TPDU: (2 or 3) + tpduLength
    let berLen = 4 +
                 (smscLength ? (2 + smscLength) : 0) +
                 (tpduLength <= 127 ? 2 : 3) + tpduLength; // In octets

    let parcelLength = (berLen <= 127 ? 2 : 3) + berLen; // In octets
    Buf.writeInt32(parcelLength * 2); // In semi-octets

    // Write a BER-TLV
    GsmPDUHelper.writeHexOctet(BER_SMS_PP_DOWNLOAD_TAG);
    if (berLen > 127) {
      GsmPDUHelper.writeHexOctet(0x81);
    }
    GsmPDUHelper.writeHexOctet(berLen);

    // Device Identifies-TLV
    GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_DEVICE_ID |
                               COMPREHENSIONTLV_FLAG_CR);
    GsmPDUHelper.writeHexOctet(0x02);
    GsmPDUHelper.writeHexOctet(STK_DEVICE_ID_NETWORK);
    GsmPDUHelper.writeHexOctet(STK_DEVICE_ID_SIM);

    // Address-TLV
    if (smscLength) {
      GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_ADDRESS);
      GsmPDUHelper.writeHexOctet(smscLength);
      Buf.copyIncomingToOutgoing(Buf.PDU_HEX_OCTET_SIZE * smscLength);
    }

    // SMS TPDU-TLV
    GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_SMS_TPDU |
                               COMPREHENSIONTLV_FLAG_CR);
    if (tpduLength > 127) {
      GsmPDUHelper.writeHexOctet(0x81);
    }
    GsmPDUHelper.writeHexOctet(tpduLength);
    Buf.copyIncomingToOutgoing(Buf.PDU_HEX_OCTET_SIZE * tpduLength);

    // Write 2 string delimitors for the total string length must be even.
    Buf.writeStringDelimiter(0);

    Buf.sendParcel();
  },

  /**
   * @param success A boolean value indicating the result of previous
   *                SMS-DELIVER message handling.
   * @param responsePduLen ICC IO response PDU length in octets.
   * @param options An object that contains four attributes: `pid`, `dcs`,
   *                `encoding` and `responsePduLen`.
   *
   * @see 3GPP TS 23.040 section 9.2.2.1a
   */
  acknowledgeIncomingGsmSmsWithPDU: function acknowledgeIncomingGsmSmsWithPDU(success, responsePduLen, options) {
    Buf.newParcel(REQUEST_ACKNOWLEDGE_INCOMING_GSM_SMS_WITH_PDU);

    // Two strings.
    Buf.writeInt32(2);

    // String 1: Success
    Buf.writeString(success ? "1" : "0");

    // String 2: RP-ACK/RP-ERROR PDU
    Buf.writeInt32(2 * (responsePduLen + (success ? 5 : 6))); // In semi-octet
    // 1. TP-MTI & TP-UDHI
    GsmPDUHelper.writeHexOctet(PDU_MTI_SMS_DELIVER);
    if (!success) {
      // 2. TP-FCS
      GsmPDUHelper.writeHexOctet(PDU_FCS_USIM_DATA_DOWNLOAD_ERROR);
    }
    // 3. TP-PI
    GsmPDUHelper.writeHexOctet(PDU_PI_USER_DATA_LENGTH |
                               PDU_PI_DATA_CODING_SCHEME |
                               PDU_PI_PROTOCOL_IDENTIFIER);
    // 4. TP-PID
    GsmPDUHelper.writeHexOctet(options.pid);
    // 5. TP-DCS
    GsmPDUHelper.writeHexOctet(options.dcs);
    // 6. TP-UDL
    if (options.encoding == PDU_DCS_MSG_CODING_7BITS_ALPHABET) {
      GsmPDUHelper.writeHexOctet(Math.floor(responsePduLen * 8 / 7));
    } else {
      GsmPDUHelper.writeHexOctet(responsePduLen);
    }
    // TP-UD
    Buf.copyIncomingToOutgoing(Buf.PDU_HEX_OCTET_SIZE * responsePduLen);
    // Write 2 string delimitors for the total string length must be even.
    Buf.writeStringDelimiter(0);

    Buf.sendParcel();
  },

  /**
   * @param message A decoded SMS-DELIVER message.
   */
  writeSmsToSIM: function writeSmsToSIM(message) {
    Buf.newParcel(REQUEST_WRITE_SMS_TO_SIM);

    // Write EFsms Status
    Buf.writeInt32(EFSMS_STATUS_FREE);

    Buf.seekIncoming(-1 * (Buf.getCurrentParcelSize() - Buf.getReadAvailable()
                           - 2 * Buf.UINT32_SIZE)); // Skip response_type & request_type.
    let messageStringLength = Buf.readInt32(); // In semi-octets
    let smscLength = GsmPDUHelper.readHexOctet(); // In octets, inclusive of TOA
    let pduLength = (messageStringLength / 2) - (smscLength + 1); // In octets

    // 1. Write PDU first.
    if (smscLength > 0) {
      Buf.seekIncoming(smscLength * Buf.PDU_HEX_OCTET_SIZE);
    }
    // Write EFsms PDU string length
    Buf.writeInt32(2 * pduLength); // In semi-octets
    if (pduLength) {
      Buf.copyIncomingToOutgoing(Buf.PDU_HEX_OCTET_SIZE * pduLength);
    }
    // Write 2 string delimitors for the total string length must be even.
    Buf.writeStringDelimiter(0);

    // 2. Write SMSC
    // Write EFsms SMSC string length
    Buf.writeInt32(2 * (smscLength + 1)); // Plus smscLength itself, in semi-octets
    // Write smscLength
    GsmPDUHelper.writeHexOctet(smscLength);
    // Write TOA & SMSC Address
    if (smscLength) {
      Buf.seekIncoming(-1 * (Buf.getCurrentParcelSize() - Buf.getReadAvailable()
                             - 2 * Buf.UINT32_SIZE // Skip response_type, request_type.
                             - 2 * Buf.PDU_HEX_OCTET_SIZE)); // Skip messageStringLength & smscLength.
      Buf.copyIncomingToOutgoing(Buf.PDU_HEX_OCTET_SIZE * smscLength);
    }
    // Write 2 string delimitors for the total string length must be even.
    Buf.writeStringDelimiter(0);

    Buf.sendParcel();
  },

  /**
   * Helper for processing multipart SMS.
   *
   * @param message
   *        Received sms message.
   *
   * @return A failure cause defined in 3GPP 23.040 clause 9.2.3.22.
   */
  _processSmsMultipart: function _processSmsMultipart(message) {
    if (message.header && message.header.segmentMaxSeq &&
        (message.header.segmentMaxSeq > 1)) {
      message = this._processReceivedSmsSegment(message);
    } else {
      if (message.encoding == PDU_DCS_MSG_CODING_8BITS_ALPHABET) {
        message.fullData = message.data;
        delete message.data;
      } else {
        message.fullBody = message.body;
        delete message.body;
      }
    }

    if (message) {
      message.result = PDU_FCS_OK;
      if (message.messageClass == GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_2]) {
        // `MS shall ensure that the message has been to the SMS data field in
        // the (U)SIM before sending an ACK to the SC.`  ~ 3GPP 23.038 clause 4
        message.result = PDU_FCS_RESERVED;
      }

      if (message.messageType == PDU_CDMA_MSG_TYPE_BROADCAST) {
        message.rilMessageType = "broadcastsms-received";
      } else {
        message.rilMessageType = "sms-received";
      }

      this.sendChromeMessage(message);

      // We will acknowledge receipt of the SMS after we try to store it
      // in the database.
      return MOZ_FCS_WAIT_FOR_EXPLICIT_ACK;
    }

    return PDU_FCS_OK;
  },

  /**
   * Helper for processing SMS-STATUS-REPORT PDUs.
   *
   * @param length
   *        Length of SMS string in the incoming parcel.
   *
   * @return A failure cause defined in 3GPP 23.040 clause 9.2.3.22.
   */
  _processSmsStatusReport: function _processSmsStatusReport(length) {
    let [message, result] = GsmPDUHelper.processReceivedSms(length);
    if (!message) {
      if (DEBUG) debug("invalid SMS-STATUS-REPORT");
      return PDU_FCS_UNSPECIFIED;
    }

    let options = this._pendingSentSmsMap[message.messageRef];
    if (!options) {
      if (DEBUG) debug("no pending SMS-SUBMIT message");
      return PDU_FCS_OK;
    }

    let status = message.status;

    // 3GPP TS 23.040 9.2.3.15 `The MS shall interpret any reserved values as
    // "Service Rejected"(01100011) but shall store them exactly as received.`
    if ((status >= 0x80)
        || ((status >= PDU_ST_0_RESERVED_BEGIN)
            && (status < PDU_ST_0_SC_SPECIFIC_BEGIN))
        || ((status >= PDU_ST_1_RESERVED_BEGIN)
            && (status < PDU_ST_1_SC_SPECIFIC_BEGIN))
        || ((status >= PDU_ST_2_RESERVED_BEGIN)
            && (status < PDU_ST_2_SC_SPECIFIC_BEGIN))
        || ((status >= PDU_ST_3_RESERVED_BEGIN)
            && (status < PDU_ST_3_SC_SPECIFIC_BEGIN))
        ) {
      status = PDU_ST_3_SERVICE_REJECTED;
    }

    // Pending. Waiting for next status report.
    if ((status >>> 5) == 0x01) {
      if (DEBUG) debug("SMS-STATUS-REPORT: delivery still pending");
      return PDU_FCS_OK;
    }

    delete this._pendingSentSmsMap[message.messageRef];

    let deliveryStatus = ((status >>> 5) === 0x00)
                       ? GECKO_SMS_DELIVERY_STATUS_SUCCESS
                       : GECKO_SMS_DELIVERY_STATUS_ERROR;
    this.sendChromeMessage({
      rilMessageType: options.rilMessageType,
      rilMessageToken: options.rilMessageToken,
      deliveryStatus: deliveryStatus
    });

    return PDU_FCS_OK;
  },

  /**
   * Helper for processing CDMA SMS Delivery Acknowledgment Message
   *
   * @param message
   *        decoded SMS Delivery ACK message from CdmaPDUHelper.
   *
   * @return A failure cause defined in 3GPP 23.040 clause 9.2.3.22.
   */
  _processCdmaSmsStatusReport: function _processCdmaSmsStatusReport(message) {
    let options = this._pendingSentSmsMap[message.msgId];
    if (!options) {
      if (DEBUG) debug("no pending SMS-SUBMIT message");
      return PDU_FCS_OK;
    }

    if (message.errorClass === 2) {
      if (DEBUG) debug("SMS-STATUS-REPORT: delivery still pending, msgStatus: " + message.msgStatus);
      return PDU_FCS_OK;
    }

    delete this._pendingSentSmsMap[message.msgId];

    if (message.errorClass === -1 && message.body) {
      // Process as normal incoming SMS, if errorClass is invalid
      // but message body is available.
      return this._processSmsMultipart(message);
    }

    let deliveryStatus = (message.errorClass === 0)
                       ? GECKO_SMS_DELIVERY_STATUS_SUCCESS
                       : GECKO_SMS_DELIVERY_STATUS_ERROR;
    this.sendChromeMessage({
      rilMessageType: options.rilMessageType,
      rilMessageToken: options.rilMessageToken,
      deliveryStatus: deliveryStatus
    });

    return PDU_FCS_OK;
  },

  /**
   * Helper for processing received multipart SMS.
   *
   * @return null for handled segments, and an object containing full message
   *         body/data once all segments are received.
   */
  _processReceivedSmsSegment: function _processReceivedSmsSegment(original) {
    let hash = original.sender + ":" + original.header.segmentRef;
    let seq = original.header.segmentSeq;

    let options = this._receivedSmsSegmentsMap[hash];
    if (!options) {
      options = original;
      this._receivedSmsSegmentsMap[hash] = options;

      options.segmentMaxSeq = original.header.segmentMaxSeq;
      options.receivedSegments = 0;
      options.segments = [];
    } else if (options.segments[seq]) {
      // Duplicated segment?
      if (DEBUG) {
        debug("Got duplicated segment no." + seq + " of a multipart SMS: "
              + JSON.stringify(original));
      }
      return null;
    }

    if (options.encoding == PDU_DCS_MSG_CODING_8BITS_ALPHABET) {
      options.segments[seq] = original.data;
      delete original.data;
    } else {
      options.segments[seq] = original.body;
      delete original.body;
    }
    options.receivedSegments++;
    if (options.receivedSegments < options.segmentMaxSeq) {
      if (DEBUG) {
        debug("Got segment no." + seq + " of a multipart SMS: "
              + JSON.stringify(options));
      }
      return null;
    }

    // Remove from map
    delete this._receivedSmsSegmentsMap[hash];

    // Rebuild full body
    if (options.encoding == PDU_DCS_MSG_CODING_8BITS_ALPHABET) {
      // Uint8Array doesn't have `concat`, so we have to merge all segements
      // by hand.
      let fullDataLen = 0;
      for (let i = 1; i <= options.segmentMaxSeq; i++) {
        fullDataLen += options.segments[i].length;
      }

      options.fullData = new Uint8Array(fullDataLen);
      for (let d= 0, i = 1; i <= options.segmentMaxSeq; i++) {
        let data = options.segments[i];
        for (let j = 0; j < data.length; j++) {
          options.fullData[d++] = data[j];
        }
      }
    } else {
      options.fullBody = options.segments.join("");
    }

    if (DEBUG) {
      debug("Got full multipart SMS: " + JSON.stringify(options));
    }

    return options;
  },

  /**
   * Helper for processing sent multipart SMS.
   */
  _processSentSmsSegment: function _processSentSmsSegment(options) {
    // Setup attributes for sending next segment
    let next = options.segmentSeq;
    options.body = options.segments[next].body;
    options.encodedBodyLength = options.segments[next].encodedBodyLength;
    options.segmentSeq = next + 1;

    this.sendSMS(options);
  },

  /**
   * Helper for processing result of send SMS.
   *
   * @param length
   *        Length of SMS string in the incoming parcel.
   * @param options
   *        Sms information.
   */
  _processSmsSendResult: function _processSmsSendResult(length, options) {
    if (options.rilRequestError) {
      if (DEBUG) debug("_processSmsSendResult: rilRequestError = " + options.rilRequestError);
      switch (options.rilRequestError) {
        case ERROR_SMS_SEND_FAIL_RETRY:
          if (options.retryCount < SMS_RETRY_MAX) {
            options.retryCount++;
            // TODO: bug 736702 TP-MR, retry interval, retry timeout
            this.sendSMS(options);
            break;
          }
          // Fallback to default error handling if it meets max retry count.
          // Fall through.
        default:
          this.sendChromeMessage({
            rilMessageType: options.rilMessageType,
            rilMessageToken: options.rilMessageToken,
            errorMsg: options.rilRequestError,
          });
          break;
      }
      return;
    }

    options.messageRef = Buf.readInt32();
    options.ackPDU = Buf.readString();
    options.errorCode = Buf.readInt32();

    if ((options.segmentMaxSeq > 1)
        && (options.segmentSeq < options.segmentMaxSeq)) {
      // Not last segment
      this._processSentSmsSegment(options);
    } else {
      // Last segment sent with success.
      if (options.requestStatusReport) {
        if (DEBUG) debug("waiting SMS-STATUS-REPORT for messageRef " + options.messageRef);
        this._pendingSentSmsMap[options.messageRef] = options;
      }

      this.sendChromeMessage({
        rilMessageType: options.rilMessageType,
        rilMessageToken: options.rilMessageToken,
      });
    }
  },

  _processReceivedSmsCbPage: function _processReceivedSmsCbPage(original) {
    if (original.numPages <= 1) {
      if (original.body) {
        original.fullBody = original.body;
        delete original.body;
      } else if (original.data) {
        original.fullData = original.data;
        delete original.data;
      }
      return original;
    }

    // Hash = <serial>:<mcc>:<mnc>:<lac>:<cid>
    let hash = original.serial + ":" + this.iccInfo.mcc + ":"
               + this.iccInfo.mnc + ":";
    switch (original.geographicalScope) {
      case CB_GSM_GEOGRAPHICAL_SCOPE_CELL_WIDE_IMMEDIATE:
      case CB_GSM_GEOGRAPHICAL_SCOPE_CELL_WIDE:
        hash += this.voiceRegistrationState.cell.gsmLocationAreaCode + ":"
             + this.voiceRegistrationState.cell.gsmCellId;
        break;
      case CB_GSM_GEOGRAPHICAL_SCOPE_LOCATION_AREA_WIDE:
        hash += this.voiceRegistrationState.cell.gsmLocationAreaCode + ":";
        break;
      default:
        hash += ":";
        break;
    }

    let index = original.pageIndex;

    let options = this._receivedSmsCbPagesMap[hash];
    if (!options) {
      options = original;
      this._receivedSmsCbPagesMap[hash] = options;

      options.receivedPages = 0;
      options.pages = [];
    } else if (options.pages[index]) {
      // Duplicated page?
      if (DEBUG) {
        debug("Got duplicated page no." + index + " of a multipage SMSCB: "
              + JSON.stringify(original));
      }
      return null;
    }

    if (options.encoding == PDU_DCS_MSG_CODING_8BITS_ALPHABET) {
      options.pages[index] = original.data;
      delete original.data;
    } else {
      options.pages[index] = original.body;
      delete original.body;
    }
    options.receivedPages++;
    if (options.receivedPages < options.numPages) {
      if (DEBUG) {
        debug("Got page no." + index + " of a multipage SMSCB: "
              + JSON.stringify(options));
      }
      return null;
    }

    // Remove from map
    delete this._receivedSmsCbPagesMap[hash];

    // Rebuild full body
    if (options.encoding == PDU_DCS_MSG_CODING_8BITS_ALPHABET) {
      // Uint8Array doesn't have `concat`, so we have to merge all pages by hand.
      let fullDataLen = 0;
      for (let i = 1; i <= options.numPages; i++) {
        fullDataLen += options.pages[i].length;
      }

      options.fullData = new Uint8Array(fullDataLen);
      for (let d= 0, i = 1; i <= options.numPages; i++) {
        let data = options.pages[i];
        for (let j = 0; j < data.length; j++) {
          options.fullData[d++] = data[j];
        }
      }
    } else {
      options.fullBody = options.pages.join("");
    }

    if (DEBUG) {
      debug("Got full multipage SMSCB: " + JSON.stringify(options));
    }

    return options;
  },

  _mergeCellBroadcastConfigs: function _mergeCellBroadcastConfigs(list, from, to) {
    if (!list) {
      return [from, to];
    }

    for (let i = 0, f1, t1; i < list.length;) {
      f1 = list[i++];
      t1 = list[i++];
      if (to == f1) {
        // ...[from]...[to|f1]...(t1)
        list[i - 2] = from;
        return list;
      }

      if (to < f1) {
        // ...[from]...(to)...[f1] or ...[from]...(to)[f1]
        if (i > 2) {
          // Not the first range pair, merge three arrays.
          return list.slice(0, i - 2).concat([from, to]).concat(list.slice(i - 2));
        } else {
          return [from, to].concat(list);
        }
      }

      if (from > t1) {
        // ...[f1]...(t1)[from] or ...[f1]...(t1)...[from]
        continue;
      }

      // Have overlap or merge-able adjacency with [f1]...(t1). Replace it
      // with [min(from, f1)]...(max(to, t1)).

      let changed = false;
      if (from < f1) {
        // [from]...[f1]...(t1) or [from][f1]...(t1)
        // Save minimum from value.
        list[i - 2] = from;
        changed = true;
      }

      if (to <= t1) {
        // [from]...[to](t1) or [from]...(to|t1)
        // Can't have further merge-able adjacency. Return.
        return list;
      }

      // Try merging possible next adjacent range.
      let j = i;
      for (let f2, t2; j < list.length;) {
        f2 = list[j++];
        t2 = list[j++];
        if (to > t2) {
          // [from]...[f2]...[t2]...(to) or [from]...[f2]...[t2](to)
          // Merge next adjacent range again.
          continue;
        }

        if (to < t2) {
          if (to < f2) {
            // [from]...(to)[f2] or [from]...(to)...[f2]
            // Roll back and give up.
            j -= 2;
          } else if (to < t2) {
            // [from]...[to|f2]...(t2), or [from]...[f2]...[to](t2)
            // Merge to [from]...(t2) and give up.
            to = t2;
          }
        }

        break;
      }

      // Save maximum to value.
      list[i - 1] = to;

      if (j != i) {
        // Remove merged adjacent ranges.
        let ret = list.slice(0, i);
        if (j < list.length) {
          ret = ret.concat(list.slice(j));
        }
        return ret;
      }

      return list;
    }

    // Append to the end.
    list.push(from);
    list.push(to);

    return list;
  },

  _isCellBroadcastConfigReady: function() {
    if (!("MMI" in this.cellBroadcastConfigs)) {
      return false;
    }

    // CBMI should be ready in GSM.
    if (!this._isCdma &&
        (!("CBMI" in this.cellBroadcastConfigs) ||
         !("CBMID" in this.cellBroadcastConfigs) ||
         !("CBMIR" in this.cellBroadcastConfigs))) {
      return false;
    }

    return true;
  },

  /**
   * Merge all members of cellBroadcastConfigs into mergedCellBroadcastConfig.
   */
  _mergeAllCellBroadcastConfigs: function _mergeAllCellBroadcastConfigs() {
    if (!this._isCellBroadcastConfigReady()) {
      if (DEBUG) {
        debug("cell broadcast configs not ready, waiting ...");
      }
      return;
    }

    // Prepare cell broadcast config. CBMI* are only used in GSM.
    let usedCellBroadcastConfigs = {MMI: this.cellBroadcastConfigs.MMI};
    if (!this._isCdma) {
      usedCellBroadcastConfigs.CBMI = this.cellBroadcastConfigs.CBMI;
      usedCellBroadcastConfigs.CBMID = this.cellBroadcastConfigs.CBMID;
      usedCellBroadcastConfigs.CBMIR = this.cellBroadcastConfigs.CBMIR;
    }

    if (DEBUG) {
      debug("Cell Broadcast search lists: " + JSON.stringify(usedCellBroadcastConfigs));
    }

    let list = null;
    for each (let ll in usedCellBroadcastConfigs) {
      if (ll == null) {
        continue;
      }

      for (let i = 0; i < ll.length; i += 2) {
        list = this._mergeCellBroadcastConfigs(list, ll[i], ll[i + 1]);
      }
    }

    if (DEBUG) {
      debug("Cell Broadcast search lists(merged): " + JSON.stringify(list));
    }
    this.mergedCellBroadcastConfig = list;
    this.updateCellBroadcastConfig();
  },

  /**
   * Check whether search list from settings is settable by MMI, that is,
   * whether the range is bounded in any entries of CB_NON_MMI_SETTABLE_RANGES.
   */
  _checkCellBroadcastMMISettable: function _checkCellBroadcastMMISettable(from, to) {
    if ((to <= from) || (from >= 65536) || (from < 0)) {
      return false;
    }

    if (!this._isCdma) {
      // GSM not settable ranges.
      for (let i = 0, f, t; i < CB_NON_MMI_SETTABLE_RANGES.length;) {
        f = CB_NON_MMI_SETTABLE_RANGES[i++];
        t = CB_NON_MMI_SETTABLE_RANGES[i++];
        if ((from < t) && (to > f)) {
          // Have overlap.
          return false;
        }
      }
    }

    return true;
  },

  /**
   * Convert Cell Broadcast settings string into search list.
   */
  _convertCellBroadcastSearchList: function _convertCellBroadcastSearchList(searchListStr) {
    let parts = searchListStr && searchListStr.split(",");
    if (!parts) {
      return null;
    }

    let list = null;
    let result, from, to;
    for (let range of parts) {
      // Match "12" or "12-34". The result will be ["12", "12", null] or
      // ["12-34", "12", "34"].
      result = range.match(/^(\d+)(?:-(\d+))?$/);
      if (!result) {
        throw "Invalid format";
      }

      from = parseInt(result[1], 10);
      to = (result[2]) ? parseInt(result[2], 10) + 1 : from + 1;
      if (!this._checkCellBroadcastMMISettable(from, to)) {
        throw "Invalid range";
      }

      if (list == null) {
        list = [];
      }
      list.push(from);
      list.push(to);
    }

    return list;
  },

  /**
   * Handle incoming messages from the main UI thread.
   *
   * @param message
   *        Object containing the message. Messages are supposed
   */
  handleChromeMessage: function handleChromeMessage(message) {
    if (DEBUG) debug("Received chrome message " + JSON.stringify(message));
    let method = this[message.rilMessageType];
    if (typeof method != "function") {
      if (DEBUG) {
        debug("Don't know what to do with message " + JSON.stringify(message));
      }
      return;
    }
    method.call(this, message);
  },

  /**
   * Get a list of current voice calls.
   */
  enumerateCalls: function enumerateCalls(options) {
    if (DEBUG) debug("Sending all current calls");
    let calls = [];
    for each (let call in this.currentCalls) {
      calls.push(call);
    }
    options.calls = calls;
    this.sendChromeMessage(options);
  },

  /**
   * Get a list of current data calls.
   */
  enumerateDataCalls: function enumerateDataCalls() {
    let datacall_list = [];
    for each (let datacall in this.currentDataCalls) {
      datacall_list.push(datacall);
    }
    this.sendChromeMessage({rilMessageType: "datacalllist",
                            datacalls: datacall_list});
  },

  /**
   * Process STK Proactive Command.
   */
  processStkProactiveCommand: function processStkProactiveCommand() {
    let length = Buf.readInt32();
    let berTlv = BerTlvHelper.decode(length / 2);
    Buf.readStringDelimiter(length);

    let ctlvs = berTlv.value;
    let ctlv = StkProactiveCmdHelper.searchForTag(
        COMPREHENSIONTLV_TAG_COMMAND_DETAILS, ctlvs);
    if (!ctlv) {
      RIL.sendStkTerminalResponse({
        resultCode: STK_RESULT_CMD_DATA_NOT_UNDERSTOOD});
      throw new Error("Can't find COMMAND_DETAILS ComprehensionTlv");
    }

    let cmdDetails = ctlv.value;
    if (DEBUG) {
      debug("commandNumber = " + cmdDetails.commandNumber +
           " typeOfCommand = " + cmdDetails.typeOfCommand.toString(16) +
           " commandQualifier = " + cmdDetails.commandQualifier);
    }

    // STK_CMD_MORE_TIME need not to propagate event to chrome.
    if (cmdDetails.typeOfCommand == STK_CMD_MORE_TIME) {
      RIL.sendStkTerminalResponse({
        command: cmdDetails,
        resultCode: STK_RESULT_OK});
      return;
    }

    cmdDetails.rilMessageType = "stkcommand";
    cmdDetails.options = StkCommandParamsFactory.createParam(cmdDetails, ctlvs);
    RIL.sendChromeMessage(cmdDetails);
  },

  /**
   * Send messages to the main thread.
   */
  sendChromeMessage: function sendChromeMessage(message) {
    postMessage(message);
  },

  /**
   * Handle incoming requests from the RIL. We find the method that
   * corresponds to the request type. Incidentally, the request type
   * _is_ the method name, so that's easy.
   */

  handleParcel: function handleParcel(request_type, length, options) {
    let method = this[request_type];
    if (typeof method == "function") {
      if (DEBUG) debug("Handling parcel as " + method.name);
      method.call(this, length, options);
    }
  },

  setInitialOptions: function setInitialOptions(options) {
    DEBUG = DEBUG_WORKER || options.debug;
    CLIENT_ID = options.clientId;
    this.cellBroadcastDisabled = options.cellBroadcastDisabled;
    this.clirMode = options.clirMode;
  }
};

RIL.initRILState();

RIL[REQUEST_GET_SIM_STATUS] = function REQUEST_GET_SIM_STATUS(length, options) {
  if (options.rilRequestError) {
    return;
  }

  let iccStatus = {};
  iccStatus.cardState = Buf.readInt32(); // CARD_STATE_*
  iccStatus.universalPINState = Buf.readInt32(); // CARD_PINSTATE_*
  iccStatus.gsmUmtsSubscriptionAppIndex = Buf.readInt32();
  iccStatus.cdmaSubscriptionAppIndex = Buf.readInt32();
  if (!RILQUIRKS_V5_LEGACY) {
    iccStatus.imsSubscriptionAppIndex = Buf.readInt32();
  }

  let apps_length = Buf.readInt32();
  if (apps_length > CARD_MAX_APPS) {
    apps_length = CARD_MAX_APPS;
  }

  iccStatus.apps = [];
  for (let i = 0 ; i < apps_length ; i++) {
    iccStatus.apps.push({
      app_type:       Buf.readInt32(), // CARD_APPTYPE_*
      app_state:      Buf.readInt32(), // CARD_APPSTATE_*
      perso_substate: Buf.readInt32(), // CARD_PERSOSUBSTATE_*
      aid:            Buf.readString(),
      app_label:      Buf.readString(),
      pin1_replaced:  Buf.readInt32(),
      pin1:           Buf.readInt32(),
      pin2:           Buf.readInt32()
    });
    if (RILQUIRKS_SIM_APP_STATE_EXTRA_FIELDS) {
      Buf.readInt32();
      Buf.readInt32();
      Buf.readInt32();
      Buf.readInt32();
    }
  }

  if (DEBUG) debug("iccStatus: " + JSON.stringify(iccStatus));
  this._processICCStatus(iccStatus);
};
RIL[REQUEST_ENTER_SIM_PIN] = function REQUEST_ENTER_SIM_PIN(length, options) {
  this._processEnterAndChangeICCResponses(length, options);
};
RIL[REQUEST_ENTER_SIM_PUK] = function REQUEST_ENTER_SIM_PUK(length, options) {
  this._processEnterAndChangeICCResponses(length, options);
};
RIL[REQUEST_ENTER_SIM_PIN2] = function REQUEST_ENTER_SIM_PIN2(length, options) {
  this._processEnterAndChangeICCResponses(length, options);
};
RIL[REQUEST_ENTER_SIM_PUK2] = function REQUEST_ENTER_SIM_PUK(length, options) {
  this._processEnterAndChangeICCResponses(length, options);
};
RIL[REQUEST_CHANGE_SIM_PIN] = function REQUEST_CHANGE_SIM_PIN(length, options) {
  this._processEnterAndChangeICCResponses(length, options);
};
RIL[REQUEST_CHANGE_SIM_PIN2] = function REQUEST_CHANGE_SIM_PIN2(length, options) {
  this._processEnterAndChangeICCResponses(length, options);
};
RIL[REQUEST_ENTER_NETWORK_DEPERSONALIZATION_CODE] =
  function REQUEST_ENTER_NETWORK_DEPERSONALIZATION_CODE(length, options) {
  this._processEnterAndChangeICCResponses(length, options);
};
RIL[REQUEST_GET_CURRENT_CALLS] = function REQUEST_GET_CURRENT_CALLS(length, options) {
  if (options.rilRequestError) {
    return;
  }

  let calls_length = 0;
  // The RIL won't even send us the length integer if there are no active calls.
  // So only read this integer if the parcel actually has it.
  if (length) {
    calls_length = Buf.readInt32();
  }
  if (!calls_length) {
    this._processCalls(null);
    return;
  }

  let calls = {};
  for (let i = 0; i < calls_length; i++) {
    let call = {};

    // Extra uint32 field to get correct callIndex and rest of call data for
    // call waiting feature.
    if (RILQUIRKS_EXTRA_UINT32_2ND_CALL && i > 0) {
      Buf.readInt32();
    }

    call.state          = Buf.readInt32(); // CALL_STATE_*
    call.callIndex      = Buf.readInt32(); // GSM index (1-based)
    call.toa            = Buf.readInt32();
    call.isMpty         = Boolean(Buf.readInt32());
    call.isMT           = Boolean(Buf.readInt32());
    call.als            = Buf.readInt32();
    call.isVoice        = Boolean(Buf.readInt32());
    call.isVoicePrivacy = Boolean(Buf.readInt32());
    if (RILQUIRKS_CALLSTATE_EXTRA_UINT32) {
      Buf.readInt32();
    }
    call.number             = Buf.readString(); //TODO munge with TOA
    call.numberPresentation = Buf.readInt32(); // CALL_PRESENTATION_*
    call.name               = Buf.readString();
    call.namePresentation   = Buf.readInt32();

    call.uusInfo = null;
    let uusInfoPresent = Buf.readInt32();
    if (uusInfoPresent == 1) {
      call.uusInfo = {
        type:     Buf.readInt32(),
        dcs:      Buf.readInt32(),
        userData: null //XXX TODO byte array?!?
      };
    }

    calls[call.callIndex] = call;
  }
  this._processCalls(calls);
};
RIL[REQUEST_DIAL] = function REQUEST_DIAL(length, options) {
  if (options.rilRequestError) {
    // The connection is not established yet.
    options.callIndex = -1;
    this._sendCallError(options.callIndex,
                        RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError]);
  }
};
RIL[REQUEST_GET_IMSI] = function REQUEST_GET_IMSI(length, options) {
  if (options.rilRequestError) {
    return;
  }

  this.iccInfoPrivate.imsi = Buf.readString();
  if (DEBUG) {
    debug("IMSI: " + this.iccInfoPrivate.imsi);
  }

  options.rilMessageType = "iccimsi";
  options.imsi = this.iccInfoPrivate.imsi;
  this.sendChromeMessage(options);
};
RIL[REQUEST_HANGUP] = function REQUEST_HANGUP(length, options) {
  if (options.rilRequestError) {
    return;
  }

  this.getCurrentCalls();
};
RIL[REQUEST_HANGUP_WAITING_OR_BACKGROUND] = function REQUEST_HANGUP_WAITING_OR_BACKGROUND(length, options) {
  if (options.rilRequestError) {
    return;
  }

  this.getCurrentCalls();
};
RIL[REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND] = function REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND(length, options) {
  if (options.rilRequestError) {
    return;
  }

  this.getCurrentCalls();
};
RIL[REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE] = function REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE(length, options) {
  if (options.rilRequestError) {
    return;
  }

  this.getCurrentCalls();
};
RIL[REQUEST_SWITCH_HOLDING_AND_ACTIVE] = function REQUEST_SWITCH_HOLDING_AND_ACTIVE(length, options) {
  if (options.rilRequestError) {
    return;
  }

  // XXX Normally we should get a UNSOLICITED_RESPONSE_CALL_STATE_CHANGED parcel
  // notifying us of call state changes, but sometimes we don't (have no idea why).
  // this.getCurrentCalls() helps update the call state actively.
  this.getCurrentCalls();
};
RIL[REQUEST_CONFERENCE] = function REQUEST_CONFERENCE(length, options) {
  if (options.rilRequestError) {
    this._hasConferenceRequest = false;
    options = {rilMessageType: "conferenceError",
               errorName: "addError",
               errorMsg: RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError]};
    this.sendChromeMessage(options);
    return;
  }
};
RIL[REQUEST_UDUB] = null;
RIL[REQUEST_LAST_CALL_FAIL_CAUSE] = function REQUEST_LAST_CALL_FAIL_CAUSE(length, options) {
  let num = 0;
  if (length) {
    num = Buf.readInt32();
  }
  if (!num) {
    // No response of REQUEST_LAST_CALL_FAIL_CAUSE. Change the call state into
    // 'disconnected' directly.
    this._handleDisconnectedCall(options);
    return;
  }

  let failCause = Buf.readInt32();
  options.failCause = RIL_CALL_FAILCAUSE_TO_GECKO_CALL_ERROR[failCause];
  this._handleDisconnectedCall(options);
};
RIL[REQUEST_SIGNAL_STRENGTH] = function REQUEST_SIGNAL_STRENGTH(length, options) {
  this._receivedNetworkInfo(NETWORK_INFO_SIGNAL);

  if (options.rilRequestError) {
    return;
  }

  let signal = {
    gsmSignalStrength: Buf.readInt32(),
    gsmBitErrorRate:   Buf.readInt32(),
    cdmaDBM:           Buf.readInt32(),
    cdmaECIO:          Buf.readInt32(),
    evdoDBM:           Buf.readInt32(),
    evdoECIO:          Buf.readInt32(),
    evdoSNR:           Buf.readInt32()
  };

  if (!RILQUIRKS_V5_LEGACY) {
    signal.lteSignalStrength = Buf.readInt32();
    signal.lteRSRP =           Buf.readInt32();
    signal.lteRSRQ =           Buf.readInt32();
    signal.lteRSSNR =          Buf.readInt32();
    signal.lteCQI =            Buf.readInt32();
  }

  if (DEBUG) debug("signal strength: " + JSON.stringify(signal));

  this._processSignalStrength(signal);
};
RIL[REQUEST_VOICE_REGISTRATION_STATE] = function REQUEST_VOICE_REGISTRATION_STATE(length, options) {
  this._receivedNetworkInfo(NETWORK_INFO_VOICE_REGISTRATION_STATE);

  if (options.rilRequestError) {
    return;
  }

  let state = Buf.readStringList();
  if (DEBUG) debug("voice registration state: " + state);

  this._processVoiceRegistrationState(state);

  if (this.cachedDialRequest &&
       (this.voiceRegistrationState.emergencyCallsOnly ||
        this.voiceRegistrationState.connected) &&
      this.voiceRegistrationState.radioTech != NETWORK_CREG_TECH_UNKNOWN) {
    // Radio is ready for making the cached emergency call.
    this.cachedDialRequest.callback();
    this.cachedDialRequest = null;
  }
};
RIL[REQUEST_DATA_REGISTRATION_STATE] = function REQUEST_DATA_REGISTRATION_STATE(length, options) {
  this._receivedNetworkInfo(NETWORK_INFO_DATA_REGISTRATION_STATE);

  if (options.rilRequestError) {
    return;
  }

  let state = Buf.readStringList();
  this._processDataRegistrationState(state);
};
RIL[REQUEST_OPERATOR] = function REQUEST_OPERATOR(length, options) {
  this._receivedNetworkInfo(NETWORK_INFO_OPERATOR);

  if (options.rilRequestError) {
    return;
  }

  let operatorData = Buf.readStringList();
  if (DEBUG) debug("Operator: " + operatorData);
  this._processOperator(operatorData);
};
RIL[REQUEST_RADIO_POWER] = function REQUEST_RADIO_POWER(length, options) {
  if (options.rilMessageType == null) {
    // The request was made by ril_worker itself.
    if (options.rilRequestError) {
      if (this.cachedDialRequest && options.enabled) {
        // Turning on radio fails. Notify the error of making an emergency call.
        this.cachedDialRequest.onerror(GECKO_ERROR_RADIO_NOT_AVAILABLE);
        this.cachedDialRequest = null;
      }
      return;
    }

    if (this._isInitialRadioState) {
      this._isInitialRadioState = false;
    }

    return;
  }

  this.sendChromeMessage(options);
};
RIL[REQUEST_DTMF] = null;
RIL[REQUEST_SEND_SMS] = function REQUEST_SEND_SMS(length, options) {
  this._processSmsSendResult(length, options);
};
RIL[REQUEST_SEND_SMS_EXPECT_MORE] = null;

RIL.readSetupDataCall_v5 = function readSetupDataCall_v5(options) {
  if (!options) {
    options = {};
  }
  let [cid, ifname, ipaddr, dns, gw] = Buf.readStringList();
  options.cid = cid;
  options.ifname = ifname;
  options.ipaddr = ipaddr;
  options.dns = dns;
  options.gw = gw;
  options.active = DATACALL_ACTIVE_UNKNOWN;
  options.state = GECKO_NETWORK_STATE_CONNECTING;
  return options;
};

RIL[REQUEST_SETUP_DATA_CALL] = function REQUEST_SETUP_DATA_CALL(length, options) {
  if (options.rilRequestError) {
    // On Data Call generic errors, we shall notify caller
    this._sendDataCallError(options, options.rilRequestError);
    return;
  }

  if (RILQUIRKS_V5_LEGACY) {
    // Populate the `options` object with the data call information. That way
    // we retain the APN and other info about how the data call was set up.
    this.readSetupDataCall_v5(options);
    this.currentDataCalls[options.cid] = options;
    options.rilMessageType = "datacallstatechange";
    this.sendChromeMessage(options);
    // Let's get the list of data calls to ensure we know whether it's active
    // or not.
    this.getDataCallList();
    return;
  }
  // Pass `options` along. That way we retain the APN and other info about
  // how the data call was set up.
  this[REQUEST_DATA_CALL_LIST](length, options);
};
RIL[REQUEST_SIM_IO] = function REQUEST_SIM_IO(length, options) {
  if (!length) {
    ICCIOHelper.processICCIOError(options);
    return;
  }

  // Don't need to read rilRequestError since we can know error status from
  // sw1 and sw2.
  options.sw1 = Buf.readInt32();
  options.sw2 = Buf.readInt32();
  if (options.sw1 != ICC_STATUS_NORMAL_ENDING) {
    ICCIOHelper.processICCIOError(options);
    return;
  }
  ICCIOHelper.processICCIO(options);
};
RIL[REQUEST_SEND_USSD] = function REQUEST_SEND_USSD(length, options) {
  if (DEBUG) {
    debug("REQUEST_SEND_USSD " + JSON.stringify(options));
  }
  options.success = (this._ussdSession = options.rilRequestError === 0);
  options.errorMsg = RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError];
  this.sendChromeMessage(options);
};
RIL[REQUEST_CANCEL_USSD] = function REQUEST_CANCEL_USSD(length, options) {
  if (DEBUG) {
    debug("REQUEST_CANCEL_USSD" + JSON.stringify(options));
  }
  options.success = (options.rilRequestError === 0);
  this._ussdSession = !options.success;
  options.errorMsg = RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError];
  this.sendChromeMessage(options);
};
RIL[REQUEST_GET_CLIR] = function REQUEST_GET_CLIR(length, options) {
  options.success = (options.rilRequestError === 0);
  if (!options.success) {
    options.errorMsg = RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError];
    this.sendChromeMessage(options);
    return;
  }

  let bufLength = Buf.readInt32();
  if (!bufLength || bufLength < 2) {
    options.success = false;
    options.errorMsg = GECKO_ERROR_GENERIC_FAILURE;
    this.sendChromeMessage(options);
    return;
  }

  options.n = Buf.readInt32(); // Will be TS 27.007 +CLIR parameter 'n'.
  options.m = Buf.readInt32(); // Will be TS 27.007 +CLIR parameter 'm'.

  if (options.rilMessageType === "sendMMI") {
    // TS 27.007 +CLIR parameter 'm'.
    switch (options.m) {
      // CLIR not provisioned.
      case 0:
        options.statusMessage = MMI_SM_KS_SERVICE_NOT_PROVISIONED;
        break;
      // CLIR provisioned in permanent mode.
      case 1:
        options.statusMessage = MMI_SM_KS_CLIR_PERMANENT;
        break;
      // Unknown (e.g. no network, etc.).
      case 2:
        options.success = false;
        options.errorMsg = MMI_ERROR_KS_ERROR;
        break;
      // CLIR temporary mode presentation restricted.
      case 3:
        // TS 27.007 +CLIR parameter 'n'.
        switch (options.n) {
          // Default.
          case 0:
          // CLIR invocation.
          case 1:
            options.statusMessage = MMI_SM_KS_CLIR_DEFAULT_ON_NEXT_CALL_ON;
            break;
          // CLIR suppression.
          case 2:
            options.statusMessage = MMI_SM_KS_CLIR_DEFAULT_ON_NEXT_CALL_OFF;
            break;
          default:
            options.success = false;
            options.errorMsg = GECKO_ERROR_GENERIC_FAILURE;
            break;
        }
        break;
      // CLIR temporary mode presentation allowed.
      case 4:
        // TS 27.007 +CLIR parameter 'n'.
        switch (options.n) {
          // Default.
          case 0:
          // CLIR suppression.
          case 2:
            options.statusMessage = MMI_SM_KS_CLIR_DEFAULT_OFF_NEXT_CALL_OFF;
            break;
          // CLIR invocation.
          case 1:
            options.statusMessage = MMI_SM_KS_CLIR_DEFAULT_OFF_NEXT_CALL_ON;
            break;
          default:
            options.success = false;
            options.errorMsg = GECKO_ERROR_GENERIC_FAILURE;
            break;
        }
        break;
      default:
        options.success = false;
        options.errorMsg = GECKO_ERROR_GENERIC_FAILURE;
        break;
    }
  }

  this.sendChromeMessage(options);
};
RIL[REQUEST_SET_CLIR] = function REQUEST_SET_CLIR(length, options) {
  if (options.rilMessageType == null) {
    // The request was made by ril_worker itself automatically. Don't report.
    return;
  }
  options.success = (options.rilRequestError === 0);
  if (!options.success) {
    options.errorMsg = RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError];
  } else if (options.rilMessageType === "sendMMI") {
    switch (options.procedure) {
      case MMI_PROCEDURE_ACTIVATION:
        options.statusMessage = MMI_SM_KS_SERVICE_ENABLED;
        break;
      case MMI_PROCEDURE_DEACTIVATION:
        options.statusMessage = MMI_SM_KS_SERVICE_DISABLED;
        break;
    }
  }
  this.sendChromeMessage(options);
};

RIL[REQUEST_QUERY_CALL_FORWARD_STATUS] =
  function REQUEST_QUERY_CALL_FORWARD_STATUS(length, options) {
    options.success = (options.rilRequestError === 0);
    if (!options.success) {
      options.errorMsg = RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError];
      this.sendChromeMessage(options);
      return;
    }

    let rulesLength = 0;
    if (length) {
      rulesLength = Buf.readInt32();
    }
    if (!rulesLength) {
      options.success = false;
      options.errorMsg = GECKO_ERROR_GENERIC_FAILURE;
      this.sendChromeMessage(options);
      return;
    }
    let rules = new Array(rulesLength);
    for (let i = 0; i < rulesLength; i++) {
      let rule = {};
      rule.active       = Buf.readInt32() == 1; // CALL_FORWARD_STATUS_*
      rule.reason       = Buf.readInt32(); // CALL_FORWARD_REASON_*
      rule.serviceClass = Buf.readInt32();
      rule.toa          = Buf.readInt32();
      rule.number       = Buf.readString();
      rule.timeSeconds  = Buf.readInt32();
      rules[i] = rule;
    }
    options.rules = rules;
    if (options.rilMessageType === "sendMMI") {
      options.statusMessage = MMI_SM_KS_SERVICE_INTERROGATED;
      // MMI query call forwarding options request returns a set of rules that
      // will be exposed in the form of an array of nsIDOMMozMobileCFInfo
      // instances.
      options.additionalInformation = rules;
    }
    this.sendChromeMessage(options);
};
RIL[REQUEST_SET_CALL_FORWARD] =
  function REQUEST_SET_CALL_FORWARD(length, options) {
    options.success = (options.rilRequestError === 0);
    if (!options.success) {
      options.errorMsg = RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError];
    } else if (options.rilMessageType === "sendMMI") {
      switch (options.action) {
        case CALL_FORWARD_ACTION_ENABLE:
          options.statusMessage = MMI_SM_KS_SERVICE_ENABLED;
          break;
        case CALL_FORWARD_ACTION_DISABLE:
          options.statusMessage = MMI_SM_KS_SERVICE_DISABLED;
          break;
        case CALL_FORWARD_ACTION_REGISTRATION:
          options.statusMessage = MMI_SM_KS_SERVICE_REGISTERED;
          break;
        case CALL_FORWARD_ACTION_ERASURE:
          options.statusMessage = MMI_SM_KS_SERVICE_ERASED;
          break;
      }
    }
    this.sendChromeMessage(options);
};
RIL[REQUEST_QUERY_CALL_WAITING] =
  function REQUEST_QUERY_CALL_WAITING(length, options) {
  options.success = (options.rilRequestError === 0);
  if (!options.success) {
    options.errorMsg = RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError];
    this.sendChromeMessage(options);
    return;
  }

  if (options.callback) {
    options.callback.call(this, options);
    return;
  }

  options.length = Buf.readInt32();
  options.enabled = ((Buf.readInt32() == 1) &&
                     ((Buf.readInt32() & ICC_SERVICE_CLASS_VOICE) == 0x01));
  this.sendChromeMessage(options);
};

RIL[REQUEST_SET_CALL_WAITING] = function REQUEST_SET_CALL_WAITING(length, options) {
  options.success = (options.rilRequestError === 0);
  if (!options.success) {
    options.errorMsg = RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError];
    this.sendChromeMessage(options);
    return;
  }

  if (options.callback) {
    options.callback.call(this, options);
    return;
  }

  this.sendChromeMessage(options);
};
RIL[REQUEST_SMS_ACKNOWLEDGE] = null;
RIL[REQUEST_GET_IMEI] = function REQUEST_GET_IMEI(length, options) {
  this.IMEI = Buf.readString();
  let rilMessageType = options.rilMessageType;
  // So far we only send the IMEI back to chrome if it was requested via MMI.
  if (rilMessageType !== "sendMMI") {
    return;
  }

  options.success = (options.rilRequestError === 0);
  options.errorMsg = RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError];
  if ((!options.success || this.IMEI == null) && !options.errorMsg) {
    options.errorMsg = GECKO_ERROR_GENERIC_FAILURE;
  }
  options.statusMessage = this.IMEI;
  this.sendChromeMessage(options);
};
RIL[REQUEST_GET_IMEISV] = function REQUEST_GET_IMEISV(length, options) {
  if (options.rilRequestError) {
    return;
  }

  this.IMEISV = Buf.readString();
};
RIL[REQUEST_ANSWER] = null;
RIL[REQUEST_DEACTIVATE_DATA_CALL] = function REQUEST_DEACTIVATE_DATA_CALL(length, options) {
  if (options.rilRequestError) {
    return;
  }

  let datacall = this.currentDataCalls[options.cid];
  delete this.currentDataCalls[options.cid];
  datacall.state = GECKO_NETWORK_STATE_UNKNOWN;
  datacall.rilMessageType = "datacallstatechange";
  this.sendChromeMessage(datacall);
};
RIL[REQUEST_QUERY_FACILITY_LOCK] = function REQUEST_QUERY_FACILITY_LOCK(length, options) {
  options.success = (options.rilRequestError === 0);
  if (!options.success) {
    options.errorMsg = RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError];
  }

  let services;
  if (length) {
    // Buf.readInt32List()[0] for Call Barring is a bit vector of services.
    services = Buf.readInt32List()[0];
  } else {
    options.success = false;
    options.errorMsg = GECKO_ERROR_GENERIC_FAILURE;
    this.sendChromeMessage(options);
    return;
  }

  options.enabled = services === 0 ? false : true;

  if (options.success && (options.rilMessageType === "sendMMI")) {
    if (!options.enabled) {
      options.statusMessage = MMI_SM_KS_SERVICE_DISABLED;
    } else {
      options.statusMessage = MMI_SM_KS_SERVICE_ENABLED_FOR;
      let serviceClass = [];
      for (let serviceClassMask = 1;
           serviceClassMask <= ICC_SERVICE_CLASS_MAX;
           serviceClassMask <<= 1) {
        if ((serviceClassMask & services) !== 0) {
          serviceClass.push(MMI_KS_SERVICE_CLASS_MAPPING[serviceClassMask]);
        }
      }

      options.additionalInformation = serviceClass;
    }
  }
  this.sendChromeMessage(options);
};
RIL[REQUEST_SET_FACILITY_LOCK] = function REQUEST_SET_FACILITY_LOCK(length, options) {
  options.success = (options.rilRequestError === 0);
  if (!options.success) {
    options.errorMsg = RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError];
  }

  options.retryCount = length ? Buf.readInt32List()[0] : -1;

  if (options.success && (options.rilMessageType === "sendMMI")) {
    switch (options.procedure) {
      case MMI_PROCEDURE_ACTIVATION:
        options.statusMessage = MMI_SM_KS_SERVICE_ENABLED;
        break;
      case MMI_PROCEDURE_DEACTIVATION:
        options.statusMessage = MMI_SM_KS_SERVICE_DISABLED;
        break;
    }
  }
  this.sendChromeMessage(options);
};
RIL[REQUEST_CHANGE_BARRING_PASSWORD] =
  function REQUEST_CHANGE_BARRING_PASSWORD(length, options) {
  if (options.rilRequestError) {
    options.errorMsg = RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError];
  }
  this.sendChromeMessage(options);
};
RIL[REQUEST_SIM_OPEN_CHANNEL] = function REQUEST_SIM_OPEN_CHANNEL(length, options) {
  if (options.rilRequestError) {
    options.errorMsg = RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError];
    this.sendChromeMessage(options);
    return;
  }

  options.channel = Buf.readInt32();
  if (DEBUG) debug("Setting channel number in options: " + options.channel);
  this.sendChromeMessage(options);
};
RIL[REQUEST_SIM_CLOSE_CHANNEL] = function REQUEST_SIM_CLOSE_CHANNEL(length, options) {
  if (options.rilRequestError) {
    options.error = RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError];
    this.sendChromeMessage(options);
    return;
  }

  // No return value
  this.sendChromeMessage(options);
};
RIL[REQUEST_SIM_ACCESS_CHANNEL] = function REQUEST_SIM_ACCESS_CHANNEL(length, options) {
  if (options.rilRequestError) {
    options.error = RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError];
    this.sendChromeMessage(options);
  }

  options.sw1 = Buf.readInt32();
  options.sw2 = Buf.readInt32();
  options.simResponse = Buf.readString();
  if (DEBUG) {
    debug("Setting return values for RIL[REQUEST_SIM_ACCESS_CHANNEL]: ["
          + options.sw1 + "," + options.sw2 + ", " + options.simResponse + "]");
  }
  this.sendChromeMessage(options);
};
RIL[REQUEST_QUERY_NETWORK_SELECTION_MODE] = function REQUEST_QUERY_NETWORK_SELECTION_MODE(length, options) {
  this._receivedNetworkInfo(NETWORK_INFO_NETWORK_SELECTION_MODE);

  if (options.rilRequestError) {
    return;
  }

  let mode = Buf.readInt32List();
  let selectionMode;

  switch (mode[0]) {
    case NETWORK_SELECTION_MODE_AUTOMATIC:
      selectionMode = GECKO_NETWORK_SELECTION_AUTOMATIC;
      break;
    case NETWORK_SELECTION_MODE_MANUAL:
      selectionMode = GECKO_NETWORK_SELECTION_MANUAL;
      break;
    default:
      selectionMode = GECKO_NETWORK_SELECTION_UNKNOWN;
      break;
  }

  if (this.networkSelectionMode != selectionMode) {
    this.networkSelectionMode = options.mode = selectionMode;
    options.rilMessageType = "networkselectionmodechange";
    this._sendNetworkInfoMessage(NETWORK_INFO_NETWORK_SELECTION_MODE, options);
  }
};
RIL[REQUEST_SET_NETWORK_SELECTION_AUTOMATIC] = function REQUEST_SET_NETWORK_SELECTION_AUTOMATIC(length, options) {
  if (options.rilRequestError) {
    options.errorMsg = RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError];
  }

  this.sendChromeMessage(options);
};
RIL[REQUEST_SET_NETWORK_SELECTION_MANUAL] = function REQUEST_SET_NETWORK_SELECTION_MANUAL(length, options) {
  if (options.rilRequestError) {
    options.errorMsg = RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError];
  }

  this.sendChromeMessage(options);
};
RIL[REQUEST_QUERY_AVAILABLE_NETWORKS] = function REQUEST_QUERY_AVAILABLE_NETWORKS(length, options) {
  if (options.rilRequestError) {
    options.errorMsg = RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError];
  } else {
    options.networks = this._processNetworks();
  }
  this.sendChromeMessage(options);
};
RIL[REQUEST_DTMF_START] = null;
RIL[REQUEST_DTMF_STOP] = null;
RIL[REQUEST_BASEBAND_VERSION] = function REQUEST_BASEBAND_VERSION(length, options) {
  if (options.rilRequestError) {
    return;
  }

  this.basebandVersion = Buf.readString();
  if (DEBUG) debug("Baseband version: " + this.basebandVersion);
};
RIL[REQUEST_SEPARATE_CONNECTION] = function REQUEST_SEPARATE_CONNECTION(length, options) {
  if (options.rilRequestError) {
    options = {rilMessageType: "conferenceError",
               errorName: "removeError",
               errorMsg: RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError]};
    this.sendChromeMessage(options);
    return;
  }
};
RIL[REQUEST_SET_MUTE] = null;
RIL[REQUEST_GET_MUTE] = null;
RIL[REQUEST_QUERY_CLIP] = function REQUEST_QUERY_CLIP(length, options) {
  options.success = (options.rilRequestError === 0);
  if (!options.success) {
    options.errorMsg = RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError];
    this.sendChromeMessage(options);
    return;
  }

  let bufLength = Buf.readInt32();
  if (!bufLength) {
    options.success = false;
    options.errorMsg = GECKO_ERROR_GENERIC_FAILURE;
    this.sendChromeMessage(options);
    return;
  }

  // options.provisioned informs about the called party receives the calling
  // party's address information:
  // 0 for CLIP not provisioned
  // 1 for CLIP provisioned
  // 2 for unknown
  options.provisioned = Buf.readInt32();
  if (options.rilMessageType === "sendMMI") {
    switch (options.provisioned) {
      case 0:
        options.statusMessage = MMI_SM_KS_SERVICE_DISABLED;
        break;
      case 1:
        options.statusMessage = MMI_SM_KS_SERVICE_ENABLED;
        break;
      default:
        options.success = false;
        options.errorMsg = MMI_ERROR_KS_ERROR;
        break;
    }
  }
  this.sendChromeMessage(options);
};
RIL[REQUEST_LAST_DATA_CALL_FAIL_CAUSE] = null;

RIL.readDataCall_v5 = function readDataCall_v5(options) {
  if (!options) {
    options = {};
  }
  options.cid = Buf.readInt32().toString();
  options.active = Buf.readInt32(); // DATACALL_ACTIVE_*
  options.type = Buf.readString();
  options.apn = Buf.readString();
  options.address = Buf.readString();
  return options;
};

RIL.readDataCall_v6 = function readDataCall_v6(options) {
  if (!options) {
    options = {};
  }
  options.status = Buf.readInt32();  // DATACALL_FAIL_*
  options.suggestedRetryTime = Buf.readInt32();
  options.cid = Buf.readInt32().toString();
  options.active = Buf.readInt32();  // DATACALL_ACTIVE_*
  options.type = Buf.readString();
  options.ifname = Buf.readString();
  options.ipaddr = Buf.readString();
  options.dns = Buf.readString();
  options.gw = Buf.readString();
  if (options.dns) {
    options.dns = options.dns.split(" ");
  }
  //TODO for now we only support one address and gateway
  if (options.ipaddr) {
    options.ipaddr = options.ipaddr.split(" ")[0];
  }
  if (options.gw) {
    options.gw = options.gw.split(" ")[0];
  }
  options.ip = null;
  options.netmask = null;
  options.broadcast = null;
  if (options.ipaddr) {
    options.ip = options.ipaddr.split("/")[0];
    let ip_value = netHelpers.stringToIP(options.ip);
    let prefix_len = options.ipaddr.split("/")[1];
    let mask_value = netHelpers.makeMask(prefix_len);
    options.netmask = netHelpers.ipToString(mask_value);
    options.broadcast = netHelpers.ipToString((ip_value & mask_value) + ~mask_value);
  }
  return options;
};

RIL[REQUEST_DATA_CALL_LIST] = function REQUEST_DATA_CALL_LIST(length, options) {
  if (options.rilRequestError) {
    return;
  }

  if (!length) {
    this._processDataCallList(null);
    return;
  }

  let version = 0;
  if (!RILQUIRKS_V5_LEGACY) {
    version = Buf.readInt32();
  }
  let num = Buf.readInt32();
  let datacalls = {};
  for (let i = 0; i < num; i++) {
    let datacall;
    if (version < 6) {
      datacall = this.readDataCall_v5();
    } else {
      datacall = this.readDataCall_v6();
    }
    datacalls[datacall.cid] = datacall;
  }

  let newDataCallOptions = null;
  if (options.rilRequestType == REQUEST_SETUP_DATA_CALL) {
    newDataCallOptions = options;
  }
  this._processDataCallList(datacalls, newDataCallOptions);
};
RIL[REQUEST_RESET_RADIO] = null;
RIL[REQUEST_OEM_HOOK_RAW] = null;
RIL[REQUEST_OEM_HOOK_STRINGS] = null;
RIL[REQUEST_SCREEN_STATE] = null;
RIL[REQUEST_SET_SUPP_SVC_NOTIFICATION] = null;
RIL[REQUEST_WRITE_SMS_TO_SIM] = function REQUEST_WRITE_SMS_TO_SIM(length, options) {
  if (options.rilRequestError) {
    // `The MS shall return a "protocol error, unspecified" error message if
    // the short message cannot be stored in the (U)SIM, and there is other
    // message storage available at the MS` ~ 3GPP TS 23.038 section 4. Here
    // we assume we always have indexed db as another storage.
    this.acknowledgeGsmSms(false, PDU_FCS_PROTOCOL_ERROR);
  } else {
    this.acknowledgeGsmSms(true, PDU_FCS_OK);
  }
};
RIL[REQUEST_DELETE_SMS_ON_SIM] = null;
RIL[REQUEST_SET_BAND_MODE] = null;
RIL[REQUEST_QUERY_AVAILABLE_BAND_MODE] = null;
RIL[REQUEST_STK_GET_PROFILE] = null;
RIL[REQUEST_STK_SET_PROFILE] = null;
RIL[REQUEST_STK_SEND_ENVELOPE_COMMAND] = null;
RIL[REQUEST_STK_SEND_TERMINAL_RESPONSE] = null;
RIL[REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM] = null;
RIL[REQUEST_EXPLICIT_CALL_TRANSFER] = null;
RIL[REQUEST_SET_PREFERRED_NETWORK_TYPE] = function REQUEST_SET_PREFERRED_NETWORK_TYPE(length, options) {
  if (options.networkType == null) {
    // The request was made by ril_worker itself automatically. Don't report.
    return;
  }

  options.success = (options.rilRequestError == ERROR_SUCCESS);
  this.sendChromeMessage(options);
};
RIL[REQUEST_GET_PREFERRED_NETWORK_TYPE] = function REQUEST_GET_PREFERRED_NETWORK_TYPE(length, options) {
  let networkType;
  if (!options.rilRequestError) {
    networkType = RIL_PREFERRED_NETWORK_TYPE_TO_GECKO.indexOf(GECKO_PREFERRED_NETWORK_TYPE_DEFAULT);
    let responseLen = Buf.readInt32(); // Number of INT32 responsed.
    if (responseLen) {
      this.preferredNetworkType = networkType = Buf.readInt32();
    }
  }

  this.sendChromeMessage({
    rilMessageType: "getPreferredNetworkType",
    networkType: networkType,
    success: options.rilRequestError == ERROR_SUCCESS
  });
};
RIL[REQUEST_GET_NEIGHBORING_CELL_IDS] = null;
RIL[REQUEST_SET_LOCATION_UPDATES] = null;
RIL[REQUEST_CDMA_SET_SUBSCRIPTION_SOURCE] = null;
RIL[REQUEST_CDMA_SET_ROAMING_PREFERENCE] = function REQUEST_CDMA_SET_ROAMING_PREFERENCE(length, options) {
  if (options.rilRequestError) {
    options.errorMsg = RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError];
  }
  this.sendChromeMessage(options);
};
RIL[REQUEST_CDMA_QUERY_ROAMING_PREFERENCE] = function REQUEST_CDMA_QUERY_ROAMING_PREFERENCE(length, options) {
  if (options.rilRequestError) {
    options.errorMsg = RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError];
  } else {
    let mode = Buf.readInt32List();
    options.mode = CDMA_ROAMING_PREFERENCE_TO_GECKO[mode[0]];
  }
  this.sendChromeMessage(options);
};
RIL[REQUEST_SET_TTY_MODE] = null;
RIL[REQUEST_QUERY_TTY_MODE] = null;
RIL[REQUEST_CDMA_SET_PREFERRED_VOICE_PRIVACY_MODE] = function REQUEST_CDMA_SET_PREFERRED_VOICE_PRIVACY_MODE(length, options) {
  if (options.rilRequestError) {
    options.errorMsg = RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError];
    this.sendChromeMessage(options);
    return;
  }

  this.sendChromeMessage(options);
};
RIL[REQUEST_CDMA_QUERY_PREFERRED_VOICE_PRIVACY_MODE] = function REQUEST_CDMA_QUERY_PREFERRED_VOICE_PRIVACY_MODE(length, options) {
  if (options.rilRequestError) {
    options.errorMsg = RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError];
    this.sendChromeMessage(options);
    return;
  }

  let enabled = Buf.readInt32List();
  options.enabled = enabled[0] ? true : false;
  this.sendChromeMessage(options);
};
RIL[REQUEST_CDMA_FLASH] = null;
RIL[REQUEST_CDMA_BURST_DTMF] = null;
RIL[REQUEST_CDMA_VALIDATE_AND_WRITE_AKEY] = null;
RIL[REQUEST_CDMA_SEND_SMS] = function REQUEST_CDMA_SEND_SMS(length, options) {
  this._processSmsSendResult(length, options);
};
RIL[REQUEST_CDMA_SMS_ACKNOWLEDGE] = null;
RIL[REQUEST_GSM_GET_BROADCAST_SMS_CONFIG] = null;
RIL[REQUEST_GSM_SET_BROADCAST_SMS_CONFIG] = function REQUEST_GSM_SET_BROADCAST_SMS_CONFIG(length, options) {
  if (options.rilRequestError == ERROR_SUCCESS) {
    this.setSmsBroadcastActivation(true);
  }
};
RIL[REQUEST_GSM_SMS_BROADCAST_ACTIVATION] = null;
RIL[REQUEST_CDMA_GET_BROADCAST_SMS_CONFIG] = null;
RIL[REQUEST_CDMA_SET_BROADCAST_SMS_CONFIG] = null;
RIL[REQUEST_CDMA_SMS_BROADCAST_ACTIVATION] = null;
RIL[REQUEST_CDMA_SUBSCRIPTION] = function REQUEST_CDMA_SUBSCRIPTION(length, options) {
  if (options.rilRequestError) {
    return;
  }

  let result = Buf.readStringList();

  this.iccInfo.mdn = result[0];
  // The result[1] is Home SID. (Already be handled in readCDMAHome())
  // The result[2] is Home NID. (Already be handled in readCDMAHome())
  // The result[3] is MIN.
  // The result[4] is PRL version.

  ICCUtilsHelper.handleICCInfoChange();
};
RIL[REQUEST_CDMA_WRITE_SMS_TO_RUIM] = null;
RIL[REQUEST_CDMA_DELETE_SMS_ON_RUIM] = null;
RIL[REQUEST_DEVICE_IDENTITY] = function REQUEST_DEVICE_IDENTITY(length, options) {
  if (options.rilRequestError) {
    return;
  }

  let result = Buf.readStringList();

  // The result[0] is for IMEI. (Already be handled in REQUEST_GET_IMEI)
  // The result[1] is for IMEISV. (Already be handled in REQUEST_GET_IMEISV)
  // They are both ignored.
  this.ESN = result[2];
  this.MEID = result[3];
};
RIL[REQUEST_EXIT_EMERGENCY_CALLBACK_MODE] = function REQUEST_EXIT_EMERGENCY_CALLBACK_MODE(length, options) {
  if (options.internal) {
    return;
  }

  options.success = (options.rilRequestError === 0);
  if (!options.success) {
    options.errorMsg = RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError];
  }
  this.sendChromeMessage(options);
};
RIL[REQUEST_GET_SMSC_ADDRESS] = function REQUEST_GET_SMSC_ADDRESS(length, options) {
  this.SMSC = options.rilRequestError ? null : Buf.readString();

  if (!options || options.rilMessageType !== "getSmscAddress") {
    return;
  }

  options.smscAddress = this.SMSC;
  options.errorMsg = RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError];
  this.sendChromeMessage(options);
};
RIL[REQUEST_SET_SMSC_ADDRESS] = null;
RIL[REQUEST_REPORT_SMS_MEMORY_STATUS] = null;
RIL[REQUEST_REPORT_STK_SERVICE_IS_RUNNING] = null;
RIL[REQUEST_ACKNOWLEDGE_INCOMING_GSM_SMS_WITH_PDU] = null;
RIL[REQUEST_STK_SEND_ENVELOPE_WITH_STATUS] = function REQUEST_STK_SEND_ENVELOPE_WITH_STATUS(length, options) {
  if (options.rilRequestError) {
    this.acknowledgeGsmSms(false, PDU_FCS_UNSPECIFIED);
    return;
  }

  let sw1 = Buf.readInt32();
  let sw2 = Buf.readInt32();
  if ((sw1 == ICC_STATUS_SAT_BUSY) && (sw2 === 0x00)) {
    this.acknowledgeGsmSms(false, PDU_FCS_USAT_BUSY);
    return;
  }

  let success = ((sw1 == ICC_STATUS_NORMAL_ENDING) && (sw2 === 0x00))
                || (sw1 == ICC_STATUS_NORMAL_ENDING_WITH_EXTRA);

  let messageStringLength = Buf.readInt32(); // In semi-octets
  let responsePduLen = messageStringLength / 2; // In octets
  if (!responsePduLen) {
    this.acknowledgeGsmSms(success, success ? PDU_FCS_OK
                                         : PDU_FCS_USIM_DATA_DOWNLOAD_ERROR);
    return;
  }

  this.acknowledgeIncomingGsmSmsWithPDU(success, responsePduLen, options);
};
RIL[REQUEST_VOICE_RADIO_TECH] = function REQUEST_VOICE_RADIO_TECH(length, options) {
  if (options.rilRequestError) {
    if (DEBUG) {
      debug("Error when getting voice radio tech: " + options.rilRequestError);
    }
    return;
  }
  let radioTech = Buf.readInt32List();
  this._processRadioTech(radioTech[0]);
};
RIL[REQUEST_GET_UNLOCK_RETRY_COUNT] = function REQUEST_GET_UNLOCK_RETRY_COUNT(length, options) {
  options.success = (options.rilRequestError === 0);
  if (!options.success) {
    options.errorMsg = RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError];
  }
  options.retryCount = length ? Buf.readInt32List()[0] : -1;
  this.sendChromeMessage(options);
};
RIL[UNSOLICITED_RESPONSE_RADIO_STATE_CHANGED] = function UNSOLICITED_RESPONSE_RADIO_STATE_CHANGED() {
  let radioState = Buf.readInt32();

  // Ensure radio state at boot time.
  if (this._isInitialRadioState) {
    // Even radioState is RADIO_STATE_OFF, we still have to maually turn radio off,
    // otherwise REQUEST_GET_SIM_STATUS will still report CARD_STATE_PRESENT.
    this.setRadioEnabled({enabled: false});
  }

  let newState;
  if (radioState == RADIO_STATE_UNAVAILABLE) {
    newState = GECKO_RADIOSTATE_UNAVAILABLE;
  } else if (radioState == RADIO_STATE_OFF) {
    newState = GECKO_RADIOSTATE_OFF;
  } else {
    newState = GECKO_RADIOSTATE_READY;
  }

  if (DEBUG) {
    debug("Radio state changed from '" + this.radioState +
          "' to '" + newState + "'");
  }
  if (this.radioState == newState) {
    return;
  }

  switch (radioState) {
  case RADIO_STATE_SIM_READY:
  case RADIO_STATE_SIM_NOT_READY:
  case RADIO_STATE_SIM_LOCKED_OR_ABSENT:
    this._isCdma = false;
    this._waitingRadioTech = false;
    break;
  case RADIO_STATE_RUIM_READY:
  case RADIO_STATE_RUIM_NOT_READY:
  case RADIO_STATE_RUIM_LOCKED_OR_ABSENT:
  case RADIO_STATE_NV_READY:
  case RADIO_STATE_NV_NOT_READY:
    this._isCdma = true;
    this._waitingRadioTech = false;
    break;
  case RADIO_STATE_ON: // RIL v7
    // This value is defined in RIL v7, we will retrieve radio tech by another
    // request. We leave _isCdma untouched, and it will be set once we get the
    // radio technology.
    this._waitingRadioTech = true;
    this.getVoiceRadioTechnology();
    break;
  }

  if ((this.radioState == GECKO_RADIOSTATE_UNAVAILABLE ||
       this.radioState == GECKO_RADIOSTATE_OFF) &&
       newState == GECKO_RADIOSTATE_READY) {
    // The radio became available, let's get its info.
    if (!this._waitingRadioTech) {
      if (this._isCdma) {
        this.getDeviceIdentity();
      } else {
        this.getIMEI();
        this.getIMEISV();
      }
    }
    this.getBasebandVersion();
    this.updateCellBroadcastConfig();
    this.setPreferredNetworkType();
    this.setCLIR();
  }

  this.radioState = newState;
  this.sendChromeMessage({
    rilMessageType: "radiostatechange",
    radioState: newState
  });

  // If the radio is up and on, so let's query the card state.
  // On older RILs only if the card is actually ready, though.
  // If _waitingRadioTech is set, we don't need to get icc status now.
  if (radioState == RADIO_STATE_UNAVAILABLE ||
      radioState == RADIO_STATE_OFF ||
      this._waitingRadioTech) {
    return;
  }
  this.getICCStatus();
};
RIL[UNSOLICITED_RESPONSE_CALL_STATE_CHANGED] = function UNSOLICITED_RESPONSE_CALL_STATE_CHANGED() {
  this.getCurrentCalls();
};
RIL[UNSOLICITED_RESPONSE_VOICE_NETWORK_STATE_CHANGED] = function UNSOLICITED_RESPONSE_VOICE_NETWORK_STATE_CHANGED() {
  if (DEBUG) debug("Network state changed, re-requesting phone state and ICC status");
  this.getICCStatus();
  this.requestNetworkInfo();
};
RIL[UNSOLICITED_RESPONSE_NEW_SMS] = function UNSOLICITED_RESPONSE_NEW_SMS(length) {
  let [message, result] = GsmPDUHelper.processReceivedSms(length);

  if (message) {
    result = this._processSmsMultipart(message);
  }

  if (result == PDU_FCS_RESERVED || result == MOZ_FCS_WAIT_FOR_EXPLICIT_ACK) {
    return;
  }

  // Not reserved FCS values, send ACK now.
  this.acknowledgeGsmSms(result == PDU_FCS_OK, result);
};
RIL[UNSOLICITED_RESPONSE_NEW_SMS_STATUS_REPORT] = function UNSOLICITED_RESPONSE_NEW_SMS_STATUS_REPORT(length) {
  let result = this._processSmsStatusReport(length);
  this.acknowledgeGsmSms(result == PDU_FCS_OK, result);
};
RIL[UNSOLICITED_RESPONSE_NEW_SMS_ON_SIM] = function UNSOLICITED_RESPONSE_NEW_SMS_ON_SIM(length) {
  let recordNumber = Buf.readInt32List()[0];

  ICCRecordHelper.readSMS(
    recordNumber,
    function onsuccess(message) {
      if (message && message.simStatus === 3) { //New Unread SMS
        this._processSmsMultipart(message);
      }
    }.bind(this),
    function onerror(errorMsg) {
      if (DEBUG) {
        debug("Failed to Read NEW SMS on SIM #" + recordNumber + ", errorMsg: " + errorMsg);
      }
    });
};
RIL[UNSOLICITED_ON_USSD] = function UNSOLICITED_ON_USSD() {
  let [typeCode, message] = Buf.readStringList();
  if (DEBUG) {
    debug("On USSD. Type Code: " + typeCode + " Message: " + message);
  }

  this._ussdSession = (typeCode != "0" && typeCode != "2");

  this.sendChromeMessage({rilMessageType: "USSDReceived",
                          message: message,
                          sessionEnded: !this._ussdSession});
};
RIL[UNSOLICITED_NITZ_TIME_RECEIVED] = function UNSOLICITED_NITZ_TIME_RECEIVED() {
  let dateString = Buf.readString();

  // The data contained in the NITZ message is
  // in the form "yy/mm/dd,hh:mm:ss(+/-)tz,dt"
  // for example: 12/02/16,03:36:08-20,00,310410

  // Always print the NITZ info so we can collection what different providers
  // send down the pipe (see bug XXX).
  // TODO once data is collected, add in |if (DEBUG)|

  debug("DateTimeZone string " + dateString);

  let now = Date.now();

  let year = parseInt(dateString.substr(0, 2), 10);
  let month = parseInt(dateString.substr(3, 2), 10);
  let day = parseInt(dateString.substr(6, 2), 10);
  let hours = parseInt(dateString.substr(9, 2), 10);
  let minutes = parseInt(dateString.substr(12, 2), 10);
  let seconds = parseInt(dateString.substr(15, 2), 10);
  // Note that |tz| is in 15-min units.
  let tz = parseInt(dateString.substr(17, 3), 10);
  // Note that |dst| is in 1-hour units and is already applied in |tz|.
  let dst = parseInt(dateString.substr(21, 2), 10);

  let timeInMS = Date.UTC(year + PDU_TIMESTAMP_YEAR_OFFSET, month - 1, day,
                          hours, minutes, seconds);

  if (isNaN(timeInMS)) {
    if (DEBUG) debug("NITZ failed to convert date");
    return;
  }

  this.sendChromeMessage({rilMessageType: "nitzTime",
                          networkTimeInMS: timeInMS,
                          networkTimeZoneInMinutes: -(tz * 15),
                          networkDSTInMinutes: -(dst * 60),
                          receiveTimeInMS: now});
};

RIL[UNSOLICITED_SIGNAL_STRENGTH] = function UNSOLICITED_SIGNAL_STRENGTH(length) {
  this[REQUEST_SIGNAL_STRENGTH](length, {rilRequestError: ERROR_SUCCESS});
};
RIL[UNSOLICITED_DATA_CALL_LIST_CHANGED] = function UNSOLICITED_DATA_CALL_LIST_CHANGED(length) {
  if (RILQUIRKS_V5_LEGACY) {
    this.getDataCallList();
    return;
  }
  this[REQUEST_DATA_CALL_LIST](length, {rilRequestError: ERROR_SUCCESS});
};
RIL[UNSOLICITED_SUPP_SVC_NOTIFICATION] = function UNSOLICITED_SUPP_SVC_NOTIFICATION(length) {
  let info = {};
  info.notificationType = Buf.readInt32();
  info.code = Buf.readInt32();
  info.index = Buf.readInt32();
  info.type = Buf.readInt32();
  info.number = Buf.readString();

  this._processSuppSvcNotification(info);
};

RIL[UNSOLICITED_STK_SESSION_END] = function UNSOLICITED_STK_SESSION_END() {
  this.sendChromeMessage({rilMessageType: "stksessionend"});
};
RIL[UNSOLICITED_STK_PROACTIVE_COMMAND] = function UNSOLICITED_STK_PROACTIVE_COMMAND() {
  this.processStkProactiveCommand();
};
RIL[UNSOLICITED_STK_EVENT_NOTIFY] = function UNSOLICITED_STK_EVENT_NOTIFY() {
  this.processStkProactiveCommand();
};
RIL[UNSOLICITED_STK_CALL_SETUP] = null;
RIL[UNSOLICITED_SIM_SMS_STORAGE_FULL] = null;
RIL[UNSOLICITED_SIM_REFRESH] = null;
RIL[UNSOLICITED_CALL_RING] = function UNSOLICITED_CALL_RING() {
  let info = {rilMessageType: "callRing"};
  let isCDMA = false; //XXX TODO hard-code this for now
  if (isCDMA) {
    info.isPresent = Buf.readInt32();
    info.signalType = Buf.readInt32();
    info.alertPitch = Buf.readInt32();
    info.signal = Buf.readInt32();
  }
  // At this point we don't know much other than the fact there's an incoming
  // call, but that's enough to bring up the Phone app already. We'll know
  // details once we get a call state changed notification and can then
  // dispatch DOM events etc.
  this.sendChromeMessage(info);
};
RIL[UNSOLICITED_RESPONSE_SIM_STATUS_CHANGED] = function UNSOLICITED_RESPONSE_SIM_STATUS_CHANGED() {
  this.getICCStatus();
};
RIL[UNSOLICITED_RESPONSE_CDMA_NEW_SMS] = function UNSOLICITED_RESPONSE_CDMA_NEW_SMS(length) {
  let [message, result] = CdmaPDUHelper.processReceivedSms(length);

  if (message) {
    if (message.subMsgType === PDU_CDMA_MSG_TYPE_DELIVER_ACK) {
      result = this._processCdmaSmsStatusReport(message);
    } else {
      result = this._processSmsMultipart(message);
    }
  }

  if (result == PDU_FCS_RESERVED || result == MOZ_FCS_WAIT_FOR_EXPLICIT_ACK) {
    return;
  }

  // Not reserved FCS values, send ACK now.
  this.acknowledgeCdmaSms(result == PDU_FCS_OK, result);
};
RIL[UNSOLICITED_RESPONSE_NEW_BROADCAST_SMS] = function UNSOLICITED_RESPONSE_NEW_BROADCAST_SMS(length) {
  let message;
  try {
    message = GsmPDUHelper.readCbMessage(Buf.readInt32());
  } catch (e) {
    if (DEBUG) {
      debug("Failed to parse Cell Broadcast message: " + JSON.stringify(e));
    }
    return;
  }

  message = this._processReceivedSmsCbPage(message);
  if (!message) {
    return;
  }

  message.rilMessageType = "cellbroadcast-received";
  this.sendChromeMessage(message);
};
RIL[UNSOLICITED_CDMA_RUIM_SMS_STORAGE_FULL] = null;
RIL[UNSOLICITED_RESTRICTED_STATE_CHANGED] = null;
RIL[UNSOLICITED_ENTER_EMERGENCY_CALLBACK_MODE] = function UNSOLICITED_ENTER_EMERGENCY_CALLBACK_MODE() {
  this._handleChangedEmergencyCbMode(true);
};
RIL[UNSOLICITED_CDMA_CALL_WAITING] = function UNSOLICITED_CDMA_CALL_WAITING(length) {
  let call = {};
  call.number              = Buf.readString();
  call.numberPresentation  = Buf.readInt32();
  call.name                = Buf.readString();
  call.namePresentation    = Buf.readInt32();
  call.isPresent           = Buf.readInt32();
  call.signalType          = Buf.readInt32();
  call.alertPitch          = Buf.readInt32();
  call.signal              = Buf.readInt32();
  this.sendChromeMessage({rilMessageType: "cdmaCallWaiting",
                          number: call.number});
};
RIL[UNSOLICITED_CDMA_OTA_PROVISION_STATUS] = function UNSOLICITED_CDMA_OTA_PROVISION_STATUS() {
  let status = Buf.readInt32List()[0];
  this.sendChromeMessage({rilMessageType: "otastatuschange",
                          status: status});
};
RIL[UNSOLICITED_CDMA_INFO_REC] = function UNSOLICITED_CDMA_INFO_REC(length) {
  let record = CdmaPDUHelper.decodeInformationRecord();
  record.rilMessageType = "cdma-info-rec-received";
  this.sendChromeMessage(record);
};
RIL[UNSOLICITED_OEM_HOOK_RAW] = null;
RIL[UNSOLICITED_RINGBACK_TONE] = null;
RIL[UNSOLICITED_RESEND_INCALL_MUTE] = null;
RIL[UNSOLICITED_EXIT_EMERGENCY_CALLBACK_MODE] = function UNSOLICITED_EXIT_EMERGENCY_CALLBACK_MODE() {
  this._handleChangedEmergencyCbMode(false);
};
RIL[UNSOLICITED_RIL_CONNECTED] = function UNSOLICITED_RIL_CONNECTED(length) {
  // Prevent response id collision between UNSOLICITED_RIL_CONNECTED and
  // UNSOLICITED_VOICE_RADIO_TECH_CHANGED for Akami on gingerbread branch.
  if (!length) {
    return;
  }

  let version = Buf.readInt32List()[0];
  RILQUIRKS_V5_LEGACY = (version < 5);
  if (DEBUG) {
    debug("Detected RIL version " + version);
    debug("RILQUIRKS_V5_LEGACY is " + RILQUIRKS_V5_LEGACY);
  }

  this.initRILState();
  // Always ensure that we are not in emergency callback mode when init.
  this.exitEmergencyCbMode();
};

/**
 * This object exposes the functionality to parse and serialize PDU strings
 *
 * A PDU is a string containing a series of hexadecimally encoded octets
 * or nibble-swapped binary-coded decimals (BCDs). It contains not only the
 * message text but information about the sender, the SMS service center,
 * timestamp, etc.
 */
let GsmPDUHelper = {

  /**
   * Read one character (2 bytes) from a RIL string and decode as hex.
   *
   * @return the nibble as a number.
   */
  readHexNibble: function readHexNibble() {
    let nibble = Buf.readUint16();
    if (nibble >= 48 && nibble <= 57) {
      nibble -= 48; // ASCII '0'..'9'
    } else if (nibble >= 65 && nibble <= 70) {
      nibble -= 55; // ASCII 'A'..'F'
    } else if (nibble >= 97 && nibble <= 102) {
      nibble -= 87; // ASCII 'a'..'f'
    } else {
      throw "Found invalid nibble during PDU parsing: " +
            String.fromCharCode(nibble);
    }
    return nibble;
  },

  /**
   * Encode a nibble as one hex character in a RIL string (2 bytes).
   *
   * @param nibble
   *        The nibble to encode (represented as a number)
   */
  writeHexNibble: function writeHexNibble(nibble) {
    nibble &= 0x0f;
    if (nibble < 10) {
      nibble += 48; // ASCII '0'
    } else {
      nibble += 55; // ASCII 'A'
    }
    Buf.writeUint16(nibble);
  },

  /**
   * Read a hex-encoded octet (two nibbles).
   *
   * @return the octet as a number.
   */
  readHexOctet: function readHexOctet() {
    return (this.readHexNibble() << 4) | this.readHexNibble();
  },

  /**
   * Write an octet as two hex-encoded nibbles.
   *
   * @param octet
   *        The octet (represented as a number) to encode.
   */
  writeHexOctet: function writeHexOctet(octet) {
    this.writeHexNibble(octet >> 4);
    this.writeHexNibble(octet);
  },

  /**
   * Read an array of hex-encoded octets.
   */
  readHexOctetArray: function readHexOctetArray(length) {
    let array = new Uint8Array(length);
    for (let i = 0; i < length; i++) {
      array[i] = this.readHexOctet();
    }
    return array;
  },

  /**
   * Convert an octet (number) to a BCD number.
   *
   * Any nibbles that are not in the BCD range count as 0.
   *
   * @param octet
   *        The octet (a number, as returned by getOctet())
   *
   * @return the corresponding BCD number.
   */
  octetToBCD: function octetToBCD(octet) {
    return ((octet & 0xf0) <= 0x90) * ((octet >> 4) & 0x0f) +
           ((octet & 0x0f) <= 0x09) * (octet & 0x0f) * 10;
  },

  /**
   * Convert a BCD number to an octet (number)
   *
   * Only take two digits with absolute value.
   *
   * @param bcd
   *
   * @return the corresponding octet.
   */
  BCDToOctet: function BCDToOctet(bcd) {
    bcd = Math.abs(bcd);
    return ((bcd % 10) << 4) + (Math.floor(bcd / 10) % 10);
  },

  /**
   * Convert a semi-octet (number) to a GSM BCD char.
   */
  bcdChars: "0123456789*#,;",
  semiOctetToBcdChar: function semiOctetToBcdChar(semiOctet) {
    if (semiOctet >= 14) {
      throw new RangeError();
    }

    return this.bcdChars.charAt(semiOctet);
  },

  /**
   * Read a *swapped nibble* binary coded decimal (BCD)
   *
   * @param pairs
   *        Number of nibble *pairs* to read.
   *
   * @return the decimal as a number.
   */
  readSwappedNibbleBcdNum: function readSwappedNibbleBcdNum(pairs) {
    let number = 0;
    for (let i = 0; i < pairs; i++) {
      let octet = this.readHexOctet();
      // Ignore 'ff' octets as they're often used as filler.
      if (octet == 0xff) {
        continue;
      }
      // If the first nibble is an "F" , only the second nibble is to be taken
      // into account.
      if ((octet & 0xf0) == 0xf0) {
        number *= 10;
        number += octet & 0x0f;
        continue;
      }
      number *= 100;
      number += this.octetToBCD(octet);
    }
    return number;
  },

  /**
   * Read a *swapped nibble* binary coded string (BCD)
   *
   * @param pairs
   *        Number of nibble *pairs* to read.
   *
   * @return The BCD string.
   */
  readSwappedNibbleBcdString: function readSwappedNibbleBcdString(pairs) {
    let str = "";
    for (let i = 0; i < pairs; i++) {
      let nibbleH = this.readHexNibble();
      let nibbleL = this.readHexNibble();
      if (nibbleL == 0x0F) {
        break;
      }

      str += this.semiOctetToBcdChar(nibbleL);
      if (nibbleH != 0x0F) {
        str += this.semiOctetToBcdChar(nibbleH);
      }
    }

    return str;
  },

  /**
   * Write numerical data as swapped nibble BCD.
   *
   * @param data
   *        Data to write (as a string or a number)
   */
  writeSwappedNibbleBCD: function writeSwappedNibbleBCD(data) {
    data = data.toString();
    if (data.length % 2) {
      data += "F";
    }
    for (let i = 0; i < data.length; i += 2) {
      Buf.writeUint16(data.charCodeAt(i + 1));
      Buf.writeUint16(data.charCodeAt(i));
    }
  },

  /**
   * Write numerical data as swapped nibble BCD.
   * If the number of digit of data is even, add '0' at the beginning.
   *
   * @param data
   *        Data to write (as a string or a number)
   */
  writeSwappedNibbleBCDNum: function writeSwappedNibbleBCDNum(data) {
    data = data.toString();
    if (data.length % 2) {
      data = "0" + data;
    }
    for (let i = 0; i < data.length; i += 2) {
      Buf.writeUint16(data.charCodeAt(i + 1));
      Buf.writeUint16(data.charCodeAt(i));
    }
  },

  /**
   * Read user data, convert to septets, look up relevant characters in a
   * 7-bit alphabet, and construct string.
   *
   * @param length
   *        Number of septets to read (*not* octets)
   * @param paddingBits
   *        Number of padding bits in the first byte of user data.
   * @param langIndex
   *        Table index used for normal 7-bit encoded character lookup.
   * @param langShiftIndex
   *        Table index used for escaped 7-bit encoded character lookup.
   *
   * @return a string.
   */
  readSeptetsToString: function readSeptetsToString(length, paddingBits, langIndex, langShiftIndex) {
    let ret = "";
    let byteLength = Math.ceil((length * 7 + paddingBits) / 8);

    /**
     * |<-                    last byte in header                    ->|
     * |<-           incompleteBits          ->|<- last header septet->|
     * +===7===|===6===|===5===|===4===|===3===|===2===|===1===|===0===|
     *
     * |<-                   1st byte in user data                   ->|
     * |<-               data septet 1               ->|<-paddingBits->|
     * +===7===|===6===|===5===|===4===|===3===|===2===|===1===|===0===|
     *
     * |<-                   2nd byte in user data                   ->|
     * |<-                   data spetet 2                   ->|<-ds1->|
     * +===7===|===6===|===5===|===4===|===3===|===2===|===1===|===0===|
     */
    let data = 0;
    let dataBits = 0;
    if (paddingBits) {
      data = this.readHexOctet() >> paddingBits;
      dataBits = 8 - paddingBits;
      --byteLength;
    }

    let escapeFound = false;
    const langTable = PDU_NL_LOCKING_SHIFT_TABLES[langIndex];
    const langShiftTable = PDU_NL_SINGLE_SHIFT_TABLES[langShiftIndex];
    do {
      // Read as much as fits in 32bit word
      let bytesToRead = Math.min(byteLength, dataBits ? 3 : 4);
      for (let i = 0; i < bytesToRead; i++) {
        data |= this.readHexOctet() << dataBits;
        dataBits += 8;
        --byteLength;
      }

      // Consume available full septets
      for (; dataBits >= 7; dataBits -= 7) {
        let septet = data & 0x7F;
        data >>>= 7;

        if (escapeFound) {
          escapeFound = false;
          if (septet == PDU_NL_EXTENDED_ESCAPE) {
            // According to 3GPP TS 23.038, section 6.2.1.1, NOTE 1, "On
            // receipt of this code, a receiving entity shall display a space
            // until another extensiion table is defined."
            ret += " ";
          } else if (septet == PDU_NL_RESERVED_CONTROL) {
            // According to 3GPP TS 23.038 B.2, "This code represents a control
            // character and therefore must not be used for language specific
            // characters."
            ret += " ";
          } else {
            ret += langShiftTable[septet];
          }
        } else if (septet == PDU_NL_EXTENDED_ESCAPE) {
          escapeFound = true;

          // <escape> is not an effective character
          --length;
        } else {
          ret += langTable[septet];
        }
      }
    } while (byteLength);

    if (ret.length != length) {
      /**
       * If num of effective characters does not equal to the length of read
       * string, cut the tail off. This happens when the last octet of user
       * data has following layout:
       *
       * |<-              penultimate octet in user data               ->|
       * |<-               data septet N               ->|<-   dsN-1   ->|
       * +===7===|===6===|===5===|===4===|===3===|===2===|===1===|===0===|
       *
       * |<-                  last octet in user data                  ->|
       * |<-                       fill bits                   ->|<-dsN->|
       * +===7===|===6===|===5===|===4===|===3===|===2===|===1===|===0===|
       *
       * The fill bits in the last octet may happen to form a full septet and
       * be appended at the end of result string.
       */
      ret = ret.slice(0, length);
    }
    return ret;
  },

  writeStringAsSeptets: function writeStringAsSeptets(message, paddingBits, langIndex, langShiftIndex) {
    const langTable = PDU_NL_LOCKING_SHIFT_TABLES[langIndex];
    const langShiftTable = PDU_NL_SINGLE_SHIFT_TABLES[langShiftIndex];

    let dataBits = paddingBits;
    let data = 0;
    for (let i = 0; i < message.length; i++) {
      let c = message.charAt(i);
      let septet = langTable.indexOf(c);
      if (septet == PDU_NL_EXTENDED_ESCAPE) {
        continue;
      }

      if (septet >= 0) {
        data |= septet << dataBits;
        dataBits += 7;
      } else {
        septet = langShiftTable.indexOf(c);
        if (septet == -1) {
          throw new Error("'" + c + "' is not in 7 bit alphabet "
                          + langIndex + ":" + langShiftIndex + "!");
        }

        if (septet == PDU_NL_RESERVED_CONTROL) {
          continue;
        }

        data |= PDU_NL_EXTENDED_ESCAPE << dataBits;
        dataBits += 7;
        data |= septet << dataBits;
        dataBits += 7;
      }

      for (; dataBits >= 8; dataBits -= 8) {
        this.writeHexOctet(data & 0xFF);
        data >>>= 8;
      }
    }

    if (dataBits !== 0) {
      this.writeHexOctet(data & 0xFF);
    }
  },

  /**
   * Read user data and decode as a UCS2 string.
   *
   * @param numOctets
   *        Number of octets to be read as UCS2 string.
   *
   * @return a string.
   */
  readUCS2String: function readUCS2String(numOctets) {
    let str = "";
    let length = numOctets / 2;
    for (let i = 0; i < length; ++i) {
      let code = (this.readHexOctet() << 8) | this.readHexOctet();
      str += String.fromCharCode(code);
    }

    if (DEBUG) debug("Read UCS2 string: " + str);

    return str;
  },

  /**
   * Write user data as a UCS2 string.
   *
   * @param message
   *        Message string to encode as UCS2 in hex-encoded octets.
   */
  writeUCS2String: function writeUCS2String(message) {
    for (let i = 0; i < message.length; ++i) {
      let code = message.charCodeAt(i);
      this.writeHexOctet((code >> 8) & 0xFF);
      this.writeHexOctet(code & 0xFF);
    }
  },

  /**
   * Read 1 + UDHL octets and construct user data header.
   *
   * @param msg
   *        message object for output.
   *
   * @see 3GPP TS 23.040 9.2.3.24
   */
  readUserDataHeader: function readUserDataHeader(msg) {
    /**
     * A header object with properties contained in received message.
     * The properties set include:
     *
     * length: totoal length of the header, default 0.
     * langIndex: used locking shift table index, default
     * PDU_NL_IDENTIFIER_DEFAULT.
     * langShiftIndex: used locking shift table index, default
     * PDU_NL_IDENTIFIER_DEFAULT.
     *
     */
    let header = {
      length: 0,
      langIndex: PDU_NL_IDENTIFIER_DEFAULT,
      langShiftIndex: PDU_NL_IDENTIFIER_DEFAULT
    };

    header.length = this.readHexOctet();
    if (DEBUG) debug("Read UDH length: " + header.length);

    let dataAvailable = header.length;
    while (dataAvailable >= 2) {
      let id = this.readHexOctet();
      let length = this.readHexOctet();
      if (DEBUG) debug("Read UDH id: " + id + ", length: " + length);

      dataAvailable -= 2;

      switch (id) {
        case PDU_IEI_CONCATENATED_SHORT_MESSAGES_8BIT: {
          let ref = this.readHexOctet();
          let max = this.readHexOctet();
          let seq = this.readHexOctet();
          dataAvailable -= 3;
          if (max && seq && (seq <= max)) {
            header.segmentRef = ref;
            header.segmentMaxSeq = max;
            header.segmentSeq = seq;
          }
          break;
        }
        case PDU_IEI_APPLICATION_PORT_ADDRESSING_SCHEME_8BIT: {
          let dstp = this.readHexOctet();
          let orip = this.readHexOctet();
          dataAvailable -= 2;
          if ((dstp < PDU_APA_RESERVED_8BIT_PORTS)
              || (orip < PDU_APA_RESERVED_8BIT_PORTS)) {
            // 3GPP TS 23.040 clause 9.2.3.24.3: "A receiving entity shall
            // ignore any information element where the value of the
            // Information-Element-Data is Reserved or not supported"
            break;
          }
          header.destinationPort = dstp;
          header.originatorPort = orip;
          break;
        }
        case PDU_IEI_APPLICATION_PORT_ADDRESSING_SCHEME_16BIT: {
          let dstp = (this.readHexOctet() << 8) | this.readHexOctet();
          let orip = (this.readHexOctet() << 8) | this.readHexOctet();
          dataAvailable -= 4;
          // 3GPP TS 23.040 clause 9.2.3.24.4: "A receiving entity shall
          // ignore any information element where the value of the
          // Information-Element-Data is Reserved or not supported"
          if ((dstp < PDU_APA_VALID_16BIT_PORTS)
              && (orip < PDU_APA_VALID_16BIT_PORTS)) {
            header.destinationPort = dstp;
            header.originatorPort = orip;
          }
          break;
        }
        case PDU_IEI_CONCATENATED_SHORT_MESSAGES_16BIT: {
          let ref = (this.readHexOctet() << 8) | this.readHexOctet();
          let max = this.readHexOctet();
          let seq = this.readHexOctet();
          dataAvailable -= 4;
          if (max && seq && (seq <= max)) {
            header.segmentRef = ref;
            header.segmentMaxSeq = max;
            header.segmentSeq = seq;
          }
          break;
        }
        case PDU_IEI_NATIONAL_LANGUAGE_SINGLE_SHIFT:
          let langShiftIndex = this.readHexOctet();
          --dataAvailable;
          if (langShiftIndex < PDU_NL_SINGLE_SHIFT_TABLES.length) {
            header.langShiftIndex = langShiftIndex;
          }
          break;
        case PDU_IEI_NATIONAL_LANGUAGE_LOCKING_SHIFT:
          let langIndex = this.readHexOctet();
          --dataAvailable;
          if (langIndex < PDU_NL_LOCKING_SHIFT_TABLES.length) {
            header.langIndex = langIndex;
          }
          break;
        case PDU_IEI_SPECIAL_SMS_MESSAGE_INDICATION:
          let msgInd = this.readHexOctet() & 0xFF;
          let msgCount = this.readHexOctet();
          dataAvailable -= 2;


          /*
           * TS 23.040 V6.8.1 Sec 9.2.3.24.2
           * bits 1 0   : basic message indication type
           * bits 4 3 2 : extended message indication type
           * bits 6 5   : Profile id
           * bit  7     : storage type
           */
          let storeType = msgInd & PDU_MWI_STORE_TYPE_BIT;
          let mwi = msg.mwi;
          if (!mwi) {
            mwi = msg.mwi = {};
          }

          if (storeType == PDU_MWI_STORE_TYPE_STORE) {
            // Store message because TP_UDH indicates so, note this may override
            // the setting in DCS, but that is expected
            mwi.discard = false;
          } else if (mwi.discard === undefined) {
            // storeType == PDU_MWI_STORE_TYPE_DISCARD
            // only override mwi.discard here if it hasn't already been set
            mwi.discard = true;
          }

          mwi.msgCount = msgCount & 0xFF;
          mwi.active = mwi.msgCount > 0;

          if (DEBUG) debug("MWI in TP_UDH received: " + JSON.stringify(mwi));

          break;
        default:
          if (DEBUG) {
            debug("readUserDataHeader: unsupported IEI(" + id
                  + "), " + length + " bytes.");
          }

          // Read out unsupported data
          if (length) {
            let octets;
            if (DEBUG) octets = new Uint8Array(length);

            for (let i = 0; i < length; i++) {
              let octet = this.readHexOctet();
              if (DEBUG) octets[i] = octet;
            }
            dataAvailable -= length;

            if (DEBUG) debug("readUserDataHeader: " + Array.slice(octets));
          }
          break;
      }
    }

    if (dataAvailable !== 0) {
      throw new Error("Illegal user data header found!");
    }

    msg.header = header;
  },

  /**
   * Write out user data header.
   *
   * @param options
   *        Options containing information for user data header write-out. The
   *        `userDataHeaderLength` property must be correctly pre-calculated.
   */
  writeUserDataHeader: function writeUserDataHeader(options) {
    this.writeHexOctet(options.userDataHeaderLength);

    if (options.segmentMaxSeq > 1) {
      if (options.segmentRef16Bit) {
        this.writeHexOctet(PDU_IEI_CONCATENATED_SHORT_MESSAGES_16BIT);
        this.writeHexOctet(4);
        this.writeHexOctet((options.segmentRef >> 8) & 0xFF);
      } else {
        this.writeHexOctet(PDU_IEI_CONCATENATED_SHORT_MESSAGES_8BIT);
        this.writeHexOctet(3);
      }
      this.writeHexOctet(options.segmentRef & 0xFF);
      this.writeHexOctet(options.segmentMaxSeq & 0xFF);
      this.writeHexOctet(options.segmentSeq & 0xFF);
    }

    if (options.dcs == PDU_DCS_MSG_CODING_7BITS_ALPHABET) {
      if (options.langIndex != PDU_NL_IDENTIFIER_DEFAULT) {
        this.writeHexOctet(PDU_IEI_NATIONAL_LANGUAGE_LOCKING_SHIFT);
        this.writeHexOctet(1);
        this.writeHexOctet(options.langIndex);
      }

      if (options.langShiftIndex != PDU_NL_IDENTIFIER_DEFAULT) {
        this.writeHexOctet(PDU_IEI_NATIONAL_LANGUAGE_SINGLE_SHIFT);
        this.writeHexOctet(1);
        this.writeHexOctet(options.langShiftIndex);
      }
    }
  },

  /**
   * Read SM-TL Address.
   *
   * @param len
   *        Length of useful semi-octets within the Address-Value field. For
   *        example, the lenth of "12345" should be 5, and 4 for "1234".
   *
   * @see 3GPP TS 23.040 9.1.2.5
   */
  readAddress: function readAddress(len) {
    // Address Length
    if (!len || (len < 0)) {
      if (DEBUG) debug("PDU error: invalid sender address length: " + len);
      return null;
    }
    if (len % 2 == 1) {
      len += 1;
    }
    if (DEBUG) debug("PDU: Going to read address: " + len);

    // Type-of-Address
    let toa = this.readHexOctet();
    let addr = "";

    if ((toa & 0xF0) == PDU_TOA_ALPHANUMERIC) {
      addr = this.readSeptetsToString(Math.floor(len * 4 / 7), 0,
          PDU_NL_IDENTIFIER_DEFAULT , PDU_NL_IDENTIFIER_DEFAULT );
      return addr;
    }
    addr = this.readSwappedNibbleBcdString(len / 2);
    if (addr.length <= 0) {
      if (DEBUG) debug("PDU error: no number provided");
      return null;
    }
    if ((toa & 0xF0) == (PDU_TOA_INTERNATIONAL)) {
      addr = '+' + addr;
    }

    return addr;
  },

  /**
   * Read TP-Protocol-Indicator(TP-PID).
   *
   * @param msg
   *        message object for output.
   *
   * @see 3GPP TS 23.040 9.2.3.9
   */
  readProtocolIndicator: function readProtocolIndicator(msg) {
    // `The MS shall interpret reserved, obsolete, or unsupported values as the
    // value 00000000 but shall store them exactly as received.`
    msg.pid = this.readHexOctet();

    msg.epid = msg.pid;
    switch (msg.epid & 0xC0) {
      case 0x40:
        // Bit 7..0 = 01xxxxxx
        switch (msg.epid) {
          case PDU_PID_SHORT_MESSAGE_TYPE_0:
          case PDU_PID_ANSI_136_R_DATA:
          case PDU_PID_USIM_DATA_DOWNLOAD:
            return;
          case PDU_PID_RETURN_CALL_MESSAGE:
            // Level 1 of message waiting indication:
            // Only a return call message is provided
            let mwi = msg.mwi = {};

            // TODO: When should we de-activate the level 1 indicator?
            mwi.active = true;
            mwi.discard = false;
            mwi.msgCount = GECKO_VOICEMAIL_MESSAGE_COUNT_UNKNOWN;
            if (DEBUG) debug("TP-PID got return call message: " + msg.sender);
            return;
        }
        break;
    }

    msg.epid = PDU_PID_DEFAULT;
  },

  /**
   * Read TP-Data-Coding-Scheme(TP-DCS)
   *
   * @param msg
   *        message object for output.
   *
   * @see 3GPP TS 23.040 9.2.3.10, 3GPP TS 23.038 4.
   */
  readDataCodingScheme: function readDataCodingScheme(msg) {
    let dcs = this.readHexOctet();
    if (DEBUG) debug("PDU: read SMS dcs: " + dcs);

    // No message class by default.
    let messageClass = PDU_DCS_MSG_CLASS_NORMAL;
    // 7 bit is the default fallback encoding.
    let encoding = PDU_DCS_MSG_CODING_7BITS_ALPHABET;
    switch (dcs & PDU_DCS_CODING_GROUP_BITS) {
      case 0x40: // bits 7..4 = 01xx
      case 0x50:
      case 0x60:
      case 0x70:
        // Bit 5..0 are coded exactly the same as Group 00xx
      case 0x00: // bits 7..4 = 00xx
      case 0x10:
      case 0x20:
      case 0x30:
        if (dcs & 0x10) {
          messageClass = dcs & PDU_DCS_MSG_CLASS_BITS;
        }
        switch (dcs & 0x0C) {
          case 0x4:
            encoding = PDU_DCS_MSG_CODING_8BITS_ALPHABET;
            break;
          case 0x8:
            encoding = PDU_DCS_MSG_CODING_16BITS_ALPHABET;
            break;
        }
        break;

      case 0xE0: // bits 7..4 = 1110
        encoding = PDU_DCS_MSG_CODING_16BITS_ALPHABET;
        // Bit 3..0 are coded exactly the same as Message Waiting Indication
        // Group 1101.
        // Fall through.
      case 0xC0: // bits 7..4 = 1100
      case 0xD0: // bits 7..4 = 1101
        // Indiciates voicemail indicator set or clear
        let active = (dcs & PDU_DCS_MWI_ACTIVE_BITS) == PDU_DCS_MWI_ACTIVE_VALUE;

        // If TP-UDH is present, these values will be overwritten
        switch (dcs & PDU_DCS_MWI_TYPE_BITS) {
          case PDU_DCS_MWI_TYPE_VOICEMAIL:
            let mwi = msg.mwi;
            if (!mwi) {
              mwi = msg.mwi = {};
            }

            mwi.active = active;
            mwi.discard = (dcs & PDU_DCS_CODING_GROUP_BITS) == 0xC0;
            mwi.msgCount = active ? GECKO_VOICEMAIL_MESSAGE_COUNT_UNKNOWN : 0;

            if (DEBUG) {
              debug("MWI in DCS received for voicemail: " + JSON.stringify(mwi));
            }
            break;
          case PDU_DCS_MWI_TYPE_FAX:
            if (DEBUG) debug("MWI in DCS received for fax");
            break;
          case PDU_DCS_MWI_TYPE_EMAIL:
            if (DEBUG) debug("MWI in DCS received for email");
            break;
          default:
            if (DEBUG) debug("MWI in DCS received for \"other\"");
            break;
        }
        break;

      case 0xF0: // bits 7..4 = 1111
        if (dcs & 0x04) {
          encoding = PDU_DCS_MSG_CODING_8BITS_ALPHABET;
        }
        messageClass = dcs & PDU_DCS_MSG_CLASS_BITS;
        break;

      default:
        // Falling back to default encoding.
        break;
    }

    msg.dcs = dcs;
    msg.encoding = encoding;
    msg.messageClass = GECKO_SMS_MESSAGE_CLASSES[messageClass];

    if (DEBUG) debug("PDU: message encoding is " + encoding + " bit.");
  },

  /**
   * Read GSM TP-Service-Centre-Time-Stamp(TP-SCTS).
   *
   * @see 3GPP TS 23.040 9.2.3.11
   */
  readTimestamp: function readTimestamp() {
    let year   = this.readSwappedNibbleBcdNum(1) + PDU_TIMESTAMP_YEAR_OFFSET;
    let month  = this.readSwappedNibbleBcdNum(1) - 1;
    let day    = this.readSwappedNibbleBcdNum(1);
    let hour   = this.readSwappedNibbleBcdNum(1);
    let minute = this.readSwappedNibbleBcdNum(1);
    let second = this.readSwappedNibbleBcdNum(1);
    let timestamp = Date.UTC(year, month, day, hour, minute, second);

    // If the most significant bit of the least significant nibble is 1,
    // the timezone offset is negative (fourth bit from the right => 0x08):
    //   localtime = UTC + tzOffset
    // therefore
    //   UTC = localtime - tzOffset
    let tzOctet = this.readHexOctet();
    let tzOffset = this.octetToBCD(tzOctet & ~0x08) * 15 * 60 * 1000;
    tzOffset = (tzOctet & 0x08) ? -tzOffset : tzOffset;
    timestamp -= tzOffset;

    return timestamp;
  },

  /**
   * Write GSM TP-Service-Centre-Time-Stamp(TP-SCTS).
   *
   * @see 3GPP TS 23.040 9.2.3.11
   */
  writeTimestamp: function writeTimestamp(date) {
    this.writeSwappedNibbleBCDNum(date.getFullYear() - PDU_TIMESTAMP_YEAR_OFFSET);

    // The value returned by getMonth() is an integer between 0 and 11.
    // 0 is corresponds to January, 1 to February, and so on.
    this.writeSwappedNibbleBCDNum(date.getMonth() + 1);
    this.writeSwappedNibbleBCDNum(date.getDate());
    this.writeSwappedNibbleBCDNum(date.getHours());
    this.writeSwappedNibbleBCDNum(date.getMinutes());
    this.writeSwappedNibbleBCDNum(date.getSeconds());

    // the value returned by getTimezoneOffset() is the difference,
    // in minutes, between UTC and local time.
    // For example, if your time zone is UTC+10 (Australian Eastern Standard Time),
    // -600 will be returned.
    // In TS 23.040 9.2.3.11, the Time Zone field of TP-SCTS indicates
    // the different between the local time and GMT.
    // And expressed in quarters of an hours. (so need to divid by 15)
    let zone = date.getTimezoneOffset() / 15;
    let octet = this.BCDToOctet(zone);

    // the bit3 of the Time Zone field represents the algebraic sign.
    // (0: positive, 1: negative).
    // For example, if the time zone is -0800 GMT,
    // 480 will be returned by getTimezoneOffset().
    // In this case, need to mark sign bit as 1. => 0x08
    if (zone > 0) {
      octet = octet | 0x08;
    }
    this.writeHexOctet(octet);
  },

  /**
   * User data can be 7 bit (default alphabet) data, 8 bit data, or 16 bit
   * (UCS2) data.
   *
   * @param msg
   *        message object for output.
   * @param length
   *        length of user data to read in octets.
   */
  readUserData: function readUserData(msg, length) {
    if (DEBUG) {
      debug("Reading " + length + " bytes of user data.");
    }

    let paddingBits = 0;
    if (msg.udhi) {
      this.readUserDataHeader(msg);

      if (msg.encoding == PDU_DCS_MSG_CODING_7BITS_ALPHABET) {
        let headerBits = (msg.header.length + 1) * 8;
        let headerSeptets = Math.ceil(headerBits / 7);

        length -= headerSeptets;
        paddingBits = headerSeptets * 7 - headerBits;
      } else {
        length -= (msg.header.length + 1);
      }
    }

    if (DEBUG) debug("After header, " + length + " septets left of user data");

    msg.body = null;
    msg.data = null;
    switch (msg.encoding) {
      case PDU_DCS_MSG_CODING_7BITS_ALPHABET:
        // 7 bit encoding allows 140 octets, which means 160 characters
        // ((140x8) / 7 = 160 chars)
        if (length > PDU_MAX_USER_DATA_7BIT) {
          if (DEBUG) debug("PDU error: user data is too long: " + length);
          break;
        }

        let langIndex = msg.udhi ? msg.header.langIndex : PDU_NL_IDENTIFIER_DEFAULT;
        let langShiftIndex = msg.udhi ? msg.header.langShiftIndex : PDU_NL_IDENTIFIER_DEFAULT;
        msg.body = this.readSeptetsToString(length, paddingBits, langIndex,
                                            langShiftIndex);
        break;
      case PDU_DCS_MSG_CODING_8BITS_ALPHABET:
        msg.data = this.readHexOctetArray(length);
        break;
      case PDU_DCS_MSG_CODING_16BITS_ALPHABET:
        msg.body = this.readUCS2String(length);
        break;
    }
  },

  /**
   * Read extra parameters if TP-PI is set.
   *
   * @param msg
   *        message object for output.
   */
  readExtraParams: function readExtraParams(msg) {
    // Because each PDU octet is converted to two UCS2 char2, we should always
    // get even messageStringLength in this#_processReceivedSms(). So, we'll
    // always need two delimitors at the end.
    if (Buf.getReadAvailable() <= 4) {
      return;
    }

    // TP-Parameter-Indicator
    let pi;
    do {
      // `The most significant bit in octet 1 and any other TP-PI octets which
      // may be added later is reserved as an extension bit which when set to a
      // 1 shall indicate that another TP-PI octet follows immediately
      // afterwards.` ~ 3GPP TS 23.040 9.2.3.27
      pi = this.readHexOctet();
    } while (pi & PDU_PI_EXTENSION);

    // `If the TP-UDL bit is set to "1" but the TP-DCS bit is set to "0" then
    // the receiving entity shall for TP-DCS assume a value of 0x00, i.e. the
    // 7bit default alphabet.` ~ 3GPP 23.040 9.2.3.27
    msg.dcs = 0;
    msg.encoding = PDU_DCS_MSG_CODING_7BITS_ALPHABET;

    // TP-Protocol-Identifier
    if (pi & PDU_PI_PROTOCOL_IDENTIFIER) {
      this.readProtocolIndicator(msg);
    }
    // TP-Data-Coding-Scheme
    if (pi & PDU_PI_DATA_CODING_SCHEME) {
      this.readDataCodingScheme(msg);
    }
    // TP-User-Data-Length
    if (pi & PDU_PI_USER_DATA_LENGTH) {
      let userDataLength = this.readHexOctet();
      this.readUserData(msg, userDataLength);
    }
  },

  /**
   * Read and decode a PDU-encoded message from the stream.
   *
   * TODO: add some basic sanity checks like:
   * - do we have the minimum number of chars available
   */
  readMessage: function readMessage() {
    // An empty message object. This gets filled below and then returned.
    let msg = {
      // D:DELIVER, DR:DELIVER-REPORT, S:SUBMIT, SR:SUBMIT-REPORT,
      // ST:STATUS-REPORT, C:COMMAND
      // M:Mandatory, O:Optional, X:Unavailable
      //                  D  DR S  SR ST C
      SMSC:      null, // M  M  M  M  M  M
      mti:       null, // M  M  M  M  M  M
      udhi:      null, // M  M  O  M  M  M
      sender:    null, // M  X  X  X  X  X
      recipient: null, // X  X  M  X  M  M
      pid:       null, // M  O  M  O  O  M
      epid:      null, // M  O  M  O  O  M
      dcs:       null, // M  O  M  O  O  X
      mwi:       null, // O  O  O  O  O  O
      replace:  false, // O  O  O  O  O  O
      header:    null, // M  M  O  M  M  M
      body:      null, // M  O  M  O  O  O
      data:      null, // M  O  M  O  O  O
      timestamp: null, // M  X  X  X  X  X
      status:    null, // X  X  X  X  M  X
      scts:      null, // X  X  X  M  M  X
      dt:        null, // X  X  X  X  M  X
    };

    // SMSC info
    let smscLength = this.readHexOctet();
    if (smscLength > 0) {
      let smscTypeOfAddress = this.readHexOctet();
      // Subtract the type-of-address octet we just read from the length.
      msg.SMSC = this.readSwappedNibbleBcdString(smscLength - 1);
      if ((smscTypeOfAddress >> 4) == (PDU_TOA_INTERNATIONAL >> 4)) {
        msg.SMSC = '+' + msg.SMSC;
      }
    }

    // First octet of this SMS-DELIVER or SMS-SUBMIT message
    let firstOctet = this.readHexOctet();
    // Message Type Indicator
    msg.mti = firstOctet & 0x03;
    // User data header indicator
    msg.udhi = firstOctet & PDU_UDHI;

    switch (msg.mti) {
      case PDU_MTI_SMS_RESERVED:
        // `If an MS receives a TPDU with a "Reserved" value in the TP-MTI it
        // shall process the message as if it were an "SMS-DELIVER" but store
        // the message exactly as received.` ~ 3GPP TS 23.040 9.2.3.1
      case PDU_MTI_SMS_DELIVER:
        return this.readDeliverMessage(msg);
      case PDU_MTI_SMS_STATUS_REPORT:
        return this.readStatusReportMessage(msg);
      default:
        return null;
    }
  },

  /**
   * Helper for processing received SMS parcel data.
   *
   * @param length
   *        Length of SMS string in the incoming parcel.
   *
   * @return Message parsed or null for invalid message.
   */
  processReceivedSms: function processReceivedSms(length) {
    if (!length) {
      if (DEBUG) debug("Received empty SMS!");
      return [null, PDU_FCS_UNSPECIFIED];
    }

    // An SMS is a string, but we won't read it as such, so let's read the
    // string length and then defer to PDU parsing helper.
    let messageStringLength = Buf.readInt32();
    if (DEBUG) debug("Got new SMS, length " + messageStringLength);
    let message = this.readMessage();
    if (DEBUG) debug("Got new SMS: " + JSON.stringify(message));

    // Read string delimiters. See Buf.readString().
    Buf.readStringDelimiter(length);

    // Determine result
    if (!message) {
      return [null, PDU_FCS_UNSPECIFIED];
    }

    if (message.epid == PDU_PID_SHORT_MESSAGE_TYPE_0) {
      // `A short message type 0 indicates that the ME must acknowledge receipt
      // of the short message but shall discard its contents.` ~ 3GPP TS 23.040
      // 9.2.3.9
      return [null, PDU_FCS_OK];
    }

    if (message.messageClass == GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_2]) {
      switch (message.epid) {
        case PDU_PID_ANSI_136_R_DATA:
        case PDU_PID_USIM_DATA_DOWNLOAD:
          if (ICCUtilsHelper.isICCServiceAvailable("DATA_DOWNLOAD_SMS_PP")) {
            // `If the service "data download via SMS Point-to-Point" is
            // allocated and activated in the (U)SIM Service Table, ... then the
            // ME shall pass the message transparently to the UICC using the
            // ENVELOPE (SMS-PP DOWNLOAD).` ~ 3GPP TS 31.111 7.1.1.1
            RIL.dataDownloadViaSMSPP(message);

            // `the ME shall not display the message, or alert the user of a
            // short message waiting.` ~ 3GPP TS 31.111 7.1.1.1
            return [null, PDU_FCS_RESERVED];
          }

          // If the service "data download via SMS-PP" is not available in the
          // (U)SIM Service Table, ..., then the ME shall store the message in
          // EFsms in accordance with TS 31.102` ~ 3GPP TS 31.111 7.1.1.1

          // Fall through.
        default:
          RIL.writeSmsToSIM(message);
          break;
      }
    }

    // TODO: Bug 739143: B2G SMS: Support SMS Storage Full event
    if ((message.messageClass != GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_0]) && !true) {
      // `When a mobile terminated message is class 0..., the MS shall display
      // the message immediately and send a ACK to the SC ..., irrespective of
      // whether there is memory available in the (U)SIM or ME.` ~ 3GPP 23.038
      // clause 4.

      if (message.messageClass == GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_2]) {
        // `If all the short message storage at the MS is already in use, the
        // MS shall return "memory capacity exceeded".` ~ 3GPP 23.038 clause 4.
        return [null, PDU_FCS_MEMORY_CAPACITY_EXCEEDED];
      }

      return [null, PDU_FCS_UNSPECIFIED];
    }

    return [message, PDU_FCS_OK];
  },

  /**
   * Read and decode a SMS-DELIVER PDU.
   *
   * @param msg
   *        message object for output.
   */
  readDeliverMessage: function readDeliverMessage(msg) {
    // - Sender Address info -
    let senderAddressLength = this.readHexOctet();
    msg.sender = this.readAddress(senderAddressLength);
    // - TP-Protocolo-Identifier -
    this.readProtocolIndicator(msg);
    // - TP-Data-Coding-Scheme -
    this.readDataCodingScheme(msg);
    // - TP-Service-Center-Time-Stamp -
    msg.timestamp = this.readTimestamp();
    // - TP-User-Data-Length -
    let userDataLength = this.readHexOctet();

    // - TP-User-Data -
    if (userDataLength > 0) {
      this.readUserData(msg, userDataLength);
    }

    return msg;
  },

  /**
   * Read and decode a SMS-STATUS-REPORT PDU.
   *
   * @param msg
   *        message object for output.
   */
  readStatusReportMessage: function readStatusReportMessage(msg) {
    // TP-Message-Reference
    msg.messageRef = this.readHexOctet();
    // TP-Recipient-Address
    let recipientAddressLength = this.readHexOctet();
    msg.recipient = this.readAddress(recipientAddressLength);
    // TP-Service-Centre-Time-Stamp
    msg.scts = this.readTimestamp();
    // TP-Discharge-Time
    msg.dt = this.readTimestamp();
    // TP-Status
    msg.status = this.readHexOctet();

    this.readExtraParams(msg);

    return msg;
  },

  /**
   * Serialize a SMS-SUBMIT PDU message and write it to the output stream.
   *
   * This method expects that a data coding scheme has been chosen already
   * and that the length of the user data payload in that encoding is known,
   * too. Both go hand in hand together anyway.
   *
   * @param address
   *        String containing the address (number) of the SMS receiver
   * @param userData
   *        String containing the message to be sent as user data
   * @param dcs
   *        Data coding scheme. One of the PDU_DCS_MSG_CODING_*BITS_ALPHABET
   *        constants.
   * @param userDataHeaderLength
   *        Length of embedded user data header, in bytes. The whole header
   *        size will be userDataHeaderLength + 1; 0 for no header.
   * @param encodedBodyLength
   *        Length of the user data when encoded with the given DCS. For UCS2,
   *        in bytes; for 7-bit, in septets.
   * @param langIndex
   *        Table index used for normal 7-bit encoded character lookup.
   * @param langShiftIndex
   *        Table index used for escaped 7-bit encoded character lookup.
   * @param requestStatusReport
   *        Request status report.
   */
  writeMessage: function writeMessage(options) {
    if (DEBUG) {
      debug("writeMessage: " + JSON.stringify(options));
    }
    let address = options.number;
    let body = options.body;
    let dcs = options.dcs;
    let userDataHeaderLength = options.userDataHeaderLength;
    let encodedBodyLength = options.encodedBodyLength;
    let langIndex = options.langIndex;
    let langShiftIndex = options.langShiftIndex;

    // SMS-SUBMIT Format:
    //
    // PDU Type - 1 octet
    // Message Reference - 1 octet
    // DA - Destination Address - 2 to 12 octets
    // PID - Protocol Identifier - 1 octet
    // DCS - Data Coding Scheme - 1 octet
    // VP - Validity Period - 0, 1 or 7 octets
    // UDL - User Data Length - 1 octet
    // UD - User Data - 140 octets

    let addressFormat = PDU_TOA_ISDN; // 81
    if (address[0] == '+') {
      addressFormat = PDU_TOA_INTERNATIONAL | PDU_TOA_ISDN; // 91
      address = address.substring(1);
    }
    //TODO validity is unsupported for now
    let validity = 0;

    let headerOctets = (userDataHeaderLength ? userDataHeaderLength + 1 : 0);
    let paddingBits;
    let userDataLengthInSeptets;
    let userDataLengthInOctets;
    if (dcs == PDU_DCS_MSG_CODING_7BITS_ALPHABET) {
      let headerSeptets = Math.ceil(headerOctets * 8 / 7);
      userDataLengthInSeptets = headerSeptets + encodedBodyLength;
      userDataLengthInOctets = Math.ceil(userDataLengthInSeptets * 7 / 8);
      paddingBits = headerSeptets * 7 - headerOctets * 8;
    } else {
      userDataLengthInOctets = headerOctets + encodedBodyLength;
      paddingBits = 0;
    }

    let pduOctetLength = 4 + // PDU Type, Message Ref, address length + format
                         Math.ceil(address.length / 2) +
                         3 + // PID, DCS, UDL
                         userDataLengthInOctets;
    if (validity) {
      //TODO: add more to pduOctetLength
    }

    // Start the string. Since octets are represented in hex, we will need
    // twice as many characters as octets.
    Buf.writeInt32(pduOctetLength * 2);

    // - PDU-TYPE-

    // +--------+----------+---------+---------+--------+---------+
    // | RP (1) | UDHI (1) | SRR (1) | VPF (2) | RD (1) | MTI (2) |
    // +--------+----------+---------+---------+--------+---------+
    // RP:    0   Reply path parameter is not set
    //        1   Reply path parameter is set
    // UDHI:  0   The UD Field contains only the short message
    //        1   The beginning of the UD field contains a header in addition
    //            of the short message
    // SRR:   0   A status report is not requested
    //        1   A status report is requested
    // VPF:   bit4  bit3
    //        0     0     VP field is not present
    //        0     1     Reserved
    //        1     0     VP field present an integer represented (relative)
    //        1     1     VP field present a semi-octet represented (absolute)
    // RD:        Instruct the SMSC to accept(0) or reject(1) an SMS-SUBMIT
    //            for a short message still held in the SMSC which has the same
    //            MR and DA as a previously submitted short message from the
    //            same OA
    // MTI:   bit1  bit0    Message Type
    //        0     0       SMS-DELIVER (SMSC ==> MS)
    //        0     1       SMS-SUBMIT (MS ==> SMSC)

    // PDU type. MTI is set to SMS-SUBMIT
    let firstOctet = PDU_MTI_SMS_SUBMIT;

    // Status-Report-Request
    if (options.requestStatusReport) {
      firstOctet |= PDU_SRI_SRR;
    }

    // Validity period
    if (validity) {
      //TODO: not supported yet, OR with one of PDU_VPF_*
    }
    // User data header indicator
    if (headerOctets) {
      firstOctet |= PDU_UDHI;
    }
    this.writeHexOctet(firstOctet);

    // Message reference 00
    this.writeHexOctet(0x00);

    // - Destination Address -
    this.writeHexOctet(address.length);
    this.writeHexOctet(addressFormat);
    this.writeSwappedNibbleBCD(address);

    // - Protocol Identifier -
    this.writeHexOctet(0x00);

    // - Data coding scheme -
    // For now it assumes bits 7..4 = 1111 except for the 16 bits use case
    this.writeHexOctet(dcs);

    // - Validity Period -
    if (validity) {
      this.writeHexOctet(validity);
    }

    // - User Data -
    if (dcs == PDU_DCS_MSG_CODING_7BITS_ALPHABET) {
      this.writeHexOctet(userDataLengthInSeptets);
    } else {
      this.writeHexOctet(userDataLengthInOctets);
    }

    if (headerOctets) {
      this.writeUserDataHeader(options);
    }

    switch (dcs) {
      case PDU_DCS_MSG_CODING_7BITS_ALPHABET:
        this.writeStringAsSeptets(body, paddingBits, langIndex, langShiftIndex);
        break;
      case PDU_DCS_MSG_CODING_8BITS_ALPHABET:
        // Unsupported.
        break;
      case PDU_DCS_MSG_CODING_16BITS_ALPHABET:
        this.writeUCS2String(body);
        break;
    }

    // End of the string. The string length is always even by definition, so
    // we write two \0 delimiters.
    Buf.writeUint16(0);
    Buf.writeUint16(0);
  },

  /**
   * Read GSM CBS message serial number.
   *
   * @param msg
   *        message object for output.
   *
   * @see 3GPP TS 23.041 section 9.4.1.2.1
   */
  readCbSerialNumber: function readCbSerialNumber(msg) {
    msg.serial = Buf.readUint8() << 8 | Buf.readUint8();
    msg.geographicalScope = (msg.serial >>> 14) & 0x03;
    msg.messageCode = (msg.serial >>> 4) & 0x03FF;
    msg.updateNumber = msg.serial & 0x0F;
  },

  /**
   * Read GSM CBS message message identifier.
   *
   * @param msg
   *        message object for output.
   *
   * @see 3GPP TS 23.041 section 9.4.1.2.2
   */
  readCbMessageIdentifier: function readCbMessageIdentifier(msg) {
    msg.messageId = Buf.readUint8() << 8 | Buf.readUint8();

    if ((msg.format != CB_FORMAT_ETWS)
        && (msg.messageId >= CB_GSM_MESSAGEID_ETWS_BEGIN)
        && (msg.messageId <= CB_GSM_MESSAGEID_ETWS_END)) {
      // `In the case of transmitting CBS message for ETWS, a part of
      // Message Code can be used to command mobile terminals to activate
      // emergency user alert and message popup in order to alert the users.`
      msg.etws = {
        emergencyUserAlert: msg.messageCode & 0x0200 ? true : false,
        popup:              msg.messageCode & 0x0100 ? true : false
      };

      let warningType = msg.messageId - CB_GSM_MESSAGEID_ETWS_BEGIN;
      if (warningType < CB_ETWS_WARNING_TYPE_NAMES.length) {
        msg.etws.warningType = warningType;
      }
    }
  },

  /**
   * Read CBS Data Coding Scheme.
   *
   * @param msg
   *        message object for output.
   *
   * @see 3GPP TS 23.038 section 5.
   */
  readCbDataCodingScheme: function readCbDataCodingScheme(msg) {
    let dcs = Buf.readUint8();
    if (DEBUG) debug("PDU: read CBS dcs: " + dcs);

    let language = null, hasLanguageIndicator = false;
    // `Any reserved codings shall be assumed to be the GSM 7bit default
    // alphabet.`
    let encoding = PDU_DCS_MSG_CODING_7BITS_ALPHABET;
    let messageClass = PDU_DCS_MSG_CLASS_NORMAL;

    switch (dcs & PDU_DCS_CODING_GROUP_BITS) {
      case 0x00: // 0000
        language = CB_DCS_LANG_GROUP_1[dcs & 0x0F];
        break;

      case 0x10: // 0001
        switch (dcs & 0x0F) {
          case 0x00:
            hasLanguageIndicator = true;
            break;
          case 0x01:
            encoding = PDU_DCS_MSG_CODING_16BITS_ALPHABET;
            hasLanguageIndicator = true;
            break;
        }
        break;

      case 0x20: // 0010
        language = CB_DCS_LANG_GROUP_2[dcs & 0x0F];
        break;

      case 0x40: // 01xx
      case 0x50:
      //case 0x60: Text Compression, not supported
      //case 0x70: Text Compression, not supported
      case 0x90: // 1001
        encoding = (dcs & 0x0C);
        if (encoding == 0x0C) {
          encoding = PDU_DCS_MSG_CODING_7BITS_ALPHABET;
        }
        messageClass = (dcs & PDU_DCS_MSG_CLASS_BITS);
        break;

      case 0xF0:
        encoding = (dcs & 0x04) ? PDU_DCS_MSG_CODING_8BITS_ALPHABET
                                : PDU_DCS_MSG_CODING_7BITS_ALPHABET;
        switch(dcs & PDU_DCS_MSG_CLASS_BITS) {
          case 0x01: messageClass = PDU_DCS_MSG_CLASS_USER_1; break;
          case 0x02: messageClass = PDU_DCS_MSG_CLASS_USER_2; break;
          case 0x03: messageClass = PDU_DCS_MSG_CLASS_3; break;
        }
        break;

      case 0x30: // 0011 (Reserved)
      case 0x80: // 1000 (Reserved)
      case 0xA0: // 1010..1100 (Reserved)
      case 0xB0:
      case 0xC0:
        break;

      default:
        throw new Error("Unsupported CBS data coding scheme: " + dcs);
    }

    msg.dcs = dcs;
    msg.encoding = encoding;
    msg.language = language;
    msg.messageClass = GECKO_SMS_MESSAGE_CLASSES[messageClass];
    msg.hasLanguageIndicator = hasLanguageIndicator;
  },

  /**
   * Read GSM CBS message page parameter.
   *
   * @param msg
   *        message object for output.
   *
   * @see 3GPP TS 23.041 section 9.4.1.2.4
   */
  readCbPageParameter: function readCbPageParameter(msg) {
    let octet = Buf.readUint8();
    msg.pageIndex = (octet >>> 4) & 0x0F;
    msg.numPages = octet & 0x0F;
    if (!msg.pageIndex || !msg.numPages) {
      // `If a mobile receives the code 0000 in either the first field or the
      // second field then it shall treat the CBS message exactly the same as a
      // CBS message with page parameter 0001 0001 (i.e. a single page message).`
      msg.pageIndex = msg.numPages = 1;
    }
  },

  /**
   * Read ETWS Primary Notification message warning type.
   *
   * @param msg
   *        message object for output.
   *
   * @see 3GPP TS 23.041 section 9.3.24
   */
  readCbWarningType: function readCbWarningType(msg) {
    let word = Buf.readUint8() << 8 | Buf.readUint8();
    msg.etws = {
      warningType:        (word >>> 9) & 0x7F,
      popup:              word & 0x80 ? true : false,
      emergencyUserAlert: word & 0x100 ? true : false
    };
  },

  /**
   * Read CBS-Message-Information-Page
   *
   * @param msg
   *        message object for output.
   * @param length
   *        length of cell broadcast data to read in octets.
   *
   * @see 3GPP TS 23.041 section 9.3.19
   */
  readGsmCbData: function readGsmCbData(msg, length) {
    let bufAdapter = {
      readHexOctet: function readHexOctet() {
        return Buf.readUint8();
      }
    };

    msg.body = null;
    msg.data = null;
    switch (msg.encoding) {
      case PDU_DCS_MSG_CODING_7BITS_ALPHABET:
        msg.body = this.readSeptetsToString.call(bufAdapter,
                                                 (length * 8 / 7), 0,
                                                 PDU_NL_IDENTIFIER_DEFAULT,
                                                 PDU_NL_IDENTIFIER_DEFAULT);
        if (msg.hasLanguageIndicator) {
          msg.language = msg.body.substring(0, 2);
          msg.body = msg.body.substring(3);
        }
        break;

      case PDU_DCS_MSG_CODING_8BITS_ALPHABET:
        msg.data = Buf.readUint8Array(length);
        break;

      case PDU_DCS_MSG_CODING_16BITS_ALPHABET:
        if (msg.hasLanguageIndicator) {
          msg.language = this.readSeptetsToString.call(bufAdapter, 2, 0,
                                                       PDU_NL_IDENTIFIER_DEFAULT,
                                                       PDU_NL_IDENTIFIER_DEFAULT);
          length -= 2;
        }
        msg.body = this.readUCS2String.call(bufAdapter, length);
        break;
    }
  },

  /**
   * Read Cell GSM/ETWS/UMTS Broadcast Message.
   *
   * @param pduLength
   *        total length of the incoming PDU in octets.
   */
  readCbMessage: function readCbMessage(pduLength) {
    // Validity                                                   GSM ETWS UMTS
    let msg = {
      // Internally used in ril_worker:
      serial:               null,                              //  O   O    O
      updateNumber:         null,                              //  O   O    O
      format:               null,                              //  O   O    O
      dcs:                  0x0F,                              //  O   X    O
      encoding:             PDU_DCS_MSG_CODING_7BITS_ALPHABET, //  O   X    O
      hasLanguageIndicator: false,                             //  O   X    O
      data:                 null,                              //  O   X    O
      body:                 null,                              //  O   X    O
      pageIndex:            1,                                 //  O   X    X
      numPages:             1,                                 //  O   X    X

      // DOM attributes:
      geographicalScope:    null,                              //  O   O    O
      messageCode:          null,                              //  O   O    O
      messageId:            null,                              //  O   O    O
      language:             null,                              //  O   X    O
      fullBody:             null,                              //  O   X    O
      fullData:             null,                              //  O   X    O
      messageClass:         GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL], //  O   x    O
      etws:                 null                               //  ?   O    ?
      /*{
        warningType:        null,                              //  X   O    X
        popup:              false,                             //  X   O    X
        emergencyUserAlert: false,                             //  X   O    X
      }*/
    };

    if (pduLength <= CB_MESSAGE_SIZE_ETWS) {
      msg.format = CB_FORMAT_ETWS;
      return this.readEtwsCbMessage(msg);
    }

    if (pduLength <= CB_MESSAGE_SIZE_GSM) {
      msg.format = CB_FORMAT_GSM;
      return this.readGsmCbMessage(msg, pduLength);
    }

    return null;
  },

  /**
   * Read GSM Cell Broadcast Message.
   *
   * @param msg
   *        message object for output.
   * @param pduLength
   *        total length of the incomint PDU in octets.
   *
   * @see 3GPP TS 23.041 clause 9.4.1.2
   */
  readGsmCbMessage: function readGsmCbMessage(msg, pduLength) {
    this.readCbSerialNumber(msg);
    this.readCbMessageIdentifier(msg);
    this.readCbDataCodingScheme(msg);
    this.readCbPageParameter(msg);

    // GSM CB message header takes 6 octets.
    this.readGsmCbData(msg, pduLength - 6);

    return msg;
  },

  /**
   * Read ETWS Primary Notification Message.
   *
   * @param msg
   *        message object for output.
   *
   * @see 3GPP TS 23.041 clause 9.4.1.3
   */
  readEtwsCbMessage: function readEtwsCbMessage(msg) {
    this.readCbSerialNumber(msg);
    this.readCbMessageIdentifier(msg);
    this.readCbWarningType(msg);

    // Octet 7..56 is Warning Security Information. However, according to
    // section 9.4.1.3.6, `The UE shall ignore this parameter.` So we just skip
    // processing it here.

    return msg;
  },

  /**
   * Read network name.
   *
   * @param len Length of the information element.
   * @return
   *   {
   *     networkName: network name.
   *     shouldIncludeCi: Should Country's initials included in text string.
   *   }
   * @see TS 24.008 clause 10.5.3.5a.
   */
  readNetworkName: function readNetworkName(len) {
    // According to TS 24.008 Sec. 10.5.3.5a, the first octet is:
    // bit 8: must be 1.
    // bit 5-7: Text encoding.
    //          000 - GSM default alphabet.
    //          001 - UCS2 (16 bit).
    //          else - reserved.
    // bit 4: MS should add the letters for Country's Initials and a space
    //        to the text string if this bit is true.
    // bit 1-3: number of spare bits in last octet.

    let codingInfo = GsmPDUHelper.readHexOctet();
    if (!(codingInfo & 0x80)) {
      return null;
    }

    let textEncoding = (codingInfo & 0x70) >> 4;
    let shouldIncludeCountryInitials = !!(codingInfo & 0x08);
    let spareBits = codingInfo & 0x07;
    let resultString;

    switch (textEncoding) {
    case 0:
      // GSM Default alphabet.
      resultString = GsmPDUHelper.readSeptetsToString(
        ((len - 1) * 8 - spareBits) / 7, 0,
        PDU_NL_IDENTIFIER_DEFAULT,
        PDU_NL_IDENTIFIER_DEFAULT);
      break;
    case 1:
      // UCS2 encoded.
      resultString = this.readUCS2String(len - 1);
      break;
    default:
      // Not an available text coding.
      return null;
    }

    // TODO - Bug 820286: According to shouldIncludeCountryInitials, add
    // country initials to the resulting string.
    return resultString;
  }
};

/**
 * Provide buffer with bitwise read/write function so make encoding/decoding easier.
 */
let BitBufferHelper = {
  readCache: 0,
  readCacheSize: 0,
  readBuffer: [],
  readIndex: 0,
  writeCache: 0,
  writeCacheSize: 0,
  writeBuffer: [],

  // Max length is 32 because we use integer as read/write cache.
  // All read/write functions are implemented based on bitwise operation.
  readBits: function readBits(length) {
    if (length <= 0 || length > 32) {
      return null;
    }

    if (length > this.readCacheSize) {
      let bytesToRead = Math.ceil((length - this.readCacheSize) / 8);
      for(let i = 0; i < bytesToRead; i++) {
        this.readCache = (this.readCache << 8) | (this.readBuffer[this.readIndex++] & 0xFF);
        this.readCacheSize += 8;
      }
    }

    let bitOffset = (this.readCacheSize - length),
        resultMask = (1 << length) - 1,
        result = 0;

    result = (this.readCache >> bitOffset) & resultMask;
    this.readCacheSize -= length;

    return result;
  },

  writeBits: function writeBits(value, length) {
    if (length <= 0 || length > 32) {
      return;
    }

    let totalLength = length + this.writeCacheSize;

    // 8-byte cache not full
    if (totalLength < 8) {
      let valueMask = (1 << length) - 1;
      this.writeCache = (this.writeCache << length) | (value & valueMask);
      this.writeCacheSize += length;
      return;
    }

    // Deal with unaligned part
    if (this.writeCacheSize) {
      let mergeLength = 8 - this.writeCacheSize,
          valueMask = (1 << mergeLength) - 1;

      this.writeCache = (this.writeCache << mergeLength) | ((value >> (length - mergeLength)) & valueMask);
      this.writeBuffer.push(this.writeCache & 0xFF);
      length -= mergeLength;
    }

    // Aligned part, just copy
    while (length >= 8) {
      length -= 8;
      this.writeBuffer.push((value >> length) & 0xFF);
    }

    // Rest part is saved into cache
    this.writeCacheSize = length;
    this.writeCache = value & ((1 << length) - 1);

    return;
  },

  // Drop what still in read cache and goto next 8-byte alignment.
  // There might be a better naming.
  nextOctetAlign: function nextOctetAlign() {
    this.readCache = 0;
    this.readCacheSize = 0;
  },

  // Flush current write cache to Buf with padding 0s.
  // There might be a better naming.
  flushWithPadding: function flushWithPadding() {
    if (this.writeCacheSize) {
      this.writeBuffer.push(this.writeCache << (8 - this.writeCacheSize));
    }
    this.writeCache = 0;
    this.writeCacheSize = 0;
  },

  startWrite: function startWrite(dataBuffer) {
    this.writeBuffer = dataBuffer;
    this.writeCache = 0;
    this.writeCacheSize = 0;
  },

  startRead: function startRead(dataBuffer) {
    this.readBuffer = dataBuffer;
    this.readCache = 0;
    this.readCacheSize = 0;
    this.readIndex = 0;
  },

  getWriteBufferSize: function getWriteBufferSize() {
    return this.writeBuffer.length;
  },

  overwriteWriteBuffer: function overwriteWriteBuffer(position, data) {
    let writeLength = data.length;
    if (writeLength + position >= this.writeBuffer.length) {
      writeLength = this.writeBuffer.length - position;
    }
    for (let i = 0; i < writeLength; i++) {
      this.writeBuffer[i + position] = data[i];
    }
  }
};

/**
 * Helper for CDMA PDU
 *
 * Currently, some function are shared with GsmPDUHelper, they should be
 * moved from GsmPDUHelper to a common object shared among GsmPDUHelper and
 * CdmaPDUHelper.
 */
let CdmaPDUHelper = {
  //       1..........C
  // Only "1234567890*#" is defined in C.S0005-D v2.0
  dtmfChars: ".1234567890*#...",

  /**
   * Entry point for SMS encoding, the options object is made compatible
   * with existing writeMessage() of GsmPDUHelper, but less key is used.
   *
   * Current used key in options:
   * @param number
   *        String containing the address (number) of the SMS receiver
   * @param body
   *        String containing the message to be sent, segmented part
   * @param dcs
   *        Data coding scheme. One of the PDU_DCS_MSG_CODING_*BITS_ALPHABET
   *        constants.
   * @param encodedBodyLength
   *        Length of the user data when encoded with the given DCS. For UCS2,
   *        in bytes; for 7-bit, in septets.
   * @param requestStatusReport
   *        Request status report.
   * @param segmentRef
   *        Reference number of concatenated SMS message
   * @param segmentMaxSeq
   *        Total number of concatenated SMS message
   * @param segmentSeq
   *        Sequence number of concatenated SMS message
   */
  writeMessage: function cdma_writeMessage(options) {
    if (DEBUG) {
      debug("cdma_writeMessage: " + JSON.stringify(options));
    }

    // Get encoding
    options.encoding = this.gsmDcsToCdmaEncoding(options.dcs);

    // Common Header
    if (options.segmentMaxSeq > 1) {
      this.writeInt(PDU_CDMA_MSG_TELESERIVCIE_ID_WEMT);
    } else {
      this.writeInt(PDU_CDMA_MSG_TELESERIVCIE_ID_SMS);
    }

    this.writeInt(0);
    this.writeInt(PDU_CDMA_MSG_CATEGORY_UNSPEC);

    // Just fill out address info in byte, rild will encap them for us
    let addrInfo = this.encodeAddr(options.number);
    this.writeByte(addrInfo.digitMode);
    this.writeByte(addrInfo.numberMode);
    this.writeByte(addrInfo.numberType);
    this.writeByte(addrInfo.numberPlan);
    this.writeByte(addrInfo.address.length);
    for (let i = 0; i < addrInfo.address.length; i++) {
      this.writeByte(addrInfo.address[i]);
    }

    // Subaddress, not supported
    this.writeByte(0);  // Subaddress : Type
    this.writeByte(0);  // Subaddress : Odd
    this.writeByte(0);  // Subaddress : length

    // User Data
    let encodeResult = this.encodeUserData(options);
    this.writeByte(encodeResult.length);
    for (let i = 0; i < encodeResult.length; i++) {
      this.writeByte(encodeResult[i]);
    }

    encodeResult = null;
  },

  /**
   * Data writters
   */
  writeInt: function writeInt(value) {
    Buf.writeInt32(value);
  },

  writeByte: function writeByte(value) {
    Buf.writeInt32(value & 0xFF);
  },

  /**
   * Transform GSM DCS to CDMA encoding.
   */
  gsmDcsToCdmaEncoding: function gsmDcsToCdmaEncoding(encoding) {
    switch (encoding) {
      case PDU_DCS_MSG_CODING_7BITS_ALPHABET:
        return PDU_CDMA_MSG_CODING_7BITS_ASCII;
      case PDU_DCS_MSG_CODING_8BITS_ALPHABET:
        return PDU_CDMA_MSG_CODING_OCTET;
      case PDU_DCS_MSG_CODING_16BITS_ALPHABET:
        return PDU_CDMA_MSG_CODING_UNICODE;
    }
    throw new Error("gsmDcsToCdmaEncoding(): Invalid GSM SMS DCS value: " + encoding);
  },

  /**
   * Encode address into CDMA address format, as a byte array.
   *
   * @see 3GGP2 C.S0015-B 2.0, 3.4.3.3 Address Parameters
   *
   * @param address
   *        String of address to be encoded
   */
  encodeAddr: function cdma_encodeAddr(address) {
    let result = {};

    result.numberType = PDU_CDMA_MSG_ADDR_NUMBER_TYPE_UNKNOWN;
    result.numberPlan = PDU_CDMA_MSG_ADDR_NUMBER_TYPE_UNKNOWN;

    if (address[0] === '+') {
      address = address.substring(1);
    }

    // Try encode with DTMF first
    result.digitMode = PDU_CDMA_MSG_ADDR_DIGIT_MODE_DTMF;
    result.numberMode = PDU_CDMA_MSG_ADDR_NUMBER_MODE_ANSI;

    result.address = [];
    for (let i = 0; i < address.length; i++) {
      let addrDigit = this.dtmfChars.indexOf(address.charAt(i));
      if (addrDigit < 0) {
        result.digitMode = PDU_CDMA_MSG_ADDR_DIGIT_MODE_ASCII;
        result.numberMode = PDU_CDMA_MSG_ADDR_NUMBER_MODE_ASCII;
        result.address = [];
        break;
      }
      result.address.push(addrDigit);
    }

    // Address can't be encoded with DTMF, then use 7-bit ASCII
    if (result.digitMode !== PDU_CDMA_MSG_ADDR_DIGIT_MODE_DTMF) {
      if (address.indexOf("@") !== -1) {
        result.numberType = PDU_CDMA_MSG_ADDR_NUMBER_TYPE_NATIONAL;
      }

      for (let i = 0; i < address.length; i++) {
        result.address.push(address.charCodeAt(i) & 0x7F);
      }
    }

    return result;
  },

  /**
   * Encode SMS contents in options into CDMA userData field.
   * Corresponding and required subparameters will be added automatically.
   *
   * @see 3GGP2 C.S0015-B 2.0, 3.4.3.7 Bearer Data
   *                           4.5 Bearer Data Parameters
   *
   * Current used key in options:
   * @param body
   *        String containing the message to be sent, segmented part
   * @param encoding
   *        Encoding method of CDMA, can be transformed from GSM DCS by function
   *        cdmaPduHelp.gsmDcsToCdmaEncoding()
   * @param encodedBodyLength
   *        Length of the user data when encoded with the given DCS. For UCS2,
   *        in bytes; for 7-bit, in septets.
   * @param requestStatusReport
   *        Request status report.
   * @param segmentRef
   *        Reference number of concatenated SMS message
   * @param segmentMaxSeq
   *        Total number of concatenated SMS message
   * @param segmentSeq
   *        Sequence number of concatenated SMS message
   */
  encodeUserData: function cdma_encodeUserData(options) {
    let userDataBuffer = [];
    BitBufferHelper.startWrite(userDataBuffer);

    // Message Identifier
    this.encodeUserDataMsgId(options);

    // User Data
    this.encodeUserDataMsg(options);

    // Reply Option
    this.encodeUserDataReplyOption(options);

    return userDataBuffer;
  },

  /**
   * User data subparameter encoder : Message Identifier
   *
   * @see 3GGP2 C.S0015-B 2.0, 4.5.1 Message Identifier
   */
  encodeUserDataMsgId: function cdma_encodeUserDataMsgId(options) {
    BitBufferHelper.writeBits(PDU_CDMA_MSG_USERDATA_MSG_ID, 8);
    BitBufferHelper.writeBits(3, 8);
    BitBufferHelper.writeBits(PDU_CDMA_MSG_TYPE_SUBMIT, 4);
    BitBufferHelper.writeBits(1, 16); // TODO: How to get message ID?
    if (options.segmentMaxSeq > 1) {
      BitBufferHelper.writeBits(1, 1);
    } else {
      BitBufferHelper.writeBits(0, 1);
    }

    BitBufferHelper.flushWithPadding();
  },

  /**
   * User data subparameter encoder : User Data
   *
   * @see 3GGP2 C.S0015-B 2.0, 4.5.2 User Data
   */
  encodeUserDataMsg: function cdma_encodeUserDataMsg(options) {
    BitBufferHelper.writeBits(PDU_CDMA_MSG_USERDATA_BODY, 8);
    // Reserve space for length
    BitBufferHelper.writeBits(0, 8);
    let lengthPosition = BitBufferHelper.getWriteBufferSize();

    BitBufferHelper.writeBits(options.encoding, 5);

    // Add user data header for message segement
    let msgBody = options.body,
        msgBodySize = (options.encoding === PDU_CDMA_MSG_CODING_7BITS_ASCII ?
                       options.encodedBodyLength :
                       msgBody.length);
    if (options.segmentMaxSeq > 1) {
      if (options.encoding === PDU_CDMA_MSG_CODING_7BITS_ASCII) {
          BitBufferHelper.writeBits(msgBodySize + 7, 8); // Required length for user data header, in septet(7-bit)

          BitBufferHelper.writeBits(5, 8);  // total header length 5 bytes
          BitBufferHelper.writeBits(PDU_IEI_CONCATENATED_SHORT_MESSAGES_8BIT, 8);  // header id 0
          BitBufferHelper.writeBits(3, 8);  // length of element for id 0 is 3
          BitBufferHelper.writeBits(options.segmentRef & 0xFF, 8);      // Segement reference
          BitBufferHelper.writeBits(options.segmentMaxSeq & 0xFF, 8);   // Max segment
          BitBufferHelper.writeBits(options.segmentSeq & 0xFF, 8);      // Current segment
          BitBufferHelper.writeBits(0, 1);  // Padding to make header data septet(7-bit) aligned
        } else {
          if (options.encoding === PDU_CDMA_MSG_CODING_UNICODE) {
            BitBufferHelper.writeBits(msgBodySize + 3, 8); // Required length for user data header, in 16-bit
          } else {
            BitBufferHelper.writeBits(msgBodySize + 6, 8); // Required length for user data header, in octet(8-bit)
          }

          BitBufferHelper.writeBits(5, 8);  // total header length 5 bytes
          BitBufferHelper.writeBits(PDU_IEI_CONCATENATED_SHORT_MESSAGES_8BIT, 8);  // header id 0
          BitBufferHelper.writeBits(3, 8);  // length of element for id 0 is 3
          BitBufferHelper.writeBits(options.segmentRef & 0xFF, 8);      // Segement reference
          BitBufferHelper.writeBits(options.segmentMaxSeq & 0xFF, 8);   // Max segment
          BitBufferHelper.writeBits(options.segmentSeq & 0xFF, 8);      // Current segment
        }
    } else {
      BitBufferHelper.writeBits(msgBodySize, 8);
    }

    // Encode message based on encoding method
    const langTable = PDU_NL_LOCKING_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];
    const langShiftTable = PDU_NL_SINGLE_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];
    for (let i = 0; i < msgBody.length; i++) {
      switch (options.encoding) {
        case PDU_CDMA_MSG_CODING_OCTET: {
          let msgDigit = msgBody.charCodeAt(i);
          BitBufferHelper.writeBits(msgDigit, 8);
          break;
        }
        case PDU_CDMA_MSG_CODING_7BITS_ASCII: {
          let msgDigit = msgBody.charCodeAt(i),
              msgDigitChar = msgBody.charAt(i);

          if (msgDigit >= 32) {
            BitBufferHelper.writeBits(msgDigit, 7);
          } else {
            msgDigit = langTable.indexOf(msgDigitChar);

            if (msgDigit === PDU_NL_EXTENDED_ESCAPE) {
              break;
            }
            if (msgDigit >= 0) {
              BitBufferHelper.writeBits(msgDigit, 7);
            } else {
              msgDigit = langShiftTable.indexOf(msgDigitChar);
              if (msgDigit == -1) {
                throw new Error("'" + msgDigitChar + "' is not in 7 bit alphabet "
                                + langIndex + ":" + langShiftIndex + "!");
              }

              if (msgDigit === PDU_NL_RESERVED_CONTROL) {
                break;
              }

              BitBufferHelper.writeBits(PDU_NL_EXTENDED_ESCAPE, 7);
              BitBufferHelper.writeBits(msgDigit, 7);
            }
          }
          break;
        }
        case PDU_CDMA_MSG_CODING_UNICODE: {
          let msgDigit = msgBody.charCodeAt(i);
          BitBufferHelper.writeBits(msgDigit, 16);
          break;
        }
      }
    }
    BitBufferHelper.flushWithPadding();

    // Fill length
    let currentPosition = BitBufferHelper.getWriteBufferSize();
    BitBufferHelper.overwriteWriteBuffer(lengthPosition - 1, [currentPosition - lengthPosition]);
  },

  /**
   * User data subparameter encoder : Reply Option
   *
   * @see 3GGP2 C.S0015-B 2.0, 4.5.11 Reply Option
   */
  encodeUserDataReplyOption: function cdma_encodeUserDataReplyOption(options) {
    if (options.requestStatusReport) {
      BitBufferHelper.writeBits(PDU_CDMA_MSG_USERDATA_REPLY_OPTION, 8);
      BitBufferHelper.writeBits(1, 8);
      BitBufferHelper.writeBits(0, 1); // USER_ACK_REQ
      BitBufferHelper.writeBits(1, 1); // DAK_REQ
      BitBufferHelper.flushWithPadding();
    }
  },

  /**
   * Entry point for SMS decoding, the returned object is made compatible
   * with existing readMessage() of GsmPDUHelper
   */
  readMessage: function cdma_readMessage() {
    let message = {};

    // Teleservice Identifier
    message.teleservice = this.readInt();

    // Message Type
    let isServicePresent = this.readByte();
    if (isServicePresent) {
      message.messageType = PDU_CDMA_MSG_TYPE_BROADCAST;
    } else {
      if (message.teleservice) {
        message.messageType = PDU_CDMA_MSG_TYPE_P2P;
      } else {
        message.messageType = PDU_CDMA_MSG_TYPE_ACK;
      }
    }

    // Service Category
    message.service = this.readInt();

    // Originated Address
    let addrInfo = {};
    addrInfo.digitMode = (this.readInt() & 0x01);
    addrInfo.numberMode = (this.readInt() & 0x01);
    addrInfo.numberType = (this.readInt() & 0x01);
    addrInfo.numberPlan = (this.readInt() & 0x01);
    addrInfo.addrLength = this.readByte();
    addrInfo.address = [];
    for (let i = 0; i < addrInfo.addrLength; i++) {
      addrInfo.address.push(this.readByte());
    }
    message.sender = this.decodeAddr(addrInfo);

    // Originated Subaddress
    addrInfo.Type = (this.readInt() & 0x07);
    addrInfo.Odd = (this.readByte() & 0x01);
    addrInfo.addrLength = this.readByte();
    for (let i = 0; i < addrInfo.addrLength; i++) {
      let addrDigit = this.readByte();
      message.sender += String.fromCharCode(addrDigit);
    }

    // Bearer Data
    this.decodeUserData(message);

    // Bearer Data Sub-Parameter: User Data
    let userData = message[PDU_CDMA_MSG_USERDATA_BODY];
    [message.header, message.body, message.encoding] =
      (userData)? [userData.header, userData.body, userData.encoding]
                : [null, null, null];

    // Bearer Data Sub-Parameter: Message Status
    // Success Delivery (0) if both Message Status and User Data are absent.
    // Message Status absent (-1) if only User Data is available.
    let msgStatus = message[PDU_CDMA_MSG_USER_DATA_MSG_STATUS];
    [message.errorClass, message.msgStatus] =
      (msgStatus)? [msgStatus.errorClass, msgStatus.msgStatus]
                 : ((message.body)? [-1, -1]: [0, 0]);

    // Transform message to GSM msg
    let msg = {
      SMSC:             "",
      mti:              0,
      udhi:             0,
      sender:           message.sender,
      recipient:        null,
      pid:              PDU_PID_DEFAULT,
      epid:             PDU_PID_DEFAULT,
      dcs:              0,
      mwi:              null,
      replace:          false,
      header:           message.header,
      body:             message.body,
      data:             null,
      timestamp:        message[PDU_CDMA_MSG_USERDATA_TIMESTAMP],
      language:         message[PDU_CDMA_LANGUAGE_INDICATOR],
      status:           null,
      scts:             null,
      dt:               null,
      encoding:         message.encoding,
      messageClass:     GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL],
      messageType:      message.messageType,
      serviceCategory:  message.service,
      subMsgType:       message[PDU_CDMA_MSG_USERDATA_MSG_ID].msgType,
      msgId:            message[PDU_CDMA_MSG_USERDATA_MSG_ID].msgId,
      errorClass:       message.errorClass,
      msgStatus:        message.msgStatus
    };

    return msg;
  },

  /**
   * Helper for processing received SMS parcel data.
   *
   * @param length
   *        Length of SMS string in the incoming parcel.
   *
   * @return Message parsed or null for invalid message.
   */
  processReceivedSms: function cdma_processReceivedSms(length) {
    if (!length) {
      if (DEBUG) debug("Received empty SMS!");
      return [null, PDU_FCS_UNSPECIFIED];
    }

    let message = this.readMessage();
    if (DEBUG) debug("Got new SMS: " + JSON.stringify(message));

    // Determine result
    if (!message) {
      return [null, PDU_FCS_UNSPECIFIED];
    }

    return [message, PDU_FCS_OK];
  },

  /**
   * Data readers
   */
  readInt: function readInt() {
    return Buf.readInt32();
  },

  readByte: function readByte() {
    return (Buf.readInt32() & 0xFF);
  },

  /**
   * Decode CDMA address data into address string
   *
   * @see 3GGP2 C.S0015-B 2.0, 3.4.3.3 Address Parameters
   *
   * Required key in addrInfo
   * @param addrLength
   *        Length of address
   * @param digitMode
   *        Address encoding method
   * @param address
   *        Array of encoded address data
   */
  decodeAddr: function cdma_decodeAddr(addrInfo) {
    let result = "";
    for (let i = 0; i < addrInfo.addrLength; i++) {
      if (addrInfo.digitMode === PDU_CDMA_MSG_ADDR_DIGIT_MODE_DTMF) {
        result += this.dtmfChars.charAt(addrInfo.address[i]);
      } else {
        result += String.fromCharCode(addrInfo.address[i]);
      }
    }
    return result;
  },

  /**
   * Read userData in parcel buffer and decode into message object.
   * Each subparameter will be stored in corresponding key.
   *
   * @see 3GGP2 C.S0015-B 2.0, 3.4.3.7 Bearer Data
   *                           4.5 Bearer Data Parameters
   */
  decodeUserData: function cdma_decodeUserData(message) {
    let userDataLength = this.readInt();

    while (userDataLength > 0) {
      let id = this.readByte(),
          length = this.readByte(),
          userDataBuffer = [];

      for (let i = 0; i < length; i++) {
          userDataBuffer.push(this.readByte());
      }

      BitBufferHelper.startRead(userDataBuffer);

      switch (id) {
        case PDU_CDMA_MSG_USERDATA_MSG_ID:
          message[id] = this.decodeUserDataMsgId();
          break;
        case PDU_CDMA_MSG_USERDATA_BODY:
          message[id] = this.decodeUserDataMsg(message[PDU_CDMA_MSG_USERDATA_MSG_ID].userHeader);
          break;
        case PDU_CDMA_MSG_USERDATA_TIMESTAMP:
          message[id] = this.decodeUserDataTimestamp();
          break;
        case PDU_CDMA_MSG_USERDATA_REPLY_OPTION:
          message[id] = this.decodeUserDataReplyOption();
          break;
        case PDU_CDMA_LANGUAGE_INDICATOR:
          message[id] = this.decodeLanguageIndicator();
          break;
        case PDU_CDMA_MSG_USERDATA_CALLBACK_NUMBER:
          message[id] = this.decodeUserDataCallbackNumber();
          break;
        case PDU_CDMA_MSG_USER_DATA_MSG_STATUS:
          message[id] = this.decodeUserDataMsgStatus();
          break;
      }

      userDataLength -= (length + 2);
      userDataBuffer = [];
    }
  },

  /**
   * User data subparameter decoder: Message Identifier
   *
   * @see 3GGP2 C.S0015-B 2.0, 4.5.1 Message Identifier
   */
  decodeUserDataMsgId: function cdma_decodeUserDataMsgId() {
    let result = {};
    result.msgType = BitBufferHelper.readBits(4);
    result.msgId = BitBufferHelper.readBits(16);
    result.userHeader = BitBufferHelper.readBits(1);

    return result;
  },

  /**
   * Decode user data header, we only care about segment information
   * on CDMA.
   *
   * This function is mostly copied from gsmPduHelper.readUserDataHeader() but
   * change the read function, because CDMA user header decoding is't byte-wise
   * aligned.
   */
  decodeUserDataHeader: function cdma_decodeUserDataHeader(encoding) {
    let header = {},
        headerSize = BitBufferHelper.readBits(8),
        userDataHeaderSize = headerSize + 1,
        headerPaddingBits = 0;

    // Calculate header size
    if (encoding === PDU_DCS_MSG_CODING_7BITS_ALPHABET) {
      // Length is in 7-bit
      header.length = Math.ceil(userDataHeaderSize * 8 / 7);
      // Calulate padding length
      headerPaddingBits = (header.length * 7) - (userDataHeaderSize * 8);
    } else if (encoding === PDU_DCS_MSG_CODING_8BITS_ALPHABET) {
      header.length = userDataHeaderSize;
    } else {
      header.length = userDataHeaderSize / 2;
    }

    while (headerSize) {
      let identifier = BitBufferHelper.readBits(8),
          length = BitBufferHelper.readBits(8);

      headerSize -= (2 + length);

      switch (identifier) {
        case PDU_IEI_CONCATENATED_SHORT_MESSAGES_8BIT: {
          let ref = BitBufferHelper.readBits(8),
              max = BitBufferHelper.readBits(8),
              seq = BitBufferHelper.readBits(8);
          if (max && seq && (seq <= max)) {
            header.segmentRef = ref;
            header.segmentMaxSeq = max;
            header.segmentSeq = seq;
          }
          break;
        }
        case PDU_IEI_APPLICATION_PORT_ADDRESSING_SCHEME_8BIT: {
          let dstp = BitBufferHelper.readBits(8),
              orip = BitBufferHelper.readBits(8);
          if ((dstp < PDU_APA_RESERVED_8BIT_PORTS)
              || (orip < PDU_APA_RESERVED_8BIT_PORTS)) {
            // 3GPP TS 23.040 clause 9.2.3.24.3: "A receiving entity shall
            // ignore any information element where the value of the
            // Information-Element-Data is Reserved or not supported"
            break;
          }
          header.destinationPort = dstp;
          header.originatorPort = orip;
          break;
        }
        case PDU_IEI_APPLICATION_PORT_ADDRESSING_SCHEME_16BIT: {
          let dstp = (BitBufferHelper.readBits(8) << 8) | BitBufferHelper.readBits(8),
              orip = (BitBufferHelper.readBits(8) << 8) | BitBufferHelper.readBits(8);
          // 3GPP TS 23.040 clause 9.2.3.24.4: "A receiving entity shall
          // ignore any information element where the value of the
          // Information-Element-Data is Reserved or not supported"
          if ((dstp < PDU_APA_VALID_16BIT_PORTS)
              && (orip < PDU_APA_VALID_16BIT_PORTS)) {
            header.destinationPort = dstp;
            header.originatorPort = orip;
          }
          break;
        }
        case PDU_IEI_CONCATENATED_SHORT_MESSAGES_16BIT: {
          let ref = (BitBufferHelper.readBits(8) << 8) | BitBufferHelper.readBits(8),
              max = BitBufferHelper.readBits(8),
              seq = BitBufferHelper.readBits(8);
          if (max && seq && (seq <= max)) {
            header.segmentRef = ref;
            header.segmentMaxSeq = max;
            header.segmentSeq = seq;
          }
          break;
        }
        case PDU_IEI_NATIONAL_LANGUAGE_SINGLE_SHIFT: {
          let langShiftIndex = BitBufferHelper.readBits(8);
          if (langShiftIndex < PDU_NL_SINGLE_SHIFT_TABLES.length) {
            header.langShiftIndex = langShiftIndex;
          }
          break;
        }
        case PDU_IEI_NATIONAL_LANGUAGE_LOCKING_SHIFT: {
          let langIndex = BitBufferHelper.readBits(8);
          if (langIndex < PDU_NL_LOCKING_SHIFT_TABLES.length) {
            header.langIndex = langIndex;
          }
          break;
        }
        case PDU_IEI_SPECIAL_SMS_MESSAGE_INDICATION: {
          let msgInd = BitBufferHelper.readBits(8) & 0xFF,
              msgCount = BitBufferHelper.readBits(8);
          /*
           * TS 23.040 V6.8.1 Sec 9.2.3.24.2
           * bits 1 0   : basic message indication type
           * bits 4 3 2 : extended message indication type
           * bits 6 5   : Profile id
           * bit  7     : storage type
           */
          let storeType = msgInd & PDU_MWI_STORE_TYPE_BIT;
          header.mwi = {};
          mwi = header.mwi;

          if (storeType == PDU_MWI_STORE_TYPE_STORE) {
            // Store message because TP_UDH indicates so, note this may override
            // the setting in DCS, but that is expected
            mwi.discard = false;
          } else if (mwi.discard === undefined) {
            // storeType == PDU_MWI_STORE_TYPE_DISCARD
            // only override mwi.discard here if it hasn't already been set
            mwi.discard = true;
          }

          mwi.msgCount = msgCount & 0xFF;
          mwi.active = mwi.msgCount > 0;

          if (DEBUG) debug("MWI in TP_UDH received: " + JSON.stringify(mwi));
          break;
        }
        default:
          // Drop unsupported id
          for (let i = 0; i < length; i++) {
            BitBufferHelper.readBits(8);
          }
      }
    }

    // Consume padding bits
    if (headerPaddingBits) {
      BitBufferHelper.readBits(headerPaddingBits);
    }

    return header;
  },

  getCdmaMsgEncoding: function getCdmaMsgEncoding(encoding) {
    // Determine encoding method
    switch (encoding) {
      case PDU_CDMA_MSG_CODING_7BITS_ASCII:
      case PDU_CDMA_MSG_CODING_IA5:
      case PDU_CDMA_MSG_CODING_7BITS_GSM:
        return PDU_DCS_MSG_CODING_7BITS_ALPHABET;
      case PDU_CDMA_MSG_CODING_OCTET:
      case PDU_CDMA_MSG_CODING_IS_91:
      case PDU_CDMA_MSG_CODING_LATIN_HEBREW:
      case PDU_CDMA_MSG_CODING_LATIN:
        return PDU_DCS_MSG_CODING_8BITS_ALPHABET;
      case PDU_CDMA_MSG_CODING_UNICODE:
      case PDU_CDMA_MSG_CODING_SHIFT_JIS:
      case PDU_CDMA_MSG_CODING_KOREAN:
        return PDU_DCS_MSG_CODING_16BITS_ALPHABET;
    }
    return null;
  },

  decodeCdmaPDUMsg: function decodeCdmaPDUMsg(encoding, msgType, msgBodySize) {
    const langTable = PDU_NL_LOCKING_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];
    const langShiftTable = PDU_NL_SINGLE_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];
    let result = "";
    let msgDigit;
    switch (encoding) {
      case PDU_CDMA_MSG_CODING_OCTET:         // TODO : Require Test
        while(msgBodySize > 0) {
          msgDigit = String.fromCharCode(BitBufferHelper.readBits(8));
          result += msgDigit;
          msgBodySize--;
        }
        break;
      case PDU_CDMA_MSG_CODING_IS_91:         // TODO : Require Test
        // Referenced from android code
        switch (msgType) {
          case PDU_CDMA_MSG_CODING_IS_91_TYPE_SMS:
          case PDU_CDMA_MSG_CODING_IS_91_TYPE_SMS_FULL:
          case PDU_CDMA_MSG_CODING_IS_91_TYPE_VOICEMAIL_STATUS:
            while(msgBodySize > 0) {
              msgDigit = String.fromCharCode(BitBufferHelper.readBits(6) + 0x20);
              result += msgDigit;
              msgBodySize--;
            }
            break;
          case PDU_CDMA_MSG_CODING_IS_91_TYPE_CLI:
            let addrInfo = {};
            addrInfo.digitMode = PDU_CDMA_MSG_ADDR_DIGIT_MODE_DTMF;
            addrInfo.numberMode = PDU_CDMA_MSG_ADDR_NUMBER_MODE_ANSI;
            addrInfo.numberType = PDU_CDMA_MSG_ADDR_NUMBER_TYPE_UNKNOWN;
            addrInfo.numberPlan = PDU_CDMA_MSG_ADDR_NUMBER_PLAN_UNKNOWN;
            addrInfo.addrLength = msgBodySize;
            addrInfo.address = [];
            for (let i = 0; i < addrInfo.addrLength; i++) {
              addrInfo.address.push(BitBufferHelper.readBits(4));
            }
            result = this.decodeAddr(addrInfo);
            break;
        }
        // Fall through.
      case PDU_CDMA_MSG_CODING_7BITS_ASCII:
      case PDU_CDMA_MSG_CODING_IA5:           // TODO : Require Test
        while(msgBodySize > 0) {
          msgDigit = BitBufferHelper.readBits(7);
          if (msgDigit >= 32) {
            msgDigit = String.fromCharCode(msgDigit);
          } else {
            if (msgDigit !== PDU_NL_EXTENDED_ESCAPE) {
              msgDigit = langTable[msgDigit];
            } else {
              msgDigit = BitBufferHelper.readBits(7);
              msgBodySize--;
              msgDigit = langShiftTable[msgDigit];
            }
          }
          result += msgDigit;
          msgBodySize--;
        }
        break;
      case PDU_CDMA_MSG_CODING_UNICODE:
        while(msgBodySize > 0) {
          msgDigit = String.fromCharCode(BitBufferHelper.readBits(16));
          result += msgDigit;
          msgBodySize--;
        }
        break;
      case PDU_CDMA_MSG_CODING_7BITS_GSM:     // TODO : Require Test
        while(msgBodySize > 0) {
          msgDigit = BitBufferHelper.readBits(7);
          if (msgDigit !== PDU_NL_EXTENDED_ESCAPE) {
            msgDigit = langTable[msgDigit];
          } else {
            msgDigit = BitBufferHelper.readBits(7);
            msgBodySize--;
            msgDigit = langShiftTable[msgDigit];
          }
          result += msgDigit;
          msgBodySize--;
        }
        break;
      case PDU_CDMA_MSG_CODING_LATIN:         // TODO : Require Test
        // Reference : http://en.wikipedia.org/wiki/ISO/IEC_8859-1
        while(msgBodySize > 0) {
          msgDigit = String.fromCharCode(BitBufferHelper.readBits(8));
          result += msgDigit;
          msgBodySize--;
        }
        break;
      case PDU_CDMA_MSG_CODING_LATIN_HEBREW:  // TODO : Require Test
        // Reference : http://en.wikipedia.org/wiki/ISO/IEC_8859-8
        while(msgBodySize > 0) {
          msgDigit = BitBufferHelper.readBits(8);
          if (msgDigit === 0xDF) {
            msgDigit = String.fromCharCode(0x2017);
          } else if (msgDigit === 0xFD) {
            msgDigit = String.fromCharCode(0x200E);
          } else if (msgDigit === 0xFE) {
            msgDigit = String.fromCharCode(0x200F);
          } else if (msgDigit >= 0xE0 && msgDigit <= 0xFA) {
            msgDigit = String.fromCharCode(0x4F0 + msgDigit);
          } else {
            msgDigit = String.fromCharCode(msgDigit);
          }
          result += msgDigit;
          msgBodySize--;
        }
        break;
      case PDU_CDMA_MSG_CODING_SHIFT_JIS:
        // Reference : http://msdn.microsoft.com/en-US/goglobal/cc305152.aspx
        //             http://demo.icu-project.org/icu-bin/convexp?conv=Shift_JIS
        // Fall through.
      case PDU_CDMA_MSG_CODING_KOREAN:
      case PDU_CDMA_MSG_CODING_GSM_DCS:
        // Fall through.
      default:
        break;
    }
    return result;
  },

  /**
   * User data subparameter decoder : User Data
   *
   * @see 3GGP2 C.S0015-B 2.0, 4.5.2 User Data
   */
  decodeUserDataMsg: function cdma_decodeUserDataMsg(hasUserHeader) {
    let result = {},
        encoding = BitBufferHelper.readBits(5),
        msgType;

    if(encoding === PDU_CDMA_MSG_CODING_IS_91) {
      msgType = BitBufferHelper.readBits(8);
    }
    result.encoding = this.getCdmaMsgEncoding(encoding);

    let msgBodySize = BitBufferHelper.readBits(8);

    // For segmented SMS, a user header is included before sms content
    if (hasUserHeader) {
      result.header = this.decodeUserDataHeader(result.encoding);
      // header size is included in body size, they are decoded
      msgBodySize -= result.header.length;
    }

    // Decode sms content
    result.body = this.decodeCdmaPDUMsg(encoding, msgType, msgBodySize);

    return result;
  },

  decodeBcd: function cdma_decodeBcd(value) {
    return ((value >> 4) & 0xF) * 10 + (value & 0x0F);
  },

  /**
   * User data subparameter decoder : Time Stamp
   *
   * @see 3GGP2 C.S0015-B 2.0, 4.5.4 Message Center Time Stamp
   */
  decodeUserDataTimestamp: function cdma_decodeUserDataTimestamp() {
    let year = this.decodeBcd(BitBufferHelper.readBits(8)),
        month = this.decodeBcd(BitBufferHelper.readBits(8)) - 1,
        date = this.decodeBcd(BitBufferHelper.readBits(8)),
        hour = this.decodeBcd(BitBufferHelper.readBits(8)),
        min = this.decodeBcd(BitBufferHelper.readBits(8)),
        sec = this.decodeBcd(BitBufferHelper.readBits(8));

    if (year >= 96 && year <= 99) {
      year += 1900;
    } else {
      year += 2000;
    }

    let result = (new Date(year, month, date, hour, min, sec, 0)).valueOf();

    return result;
  },

  /**
   * User data subparameter decoder : Reply Option
   *
   * @see 3GGP2 C.S0015-B 2.0, 4.5.11 Reply Option
   */
  decodeUserDataReplyOption: function cdma_decodeUserDataReplyOption() {
    let replyAction = BitBufferHelper.readBits(4),
        result = { userAck: (replyAction & 0x8) ? true : false,
                   deliverAck: (replyAction & 0x4) ? true : false,
                   readAck: (replyAction & 0x2) ? true : false,
                   report: (replyAction & 0x1) ? true : false
                 };

    return result;
  },

  /**
   * User data subparameter decoder : Language Indicator
   *
   * @see 3GGP2 C.S0015-B 2.0, 4.5.14 Language Indicator
   */
  decodeLanguageIndicator: function cdma_decodeLanguageIndicator() {
    let language = BitBufferHelper.readBits(8);
    let result = CB_CDMA_LANG_GROUP[language];
    return result;
  },

  /**
   * User data subparameter decoder : Call-Back Number
   *
   * @see 3GGP2 C.S0015-B 2.0, 4.5.15 Call-Back Number
   */
  decodeUserDataCallbackNumber: function cdma_decodeUserDataCallbackNumber() {
    let digitMode = BitBufferHelper.readBits(1);
    if (digitMode) {
      let numberType = BitBufferHelper.readBits(3),
          numberPlan = BitBufferHelper.readBits(4);
    }
    let numberFields = BitBufferHelper.readBits(8),
        result = "";
    for (let i = 0; i < numberFields; i++) {
      if (digitMode === PDU_CDMA_MSG_ADDR_DIGIT_MODE_DTMF) {
        let addrDigit = BitBufferHelper.readBits(4);
        result += this.dtmfChars.charAt(addrDigit);
      } else {
        let addrDigit = BitBufferHelper.readBits(8);
        result += String.fromCharCode(addrDigit);
      }
    }

    return result;
  },

  /**
   * User data subparameter decoder : Message Status
   *
   * @see 3GGP2 C.S0015-B 2.0, 4.5.21 Message Status
   */
  decodeUserDataMsgStatus: function cdma_decodeUserDataMsgStatus() {
    let result = {
      errorClass: BitBufferHelper.readBits(2),
      msgStatus: BitBufferHelper.readBits(6)
    };

    return result;
  },

  /**
   * Decode information record parcel.
   */
  decodeInformationRecord: function cdma_decodeInformationRecord() {
    let record = {};
    let numOfRecords = Buf.readInt32();

    let type;
    for (let i = 0; i < numOfRecords; i++) {
      type = Buf.readInt32();

      switch (type) {
        /*
         * Every type is encaped by ril, except extended display
         */
        case PDU_CDMA_INFO_REC_TYPE_DISPLAY:
          record.display = Buf.readString();
          break;
        case PDU_CDMA_INFO_REC_TYPE_CALLED_PARTY_NUMBER:
          record.calledNumber = {};
          record.calledNumber.number = Buf.readString();
          record.calledNumber.type = Buf.readInt32();
          record.calledNumber.plan = Buf.readInt32();
          record.calledNumber.pi = Buf.readInt32();
          record.calledNumber.si = Buf.readInt32();
          break;
        case PDU_CDMA_INFO_REC_TYPE_CALLING_PARTY_NUMBER:
          record.callingNumber = {};
          record.callingNumber.number = Buf.readString();
          record.callingNumber.type = Buf.readInt32();
          record.callingNumber.plan = Buf.readInt32();
          record.callingNumber.pi = Buf.readInt32();
          record.callingNumber.si = Buf.readInt32();
          break;
        case PDU_CDMA_INFO_REC_TYPE_CONNECTED_NUMBER:
          record.connectedNumber = {};
          record.connectedNumber.number = Buf.readString();
          record.connectedNumber.type = Buf.readInt32();
          record.connectedNumber.plan = Buf.readInt32();
          record.connectedNumber.pi = Buf.readInt32();
          record.connectedNumber.si = Buf.readInt32();
          break;
        case PDU_CDMA_INFO_REC_TYPE_SIGNAL:
          record.signal = {};
          record.signal.present = Buf.readInt32();
          record.signal.type = Buf.readInt32();
          record.signal.alertPitch = Buf.readInt32();
          record.signal.signal = Buf.readInt32();
          break;
        case PDU_CDMA_INFO_REC_TYPE_REDIRECTING_NUMBER:
          record.redirect = {};
          record.redirect.number = Buf.readString();
          record.redirect.type = Buf.readInt32();
          record.redirect.plan = Buf.readInt32();
          record.redirect.pi = Buf.readInt32();
          record.redirect.si = Buf.readInt32();
          record.redirect.reason = Buf.readInt32();
          break;
        case PDU_CDMA_INFO_REC_TYPE_LINE_CONTROL:
          record.lineControl = {};
          record.lineControl.polarityIncluded = Buf.readInt32();
          record.lineControl.toggle = Buf.readInt32();
          record.lineControl.recerse = Buf.readInt32();
          record.lineControl.powerDenial = Buf.readInt32();
          break;
        case PDU_CDMA_INFO_REC_TYPE_EXTENDED_DISPLAY:
          let length = Buf.readInt32();
          /*
           * Extended display is still in format defined in
           * C.S0005-F v1.0, 3.7.5.16
           */
          record.extendedDisplay = {};

          let headerByte = Buf.readInt32();
          length--;
          // Based on spec, headerByte must be 0x80 now
          record.extendedDisplay.indicator = (headerByte >> 7);
          record.extendedDisplay.type = (headerByte & 0x7F);
          record.extendedDisplay.records = [];

          while (length > 0) {
            let display = {};

            display.tag = Buf.readInt32();
            length--;
            if (display.tag !== INFO_REC_EXTENDED_DISPLAY_BLANK &&
                display.tag !== INFO_REC_EXTENDED_DISPLAY_SKIP) {
              display.content = Buf.readString();
              length -= (display.content.length + 1);
            }

            record.extendedDisplay.records.push(display);
          }
          break;
        case PDU_CDMA_INFO_REC_TYPE_T53_CLIR:
          record.cause = Buf.readInt32();
          break;
        case PDU_CDMA_INFO_REC_TYPE_T53_AUDIO_CONTROL:
          record.audioControl = {};
          record.audioControl.upLink = Buf.readInt32();
          record.audioControl.downLink = Buf.readInt32();
          break;
        case PDU_CDMA_INFO_REC_TYPE_T53_RELEASE:
          // Fall through
        default:
          throw new Error("UNSOLICITED_CDMA_INFO_REC(), Unsupported information record type " + record.type + "\n");
      }
    }

    return record;
  }
};

/**
 * Helper for processing ICC PDUs.
 */
let ICCPDUHelper = {
  /**
   * Read GSM 8-bit unpacked octets,
   * which are default 7-bit alphabets with bit 8 set to 0.
   *
   * @param numOctets
   *        Number of octets to be read.
   */
  read8BitUnpackedToString: function read8BitUnpackedToString(numOctets) {
    let ret = "";
    let escapeFound = false;
    let i;
    const langTable = PDU_NL_LOCKING_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];
    const langShiftTable = PDU_NL_SINGLE_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];

    for(i = 0; i < numOctets; i++) {
      let octet = GsmPDUHelper.readHexOctet();
      if (octet == 0xff) {
        i++;
        break;
      }

      if (escapeFound) {
        escapeFound = false;
        if (octet == PDU_NL_EXTENDED_ESCAPE) {
          // According to 3GPP TS 23.038, section 6.2.1.1, NOTE 1, "On
          // receipt of this code, a receiving entity shall display a space
          // until another extensiion table is defined."
          ret += " ";
        } else if (octet == PDU_NL_RESERVED_CONTROL) {
          // According to 3GPP TS 23.038 B.2, "This code represents a control
          // character and therefore must not be used for language specific
          // characters."
          ret += " ";
        } else {
          ret += langShiftTable[octet];
        }
      } else if (octet == PDU_NL_EXTENDED_ESCAPE) {
        escapeFound = true;
      } else {
        ret += langTable[octet];
      }
    }

    Buf.seekIncoming((numOctets - i) * Buf.PDU_HEX_OCTET_SIZE);
    return ret;
  },

  /**
   * Write GSM 8-bit unpacked octets.
   *
   * @param numOctets   Number of total octets to be writen, including trailing
   *                    0xff.
   * @param str         String to be written. Could be null.
   */
  writeStringTo8BitUnpacked: function writeStringTo8BitUnpacked(numOctets, str) {
    const langTable = PDU_NL_LOCKING_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];
    const langShiftTable = PDU_NL_SINGLE_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];

    // If the character is GSM extended alphabet, two octets will be written.
    // So we need to keep track of number of octets to be written.
    let i, j;
    let len = str ? str.length : 0;
    for (i = 0, j = 0; i < len && j < numOctets; i++) {
      let c = str.charAt(i);
      let octet = langTable.indexOf(c);

      if (octet == -1) {
        // Make sure we still have enough space to write two octets.
        if (j + 2 > numOctets) {
          break;
        }

        octet = langShiftTable.indexOf(c);
        if (octet == -1) {
          // Fallback to ASCII space.
          octet = langTable.indexOf(' ');
        }
        GsmPDUHelper.writeHexOctet(PDU_NL_EXTENDED_ESCAPE);
        j++;
      }
      GsmPDUHelper.writeHexOctet(octet);
      j++;
    }

    // trailing 0xff
    while (j++ < numOctets) {
      GsmPDUHelper.writeHexOctet(0xff);
    }
  },

  /**
   * Read UCS2 String on UICC.
   *
   * @see TS 101.221, Annex A.
   * @param scheme
   *        Coding scheme for UCS2 on UICC. One of 0x80, 0x81 or 0x82.
   * @param numOctets
   *        Number of octets to be read as UCS2 string.
   */
  readICCUCS2String: function readICCUCS2String(scheme, numOctets) {
    let str = "";
    switch (scheme) {
      /**
       * +------+---------+---------+---------+---------+------+------+
       * | 0x80 | Ch1_msb | Ch1_lsb | Ch2_msb | Ch2_lsb | 0xff | 0xff |
       * +------+---------+---------+---------+---------+------+------+
       */
      case 0x80:
        let isOdd = numOctets % 2;
        let i;
        for (i = 0; i < numOctets - isOdd; i += 2) {
          let code = (GsmPDUHelper.readHexOctet() << 8) | GsmPDUHelper.readHexOctet();
          if (code == 0xffff) {
            i += 2;
            break;
          }
          str += String.fromCharCode(code);
        }

        // Skip trailing 0xff
        Buf.seekIncoming((numOctets - i) * Buf.PDU_HEX_OCTET_SIZE);
        break;
      case 0x81: // Fall through
      case 0x82:
        /**
         * +------+-----+--------+-----+-----+-----+--------+------+
         * | 0x81 | len | offset | Ch1 | Ch2 | ... | Ch_len | 0xff |
         * +------+-----+--------+-----+-----+-----+--------+------+
         *
         * len : The length of characters.
         * offset : 0hhh hhhh h000 0000
         * Ch_n: bit 8 = 0
         *       GSM default alphabets
         *       bit 8 = 1
         *       UCS2 character whose char code is (Ch_n & 0x7f) + offset
         *
         * +------+-----+------------+------------+-----+-----+-----+--------+
         * | 0x82 | len | offset_msb | offset_lsb | Ch1 | Ch2 | ... | Ch_len |
         * +------+-----+------------+------------+-----+-----+-----+--------+
         *
         * len : The length of characters.
         * offset_msb, offset_lsn: offset
         * Ch_n: bit 8 = 0
         *       GSM default alphabets
         *       bit 8 = 1
         *       UCS2 character whose char code is (Ch_n & 0x7f) + offset
         */
        let len = GsmPDUHelper.readHexOctet();
        let offset, headerLen;
        if (scheme == 0x81) {
          offset = GsmPDUHelper.readHexOctet() << 7;
          headerLen = 2;
        } else {
          offset = (GsmPDUHelper.readHexOctet() << 8) | GsmPDUHelper.readHexOctet();
          headerLen = 3;
        }

        for (let i = 0; i < len; i++) {
          let ch = GsmPDUHelper.readHexOctet();
          if (ch & 0x80) {
            // UCS2
            str += String.fromCharCode((ch & 0x7f) + offset);
          } else {
            // GSM 8bit
            let count = 0, gotUCS2 = 0;
            while ((i + count + 1 < len)) {
              count++;
              if (GsmPDUHelper.readHexOctet() & 0x80) {
                gotUCS2 = 1;
                break;
              }
            }
            // Unread.
            // +1 for the GSM alphabet indexed at i,
            Buf.seekIncoming(-1 * (count + 1) * Buf.PDU_HEX_OCTET_SIZE);
            str += this.read8BitUnpackedToString(count + 1 - gotUCS2);
            i += count - gotUCS2;
          }
        }

        // Skipping trailing 0xff
        Buf.seekIncoming((numOctets - len - headerLen) * Buf.PDU_HEX_OCTET_SIZE);
        break;
    }
    return str;
  },

  /**
   * Read Alpha Id and Dialling number from TS TS 151.011 clause 10.5.1
   *
   * @param recordSize  The size of linear fixed record.
   */
  readAlphaIdDiallingNumber: function readAlphaIdDiallingNumber(recordSize) {
    let length = Buf.readInt32();

    let alphaLen = recordSize - ADN_FOOTER_SIZE_BYTES;
    let alphaId = this.readAlphaIdentifier(alphaLen);

    let number = this.readNumberWithLength();

    // Skip 2 unused octets, CCP and EXT1.
    Buf.seekIncoming(2 * Buf.PDU_HEX_OCTET_SIZE);
    Buf.readStringDelimiter(length);

    let contact = null;
    if (alphaId || number) {
      contact = {alphaId: alphaId,
                 number: number};
    }
    return contact;
  },

  /**
   * Write Alpha Identifier and Dialling number from TS 151.011 clause 10.5.1
   *
   * @param recordSize  The size of linear fixed record.
   * @param alphaId     Alpha Identifier to be written.
   * @param number      Dialling Number to be written.
   */
  writeAlphaIdDiallingNumber: function writeAlphaIdDiallingNumber(recordSize,
                                                                  alphaId,
                                                                  number) {
    // Write String length
    let strLen = recordSize * 2;
    Buf.writeInt32(strLen);

    let alphaLen = recordSize - ADN_FOOTER_SIZE_BYTES;
    this.writeAlphaIdentifier(alphaLen, alphaId);
    this.writeNumberWithLength(number);

    // Write unused octets 0xff, CCP and EXT1.
    GsmPDUHelper.writeHexOctet(0xff);
    GsmPDUHelper.writeHexOctet(0xff);
    Buf.writeStringDelimiter(strLen);
  },

  /**
   * Read Alpha Identifier.
   *
   * @see TS 131.102
   *
   * @param numOctets
   *        Number of octets to be read.
   *
   * It uses either
   *  1. SMS default 7-bit alphabet with bit 8 set to 0.
   *  2. UCS2 string.
   *
   * Unused bytes should be set to 0xff.
   */
  readAlphaIdentifier: function readAlphaIdentifier(numOctets) {
    if (numOctets === 0) {
      return "";
    }

    let temp;
    // Read the 1st octet to determine the encoding.
    if ((temp = GsmPDUHelper.readHexOctet()) == 0x80 ||
         temp == 0x81 ||
         temp == 0x82) {
      numOctets--;
      return this.readICCUCS2String(temp, numOctets);
    } else {
      Buf.seekIncoming(-1 * Buf.PDU_HEX_OCTET_SIZE);
      return this.read8BitUnpackedToString(numOctets);
    }
  },

  /**
   * Write Alpha Identifier.
   *
   * @param numOctets
   *        Total number of octets to be written. This includes the length of
   *        alphaId and the length of trailing unused octets(0xff).
   * @param alphaId
   *        Alpha Identifier to be written.
   *
   * Unused octets will be written as 0xff.
   */
  writeAlphaIdentifier: function writeAlphaIdentifier(numOctets, alphaId) {
    if (numOctets === 0) {
      return;
    }

    // If alphaId is empty or it's of GSM 8 bit.
    if (!alphaId || ICCUtilsHelper.isGsm8BitAlphabet(alphaId)) {
      this.writeStringTo8BitUnpacked(numOctets, alphaId);
    } else {
      // Currently only support UCS2 coding scheme 0x80.
      GsmPDUHelper.writeHexOctet(0x80);
      numOctets--;
      // Now the alphaId is UCS2 string, each character will take 2 octets.
      if (alphaId.length * 2 > numOctets) {
        alphaId = alphaId.substring(0, Math.floor(numOctets / 2));
      }
      GsmPDUHelper.writeUCS2String(alphaId);
      for (let i = alphaId.length * 2; i < numOctets; i++) {
        GsmPDUHelper.writeHexOctet(0xff);
      }
    }
  },

  /**
   * Read Dialling number.
   *
   * @see TS 131.102
   *
   * @param len
   *        The Length of BCD number.
   *
   * From TS 131.102, in EF_ADN, EF_FDN, the field 'Length of BCD number'
   * means the total bytes should be allocated to store the TON/NPI and
   * the dialing number.
   * For example, if the dialing number is 1234567890,
   * and the TON/NPI is 0x81,
   * The field 'Length of BCD number' should be 06, which is
   * 1 byte to store the TON/NPI, 0x81
   * 5 bytes to store the BCD number 2143658709.
   *
   * Here the definition of the length is different from SMS spec,
   * TS 23.040 9.1.2.5, which the length means
   * "number of useful semi-octets within the Address-Value field".
   */
  readDiallingNumber: function readDiallingNumber(len) {
    if (DEBUG) debug("PDU: Going to read Dialling number: " + len);
    if (len === 0) {
      return "";
    }

    // TOA = TON + NPI
    let toa = GsmPDUHelper.readHexOctet();

    let number = GsmPDUHelper.readSwappedNibbleBcdString(len - 1);
    if (number.length <= 0) {
      if (DEBUG) debug("No number provided");
      return "";
    }
    if ((toa >> 4) == (PDU_TOA_INTERNATIONAL >> 4)) {
      number = '+' + number;
    }
    return number;
  },

  /**
   * Write Dialling Number.
   *
   * @param number  The Dialling number
   */
  writeDiallingNumber: function writeDiallingNumber(number) {
    let toa = PDU_TOA_ISDN; // 81
    if (number[0] == '+') {
      toa = PDU_TOA_INTERNATIONAL | PDU_TOA_ISDN; // 91
      number = number.substring(1);
    }
    GsmPDUHelper.writeHexOctet(toa);
    GsmPDUHelper.writeSwappedNibbleBCD(number);
  },

  readNumberWithLength: function readNumberWithLength() {
    let number;
    let numLen = GsmPDUHelper.readHexOctet();
    if (numLen != 0xff) {
      if (numLen > ADN_MAX_BCD_NUMBER_BYTES) {
        throw new Error("invalid length of BCD number/SSC contents - " + numLen);
      }

      number = this.readDiallingNumber(numLen);
      Buf.seekIncoming((ADN_MAX_BCD_NUMBER_BYTES - numLen) * Buf.PDU_HEX_OCTET_SIZE);
    } else {
      Buf.seekIncoming(ADN_MAX_BCD_NUMBER_BYTES * Buf.PDU_HEX_OCTET_SIZE);
    }

    return number;
  },

  writeNumberWithLength: function writeNumberWithLength(number) {
    if (number) {
      let numStart = number[0] == "+" ? 1 : 0;
      let numDigits = number.length - numStart;
      if (numDigits > ADN_MAX_NUMBER_DIGITS) {
        number = number.substring(0, ADN_MAX_NUMBER_DIGITS + numStart);
        numDigits = number.length - numStart;
      }

      // +1 for TON/NPI
      let numLen = Math.ceil(numDigits / 2) + 1;
      GsmPDUHelper.writeHexOctet(numLen);
      this.writeDiallingNumber(number);
      // Write trailing 0xff of Dialling Number.
      for (let i = 0; i < ADN_MAX_BCD_NUMBER_BYTES - numLen; i++) {
        GsmPDUHelper.writeHexOctet(0xff);
      }
    } else {
      // +1 for numLen
      for (let i = 0; i < ADN_MAX_BCD_NUMBER_BYTES + 1; i++) {
        GsmPDUHelper.writeHexOctet(0xff);
      }
    }
  }
};

let StkCommandParamsFactory = {
  createParam: function createParam(cmdDetails, ctlvs) {
    let method = StkCommandParamsFactory[cmdDetails.typeOfCommand];
    if (typeof method != "function") {
      if (DEBUG) {
        debug("Unknown proactive command " + cmdDetails.typeOfCommand.toString(16));
      }
      return null;
    }
    return method.call(this, cmdDetails, ctlvs);
  },

  /**
   * Construct a param for Refresh.
   *
   * @param cmdDetails
   *        The value object of CommandDetails TLV.
   * @param ctlvs
   *        The all TLVs in this proactive command.
   */
  processRefresh: function processRefresh(cmdDetails, ctlvs) {
    let refreshType = cmdDetails.commandQualifier;
    switch (refreshType) {
      case STK_REFRESH_FILE_CHANGE:
      case STK_REFRESH_NAA_INIT_AND_FILE_CHANGE:
        let ctlv = StkProactiveCmdHelper.searchForTag(
          COMPREHENSIONTLV_TAG_FILE_LIST, ctlvs);
        if (ctlv) {
          let list = ctlv.value.fileList;
          if (DEBUG) {
            debug("Refresh, list = " + list);
          }
          ICCRecordHelper.fetchICCRecords();
        }
        break;
    }
    return null;
  },

  /**
   * Construct a param for Poll Interval.
   *
   * @param cmdDetails
   *        The value object of CommandDetails TLV.
   * @param ctlvs
   *        The all TLVs in this proactive command.
   */
  processPollInterval: function processPollInterval(cmdDetails, ctlvs) {
    let ctlv = StkProactiveCmdHelper.searchForTag(
        COMPREHENSIONTLV_TAG_DURATION, ctlvs);
    if (!ctlv) {
      RIL.sendStkTerminalResponse({
        command: cmdDetails,
        resultCode: STK_RESULT_REQUIRED_VALUES_MISSING});
      throw new Error("Stk Poll Interval: Required value missing : Duration");
    }

    return ctlv.value;
  },

  /**
   * Construct a param for Poll Off.
   *
   * @param cmdDetails
   *        The value object of CommandDetails TLV.
   * @param ctlvs
   *        The all TLVs in this proactive command.
   */
  processPollOff: function processPollOff(cmdDetails, ctlvs) {
    return null;
  },

  /**
   * Construct a param for Set Up Event list.
   *
   * @param cmdDetails
   *        The value object of CommandDetails TLV.
   * @param ctlvs
   *        The all TLVs in this proactive command.
   */
  processSetUpEventList: function processSetUpEventList(cmdDetails, ctlvs) {
    let ctlv = StkProactiveCmdHelper.searchForTag(
        COMPREHENSIONTLV_TAG_EVENT_LIST, ctlvs);
    if (!ctlv) {
      RIL.sendStkTerminalResponse({
        command: cmdDetails,
        resultCode: STK_RESULT_REQUIRED_VALUES_MISSING});
      throw new Error("Stk Event List: Required value missing : Event List");
    }

    return ctlv.value || {eventList: null};
  },

  /**
   * Construct a param for Select Item.
   *
   * @param cmdDetails
   *        The value object of CommandDetails TLV.
   * @param ctlvs
   *        The all TLVs in this proactive command.
   */
  processSelectItem: function processSelectItem(cmdDetails, ctlvs) {
    let menu = {};

    let ctlv = StkProactiveCmdHelper.searchForTag(COMPREHENSIONTLV_TAG_ALPHA_ID, ctlvs);
    if (ctlv) {
      menu.title = ctlv.value.identifier;
    }

    menu.items = [];
    for (let i = 0; i < ctlvs.length; i++) {
      let ctlv = ctlvs[i];
      if (ctlv.tag == COMPREHENSIONTLV_TAG_ITEM) {
        menu.items.push(ctlv.value);
      }
    }

    if (menu.items.length === 0) {
      RIL.sendStkTerminalResponse({
        command: cmdDetails,
        resultCode: STK_RESULT_REQUIRED_VALUES_MISSING});
      throw new Error("Stk Menu: Required value missing : items");
    }

    ctlv = StkProactiveCmdHelper.searchForTag(COMPREHENSIONTLV_TAG_ITEM_ID, ctlvs);
    if (ctlv) {
      menu.defaultItem = ctlv.value.identifier - 1;
    }

    // The 1st bit and 2nd bit determines the presentation type.
    menu.presentationType = cmdDetails.commandQualifier & 0x03;

    // Help information available.
    if (cmdDetails.commandQualifier & 0x80) {
      menu.isHelpAvailable = true;
    }

    ctlv = StkProactiveCmdHelper.searchForTag(COMPREHENSIONTLV_TAG_NEXT_ACTION_IND, ctlvs);
    if (ctlv) {
      menu.nextActionList = ctlv.value;
    }

    return menu;
  },

  processDisplayText: function processDisplayText(cmdDetails, ctlvs) {
    let textMsg = {};

    let ctlv = StkProactiveCmdHelper.searchForTag(COMPREHENSIONTLV_TAG_TEXT_STRING, ctlvs);
    if (!ctlv) {
      RIL.sendStkTerminalResponse({
        command: cmdDetails,
        resultCode: STK_RESULT_REQUIRED_VALUES_MISSING});
      throw new Error("Stk Display Text: Required value missing : Text String");
    }
    textMsg.text = ctlv.value.textString;

    ctlv = StkProactiveCmdHelper.searchForTag(COMPREHENSIONTLV_TAG_IMMEDIATE_RESPONSE, ctlvs);
    if (ctlv) {
      textMsg.responseNeeded = true;
    }

    ctlv = StkProactiveCmdHelper.searchForTag(COMPREHENSIONTLV_TAG_DURATION, ctlvs);
    if (ctlv) {
      textMsg.duration = ctlv.value;
    }

    // High priority.
    if (cmdDetails.commandQualifier & 0x01) {
      textMsg.isHighPriority = true;
    }

    // User clear.
    if (cmdDetails.commandQualifier & 0x80) {
      textMsg.userClear = true;
    }

    return textMsg;
  },

  processSetUpIdleModeText: function processSetUpIdleModeText(cmdDetails, ctlvs) {
    let textMsg = {};

    let ctlv = StkProactiveCmdHelper.searchForTag(COMPREHENSIONTLV_TAG_TEXT_STRING, ctlvs);
    if (!ctlv) {
      RIL.sendStkTerminalResponse({
        command: cmdDetails,
        resultCode: STK_RESULT_REQUIRED_VALUES_MISSING});
      throw new Error("Stk Set Up Idle Text: Required value missing : Text String");
    }
    textMsg.text = ctlv.value.textString;

    return textMsg;
  },

  processGetInkey: function processGetInkey(cmdDetails, ctlvs) {
    let input = {};

    let ctlv = StkProactiveCmdHelper.searchForTag(COMPREHENSIONTLV_TAG_TEXT_STRING, ctlvs);
    if (!ctlv) {
      RIL.sendStkTerminalResponse({
        command: cmdDetails,
        resultCode: STK_RESULT_REQUIRED_VALUES_MISSING});
      throw new Error("Stk Get InKey: Required value missing : Text String");
    }
    input.text = ctlv.value.textString;

    // duration
    ctlv = StkProactiveCmdHelper.searchForTag(
        COMPREHENSIONTLV_TAG_DURATION, ctlvs);
    if (ctlv) {
      input.duration = ctlv.value;
    }

    input.minLength = 1;
    input.maxLength = 1;

    // isAlphabet
    if (cmdDetails.commandQualifier & 0x01) {
      input.isAlphabet = true;
    }

    // UCS2
    if (cmdDetails.commandQualifier & 0x02) {
      input.isUCS2 = true;
    }

    // Character sets defined in bit 1 and bit 2 are disable and
    // the YES/NO reponse is required.
    if (cmdDetails.commandQualifier & 0x04) {
      input.isYesNoRequested = true;
    }

    // Help information available.
    if (cmdDetails.commandQualifier & 0x80) {
      input.isHelpAvailable = true;
    }

    return input;
  },

  processGetInput: function processGetInput(cmdDetails, ctlvs) {
    let input = {};

    let ctlv = StkProactiveCmdHelper.searchForTag(COMPREHENSIONTLV_TAG_TEXT_STRING, ctlvs);
    if (!ctlv) {
      RIL.sendStkTerminalResponse({
        command: cmdDetails,
        resultCode: STK_RESULT_REQUIRED_VALUES_MISSING});
      throw new Error("Stk Get Input: Required value missing : Text String");
    }
    input.text = ctlv.value.textString;

    ctlv = StkProactiveCmdHelper.searchForTag(COMPREHENSIONTLV_TAG_RESPONSE_LENGTH, ctlvs);
    if (ctlv) {
      input.minLength = ctlv.value.minLength;
      input.maxLength = ctlv.value.maxLength;
    }

    ctlv = StkProactiveCmdHelper.searchForTag(COMPREHENSIONTLV_TAG_DEFAULT_TEXT, ctlvs);
    if (ctlv) {
      input.defaultText = ctlv.value.textString;
    }

    // Alphabet only
    if (cmdDetails.commandQualifier & 0x01) {
      input.isAlphabet = true;
    }

    // UCS2
    if (cmdDetails.commandQualifier & 0x02) {
      input.isUCS2 = true;
    }

    // User input shall not be revealed
    if (cmdDetails.commandQualifier & 0x04) {
      input.hideInput = true;
    }

    // User input in SMS packed format
    if (cmdDetails.commandQualifier & 0x08) {
      input.isPacked = true;
    }

    // Help information available.
    if (cmdDetails.commandQualifier & 0x80) {
      input.isHelpAvailable = true;
    }

    return input;
  },

  processEventNotify: function processEventNotify(cmdDetails, ctlvs) {
    let textMsg = {};

    let ctlv = StkProactiveCmdHelper.searchForTag(COMPREHENSIONTLV_TAG_ALPHA_ID, ctlvs);
    if (!ctlv) {
      RIL.sendStkTerminalResponse({
        command: cmdDetails,
        resultCode: STK_RESULT_REQUIRED_VALUES_MISSING});
      throw new Error("Stk Event Notfiy: Required value missing : Alpha ID");
    }
    textMsg.text = ctlv.value.identifier;

    return textMsg;
  },

  processSetupCall: function processSetupCall(cmdDetails, ctlvs) {
    let call = {};
    let iter = Iterator(ctlvs);

    let ctlv = StkProactiveCmdHelper.searchForNextTag(COMPREHENSIONTLV_TAG_ALPHA_ID, iter);
    if (ctlv) {
      call.confirmMessage = ctlv.value.identifier;
    }

    ctlv = StkProactiveCmdHelper.searchForNextTag(COMPREHENSIONTLV_TAG_ALPHA_ID, iter);
    if (ctlv) {
      call.callMessage = ctlv.value.identifier;
    }

    ctlv = StkProactiveCmdHelper.searchForTag(COMPREHENSIONTLV_TAG_ADDRESS, ctlvs);
    if (!ctlv) {
      RIL.sendStkTerminalResponse({
        command: cmdDetails,
        resultCode: STK_RESULT_REQUIRED_VALUES_MISSING});
      throw new Error("Stk Set Up Call: Required value missing : Adress");
    }
    call.address = ctlv.value.number;

    // see 3GPP TS 31.111 section 6.4.13
    ctlv = StkProactiveCmdHelper.searchForTag(COMPREHENSIONTLV_TAG_DURATION, ctlvs);
    if (ctlv) {
      call.duration = ctlv.value;
    }

    return call;
  },

  processLaunchBrowser: function processLaunchBrowser(cmdDetails, ctlvs) {
    let browser = {};

    let ctlv = StkProactiveCmdHelper.searchForTag(COMPREHENSIONTLV_TAG_URL, ctlvs);
    if (!ctlv) {
      RIL.sendStkTerminalResponse({
        command: cmdDetails,
        resultCode: STK_RESULT_REQUIRED_VALUES_MISSING});
      throw new Error("Stk Launch Browser: Required value missing : URL");
    }
    browser.url = ctlv.value.url;

    ctlv = StkProactiveCmdHelper.searchForTag(COMPREHENSIONTLV_TAG_ALPHA_ID, ctlvs);
    if (ctlv) {
      browser.confirmMessage = ctlv.value.identifier;
    }

    browser.mode = cmdDetails.commandQualifier & 0x03;

    return browser;
  },

  processPlayTone: function processPlayTone(cmdDetails, ctlvs) {
    let playTone = {};

    let ctlv = StkProactiveCmdHelper.searchForTag(
        COMPREHENSIONTLV_TAG_ALPHA_ID, ctlvs);
    if (ctlv) {
      playTone.text = ctlv.value.identifier;
    }

    ctlv = StkProactiveCmdHelper.searchForTag(COMPREHENSIONTLV_TAG_TONE, ctlvs);
    if (ctlv) {
      playTone.tone = ctlv.value.tone;
    }

    ctlv = StkProactiveCmdHelper.searchForTag(
        COMPREHENSIONTLV_TAG_DURATION, ctlvs);
    if (ctlv) {
      playTone.duration = ctlv.value;
    }

    // vibrate is only defined in TS 102.223
    playTone.isVibrate = (cmdDetails.commandQualifier & 0x01) !== 0x00;

    return playTone;
  },

  /**
   * Construct a param for Provide Local Information
   *
   * @param cmdDetails
   *        The value object of CommandDetails TLV.
   * @param ctlvs
   *        The all TLVs in this proactive command.
   */
  processProvideLocalInfo: function processProvideLocalInfo(cmdDetails, ctlvs) {
    let provideLocalInfo = {
      localInfoType: cmdDetails.commandQualifier
    };
    return provideLocalInfo;
  },

  processTimerManagement: function processTimerManagement(cmdDetails, ctlvs) {
    let timer = {
      timerAction: cmdDetails.commandQualifier
    };

    let ctlv = StkProactiveCmdHelper.searchForTag(
        COMPREHENSIONTLV_TAG_TIMER_IDENTIFIER, ctlvs);
    if (ctlv) {
      timer.timerId = ctlv.value.timerId;
    }

    ctlv = StkProactiveCmdHelper.searchForTag(
        COMPREHENSIONTLV_TAG_TIMER_VALUE, ctlvs);
    if (ctlv) {
      timer.timerValue = ctlv.value.timerValue;
    }

    return timer;
  },

   /**
    * Construct a param for BIP commands.
    *
    * @param cmdDetails
    *        The value object of CommandDetails TLV.
    * @param ctlvs
    *        The all TLVs in this proactive command.
    */
  processBipMessage: function processBipMessage(cmdDetails, ctlvs) {
    let bipMsg = {};

    let ctlv = StkProactiveCmdHelper.searchForTag(
        COMPREHENSIONTLV_TAG_ALPHA_ID, ctlvs);
    if (ctlv) {
      bipMsg.text = ctlv.value.identifier;
    }

    return bipMsg;
  }
};
StkCommandParamsFactory[STK_CMD_REFRESH] = function STK_CMD_REFRESH(cmdDetails, ctlvs) {
  return this.processRefresh(cmdDetails, ctlvs);
};
StkCommandParamsFactory[STK_CMD_POLL_INTERVAL] = function STK_CMD_POLL_INTERVAL(cmdDetails, ctlvs) {
  return this.processPollInterval(cmdDetails, ctlvs);
};
StkCommandParamsFactory[STK_CMD_POLL_OFF] = function STK_CMD_POLL_OFF(cmdDetails, ctlvs) {
  return this.processPollOff(cmdDetails, ctlvs);
};
StkCommandParamsFactory[STK_CMD_PROVIDE_LOCAL_INFO] = function STK_CMD_PROVIDE_LOCAL_INFO(cmdDetails, ctlvs) {
  return this.processProvideLocalInfo(cmdDetails, ctlvs);
};
StkCommandParamsFactory[STK_CMD_SET_UP_EVENT_LIST] = function STK_CMD_SET_UP_EVENT_LIST(cmdDetails, ctlvs) {
  return this.processSetUpEventList(cmdDetails, ctlvs);
};
StkCommandParamsFactory[STK_CMD_SET_UP_MENU] = function STK_CMD_SET_UP_MENU(cmdDetails, ctlvs) {
  return this.processSelectItem(cmdDetails, ctlvs);
};
StkCommandParamsFactory[STK_CMD_SELECT_ITEM] = function STK_CMD_SELECT_ITEM(cmdDetails, ctlvs) {
  return this.processSelectItem(cmdDetails, ctlvs);
};
StkCommandParamsFactory[STK_CMD_DISPLAY_TEXT] = function STK_CMD_DISPLAY_TEXT(cmdDetails, ctlvs) {
  return this.processDisplayText(cmdDetails, ctlvs);
};
StkCommandParamsFactory[STK_CMD_SET_UP_IDLE_MODE_TEXT] = function STK_CMD_SET_UP_IDLE_MODE_TEXT(cmdDetails, ctlvs) {
  return this.processSetUpIdleModeText(cmdDetails, ctlvs);
};
StkCommandParamsFactory[STK_CMD_GET_INKEY] = function STK_CMD_GET_INKEY(cmdDetails, ctlvs) {
  return this.processGetInkey(cmdDetails, ctlvs);
};
StkCommandParamsFactory[STK_CMD_GET_INPUT] = function STK_CMD_GET_INPUT(cmdDetails, ctlvs) {
  return this.processGetInput(cmdDetails, ctlvs);
};
StkCommandParamsFactory[STK_CMD_SEND_SS] = function STK_CMD_SEND_SS(cmdDetails, ctlvs) {
  return this.processEventNotify(cmdDetails, ctlvs);
};
StkCommandParamsFactory[STK_CMD_SEND_USSD] = function STK_CMD_SEND_USSD(cmdDetails, ctlvs) {
  return this.processEventNotify(cmdDetails, ctlvs);
};
StkCommandParamsFactory[STK_CMD_SEND_SMS] = function STK_CMD_SEND_SMS(cmdDetails, ctlvs) {
  return this.processEventNotify(cmdDetails, ctlvs);
};
StkCommandParamsFactory[STK_CMD_SEND_DTMF] = function STK_CMD_SEND_DTMF(cmdDetails, ctlvs) {
  return this.processEventNotify(cmdDetails, ctlvs);
};
StkCommandParamsFactory[STK_CMD_SET_UP_CALL] = function STK_CMD_SET_UP_CALL(cmdDetails, ctlvs) {
  return this.processSetupCall(cmdDetails, ctlvs);
};
StkCommandParamsFactory[STK_CMD_LAUNCH_BROWSER] = function STK_CMD_LAUNCH_BROWSER(cmdDetails, ctlvs) {
  return this.processLaunchBrowser(cmdDetails, ctlvs);
};
StkCommandParamsFactory[STK_CMD_PLAY_TONE] = function STK_CMD_PLAY_TONE(cmdDetails, ctlvs) {
  return this.processPlayTone(cmdDetails, ctlvs);
};
StkCommandParamsFactory[STK_CMD_TIMER_MANAGEMENT] = function STK_CMD_TIMER_MANAGEMENT(cmdDetails, ctlvs) {
  return this.processTimerManagement(cmdDetails, ctlvs);
};
StkCommandParamsFactory[STK_CMD_OPEN_CHANNEL] = function STK_CMD_OPEN_CHANNEL(cmdDetails, ctlvs) {
  return this.processBipMessage(cmdDetails, ctlvs);
};
StkCommandParamsFactory[STK_CMD_CLOSE_CHANNEL] = function STK_CMD_CLOSE_CHANNEL(cmdDetails, ctlvs) {
  return this.processBipMessage(cmdDetails, ctlvs);
};
StkCommandParamsFactory[STK_CMD_RECEIVE_DATA] = function STK_CMD_RECEIVE_DATA(cmdDetails, ctlvs) {
  return this.processBipMessage(cmdDetails, ctlvs);
};
StkCommandParamsFactory[STK_CMD_SEND_DATA] = function STK_CMD_SEND_DATA(cmdDetails, ctlvs) {
  return this.processBipMessage(cmdDetails, ctlvs);
};

let StkProactiveCmdHelper = {
  retrieve: function retrieve(tag, length) {
    let method = StkProactiveCmdHelper[tag];
    if (typeof method != "function") {
      if (DEBUG) {
        debug("Unknown comprehension tag " + tag.toString(16));
      }
      Buf.seekIncoming(length * Buf.PDU_HEX_OCTET_SIZE);
      return null;
    }
    return method.call(this, length);
  },

  /**
   * Command Details.
   *
   * | Byte | Description         | Length |
   * |  1   | Command details Tag |   1    |
   * |  2   | Length = 03         |   1    |
   * |  3   | Command number      |   1    |
   * |  4   | Type of Command     |   1    |
   * |  5   | Command Qualifier   |   1    |
   */
  retrieveCommandDetails: function retrieveCommandDetails(length) {
    let cmdDetails = {
      commandNumber: GsmPDUHelper.readHexOctet(),
      typeOfCommand: GsmPDUHelper.readHexOctet(),
      commandQualifier: GsmPDUHelper.readHexOctet()
    };
    return cmdDetails;
  },

  /**
   * Device Identities.
   *
   * | Byte | Description            | Length |
   * |  1   | Device Identity Tag    |   1    |
   * |  2   | Length = 02            |   1    |
   * |  3   | Source device Identity |   1    |
   * |  4   | Destination device Id  |   1    |
   */
  retrieveDeviceId: function retrieveDeviceId(length) {
    let deviceId = {
      sourceId: GsmPDUHelper.readHexOctet(),
      destinationId: GsmPDUHelper.readHexOctet()
    };
    return deviceId;
  },

  /**
   * Alpha Identifier.
   *
   * | Byte         | Description            | Length |
   * |  1           | Alpha Identifier Tag   |   1    |
   * | 2 ~ (Y-1)+2  | Length (X)             |   Y    |
   * | (Y-1)+3 ~    | Alpha identfier        |   X    |
   * | (Y-1)+X+2    |                        |        |
   */
  retrieveAlphaId: function retrieveAlphaId(length) {
    let alphaId = {
      identifier: ICCPDUHelper.readAlphaIdentifier(length)
    };
    return alphaId;
  },

  /**
   * Duration.
   *
   * | Byte | Description           | Length |
   * |  1   | Response Length Tag   |   1    |
   * |  2   | Lenth = 02            |   1    |
   * |  3   | Time unit             |   1    |
   * |  4   | Time interval         |   1    |
   */
  retrieveDuration: function retrieveDuration(length) {
    let duration = {
      timeUnit: GsmPDUHelper.readHexOctet(),
      timeInterval: GsmPDUHelper.readHexOctet(),
    };
    return duration;
  },

  /**
   * Address.
   *
   * | Byte         | Description            | Length |
   * |  1           | Alpha Identifier Tag   |   1    |
   * | 2 ~ (Y-1)+2  | Length (X)             |   Y    |
   * | (Y-1)+3      | TON and NPI            |   1    |
   * | (Y-1)+4 ~    | Dialling number        |   X    |
   * | (Y-1)+X+2    |                        |        |
   */
  retrieveAddress: function retrieveAddress(length) {
    let address = {
      number : ICCPDUHelper.readDiallingNumber(length)
    };
    return address;
  },

  /**
   * Text String.
   *
   * | Byte         | Description        | Length |
   * |  1           | Text String Tag    |   1    |
   * | 2 ~ (Y-1)+2  | Length (X)         |   Y    |
   * | (Y-1)+3      | Data coding scheme |   1    |
   * | (Y-1)+4~     | Text String        |   X    |
   * | (Y-1)+X+2    |                    |        |
   */
  retrieveTextString: function retrieveTextString(length) {
    if (!length) {
      // null string.
      return {textString: null};
    }

    let text = {
      codingScheme: GsmPDUHelper.readHexOctet()
    };

    length--; // -1 for the codingScheme.
    switch (text.codingScheme & 0x0f) {
      case STK_TEXT_CODING_GSM_7BIT_PACKED:
        text.textString = GsmPDUHelper.readSeptetsToString(length * 8 / 7, 0, 0, 0);
        break;
      case STK_TEXT_CODING_GSM_8BIT:
        text.textString = ICCPDUHelper.read8BitUnpackedToString(length);
        break;
      case STK_TEXT_CODING_UCS2:
        text.textString = GsmPDUHelper.readUCS2String(length);
        break;
    }
    return text;
  },

  /**
   * Tone.
   *
   * | Byte | Description     | Length |
   * |  1   | Tone Tag        |   1    |
   * |  2   | Lenth = 01      |   1    |
   * |  3   | Tone            |   1    |
   */
  retrieveTone: function retrieveTone(length) {
    let tone = {
      tone: GsmPDUHelper.readHexOctet(),
    };
    return tone;
  },

  /**
   * Item.
   *
   * | Byte         | Description            | Length |
   * |  1           | Item Tag               |   1    |
   * | 2 ~ (Y-1)+2  | Length (X)             |   Y    |
   * | (Y-1)+3      | Identifier of item     |   1    |
   * | (Y-1)+4 ~    | Text string of item    |   X    |
   * | (Y-1)+X+2    |                        |        |
   */
  retrieveItem: function retrieveItem(length) {
    // TS 102.223 ,clause 6.6.7 SET-UP MENU
    // If the "Item data object for item 1" is a null data object
    // (i.e. length = '00' and no value part), this is an indication to the ME
    // to remove the existing menu from the menu system in the ME.
    if (!length) {
      return null;
    }
    let item = {
      identifier: GsmPDUHelper.readHexOctet(),
      text: ICCPDUHelper.readAlphaIdentifier(length - 1)
    };
    return item;
  },

  /**
   * Item Identifier.
   *
   * | Byte | Description               | Length |
   * |  1   | Item Identifier Tag       |   1    |
   * |  2   | Lenth = 01                |   1    |
   * |  3   | Identifier of Item chosen |   1    |
   */
  retrieveItemId: function retrieveItemId(length) {
    let itemId = {
      identifier: GsmPDUHelper.readHexOctet()
    };
    return itemId;
  },

  /**
   * Response Length.
   *
   * | Byte | Description                | Length |
   * |  1   | Response Length Tag        |   1    |
   * |  2   | Lenth = 02                 |   1    |
   * |  3   | Minimum length of response |   1    |
   * |  4   | Maximum length of response |   1    |
   */
  retrieveResponseLength: function retrieveResponseLength(length) {
    let rspLength = {
      minLength : GsmPDUHelper.readHexOctet(),
      maxLength : GsmPDUHelper.readHexOctet()
    };
    return rspLength;
  },

  /**
   * File List.
   *
   * | Byte         | Description            | Length |
   * |  1           | File List Tag          |   1    |
   * | 2 ~ (Y-1)+2  | Length (X)             |   Y    |
   * | (Y-1)+3      | Number of files        |   1    |
   * | (Y-1)+4 ~    | Files                  |   X    |
   * | (Y-1)+X+2    |                        |        |
   */
  retrieveFileList: function retrieveFileList(length) {
    let num = GsmPDUHelper.readHexOctet();
    let fileList = "";
    length--; // -1 for the num octet.
    for (let i = 0; i < 2 * length; i++) {
      // Didn't use readHexOctet here,
      // otherwise 0x00 will be "0", not "00"
      fileList += String.fromCharCode(Buf.readUint16());
    }
    return {
      fileList: fileList
    };
  },

  /**
   * Default Text.
   *
   * Same as Text String.
   */
  retrieveDefaultText: function retrieveDefaultText(length) {
    return this.retrieveTextString(length);
  },

  /**
   * Event List.
   */
  retrieveEventList: function retrieveEventList(length) {
    if (!length) {
      // null means an indication to ME to remove the existing list of events
      // in ME.
      return null;
    }

    let eventList = [];
    for (let i = 0; i < length; i++) {
      eventList.push(GsmPDUHelper.readHexOctet());
    }
    return {
      eventList: eventList
    };
  },

  /**
   * Timer Identifier.
   *
   * | Byte  | Description          | Length |
   * |  1    | Timer Identifier Tag |   1    |
   * |  2    | Length = 01          |   1    |
   * |  3    | Timer Identifier     |   1    |
   */
  retrieveTimerId: function retrieveTimerId(length) {
    let id = {
      timerId: GsmPDUHelper.readHexOctet()
    };
    return id;
  },

  /**
   * Timer Value.
   *
   * | Byte  | Description          | Length |
   * |  1    | Timer Value Tag      |   1    |
   * |  2    | Length = 03          |   1    |
   * |  3    | Hour                 |   1    |
   * |  4    | Minute               |   1    |
   * |  5    | Second               |   1    |
   */
  retrieveTimerValue: function retrieveTimerValue(length) {
    let value = {
      timerValue: (GsmPDUHelper.readSwappedNibbleBcdNum(1) * 60 * 60) +
                  (GsmPDUHelper.readSwappedNibbleBcdNum(1) * 60) +
                  (GsmPDUHelper.readSwappedNibbleBcdNum(1))
    };
    return value;
  },

  /**
   * Immediate Response.
   *
   * | Byte  | Description            | Length |
   * |  1    | Immediate Response Tag |   1    |
   * |  2    | Length = 00            |   1    |
   */
  retrieveImmediaResponse: function retrieveImmediaResponse(length) {
    return {};
  },

  /**
   * URL
   *
   * | Byte      | Description         | Length |
   * |  1        | URL Tag             |   1    |
   * | 2 ~ (Y+1) | Length(X)           |   Y    |
   * | (Y+2) ~   | URL                 |   X    |
   * | (Y+1+X)   |                     |        |
   */
  retrieveUrl: function retrieveUrl(length) {
    let s = "";
    for (let i = 0; i < length; i++) {
      s += String.fromCharCode(GsmPDUHelper.readHexOctet());
    }
    return {url: s};
  },

  /**
   * Next Action Indicator List.
   *
   * | Byte  | Description      | Length |
   * |  1    | Next Action tag  |   1    |
   * |  1    | Length(X)        |   1    |
   * |  3~   | Next Action List |   X    |
   * | 3+X-1 |                  |        |
   */
  retrieveNextActionList: function retrieveNextActionList(length) {
    let nextActionList = [];
    for (let i = 0; i < length; i++) {
      nextActionList.push(GsmPDUHelper.readHexOctet());
    }
    return nextActionList;
  },

  searchForTag: function searchForTag(tag, ctlvs) {
    let iter = Iterator(ctlvs);
    return this.searchForNextTag(tag, iter);
  },

  searchForNextTag: function searchForNextTag(tag, iter) {
    for (let [index, ctlv] in iter) {
      if ((ctlv.tag & ~COMPREHENSIONTLV_FLAG_CR) == tag) {
        return ctlv;
      }
    }
    return null;
  },
};
StkProactiveCmdHelper[COMPREHENSIONTLV_TAG_COMMAND_DETAILS] = function COMPREHENSIONTLV_TAG_COMMAND_DETAILS(length) {
  return this.retrieveCommandDetails(length);
};
StkProactiveCmdHelper[COMPREHENSIONTLV_TAG_DEVICE_ID] = function COMPREHENSIONTLV_TAG_DEVICE_ID(length) {
  return this.retrieveDeviceId(length);
};
StkProactiveCmdHelper[COMPREHENSIONTLV_TAG_ALPHA_ID] = function COMPREHENSIONTLV_TAG_ALPHA_ID(length) {
  return this.retrieveAlphaId(length);
};
StkProactiveCmdHelper[COMPREHENSIONTLV_TAG_DURATION] = function COMPREHENSIONTLV_TAG_DURATION(length) {
  return this.retrieveDuration(length);
};
StkProactiveCmdHelper[COMPREHENSIONTLV_TAG_ADDRESS] = function COMPREHENSIONTLV_TAG_ADDRESS(length) {
  return this.retrieveAddress(length);
};
StkProactiveCmdHelper[COMPREHENSIONTLV_TAG_TEXT_STRING] = function COMPREHENSIONTLV_TAG_TEXT_STRING(length) {
  return this.retrieveTextString(length);
};
StkProactiveCmdHelper[COMPREHENSIONTLV_TAG_TONE] = function COMPREHENSIONTLV_TAG_TONE(length) {
  return this.retrieveTone(length);
};
StkProactiveCmdHelper[COMPREHENSIONTLV_TAG_ITEM] = function COMPREHENSIONTLV_TAG_ITEM(length) {
  return this.retrieveItem(length);
};
StkProactiveCmdHelper[COMPREHENSIONTLV_TAG_ITEM_ID] = function COMPREHENSIONTLV_TAG_ITEM_ID(length) {
  return this.retrieveItemId(length);
};
StkProactiveCmdHelper[COMPREHENSIONTLV_TAG_RESPONSE_LENGTH] = function COMPREHENSIONTLV_TAG_RESPONSE_LENGTH(length) {
  return this.retrieveResponseLength(length);
};
StkProactiveCmdHelper[COMPREHENSIONTLV_TAG_FILE_LIST] = function COMPREHENSIONTLV_TAG_FILE_LIST(length) {
  return this.retrieveFileList(length);
};
StkProactiveCmdHelper[COMPREHENSIONTLV_TAG_DEFAULT_TEXT] = function COMPREHENSIONTLV_TAG_DEFAULT_TEXT(length) {
  return this.retrieveDefaultText(length);
};
StkProactiveCmdHelper[COMPREHENSIONTLV_TAG_EVENT_LIST] = function COMPREHENSIONTLV_TAG_EVENT_LIST(length) {
  return this.retrieveEventList(length);
};
StkProactiveCmdHelper[COMPREHENSIONTLV_TAG_TIMER_IDENTIFIER] = function COMPREHENSIONTLV_TAG_TIMER_IDENTIFIER(length) {
  return this.retrieveTimerId(length);
};
StkProactiveCmdHelper[COMPREHENSIONTLV_TAG_TIMER_VALUE] = function COMPREHENSIONTLV_TAG_TIMER_VALUE(length) {
  return this.retrieveTimerValue(length);
};
StkProactiveCmdHelper[COMPREHENSIONTLV_TAG_IMMEDIATE_RESPONSE] = function COMPREHENSIONTLV_TAG_IMMEDIATE_RESPONSE(length) {
  return this.retrieveImmediaResponse(length);
};
StkProactiveCmdHelper[COMPREHENSIONTLV_TAG_URL] = function COMPREHENSIONTLV_TAG_URL(length) {
  return this.retrieveUrl(length);
};
StkProactiveCmdHelper[COMPREHENSIONTLV_TAG_NEXT_ACTION_IND] = function COMPREHENSIONTLV_TAG_NEXT_ACTION_IND(length) {
  return this.retrieveNextActionList(length);
};

let ComprehensionTlvHelper = {
  /**
   * Decode raw data to a Comprehension-TLV.
   */
  decode: function decode() {
    let hlen = 0; // For header(tag field + length field) length.
    let temp = GsmPDUHelper.readHexOctet();
    hlen++;

    // TS 101.220, clause 7.1.1
    let tag, cr;
    switch (temp) {
      // TS 101.220, clause 7.1.1
      case 0x0: // Not used.
      case 0xff: // Not used.
      case 0x80: // Reserved for future use.
        RIL.sendStkTerminalResponse({
          resultCode: STK_RESULT_CMD_DATA_NOT_UNDERSTOOD});
        throw new Error("Invalid octet when parsing Comprehension TLV :" + temp);
      case 0x7f: // Tag is three byte format.
        // TS 101.220 clause 7.1.1.2.
        // | Byte 1 | Byte 2                        | Byte 3 |
        // |        | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 |        |
        // | 0x7f   |CR | Tag Value                          |
        tag = (GsmPDUHelper.readHexOctet() << 8) | GsmPDUHelper.readHexOctet();
        hlen += 2;
        cr = (tag & 0x8000) !== 0;
        tag &= ~0x8000;
        break;
      default: // Tag is single byte format.
        tag = temp;
        // TS 101.220 clause 7.1.1.1.
        // | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 |
        // |CR | Tag Value                 |
        cr = (tag & 0x80) !== 0;
        tag &= ~0x80;
    }

    // TS 101.220 clause 7.1.2, Length Encoding.
    // Length   |  Byte 1  | Byte 2 | Byte 3 | Byte 4 |
    // 0 - 127  |  00 - 7f | N/A    | N/A    | N/A    |
    // 128-255  |  81      | 80 - ff| N/A    | N/A    |
    // 256-65535|  82      | 0100 - ffff     | N/A    |
    // 65536-   |  83      |     010000 - ffffff      |
    // 16777215
    //
    // Length errors: TS 11.14, clause 6.10.6

    let length; // Data length.
    temp = GsmPDUHelper.readHexOctet();
    hlen++;
    if (temp < 0x80) {
      length = temp;
    } else if (temp == 0x81) {
      length = GsmPDUHelper.readHexOctet();
      hlen++;
      if (length < 0x80) {
        RIL.sendStkTerminalResponse({
          resultCode: STK_RESULT_CMD_DATA_NOT_UNDERSTOOD});
        throw new Error("Invalid length in Comprehension TLV :" + length);
      }
    } else if (temp == 0x82) {
      length = (GsmPDUHelper.readHexOctet() << 8) | GsmPDUHelper.readHexOctet();
      hlen += 2;
      if (lenth < 0x0100) {
         RIL.sendStkTerminalResponse({
          resultCode: STK_RESULT_CMD_DATA_NOT_UNDERSTOOD});
        throw new Error("Invalid length in 3-byte Comprehension TLV :" + length);
      }
    } else if (temp == 0x83) {
      length = (GsmPDUHelper.readHexOctet() << 16) |
               (GsmPDUHelper.readHexOctet() << 8)  |
                GsmPDUHelper.readHexOctet();
      hlen += 3;
      if (length < 0x010000) {
        RIL.sendStkTerminalResponse({
          resultCode: STK_RESULT_CMD_DATA_NOT_UNDERSTOOD});
        throw new Error("Invalid length in 4-byte Comprehension TLV :" + length);
      }
    } else {
      RIL.sendStkTerminalResponse({
        resultCode: STK_RESULT_CMD_DATA_NOT_UNDERSTOOD});
      throw new Error("Invalid octet in Comprehension TLV :" + temp);
    }

    let ctlv = {
      tag: tag,
      length: length,
      value: StkProactiveCmdHelper.retrieve(tag, length),
      cr: cr,
      hlen: hlen
    };
    return ctlv;
  },

  decodeChunks: function decodeChunks(length) {
    let chunks = [];
    let index = 0;
    while (index < length) {
      let tlv = this.decode();
      chunks.push(tlv);
      index += tlv.length;
      index += tlv.hlen;
    }
    return chunks;
  },

  /**
   * Write Location Info Comprehension TLV.
   *
   * @param loc location Information.
   */
  writeLocationInfoTlv: function writeLocationInfoTlv(loc) {
    GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_LOCATION_INFO |
                               COMPREHENSIONTLV_FLAG_CR);
    GsmPDUHelper.writeHexOctet(loc.gsmCellId > 0xffff ? 9 : 7);
    // From TS 11.14, clause 12.19
    // "The mobile country code (MCC), the mobile network code (MNC),
    // the location area code (LAC) and the cell ID are
    // coded as in TS 04.08."
    // And from TS 04.08 and TS 24.008,
    // the format is as follows:
    //
    // MCC = MCC_digit_1 + MCC_digit_2 + MCC_digit_3
    //
    //   8  7  6  5    4  3  2  1
    // +-------------+-------------+
    // | MCC digit 2 | MCC digit 1 | octet 2
    // | MNC digit 3 | MCC digit 3 | octet 3
    // | MNC digit 2 | MNC digit 1 | octet 4
    // +-------------+-------------+
    //
    // Also in TS 24.008
    // "However a network operator may decide to
    // use only two digits in the MNC in the LAI over the
    // radio interface. In this case, bits 5 to 8 of octet 3
    // shall be coded as '1111'".

    // MCC & MNC, 3 octets
    let mcc = loc.mcc, mnc;
    if (loc.mnc.length == 2) {
      mnc = "F" + loc.mnc;
    } else {
      mnc = loc.mnc[2] + loc.mnc[0] + loc.mnc[1];
    }
    GsmPDUHelper.writeSwappedNibbleBCD(mcc + mnc);

    // LAC, 2 octets
    GsmPDUHelper.writeHexOctet((loc.gsmLocationAreaCode >> 8) & 0xff);
    GsmPDUHelper.writeHexOctet(loc.gsmLocationAreaCode & 0xff);

    // Cell Id
    if (loc.gsmCellId > 0xffff) {
      // UMTS/WCDMA, gsmCellId is 28 bits.
      GsmPDUHelper.writeHexOctet((loc.gsmCellId >> 24) & 0xff);
      GsmPDUHelper.writeHexOctet((loc.gsmCellId >> 16) & 0xff);
      GsmPDUHelper.writeHexOctet((loc.gsmCellId >> 8) & 0xff);
      GsmPDUHelper.writeHexOctet(loc.gsmCellId & 0xff);
    } else {
      // GSM, gsmCellId is 16 bits.
      GsmPDUHelper.writeHexOctet((loc.gsmCellId >> 8) & 0xff);
      GsmPDUHelper.writeHexOctet(loc.gsmCellId & 0xff);
    }
  },

  /**
   * Given a geckoError string, this function translates it into cause value
   * and write the value into buffer.
   *
   * @param geckoError Error string that is passed to gecko.
   */
  writeCauseTlv: function writeCauseTlv(geckoError) {
    let cause = -1;
    for (let errorNo in RIL_ERROR_TO_GECKO_ERROR) {
      if (geckoError == RIL_ERROR_TO_GECKO_ERROR[errorNo]) {
        cause = errorNo;
        break;
      }
    }
    cause = (cause == -1) ? ERROR_SUCCESS : cause;

    GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_CAUSE |
                               COMPREHENSIONTLV_FLAG_CR);
    GsmPDUHelper.writeHexOctet(2);  // For single cause value.

    // TS 04.08, clause 10.5.4.11: National standard code + user location.
    GsmPDUHelper.writeHexOctet(0x60);

    // TS 04.08, clause 10.5.4.11: ext bit = 1 + 7 bits for cause.
    // +-----------------+----------------------------------+
    // | Ext = 1 (1 bit) |          Cause (7 bits)          |
    // +-----------------+----------------------------------+
    GsmPDUHelper.writeHexOctet(0x80 | cause);
  },

  writeDateTimeZoneTlv: function writeDataTimeZoneTlv(date) {
    GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_DATE_TIME_ZONE);
    GsmPDUHelper.writeHexOctet(7);
    GsmPDUHelper.writeTimestamp(date);
  },

  writeLanguageTlv: function writeLanguageTlv(language) {
    GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_LANGUAGE);
    GsmPDUHelper.writeHexOctet(2);

    // ISO 639-1, Alpha-2 code
    // TS 123.038, clause 6.2.1, GSM 7 bit Default Alphabet
    GsmPDUHelper.writeHexOctet(
      PDU_NL_LOCKING_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT].indexOf(language[0]));
    GsmPDUHelper.writeHexOctet(
      PDU_NL_LOCKING_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT].indexOf(language[1]));
  },

  /**
   * Write Timer Value Comprehension TLV.
   *
   * @param seconds length of time during of the timer.
   * @param cr Comprehension Required or not
   */
  writeTimerValueTlv: function writeTimerValueTlv(seconds, cr) {
    GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_TIMER_VALUE |
                               (cr ? COMPREHENSIONTLV_FLAG_CR : 0));
    GsmPDUHelper.writeHexOctet(3);

    // TS 102.223, clause 8.38
    // +----------------+------------------+-------------------+
    // | hours (1 byte) | minutes (1 btye) | secounds (1 byte) |
    // +----------------+------------------+-------------------+
    GsmPDUHelper.writeSwappedNibbleBCDNum(Math.floor(seconds / 60 / 60));
    GsmPDUHelper.writeSwappedNibbleBCDNum(Math.floor(seconds / 60) % 60);
    GsmPDUHelper.writeSwappedNibbleBCDNum(seconds % 60);
  },

  getSizeOfLengthOctets: function getSizeOfLengthOctets(length) {
    if (length >= 0x10000) {
      return 4; // 0x83, len_1, len_2, len_3
    } else if (length >= 0x100) {
      return 3; // 0x82, len_1, len_2
    } else if (length >= 0x80) {
      return 2; // 0x81, len
    } else {
      return 1; // len
    }
  },

  writeLength: function writeLength(length) {
    // TS 101.220 clause 7.1.2, Length Encoding.
    // Length   |  Byte 1  | Byte 2 | Byte 3 | Byte 4 |
    // 0 - 127  |  00 - 7f | N/A    | N/A    | N/A    |
    // 128-255  |  81      | 80 - ff| N/A    | N/A    |
    // 256-65535|  82      | 0100 - ffff     | N/A    |
    // 65536-   |  83      |     010000 - ffffff      |
    // 16777215
    if (length < 0x80) {
      GsmPDUHelper.writeHexOctet(length);
    } else if (0x80 <= length && length < 0x100) {
      GsmPDUHelper.writeHexOctet(0x81);
      GsmPDUHelper.writeHexOctet(length);
    } else if (0x100 <= length && length < 0x10000) {
      GsmPDUHelper.writeHexOctet(0x82);
      GsmPDUHelper.writeHexOctet((length >> 8) & 0xff);
      GsmPDUHelper.writeHexOctet(length & 0xff);
    } else if (0x10000 <= length && length < 0x1000000) {
      GsmPDUHelper.writeHexOctet(0x83);
      GsmPDUHelper.writeHexOctet((length >> 16) & 0xff);
      GsmPDUHelper.writeHexOctet((length >> 8) & 0xff);
      GsmPDUHelper.writeHexOctet(length & 0xff);
    } else {
      throw new Error("Invalid length value :" + length);
    }
  },
};

let BerTlvHelper = {
  /**
   * Decode Ber TLV.
   *
   * @param dataLen
   *        The length of data in bytes.
   */
  decode: function decode(dataLen) {
    // See TS 11.14, Annex D for BerTlv.
    let hlen = 0;
    let tag = GsmPDUHelper.readHexOctet();
    hlen++;

    // Length    | Byte 1                | Byte 2
    // 0 - 127   | length ('00' to '7f') | N/A
    // 128 - 255 | '81'                  | length ('80' to 'ff')
    let length;
    if (tag == BER_PROACTIVE_COMMAND_TAG) {
      let temp = GsmPDUHelper.readHexOctet();
      hlen++;
      if (temp < 0x80) {
        length = temp;
      } else if(temp == 0x81) {
        length = GsmPDUHelper.readHexOctet();
        if (length < 0x80) {
          RIL.sendStkTerminalResponse({
            resultCode: STK_RESULT_CMD_DATA_NOT_UNDERSTOOD});
          throw new Error("Invalid length " + length);
        }
      } else {
        RIL.sendStkTerminalResponse({
          resultCode: STK_RESULT_CMD_DATA_NOT_UNDERSTOOD});
        throw new Error("Invalid length octet " + temp);
      }
    } else {
      RIL.sendStkTerminalResponse({
        resultCode: STK_RESULT_CMD_DATA_NOT_UNDERSTOOD});
      throw new Error("Unknown BER tag");
    }

    // If the value length of the BerTlv is larger than remaining value on Parcel.
    if (dataLen - hlen < length) {
      RIL.sendStkTerminalResponse({
        resultCode: STK_RESULT_CMD_DATA_NOT_UNDERSTOOD});
      throw new Error("BerTlvHelper value length too long!!");
    }

    let ctlvs = ComprehensionTlvHelper.decodeChunks(length);
    let berTlv = {
      tag: tag,
      length: length,
      value: ctlvs
    };
    return berTlv;
  }
};

/**
 * ICC Helper for getting EF path.
 */
let ICCFileHelper = {
  /**
   * This function handles only EFs that are common to RUIM, SIM, USIM
   * and other types of ICC cards.
   */
  getCommonEFPath: function getCommonEFPath(fileId) {
    switch (fileId) {
      case ICC_EF_ICCID:
        return EF_PATH_MF_SIM;
      case ICC_EF_ADN:
        return EF_PATH_MF_SIM + EF_PATH_DF_TELECOM;
      case ICC_EF_PBR:
        return EF_PATH_MF_SIM + EF_PATH_DF_TELECOM + EF_PATH_DF_PHONEBOOK;
    }
    return null;
  },

  /**
   * This function handles EFs for SIM.
   */
  getSimEFPath: function getSimEFPath(fileId) {
    switch (fileId) {
      case ICC_EF_FDN:
      case ICC_EF_MSISDN:
      case ICC_EF_SMS:
        return EF_PATH_MF_SIM + EF_PATH_DF_TELECOM;
      case ICC_EF_AD:
      case ICC_EF_MBDN:
      case ICC_EF_PLMNsel:
      case ICC_EF_SPN:
      case ICC_EF_SPDI:
      case ICC_EF_SST:
      case ICC_EF_PHASE:
      case ICC_EF_CBMI:
      case ICC_EF_CBMID:
      case ICC_EF_CBMIR:
      case ICC_EF_OPL:
      case ICC_EF_PNN:
        return EF_PATH_MF_SIM + EF_PATH_DF_GSM;
      default:
        return null;
    }
  },

  /**
   * This function handles EFs for USIM.
   */
  getUSimEFPath: function getUSimEFPath(fileId) {
    switch (fileId) {
      case ICC_EF_AD:
      case ICC_EF_FDN:
      case ICC_EF_MBDN:
      case ICC_EF_UST:
      case ICC_EF_MSISDN:
      case ICC_EF_SPN:
      case ICC_EF_SPDI:
      case ICC_EF_CBMI:
      case ICC_EF_CBMID:
      case ICC_EF_CBMIR:
      case ICC_EF_OPL:
      case ICC_EF_PNN:
      case ICC_EF_SMS:
        return EF_PATH_MF_SIM + EF_PATH_ADF_USIM;
      default:
        // The file ids in USIM phone book entries are decided by the
        // card manufacturer. So if we don't match any of the cases
        // above and if its a USIM return the phone book path.
        return EF_PATH_MF_SIM + EF_PATH_DF_TELECOM + EF_PATH_DF_PHONEBOOK;
    }
  },

  /**
   * This function handles EFs for RUIM
   */
  getRuimEFPath: function getRuimEFPath(fileId) {
    switch(fileId) {
      case ICC_EF_CSIM_IMSI_M:
      case ICC_EF_CSIM_CDMAHOME:
      case ICC_EF_CSIM_CST:
      case ICC_EF_CSIM_SPN:
        return EF_PATH_MF_SIM + EF_PATH_DF_CDMA;
      case ICC_EF_FDN:
        return EF_PATH_MF_SIM + EF_PATH_DF_TELECOM;
      default:
        return null;
    }
  },

  /**
   * Helper function for getting the pathId for the specific ICC record
   * depeding on which type of ICC card we are using.
   *
   * @param fileId
   *        File id.
   * @return The pathId or null in case of an error or invalid input.
   */
  getEFPath: function getEFPath(fileId) {
    if (RIL.appType == null) {
      return null;
    }

    let path = this.getCommonEFPath(fileId);
    if (path) {
      return path;
    }

    switch (RIL.appType) {
      case CARD_APPTYPE_SIM:
        return this.getSimEFPath(fileId);
      case CARD_APPTYPE_USIM:
        return this.getUSimEFPath(fileId);
      case CARD_APPTYPE_RUIM:
        return this.getRuimEFPath(fileId);
      default:
        return null;
    }
  }
};

/**
 * Helper for ICC IO functionalities.
 */
let ICCIOHelper = {
  /**
   * Load EF with type 'Linear Fixed'.
   *
   * @param fileId
   *        The file to operate on, one of the ICC_EF_* constants.
   * @param recordNumber [optional]
   *        The number of the record shall be loaded.
   * @param recordSize [optional]
   *        The size of the record.
   * @param callback [optional]
   *        The callback function shall be called when the record(s) is read.
   * @param onerror [optional]
   *        The callback function shall be called when failure.
   */
  loadLinearFixedEF: function loadLinearFixedEF(options) {
    let cb;
    function readRecord(options) {
      options.command = ICC_COMMAND_READ_RECORD;
      options.p1 = options.recordNumber || 1; // Record number
      options.p2 = READ_RECORD_ABSOLUTE_MODE;
      options.p3 = options.recordSize;
      options.callback = cb || options.callback;
      RIL.iccIO(options);
    }

    options.type = EF_TYPE_LINEAR_FIXED;
    options.pathId = ICCFileHelper.getEFPath(options.fileId);
    if (options.recordSize) {
      readRecord(options);
      return;
    }

    cb = options.callback;
    options.callback = readRecord.bind(this);
    this.getResponse(options);
  },

  /**
   * Load next record from current record Id.
   */
  loadNextRecord: function loadNextRecord(options) {
    options.p1++;
    RIL.iccIO(options);
  },

  /**
   * Update EF with type 'Linear Fixed'.
   *
   * @param fileId
   *        The file to operate on, one of the ICC_EF_* constants.
   * @param recordNumber
   *        The number of the record shall be updated.
   * @param dataWriter [optional]
   *        The function for writing string parameter for the ICC_COMMAND_UPDATE_RECORD.
   * @param pin2 [optional]
   *        PIN2 is required when updating ICC_EF_FDN.
   * @param callback [optional]
   *        The callback function shall be called when the record is updated.
   * @param onerror [optional]
   *        The callback function shall be called when failure.
   */
  updateLinearFixedEF: function updateLinearFixedEF(options) {
    if (!options.fileId || !options.recordNumber) {
      throw new Error("Unexpected fileId " + options.fileId +
                      " or recordNumber " + options.recordNumber);
    }

    options.type = EF_TYPE_LINEAR_FIXED;
    options.pathId = ICCFileHelper.getEFPath(options.fileId);
    let cb = options.callback;
    options.callback = function callback(options) {
      options.callback = cb;
      options.command = ICC_COMMAND_UPDATE_RECORD;
      options.p1 = options.recordNumber;
      options.p2 = READ_RECORD_ABSOLUTE_MODE;
      options.p3 = options.recordSize;
      RIL.iccIO(options);
    }.bind(this);
    this.getResponse(options);
  },

  /**
   * Load EF with type 'Transparent'.
   *
   * @param fileId
   *        The file to operate on, one of the ICC_EF_* constants.
   * @param callback [optional]
   *        The callback function shall be called when the record(s) is read.
   * @param onerror [optional]
   *        The callback function shall be called when failure.
   */
  loadTransparentEF: function loadTransparentEF(options) {
    options.type = EF_TYPE_TRANSPARENT;
    let cb = options.callback;
    options.callback = function callback(options) {
      options.callback = cb;
      options.command = ICC_COMMAND_READ_BINARY;
      options.p3 = options.fileSize;
      RIL.iccIO(options);
    }.bind(this);
    this.getResponse(options);
  },

  /**
   * Use ICC_COMMAND_GET_RESPONSE to query the EF.
   *
   * @param fileId
   *        The file to operate on, one of the ICC_EF_* constants.
   */
  getResponse: function getResponse(options) {
    options.command = ICC_COMMAND_GET_RESPONSE;
    options.pathId = options.pathId || ICCFileHelper.getEFPath(options.fileId);
    if (!options.pathId) {
      throw new Error("Unknown pathId for " + options.fileId.toString(16));
    }
    options.p1 = 0; // For GET_RESPONSE, p1 = 0
    options.p2 = 0; // For GET_RESPONSE, p2 = 0
    options.p3 = GET_RESPONSE_EF_SIZE_BYTES;
    RIL.iccIO(options);
  },

  /**
   * Process ICC I/O response.
   */
  processICCIO: function processICCIO(options) {
    let func = this[options.command];
    func.call(this, options);
  },

  /**
   * Process a ICC_COMMAND_GET_RESPONSE type command for REQUEST_SIM_IO.
   */
  processICCIOGetResponse: function processICCIOGetResponse(options) {
    let strLen = Buf.readInt32();

    // The format is from TS 51.011, clause 9.2.1

    // Skip RFU, data[0] data[1]
    Buf.seekIncoming(2 * Buf.PDU_HEX_OCTET_SIZE);

    // File size, data[2], data[3]
    options.fileSize = (GsmPDUHelper.readHexOctet() << 8) |
                        GsmPDUHelper.readHexOctet();

    // 2 bytes File id. data[4], data[5]
    let fileId = (GsmPDUHelper.readHexOctet() << 8) |
                  GsmPDUHelper.readHexOctet();
    if (fileId != options.fileId) {
      throw new Error("Expected file ID " + options.fileId.toString(16) +
                      " but read " + fileId.toString(16));
    }

    // Type of file, data[6]
    let fileType = GsmPDUHelper.readHexOctet();
    if (fileType != TYPE_EF) {
      throw new Error("Unexpected file type " + fileType);
    }

    // Skip 1 byte RFU, data[7],
    //      3 bytes Access conditions, data[8] data[9] data[10],
    //      1 byte File status, data[11],
    //      1 byte Length of the following data, data[12].
    Buf.seekIncoming(((RESPONSE_DATA_STRUCTURE - RESPONSE_DATA_FILE_TYPE - 1) *
        Buf.PDU_HEX_OCTET_SIZE));

    // Read Structure of EF, data[13]
    let efType = GsmPDUHelper.readHexOctet();
    if (efType != options.type) {
      throw new Error("Expected EF type " + options.type + " but read " + efType);
    }

    // Length of a record, data[14].
    // Only available for LINEAR_FIXED and CYCLIC.
    if (efType == EF_TYPE_LINEAR_FIXED || efType == EF_TYPE_CYCLIC) {
      options.recordSize = GsmPDUHelper.readHexOctet();
      options.totalRecords = options.fileSize / options.recordSize;
    } else {
      Buf.seekIncoming(1 * Buf.PDU_HEX_OCTET_SIZE);
    }

    Buf.readStringDelimiter(strLen);

    if (options.callback) {
      options.callback(options);
    }
  },

  /**
   * Process a ICC_COMMAND_READ_RECORD type command for REQUEST_SIM_IO.
   */
  processICCIOReadRecord: function processICCIOReadRecord(options) {
    if (options.callback) {
      options.callback(options);
    }
  },

  /**
   * Process a ICC_COMMAND_READ_BINARY type command for REQUEST_SIM_IO.
   */
  processICCIOReadBinary: function processICCIOReadBinary(options) {
    if (options.callback) {
      options.callback(options);
    }
  },

  /**
   * Process a ICC_COMMAND_UPDATE_RECORD type command for REQUEST_SIM_IO.
   */
  processICCIOUpdateRecord: function processICCIOUpdateRecord(options) {
    if (options.callback) {
      options.callback(options);
    }
  },

  /**
   * Process ICC IO error.
   */
  processICCIOError: function processICCIOError(options) {
    let requestError = RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError];
    if (DEBUG) {
      // See GSM11.11, TS 51.011 clause 9.4, and ISO 7816-4 for the error
      // description.
      let errorMsg = "ICC I/O Error code " + requestError +
                     " EF id = " + options.fileId.toString(16) +
                     " command = " + options.command.toString(16);
      if (options.sw1 && options.sw2) {
        errorMsg += "(" + options.sw1.toString(16) +
                    "/" + options.sw2.toString(16) + ")";
      }
      debug(errorMsg);
    }
    onerror(requestError);
  },
};
ICCIOHelper[ICC_COMMAND_SEEK] = null;
ICCIOHelper[ICC_COMMAND_READ_BINARY] = function ICC_COMMAND_READ_BINARY(options) {
  this.processICCIOReadBinary(options);
};
ICCIOHelper[ICC_COMMAND_READ_RECORD] = function ICC_COMMAND_READ_RECORD(options) {
  this.processICCIOReadRecord(options);
};
ICCIOHelper[ICC_COMMAND_GET_RESPONSE] = function ICC_COMMAND_GET_RESPONSE(options) {
  this.processICCIOGetResponse(options);
};
ICCIOHelper[ICC_COMMAND_UPDATE_BINARY] = null;
ICCIOHelper[ICC_COMMAND_UPDATE_RECORD] = function ICC_COMMAND_UPDATE_RECORD(options) {
  this.processICCIOUpdateRecord(options);
};

/**
 * Helper for ICC records.
 */
let ICCRecordHelper = {
  /**
   * Fetch ICC records.
   */
  fetchICCRecords: function fetchICCRecords() {
    switch (RIL.appType) {
      case CARD_APPTYPE_SIM:
      case CARD_APPTYPE_USIM:
        SimRecordHelper.fetchSimRecords();
        break;
      case CARD_APPTYPE_RUIM:
        RuimRecordHelper.fetchRuimRecords();
        break;
    }
  },

  /**
   * Read the ICCID.
   */
  readICCID: function readICCID() {
    function callback() {
      let strLen = Buf.readInt32();
      let octetLen = strLen / 2;
      RIL.iccInfo.iccid = GsmPDUHelper.readSwappedNibbleBcdString(octetLen);
      Buf.readStringDelimiter(strLen);

      if (DEBUG) debug("ICCID: " + RIL.iccInfo.iccid);
      if (RIL.iccInfo.iccid) {
        ICCUtilsHelper.handleICCInfoChange();
        RIL.reportStkServiceIsRunning();
      }
    }

    ICCIOHelper.loadTransparentEF({fileId: ICC_EF_ICCID,
                                   callback: callback.bind(this)});
  },

  /**
   * Read ICC ADN like EF, i.e. EF_ADN, EF_FDN.
   *
   * @param fileId      EF id of the ADN or FDN.
   * @param onsuccess   Callback to be called when success.
   * @param onerror     Callback to be called when error.
   */
  readADNLike: function readADNLike(fileId, onsuccess, onerror) {
    function callback(options) {
      let contact = ICCPDUHelper.readAlphaIdDiallingNumber(options.recordSize);
      if (contact) {
        contact.recordId = options.p1;
        contacts.push(contact);
      }

      if (options.p1 < options.totalRecords) {
        ICCIOHelper.loadNextRecord(options);
      } else {
        if (DEBUG) {
          for (let i = 0; i < contacts.length; i++) {
            debug("contact [" + i + "] " + JSON.stringify(contacts[i]));
          }
        }
        if (onsuccess) {
          onsuccess(contacts);
        }
      }
    }

    let contacts = [];
    ICCIOHelper.loadLinearFixedEF({fileId: fileId,
                                   callback: callback.bind(this),
                                   onerror: onerror});
  },

  /**
   * Update ICC ADN like EFs, like EF_ADN, EF_FDN.
   *
   * @param fileId      EF id of the ADN or FDN.
   * @param contact     The contact will be updated. (Shall have recordId property)
   * @param pin2        PIN2 is required when updating ICC_EF_FDN.
   * @param onsuccess   Callback to be called when success.
   * @param onerror     Callback to be called when error.
   */
  updateADNLike: function updateADNLike(fileId, contact, pin2, onsuccess, onerror) {
    function dataWriter(recordSize) {
      ICCPDUHelper.writeAlphaIdDiallingNumber(recordSize,
                                              contact.alphaId,
                                              contact.number);
    }

    function callback(options) {
      if (onsuccess) {
        onsuccess();
      }
    }

    if (!contact || !contact.recordId) {
      let error = onerror || debug;
      error(GECKO_ERROR_INVALID_PARAMETER);
      return;
    }

    ICCIOHelper.updateLinearFixedEF({fileId: fileId,
                                     recordNumber: contact.recordId,
                                     dataWriter: dataWriter.bind(this),
                                     pin2: pin2,
                                     callback: callback.bind(this),
                                     onerror: onerror});
  },

  /**
   * Read USIM/RUIM Phonebook.
   *
   * @param onsuccess   Callback to be called when success.
   * @param onerror     Callback to be called when error.
   */
  readPBR: function readPBR(onsuccess, onerror) {
    function callback(options) {
      let strLen = Buf.readInt32();
      let octetLen = strLen / 2, readLen = 0;

      let pbrTlvs = [];
      while (readLen < octetLen) {
        let tag = GsmPDUHelper.readHexOctet();
        if (tag == 0xff) {
          readLen++;
          Buf.seekIncoming((octetLen - readLen) * Buf.PDU_HEX_OCTET_SIZE);
          break;
        }

        let tlvLen = GsmPDUHelper.readHexOctet();
        let tlvs = ICCUtilsHelper.decodeSimTlvs(tlvLen);
        pbrTlvs.push({tag: tag,
                      length: tlvLen,
                      value: tlvs});

        readLen += tlvLen + 2; // +2 for tag and tlvLen
      }
      Buf.readStringDelimiter(strLen);

      if (pbrTlvs.length > 0) {
        let pbr = ICCUtilsHelper.parsePbrTlvs(pbrTlvs);
        // EF_ADN is mandatory if and only if DF_PHONEBOOK is present.
        if (!pbr.adn) {
          let error = onerror || debug;
          error("Cannot access ADN.");
          return;
        }
        pbrs.push(pbr);
      }

      if (options.p1 < options.totalRecords) {
        ICCIOHelper.loadNextRecord(options);
      } else {
        if (onsuccess) {
          RIL.iccInfoPrivate.pbrs = pbrs;
          onsuccess(pbrs);
        }
      }
    }

    if (RIL.iccInfoPrivate.pbrs) {
      onsuccess(RIL.iccInfoPrivate.pbrs);
      return;
    }

    let pbrs = [];
    ICCIOHelper.loadLinearFixedEF({fileId : ICC_EF_PBR,
                                   callback: callback.bind(this),
                                   onerror: onerror});
  },

  /**
   * Cache EF_IAP record size.
   */
  _iapRecordSize: null,

  /**
   * Read ICC EF_IAP. (Index Administration Phonebook)
   *
   * @see TS 131.102, clause 4.4.2.2
   *
   * @param fileId       EF id of the IAP.
   * @param recordNumber The number of the record shall be loaded.
   * @param onsuccess    Callback to be called when success.
   * @param onerror      Callback to be called when error.
   */
  readIAP: function readIAP(fileId, recordNumber, onsuccess, onerror) {
    function callback(options) {
      let strLen = Buf.readInt32();
      let octetLen = strLen / 2;
      this._iapRecordSize = options.recordSize;

      let iap = GsmPDUHelper.readHexOctetArray(octetLen);
      Buf.readStringDelimiter(strLen);

      if (onsuccess) {
        onsuccess(iap);
      }
    }

    ICCIOHelper.loadLinearFixedEF({fileId: fileId,
                                   recordNumber: recordNumber,
                                   recordSize: this._iapRecordSize,
                                   callback: callback.bind(this),
                                   onerror: onerror});
  },

  /**
   * Update USIM/RUIM Phonebook EF_IAP.
   *
   * @see TS 131.102, clause 4.4.2.13
   *
   * @param fileId       EF id of the IAP.
   * @param recordNumber The identifier of the record shall be updated.
   * @param iap          The IAP value to be written.
   * @param onsuccess    Callback to be called when success.
   * @param onerror      Callback to be called when error.
   */
  updateIAP: function updateIAP(fileId, recordNumber, iap, onsuccess, onerror) {
    let dataWriter = function dataWriter(recordSize) {
      // Write String length
      let strLen = recordSize * 2;
      Buf.writeInt32(strLen);

      for (let i = 0; i < iap.length; i++) {
        GsmPDUHelper.writeHexOctet(iap[i]);
      }

      Buf.writeStringDelimiter(strLen);
    }.bind(this);

    ICCIOHelper.updateLinearFixedEF({fileId: fileId,
                                     recordNumber: recordNumber,
                                     dataWriter: dataWriter,
                                     callback: onsuccess,
                                     onerror: onerror});
  },

  /**
   * Cache EF_Email record size.
   */
  _emailRecordSize: null,

  /**
   * Read USIM/RUIM Phonebook EF_EMAIL.
   *
   * @see TS 131.102, clause 4.4.2.13
   *
   * @param fileId       EF id of the EMAIL.
   * @param fileType     The type of the EMAIL, one of the ICC_USIM_TYPE* constants.
   * @param recordNumber The number of the record shall be loaded.
   * @param onsuccess    Callback to be called when success.
   * @param onerror      Callback to be called when error.
   */
  readEmail: function readEmail(fileId, fileType, recordNumber, onsuccess, onerror) {
    function callback(options) {
      let strLen = Buf.readInt32();
      let octetLen = strLen / 2;
      let email = null;
      this._emailRecordSize = options.recordSize;

      // Read contact's email
      //
      // | Byte     | Description                 | Length | M/O
      // | 1 ~ X    | E-mail Address              |   X    |  M
      // | X+1      | ADN file SFI                |   1    |  C
      // | X+2      | ADN file Record Identifier  |   1    |  C
      // Note: The fields marked as C above are mandatort if the file
      //       is not type 1 (as specified in EF_PBR)
      if (fileType == ICC_USIM_TYPE1_TAG) {
        email = ICCPDUHelper.read8BitUnpackedToString(octetLen);
      } else {
        email = ICCPDUHelper.read8BitUnpackedToString(octetLen - 2);

        // Consumes the remaining buffer
        Buf.seekIncoming(2 * Buf.PDU_HEX_OCTET_SIZE); // For ADN SFI and Record Identifier
      }

      Buf.readStringDelimiter(strLen);

      if (onsuccess) {
        onsuccess(email);
      }
    }

    ICCIOHelper.loadLinearFixedEF({fileId: fileId,
                                   recordNumber: recordNumber,
                                   recordSize: this._emailRecordSize,
                                   callback: callback.bind(this),
                                   onerror: onerror});
  },

  /**
   * Update USIM/RUIM Phonebook EF_EMAIL.
   *
   * @see TS 131.102, clause 4.4.2.13
   *
   * @param pbr          Phonebook Reference File.
   * @param recordNumber The identifier of the record shall be updated.
   * @param email        The value to be written.
   * @param adnRecordId  The record Id of ADN, only needed if the fileType of Email is TYPE2.
   * @param onsuccess    Callback to be called when success.
   * @param onerror      Callback to be called when error.
   */
  updateEmail: function updateEmail(pbr, recordNumber, email, adnRecordId, onsuccess, onerror) {
    let fileId = pbr[USIM_PBR_EMAIL].fileId;
    let fileType = pbr[USIM_PBR_EMAIL].fileType;
    let dataWriter = function dataWriter(recordSize) {
      // Write String length
      let strLen = recordSize * 2;
      Buf.writeInt32(strLen);

      if (fileType == ICC_USIM_TYPE1_TAG) {
        ICCPDUHelper.writeStringTo8BitUnpacked(recordSize, email);
      } else {
        ICCPDUHelper.writeStringTo8BitUnpacked(recordSize - 2, email);
        GsmPDUHelper.writeHexOctet(pbr.adn.sfi || 0xff);
        GsmPDUHelper.writeHexOctet(adnRecordId);
      }

      Buf.writeStringDelimiter(strLen);
    }.bind(this);

    ICCIOHelper.updateLinearFixedEF({fileId: fileId,
                                     recordNumber: recordNumber,
                                     dataWriter: dataWriter,
                                     callback: onsuccess,
                                     onerror: onerror});
 },

  /**
   * Cache EF_ANR record size.
   */
  _anrRecordSize: null,

  /**
   * Read USIM/RUIM Phonebook EF_ANR.
   *
   * @see TS 131.102, clause 4.4.2.9
   *
   * @param fileId       EF id of the ANR.
   * @param fileType     One of the ICC_USIM_TYPE* constants.
   * @param recordNumber The number of the record shall be loaded.
   * @param onsuccess    Callback to be called when success.
   * @param onerror      Callback to be called when error.
   */
  readANR: function readANR(fileId, fileType, recordNumber, onsuccess, onerror) {
    function callback(options) {
      let strLen = Buf.readInt32();
      let number = null;
      this._anrRecordSize = options.recordSize;

      // Skip EF_AAS Record ID.
      Buf.seekIncoming(1 * Buf.PDU_HEX_OCTET_SIZE);

      number = ICCPDUHelper.readNumberWithLength();

      // Skip 2 unused octets, CCP and EXT1.
      Buf.seekIncoming(2 * Buf.PDU_HEX_OCTET_SIZE);

      // For Type 2 there are two extra octets.
      if (fileType == ICC_USIM_TYPE2_TAG) {
        // Skip 2 unused octets, ADN SFI and Record Identifier.
        Buf.seekIncoming(2 * Buf.PDU_HEX_OCTET_SIZE);
      }

      Buf.readStringDelimiter(strLen);

      if (onsuccess) {
        onsuccess(number);
      }
    }

    ICCIOHelper.loadLinearFixedEF({fileId: fileId,
                                   recordNumber: recordNumber,
                                   recordSize: this._anrRecordSize,
                                   callback: callback.bind(this),
                                   onerror: onerror});
  },

  /**
   * Update USIM/RUIM Phonebook EF_ANR.
   *
   * @see TS 131.102, clause 4.4.2.9
   *
   * @param pbr          Phonebook Reference File.
   * @param recordNumber The identifier of the record shall be updated.
   * @param number       The value to be written.
   * @param adnRecordId  The record Id of ADN, only needed if the fileType of Email is TYPE2.
   * @param onsuccess    Callback to be called when success.
   * @param onerror      Callback to be called when error.
   */
  updateANR: function updateANR(pbr, recordNumber, number, adnRecordId, onsuccess, onerror) {
    let fileId = pbr[USIM_PBR_ANR0].fileId;
    let fileType = pbr[USIM_PBR_ANR0].fileType;
    let dataWriter = function dataWriter(recordSize) {
      // Write String length
      let strLen = recordSize * 2;
      Buf.writeInt32(strLen);

      // EF_AAS record Id. Unused for now.
      GsmPDUHelper.writeHexOctet(0xff);

      ICCPDUHelper.writeNumberWithLength(number);

      // Write unused octets 0xff, CCP and EXT1.
      GsmPDUHelper.writeHexOctet(0xff);
      GsmPDUHelper.writeHexOctet(0xff);

      // For Type 2 there are two extra octets.
      if (fileType == ICC_USIM_TYPE2_TAG) {
        GsmPDUHelper.writeHexOctet(pbr.adn.sfi || 0xff);
        GsmPDUHelper.writeHexOctet(adnRecordId);
      }

      Buf.writeStringDelimiter(strLen);
    }.bind(this);

    ICCIOHelper.updateLinearFixedEF({fileId: fileId,
                                     recordNumber: recordNumber,
                                     dataWriter: dataWriter,
                                     callback: onsuccess,
                                     onerror: onerror});
  },

  /**
   * Find free record id.
   *
   * @param fileId      EF id.
   * @param onsuccess   Callback to be called when success.
   * @param onerror     Callback to be called when error.
   */
  findFreeRecordId: function findFreeRecordId(fileId, onsuccess, onerror) {
    function callback(options) {
      let strLen = Buf.readInt32();
      let octetLen = strLen / 2;
      let readLen = 0;

      while (readLen < octetLen) {
        let octet = GsmPDUHelper.readHexOctet();
        readLen++;
        if (octet != 0xff) {
          break;
        }
      }

      if (readLen == octetLen) {
        // Find free record.
        if (onsuccess) {
          onsuccess(options.p1);
        }
        return;
      } else {
        Buf.seekIncoming((octetLen - readLen) * Buf.PDU_HEX_OCTET_SIZE);
      }

      Buf.readStringDelimiter(strLen);

      if (options.p1 < options.totalRecords) {
        ICCIOHelper.loadNextRecord(options);
      } else {
        // No free record found.
        if (DEBUG) {
          debug(CONTACT_ERR_NO_FREE_RECORD_FOUND);
        }
        onerror(CONTACT_ERR_NO_FREE_RECORD_FOUND);
      }
    }

    ICCIOHelper.loadLinearFixedEF({fileId: fileId,
                                   callback: callback.bind(this),
                                   onerror: onerror});
  },
};

/**
 * Helper for (U)SIM Records.
 */
let SimRecordHelper = {
  /**
   * Fetch (U)SIM records.
   */
  fetchSimRecords: function fetchSimRecords() {
    RIL.getIMSI();
    this.readAD();
    this.readSST();
  },

  /**
   * Read EF_phase.
   * This EF is only available in SIM.
   */
  readSimPhase: function readSimPhase() {
    function callback() {
      let strLen = Buf.readInt32();

      let phase = GsmPDUHelper.readHexOctet();
      // If EF_phase is coded '03' or greater, an ME supporting STK shall
      // perform the PROFILE DOWNLOAD procedure.
      if (RILQUIRKS_SEND_STK_PROFILE_DOWNLOAD &&
          phase >= ICC_PHASE_2_PROFILE_DOWNLOAD_REQUIRED) {
        RIL.sendStkTerminalProfile(STK_SUPPORTED_TERMINAL_PROFILE);
      }

      Buf.readStringDelimiter(strLen);
    }

    ICCIOHelper.loadTransparentEF({fileId: ICC_EF_PHASE,
                                   callback: callback.bind(this)});
  },

  /**
   * Read the MSISDN from the (U)SIM.
   */
  readMSISDN: function readMSISDN() {
    function callback(options) {
      let contact = ICCPDUHelper.readAlphaIdDiallingNumber(options.recordSize);
      if (!contact ||
          (RIL.iccInfo.msisdn !== undefined &&
           RIL.iccInfo.msisdn === contact.number)) {
        return;
      }
      RIL.iccInfo.msisdn = contact.number;
      if (DEBUG) debug("MSISDN: " + RIL.iccInfo.msisdn);
      ICCUtilsHelper.handleICCInfoChange();
    }

    ICCIOHelper.loadLinearFixedEF({fileId: ICC_EF_MSISDN,
                                   callback: callback.bind(this)});
  },

  /**
   * Read the AD (Administrative Data) from the (U)SIM.
   */
  readAD: function readAD() {
    function callback() {
      let strLen = Buf.readInt32();
      // Each octet is encoded into two chars.
      let octetLen = strLen / 2;
      let ad = GsmPDUHelper.readHexOctetArray(octetLen);
      Buf.readStringDelimiter(strLen);

      if (DEBUG) {
        let str = "";
        for (let i = 0; i < ad.length; i++) {
          str += ad[i] + ", ";
        }
        debug("AD: " + str);
      }

      // The 4th byte of the response is the length of MNC.
      let mccMnc = ICCUtilsHelper.parseMccMncFromImsi(RIL.iccInfoPrivate.imsi,
                                                      ad && ad[3]);
      if (mccMnc) {
        RIL.iccInfo.mcc = mccMnc.mcc;
        RIL.iccInfo.mnc = mccMnc.mnc;
        ICCUtilsHelper.handleICCInfoChange();
      }
    }

    ICCIOHelper.loadTransparentEF({fileId: ICC_EF_AD,
                                   callback: callback.bind(this)});
  },

  /**
   * Read the SPN (Service Provider Name) from the (U)SIM.
   */
  readSPN: function readSPN() {
    function callback() {
      let strLen = Buf.readInt32();
      // Each octet is encoded into two chars.
      let octetLen = strLen / 2;
      let spnDisplayCondition = GsmPDUHelper.readHexOctet();
      // Minus 1 because the first octet is used to store display condition.
      let spn = ICCPDUHelper.readAlphaIdentifier(octetLen - 1);
      Buf.readStringDelimiter(strLen);

      if (DEBUG) {
        debug("SPN: spn = " + spn +
              ", spnDisplayCondition = " + spnDisplayCondition);
      }

      RIL.iccInfoPrivate.spnDisplayCondition = spnDisplayCondition;
      RIL.iccInfo.spn = spn;
      ICCUtilsHelper.updateDisplayCondition();
      ICCUtilsHelper.handleICCInfoChange();
    }

    ICCIOHelper.loadTransparentEF({fileId: ICC_EF_SPN,
                                   callback: callback.bind(this)});
  },

  /**
   * Read the (U)SIM Service Table from the (U)SIM.
   */
  readSST: function readSST() {
    function callback() {
      let strLen = Buf.readInt32();
      // Each octet is encoded into two chars.
      let octetLen = strLen / 2;
      let sst = GsmPDUHelper.readHexOctetArray(octetLen);
      Buf.readStringDelimiter(strLen);
      RIL.iccInfoPrivate.sst = sst;
      if (DEBUG) {
        let str = "";
        for (let i = 0; i < sst.length; i++) {
          str += sst[i] + ", ";
        }
        debug("SST: " + str);
      }

      if (ICCUtilsHelper.isICCServiceAvailable("MSISDN")) {
        if (DEBUG) debug("MSISDN: MSISDN is available");
        this.readMSISDN();
      } else {
        if (DEBUG) debug("MSISDN: MSISDN service is not available");
      }

      // Fetch SPN and PLMN list, if some of them are available.
      if (ICCUtilsHelper.isICCServiceAvailable("SPN")) {
        if (DEBUG) debug("SPN: SPN is available");
        this.readSPN();
      } else {
        if (DEBUG) debug("SPN: SPN service is not available");
      }

      if (ICCUtilsHelper.isICCServiceAvailable("MDN")) {
        if (DEBUG) debug("MDN: MDN available.");
        this.readMBDN();
      } else {
        if (DEBUG) debug("MDN: MDN service is not available");
      }

      if (ICCUtilsHelper.isICCServiceAvailable("SPDI")) {
        if (DEBUG) debug("SPDI: SPDI available.");
        this.readSPDI();
      } else {
        if (DEBUG) debug("SPDI: SPDI service is not available");
      }

      if (ICCUtilsHelper.isICCServiceAvailable("PNN")) {
        if (DEBUG) debug("PNN: PNN is available");
        this.readPNN();
      } else {
        if (DEBUG) debug("PNN: PNN is not available");
      }

      if (ICCUtilsHelper.isICCServiceAvailable("OPL")) {
        if (DEBUG) debug("OPL: OPL is available");
        this.readOPL();
      } else {
        if (DEBUG) debug("OPL: OPL is not available");
      }

      if (ICCUtilsHelper.isICCServiceAvailable("CBMI")) {
        this.readCBMI();
      } else {
        RIL.cellBroadcastConfigs.CBMI = null;
      }
      if (ICCUtilsHelper.isICCServiceAvailable("DATA_DOWNLOAD_SMS_CB")) {
        this.readCBMID();
      } else {
        RIL.cellBroadcastConfigs.CBMID = null;
      }
      if (ICCUtilsHelper.isICCServiceAvailable("CBMIR")) {
        this.readCBMIR();
      } else {
        RIL.cellBroadcastConfigs.CBMIR = null;
      }
      RIL._mergeAllCellBroadcastConfigs();
    }

    // ICC_EF_UST has the same value with ICC_EF_SST.
    ICCIOHelper.loadTransparentEF({fileId: ICC_EF_SST,
                                   callback: callback.bind(this)});
  },

  /**
   * Read (U)SIM MBDN. (Mailbox Dialling Number)
   *
   * @see TS 131.102, clause 4.2.60
   */
  readMBDN: function readMBDN() {
    function callback(options) {
      let contact = ICCPDUHelper.readAlphaIdDiallingNumber(options.recordSize);
      if (!contact ||
          (RIL.iccInfoPrivate.mbdn !== undefined &&
           RIL.iccInfoPrivate.mbdn === contact.number)) {
        return;
      }
      RIL.iccInfoPrivate.mbdn = contact.number;
      if (DEBUG) {
        debug("MBDN, alphaId="+contact.alphaId+" number="+contact.number);
      }
      contact.rilMessageType = "iccmbdn";
      RIL.sendChromeMessage(contact);
    }

    ICCIOHelper.loadLinearFixedEF({fileId: ICC_EF_MBDN,
                                   callback: callback.bind(this)});
  },

  /**
   * Read the SPDI (Service Provider Display Information) from the (U)SIM.
   *
   * See TS 131.102 section 4.2.66 for USIM and TS 51.011 section 10.3.50
   * for SIM.
   */
  readSPDI: function readSPDI() {
    function callback() {
      let strLen = Buf.readInt32();
      let octetLen = strLen / 2;
      let readLen = 0;
      let endLoop = false;
      RIL.iccInfoPrivate.SPDI = null;
      while ((readLen < octetLen) && !endLoop) {
        let tlvTag = GsmPDUHelper.readHexOctet();
        let tlvLen = GsmPDUHelper.readHexOctet();
        readLen += 2; // For tag and length fields.
        switch (tlvTag) {
        case SPDI_TAG_SPDI:
          // The value part itself is a TLV.
          continue;
        case SPDI_TAG_PLMN_LIST:
          // This PLMN list is what we want.
          RIL.iccInfoPrivate.SPDI = this.readPLMNEntries(tlvLen / 3);
          readLen += tlvLen;
          endLoop = true;
          break;
        default:
          // We don't care about its content if its tag is not SPDI nor
          // PLMN_LIST.
          endLoop = true;
          break;
        }
      }

      // Consume unread octets.
      Buf.seekIncoming((octetLen - readLen) * Buf.PDU_HEX_OCTET_SIZE);
      Buf.readStringDelimiter(strLen);

      if (DEBUG) debug("SPDI: " + JSON.stringify(RIL.iccInfoPrivate.SPDI));
      if (ICCUtilsHelper.updateDisplayCondition()) {
        ICCUtilsHelper.handleICCInfoChange();
      }
    }

    // PLMN List is Servive 51 in USIM, EF_SPDI
    ICCIOHelper.loadTransparentEF({fileId: ICC_EF_SPDI,
                                   callback: callback.bind(this)});
  },

  _readCbmiHelper: function _readCbmiHelper(which) {
    function callback() {
      let strLength = Buf.readInt32();

      // Each Message Identifier takes two octets and each octet is encoded
      // into two chars.
      let numIds = strLength / 4, list = null;
      if (numIds) {
        list = [];
        for (let i = 0, id; i < numIds; i++) {
          id = GsmPDUHelper.readHexOctet() << 8 | GsmPDUHelper.readHexOctet();
          // `Unused entries shall be set to 'FF FF'.`
          if (id != 0xFFFF) {
            list.push(id);
            list.push(id + 1);
          }
        }
      }
      if (DEBUG) {
        debug(which + ": " + JSON.stringify(list));
      }

      Buf.readStringDelimiter(strLength);

      RIL.cellBroadcastConfigs[which] = list;
      RIL._mergeAllCellBroadcastConfigs();
    }

    function onerror() {
      RIL.cellBroadcastConfigs[which] = null;
      RIL._mergeAllCellBroadcastConfigs();
    }

    let fileId = GLOBAL["ICC_EF_" + which];
    ICCIOHelper.loadTransparentEF({fileId: fileId,
                                   callback: callback.bind(this),
                                   onerror: onerror.bind(this)});
  },

  /**
   * Read EFcbmi (Cell Broadcast Message Identifier selection)
   *
   * @see 3GPP TS 31.102 v110.02.0 section 4.2.14 EFcbmi
   * @see 3GPP TS 51.011 v5.0.0 section 10.3.13 EFcbmi
   */
  readCBMI: function readCBMI() {
    this._readCbmiHelper("CBMI");
  },

  /**
   * Read EFcbmid (Cell Broadcast Message Identifier for Data Download)
   *
   * @see 3GPP TS 31.102 v110.02.0 section 4.2.20 EFcbmid
   * @see 3GPP TS 51.011 v5.0.0 section 10.3.26 EFcbmid
   */
  readCBMID: function readCBMID() {
    this._readCbmiHelper("CBMID");
  },

  /**
   * Read EFcbmir (Cell Broadcast Message Identifier Range selection)
   *
   * @see 3GPP TS 31.102 v110.02.0 section 4.2.22 EFcbmir
   * @see 3GPP TS 51.011 v5.0.0 section 10.3.28 EFcbmir
   */
  readCBMIR: function readCBMIR() {
    function callback() {
      let strLength = Buf.readInt32();

      // Each Message Identifier range takes four octets and each octet is
      // encoded into two chars.
      let numIds = strLength / 8, list = null;
      if (numIds) {
        list = [];
        for (let i = 0, from, to; i < numIds; i++) {
          // `Bytes one and two of each range identifier equal the lower value
          // of a cell broadcast range, bytes three and four equal the upper
          // value of a cell broadcast range.`
          from = GsmPDUHelper.readHexOctet() << 8 | GsmPDUHelper.readHexOctet();
          to = GsmPDUHelper.readHexOctet() << 8 | GsmPDUHelper.readHexOctet();
          // `Unused entries shall be set to 'FF FF'.`
          if ((from != 0xFFFF) && (to != 0xFFFF)) {
            list.push(from);
            list.push(to + 1);
          }
        }
      }
      if (DEBUG) {
        debug("CBMIR: " + JSON.stringify(list));
      }

      Buf.readStringDelimiter(strLength);

      RIL.cellBroadcastConfigs.CBMIR = list;
      RIL._mergeAllCellBroadcastConfigs();
    }

    function onerror() {
      RIL.cellBroadcastConfigs.CBMIR = null;
      RIL._mergeAllCellBroadcastConfigs();
    }

    ICCIOHelper.loadTransparentEF({fileId: ICC_EF_CBMIR,
                                   callback: callback.bind(this),
                                   onerror: onerror.bind(this)});
  },

  /**
   * Read OPL (Operator PLMN List) from (U)SIM.
   *
   * See 3GPP TS 31.102 Sec. 4.2.59 for USIM
   *     3GPP TS 51.011 Sec. 10.3.42 for SIM.
   */
  readOPL: function readOPL() {
    let opl = [];
    function callback(options) {
      let strLen = Buf.readInt32();
      // The first 7 bytes are LAI (for UMTS) and the format of LAI is defined
      // in 3GPP TS 23.003, Sec 4.1
      //    +-------------+---------+
      //    | Octet 1 - 3 | MCC/MNC |
      //    +-------------+---------+
      //    | Octet 4 - 7 |   LAC   |
      //    +-------------+---------+
      let mccMnc = [GsmPDUHelper.readHexOctet(),
                    GsmPDUHelper.readHexOctet(),
                    GsmPDUHelper.readHexOctet()];
      if (mccMnc[0] != 0xFF || mccMnc[1] != 0xFF || mccMnc[2] != 0xFF) {
        let oplElement = {};
        let semiOctets = [];
        for (let i = 0; i < mccMnc.length; i++) {
          semiOctets.push((mccMnc[i] & 0xf0) >> 4);
          semiOctets.push(mccMnc[i] & 0x0f);
        }
        let reformat = [semiOctets[1], semiOctets[0], semiOctets[3],
                        semiOctets[5], semiOctets[4], semiOctets[2]];
        let buf = "";
        for (let i = 0; i < reformat.length; i++) {
          if (reformat[i] != 0xF) {
            buf += GsmPDUHelper.semiOctetToBcdChar(reformat[i]);
          }
          if (i === 2) {
            // 0-2: MCC
            oplElement.mcc = buf;
            buf = "";
          } else if (i === 5) {
            // 3-5: MNC
            oplElement.mnc = buf;
          }
        }
        // LAC/TAC
        oplElement.lacTacStart =
          (GsmPDUHelper.readHexOctet() << 8) | GsmPDUHelper.readHexOctet();
        oplElement.lacTacEnd =
          (GsmPDUHelper.readHexOctet() << 8) | GsmPDUHelper.readHexOctet();
        // PLMN Network Name Record Identifier
        oplElement.pnnRecordId = GsmPDUHelper.readHexOctet();
        if (DEBUG) {
          debug("OPL: [" + (opl.length + 1) + "]: " + JSON.stringify(oplElement));
        }
        opl.push(oplElement);
      } else {
        Buf.seekIncoming(5 * Buf.PDU_HEX_OCTET_SIZE);
      }
      Buf.readStringDelimiter(strLen);

      if (options.p1 < options.totalRecords) {
        ICCIOHelper.loadNextRecord(options);
      } else {
        RIL.iccInfoPrivate.OPL = opl;
      }
    }

    ICCIOHelper.loadLinearFixedEF({fileId: ICC_EF_OPL,
                                   callback: callback.bind(this)});
  },

  /**
   * Read PNN (PLMN Network Name) from (U)SIM.
   *
   * See 3GPP TS 31.102 Sec. 4.2.58 for USIM
   *     3GPP TS 51.011 Sec. 10.3.41 for SIM.
   */
  readPNN: function readPNN() {
    function callback(options) {
      let pnnElement;
      let strLen = Buf.readInt32();
      let octetLen = strLen / 2;
      let readLen = 0;

      while (readLen < octetLen) {
        let tlvTag = GsmPDUHelper.readHexOctet();

        if (tlvTag == 0xFF) {
          // Unused byte
          readLen++;
          Buf.seekIncoming((octetLen - readLen) * Buf.PDU_HEX_OCTET_SIZE);
          break;
        }

        // Needs this check to avoid initializing twice.
        pnnElement = pnnElement || {};

        let tlvLen = GsmPDUHelper.readHexOctet();

        switch (tlvTag) {
          case PNN_IEI_FULL_NETWORK_NAME:
            pnnElement.fullName = GsmPDUHelper.readNetworkName(tlvLen);
            break;
          case PNN_IEI_SHORT_NETWORK_NAME:
            pnnElement.shortName = GsmPDUHelper.readNetworkName(tlvLen);
            break;
          default:
            Buf.seekIncoming(tlvLen * Buf.PDU_HEX_OCTET_SIZE);
            break;
        }

        readLen += (tlvLen + 2); // +2 for tlvTag and tlvLen
      }
      Buf.readStringDelimiter(strLen);

      if (pnnElement) {
        pnn.push(pnnElement);
      }

      // Will ignore remaining records when got the contents of a record are all 0xff.
      if (pnnElement && options.p1 < options.totalRecords) {
        ICCIOHelper.loadNextRecord(options);
      } else {
        if (DEBUG) {
          for (let i = 0; i < pnn.length; i++) {
            debug("PNN: [" + i + "]: " + JSON.stringify(pnn[i]));
          }
        }
        RIL.iccInfoPrivate.PNN = pnn;
      }
    }

    let pnn = [];
    ICCIOHelper.loadLinearFixedEF({fileId: ICC_EF_PNN,
                                   callback: callback.bind(this)});
  },

  /**
   *  Read the list of PLMN (Public Land Mobile Network) entries
   *  We cannot directly rely on readSwappedNibbleBcdToString(),
   *  since it will no correctly handle some corner-cases that are
   *  not a problem in our case (0xFF 0xFF 0xFF).
   *
   *  @param length The number of PLMN records.
   *  @return An array of string corresponding to the PLMNs.
   */
  readPLMNEntries: function readPLMNEntries(length) {
    let plmnList = [];
    // Each PLMN entry has 3 bytes.
    if (DEBUG) debug("readPLMNEntries: PLMN entries length = " + length);
    let index = 0;
    while (index < length) {
      // Unused entries will be 0xFFFFFF, according to EF_SPDI
      // specs (TS 131 102, section 4.2.66)
      try {
        let plmn = [GsmPDUHelper.readHexOctet(),
                    GsmPDUHelper.readHexOctet(),
                    GsmPDUHelper.readHexOctet()];
        if (DEBUG) debug("readPLMNEntries: Reading PLMN entry: [" + index +
                         "]: '" + plmn + "'");
        if (plmn[0] != 0xFF &&
            plmn[1] != 0xFF &&
            plmn[2] != 0xFF) {
          let semiOctets = [];
          for (let idx = 0; idx < plmn.length; idx++) {
            semiOctets.push((plmn[idx] & 0xF0) >> 4);
            semiOctets.push(plmn[idx] & 0x0F);
          }

          // According to TS 24.301, 9.9.3.12, the semi octets is arranged
          // in format:
          // Byte 1: MCC[2] | MCC[1]
          // Byte 2: MNC[3] | MCC[3]
          // Byte 3: MNC[2] | MNC[1]
          // Therefore, we need to rearrange them.
          let reformat = [semiOctets[1], semiOctets[0], semiOctets[3],
                          semiOctets[5], semiOctets[4], semiOctets[2]];
          let buf = "";
          let plmnEntry = {};
          for (let i = 0; i < reformat.length; i++) {
            if (reformat[i] != 0xF) {
              buf += GsmPDUHelper.semiOctetToBcdChar(reformat[i]);
            }
            if (i === 2) {
              // 0-2: MCC
              plmnEntry.mcc = buf;
              buf = "";
            } else if (i === 5) {
              // 3-5: MNC
              plmnEntry.mnc = buf;
            }
          }
          if (DEBUG) debug("readPLMNEntries: PLMN = " + plmnEntry.mcc + ", " + plmnEntry.mnc);
          plmnList.push(plmnEntry);
        }
      } catch (e) {
        if (DEBUG) debug("readPLMNEntries: PLMN entry " + index + " is invalid.");
        break;
      }
      index ++;
    }
    return plmnList;
  },

  /**
   * Read the SMS from the ICC.
   *
   * @param recordNumber The number of the record shall be loaded.
   * @param onsuccess    Callback to be called when success.
   * @param onerror      Callback to be called when error.
   */
  readSMS: function readSMS(recordNumber, onsuccess, onerror) {
    function callback(options) {
      let strLen = Buf.readInt32();

      // TS 51.011, 10.5.3 EF_SMS
      // b3 b2 b1
      //  0  0  1 message received by MS from network; message read
      //  0  1  1 message received by MS from network; message to be read
      //  1  1  1 MS originating message; message to be sent
      //  1  0  1 MS originating message; message sent to the network:
      let status = GsmPDUHelper.readHexOctet();

      let message = GsmPDUHelper.readMessage();
      message.simStatus = status;

      // Consumes the remaining buffer
      Buf.seekIncoming(Buf.getReadAvailable() - Buf.PDU_HEX_OCTET_SIZE);

      Buf.readStringDelimiter(strLen);

      if (message) {
        onsuccess(message);
      } else {
        onerror("Failed to decode SMS on SIM #" + recordNumber);
      }
    }

    ICCIOHelper.loadLinearFixedEF({fileId: ICC_EF_SMS,
                                   recordNumber: recordNumber,
                                   callback: callback,
                                   onerror: onerror});
  },
};

let RuimRecordHelper = {
  fetchRuimRecords: function fetchRuimRecords() {
    this.getIMSI_M();
    this.readCST();
    this.readCDMAHome();
    RIL.getCdmaSubscription();
  },

  /**
   * Get IMSI_M from CSIM/RUIM.
   * See 3GPP2 C.S0065 Sec. 5.2.2
   */
  getIMSI_M: function getIMSI_M() {
    function callback() {
      let strLen = Buf.readInt32();
      let encodedImsi = GsmPDUHelper.readHexOctetArray(strLen / 2);
      Buf.readStringDelimiter(strLen);

      if ((encodedImsi[CSIM_IMSI_M_PROGRAMMED_BYTE] & 0x80)) { // IMSI_M programmed
        RIL.iccInfoPrivate.imsi = this.decodeIMSI(encodedImsi);
        RIL.sendChromeMessage({rilMessageType: "iccimsi",
                               imsi: RIL.iccInfoPrivate.imsi});

        let mccMnc = ICCUtilsHelper.parseMccMncFromImsi(RIL.iccInfoPrivate.imsi);
        if (mccMnc) {
          RIL.iccInfo.mcc = mccMnc.mcc;
          RIL.iccInfo.mnc = mccMnc.mnc;
          ICCUtilsHelper.handleICCInfoChange();
        }
      }
    }

    ICCIOHelper.loadTransparentEF({fileId: ICC_EF_CSIM_IMSI_M,
                                   callback: callback.bind(this)});
  },

  /**
   * Decode IMSI from IMSI_M
   * See 3GPP2 C.S0005 Sec. 2.3.1
   * +---+---------+------------+---+--------+---------+---+---------+--------+
   * |RFU|   MCC   | programmed |RFU|  MNC   |  MIN1   |RFU|   MIN2  |  CLASS |
   * +---+---------+------------+---+--------+---------+---+---------+--------+
   * | 6 | 10 bits |   8 bits   | 1 | 7 bits | 24 bits | 6 | 10 bits | 8 bits |
   * +---+---------+------------+---+--------+---------+---+---------+--------+
   */
  decodeIMSI: function decodeIMSI(encodedImsi) {
    // MCC: 10 bits, 3 digits
    let encodedMCC = ((encodedImsi[CSIM_IMSI_M_MCC_BYTE + 1] & 0x03) << 8) +
                      (encodedImsi[CSIM_IMSI_M_MCC_BYTE] & 0xff);
    let mcc = this.decodeIMSIValue(encodedMCC, 3);

    // MNC: 7 bits, 2 digits
    let encodedMNC =  encodedImsi[CSIM_IMSI_M_MNC_BYTE] & 0x7f;
    let mnc = this.decodeIMSIValue(encodedMNC, 2);

    // MIN2: 10 bits, 3 digits
    let encodedMIN2 = ((encodedImsi[CSIM_IMSI_M_MIN2_BYTE + 1] & 0x03) << 8) +
                       (encodedImsi[CSIM_IMSI_M_MIN2_BYTE] & 0xff);
    let min2 = this.decodeIMSIValue(encodedMIN2, 3);

    // MIN1: 10+4+10 bits, 3+1+3 digits
    let encodedMIN1First3 = ((encodedImsi[CSIM_IMSI_M_MIN1_BYTE + 2] & 0xff) << 2) +
                             ((encodedImsi[CSIM_IMSI_M_MIN1_BYTE + 1] & 0xc0) >> 6);
    let min1First3 = this.decodeIMSIValue(encodedMIN1First3, 3);

    let encodedFourthDigit = (encodedImsi[CSIM_IMSI_M_MIN1_BYTE + 1] & 0x3c) >> 2;
    if (encodedFourthDigit > 9) {
      encodedFourthDigit = 0;
    }
    let fourthDigit = encodedFourthDigit.toString();

    let encodedMIN1Last3 = ((encodedImsi[CSIM_IMSI_M_MIN1_BYTE + 1] & 0x03) << 8) +
                            (encodedImsi[CSIM_IMSI_M_MIN1_BYTE] & 0xff);
    let min1Last3 = this.decodeIMSIValue(encodedMIN1Last3, 3);

    return mcc + mnc + min2 + min1First3 + fourthDigit + min1Last3;
  },

  /**
   * Decode IMSI Helper function
   * See 3GPP2 C.S0005 section 2.3.1.1
   */
  decodeIMSIValue: function decodeIMSIValue(encoded, length) {
    let offset = length === 3 ? 111 : 11;
    let value = encoded + offset;

    for (let base = 10, temp = value, i = 0; i < length; i++) {
      if (temp % 10 === 0) {
        value -= base;
      }
      temp = Math.floor(value / base);
      base = base * 10;
    }

    let s = value.toString();
    while (s.length < length) {
      s = "0" + s;
    }

    return s;
  },

  /**
   * Read CDMAHOME for CSIM.
   * See 3GPP2 C.S0023 Sec. 3.4.8.
   */
  readCDMAHome: function readCDMAHome() {
    function callback(options) {
      let strLen = Buf.readInt32();
      let tempOctet = GsmPDUHelper.readHexOctet();
      cdmaHomeSystemId.push(((GsmPDUHelper.readHexOctet() & 0x7f) << 8) | tempOctet);
      tempOctet = GsmPDUHelper.readHexOctet();
      cdmaHomeNetworkId.push(((GsmPDUHelper.readHexOctet() & 0xff) << 8) | tempOctet);

      // Consuming the last octet: band class.
      Buf.seekIncoming(Buf.PDU_HEX_OCTET_SIZE);

      Buf.readStringDelimiter(strLen);
      if (options.p1 < options.totalRecords) {
        ICCIOHelper.loadNextRecord(options);
      } else {
        if (DEBUG) {
          debug("CDMAHome system id: " + JSON.stringify(cdmaHomeSystemId));
          debug("CDMAHome network id: " + JSON.stringify(cdmaHomeNetworkId));
        }
        RIL.cdmaHome = {
          systemId: cdmaHomeSystemId,
          networkId: cdmaHomeNetworkId
        };
      }
    }

    let cdmaHomeSystemId = [], cdmaHomeNetworkId = [];
    ICCIOHelper.loadLinearFixedEF({fileId: ICC_EF_CSIM_CDMAHOME,
                                   callback: callback.bind(this)});
  },

  /**
   * Read CDMA Service Table.
   * See 3GPP2 C.S0023 Sec. 3.4.18
   */
  readCST: function readCST() {
    function callback() {
      let strLen = Buf.readInt32();
      // Each octet is encoded into two chars.
      RIL.iccInfoPrivate.cst = GsmPDUHelper.readHexOctetArray(strLen / 2);
      Buf.readStringDelimiter(strLen);

      if (DEBUG) {
        let str = "";
        for (let i = 0; i < RIL.iccInfoPrivate.cst.length; i++) {
          str += RIL.iccInfoPrivate.cst[i] + ", ";
        }
        debug("CST: " + str);
      }

      if (ICCUtilsHelper.isICCServiceAvailable("SPN")) {
        if (DEBUG) debug("SPN: SPN is available");
        this.readSPN();
      }
    }
    ICCIOHelper.loadTransparentEF({fileId: ICC_EF_CSIM_CST,
                                   callback: callback.bind(this)});
  },

  readSPN: function readSPN() {
    function callback() {
      let strLen = Buf.readInt32();
      let octetLen = strLen / 2;
      let displayCondition = GsmPDUHelper.readHexOctet();
      let codingScheme = GsmPDUHelper.readHexOctet();
      // Skip one octet: language indicator.
      Buf.seekIncoming(Buf.PDU_HEX_OCTET_SIZE);
      let readLen = 3;

      // SPN String ends up with 0xff.
      let userDataBuffer = [];

      while (readLen < octetLen) {
        let octet = GsmPDUHelper.readHexOctet();
        readLen++;
        if (octet == 0xff) {
          break;
        }
        userDataBuffer.push(octet);
      }

      BitBufferHelper.startRead(userDataBuffer);

      let msgLen;
      switch (CdmaPDUHelper.getCdmaMsgEncoding(codingScheme)) {
      case PDU_DCS_MSG_CODING_7BITS_ALPHABET:
        msgLen = Math.floor(userDataBuffer.length * 8 / 7);
        break;
      case PDU_DCS_MSG_CODING_8BITS_ALPHABET:
        msgLen = userDataBuffer.length;
        break;
      case PDU_DCS_MSG_CODING_16BITS_ALPHABET:
        msgLen = Math.floor(userDataBuffer.length / 2);
        break;
      }

      RIL.iccInfo.spn = CdmaPDUHelper.decodeCdmaPDUMsg(codingScheme, null, msgLen);
      if (DEBUG) {
        debug("CDMA SPN: " + RIL.iccInfo.spn +
              ", Display condition: " + displayCondition);
      }
      RIL.iccInfoPrivate.spnDisplayCondition = displayCondition;
      Buf.seekIncoming((octetLen - readLen) * Buf.PDU_HEX_OCTET_SIZE);
      Buf.readStringDelimiter(strLen);
    }

    ICCIOHelper.loadTransparentEF({fileId: ICC_EF_CSIM_SPN,
                                   callback: callback.bind(this)});
  }
};

/**
 * Helper functions for ICC utilities.
 */
let ICCUtilsHelper = {
  /**
   * Get network names by using EF_OPL and EF_PNN
   *
   * @See 3GPP TS 31.102 sec. 4.2.58 and sec. 4.2.59 for USIM,
   *      3GPP TS 51.011 sec. 10.3.41 and sec. 10.3.42 for SIM.
   *
   * @param mcc   The mobile country code of the network.
   * @param mnc   The mobile network code of the network.
   * @param lac   The location area code of the network.
   */
  getNetworkNameFromICC: function getNetworkNameFromICC(mcc, mnc, lac) {
    let iccInfoPriv = RIL.iccInfoPrivate;
    let iccInfo = RIL.iccInfo;
    let pnnEntry;

    if (!mcc || !mnc || !lac) {
      return null;
    }

    // We won't get network name if there is no PNN file.
    if (!iccInfoPriv.PNN) {
      return null;
    }

    if (!iccInfoPriv.OPL) {
      // When OPL is not present:
      // According to 3GPP TS 31.102 Sec. 4.2.58 and 3GPP TS 51.011 Sec. 10.3.41,
      // If EF_OPL is not present, the first record in this EF is used for the
      // default network name when registered to the HPLMN.
      // If we haven't get pnnEntry assigned, we should try to assign default
      // value to it.
      if (mcc == iccInfo.mcc && mnc == iccInfo.mnc) {
        pnnEntry = iccInfoPriv.PNN[0];
      }
    } else {
      // According to 3GPP TS 31.102 Sec. 4.2.59 and 3GPP TS 51.011 Sec. 10.3.42,
      // the ME shall use this EF_OPL in association with the EF_PNN in place
      // of any network name stored within the ME's internal list and any network
      // name received when registered to the PLMN.
      let length = iccInfoPriv.OPL ? iccInfoPriv.OPL.length : 0;
      for (let i = 0; i < length; i++) {
        let opl = iccInfoPriv.OPL[i];
        // Try to match the MCC/MNC.
        if (mcc != opl.mcc || mnc != opl.mnc) {
          continue;
        }
        // Try to match the location area code. If current local area code is
        // covered by lac range that specified in the OPL entry, use the PNN
        // that specified in the OPL entry.
        if ((opl.lacTacStart === 0x0 && opl.lacTacEnd == 0xFFFE) ||
            (opl.lacTacStart <= lac && opl.lacTacEnd >= lac)) {
          if (opl.pnnRecordId === 0) {
            // See 3GPP TS 31.102 Sec. 4.2.59 and 3GPP TS 51.011 Sec. 10.3.42,
            // A value of '00' indicates that the name is to be taken from other
            // sources.
            return null;
          }
          pnnEntry = iccInfoPriv.PNN[opl.pnnRecordId - 1];
          break;
        }
      }
    }

    if (!pnnEntry) {
      return null;
    }

    // Return a new object to avoid global variable, PNN, be modified by accident.
    return { fullName: pnnEntry.fullName || "",
             shortName: pnnEntry.shortName || "" };
  },

  /**
   * This will compute the spnDisplay field of the network.
   * See TS 22.101 Annex A and TS 51.011 10.3.11 for details.
   *
   * @return True if some of iccInfo is changed in by this function.
   */
  updateDisplayCondition: function updateDisplayCondition() {
    // If EFspn isn't existed in SIM or it haven't been read yet, we should
    // just set isDisplayNetworkNameRequired = true and
    // isDisplaySpnRequired = false
    let iccInfo = RIL.iccInfo;
    let iccInfoPriv = RIL.iccInfoPrivate;
    let displayCondition = iccInfoPriv.spnDisplayCondition;
    let origIsDisplayNetworkNameRequired = iccInfo.isDisplayNetworkNameRequired;
    let origIsDisplaySPNRequired = iccInfo.isDisplaySpnRequired;

    if (displayCondition === undefined) {
      iccInfo.isDisplayNetworkNameRequired = true;
      iccInfo.isDisplaySpnRequired = false;
    } else if (RIL._isCdma) {
      // CDMA family display rule.
      let cdmaHome = RIL.cdmaHome;
      let cell = RIL.voiceRegistrationState.cell;
      let sid = cell && cell.cdmaSystemId;
      let nid = cell && cell.cdmaNetworkId;

      iccInfo.isDisplayNetworkNameRequired = false;

      // If display condition is 0x0, we don't even need to check network id
      // or system id.
      if (displayCondition === 0x0) {
        iccInfo.isDisplaySpnRequired = false;
      } else {
        // CDMA SPN Display condition dosen't specify whenever network name is
        // reqired.
        if (!cdmaHome ||
            !cdmaHome.systemId ||
            cdmaHome.systemId.length === 0 ||
            cdmaHome.systemId.length != cdmaHome.networkId.length ||
            !sid || !nid) {
          // CDMA Home haven't been ready, or we haven't got the system id and
          // network id of the network we register to, assuming we are in home
          // network.
          iccInfo.isDisplaySpnRequired = true;
        } else {
          // Determine if we are registered in the home service area.
          // System ID and Network ID are described in 3GPP2 C.S0005 Sec. 2.6.5.2.
          let inHomeArea = false;
          for (let i = 0; i < cdmaHome.systemId.length; i++) {
            let homeSid = cdmaHome.systemId[i],
                homeNid = cdmaHome.networkId[i];
            if (homeSid === 0 || homeNid === 0 // Reserved system id/network id
               || homeSid != sid) {
              continue;
            }
            // According to 3GPP2 C.S0005 Sec. 2.6.5.2, NID number 65535 means
            // all networks in the system should be considered as home.
            if (homeNid == 65535 || homeNid == nid) {
              inHomeArea = true;
              break;
            }
          }
          iccInfo.isDisplaySpnRequired = inHomeArea;
        }
      }
    } else {
      // GSM family display rule.
      let operatorMnc = RIL.operator.mnc;
      let operatorMcc = RIL.operator.mcc;

      // First detect if we are on HPLMN or one of the PLMN
      // specified by the SIM card.
      let isOnMatchingPlmn = false;

      // If the current network is the one defined as mcc/mnc
      // in SIM card, it's okay.
      if (iccInfo.mcc == operatorMcc && iccInfo.mnc == operatorMnc) {
        isOnMatchingPlmn = true;
      }

      // Test to see if operator's mcc/mnc match mcc/mnc of PLMN.
      if (!isOnMatchingPlmn && iccInfoPriv.SPDI) {
        let iccSpdi = iccInfoPriv.SPDI; // PLMN list
        for (let plmn in iccSpdi) {
          let plmnMcc = iccSpdi[plmn].mcc;
          let plmnMnc = iccSpdi[plmn].mnc;
          isOnMatchingPlmn = (plmnMcc == operatorMcc) && (plmnMnc == operatorMnc);
          if (isOnMatchingPlmn) {
            break;
          }
        }
      }

      if (isOnMatchingPlmn) {
        // The first bit of display condition tells us if we should display
        // registered PLMN.
        if (DEBUG) debug("updateDisplayCondition: PLMN is HPLMN or PLMN is in PLMN list");

        // TS 31.102 Sec. 4.2.66 and TS 51.011 Sec. 10.3.50
        // EF_SPDI contains a list of PLMNs in which the Service Provider Name
        // shall be displayed.
        iccInfo.isDisplaySpnRequired = true;
        iccInfo.isDisplayNetworkNameRequired = (displayCondition & 0x01) !== 0;
      } else {
        // The second bit of display condition tells us if we should display
        // registered PLMN.
        if (DEBUG) debug("updateICCDisplayName: PLMN isn't HPLMN and PLMN isn't in PLMN list");

        // We didn't found the requirement of displaying network name if
        // current PLMN isn't HPLMN nor one of PLMN in SPDI. So we keep
        // isDisplayNetworkNameRequired false.
        iccInfo.isDisplayNetworkNameRequired = false;
        iccInfo.isDisplaySpnRequired = (displayCondition & 0x02) === 0;
      }
    }

    if (DEBUG) {
      debug("updateDisplayCondition: isDisplayNetworkNameRequired = " + iccInfo.isDisplayNetworkNameRequired);
      debug("updateDisplayCondition: isDisplaySpnRequired = " + iccInfo.isDisplaySpnRequired);
    }

    return ((origIsDisplayNetworkNameRequired !== iccInfo.isDisplayNetworkNameRequired) ||
            (origIsDisplaySPNRequired !== iccInfo.isDisplaySpnRequired));
  },

  decodeSimTlvs: function decodeSimTlvs(tlvsLen) {
    let index = 0;
    let tlvs = [];
    while (index < tlvsLen) {
      let simTlv = {
        tag : GsmPDUHelper.readHexOctet(),
        length : GsmPDUHelper.readHexOctet(),
      };
      simTlv.value = GsmPDUHelper.readHexOctetArray(simTlv.length);
      tlvs.push(simTlv);
      index += simTlv.length + 2; // The length of 'tag' and 'length' field.
    }
    return tlvs;
  },

  /**
   * Parse those TLVs and convert it to an object.
   */
  parsePbrTlvs: function parsePbrTlvs(pbrTlvs) {
    let pbr = {};
    for (let i = 0; i < pbrTlvs.length; i++) {
      let pbrTlv = pbrTlvs[i];
      let anrIndex = 0;
      for (let j = 0; j < pbrTlv.value.length; j++) {
        let tlv = pbrTlv.value[j];
        let tagName = USIM_TAG_NAME[tlv.tag];

        // ANR could have multiple files. We save it as anr0, anr1,...etc.
        if (tlv.tag == ICC_USIM_EFANR_TAG) {
          tagName += anrIndex;
          anrIndex++;
        }
        pbr[tagName] = tlv;
        pbr[tagName].fileType = pbrTlv.tag;
        pbr[tagName].fileId = (tlv.value[0] << 8) | tlv.value[1];
        pbr[tagName].sfi = tlv.value[2];

        // For Type 2, the order of files is in the same order in IAP.
        if (pbrTlv.tag == ICC_USIM_TYPE2_TAG) {
          pbr[tagName].indexInIAP = j;
        }
      }
    }

    return pbr;
  },

  /**
   * Update the ICC information to RadioInterfaceLayer.
   */
  handleICCInfoChange: function handleICCInfoChange() {
    RIL.iccInfo.rilMessageType = "iccinfochange";
    RIL.sendChromeMessage(RIL.iccInfo);
  },

  /**
   * Get whether specificed (U)SIM service is available.
   *
   * @param geckoService
   *        Service name like "ADN", "BDN", etc.
   *
   * @return true if the service is enabled, false otherwise.
   */
  isICCServiceAvailable: function isICCServiceAvailable(geckoService) {
    let serviceTable = RIL._isCdma ? RIL.iccInfoPrivate.cst:
                                     RIL.iccInfoPrivate.sst;
    let index, bitmask;
    if (RIL.appType == CARD_APPTYPE_SIM || RIL.appType == CARD_APPTYPE_RUIM) {
      /**
       * Service id is valid in 1..N, and 2 bits are used to code each service.
       *
       * +----+--  --+----+----+
       * | b8 | ...  | b2 | b1 |
       * +----+--  --+----+----+
       *
       * b1 = 0, service not allocated.
       *      1, service allocated.
       * b2 = 0, service not activatd.
       *      1, service allocated.
       *
       * @see 3GPP TS 51.011 10.3.7.
       */
      let simService;
      if (RIL.appType == CARD_APPTYPE_SIM) {
        simService = GECKO_ICC_SERVICES.sim[geckoService];
      } else {
        simService = GECKO_ICC_SERVICES.ruim[geckoService];
      }
      if (!simService) {
        return false;
      }
      simService -= 1;
      index = Math.floor(simService / 4);
      bitmask = 2 << ((simService % 4) << 1);
    } else if (RIL.appType == CARD_APPTYPE_USIM) {
      /**
       * Service id is valid in 1..N, and 1 bit is used to code each service.
       *
       * +----+--  --+----+----+
       * | b8 | ...  | b2 | b1 |
       * +----+--  --+----+----+
       *
       * b1 = 0, service not avaiable.
       *      1, service available.
       * b2 = 0, service not avaiable.
       *      1, service available.
       *
       * @see 3GPP TS 31.102 4.2.8.
       */
      let usimService = GECKO_ICC_SERVICES.usim[geckoService];
      if (!usimService) {
        return false;
      }
      usimService -= 1;
      index = Math.floor(usimService / 8);
      bitmask = 1 << ((usimService % 8) << 0);
    }

    return (serviceTable !== null) &&
           (index < serviceTable.length) &&
           ((serviceTable[index] & bitmask) !== 0);
  },

  /**
   * Check if the string is of GSM default 7-bit coded alphabets with bit 8
   * set to 0.
   *
   * @param str  String to be checked.
   */
  isGsm8BitAlphabet: function isGsm8BitAlphabet(str) {
    if (!str) {
      return false;
    }

    const langTable = PDU_NL_LOCKING_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];
    const langShiftTable = PDU_NL_SINGLE_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];

    for (let i = 0; i < str.length; i++) {
      let c = str.charAt(i);
      let octet = langTable.indexOf(c);
      if (octet == -1) {
        octet = langShiftTable.indexOf(c);
        if (octet == -1) {
          return false;
        }
      }
    }

    return true;
  },

  /**
   * Parse MCC/MNC from IMSI. If there is no available value for the length of
   * mnc, it will use the data in MCC table to parse.
   *
   * @param imsi
   *        The imsi of icc.
   * @param mncLength [optional]
   *        The length of mnc.
   *
   * @return An object contains the parsing result of mcc and mnc.
   *         Or null if any error occurred.
   */
  parseMccMncFromImsi: function parseMccMncFromImsi(imsi, mncLength) {
    if (!imsi) {
      return null;
    }

    // MCC is the first 3 digits of IMSI.
    let mcc = imsi.substr(0,3);
    if (!mncLength) {
      // Check the MCC table to decide the length of MNC.
      let index = MCC_TABLE_FOR_MNC_LENGTH_IS_3.indexOf(mcc);
      mncLength = (index !== -1) ? 3 : 2;
    }
    let mnc = imsi.substr(3, mncLength);
    if (DEBUG) debug("IMSI: " + imsi + " MCC: " + mcc + " MNC: " + mnc);

    return { mcc: mcc, mnc: mnc};
  },
};

/**
 * Helper for ICC Contacts.
 */
let ICCContactHelper = {
  /**
   * Helper function to check DF_PHONEBOOK.
   */
  hasDfPhoneBook: function hasDfPhoneBook(appType) {
    switch (appType) {
      case CARD_APPTYPE_SIM:
        return false;
      case CARD_APPTYPE_USIM:
        return true;
      case CARD_APPTYPE_RUIM:
        return ICCUtilsHelper.isICCServiceAvailable("ENHANCED_PHONEBOOK");
      default:
        return false;
    }
  },

  /**
   * Helper function to read ICC contacts.
   *
   * @param appType       One of CARD_APPTYPE_*.
   * @param contactType   "adn" or "fdn".
   * @param onsuccess     Callback to be called when success.
   * @param onerror       Callback to be called when error.
   */
  readICCContacts: function readICCContacts(appType, contactType, onsuccess, onerror) {
    switch (contactType) {
      case "adn":
        if (!this.hasDfPhoneBook(appType)) {
          ICCRecordHelper.readADNLike(ICC_EF_ADN, onsuccess, onerror);
        } else {
          this.readUSimContacts(onsuccess, onerror);
        }
        break;
      case "fdn":
        ICCRecordHelper.readADNLike(ICC_EF_FDN, onsuccess, onerror);
        break;
      default:
        if (DEBUG) {
          debug("Unsupported contactType :" + contactType);
        }
        onerror(CONTACT_ERR_CONTACT_TYPE_NOT_SUPPORTED);
        break;
    }
  },

  /**
   * Helper function to find free contact record.
   *
   * @param appType       One of CARD_APPTYPE_*.
   * @param contactType   "adn" or "fdn".
   * @param onsuccess     Callback to be called when success.
   * @param onerror       Callback to be called when error.
   */
  findFreeICCContact: function findFreeICCContact(appType, contactType, onsuccess, onerror) {
    switch (contactType) {
      case "adn":
        if (!this.hasDfPhoneBook(appType)) {
          ICCRecordHelper.findFreeRecordId(ICC_EF_ADN, onsuccess.bind(null, 0), onerror);
        } else {
          let gotPbrCb = function gotPbrCb(pbrs) {
            this.findUSimFreeADNRecordId(pbrs, onsuccess, onerror);
          }.bind(this);

          ICCRecordHelper.readPBR(gotPbrCb, onerror);
        }
        break;
      case "fdn":
        ICCRecordHelper.findFreeRecordId(ICC_EF_FDN, onsuccess.bind(null, 0), onerror);
        break;
      default:
        if (DEBUG) {
          debug("Unsupported contactType :" + contactType);
        }
        onerror(CONTACT_ERR_CONTACT_TYPE_NOT_SUPPORTED);
        break;
    }
  },

   /**
    * Find free ADN record id in USIM.
    *
    * @param pbrs          All Phonebook Reference Files read.
    * @param onsuccess     Callback to be called when success.
    * @param onerror       Callback to be called when error.
    */
  findUSimFreeADNRecordId: function findUSimFreeADNRecordId(pbrs, onsuccess, onerror) {
    (function findFreeRecordId(pbrIndex) {
      if (pbrIndex >= pbrs.length) {
        if (DEBUG) {
          debug(CONTACT_ERR_NO_FREE_RECORD_FOUND);
        }
        onerror(CONTACT_ERR_NO_FREE_RECORD_FOUND);
        return;
      }

      let pbr = pbrs[pbrIndex];
      ICCRecordHelper.findFreeRecordId(
        pbr.adn.fileId,
        onsuccess.bind(this, pbrIndex),
        findFreeRecordId.bind(null, pbrIndex + 1));
    })(0);
  },

  /**
   * Helper function to add a new ICC contact.
   *
   * @param appType       One of CARD_APPTYPE_*.
   * @param contactType   "adn" or "fdn".
   * @param contact       The contact will be added.
   * @param pin2          PIN2 is required for FDN.
   * @param onsuccess     Callback to be called when success.
   * @param onerror       Callback to be called when error.
   */
  addICCContact: function addICCContact(appType, contactType, contact, pin2, onsuccess, onerror) {
    let foundFreeCb = function foundFreeCb(pbrIndex, recordId) {
      contact.pbrIndex = pbrIndex;
      contact.recordId = recordId;
      ICCContactHelper.updateICCContact(appType, contactType, contact, pin2, onsuccess, onerror);
    };

    // Find free record first.
    ICCContactHelper.findFreeICCContact(appType, contactType, foundFreeCb, onerror);
  },

  /**
   * Helper function to update ICC contact.
   *
   * @param appType       One of CARD_APPTYPE_*.
   * @param contactType   "adn" or "fdn".
   * @param contact       The contact will be updated.
   * @param pin2          PIN2 is required for FDN.
   * @param onsuccess     Callback to be called when success.
   * @param onerror       Callback to be called when error.
   */
  updateICCContact: function updateICCContact(appType, contactType, contact, pin2, onsuccess, onerror) {
    switch (contactType) {
      case "adn":
        if (!this.hasDfPhoneBook(appType)) {
          ICCRecordHelper.updateADNLike(ICC_EF_ADN, contact, null, onsuccess, onerror);
        } else {
          this.updateUSimContact(contact, onsuccess, onerror);
        }
        break;
      case "fdn":
        if (!pin2) {
          onerror(GECKO_ERROR_SIM_PIN2);
          return;
        }
        ICCRecordHelper.updateADNLike(ICC_EF_FDN, contact, pin2, onsuccess, onerror);
        break;
      default:
        if (DEBUG) {
          debug("Unsupported contactType :" + contactType);
        }
        onerror(CONTACT_ERR_CONTACT_TYPE_NOT_SUPPORTED);
        break;
    }
  },

  /**
   * Read contacts from USIM.
   *
   * @param onsuccess     Callback to be called when success.
   * @param onerror       Callback to be called when error.
   */
  readUSimContacts: function readUSimContacts(onsuccess, onerror) {
    let gotPbrCb = function gotPbrCb(pbrs) {
      this.readAllPhonebookSets(pbrs, onsuccess, onerror);
    }.bind(this);

    ICCRecordHelper.readPBR(gotPbrCb, onerror);
  },

  /**
   * Read all Phonebook sets.
   *
   * @param pbrs          All Phonebook Reference Files read.
   * @param onsuccess     Callback to be called when success.
   * @param onerror       Callback to be called when error.
   */
  readAllPhonebookSets: function readAllPhonebookSets(pbrs, onsuccess, onerror) {
    let allContacts = [], pbrIndex = 0;
    let readPhonebook = function readPhonebook(contacts) {
      if (contacts) {
        allContacts = allContacts.concat(contacts);
      }

      let cLen = contacts ? contacts.length : 0;
      for (let i = 0; i < cLen; i++) {
        contacts[i].pbrIndex = pbrIndex;
      }

      pbrIndex++;
      if (pbrIndex >= pbrs.length) {
        if (onsuccess) {
          onsuccess(allContacts);
        }
        return;
      }

      this.readPhonebookSet(pbrs[pbrIndex], readPhonebook, onerror);
    }.bind(this);

    this.readPhonebookSet(pbrs[pbrIndex], readPhonebook, onerror);
  },

  /**
   * Read from Phonebook Reference File.
   *
   * @param pbr           Phonebook Reference File to be read.
   * @param onsuccess     Callback to be called when success.
   * @param onerror       Callback to be called when error.
   */
  readPhonebookSet: function readPhonebookSet(pbr, onsuccess, onerror) {
    let gotAdnCb = function gotAdnCb(contacts) {
      this.readSupportedPBRFields(pbr, contacts, onsuccess, onerror);
    }.bind(this);

    ICCRecordHelper.readADNLike(pbr.adn.fileId, gotAdnCb, onerror);
  },

  /**
   * Read supported Phonebook fields.
   *
   * @param pbr         Phone Book Reference file.
   * @param contacts    Contacts stored on ICC.
   * @param onsuccess   Callback to be called when success.
   * @param onerror     Callback to be called when error.
   */
  readSupportedPBRFields: function readSupportedPBRFields(pbr, contacts, onsuccess, onerror) {
    let fieldIndex = 0;
    (function readField() {
      let field = USIM_PBR_FIELDS[fieldIndex];
      fieldIndex += 1;
      if (!field) {
        if (onsuccess) {
          onsuccess(contacts);
        }
        return;
      }

      ICCContactHelper.readPhonebookField(pbr, contacts, field, readField, onerror);
    })();
  },

  /**
   * Read Phonebook field.
   *
   * @param pbr           The phonebook reference file.
   * @param contacts      Contacts stored on ICC.
   * @param field         Phonebook field to be retrieved.
   * @param onsuccess     Callback to be called when success.
   * @param onerror       Callback to be called when error.
   */
  readPhonebookField: function readPhonebookField(pbr, contacts, field, onsuccess, onerror) {
    if (!pbr[field]) {
      if (onsuccess) {
        onsuccess(contacts);
      }
      return;
    }

    (function doReadContactField(n) {
      if (n >= contacts.length) {
        // All contact's fields are read.
        if (onsuccess) {
          onsuccess(contacts);
        }
        return;
      }

      // get n-th contact's field.
      ICCContactHelper.readContactField(
        pbr, contacts[n], field, doReadContactField.bind(this, n + 1), onerror);
    })(0);
  },

  /**
   * Read contact's field from USIM.
   *
   * @param pbr           The phonebook reference file.
   * @param contact       The contact needs to get field.
   * @param field         Phonebook field to be retrieved.
   * @param onsuccess     Callback to be called when success.
   * @param onerror       Callback to be called when error.
   */
  readContactField: function readContactField(pbr, contact, field, onsuccess, onerror) {
    let gotRecordIdCb = function gotRecordIdCb(recordId) {
      if (recordId == 0xff) {
        if (onsuccess) {
          onsuccess();
        }
        return;
      }

      let fileId = pbr[field].fileId;
      let fileType = pbr[field].fileType;
      let gotFieldCb = function gotFieldCb(value) {
        if (value) {
          // Move anr0 anr1,.. into anr[].
          if (field.startsWith(USIM_PBR_ANR)) {
            if (!contact[USIM_PBR_ANR]) {
              contact[USIM_PBR_ANR] = [];
            }
            contact[USIM_PBR_ANR].push(value);
          } else {
            contact[field] = value;
          }
        }

        if (onsuccess) {
          onsuccess();
        }
      }.bind(this);

      // Detect EF to be read, for anr, it could have anr0, anr1,...
      let ef = field.startsWith(USIM_PBR_ANR) ? USIM_PBR_ANR : field;
      switch (ef) {
        case USIM_PBR_EMAIL:
          ICCRecordHelper.readEmail(fileId, fileType, recordId, gotFieldCb, onerror);
          break;
        case USIM_PBR_ANR:
          ICCRecordHelper.readANR(fileId, fileType, recordId, gotFieldCb, onerror);
          break;
        default:
          if (DEBUG) {
            debug("Unsupported field :" + field);
          }
          onerror(CONTACT_ERR_FIELD_NOT_SUPPORTED);
          break;
      }
    }.bind(this);

    this.getContactFieldRecordId(pbr, contact, field, gotRecordIdCb, onerror);
  },

  /**
   * Get the recordId.
   *
   * If the fileType of field is ICC_USIM_TYPE1_TAG, use corresponding ADN recordId.
   * otherwise get the recordId from IAP.
   *
   * @see TS 131.102, clause 4.4.2.2
   *
   * @param pbr          The phonebook reference file.
   * @param contact      The contact will be updated.
   * @param onsuccess    Callback to be called when success.
   * @param onerror      Callback to be called when error.
   */
  getContactFieldRecordId: function getContactFieldRecordId(pbr, contact, field, onsuccess, onerror) {
    if (pbr[field].fileType == ICC_USIM_TYPE1_TAG) {
      // If the file type is ICC_USIM_TYPE1_TAG, use corresponding ADN recordId.
      if (onsuccess) {
        onsuccess(contact.recordId);
      }
    } else if (pbr[field].fileType == ICC_USIM_TYPE2_TAG) {
      // If the file type is ICC_USIM_TYPE2_TAG, the recordId shall be got from IAP.
      let gotIapCb = function gotIapCb(iap) {
        let indexInIAP = pbr[field].indexInIAP;
        let recordId = iap[indexInIAP];

        if (onsuccess) {
          onsuccess(recordId);
        }
      }.bind(this);

      ICCRecordHelper.readIAP(pbr.iap.fileId, contact.recordId, gotIapCb, onerror);
    } else {
      if (DEBUG) {
        debug("USIM PBR files in Type 3 format are not supported.");
      }
      onerror(CONTACT_ERR_REQUEST_NOT_SUPPORTED);
    }
  },

  /**
   * Update USIM contact.
   *
   * @param contact       The contact will be updated.
   * @param onsuccess     Callback to be called when success.
   * @param onerror       Callback to be called when error.
   */
  updateUSimContact: function updateUSimContact(contact, onsuccess, onerror) {
    let gotPbrCb = function gotPbrCb(pbrs) {
      let pbr = pbrs[contact.pbrIndex];
      if (!pbr) {
        if (DEBUG) {
          debug(CONTACT_ERR_CANNOT_ACCESS_PHONEBOOK);
        }
        onerror(CONTACT_ERR_CANNOT_ACCESS_PHONEBOOK);
        return;
      }
      this.updatePhonebookSet(pbr, contact, onsuccess, onerror);
    }.bind(this);

    ICCRecordHelper.readPBR(gotPbrCb, onerror);
  },

  /**
   * Update fields in Phonebook Reference File.
   *
   * @param pbr           Phonebook Reference File to be read.
   * @param onsuccess     Callback to be called when success.
   * @param onerror       Callback to be called when error.
   */
  updatePhonebookSet: function updatePhonebookSet(pbr, contact, onsuccess, onerror) {
    let updateAdnCb = function () {
      this.updateSupportedPBRFields(pbr, contact, onsuccess, onerror);
    }.bind(this);

    ICCRecordHelper.updateADNLike(pbr.adn.fileId, contact, null, updateAdnCb, onerror);
  },

  /**
   * Update supported Phonebook fields.
   *
   * @param pbr         Phone Book Reference file.
   * @param contact     Contact to be updated.
   * @param onsuccess   Callback to be called when success.
   * @param onerror     Callback to be called when error.
   */
  updateSupportedPBRFields: function updateSupportedPBRFields(pbr, contact, onsuccess, onerror) {
    let fieldIndex = 0;
    (function updateField() {
      let field = USIM_PBR_FIELDS[fieldIndex];
      fieldIndex += 1;
      if (!field) {
        if (onsuccess) {
          onsuccess();
        }
        return;
      }

      // Check if PBR has this field.
      if (!pbr[field]) {
        updateField();
        return;
      }

      // Check if contact has additional properties (email, anr, ...etc) that
      // need to be updated as well.
      if ((field === USIM_PBR_EMAIL && !contact.email) ||
          (field === USIM_PBR_ANR0 && (!Array.isArray(contact.anr) ||
                                       !contact.anr[0]))) {
        updateField();
        return;
      }

      ICCContactHelper.updateContactField(pbr, contact, field, updateField, onerror);
    })();
  },

  /**
   * Update contact's field from USIM.
   *
   * @param pbr           The phonebook reference file.
   * @param contact       The contact needs to be updated.
   * @param field         Phonebook field to be updated.
   * @param onsuccess     Callback to be called when success.
   * @param onerror       Callback to be called when error.
   */
  updateContactField: function updateContactField(pbr, contact, field, onsuccess, onerror) {
    if (pbr[field].fileType === ICC_USIM_TYPE1_TAG) {
      this.updateContactFieldType1(pbr, contact, field, onsuccess, onerror);
    } else if (pbr[field].fileType === ICC_USIM_TYPE2_TAG) {
      this.updateContactFieldType2(pbr, contact, field, onsuccess, onerror);
    } else {
      if (DEBUG) {
        debug("USIM PBR files in Type 3 format are not supported.");
      }
      onerror(CONTACT_ERR_REQUEST_NOT_SUPPORTED);
    }
  },

  /**
   * Update Type 1 USIM contact fields.
   *
   * @param pbr           The phonebook reference file.
   * @param contact       The contact needs to be updated.
   * @param field         Phonebook field to be updated.
   * @param onsuccess     Callback to be called when success.
   * @param onerror       Callback to be called when error.
   */
  updateContactFieldType1: function updateContactFieldType1(pbr, contact, field, onsuccess, onerror) {
    if (field === USIM_PBR_EMAIL) {
      ICCRecordHelper.updateEmail(pbr, contact.recordId, contact.email, null, onsuccess, onerror);
    } else if (field === USIM_PBR_ANR0) {
      ICCRecordHelper.updateANR(pbr, contact.recordId, contact.anr[0], null, onsuccess, onerror);
    } else {
     if (DEBUG) {
       debug("Unsupported field :" + field);
     }
     onerror(CONTACT_ERR_FIELD_NOT_SUPPORTED);
    }
  },

  /**
   * Update Type 2 USIM contact fields.
   *
   * @param pbr           The phonebook reference file.
   * @param contact       The contact needs to be updated.
   * @param field         Phonebook field to be updated.
   * @param onsuccess     Callback to be called when success.
   * @param onerror       Callback to be called when error.
   */
  updateContactFieldType2: function updateContactFieldType2(pbr, contact, field, onsuccess, onerror) {
    // Case 1 : EF_IAP[adnRecordId] doesn't have a value(0xff)
    //   Find a free recordId for EF_field
    //   Update field with that free recordId.
    //   Update IAP.
    //
    // Case 2: EF_IAP[adnRecordId] has a value
    //   update EF_field[iap[field.indexInIAP]]

    let gotIapCb = function gotIapCb(iap) {
      let recordId = iap[pbr[field].indexInIAP];
      if (recordId === 0xff) {
        // Case 1.
        this.addContactFieldType2(pbr, contact, field, onsuccess, onerror);
        return;
      }

      // Case 2.
      if (field === USIM_PBR_EMAIL) {
        ICCRecordHelper.updateEmail(pbr, recordId, contact.email, contact.recordId, onsuccess, onerror);
      } else if (field === USIM_PBR_ANR0) {
        ICCRecordHelper.updateANR(pbr, recordId, contact.anr[0], contact.recordId, onsuccess, onerror);
      } else {
        if (DEBUG) {
          debug("Unsupported field :" + field);
        }
        onerror(CONTACT_ERR_FIELD_NOT_SUPPORTED);
      }

    }.bind(this);

    ICCRecordHelper.readIAP(pbr.iap.fileId, contact.recordId, gotIapCb, onerror);
  },

  /**
   * Add Type 2 USIM contact fields.
   *
   * @param pbr           The phonebook reference file.
   * @param contact       The contact needs to be updated.
   * @param field         Phonebook field to be updated.
   * @param onsuccess     Callback to be called when success.
   * @param onerror       Callback to be called when error.
   */
  addContactFieldType2: function addContactFieldType2(pbr, contact, field, onsuccess, onerror) {
    let successCb = function successCb(recordId) {
      let updateCb = function updateCb() {
        this.updateContactFieldIndexInIAP(pbr, contact.recordId, field, recordId, onsuccess, onerror);
      }.bind(this);

      if (field === USIM_PBR_EMAIL) {
        ICCRecordHelper.updateEmail(pbr, recordId, contact.email, contact.recordId, updateCb, onerror);
      } else if (field === USIM_PBR_ANR0) {
        ICCRecordHelper.updateANR(pbr, recordId, contact.anr[0], contact.recordId, updateCb, onerror);
      }
    }.bind(this);

    let errorCb = function errorCb(errorMsg) {
      if (DEBUG) {
        debug(errorMsg + " USIM field " + field);
      }
      onerror(errorMsg);
    }.bind(this);

    ICCRecordHelper.findFreeRecordId(pbr[field].fileId, successCb, errorCb);
  },

  /**
   * Update IAP value.
   *
   * @param pbr           The phonebook reference file.
   * @param recordNumber  The record identifier of EF_IAP.
   * @param field         Phonebook field.
   * @param value         The value of 'field' in IAP.
   * @param onsuccess     Callback to be called when success.
   * @param onerror       Callback to be called when error.
   *
   */
  updateContactFieldIndexInIAP: function updateContactFieldIndexInIAP(pbr, recordNumber, field, value, onsuccess, onerror) {
    let gotIAPCb = function gotIAPCb(iap) {
      iap[pbr[field].indexInIAP] = value;
      ICCRecordHelper.updateIAP(pbr.iap.fileId, recordNumber, iap, onsuccess, onerror);
    }.bind(this);
    ICCRecordHelper.readIAP(pbr.iap.fileId, recordNumber, gotIAPCb, onerror);
  },
};

/**
 * Global stuff.
 */

// Initialize buffers. This is a separate function so that unit tests can
// re-initialize the buffers at will.
Buf.init();

function onRILMessage(data) {
  Buf.processIncoming(data);
}

onmessage = function onmessage(event) {
  RIL.handleChromeMessage(event.data);
};

onerror = function onerror(event) {
  debug("RIL Worker error" + event.message + "\n");
};
