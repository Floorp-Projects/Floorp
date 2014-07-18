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

Cu.import("resource://gre/modules/DOMRequestHelper.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "RIL", function () {
  let obj = {};
  Cu.import("resource://gre/modules/ril_consts.js", obj);
  return obj;
});

const NS_XPCOM_SHUTDOWN_OBSERVER_ID = "xpcom-shutdown";

const NS_PREFBRANCH_PREFCHANGE_TOPIC_ID = "nsPref:changed";

const kPrefRilNumRadioInterfaces = "ril.numRadioInterfaces";
const kPrefRilDebuggingEnabled = "ril.debugging.enabled";
const kPrefVoicemailDefaultServiceId = "dom.voicemail.defaultServiceId";

let DEBUG;
function debug(s) {
  dump("-*- RILContentHelper: " + s + "\n");
}

const RILCONTENTHELPER_CID =
  Components.ID("{472816e1-1fd6-4405-996c-806f9ea68174}");
const GSMICCINFO_CID =
  Components.ID("{e0fa785b-ad3f-46ed-bc56-fcb0d6fe4fa8}");
const CDMAICCINFO_CID =
  Components.ID("{3d1f844f-9ec5-48fb-8907-aed2e5421709}");
const MOBILECONNECTIONINFO_CID =
  Components.ID("{a35cfd39-2d93-4489-ac7d-396475dacb27}");
const MOBILENETWORKINFO_CID =
  Components.ID("{a6c8416c-09b4-46d1-bf29-6520d677d085}");
const MOBILECELLINFO_CID =
  Components.ID("{ae724dd4-ccaf-4006-98f1-6ce66a092464}");
const VOICEMAILSTATUS_CID=
  Components.ID("{5467f2eb-e214-43ea-9b89-67711241ec8e}");
const MOBILECFINFO_CID=
  Components.ID("{a4756f16-e728-4d9f-8baa-8464f894888a}");
const CELLBROADCASTMESSAGE_CID =
  Components.ID("{29474c96-3099-486f-bb4a-3c9a1da834e4}");
const CELLBROADCASTETWSINFO_CID =
  Components.ID("{59f176ee-9dcd-4005-9d47-f6be0cd08e17}");
const ICCCARDLOCKERROR_CID =
  Components.ID("{08a71987-408c-44ff-93fd-177c0a85c3dd}");

const RIL_IPC_MSG_NAMES = [
  "RIL:CardStateChanged",
  "RIL:IccInfoChanged",
  "RIL:VoiceInfoChanged",
  "RIL:DataInfoChanged",
  "RIL:GetAvailableNetworks",
  "RIL:NetworkSelectionModeChanged",
  "RIL:SelectNetwork",
  "RIL:SelectNetworkAuto",
  "RIL:SetPreferredNetworkType",
  "RIL:GetPreferredNetworkType",
  "RIL:EmergencyCbModeChanged",
  "RIL:VoicemailNotification",
  "RIL:VoicemailInfoChanged",
  "RIL:CardLockResult",
  "RIL:CardLockRetryCount",
  "RIL:USSDReceived",
  "RIL:SendMMI",
  "RIL:CancelMMI",
  "RIL:StkCommand",
  "RIL:StkSessionEnd",
  "RIL:DataError",
  "RIL:SetCallForwardingOptions",
  "RIL:GetCallForwardingOptions",
  "RIL:SetCallBarringOptions",
  "RIL:GetCallBarringOptions",
  "RIL:ChangeCallBarringPassword",
  "RIL:SetCallWaitingOptions",
  "RIL:GetCallWaitingOptions",
  "RIL:SetCallingLineIdRestriction",
  "RIL:GetCallingLineIdRestriction",
  "RIL:CellBroadcastReceived",
  "RIL:CfStateChanged",
  "RIL:IccOpenChannel",
  "RIL:IccCloseChannel",
  "RIL:IccExchangeAPDU",
  "RIL:ReadIccContacts",
  "RIL:UpdateIccContact",
  "RIL:SetRoamingPreference",
  "RIL:GetRoamingPreference",
  "RIL:ExitEmergencyCbMode",
  "RIL:SetRadioEnabled",
  "RIL:RadioStateChanged",
  "RIL:SetVoicePrivacyMode",
  "RIL:GetVoicePrivacyMode",
  "RIL:OtaStatusChanged",
  "RIL:MatchMvno",
  "RIL:ClirModeChanged"
];

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsISyncMessageSender");

XPCOMUtils.defineLazyGetter(this, "gNumRadioInterfaces", function() {
  let appInfo = Cc["@mozilla.org/xre/app-info;1"];
  let isParentProcess = !appInfo || appInfo.getService(Ci.nsIXULRuntime)
                          .processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;

  if (isParentProcess) {
    let ril = Cc["@mozilla.org/ril;1"].getService(Ci.nsIRadioInterfaceLayer);
    return ril.numRadioInterfaces;
  }

  return Services.prefs.getIntPref(kPrefRilNumRadioInterfaces);
});

function MobileIccCardLockResult(options) {
  this.lockType = options.lockType;
  this.enabled = options.enabled;
  this.retryCount = options.retryCount;
  this.success = options.success;
}
MobileIccCardLockResult.prototype = {
  __exposedProps__ : {lockType: 'r',
                      enabled: 'r',
                      retryCount: 'r',
                      success: 'r'}
};

function MobileIccCardLockRetryCount(options) {
  this.lockType = options.lockType;
  this.retryCount = options.retryCount;
  this.success = options.success;
}
MobileIccCardLockRetryCount.prototype = {
  __exposedProps__ : {lockType: 'r',
                      retryCount: 'r',
                      success: 'r'}
};

function IccInfo() {}
IccInfo.prototype = {
  iccType: null,
  iccid: null,
  mcc: null,
  mnc: null,
  spn: null,
  isDisplayNetworkNameRequired: null,
  isDisplaySpnRequired: null
};

function GsmIccInfo() {}
GsmIccInfo.prototype = {
  __proto__: IccInfo.prototype,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMMozGsmIccInfo]),
  classID: GSMICCINFO_CID,
  classInfo: XPCOMUtils.generateCI({
    classID:          GSMICCINFO_CID,
    classDescription: "MozGsmIccInfo",
    flags:            Ci.nsIClassInfo.DOM_OBJECT,
    interfaces:       [Ci.nsIDOMMozGsmIccInfo]
  }),

  // nsIDOMMozGsmIccInfo

  msisdn: null
};

function CdmaIccInfo() {}
CdmaIccInfo.prototype = {
  __proto__: IccInfo.prototype,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMMozCdmaIccInfo]),
  classID: CDMAICCINFO_CID,
  classInfo: XPCOMUtils.generateCI({
    classID:          CDMAICCINFO_CID,
    classDescription: "MozCdmaIccInfo",
    flags:            Ci.nsIClassInfo.DOM_OBJECT,
    interfaces:       [Ci.nsIDOMMozCdmaIccInfo]
  }),

  // nsIDOMMozCdmaIccInfo

  mdn: null,
  prlVersion: 0
};

function VoicemailInfo() {}
VoicemailInfo.prototype = {
  number: null,
  displayName: null
};

function MobileConnectionInfo() {}
MobileConnectionInfo.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIMobileConnectionInfo]),
  classID:        MOBILECONNECTIONINFO_CID,
  classInfo:      XPCOMUtils.generateCI({
    classID:          MOBILECONNECTIONINFO_CID,
    classDescription: "MobileConnectionInfo",
    interfaces:       [Ci.nsIMobileConnectionInfo]
  }),

  // nsIMobileConnectionInfo

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
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIMobileNetworkInfo]),
  classID:        MOBILENETWORKINFO_CID,
  classInfo:      XPCOMUtils.generateCI({
    classID:          MOBILENETWORKINFO_CID,
    classDescription: "MobileNetworkInfo",
    interfaces:       [Ci.nsIMobileNetworkInfo]
  }),

  // nsIMobileNetworkInfo

  shortName: null,
  longName: null,
  mcc: null,
  mnc: null,
  state: null
};

function MobileCellInfo() {}
MobileCellInfo.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIMobileCellInfo]),
  classID:        MOBILECELLINFO_CID,
  classInfo:      XPCOMUtils.generateCI({
    classID:          MOBILECELLINFO_CID,
    classDescription: "MobileCellInfo",
    interfaces:       [Ci.nsIMobileCellInfo]
  }),

  // nsIMobileCellInfo

  gsmLocationAreaCode: -1,
  gsmCellId: -1,
  cdmaBaseStationId: -1,
  cdmaBaseStationLatitude: -2147483648,
  cdmaBaseStationLongitude: -2147483648,
  cdmaSystemId: -1,
  cdmaNetworkId: -1
};

function VoicemailStatus(clientId) {
  this.serviceId = clientId;
}
VoicemailStatus.prototype = {
  QueryInterface: XPCOMUtils.generateQI([]),
  classID:        VOICEMAILSTATUS_CID,
  contractID:     "@mozilla.org/voicemailstatus;1",

  serviceId: -1,
  hasMessages: false,
  messageCount: -1, // Count unknown.
  returnNumber: null,
  returnMessage: null
};

function MobileCallForwardingInfo(options) {
  this.active = options.active;
  this.action = options.action;
  this.reason = options.reason;
  this.number = options.number;
  this.timeSeconds = options.timeSeconds;
  this.serviceClass = options.serviceClass;
}
MobileCallForwardingInfo.prototype = {
  __exposedProps__ : {active: 'r',
                      action: 'r',
                      reason: 'r',
                      number: 'r',
                      timeSeconds: 'r',
                      serviceClass: 'r'}
};

function CellBroadcastMessage(clientId, pdu) {
  this.serviceId = clientId;
  this.gsmGeographicalScope = RIL.CB_GSM_GEOGRAPHICAL_SCOPE_NAMES[pdu.geographicalScope];
  this.messageCode = pdu.messageCode;
  this.messageId = pdu.messageId;
  this.language = pdu.language;
  this.body = pdu.fullBody;
  this.messageClass = pdu.messageClass;
  this.timestamp = pdu.timestamp;

  if (pdu.etws != null) {
    this.etws = new CellBroadcastEtwsInfo(pdu.etws);
  }

  this.cdmaServiceCategory = pdu.serviceCategory;
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
  serviceId: -1,

  gsmGeographicalScope: null,
  messageCode: null,
  messageId: null,
  language: null,
  body: null,
  messageClass: null,
  timestamp: null,

  etws: null,
  cdmaServiceCategory: null
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

function CallBarringOptions(options) {
  this.program = options.program;
  this.enabled = options.enabled;
  this.password = options.password;
  this.serviceClass = options.serviceClass;
}
CallBarringOptions.prototype = {
  __exposedProps__ : {program: 'r',
                      enabled: 'r',
                      password: 'r',
                      serviceClass: 'r'}
};

function DOMMMIResult(result) {
  this.serviceCode = result.serviceCode;
  this.statusMessage = result.statusMessage;
  this.additionalInformation = result.additionalInformation;
}
DOMMMIResult.prototype = {
  __exposedProps__: {serviceCode: 'r',
                     statusMessage: 'r',
                     additionalInformation: 'r'}
};

function DOMCLIRStatus(option) {
  this.n = option.n;
  this.m = option.m;
}
DOMCLIRStatus.prototype = {
  __exposedProps__ : {n: 'r',
                      m: 'r'}
};

function IccCardLockError() {
}
IccCardLockError.prototype = {
  classDescription: "IccCardLockError",
  classID:          ICCCARDLOCKERROR_CID,
  contractID:       "@mozilla.org/dom/icccardlock-error;1",
  QueryInterface:   XPCOMUtils.generateQI([Ci.nsISupports]),
  __init: function(lockType, errorMsg, retryCount) {
    this.__DOM_IMPL__.init(errorMsg);
    this.lockType = lockType;
    this.retryCount = retryCount;
  },
};

function RILContentHelper() {
  this.updateDebugFlag();

  this.numClients = gNumRadioInterfaces;
  if (DEBUG) debug("Number of clients: " + this.numClients);

  this.rilContexts = [];
  this.voicemailInfos = [];
  this.voicemailStatuses = [];
  for (let clientId = 0; clientId < this.numClients; clientId++) {
    this.rilContexts[clientId] = {
      cardState:            RIL.GECKO_CARDSTATE_UNKNOWN,
      networkSelectionMode: RIL.GECKO_NETWORK_SELECTION_UNKNOWN,
      radioState:           null,
      iccInfo:              null,
      voiceConnectionInfo:  new MobileConnectionInfo(),
      dataConnectionInfo:   new MobileConnectionInfo()
    };

    this.voicemailInfos[clientId] = new VoicemailInfo();
  }

  this.voicemailDefaultServiceId = this.getVoicemailDefaultServiceId();

  this.initDOMRequestHelper(/* aWindow */ null, RIL_IPC_MSG_NAMES);
  this._windowsMap = [];
  this._mobileConnectionListeners = [];
  this._cellBroadcastListeners = [];
  this._voicemailListeners = [];
  this._iccListeners = [];

  Services.obs.addObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);

  Services.prefs.addObserver(kPrefRilDebuggingEnabled, this, false);
  Services.prefs.addObserver(kPrefVoicemailDefaultServiceId, this, false);
}

RILContentHelper.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIMobileConnectionProvider,
                                         Ci.nsICellBroadcastProvider,
                                         Ci.nsIVoicemailProvider,
                                         Ci.nsIIccProvider,
                                         Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference,
                                         Ci.nsIObserver]),
  classID:   RILCONTENTHELPER_CID,
  classInfo: XPCOMUtils.generateCI({classID: RILCONTENTHELPER_CID,
                                    classDescription: "RILContentHelper",
                                    interfaces: [Ci.nsIMobileConnectionProvider,
                                                 Ci.nsICellBroadcastProvider,
                                                 Ci.nsIVoicemailProvider,
                                                 Ci.nsIIccProvider]}),

  updateDebugFlag: function() {
    try {
      DEBUG = RIL.DEBUG_CONTENT_HELPER ||
              Services.prefs.getBoolPref(kPrefRilDebuggingEnabled);
    } catch (e) {}
  },

  // An utility function to copy objects.
  updateInfo: function(srcInfo, destInfo) {
    for (let key in srcInfo) {
      destInfo[key] = srcInfo[key];
    }
  },

  updateConnectionInfo: function(srcInfo, destInfo) {
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

      this.updateInfo(srcCell, cell);
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

  /**
   * We need to consider below cases when update iccInfo:
   * 1. Should clear iccInfo to null if there is no card detected.
   * 2. Need to create corresponding object based on iccType.
   */
  updateIccInfo: function(clientId, newInfo) {
    let rilContext = this.rilContexts[clientId];

    // Card is not detected, clear iccInfo to null.
    if (!newInfo || !newInfo.iccType || !newInfo.iccid) {
      if (rilContext.iccInfo) {
        rilContext.iccInfo = null;
        this._deliverEvent(clientId,
                           "_mobileConnectionListeners",
                           "notifyIccChanged",
                           null);
      }
      return;
    }

    // If iccInfo is null, new corresponding object based on iccType.
    if (!rilContext.iccInfo) {
      if (newInfo.iccType === "ruim" || newInfo.iccType === "csim") {
        rilContext.iccInfo = new CdmaIccInfo();
      } else {
        rilContext.iccInfo = new GsmIccInfo();
      }
    }
    let changed = (rilContext.iccInfo.iccid != newInfo.iccid) ?
      true : false;

    this.updateInfo(newInfo, rilContext.iccInfo);

    // Deliver event after info is updated.
    if (changed) {
      this._deliverEvent(clientId,
                         "_mobileConnectionListeners",
                         "notifyIccChanged",
                         null);
    }
  },

  _windowsMap: null,

  rilContexts: null,

  getRilContext: function(clientId) {
    // Update ril contexts by sending IPC message to chrome only when the first
    // time we require it. The information will be updated by following info
    // changed messages.
    this.getRilContext = function getRilContext(clientId) {
      return this.rilContexts[clientId];
    };

    for (let cId = 0; cId < this.numClients; cId++) {
      let rilContext =
        cpmm.sendSyncMessage("RIL:GetRilContext", {clientId: cId})[0];
      if (!rilContext) {
        if (DEBUG) debug("Received null rilContext from chrome process.");
        continue;
      }
      this.rilContexts[cId].cardState = rilContext.cardState;
      this.rilContexts[cId].networkSelectionMode = rilContext.networkSelectionMode;
      this.rilContexts[cId].radioState = rilContext.detailedRadioState;
      this.updateIccInfo(cId, rilContext.iccInfo);
      this.updateConnectionInfo(rilContext.voice, this.rilContexts[cId].voiceConnectionInfo);
      this.updateConnectionInfo(rilContext.data, this.rilContexts[cId].dataConnectionInfo);
    }

    return this.rilContexts[clientId];
  },

  /**
   * nsIIccProvider
   */

  getIccInfo: function(clientId) {
    let context = this.getRilContext(clientId);
    return context && context.iccInfo;
  },

  getCardState: function(clientId) {
    let context = this.getRilContext(clientId);
    return context && context.cardState;
  },

  matchMvno: function(clientId, window, mvnoType, mvnoData) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }

    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);

    cpmm.sendAsyncMessage("RIL:MatchMvno", {
      clientId: clientId,
      data: {
        requestId: requestId,
        mvnoType: mvnoType,
        mvnoData: mvnoData
      }
    });
    return request;
  },

  /**
   * nsIMobileConnectionProvider
   */

  getLastKnownNetwork: function(clientId) {
    return cpmm.sendSyncMessage("RIL:GetLastKnownNetwork", {
      clientId: clientId
    })[0];
  },

  getLastKnownHomeNetwork: function(clientId) {
    return cpmm.sendSyncMessage("RIL:GetLastKnownHomeNetwork", {
      clientId: clientId
    })[0];
  },

  getVoiceConnectionInfo: function(clientId) {
    let context = this.getRilContext(clientId);
    return context && context.voiceConnectionInfo;
  },

  getDataConnectionInfo: function(clientId) {
    let context = this.getRilContext(clientId);
    return context && context.dataConnectionInfo;
  },

  getIccId: function(clientId) {
    let context = this.getRilContext(clientId);
    return context && context.iccInfo && context.iccInfo.iccid;
  },

  getNetworkSelectionMode: function(clientId) {
    let context = this.getRilContext(clientId);
    return context && context.networkSelectionMode;
  },

  getRadioState: function(clientId) {
    let context = this.getRilContext(clientId);
    return context && context.radioState;
  },

  getSupportedNetworkTypes: function(clientId) {
    return cpmm.sendSyncMessage("RIL:GetSupportedNetworkTypes", {
      clientId: clientId
    })[0];
  },

  getNetworks: function(clientId, window) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }

    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);
    // We need to save the global window to get the proper MobileNetworkInfo
    // constructor once we get the reply from the parent process.
    this._windowsMap[requestId] = window;

    cpmm.sendAsyncMessage("RIL:GetAvailableNetworks", {
      clientId: clientId,
      data: {
        requestId: requestId
      }
    });
    return request;
  },

  selectNetwork: function(clientId, window, network) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }

    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);

    if (!network ||
        isNaN(parseInt(network.mcc, 10)) || isNaN(parseInt(network.mnc, 10))) {
      this.dispatchFireRequestError(RIL.GECKO_ERROR_INVALID_PARAMETER);
      return request;
    }

    if (this.rilContexts[clientId].networkSelectionMode == RIL.GECKO_NETWORK_SELECTION_MANUAL &&
        this.rilContexts[clientId].voiceConnectionInfo.network === network) {
      // Already manually selected this network, so schedule
      // onsuccess to be fired on the next tick
      this.dispatchFireRequestSuccess(requestId, null);
      return request;
    }

    cpmm.sendAsyncMessage("RIL:SelectNetwork", {
      clientId: clientId,
      data: {
        requestId: requestId,
        mnc: network.mnc,
        mcc: network.mcc
      }
    });

    return request;
  },

  selectNetworkAutomatically: function(clientId, window) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }

    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);

    if (this.rilContexts[clientId].networkSelectionMode == RIL.GECKO_NETWORK_SELECTION_AUTOMATIC) {
      // Already using automatic selection mode, so schedule
      // onsuccess to be be fired on the next tick
      this.dispatchFireRequestSuccess(requestId, null);
      return request;
    }

    cpmm.sendAsyncMessage("RIL:SelectNetworkAuto", {
      clientId: clientId,
      data: {
        requestId: requestId
      }
    });
    return request;
  },

  setPreferredNetworkType: function(clientId, window, type) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }

    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);

    let radioState = this.rilContexts[clientId].radioState;
    if (radioState !== RIL.GECKO_DETAILED_RADIOSTATE_ENABLED) {
      this.dispatchFireRequestError(requestId,
                                    RIL.GECKO_ERROR_RADIO_NOT_AVAILABLE);
      return request;
    }

    cpmm.sendAsyncMessage("RIL:SetPreferredNetworkType", {
      clientId: clientId,
      data: {
        requestId: requestId,
        type: type
      }
    });
    return request;
  },

  getPreferredNetworkType: function(clientId, window) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }

    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);

    let radioState = this.rilContexts[clientId].radioState;
    if (radioState !== RIL.GECKO_DETAILED_RADIOSTATE_ENABLED) {
      this.dispatchFireRequestError(requestId,
                                    RIL.GECKO_ERROR_RADIO_NOT_AVAILABLE);
      return request;
    }

    cpmm.sendAsyncMessage("RIL:GetPreferredNetworkType", {
      clientId: clientId,
      data: {
        requestId: requestId
      }
    });
    return request;
  },

  setRoamingPreference: function(clientId, window, mode) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }

    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);

    if (!mode) {
      this.dispatchFireRequestError(requestId,
                                    RIL.GECKO_ERROR_INVALID_PARAMETER);
      return request;
    }

    cpmm.sendAsyncMessage("RIL:SetRoamingPreference", {
      clientId: clientId,
      data: {
        requestId: requestId,
        mode: mode
      }
    });
    return request;
  },

  getRoamingPreference: function(clientId, window) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }

    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);

    cpmm.sendAsyncMessage("RIL:GetRoamingPreference", {
      clientId: clientId,
      data: {
        requestId: requestId
      }
    });
    return request;
  },

  setVoicePrivacyMode: function(clientId, window, enabled) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }

    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);

    cpmm.sendAsyncMessage("RIL:SetVoicePrivacyMode", {
      clientId: clientId,
      data: {
        requestId: requestId,
        enabled: enabled
      }
    });
    return request;
  },

  getVoicePrivacyMode: function(clientId, window) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }

    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);

    cpmm.sendAsyncMessage("RIL:GetVoicePrivacyMode", {
      clientId: clientId,
      data: {
        requestId: requestId
      }
    });
    return request;
  },

  getCardLockState: function(clientId, window, lockType) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);
    this._windowsMap[requestId] = window;

    cpmm.sendAsyncMessage("RIL:GetCardLockState", {
      clientId: clientId,
      data: {
        lockType: lockType,
        requestId: requestId
      }
    });
    return request;
  },

  unlockCardLock: function(clientId, window, info) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    let request = Services.DOMRequest.createRequest(window);
    info.requestId = this.getRequestId(request);
    this._windowsMap[info.requestId] = window;

    cpmm.sendAsyncMessage("RIL:UnlockCardLock", {
      clientId: clientId,
      data: info
    });
    return request;
  },

  setCardLock: function(clientId, window, info) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    let request = Services.DOMRequest.createRequest(window);
    info.requestId = this.getRequestId(request);
    this._windowsMap[info.requestId] = window;

    cpmm.sendAsyncMessage("RIL:SetCardLock", {
      clientId: clientId,
      data: info
    });
    return request;
  },

  getCardLockRetryCount: function(clientId, window, lockType) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);
    cpmm.sendAsyncMessage("RIL:GetCardLockRetryCount", {
      clientId: clientId,
      data: {
        lockType: lockType,
        requestId: requestId
      }
    });
    return request;
  },

  sendMMI: function(clientId, window, mmi) {
    if (DEBUG) debug("Sending MMI " + mmi);
    if (!window) {
      throw Components.Exception("Can't get window object",
                                 Cr.NS_ERROR_UNEXPECTED);
    }
    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);
    // We need to save the global window to get the proper MMIError
    // constructor once we get the reply from the parent process.
    this._windowsMap[requestId] = window;

    cpmm.sendAsyncMessage("RIL:SendMMI", {
      clientId: clientId,
      data: {
        mmi: mmi,
        requestId: requestId
      }
    });
    return request;
  },

  cancelMMI: function(clientId, window) {
    if (DEBUG) debug("Cancel MMI");
    if (!window) {
      throw Components.Exception("Can't get window object",
                                 Cr.NS_ERROR_UNEXPECTED);
    }
    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);
    cpmm.sendAsyncMessage("RIL:CancelMMI", {
      clientId: clientId,
      data: {
        requestId: requestId
      }
    });
    return request;
  },

  sendStkResponse: function(clientId, window, command, response) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    response.command = command;
    cpmm.sendAsyncMessage("RIL:SendStkResponse", {
      clientId: clientId,
      data: response
    });
  },

  sendStkMenuSelection: function(clientId, window, itemIdentifier,
                                 helpRequested) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    cpmm.sendAsyncMessage("RIL:SendStkMenuSelection", {
      clientId: clientId,
      data: {
        itemIdentifier: itemIdentifier,
        helpRequested: helpRequested
      }
    });
  },

  sendStkTimerExpiration: function(clientId, window, timer) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    cpmm.sendAsyncMessage("RIL:SendStkTimerExpiration", {
      clientId: clientId,
      data: {
        timer: timer
      }
    });
  },

  sendStkEventDownload: function(clientId, window, event) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    cpmm.sendAsyncMessage("RIL:SendStkEventDownload", {
      clientId: clientId,
      data: {
        event: event
      }
    });
  },

  iccOpenChannel: function(clientId, window, aid) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }

    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);

    cpmm.sendAsyncMessage("RIL:IccOpenChannel", {
      clientId: clientId,
      data: {
        requestId: requestId,
        aid: aid
      }
    });
    return request;
  },

  iccExchangeAPDU: function(clientId, window, channel, apdu) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }

    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);

    //Potentially you need serialization here and can't pass the jsval through
    cpmm.sendAsyncMessage("RIL:IccExchangeAPDU", {
      clientId: clientId,
      data: {
        requestId: requestId,
        channel: channel,
        apdu: apdu
      }
    });
    return request;
  },

  iccCloseChannel: function(clientId, window, channel) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }

    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);

    cpmm.sendAsyncMessage("RIL:IccCloseChannel", {
      clientId: clientId,
      data: {
        requestId: requestId,
        channel: channel
      }
    });
    return request;
  },

  readContacts: function(clientId, window, contactType) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }

    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);
    this._windowsMap[requestId] = window;

    cpmm.sendAsyncMessage("RIL:ReadIccContacts", {
      clientId: clientId,
      data: {
        requestId: requestId,
        contactType: contactType
      }
    });
    return request;
  },

  updateContact: function(clientId, window, contactType, contact, pin2) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }

    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);
    this._windowsMap[requestId] = window;

    // Parsing nsDOMContact to Icc Contact format
    let iccContact = {};

    if (Array.isArray(contact.name) && contact.name[0]) {
      iccContact.alphaId = contact.name[0];
    }

    if (Array.isArray(contact.tel)) {
      iccContact.number = contact.tel[0] && contact.tel[0].value;
      let telArray = contact.tel.slice(1);
      let length = telArray.length;
      if (length > 0) {
        iccContact.anr = [];
      }
      for (let i = 0; i < telArray.length; i++) {
        iccContact.anr.push(telArray[i].value);
      }
    }

    if (Array.isArray(contact.email) && contact.email[0]) {
      iccContact.email = contact.email[0].value;
    }

    iccContact.contactId = contact.id;

    cpmm.sendAsyncMessage("RIL:UpdateIccContact", {
      clientId: clientId,
      data: {
        requestId: requestId,
        contactType: contactType,
        contact: iccContact,
        pin2: pin2
      }
    });

    return request;
  },

  getCallForwarding: function(clientId, window, reason) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);

    if (!this._isValidCallForwardingReason(reason)) {
      this.dispatchFireRequestError(requestId,
                                    RIL.GECKO_ERROR_INVALID_PARAMETER);
      return request;
    }

    cpmm.sendAsyncMessage("RIL:GetCallForwardingOptions", {
      clientId: clientId,
      data: {
        requestId: requestId,
        reason: reason
      }
    });

    return request;
  },

  setCallForwarding: function(clientId, window, options) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);

    if (!options ||
        !this._isValidCallForwardingReason(options.reason) ||
        !this._isValidCallForwardingAction(options.action)) {
      this.dispatchFireRequestError(requestId,
                                    RIL.GECKO_ERROR_INVALID_PARAMETER);
      return request;
    }

    cpmm.sendAsyncMessage("RIL:SetCallForwardingOptions", {
      clientId: clientId,
      data: {
        requestId: requestId,
        action: options.action,
        reason: options.reason,
        number: options.number,
        timeSeconds: options.timeSeconds
      }
    });

    return request;
  },

  getCallBarring: function(clientId, window, options) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);

    if (DEBUG) debug("getCallBarring: " + JSON.stringify(options));
    if (!this._isValidCallBarringOptions(options)) {
      this.dispatchFireRequestError(requestId,
                                    RIL.GECKO_ERROR_INVALID_PARAMETER);
      return request;
    }

    cpmm.sendAsyncMessage("RIL:GetCallBarringOptions", {
      clientId: clientId,
      data: {
        requestId: requestId,
        program: options.program,
        password: options.password,
        serviceClass: options.serviceClass
      }
    });
    return request;
  },

  setCallBarring: function(clientId, window, options) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);

    if (DEBUG) debug("setCallBarringOptions: " + JSON.stringify(options));
    if (!this._isValidCallBarringOptions(options, true)) {
      this.dispatchFireRequestError(requestId,
                                    RIL.GECKO_ERROR_INVALID_PARAMETER);
      return request;
    }

    cpmm.sendAsyncMessage("RIL:SetCallBarringOptions", {
      clientId: clientId,
      data: {
        requestId: requestId,
        program: options.program,
        enabled: options.enabled,
        password: options.password,
        serviceClass: options.serviceClass
      }
    });
    return request;
  },

  changeCallBarringPassword: function(clientId, window, info) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);

    // Checking valid PIN for supplementary services. See TS.22.004 clause 5.2.
    if (info.pin == null || !info.pin.match(/^\d{4}$/) ||
        info.newPin == null || !info.newPin.match(/^\d{4}$/)) {
      this.dispatchFireRequestError(requestId, "InvalidPassword");
      return request;
    }

    if (DEBUG) debug("changeCallBarringPassword: " + JSON.stringify(info));
    info.requestId = requestId;
    cpmm.sendAsyncMessage("RIL:ChangeCallBarringPassword", {
      clientId: clientId,
      data: info
    });

    return request;
  },

  getCallWaiting: function(clientId, window) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);

    cpmm.sendAsyncMessage("RIL:GetCallWaitingOptions", {
      clientId: clientId,
      data: {
        requestId: requestId
      }
    });

    return request;
  },

  setCallWaiting: function(clientId, window, enabled) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);

    cpmm.sendAsyncMessage("RIL:SetCallWaitingOptions", {
      clientId: clientId,
      data: {
        requestId: requestId,
        enabled: enabled
      }
    });

    return request;
  },

  getCallingLineIdRestriction: function(clientId, window) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);

    let radioState = this.rilContexts[clientId].radioState;
    if (radioState !== RIL.GECKO_DETAILED_RADIOSTATE_ENABLED) {
      this.dispatchFireRequestError(requestId,
                                    RIL.GECKO_ERROR_RADIO_NOT_AVAILABLE);
      return request;
    }

    cpmm.sendAsyncMessage("RIL:GetCallingLineIdRestriction", {
      clientId: clientId,
      data: {
        requestId: requestId
      }
    });

    return request;
  },

  setCallingLineIdRestriction: function(clientId, window, clirMode) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);

    let radioState = this.rilContexts[clientId].radioState;
    if (radioState !== RIL.GECKO_DETAILED_RADIOSTATE_ENABLED) {
      this.dispatchFireRequestError(requestId,
                                    RIL.GECKO_ERROR_RADIO_NOT_AVAILABLE);
      return request;
    }

    if (!this._isValidClirMode(clirMode)) {
      this.dispatchFireRequestError(requestId,
                                    RIL.GECKO_ERROR_INVALID_PARAMETER);
      return request;
    }

    cpmm.sendAsyncMessage("RIL:SetCallingLineIdRestriction", {
      clientId: clientId,
      data: {
        requestId: requestId,
        clirMode: clirMode
      }
    });

    return request;
  },

  exitEmergencyCbMode: function(clientId, window) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);

    cpmm.sendAsyncMessage("RIL:ExitEmergencyCbMode", {
      clientId: clientId,
      data: {
        requestId: requestId,
      }
    });

    return request;
  },

  setRadioEnabled: function(clientId, window, enabled) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);

    cpmm.sendAsyncMessage("RIL:SetRadioEnabled", {
      clientId: clientId,
      data: {
        requestId: requestId,
        enabled: enabled,
      }
    });

    return request;
  },

  _mobileConnectionListeners: null,
  _cellBroadcastListeners: null,
  _voicemailListeners: null,
  _iccListeners: null,

  voicemailInfos: null,
  voicemailStatuses: null,

  voicemailDefaultServiceId: 0,
  getVoicemailDefaultServiceId: function() {
    let id = Services.prefs.getIntPref(kPrefVoicemailDefaultServiceId);

    if (id >= gNumRadioInterfaces || id < 0) {
      id = 0;
    }

    return id;
  },

  getVoicemailInfo: function(clientId) {
    // Get voicemail infomation by IPC only on first time.
    this.getVoicemailInfo = function getVoicemailInfo(clientId) {
      return this.voicemailInfos[clientId];
    };

    for (let cId = 0; cId < gNumRadioInterfaces; cId++) {
      let voicemailInfo =
        cpmm.sendSyncMessage("RIL:GetVoicemailInfo", {clientId: cId})[0];
      if (voicemailInfo) {
        this.updateInfo(voicemailInfo, this.voicemailInfos[cId]);
      }
    }

    return this.voicemailInfos[clientId];
  },

  getVoicemailNumber: function(clientId) {
    return this.getVoicemailInfo(clientId).number;
  },

  getVoicemailDisplayName: function(clientId) {
    return this.getVoicemailInfo(clientId).displayName;
  },

  getVoicemailStatus: function(clientId) {
    return this.voicemailStatuses[clientId];
  },

  registerListener: function(listenerType, clientId, listener) {
    if (!this[listenerType]) {
      return;
    }
    let listeners = this[listenerType][clientId];
    if (!listeners) {
      listeners = this[listenerType][clientId] = [];
    }

    if (listeners.indexOf(listener) != -1) {
      throw new Error("Already registered this listener!");
    }

    listeners.push(listener);
    if (DEBUG) debug("Registered " + listenerType + " listener: " + listener);
  },

  unregisterListener: function(listenerType, clientId, listener) {
    if (!this[listenerType]) {
      return;
    }
    let listeners = this[listenerType][clientId];
    if (!listeners) {
      return;
    }

    let index = listeners.indexOf(listener);
    if (index != -1) {
      listeners.splice(index, 1);
      if (DEBUG) debug("Unregistered listener: " + listener);
    }
  },

  registerMobileConnectionMsg: function(clientId, listener) {
    if (DEBUG) debug("Registering for mobile connection related messages");
    this.registerListener("_mobileConnectionListeners", clientId, listener);
    cpmm.sendAsyncMessage("RIL:RegisterMobileConnectionMsg");
  },

  unregisterMobileConnectionMsg: function(clientId, listener) {
    this.unregisterListener("_mobileConnectionListeners", clientId, listener);
  },

  registerVoicemailMsg: function(listener) {
    if (DEBUG) debug("Registering for voicemail-related messages");
    // To follow the listener registration scheme, we add a dummy clientId 0.
    // All voicemail events are routed to listener for client id 0.
    // See |handleVoicemailNotification|.
    this.registerListener("_voicemailListeners", 0, listener);
    cpmm.sendAsyncMessage("RIL:RegisterVoicemailMsg");
  },

  unregisterVoicemailMsg: function(listener) {
    // To follow the listener unregistration scheme, we add a dummy clientId 0.
    // All voicemail events are routed to listener for client id 0.
    // See |handleVoicemailNotification|.
    this.unregisterListener("_voicemailListeners", 0, listener);
  },

  registerCellBroadcastMsg: function(listener) {
    if (DEBUG) debug("Registering for Cell Broadcast related messages");
    // Instead of registering multiple listeners for Multi-SIM, we reuse
    // clientId 0 to route all CBS messages to single listener and provide the
    // |clientId| info by |CellBroadcastMessage.serviceId|.
    this.registerListener("_cellBroadcastListeners", 0, listener);
    cpmm.sendAsyncMessage("RIL:RegisterCellBroadcastMsg");
  },

  unregisterCellBroadcastMsg: function(listener) {
    // Instead of unregistering multiple listeners for Multi-SIM, we reuse
    // clientId 0 to route all CBS messages to single listener and provide the
    // |clientId| info by |CellBroadcastMessage.serviceId|.
    this.unregisterListener("_cellBroadcastListeners", 0, listener);
  },

  registerIccMsg: function(clientId, listener) {
    if (DEBUG) debug("Registering for ICC related messages");
    this.registerListener("_iccListeners", clientId, listener);
    cpmm.sendAsyncMessage("RIL:RegisterIccMsg");
  },

  unregisterIccMsg: function(clientId, listener) {
    this.unregisterListener("_iccListeners", clientId, listener);
  },

  // nsIObserver

  observe: function(subject, topic, data) {
    switch (topic) {
      case NS_PREFBRANCH_PREFCHANGE_TOPIC_ID:
        if (data == kPrefRilDebuggingEnabled) {
          this.updateDebugFlag();
        } else if (data == kPrefVoicemailDefaultServiceId) {
          this.voicemailDefaultServiceId = this.getVoicemailDefaultServiceId();
        }
        break;

      case NS_XPCOM_SHUTDOWN_OBSERVER_ID:
        this.destroyDOMRequestHelper();
        Services.obs.removeObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
        break;
    }
  },

  // nsIMessageListener

  fireRequestSuccess: function(requestId, result) {
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

  dispatchFireRequestSuccess: function(requestId, result) {
    let currentThread = Services.tm.currentThread;

    currentThread.dispatch(this.fireRequestSuccess.bind(this, requestId, result),
                           Ci.nsIThread.DISPATCH_NORMAL);
  },

  fireRequestError: function(requestId, error) {
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

  dispatchFireRequestError: function(requestId, error) {
    let currentThread = Services.tm.currentThread;

    currentThread.dispatch(this.fireRequestError.bind(this, requestId, error),
                           Ci.nsIThread.DISPATCH_NORMAL);
  },

  fireRequestDetailedError: function(requestId, detailedError) {
    let request = this.takeRequest(requestId);
    if (!request) {
      if (DEBUG) {
        debug("not firing detailed error for id: " + requestId +
              ", detailedError: " + JSON.stringify(detailedError));
      }
      return;
    }

    Services.DOMRequest.fireDetailedError(request, detailedError);
  },

  receiveMessage: function(msg) {
    let request;
    if (DEBUG) {
      debug("Received message '" + msg.name + "': " + JSON.stringify(msg.json));
    }

    let data = msg.json.data;
    let clientId = msg.json.clientId;
    switch (msg.name) {
      case "RIL:CardStateChanged":
        if (this.rilContexts[clientId].cardState != data.cardState) {
          this.rilContexts[clientId].cardState = data.cardState;
          this._deliverEvent(clientId,
                             "_iccListeners",
                             "notifyCardStateChanged",
                             null);
        }
        break;
      case "RIL:IccInfoChanged":
        this.updateIccInfo(clientId, data);
        this._deliverEvent(clientId,
                           "_iccListeners",
                           "notifyIccInfoChanged",
                           null);
        break;
      case "RIL:VoiceInfoChanged":
        this.updateConnectionInfo(data,
                                  this.rilContexts[clientId].voiceConnectionInfo);
        this._deliverEvent(clientId,
                           "_mobileConnectionListeners",
                           "notifyVoiceChanged",
                           null);
        break;
      case "RIL:DataInfoChanged":
        this.updateConnectionInfo(data,
                                  this.rilContexts[clientId].dataConnectionInfo);
        this._deliverEvent(clientId,
                           "_mobileConnectionListeners",
                           "notifyDataChanged",
                           null);
        break;
      case "RIL:OtaStatusChanged":
        this._deliverEvent(clientId,
                           "_mobileConnectionListeners",
                           "notifyOtaStatusChanged",
                           [data]);
        break;
      case "RIL:GetAvailableNetworks":
        this.handleGetAvailableNetworks(data);
        break;
      case "RIL:NetworkSelectionModeChanged":
        this.rilContexts[clientId].networkSelectionMode = data.mode;
        break;
      case "RIL:SelectNetwork":
        this.handleSimpleRequest(data.requestId, data.errorMsg, null);
        break;
      case "RIL:SelectNetworkAuto":
        this.handleSimpleRequest(data.requestId, data.errorMsg, null);
        break;
      case "RIL:SetPreferredNetworkType":
        this.handleSimpleRequest(data.requestId, data.errorMsg, null);
        break;
      case "RIL:GetPreferredNetworkType":
        this.handleSimpleRequest(data.requestId, data.errorMsg, data.type);
        break;
      case "RIL:VoicemailNotification":
        this.handleVoicemailNotification(clientId, data);
        break;
      case "RIL:VoicemailInfoChanged":
        this.updateInfo(data, this.voicemailInfos[clientId]);
        break;
      case "RIL:CardLockResult": {
        let requestId = data.requestId;
        let requestWindow = this._windowsMap[requestId];
        delete this._windowsMap[requestId];

        if (data.success) {
          let result = new MobileIccCardLockResult(data);
          this.fireRequestSuccess(requestId, result);
        } else {
          if (data.rilMessageType == "iccSetCardLock" ||
              data.rilMessageType == "iccUnlockCardLock") {
            let cardLockError = new requestWindow.IccCardLockError(data.lockType,
                                                                   data.errorMsg,
                                                                   data.retryCount);
            this.fireRequestDetailedError(requestId, cardLockError);
          } else {
            this.fireRequestError(requestId, data.errorMsg);
          }
        }
        break;
      }
      case "RIL:CardLockRetryCount":
        if (data.success) {
          let result = new MobileIccCardLockRetryCount(data);
          this.fireRequestSuccess(data.requestId, result);
        } else {
          this.fireRequestError(data.requestId, data.errorMsg);
        }
        break;
      case "RIL:USSDReceived":
        this._deliverEvent(clientId,
                           "_mobileConnectionListeners",
                           "notifyUssdReceived",
                           [data.message, data.sessionEnded]);
        break;
      case "RIL:SendMMI":
      case "RIL:CancelMMI":
        this.handleSendCancelMMI(data);
        break;
      case "RIL:StkCommand":
        this._deliverEvent(clientId, "_iccListeners", "notifyStkCommand",
                           [JSON.stringify(data)]);
        break;
      case "RIL:StkSessionEnd":
        this._deliverEvent(clientId, "_iccListeners", "notifyStkSessionEnd", null);
        break;
      case "RIL:IccOpenChannel":
        this.handleSimpleRequest(data.requestId, data.errorMsg,
                                 data.channel);
        break;
      case "RIL:IccCloseChannel":
        this.handleSimpleRequest(data.requestId, data.errorMsg, null);
        break;
      case "RIL:IccExchangeAPDU":
        this.handleIccExchangeAPDU(data);
        break;
      case "RIL:ReadIccContacts":
        this.handleReadIccContacts(data);
        break;
      case "RIL:UpdateIccContact":
        this.handleUpdateIccContact(data);
        break;
      case "RIL:MatchMvno":
        this.handleSimpleRequest(data.requestId, data.errorMsg, data.result);
        break;
      case "RIL:DataError":
        this.updateConnectionInfo(data, this.rilContexts[clientId].dataConnectionInfo);
        this._deliverEvent(clientId, "_mobileConnectionListeners", "notifyDataError",
                           [data.errorMsg]);
        break;
      case "RIL:GetCallForwardingOptions":
        this.handleGetCallForwardingOptions(data);
        break;
      case "RIL:SetCallForwardingOptions":
        this.handleSimpleRequest(data.requestId, data.errorMsg, null);
        break;
      case "RIL:GetCallBarringOptions":
        this.handleGetCallBarringOptions(data);
        break;
      case "RIL:SetCallBarringOptions":
        this.handleSimpleRequest(data.requestId, data.errorMsg, null);
        break;
      case "RIL:ChangeCallBarringPassword":
        this.handleSimpleRequest(data.requestId, data.errorMsg, null);
        break;
      case "RIL:GetCallWaitingOptions":
        this.handleSimpleRequest(data.requestId, data.errorMsg,
                                 data.enabled);
        break;
      case "RIL:SetCallWaitingOptions":
        this.handleSimpleRequest(data.requestId, data.errorMsg, null);
        break;
      case "RIL:CfStateChanged":
        this._deliverEvent(clientId,
                           "_mobileConnectionListeners",
                           "notifyCFStateChange",
                           [data.success, data.action,
                            data.reason, data.number,
                            data.timeSeconds, data.serviceClass]);
        break;
      case "RIL:GetCallingLineIdRestriction":
        this.handleGetCallingLineIdRestriction(data);
        break;
      case "RIL:SetCallingLineIdRestriction":
        this.handleSimpleRequest(data.requestId, data.errorMsg, null);
        break;
      case "RIL:CellBroadcastReceived": {
        // All CBS messages are to routed the listener for clientId 0 and
        // provide the |clientId| info by |CellBroadcastMessage.serviceId|.
        let message = new CellBroadcastMessage(clientId, data);
        this._deliverEvent(0, // route to clientId 0.
                           "_cellBroadcastListeners",
                           "notifyMessageReceived",
                           [message]);
        break;
      }
      case "RIL:SetRoamingPreference":
        this.handleSimpleRequest(data.requestId, data.errorMsg, null);
        break;
      case "RIL:GetRoamingPreference":
        this.handleSimpleRequest(data.requestId, data.errorMsg,
                                 data.mode);
        break;
      case "RIL:ExitEmergencyCbMode":
        this.handleExitEmergencyCbMode(data);
        break;
      case "RIL:EmergencyCbModeChanged":
        this._deliverEvent(clientId,
                           "_mobileConnectionListeners",
                           "notifyEmergencyCbModeChanged",
                           [data.active, data.timeoutMs]);
        break;
      case "RIL:SetRadioEnabled":
        this.handleSimpleRequest(data.requestId, data.errorMsg, null);
        break;
      case "RIL:RadioStateChanged":
        this.rilContexts[clientId].radioState = data;
        this._deliverEvent(clientId,
                           "_mobileConnectionListeners",
                           "notifyRadioStateChanged",
                           null);
        break;
      case "RIL:SetVoicePrivacyMode":
        this.handleSimpleRequest(data.requestId, data.errorMsg, null);
        break;
      case "RIL:GetVoicePrivacyMode":
        this.handleSimpleRequest(data.requestId, data.errorMsg,
                                 data.enabled);
        break;
      case "RIL:ClirModeChanged":
        this._deliverEvent(clientId,
                           "_mobileConnectionListeners",
                           "notifyClirModeChanged",
                           [data]);
        break;
    }
  },

  handleSimpleRequest: function(requestId, errorMsg, result) {
    if (errorMsg) {
      this.fireRequestError(requestId, errorMsg);
    } else {
      this.fireRequestSuccess(requestId, result);
    }
  },

  handleGetAvailableNetworks: function(message) {
    if (DEBUG) debug("handleGetAvailableNetworks: " + JSON.stringify(message));
    if (message.errorMsg) {
      if (DEBUG) {
        debug("Received error from getAvailableNetworks: " + message.errorMsg);
      }
      this.fireRequestError(message.requestId, message.errorMsg);
      return;
    }

    let requestId = message.requestId;
    let requestWindow = this._windowsMap[requestId];
    delete this._windowsMap[requestId];

    let networks = message.networks;
    for (let i = 0; i < networks.length; i++) {
      let network = networks[i];
      let info = new requestWindow.MozMobileNetworkInfo(network.shortName,
                                                        network.longName,
                                                        network.mcc,
                                                        network.mnc,
                                                        network.state);
      networks[i] = info;
    }

    this.fireRequestSuccess(message.requestId, networks);
  },

  handleIccExchangeAPDU: function(message) {
    if (message.errorMsg) {
      this.fireRequestError(message.requestId, message.errorMsg);
    } else {
      var result = [message.sw1, message.sw2, message.simResponse];
      this.fireRequestSuccess(message.requestId, result);
    }
  },

  handleReadIccContacts: function(message) {
    if (message.errorMsg) {
      this.fireRequestError(message.requestId, message.errorMsg);
      return;
    }

    let window = this._windowsMap[message.requestId];
    delete this._windowsMap[message.requestId];
    let contacts = message.contacts;
    let result = contacts.map(function(c) {
      let prop = {name: [c.alphaId], tel: [{value: c.number}]};

      if (c.email) {
        prop.email = [{value: c.email}];
      }

      // ANR - Additional Number
      let anrLen = c.anr ? c.anr.length : 0;
      for (let i = 0; i < anrLen; i++) {
        prop.tel.push({value: c.anr[i]});
      }

      let contact = new window.mozContact(prop);
      contact.id = c.contactId;
      return contact;
    });

    this.fireRequestSuccess(message.requestId, result);
  },

  handleUpdateIccContact: function(message) {
    if (message.errorMsg) {
      this.fireRequestError(message.requestId, message.errorMsg);
      return;
    }

    let window = this._windowsMap[message.requestId];
    delete this._windowsMap[message.requestId];
    let iccContact = message.contact;
    let prop = {name: [iccContact.alphaId], tel: [{value: iccContact.number}]};
    if (iccContact.email) {
      prop.email = [{value: iccContact.email}];
    }

    // ANR - Additional Number
    let anrLen = iccContact.anr ? iccContact.anr.length : 0;
    for (let i = 0; i < anrLen; i++) {
      prop.tel.push({value: iccContact.anr[i]});
    }

    let contact = new window.mozContact(prop);
    contact.id = iccContact.contactId;

    this.fireRequestSuccess(message.requestId, contact);
  },

  handleVoicemailNotification: function(clientId, message) {
    let changed = false;
    if (!this.voicemailStatuses[clientId]) {
      this.voicemailStatuses[clientId] = new VoicemailStatus(clientId);
    }

    let voicemailStatus = this.voicemailStatuses[clientId];
    if (voicemailStatus.hasMessages != message.active) {
      changed = true;
      voicemailStatus.hasMessages = message.active;
    }

    if (voicemailStatus.messageCount != message.msgCount) {
      changed = true;
      voicemailStatus.messageCount = message.msgCount;
    } else if (message.msgCount == -1) {
      // For MWI using DCS the message count is not available
      changed = true;
    }

    if (voicemailStatus.returnNumber != message.returnNumber) {
      changed = true;
      voicemailStatus.returnNumber = message.returnNumber;
    }

    if (voicemailStatus.returnMessage != message.returnMessage) {
      changed = true;
      voicemailStatus.returnMessage = message.returnMessage;
    }

    if (changed) {
      // To follow the event delivering scheme, we add a dummy clientId 0.
      // All voicemail events are routed to listener for client id 0.
      this._deliverEvent(0,
                         "_voicemailListeners",
                         "notifyStatusChanged",
                         [voicemailStatus]);
    }
  },

  _cfRulesToMobileCfInfo: function(rules) {
    for (let i = 0; i < rules.length; i++) {
      rules[i] = new MobileCallForwardingInfo(rules[i]);
    }
  },

  handleGetCallForwardingOptions: function(message) {
    if (message.errorMsg) {
      this.fireRequestError(message.requestId, message.errorMsg);
      return;
    }

    this._cfRulesToMobileCfInfo(message.rules);
    this.fireRequestSuccess(message.requestId, message.rules);
  },

  handleGetCallBarringOptions: function(message) {
    if (!message.success) {
      this.fireRequestError(message.requestId, message.errorMsg);
    } else {
      let options = new CallBarringOptions(message);
      this.fireRequestSuccess(message.requestId, options);
    }
  },

  handleGetCallingLineIdRestriction: function(message) {
    if (message.errorMsg) {
      this.fireRequestError(message.requestId, message.errorMsg);
      return;
    }

    let status = new DOMCLIRStatus(message);
    this.fireRequestSuccess(message.requestId, status);
  },

  handleExitEmergencyCbMode: function(message) {
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

  handleSendCancelMMI: function(message) {
    if (DEBUG) debug("handleSendCancelMMI " + JSON.stringify(message));
    let request = this.takeRequest(message.requestId);
    let requestWindow = this._windowsMap[message.requestId];
    delete this._windowsMap[message.requestId];
    if (!request) {
      return;
    }

    let success = message.success;

    // We expect to have an IMEI at this point if the request was supposed
    // to query for the IMEI, so getting a successful reply from the RIL
    // without containing an actual IMEI number is considered an error.
    if (message.mmiServiceCode === RIL.MMI_KS_SC_IMEI &&
        !message.statusMessage) {
        message.errorMsg = message.errorMsg ?
          message.errorMsg : RIL.GECKO_ERROR_GENERIC_FAILURE;
        success = false;
    }

    // MMI query call forwarding options request returns a set of rules that
    // will be exposed in the form of an array of MozCallForwardingOptions
    // instances.
    if (message.mmiServiceCode === RIL.MMI_KS_SC_CALL_FORWARDING &&
        message.additionalInformation) {
      this._cfRulesToMobileCfInfo(message.additionalInformation);
    }

    let result = {
      serviceCode: message.mmiServiceCode || "",
      additionalInformation: message.additionalInformation
    };

    if (success) {
      result.statusMessage = message.statusMessage;
      let mmiResult = new DOMMMIResult(result);
      Services.DOMRequest.fireSuccess(request, mmiResult);
    } else {
      let mmiError = new requestWindow.DOMMMIError(result.serviceCode,
                                                   message.errorMsg,
                                                   "",
                                                   result.additionalInformation);
      Services.DOMRequest.fireDetailedError(request, mmiError);
    }
  },

  _deliverEvent: function(clientId, listenerType, name, args) {
    if (!this[listenerType]) {
      return;
    }
    let thisListeners = this[listenerType][clientId];
    if (!thisListeners) {
      return;
    }

    let listeners = thisListeners.slice();
    for (let listener of listeners) {
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
        if (DEBUG) debug("listener for " + name + " threw an exception: " + e);
      }
    }
  },

  /**
   * Helper for guarding us against invalid reason values for call forwarding.
   */
  _isValidCallForwardingReason: function(reason) {
    switch (reason) {
      case Ci.nsIMobileConnectionProvider.CALL_FORWARD_REASON_UNCONDITIONAL:
      case Ci.nsIMobileConnectionProvider.CALL_FORWARD_REASON_MOBILE_BUSY:
      case Ci.nsIMobileConnectionProvider.CALL_FORWARD_REASON_NO_REPLY:
      case Ci.nsIMobileConnectionProvider.CALL_FORWARD_REASON_NOT_REACHABLE:
      case Ci.nsIMobileConnectionProvider.CALL_FORWARD_REASON_ALL_CALL_FORWARDING:
      case Ci.nsIMobileConnectionProvider.CALL_FORWARD_REASON_ALL_CONDITIONAL_CALL_FORWARDING:
        return true;
      default:
        return false;
    }
  },

  /**
   * Helper for guarding us against invalid action values for call forwarding.
   */
  _isValidCallForwardingAction: function(action) {
    switch (action) {
      case Ci.nsIMobileConnectionProvider.CALL_FORWARD_ACTION_DISABLE:
      case Ci.nsIMobileConnectionProvider.CALL_FORWARD_ACTION_ENABLE:
      case Ci.nsIMobileConnectionProvider.CALL_FORWARD_ACTION_REGISTRATION:
      case Ci.nsIMobileConnectionProvider.CALL_FORWARD_ACTION_ERASURE:
        return true;
      default:
        return false;
    }
  },

  /**
   * Helper for guarding us against invalid program values for call barring.
   */
  _isValidCallBarringProgram: function(program) {
    switch (program) {
      case Ci.nsIMobileConnectionProvider.CALL_BARRING_PROGRAM_ALL_OUTGOING:
      case Ci.nsIMobileConnectionProvider.CALL_BARRING_PROGRAM_OUTGOING_INTERNATIONAL:
      case Ci.nsIMobileConnectionProvider.CALL_BARRING_PROGRAM_OUTGOING_INTERNATIONAL_EXCEPT_HOME:
      case Ci.nsIMobileConnectionProvider.CALL_BARRING_PROGRAM_ALL_INCOMING:
      case Ci.nsIMobileConnectionProvider.CALL_BARRING_PROGRAM_INCOMING_ROAMING:
        return true;
      default:
        return false;
    }
  },

  /**
   * Helper for guarding us against invalid options for call barring.
   */
  _isValidCallBarringOptions: function(options, usedForSetting) {
    if (!options ||
        options.serviceClass == null ||
        !this._isValidCallBarringProgram(options.program)) {
      return false;
    }

    // For setting callbarring options, |enabled| and |password| are required.
    if (usedForSetting && (options.enabled == null || options.password == null)) {
      return false;
    }

    return true;
  },

  /**
   * Helper for guarding us against invalid mode for clir.
   */
  _isValidClirMode: function(mode) {
    switch (mode) {
      case Ci.nsIMobileConnectionProvider.CLIR_DEFAULT:
      case Ci.nsIMobileConnectionProvider.CLIR_INVOCATION:
      case Ci.nsIMobileConnectionProvider.CLIR_SUPPRESSION:
        return true;
      default:
        return false;
    }
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([RILContentHelper,
                                                     IccCardLockError]);
