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
  "RIL:CellBroadcastReceived",
  "RIL:CfStateChanged",
  "RIL:IccOpenChannel",
  "RIL:IccCloseChannel",
  "RIL:IccExchangeAPDU",
  "RIL:IccUpdateContact"
];

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
  mcc: null,
  mnc: null,
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
  lastKnownMcc: null,
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
  mcc: null,
  mnc: null,
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
  this.rilContext = {
    cardState:            RIL.GECKO_CARDSTATE_UNKNOWN,
    iccInfo:              new MobileICCInfo(),
    voiceConnectionInfo:  new MobileConnectionInfo(),
    dataConnectionInfo:   new MobileConnectionInfo()
  };
  this.voicemailInfo = new VoicemailInfo();

  this.initRequests();
  this.initMessageListener(RIL_IPC_MSG_NAMES);
  Services.obs.addObserver(this, "xpcom-shutdown", false);
}

RILContentHelper.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIMobileConnectionProvider,
                                         Ci.nsICellBroadcastProvider,
                                         Ci.nsIVoicemailProvider,
                                         Ci.nsITelephonyProvider,
                                         Ci.nsIIccProvider,
                                         Ci.nsIObserver]),
  classID:   RILCONTENTHELPER_CID,
  classInfo: XPCOMUtils.generateCI({classID: RILCONTENTHELPER_CID,
                                    classDescription: "RILContentHelper",
                                    interfaces: [Ci.nsIMobileConnectionProvider,
                                                 Ci.nsICellBroadcastProvider,
                                                 Ci.nsIVoicemailProvider,
                                                 Ci.nsITelephonyProvider,
                                                 Ci.nsIIccProvider]}),

  // An utility function to copy objects.
  updateInfo: function updateInfo(srcInfo, destInfo) {
    for (let key in srcInfo) {
      destInfo[key] = srcInfo[key];
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

    this.updateInfo(srcNetwork, network);
 },

  // nsIRILContentHelper

  networkSelectionMode: RIL.GECKO_NETWORK_SELECTION_UNKNOWN,

  rilContext: null,

  getRilContext: function getRilContext() {
    // Update ril context by sending IPC message to chrome only when the first
    // time we require it. The information will be updated by following info
    // changed messages.
    this.getRilContext = function getRilContext() {
      return this.rilContext;
    };

    let rilContext = cpmm.sendSyncMessage("RIL:GetRilContext")[0];
    if (!rilContext) {
      debug("Received null rilContext from chrome process.");
      return;
    }
    this.rilContext.cardState = rilContext.cardState;
    this.updateInfo(rilContext.iccInfo, this.rilContext.iccInfo);
    this.updateConnectionInfo(rilContext.voice, this.rilContext.voiceConnectionInfo);
    this.updateConnectionInfo(rilContext.data, this.rilContext.dataConnectionInfo);

    return this.rilContext;
  },

  get iccInfo() {
    return this.getRilContext().iccInfo;
  },

  get voiceConnectionInfo() {
    return this.getRilContext().voiceConnectionInfo;
  },

  get dataConnectionInfo() {
    return this.getRilContext().dataConnectionInfo;
  },

  get cardState() {
    return this.getRilContext().cardState;
  },

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

    if (isNaN(parseInt(network.mnc, 10))) {
      throw new Error("Invalid network MNC: " + network.mnc);
    }

    if (isNaN(parseInt(network.mcc, 10))) {
      throw new Error("Invalid network MCC: " + network.mcc);
    }

    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);

    if (this.networkSelectionMode == RIL.GECKO_NETWORK_SELECTION_MANUAL
        && this.rilContext.voiceConnectionInfo.network === network) {

      // Already manually selected this network, so schedule
      // onsuccess to be fired on the next tick
      this.dispatchFireRequestSuccess(requestId, null);
      return request;
    }

    this._selectingNetwork = network;

    cpmm.sendAsyncMessage("RIL:SelectNetwork", {
      requestId: requestId,
      mnc: network.mnc,
      mcc: network.mcc
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

  iccOpenChannel: function iccOpenChannel(window,
                                          aid) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }

    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);

    cpmm.sendAsyncMessage("RIL:IccOpenChannel", {requestId: requestId,
                                                           aid: aid});
    return request;
  },

  iccExchangeAPDU: function iccExchangeAPDU(window,
                                            channel,
                                            apdu) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }

    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);

    //Potentially you need serialization here and can't pass the jsval through
    cpmm.sendAsyncMessage("RIL:IccExchangeAPDU", {requestId: requestId,
                                                      channel: channel,
                                                          apdu: apdu});
    return request;
  },

  iccCloseChannel: function iccCloseChannel(window,
                                            channel) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }

    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);

    cpmm.sendAsyncMessage("RIL:IccCloseChannel", {requestId: requestId,
                                                    channel: channel});
    return request;
  },

  updateContact: function updateContact(window, contactType, contact, pin2) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }

    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);

    // Parsing nsDOMContact to Icc Contact format
    let iccContact = {};

    if (contact.name) {
      iccContact.alphaId = contact.name[0];
    }

    if (contact.tel) {
      iccContact.number = contact.tel[0].value;
    }

    cpmm.sendAsyncMessage("RIL:IccUpdateContact", {requestId: requestId,
                                                   contactType: contactType,
                                                   contact: iccContact,
                                                   pin2: pin2});

    return request;
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

  _mobileConnectionListeners: null,
  _telephonyListeners: null,
  _cellBroadcastListeners: null,
  _voicemailListeners: null,
  _iccListeners: null,
  _enumerateTelephonyCallbacks: null,

  voicemailStatus: null,

  getVoicemailInfo: function getVoicemailInfo() {
    // Get voicemail infomation by IPC only on first time.
    this.getVoicemailInfo = function getVoicemailInfo() {
      return this.voicemailInfo;
    };

    let voicemailInfo = cpmm.sendSyncMessage("RIL:GetVoicemailInfo")[0];
    if (voicemailInfo) {
      this.updateInfo(voicemailInfo, this.voicemailInfo);
    }

    return this.voicemailInfo;
  },
  get voicemailNumber() {
    return this.getVoicemailInfo().number;
  },
  get voicemailDisplayName() {
    return this.getVoicemailInfo().displayName;
  },

  registerListener: function registerListener(listenerType, listener) {
    let listeners = this[listenerType];
    if (!listeners) {
      listeners = this[listenerType] = [];
    }

    if (listeners.indexOf(listener) != -1) {
      throw new Error("Already registered this listener!");
    }

    listeners.push(listener);
    if (DEBUG) debug("Registered " + listenerType + " listener: " + listener);
  },

  unregisterListener: function unregisterListener(listenerType, listener) {
    let listeners = this[listenerType];
    if (!listeners) {
      return;
    }

    let index = listeners.indexOf(listener);
    if (index != -1) {
      listeners.splice(index, 1);
      if (DEBUG) debug("Unregistered listener: " + listener);
    }
  },

  registerMobileConnectionMsg: function registerMobileConnectionMsg(listener) {
    debug("Registering for mobile connection related messages");
    this.registerListener("_mobileConnectionListeners", listener);
    cpmm.sendAsyncMessage("RIL:RegisterMobileConnectionMsg");
  },

  unregisterMobileConnectionMsg: function unregisteMobileConnectionMsg(listener) {
    this.unregisterListener("_mobileConnectionListeners", listener);
  },

  registerTelephonyMsg: function registerTelephonyMsg(listener) {
    debug("Registering for telephony-related messages");
    this.registerListener("_telephonyListeners", listener);
    cpmm.sendAsyncMessage("RIL:RegisterTelephonyMsg");
  },

  unregisterTelephonyMsg: function unregisteTelephonyMsg(listener) {
    this.unregisterListener("_telephonyListeners", listener);

    // We also need to make sure the listener is removed from
    // _enumerateTelephonyCallbacks.
    let index = this._enumerateTelephonyCallbacks.indexOf(listener);
    if (index != -1) {
      this._enumerateTelephonyCallbacks.splice(index, 1);
      if (DEBUG) debug("Unregistered enumerateTelephony callback: " + listener);
    }
  },

  registerVoicemailMsg: function registerVoicemailMsg(listener) {
    debug("Registering for voicemail-related messages");
    this.registerListener("_voicemailListeners", listener);
    cpmm.sendAsyncMessage("RIL:RegisterVoicemailMsg");
  },

  unregisterVoicemailMsg: function unregisteVoicemailMsg(listener) {
    this.unregisterListener("_voicemailListeners", listener);
  },

  registerCellBroadcastMsg: function registerCellBroadcastMsg(listener) {
    debug("Registering for Cell Broadcast related messages");
    this.registerListener("_cellBroadcastListeners", listener);
    cpmm.sendAsyncMessage("RIL:RegisterCellBroadcastMsg");
  },

  unregisterCellBroadcastMsg: function unregisterCellBroadcastMsg(listener) {
    this.unregisterListener("_cellBroadcastListeners", listener);
  },

  registerIccMsg: function registerIccMsg(listener) {
    debug("Registering for ICC related messages");
    this.registerListener("_iccListeners", listener);
    cpmm.sendAsyncMessage("RIL:RegisterIccMsg");
  },

  unregisterIccMsg: function unregisterIccMsg(listener) {
    this.unregisterListener("_iccListeners", listener);
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
        if (this.rilContext.cardState != msg.json.cardState) {
          this.rilContext.cardState = msg.json.cardState;
          this._deliverEvent("_mobileConnectionListeners",
                             "notifyCardStateChanged",
                             null);
        }
        break;
      case "RIL:IccInfoChanged":
        this.updateInfo(msg.json, this.rilContext.iccInfo);
        this._deliverEvent("_mobileConnectionListeners",
                           "notifyIccInfoChanged", null);
        break;
      case "RIL:VoiceInfoChanged":
        this.updateConnectionInfo(msg.json, this.rilContext.voiceConnectionInfo);
        this._deliverEvent("_mobileConnectionListeners",
                           "notifyVoiceChanged",
                           null);
        break;
      case "RIL:DataInfoChanged":
        this.updateConnectionInfo(msg.json, this.rilContext.dataConnectionInfo);
        this._deliverEvent("_mobileConnectionListeners",
                           "notifyDataChanged",
                           null);
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
        this._deliverEvent("_telephonyListeners",
                           "callStateChanged",
                           [msg.json.callIndex, msg.json.state,
                            msg.json.number, msg.json.isActive]);
        break;
      case "RIL:CallError":
        this._deliverEvent("_telephonyListeners",
                           "notifyError",
                           [msg.json.callIndex, msg.json.errorMsg]);
        break;
      case "RIL:VoicemailNotification":
        this.handleVoicemailNotification(msg.json);
        break;
      case "RIL:VoicemailInfoChanged":
        this.updateInfo(msg.json, this.voicemailInfo);
        break;
      case "RIL:CardLockResult":
        if (msg.json.success) {
          let result = new MobileICCCardLockResult(msg.json);
          this.fireRequestSuccess(msg.json.requestId, result);
        } else {
          if (msg.json.rilMessageType == "iccSetCardLock" ||
              msg.json.rilMessageType == "iccUnlockCardLock") {
            this._deliverEvent("_mobileConnectionListeners",
                               "notifyIccCardLockError",
                               [msg.json.lockType, msg.json.retryCount]);
          }
          this.fireRequestError(msg.json.requestId, msg.json.errorMsg);
        }
        break;
      case "RIL:USSDReceived":
        let res = JSON.stringify({message: msg.json.message,
                                  sessionEnded: msg.json.sessionEnded});
        this._deliverEvent("_mobileConnectionListeners",
                           "notifyUssdReceived",
                           [msg.json.message, msg.json.sessionEnded]);
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
        this._deliverEvent("_iccListeners", "notifyStkCommand",
                           [JSON.stringify(msg.json)]);
        break;
      case "RIL:StkSessionEnd":
        this._deliverEvent("_iccListeners", "notifyStkSessionEnd", null);
        break;
      case "RIL:IccOpenChannel":
        this.handleIccOpenChannel(msg.json);
        break;
      case "RIL:IccCloseChannel":
        this.handleIccCloseChannel(msg.json);
        break;
      case "RIL:IccExchangeAPDU":
        this.handleIccExchangeAPDU(msg.json);
        break;
      case "RIL:IccUpdateContact":
        this.handleIccUpdateContact(msg.json);
        break;
      case "RIL:DataError":
        this.updateConnectionInfo(msg.json, this.rilContext.dataConnectionInfo);
        this._deliverEvent("_mobileConnectionListeners", "notifyDataError",
                           [msg.json.errorMsg]);
        break;
      case "RIL:GetCallForwardingOption":
        this.handleGetCallForwardingOption(msg.json);
        break;
      case "RIL:SetCallForwardingOption":
        this.handleSetCallForwardingOption(msg.json);
        break;
      case "RIL:CfStateChanged":
        this._deliverEvent("_mobileConnectionListeners",
                           "notifyCFStateChange",
                           [msg.json.success, msg.json.action,
                            msg.json.reason, msg.json.number,
                            msg.json.timeSeconds, msg.json.serviceClass]);
        break;
      case "RIL:CellBroadcastReceived":
        let message = new CellBroadcastMessage(msg.json);
        this._deliverEvent("_cellBroadcastListeners",
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

    if (message.errorMsg) {
      debug("Received error from getAvailableNetworks: " + message.errorMsg);
      Services.DOMRequest.fireError(request, message.errorMsg);
      return;
    }

    let networks = message.networks;
    for (let i = 0; i < networks.length; i++) {
      let network = networks[i];
      let info = new MobileNetworkInfo();
      this.updateInfo(network, info);
      networks[i] = info;
    }

    Services.DOMRequest.fireSuccess(request, networks);
  },

  handleSelectNetwork: function handleSelectNetwork(message, mode) {
    this._selectingNetwork = null;
    this.networkSelectionMode = mode;

    if (message.errorMsg) {
      this.fireRequestError(message.requestId, message.errorMsg);
    } else {
      this.fireRequestSuccess(message.requestId, null);
    }
  },

  handleIccOpenChannel: function handleIccOpenChannel(message) {
    if (message.errorMsg) {
      this.fireRequestError(message.requestId, message.errorMsg);
    } else {
      this.fireRequestSuccess(message.requestId, message.channel);
    }
  },

  handleIccCloseChannel: function handleIccCloseChannel(message) {
    if (message.errorMsg) {
      this.fireRequestError(message.requestId, message.errorMsg);
    } else {
      this.fireRequestSuccess(message.requestId, null);
    }
  },

  handleIccExchangeAPDU: function handleIccExchangeAPDU(message) {
    if (message.errorMsg) {
      this.fireRequestError(message.requestId, message.errorMsg);
    } else {
      var result = [message.sw1, message.sw2, message.simResponse];
      this.fireRequestSuccess(message.requestId, result);
    }
  },

  handleIccUpdateContact: function handleIccUpdateContact(message) {
    if (message.errorMsg) {
      this.fireRequestError(message.requestId, message.errorMsg);
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
      this._deliverEvent("_voicemailListeners",
                         "voicemailNotification",
                         [this.voicemailStatus]);
    }
  },

  _cfRulesToMobileCfInfo: function _cfRulesToMobileCfInfo(rules) {
    for (let i = 0; i < rules.length; i++) {
      let rule = rules[i];
      let info = new MobileCFInfo();
      this.updateInfo(rule, info);
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

  _deliverEvent: function _deliverEvent(listenerType, name, args) {
    let thisListeners = this[listenerType];
    if (!thisListeners) {
      return;
    }

    let listeners = thisListeners.slice();
    for each (let listener in listeners) {
      if (thisListeners.indexOf(listener) == -1) {
        continue;
      }
      let handler = listener[name];
      if (typeof handler != "function") {
        throw new Error("No handler for " + name);
      }
      try {
        handler.apply(listener, args);
      } catch (e) {
        debug("listener for " + name + " threw an exception: " + e);
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
