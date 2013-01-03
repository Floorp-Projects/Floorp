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

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/DOMRequestHelper.jsm");

var RIL = {};
Cu.import("resource://gre/modules/ril_consts.js", RIL);

// set to true to in ril_consts.js to see debug messages
var DEBUG = RIL.DEBUG_CONTENT_HELPER;

// Read debug setting from pref
try {
  let debugPref = Services.prefs.getBoolPref("ril.debugging.enabled");
  DEBUG = RIL.DEBUG_CONTENT_HELPER || debugPref;
} catch (e) {};

const RILCONTENTHELPER_CID =
  Components.ID("{472816e1-1fd6-4405-996c-806f9ea68174}");
const MOBILEICCINFO_CID =
  Components.ID("{8649c12f-f8f4-4664-bbdd-7d115c23e2a7}");
const MOBILECONNECTIONINFO_CID =
  Components.ID("{a35cfd39-2d93-4489-ac7d-396475dacb27}");
const MOBILENETWORKINFO_CID =
  Components.ID("{a6c8416c-09b4-46d1-bf29-6520d677d085}");
const MOBILECELLINFO_CID =
  Components.ID("{5e809018-68c0-4c54-af0b-2a9b8f748c45}");
const VOICEMAILSTATUS_CID=
  Components.ID("{5467f2eb-e214-43ea-9b89-67711241ec8e}");
const MOBILECFINFO_CID=
  Components.ID("{a4756f16-e728-4d9f-8baa-8464f894888a}");
const CELLBROADCASTMESSAGE_CID =
  Components.ID("{29474c96-3099-486f-bb4a-3c9a1da834e4}");
const CELLBROADCASTETWSINFO_CID =
  Components.ID("{59f176ee-9dcd-4005-9d47-f6be0cd08e17}");

const RIL_IPC_MSG_NAMES = [
  "RIL:CardStateChanged",
  "RIL:IccInfoChanged",
  "RIL:VoiceInfoChanged",
  "RIL:DataInfoChanged",
  "RIL:EnumerateCalls",
  "RIL:GetAvailableNetworks",
  "RIL:NetworkSelectionModeChanged",
  "RIL:SelectNetwork",
  "RIL:SelectNetworkAuto",
  "RIL:CallStateChanged",
  "RIL:VoicemailNotification",
  "RIL:VoicemailInfoChanged",
  "RIL:CallError",
  "RIL:CardLockResult",
  "RIL:USSDReceived",
  "RIL:SendMMI:Return:OK",
  "RIL:SendMMI:Return:KO",
  "RIL:CancelMMI:Return:OK",
  "RIL:CancelMMI:Return:KO",
  "RIL:StkCommand",
  "RIL:StkSessionEnd",
  "RIL:DataError",
  "RIL:SetCallForwardingOption",
  "RIL:GetCallForwardingOption",
  "RIL:CellBroadcastReceived"
];

const kVoiceChangedTopic     = "mobile-connection-voice-changed";
const kDataChangedTopic      = "mobile-connection-data-changed";
const kCardStateChangedTopic = "mobile-connection-cardstate-changed";
const kIccInfoChangedTopic   = "mobile-connection-iccinfo-changed";
const kUssdReceivedTopic     = "mobile-connection-ussd-received";
const kStkCommandTopic       = "icc-manager-stk-command";
const kStkSessionEndTopic    = "icc-manager-stk-session-end";
const kDataErrorTopic        = "mobile-connection-data-error";
const kIccCardLockErrorTopic = "mobile-connection-icccardlock-error";

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsISyncMessageSender");

XPCOMUtils.defineLazyServiceGetter(this, "gUUIDGenerator",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");

function MobileICCCardLockResult(options) {
  this.lockType = options.lockType;
  this.enabled = options.enabled;
  this.retryCount = options.retryCount;
  this.success = options.success;
};
MobileICCCardLockResult.prototype = {
  __exposedProps__ : {lockType: 'r',
                      enabled: 'r',
                      retryCount: 'r',
                      success: 'r'}
};

function MobileICCInfo() {
  try {
    this.lastKnownMcc = Services.prefs.getIntPref("ril.lastKnownMcc");
  } catch (e) {}
};
MobileICCInfo.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMMozMobileICCInfo]),
  classID:        MOBILEICCINFO_CID,
  classInfo:      XPCOMUtils.generateCI({
    classID:          MOBILEICCINFO_CID,
    classDescription: "MobileICCInfo",
    flags:            Ci.nsIClassInfo.DOM_OBJECT,
    interfaces:       [Ci.nsIDOMMozMobileICCInfo]
  }),

  // nsIDOMMozMobileICCInfo

  iccid: null,
  mcc: 0,
  lastKnownMcc: 0,
  mnc: 0,
  spn: null,
  msisdn: null
};

function VoicemailInfo() {}
VoicemailInfo.prototype = {
  number: null,
  displayName: null
};

function MobileConnectionInfo() {}
MobileConnectionInfo.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMMozMobileConnectionInfo]),
  classID:        MOBILECONNECTIONINFO_CID,
  classInfo:      XPCOMUtils.generateCI({
    classID:          MOBILECONNECTIONINFO_CID,
    classDescription: "MobileConnectionInfo",
    flags:            Ci.nsIClassInfo.DOM_OBJECT,
    interfaces:       [Ci.nsIDOMMozMobileConnectionInfo]
  }),

  // nsIDOMMozMobileConnectionInfo

  connected: false,
  state: null,
  emergencyCallsOnly: false,
  roaming: false,
  network: null,
  cell: null,
  type: null,
  signalStrength: null,
  relSignalStrength: null
};

function MobileNetworkInfo() {}
MobileNetworkInfo.prototype = {
  __exposedProps__ : {shortName: 'r',
                      longName: 'r',
                      mcc: 'r',
                      mnc: 'r',
                      state: 'r'},

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMMozMobileNetworkInfo]),
  classID:        MOBILENETWORKINFO_CID,
  classInfo:      XPCOMUtils.generateCI({
    classID:          MOBILENETWORKINFO_CID,
    classDescription: "MobileNetworkInfo",
    flags:            Ci.nsIClassInfo.DOM_OBJECT,
    interfaces:       [Ci.nsIDOMMozMobileNetworkInfo]
  }),

  // nsIDOMMozMobileNetworkInfo

  shortName: null,
  longName: null,
  mcc: 0,
  mnc: 0,
  state: null
};

function MobileCellInfo() {}
MobileCellInfo.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMMozMobileCellInfo]),
  classID:        MOBILECELLINFO_CID,
  classInfo:      XPCOMUtils.generateCI({
    classID:          MOBILECELLINFO_CID,
    classDescription: "MobileCellInfo",
    flags:            Ci.nsIClassInfo.DOM_OBJECT,
    interfaces:       [Ci.nsIDOMMozMobileCellInfo]
  }),

  // nsIDOMMozMobileCellInfo

  gsmLocationAreaCode: null,
  gsmCellId: null
};

function VoicemailStatus() {}
VoicemailStatus.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMMozVoicemailStatus]),
  classID:        VOICEMAILSTATUS_CID,
  classInfo:      XPCOMUtils.generateCI({
    classID:          VOICEMAILSTATUS_CID,
    classDescription: "VoicemailStatus",
    flags:            Ci.nsIClassInfo.DOM_OBJECT,
    interfaces:       [Ci.nsIDOMMozVoicemailStatus]
  }),

  // nsIDOMMozVoicemailStatus

  hasMessages: false,
  messageCount: Ci.nsIDOMMozVoicemailStatus.MESSAGE_COUNT_UNKNOWN,
  returnNumber: null,
  returnMessage: null
};

function MobileCFInfo() {}
MobileCFInfo.prototype = {
  __exposedProps__ : {active: 'r',
                      action: 'r',
                      reason: 'r',
                      number: 'r',
                      timeSeconds: 'r',
                      serviceClass: 'r'},
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMMozMobileCFInfo]),
  classID:        MOBILECFINFO_CID,
  classInfo:      XPCOMUtils.generateCI({
    classID:          MOBILECFINFO_CID,
    classDescription: "MobileCFInfo",
    flags:            Ci.nsIClassInfo.DOM_OBJECT,
    interfaces:       [Ci.nsIDOMMozMobileCFInfo]
  }),

  // nsIDOMMozMobileCFInfo

  active: false,
  action: -1,
  reason: -1,
  number: null,
  timeSeconds: 0,
  serviceClass: -1
};

function CellBroadcastMessage(pdu) {
  this.geographicalScope = RIL.CB_GSM_GEOGRAPHICAL_SCOPE_NAMES[pdu.geographicalScope];
  this.messageCode = pdu.messageCode;
  this.messageId = pdu.messageId;
  this.language = pdu.language;
  this.body = pdu.fullBody;
  this.messageClass = pdu.messageClass;
  this.timestamp = new Date(pdu.timestamp);

  if (pdu.etws != null) {
    this.etws = new CellBroadcastEtwsInfo(pdu.etws);
  }
}
CellBroadcastMessage.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMMozCellBroadcastMessage]),
  classID:        CELLBROADCASTMESSAGE_CID,
  classInfo:      XPCOMUtils.generateCI({
    classID:          CELLBROADCASTMESSAGE_CID,
    classDescription: "CellBroadcastMessage",
    flags:            Ci.nsIClassInfo.DOM_OBJECT,
    interfaces:       [Ci.nsIDOMMozCellBroadcastMessage]
  }),

  // nsIDOMMozCellBroadcastMessage

  geographicalScope: null,
  messageCode: null,
  messageId: null,
  language: null,
  body: null,
  messageClass: null,
  timestamp: null,

  etws: null
};

function CellBroadcastEtwsInfo(etwsInfo) {
  if (etwsInfo.warningType != null) {
    this.warningType = RIL.CB_ETWS_WARNING_TYPE_NAMES[etwsInfo.warningType];
  }
  this.emergencyUserAlert = etwsInfo.emergencyUserAlert;
  this.popup = etwsInfo.popup;
}
CellBroadcastEtwsInfo.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMMozCellBroadcastEtwsInfo]),
  classID:        CELLBROADCASTETWSINFO_CID,
  classInfo:      XPCOMUtils.generateCI({
    classID:          CELLBROADCASTETWSINFO_CID,
    classDescription: "CellBroadcastEtwsInfo",
    flags:            Ci.nsIClassInfo.DOM_OBJECT,
    interfaces:       [Ci.nsIDOMMozCellBroadcastEtwsInfo]
  }),

  // nsIDOMMozCellBroadcastEtwsInfo

  warningType: null,
  emergencyUserAlert: null,
  popup: null
};

function RILContentHelper() {
  this.iccInfo = new MobileICCInfo();
  this.voiceConnectionInfo = new MobileConnectionInfo();
  this.dataConnectionInfo = new MobileConnectionInfo();
  this.voicemailInfo = new VoicemailInfo();

  this.initRequests();
  this.initMessageListener(RIL_IPC_MSG_NAMES);
  Services.obs.addObserver(this, "xpcom-shutdown", false);

  // Request initial context.
  let rilContext = cpmm.sendSyncMessage("RIL:GetRilContext")[0];

  if (!rilContext) {
    debug("Received null rilContext from chrome process.");
    return;
  }
  this.cardState = rilContext.cardState;
  this.updateICCInfo(rilContext.icc, this.iccInfo);
  this.updateConnectionInfo(rilContext.voice, this.voiceConnectionInfo);
  this.updateConnectionInfo(rilContext.data, this.dataConnectionInfo);
}

RILContentHelper.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIMobileConnectionProvider,
                                         Ci.nsIRILContentHelper,
                                         Ci.nsIObserver]),
  classID:   RILCONTENTHELPER_CID,
  classInfo: XPCOMUtils.generateCI({classID: RILCONTENTHELPER_CID,
                                    classDescription: "RILContentHelper",
                                    interfaces: [Ci.nsIMobileConnectionProvider,
                                                 Ci.nsIRILContentHelper]}),

  updateVoicemailInfo: function updateVoicemailInfo(srcInfo, destInfo) {
    for (let key in srcInfo) {
      destInfo[key] = srcInfo[key];
    }
  },

  updateICCInfo: function updateICCInfo(srcInfo, destInfo) {
    for (let key in srcInfo) {
      destInfo[key] = srcInfo[key];
      if (key === 'mcc') {
        destInfo['lastKnownMcc'] = srcInfo[key];
      }
    }
  },

  updateConnectionInfo: function updateConnectionInfo(srcInfo, destInfo) {
    for (let key in srcInfo) {
      if ((key != "network") && (key != "cell")) {
        destInfo[key] = srcInfo[key];
      }
    }

    let srcCell = srcInfo.cell;
    if (!srcCell) {
      destInfo.cell = null;
    } else {
      let cell = destInfo.cell;
      if (!cell) {
        cell = destInfo.cell = new MobileCellInfo();
      }

      cell.gsmLocationAreaCode = srcCell.gsmLocationAreaCode;
      cell.gsmCellId = srcCell.gsmCellId;
    }

    let srcNetwork = srcInfo.network;
    if (!srcNetwork) {
      destInfo.network= null;
      return;
    }

    let network = destInfo.network;
    if (!network) {
      network = destInfo.network = new MobileNetworkInfo();
    }

    network.longName = srcNetwork.longName;
    network.shortName = srcNetwork.shortName;
    network.mnc = srcNetwork.mnc;
    network.mcc = srcNetwork.mcc;
  },

  // nsIRILContentHelper

  cardState:            RIL.GECKO_CARDSTATE_UNAVAILABLE,
  iccInfo:              null,
  voiceConnectionInfo:  null,
  dataConnectionInfo:   null,
  networkSelectionMode: RIL.GECKO_NETWORK_SELECTION_UNKNOWN,

  /**
   * The network that is currently trying to be selected (or "automatic").
   * This helps ensure that only one network is selected at a time.
   */
  _selectingNetwork: null,

  getNetworks: function getNetworks(window) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }

    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);

    cpmm.sendAsyncMessage("RIL:GetAvailableNetworks", {requestId: requestId});
    return request;
  },

  selectNetwork: function selectNetwork(window, network) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }

    if (this._selectingNetwork) {
      throw new Error("Already selecting a network: " + this._selectingNetwork);
    }

    if (!network) {
      throw new Error("Invalid network provided: " + network);
    }

    let mnc = network.mnc;
    if (!mnc) {
      throw new Error("Invalid network MNC: " + mnc);
    }

    let mcc = network.mcc;
    if (!mcc) {
      throw new Error("Invalid network MCC: " + mcc);
    }

    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);

    if (this.networkSelectionMode == RIL.GECKO_NETWORK_SELECTION_MANUAL
        && this.voiceConnectionInfo.network === network) {

      // Already manually selected this network, so schedule
      // onsuccess to be fired on the next tick
      this.dispatchFireRequestSuccess(requestId, null);
      return request;
    }

    this._selectingNetwork = network;

    cpmm.sendAsyncMessage("RIL:SelectNetwork", {
      requestId: requestId,
      mnc: mnc,
      mcc: mcc
    });

    return request;
  },

  selectNetworkAutomatically: function selectNetworkAutomatically(window) {

    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }

    if (this._selectingNetwork) {
      throw new Error("Already selecting a network: " + this._selectingNetwork);
    }

    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);

    if (this.networkSelectionMode == RIL.GECKO_NETWORK_SELECTION_AUTOMATIC) {
      // Already using automatic selection mode, so schedule
      // onsuccess to be be fired on the next tick
      this.dispatchFireRequestSuccess(requestId, null);
      return request;
    }

    this._selectingNetwork = "automatic";
    cpmm.sendAsyncMessage("RIL:SelectNetworkAuto", {requestId: requestId});
    return request;
  },

  getCardLock: function getCardLock(window, lockType) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);
    cpmm.sendAsyncMessage("RIL:GetCardLock", {lockType: lockType, requestId: requestId});
    return request;
  },

  unlockCardLock: function unlockCardLock(window, info) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    let request = Services.DOMRequest.createRequest(window);
    info.requestId = this.getRequestId(request);
    cpmm.sendAsyncMessage("RIL:UnlockCardLock", info);
    return request;
  },

  setCardLock: function setCardLock(window, info) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    let request = Services.DOMRequest.createRequest(window);
    info.requestId = this.getRequestId(request);
    cpmm.sendAsyncMessage("RIL:SetCardLock", info);
    return request;
  },

  sendMMI: function sendMMI(window, mmi) {
    debug("Sending MMI " + mmi);
    if (!window) {
      throw Components.Exception("Can't get window object",
                                 Cr.NS_ERROR_EXPECTED);
    }
    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);
    cpmm.sendAsyncMessage("RIL:SendMMI", {mmi: mmi, requestId: requestId});
    return request;
  },

  cancelMMI: function cancelMMI(window) {
    debug("Cancel MMI");
    if (!window) {
      throw Components.Exception("Can't get window object",
                                 Cr.NS_ERROR_UNEXPECTED);
    }
    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);
    cpmm.sendAsyncMessage("RIL:CancelMMI", {requestId: requestId});
    return request;
  },

  sendStkResponse: function sendStkResponse(window, command, response) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    response.command = command;
    cpmm.sendAsyncMessage("RIL:SendStkResponse", response);
  },

  sendStkMenuSelection: function sendStkMenuSelection(window,
                                                      itemIdentifier,
                                                      helpRequested) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    cpmm.sendAsyncMessage("RIL:SendStkMenuSelection", {itemIdentifier: itemIdentifier,
                                                       helpRequested: helpRequested});
  },

  sendStkTimerExpiration: function sendStkTimerExpiration(window,
                                                          timer) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    cpmm.sendAsyncMessage("RIL:SendStkTimerExpiration", {timer: timer});
  },

  sendStkEventDownload: function sendStkEventDownload(window,
                                                      event) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    cpmm.sendAsyncMessage("RIL:SendStkEventDownload", {event: event});
  },

  getCallForwardingOption: function getCallForwardingOption(window, reason) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);

    if (!this._isValidCFReason(reason)){
      this.dispatchFireRequestError(requestId, "Invalid call forwarding reason.");
      return request;
    }

    cpmm.sendAsyncMessage("RIL:GetCallForwardingOption", {
      requestId: requestId,
      reason: reason
    });

   return request;
  },

  setCallForwardingOption: function setCallForwardingOption(window, cfInfo) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);

    if (!cfInfo ||
        !this._isValidCFReason(cfInfo.reason) ||
        !this._isValidCFAction(cfInfo.action)){
      this.dispatchFireRequestError(requestId, "Invalid call forwarding rule definition.");
      return request;
    }

    cpmm.sendAsyncMessage("RIL:SetCallForwardingOption", {
      requestId: requestId,
      active: cfInfo.active,
      action: cfInfo.action,
      reason: cfInfo.reason,
      number: cfInfo.number,
      timeSeconds: cfInfo.timeSeconds
    });

    return request;
  },

  _telephonyCallbacks: null,
  _voicemailCallbacks: null,
  _enumerateTelephonyCallbacks: null,

  voicemailStatus: null,

  getVoicemailInfo: function getVoicemailInfo() {
    // Get voicemail infomation by IPC only on first time.
    this.getVoicemailInfo = function getVoicemailInfo() {
      return this.voicemailInfo;
    };

    let voicemailInfo = cpmm.sendSyncMessage("RIL:GetVoicemailInfo")[0];
    if (voicemailInfo) {
      this.updateVoicemailInfo(voicemailInfo, this.voicemailInfo);
    }

    return this.voicemailInfo;
  },
  get voicemailNumber() {
    return this.getVoicemailInfo().number;
  },
  get voicemailDisplayName() {
    return this.getVoicemailInfo().displayName;
  },

  registerCallback: function registerCallback(callbackType, callback) {
    let callbacks = this[callbackType];
    if (!callbacks) {
      callbacks = this[callbackType] = [];
    }

    if (callbacks.indexOf(callback) != -1) {
      throw new Error("Already registered this callback!");
    }

    callbacks.push(callback);
    if (DEBUG) debug("Registered " + callbackType + " callback: " + callback);
  },

  unregisterCallback: function unregisterCallback(callbackType, callback) {
    let callbacks = this[callbackType];
    if (!callbacks) {
      return;
    }

    let index = callbacks.indexOf(callback);
    if (index != -1) {
      callbacks.splice(index, 1);
      if (DEBUG) debug("Unregistered telephony callback: " + callback);
    }
  },

  registerTelephonyCallback: function registerTelephonyCallback(callback) {
    this.registerCallback("_telephonyCallbacks", callback);
  },

  unregisterTelephonyCallback: function unregisteTelephonyCallback(callback) {
    this.unregisterCallback("_telephonyCallbacks", callback);
  },

  registerVoicemailCallback: function registerVoicemailCallback(callback) {
    this.registerCallback("_voicemailCallbacks", callback);
  },

  unregisterVoicemailCallback: function unregisteVoicemailCallback(callback) {
    this.unregisterCallback("_voicemailCallbacks", callback);
  },

  registerCellBroadcastCallback: function registerCellBroadcastCallback(callback) {
    this.registerCallback("_cellBroadcastCallbacks", callback);
  },

  unregisterCellBroadcastCallback: function unregisterCellBroadcastCallback(callback) {
    this.unregisterCallback("_cellBroadcastCallbacks", callback);
  },

  registerTelephonyMsg: function registerTelephonyMsg() {
    debug("Registering for telephony-related messages");
    cpmm.sendAsyncMessage("RIL:RegisterTelephonyMsg");
  },

  registerMobileConnectionMsg: function registerMobileConnectionMsg() {
    debug("Registering for mobile connection-related messages");
    cpmm.sendAsyncMessage("RIL:RegisterMobileConnectionMsg");
  },

  registerVoicemailMsg: function registerVoicemailMsg() {
    debug("Registering for voicemail-related messages");
    cpmm.sendAsyncMessage("RIL:RegisterVoicemailMsg");
  },

  registerCellBroadcastMsg: function registerCellBroadcastMsg() {
    debug("Registering for Cell Broadcast related messages");
    cpmm.sendAsyncMessage("RIL:RegisterCellBroadcastMsg");
  },

  enumerateCalls: function enumerateCalls(callback) {
    debug("Requesting enumeration of calls for callback: " + callback);
    // We need 'requestId' to meet the 'RILContentHelper <--> RadioInterfaceLayer'
    // protocol.
    let requestId = this._getRandomId();
    cpmm.sendAsyncMessage("RIL:EnumerateCalls", {requestId: requestId});
    if (!this._enumerateTelephonyCallbacks) {
      this._enumerateTelephonyCallbacks = [];
    }
    this._enumerateTelephonyCallbacks.push(callback);
  },

  startTone: function startTone(dtmfChar) {
    debug("Sending Tone for " + dtmfChar);
    cpmm.sendAsyncMessage("RIL:StartTone", dtmfChar);
  },

  stopTone: function stopTone() {
    debug("Stopping Tone");
    cpmm.sendAsyncMessage("RIL:StopTone");
  },

  dial: function dial(number) {
    debug("Dialing " + number);
    cpmm.sendAsyncMessage("RIL:Dial", number);
  },

  dialEmergency: function dialEmergency(number) {
    debug("Dialing emergency " + number);
    cpmm.sendAsyncMessage("RIL:DialEmergency", number);
  },

  hangUp: function hangUp(callIndex) {
    debug("Hanging up call no. " + callIndex);
    cpmm.sendAsyncMessage("RIL:HangUp", callIndex);
  },

  answerCall: function answerCall(callIndex) {
    cpmm.sendAsyncMessage("RIL:AnswerCall", callIndex);
  },

  rejectCall: function rejectCall(callIndex) {
    cpmm.sendAsyncMessage("RIL:RejectCall", callIndex);
  },

  holdCall: function holdCall(callIndex) {
    cpmm.sendAsyncMessage("RIL:HoldCall", callIndex);
  },

  resumeCall: function resumeCall(callIndex) {
    cpmm.sendAsyncMessage("RIL:ResumeCall", callIndex);
  },

  get microphoneMuted() {
    return cpmm.sendSyncMessage("RIL:GetMicrophoneMuted")[0];
  },

  set microphoneMuted(value) {
    cpmm.sendAsyncMessage("RIL:SetMicrophoneMuted", value);
  },

  get speakerEnabled() {
    return cpmm.sendSyncMessage("RIL:GetSpeakerEnabled")[0];
  },

  set speakerEnabled(value) {
    cpmm.sendAsyncMessage("RIL:SetSpeakerEnabled", value);
  },

  // nsIObserver

  observe: function observe(subject, topic, data) {
    if (topic == "xpcom-shutdown") {
      this.removeMessageListener();
      Services.obs.removeObserver(this, "xpcom-shutdown");
      cpmm = null;
    }
  },

  // nsIMessageListener

  fireRequestSuccess: function fireRequestSuccess(requestId, result) {
    let request = this.takeRequest(requestId);
    if (!request) {
      if (DEBUG) {
        debug("not firing success for id: " + requestId +
              ", result: " + JSON.stringify(result));
      }
      return;
    }

    if (DEBUG) {
      debug("fire request success, id: " + requestId +
            ", result: " + JSON.stringify(result));
    }
    Services.DOMRequest.fireSuccess(request, result);
  },

  dispatchFireRequestSuccess: function dispatchFireRequestSuccess(requestId, result) {
    let currentThread = Services.tm.currentThread;

    currentThread.dispatch(this.fireRequestSuccess.bind(this, requestId, result),
                           Ci.nsIThread.DISPATCH_NORMAL);
  },

  fireRequestError: function fireRequestError(requestId, error) {
    let request = this.takeRequest(requestId);
    if (!request) {
      if (DEBUG) {
        debug("not firing error for id: " + requestId +
              ", error: " + JSON.stringify(error));
      }
      return;
    }

    if (DEBUG) {
      debug("fire request error, id: " + requestId +
            ", result: " + JSON.stringify(error));
    }
    Services.DOMRequest.fireError(request, error);
  },

  dispatchFireRequestError: function dispatchFireRequestError(requestId, error) {
    let currentThread = Services.tm.currentThread;

    currentThread.dispatch(this.fireRequestError.bind(this, requestId, error),
                           Ci.nsIThread.DISPATCH_NORMAL);
  },

  receiveMessage: function receiveMessage(msg) {
    let request;
    debug("Received message '" + msg.name + "': " + JSON.stringify(msg.json));
    switch (msg.name) {
      case "RIL:CardStateChanged":
        if (this.cardState != msg.json.cardState) {
          this.cardState = msg.json.cardState;
          Services.obs.notifyObservers(null, kCardStateChangedTopic, null);
        }
        break;
      case "RIL:IccInfoChanged":
        this.updateICCInfo(msg.json, this.iccInfo);
        if (this.iccInfo.mcc) {
          Services.prefs.setIntPref("ril.lastKnownMcc", this.iccInfo.mcc);
        }
        Services.obs.notifyObservers(null, kIccInfoChangedTopic, null);
        break;
      case "RIL:VoiceInfoChanged":
        this.updateConnectionInfo(msg.json, this.voiceConnectionInfo);
        Services.obs.notifyObservers(null, kVoiceChangedTopic, null);
        break;
      case "RIL:DataInfoChanged":
        this.updateConnectionInfo(msg.json, this.dataConnectionInfo);
        Services.obs.notifyObservers(null, kDataChangedTopic, null);
        break;
      case "RIL:EnumerateCalls":
        this.handleEnumerateCalls(msg.json.calls);
        break;
      case "RIL:GetAvailableNetworks":
        this.handleGetAvailableNetworks(msg.json);
        break;
      case "RIL:NetworkSelectionModeChanged":
        this.networkSelectionMode = msg.json.mode;
        break;
      case "RIL:SelectNetwork":
        this.handleSelectNetwork(msg.json,
                                 RIL.GECKO_NETWORK_SELECTION_MANUAL);
        break;
      case "RIL:SelectNetworkAuto":
        this.handleSelectNetwork(msg.json,
                                 RIL.GECKO_NETWORK_SELECTION_AUTOMATIC);
        break;
      case "RIL:CallStateChanged":
        this._deliverCallback("_telephonyCallbacks",
                              "callStateChanged",
                              [msg.json.callIndex, msg.json.state,
                               msg.json.number, msg.json.isActive]);
        break;
      case "RIL:CallError":
        this._deliverCallback("_telephonyCallbacks",
                              "notifyError",
                              [msg.json.callIndex,
                               msg.json.error]);
        break;
      case "RIL:VoicemailNotification":
        this.handleVoicemailNotification(msg.json);
        break;
      case "RIL:VoicemailInfoChanged":
        this.updateVoicemailInfo(msg.json, this.voicemailInfo);
        break;
      case "RIL:CardLockResult":
        if (msg.json.success) {
          let result = new MobileICCCardLockResult(msg.json);
          this.fireRequestSuccess(msg.json.requestId, result);
        } else {
          if (msg.json.rilMessageType == "iccSetCardLock" ||
              msg.json.rilMessageType == "iccUnlockCardLock") {
            let result = JSON.stringify({lockType: msg.json.lockType,
                                         retryCount: msg.json.retryCount});
            Services.obs.notifyObservers(null, kIccCardLockErrorTopic,
                                         result);
          }
          this.fireRequestError(msg.json.requestId, msg.json.errorMsg);
        }
        break;
      case "RIL:USSDReceived":
        let res = JSON.stringify({message: msg.json.message,
                                  sessionEnded: msg.json.sessionEnded});
        Services.obs.notifyObservers(null, kUssdReceivedTopic, res);
        break;
      case "RIL:SendMMI:Return:OK":
      case "RIL:CancelMMI:Return:OK":
        this.handleSendCancelMMIOK(msg.json);
        break;
      case "RIL:SendMMI:Return:KO":
      case "RIL:CancelMMI:Return:KO":
        request = this.takeRequest(msg.json.requestId);
        if (request) {
          Services.DOMRequest.fireError(request, msg.json.errorMsg);
        }
        break;
      case "RIL:StkCommand":
        let jsonString = JSON.stringify(msg.json);
        Services.obs.notifyObservers(null, kStkCommandTopic, jsonString);
        break;
      case "RIL:StkSessionEnd":
        Services.obs.notifyObservers(null, kStkSessionEndTopic, null);
        break;
      case "RIL:DataError":
        this.updateConnectionInfo(msg.json, this.dataConnectionInfo);
        Services.obs.notifyObservers(null, kDataErrorTopic, msg.json.error);
        break;
      case "RIL:GetCallForwardingOption":
        this.handleGetCallForwardingOption(msg.json);
        break;
      case "RIL:SetCallForwardingOption":
        this.handleSetCallForwardingOption(msg.json);
        break;
      case "RIL:CellBroadcastReceived":
        let message = new CellBroadcastMessage(msg.json);
        this._deliverCallback("_cellBroadcastCallbacks",
                              "notifyMessageReceived",
                              [message]);
        break;
    }
  },

  handleEnumerateCalls: function handleEnumerateCalls(calls) {
    debug("handleEnumerateCalls: " + JSON.stringify(calls));
    let callback = this._enumerateTelephonyCallbacks.shift();
    for (let i in calls) {
      let call = calls[i];
      let keepGoing;
      try {
        keepGoing =
          callback.enumerateCallState(call.callIndex, call.state, call.number,
                                      call.isActive);
      } catch (e) {
        debug("callback handler for 'enumerateCallState' threw an " +
              " exception: " + e);
        keepGoing = true;
      }
      if (!keepGoing) {
        break;
      }
    }
  },

  handleGetAvailableNetworks: function handleGetAvailableNetworks(message) {
    debug("handleGetAvailableNetworks: " + JSON.stringify(message));

    let requestId = message.requestId;
    let request = this.takeRequest(requestId);
    if (!request) {
      debug("no DOMRequest found with request ID: " + requestId);
      return;
    }

    if (message.error) {
      debug("Received error from getAvailableNetworks: " + message.error);
      Services.DOMRequest.fireError(request, message.error);
      return;
    }

    let networks = message.networks;
    for (let i = 0; i < networks.length; i++) {
      let network = networks[i];
      let info = new MobileNetworkInfo();

      for (let key in network) {
        info[key] = network[key];
      }

      networks[i] = info;
    }

    Services.DOMRequest.fireSuccess(request, networks);
  },

  handleSelectNetwork: function handleSelectNetwork(message, mode) {
    this._selectingNetwork = null;
    this.networkSelectionMode = mode;

    if (message.error) {
      this.fireRequestError(message.requestId, message.error);
    } else {
      this.fireRequestSuccess(message.requestId, null);
    }
  },

  handleVoicemailNotification: function handleVoicemailNotification(message) {
    let changed = false;
    if (!this.voicemailStatus) {
      this.voicemailStatus = new VoicemailStatus();
    }

    if (this.voicemailStatus.hasMessages != message.active) {
      changed = true;
      this.voicemailStatus.hasMessages = message.active;
    }

    if (this.voicemailStatus.messageCount != message.msgCount) {
      changed = true;
      this.voicemailStatus.messageCount = message.msgCount;
    }

    if (this.voicemailStatus.returnNumber != message.returnNumber) {
      changed = true;
      this.voicemailStatus.returnNumber = message.returnNumber;
    }

    if (this.voicemailStatus.returnMessage != message.returnMessage) {
      changed = true;
      this.voicemailStatus.returnMessage = message.returnMessage;
    }

    if (changed) {
      this._deliverCallback("_voicemailCallbacks",
                            "voicemailNotification",
                            [this.voicemailStatus]);
    }
  },

  _cfRulesToMobileCfInfo: function _cfRulesToMobileCfInfo(rules) {
    for (let i = 0; i < rules.length; i++) {
      let rule = rules[i];
      let info = new MobileCFInfo();

      for (let key in rule) {
        info[key] = rule[key];
      }

      rules[i] = info;
    }
  },

  handleGetCallForwardingOption: function handleGetCallForwardingOption(message) {
    let requestId = message.requestId;
    let request = this.takeRequest(requestId);
    if (!request) {
      return;
    }

    if (!message.success) {
      Services.DOMRequest.fireError(request, message.errorMsg);
      return;
    }

    this._cfRulesToMobileCfInfo(message.rules);
    Services.DOMRequest.fireSuccess(request, message.rules);
  },

  handleSetCallForwardingOption: function handleSetCallForwardingOption(message) {
    let requestId = message.requestId;
    let request = this.takeRequest(requestId);
    if (!request) {
      return;
    }

    if (!message.success) {
      Services.DOMRequest.fireError(request, message.errorMsg);
      return;
    }
    Services.DOMRequest.fireSuccess(request, null);
  },

  handleSendCancelMMIOK: function handleSendCancelMMIOK(message) {
    let request = this.takeRequest(message.requestId);
    if (!request) {
      return;
    }

    // MMI query call forwarding options request returns a set of rules that
    // will be exposed in the form of an array of nsIDOMMozMobileCFInfo
    // instances.
    if (message.success && message.rules) {
      this._cfRulesToMobileCfInfo(message.rules);
      message.result = message.rules;
    }

    Services.DOMRequest.fireSuccess(request, message.result);
  },

  _getRandomId: function _getRandomId() {
    return gUUIDGenerator.generateUUID().toString();
  },

  _deliverCallback: function _deliverCallback(callbackType, name, args) {
    let thisCallbacks = this[callbackType];
    if (!thisCallbacks) {
      return;
    }

    let callbacks = thisCallbacks.slice();
    for each (let callback in callbacks) {
      if (thisCallbacks.indexOf(callback) == -1) {
        continue;
      }
      let handler = callback[name];
      if (typeof handler != "function") {
        throw new Error("No handler for " + name);
      }
      try {
        handler.apply(callback, args);
      } catch (e) {
        debug("callback handler for " + name + " threw an exception: " + e);
      }
    }
  },

  /**
   * Helper for guarding us again invalid reason values for call forwarding.
   */
   _isValidCFReason: function _isValidCFReason(reason) {
     switch (reason) {
       case Ci.nsIDOMMozMobileCFInfo.CALL_FORWARD_REASON_UNCONDITIONAL:
       case Ci.nsIDOMMozMobileCFInfo.CALL_FORWARD_REASON_MOBILE_BUSY:
       case Ci.nsIDOMMozMobileCFInfo.CALL_FORWARD_REASON_NO_REPLY:
       case Ci.nsIDOMMozMobileCFInfo.CALL_FORWARD_REASON_NOT_REACHABLE:
       case Ci.nsIDOMMozMobileCFInfo.CALL_FORWARD_REASON_ALL_CALL_FORWARDING:
       case Ci.nsIDOMMozMobileCFInfo.CALL_FORWARD_REASON_ALL_CONDITIONAL_CALL_FORWARDING:
         return true;
       default:
         return false;
     }
  },

  /**
   * Helper for guarding us again invalid action values for call forwarding.
   */
   _isValidCFAction: function _isValidCFAction(action) {
     switch (action) {
       case Ci.nsIDOMMozMobileCFInfo.CALL_FORWARD_ACTION_DISABLE:
       case Ci.nsIDOMMozMobileCFInfo.CALL_FORWARD_ACTION_ENABLE:
       case Ci.nsIDOMMozMobileCFInfo.CALL_FORWARD_ACTION_REGISTRATION:
       case Ci.nsIDOMMozMobileCFInfo.CALL_FORWARD_ACTION_ERASURE:
         return true;
       default:
         return false;
     }
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([RILContentHelper]);

let debug;
if (DEBUG) {
  debug = function (s) {
    dump("-*- RILContentHelper: " + s + "\n");
  };
} else {
  debug = function (s) {};
}
