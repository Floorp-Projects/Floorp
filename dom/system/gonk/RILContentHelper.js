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

var RIL = {};
Cu.import("resource://gre/modules/ril_consts.js", RIL);

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
const DOMMMIERROR_CID =
  Components.ID("{6b204c42-7928-4e71-89ad-f90cd82aff96}");
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
  "RIL:SetCallForwardingOption",
  "RIL:GetCallForwardingOption",
  "RIL:SetCallBarringOption",
  "RIL:GetCallBarringOption",
  "RIL:ChangeCallBarringPassword",
  "RIL:SetCallWaitingOption",
  "RIL:GetCallWaitingOption",
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
  "RIL:SetVoicePrivacyMode",
  "RIL:GetVoicePrivacyMode",
  "RIL:OtaStatusChanged"
];

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsISyncMessageSender");

XPCOMUtils.defineLazyGetter(this, "gNumRadioInterfaces", function () {
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
  min: null
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

  gsmLocationAreaCode: -1,
  gsmCellId: -1,
  cdmaBaseStationId: -1,
  cdmaBaseStationLatitude: -2147483648,
  cdmaBaseStationLongitude: -2147483648,
  cdmaSystemId: -1,
  cdmaNetworkId: -1
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

function CallBarringOption(option) {
  this.program = option.program;
  this.enabled = option.enabled;
  this.password = option.password;
  this.serviceClass = option.serviceClass;
}
CallBarringOption.prototype = {
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

function DOMMMIError() {
}
DOMMMIError.prototype = {
  classDescription: "DOMMMIError",
  classID:          DOMMMIERROR_CID,
  contractID:       "@mozilla.org/dom/mmi-error;1",
  QueryInterface:   XPCOMUtils.generateQI([Ci.nsISupports]),
  __init: function(serviceCode,
                   name,
                   message,
                   additionalInformation) {
    this.__DOM_IMPL__.init(name, message);
    this.serviceCode = serviceCode;
    this.additionalInformation = additionalInformation;
  },
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

  this.rilContext = {
    cardState:            RIL.GECKO_CARDSTATE_UNKNOWN,
    networkSelectionMode: RIL.GECKO_NETWORK_SELECTION_UNKNOWN,
    iccInfo:              null,
    voiceConnectionInfo:  new MobileConnectionInfo(),
    dataConnectionInfo:   new MobileConnectionInfo()
  };
  this.voicemailInfo = new VoicemailInfo();
  this.voicemailDefaultServiceId = this.getVoicemailDefaultServiceId();

  this.initDOMRequestHelper(/* aWindow */ null, RIL_IPC_MSG_NAMES);
  this._windowsMap = [];

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
                                         Ci.nsISupportsWeakReference]),
  classID:   RILCONTENTHELPER_CID,
  classInfo: XPCOMUtils.generateCI({classID: RILCONTENTHELPER_CID,
                                    classDescription: "RILContentHelper",
                                    interfaces: [Ci.nsIMobileConnectionProvider,
                                                 Ci.nsICellBroadcastProvider,
                                                 Ci.nsIVoicemailProvider,
                                                 Ci.nsIIccProvider]}),

  updateDebugFlag: function updateDebugFlag() {
    try {
      DEBUG = RIL.DEBUG_CONTENT_HELPER ||
              Services.prefs.getBoolPref(kPrefRilDebuggingEnabled);
    } catch (e) {}
  },

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
  updateIccInfo: function updateIccInfo(newInfo) {
    // Card is not detected, clear iccInfo to null.
    if (!newInfo || !newInfo.iccType) {
      this.rilContext.iccInfo = null;
      return;
    }

    // If iccInfo is null, new corresponding object based on iccType.
    if (!this.rilContext.iccInfo) {
      if (newInfo.iccType === "ruim" || newInfo.iccType === "csim") {
        this.rilContext.iccInfo = new CdmaIccInfo();
      } else {
        this.rilContext.iccInfo = new GsmIccInfo();
      }
    }

    this.updateInfo(newInfo, this.rilContext.iccInfo);
  },

  _windowsMap: null,

  rilContext: null,

  getRilContext: function getRilContext() {
    // Update ril context by sending IPC message to chrome only when the first
    // time we require it. The information will be updated by following info
    // changed messages.
    this.getRilContext = function getRilContext() {
      return this.rilContext;
    };

    let rilContext =
      cpmm.sendSyncMessage("RIL:GetRilContext", {clientId: 0})[0];
    if (!rilContext) {
      debug("Received null rilContext from chrome process.");
      return;
    }
    this.rilContext.cardState = rilContext.cardState;
    this.rilContext.networkSelectionMode = rilContext.networkSelectionMode;
    this.updateIccInfo(rilContext.iccInfo);
    this.updateConnectionInfo(rilContext.voice, this.rilContext.voiceConnectionInfo);
    this.updateConnectionInfo(rilContext.data, this.rilContext.dataConnectionInfo);

    return this.rilContext;
  },

  /**
   * nsIMobileConnectionProvider
   */

  get iccInfo() {
    let context = this.getRilContext();
    return context && context.iccInfo;
  },

  get voiceConnectionInfo() {
    let context = this.getRilContext();
    return context && context.voiceConnectionInfo;
  },

  get dataConnectionInfo() {
    let context = this.getRilContext();
    return context && context.dataConnectionInfo;
  },

  get cardState() {
    let context = this.getRilContext();
    return context && context.cardState;
  },

  get networkSelectionMode() {
    let context = this.getRilContext();
    return context && context.networkSelectionMode;
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

    cpmm.sendAsyncMessage("RIL:GetAvailableNetworks", {
      clientId: 0,
      data: {
        requestId: requestId
      }
    });
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

    if (this.rilContext.networkSelectionMode == RIL.GECKO_NETWORK_SELECTION_MANUAL &&
        this.rilContext.voiceConnectionInfo.network === network) {

      // Already manually selected this network, so schedule
      // onsuccess to be fired on the next tick
      this.dispatchFireRequestSuccess(requestId, null);
      return request;
    }

    this._selectingNetwork = network;

    cpmm.sendAsyncMessage("RIL:SelectNetwork", {
      clientId: 0,
      data: {
        requestId: requestId,
        mnc: network.mnc,
        mcc: network.mcc
      }
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

    if (this.rilContext.networkSelectionMode == RIL.GECKO_NETWORK_SELECTION_AUTOMATIC) {
      // Already using automatic selection mode, so schedule
      // onsuccess to be be fired on the next tick
      this.dispatchFireRequestSuccess(requestId, null);
      return request;
    }

    this._selectingNetwork = "automatic";
    cpmm.sendAsyncMessage("RIL:SelectNetworkAuto", {
      clientId: 0,
      data: {
        requestId: requestId
      }
    });
    return request;
  },

  setRoamingPreference: function setRoamingPreference(window, mode) {
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
      clientId: 0,
      data: {
        requestId: requestId,
        mode: mode
      }
    });
    return request;
  },

  getRoamingPreference: function getRoamingPreference(window) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }

    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);

    cpmm.sendAsyncMessage("RIL:GetRoamingPreference", {
      clientId: 0,
      data: {
        requestId: requestId
      }
    });
    return request;
  },

  setVoicePrivacyMode: function setVoicePrivacyMode(window, enabled) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }

    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);

    cpmm.sendAsyncMessage("RIL:SetVoicePrivacyMode", {
      clientId: 0,
      data: {
        requestId: requestId,
        enabled: enabled
      }
    });
    return request;
  },

  getVoicePrivacyMode: function getVoicePrivacyMode(window) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }

    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);

    cpmm.sendAsyncMessage("RIL:GetVoicePrivacyMode", {
      clientId: 0,
      data: {
        requestId: requestId
      }
    });
    return request;
  },

  getCardLockState: function getCardLockState(window, lockType) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);
    this._windowsMap[requestId] = window;

    cpmm.sendAsyncMessage("RIL:GetCardLockState", {
      clientId: 0,
      data: {
        lockType: lockType,
        requestId: requestId
      }
    });
    return request;
  },

  unlockCardLock: function unlockCardLock(window, info) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    let request = Services.DOMRequest.createRequest(window);
    info.requestId = this.getRequestId(request);
    this._windowsMap[info.requestId] = window;

    cpmm.sendAsyncMessage("RIL:UnlockCardLock", {
      clientId: 0,
      data: info
    });
    return request;
  },

  setCardLock: function setCardLock(window, info) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    let request = Services.DOMRequest.createRequest(window);
    info.requestId = this.getRequestId(request);
    this._windowsMap[info.requestId] = window;

    cpmm.sendAsyncMessage("RIL:SetCardLock", {
      clientId: 0,
      data: info
    });
    return request;
  },

  getCardLockRetryCount: function getCardLockRetryCount(window, lockType) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);
    cpmm.sendAsyncMessage("RIL:GetCardLockRetryCount", {
      clientId: 0,
      data: {
        lockType: lockType,
        requestId: requestId
      }
    });
    return request;
  },

  sendMMI: function sendMMI(window, mmi) {
    debug("Sending MMI " + mmi);
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
      clientId: 0,
      data: {
        mmi: mmi,
        requestId: requestId
      }
    });
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
    cpmm.sendAsyncMessage("RIL:CancelMMI", {
      clientId: 0,
      data: {
        requestId: requestId
      }
    });
    return request;
  },

  sendStkResponse: function sendStkResponse(window, command, response) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    response.command = command;
    cpmm.sendAsyncMessage("RIL:SendStkResponse", {
      clientId: 0,
      data: response
    });
  },

  sendStkMenuSelection: function sendStkMenuSelection(window,
                                                      itemIdentifier,
                                                      helpRequested) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    cpmm.sendAsyncMessage("RIL:SendStkMenuSelection", {
      clientId: 0,
      data: {
        itemIdentifier: itemIdentifier,
        helpRequested: helpRequested
      }
    });
  },

  sendStkTimerExpiration: function sendStkTimerExpiration(window,
                                                          timer) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    cpmm.sendAsyncMessage("RIL:SendStkTimerExpiration", {
      clientId: 0,
      data: {
        timer: timer
      }
    });
  },

  sendStkEventDownload: function sendStkEventDownload(window,
                                                      event) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    cpmm.sendAsyncMessage("RIL:SendStkEventDownload", {
      clientId: 0,
      data: {
        event: event
      }
    });
  },

  iccOpenChannel: function iccOpenChannel(window,
                                          aid) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }

    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);

    cpmm.sendAsyncMessage("RIL:IccOpenChannel", {
      clientId: 0,
      data: {
        requestId: requestId,
        aid: aid
      }
    });
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
    cpmm.sendAsyncMessage("RIL:IccExchangeAPDU", {
      clientId: 0,
      data: {
        requestId: requestId,
        channel: channel,
        apdu: apdu
      }
    });
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

    cpmm.sendAsyncMessage("RIL:IccCloseChannel", {
      clientId: 0,
      data: {
        requestId: requestId,
        channel: channel
      }
    });
    return request;
  },

  readContacts: function readContacts(window, contactType) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }

    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);
    this._windowsMap[requestId] = window;

    cpmm.sendAsyncMessage("RIL:ReadIccContacts", {
      clientId: 0,
      data: {
        requestId: requestId,
        contactType: contactType
      }
    });
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

    iccContact.id = contact.id;

    cpmm.sendAsyncMessage("RIL:UpdateIccContact", {
      clientId: 0,
      data: {
        requestId: requestId,
        contactType: contactType,
        contact: iccContact,
        pin2: pin2
      }
    });

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
      this.dispatchFireRequestError(requestId,
                                    RIL.GECKO_ERROR_INVALID_PARAMETER);
      return request;
    }

    cpmm.sendAsyncMessage("RIL:GetCallForwardingOption", {
      clientId: 0,
      data: {
        requestId: requestId,
        reason: reason
      }
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
      this.dispatchFireRequestError(requestId,
                                    RIL.GECKO_ERROR_INVALID_PARAMETER);
      return request;
    }

    cpmm.sendAsyncMessage("RIL:SetCallForwardingOption", {
      clientId: 0,
      data: {
        requestId: requestId,
        active: cfInfo.active,
        action: cfInfo.action,
        reason: cfInfo.reason,
        number: cfInfo.number,
        timeSeconds: cfInfo.timeSeconds
      }
    });

    return request;
  },

  getCallBarringOption: function getCallBarringOption(window, option) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);

    if (DEBUG) debug("getCallBarringOption: " + JSON.stringify(option));
    if (!this._isValidCallBarringOption(option)) {
      this.dispatchFireRequestError(requestId,
                                    RIL.GECKO_ERROR_INVALID_PARAMETER);
      return request;
    }

    cpmm.sendAsyncMessage("RIL:GetCallBarringOption", {
      clientId: 0,
      data: {
        requestId: requestId,
        program: option.program,
        password: option.password,
        serviceClass: option.serviceClass
      }
    });
    return request;
  },

  setCallBarringOption: function setCallBarringOption(window, option) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);

    if (DEBUG) debug("setCallBarringOption: " + JSON.stringify(option));
    if (!this._isValidCallBarringOption(option, true)) {
      this.dispatchFireRequestError(requestId,
                                    RIL.GECKO_ERROR_INVALID_PARAMETER);
      return request;
    }

    cpmm.sendAsyncMessage("RIL:SetCallBarringOption", {
      clientId: 0,
      data: {
        requestId: requestId,
        program: option.program,
        enabled: option.enabled,
        password: option.password,
        serviceClass: option.serviceClass
      }
    });
    return request;
  },

  changeCallBarringPassword: function changeCallBarringPassword(window, info) {
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
      clientId: 0,
      data: info
    });

    return request;
  },

  getCallWaitingOption: function getCallWaitingOption(window) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);

    cpmm.sendAsyncMessage("RIL:GetCallWaitingOption", {
      clientId: 0,
      data: {
        requestId: requestId
      }
    });

    return request;
  },

  setCallWaitingOption: function setCallWaitingOption(window, enabled) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);

    cpmm.sendAsyncMessage("RIL:SetCallWaitingOption", {
      clientId: 0,
      data: {
        requestId: requestId,
        enabled: enabled
      }
    });

    return request;
  },

  getCallingLineIdRestriction: function getCallingLineIdRestriction(window) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);

    cpmm.sendAsyncMessage("RIL:GetCallingLineIdRestriction", {
      clientId: 0,
      data: {
        requestId: requestId
      }
    });

    return request;
  },

  setCallingLineIdRestriction:
    function setCallingLineIdRestriction(window, clirMode) {

    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);

    cpmm.sendAsyncMessage("RIL:SetCallingLineIdRestriction", {
      clientId: 0,
      data: {
        requestId: requestId,
        clirMode: clirMode
      }
    });

    return request;
  },

  exitEmergencyCbMode: function exitEmergencyCbMode(window) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);

    cpmm.sendAsyncMessage("RIL:ExitEmergencyCbMode", {
      clientId: 0,
      data: {
        requestId: requestId,
      }
    });

    return request;
  },

  _mobileConnectionListeners: null,
  _cellBroadcastListeners: null,
  _voicemailListeners: null,
  _iccListeners: null,

  voicemailStatus: null,

  voicemailDefaultServiceId: 0,
  getVoicemailDefaultServiceId: function getVoicemailDefaultServiceId() {
    let id = Services.prefs.getIntPref(kPrefVoicemailDefaultServiceId);

    if (id >= gNumRadioInterfaces || id < 0) {
      id = 0;
    }

    return id;
  },

  getVoicemailInfo: function getVoicemailInfo() {
    // Get voicemail infomation by IPC only on first time.
    this.getVoicemailInfo = function getVoicemailInfo() {
      return this.voicemailInfo;
    };

    let voicemailInfo =
      cpmm.sendSyncMessage("RIL:GetVoicemailInfo", {clientId: 0})[0];
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

  // nsIObserver

  observe: function observe(subject, topic, data) {
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

  fireRequestDetailedError: function fireRequestDetailedError(requestId, detailedError) {
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

  receiveMessage: function receiveMessage(msg) {
    let request;
    debug("Received message '" + msg.name + "': " + JSON.stringify(msg.json));

    let data = msg.json.data;
    switch (msg.name) {
      case "RIL:CardStateChanged":
        if (this.rilContext.cardState != data.cardState) {
          this.rilContext.cardState = data.cardState;
          this._deliverEvent("_iccListeners",
                             "notifyCardStateChanged",
                             null);
        }
        break;
      case "RIL:IccInfoChanged":
        this.updateIccInfo(data);
        this._deliverEvent("_iccListeners", "notifyIccInfoChanged", null);
        break;
      case "RIL:VoiceInfoChanged":
        this.updateConnectionInfo(data,
                                  this.rilContext.voiceConnectionInfo);
        this._deliverEvent("_mobileConnectionListeners",
                           "notifyVoiceChanged",
                           null);
        break;
      case "RIL:DataInfoChanged":
        this.updateConnectionInfo(data,
                                  this.rilContext.dataConnectionInfo);
        this._deliverEvent("_mobileConnectionListeners",
                           "notifyDataChanged",
                           null);
        break;
      case "RIL:OtaStatusChanged":
        this._deliverEvent("_mobileConnectionListeners",
                           "notifyOtaStatusChanged",
                           [data]);
        break;
      case "RIL:GetAvailableNetworks":
        this.handleGetAvailableNetworks(data);
        break;
      case "RIL:NetworkSelectionModeChanged":
        this.rilContext.networkSelectionMode = data.mode;
        break;
      case "RIL:SelectNetwork":
        this.handleSelectNetwork(data,
                                 RIL.GECKO_NETWORK_SELECTION_MANUAL);
        break;
      case "RIL:SelectNetworkAuto":
        this.handleSelectNetwork(data,
                                 RIL.GECKO_NETWORK_SELECTION_AUTOMATIC);
        break;
      case "RIL:VoicemailNotification":
        this.handleVoicemailNotification(data);
        break;
      case "RIL:VoicemailInfoChanged":
        this.updateInfo(data, this.voicemailInfo);
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
        this._deliverEvent("_mobileConnectionListeners",
                           "notifyUssdReceived",
                           [data.message, data.sessionEnded]);
        break;
      case "RIL:SendMMI":
      case "RIL:CancelMMI":
        this.handleSendCancelMMI(data);
        break;
      case "RIL:StkCommand":
        this._deliverEvent("_iccListeners", "notifyStkCommand",
                           [JSON.stringify(data)]);
        break;
      case "RIL:StkSessionEnd":
        this._deliverEvent("_iccListeners", "notifyStkSessionEnd", null);
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
        this.handleSimpleRequest(data.requestId, data.errorMsg, null);
        break;
      case "RIL:DataError":
        this.updateConnectionInfo(data, this.rilContext.dataConnectionInfo);
        this._deliverEvent("_mobileConnectionListeners", "notifyDataError",
                           [data.errorMsg]);
        break;
      case "RIL:GetCallForwardingOption":
        this.handleGetCallForwardingOption(data);
        break;
      case "RIL:SetCallForwardingOption":
        this.handleSimpleRequest(data.requestId, data.errorMsg, null);
        break;
      case "RIL:GetCallBarringOption":
        this.handleGetCallBarringOption(data);
        break;
      case "RIL:SetCallBarringOption":
        this.handleSimpleRequest(data.requestId, data.errorMsg, null);
        break;
      case "RIL:ChangeCallBarringPassword":
        this.handleSimpleRequest(data.requestId, data.errorMsg, null);
        break;
      case "RIL:GetCallWaitingOption":
        this.handleSimpleRequest(data.requestId, data.errorMsg,
                                 data.enabled);
        break;
      case "RIL:SetCallWaitingOption":
        this.handleSimpleRequest(data.requestId, data.errorMsg, null);
        break;
      case "RIL:CfStateChanged":
        this._deliverEvent("_mobileConnectionListeners",
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
        let message = new CellBroadcastMessage(data);
        this._deliverEvent("_cellBroadcastListeners",
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
        this._deliverEvent("_mobileConnectionListeners",
                           "notifyEmergencyCbModeChanged",
                           [data.active, data.timeoutMs]);
        break;
      case "RIL:SetVoicePrivacyMode":
        this.handleSimpleRequest(data.requestId, data.errorMsg, null);
        break;
      case "RIL:GetVoicePrivacyMode":
        this.handleSimpleRequest(data.requestId, data.errorMsg,
                                 data.enabled);
        break;
    }
  },

  handleSimpleRequest: function handleSimpleRequest(requestId, errorMsg, result) {
    if (errorMsg) {
      this.fireRequestError(requestId, errorMsg);
    } else {
      this.fireRequestSuccess(requestId, result);
    }
  },

  handleGetAvailableNetworks: function handleGetAvailableNetworks(message) {
    debug("handleGetAvailableNetworks: " + JSON.stringify(message));
    if (message.errorMsg) {
      debug("Received error from getAvailableNetworks: " + message.errorMsg);
      this.fireRequestError(message.requestId, message.errorMsg);
      return;
    }

    let networks = message.networks;
    for (let i = 0; i < networks.length; i++) {
      let network = networks[i];
      let info = new MobileNetworkInfo();
      this.updateInfo(network, info);
      networks[i] = info;
    }

    this.fireRequestSuccess(message.requestId, networks);
  },

  handleSelectNetwork: function handleSelectNetwork(message, mode) {
    this._selectingNetwork = null;
    this.rilContext.networkSelectionMode = mode;

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

  handleReadIccContacts: function handleReadIccContacts(message) {
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
      contact.id = message.iccid + c.recordId;
      return contact;
    });

    this.fireRequestSuccess(message.requestId, result);
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
    } else if (message.msgCount == -1) {
      // For MWI using DCS the message count is not available
      changed = true;
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
                         "notifyStatusChanged",
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
    if (message.errorMsg) {
      this.fireRequestError(message.requestId, message.errorMsg);
      return;
    }

    this._cfRulesToMobileCfInfo(message.rules);
    this.fireRequestSuccess(message.requestId, message.rules);
  },

  handleGetCallBarringOption: function handleGetCallBarringOption(message) {
    if (!message.success) {
      this.fireRequestError(message.requestId, message.errorMsg);
    } else {
      let option = new CallBarringOption(message);
      this.fireRequestSuccess(message.requestId, option);
    }
  },

  handleGetCallingLineIdRestriction:
    function handleGetCallingLineIdRestriction(message) {
    if (message.errorMsg) {
      this.fireRequestError(message.requestId, message.errorMsg);
      return;
    }

    let status = new DOMCLIRStatus(message);
    this.fireRequestSuccess(message.requestId, status);
  },

  handleExitEmergencyCbMode: function handleExitEmergencyCbMode(message) {
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

  handleSendCancelMMI: function handleSendCancelMMI(message) {
    debug("handleSendCancelMMI " + JSON.stringify(message));
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
    // will be exposed in the form of an array of nsIDOMMozMobileCFInfo
    // instances.
    if (message.mmiServiceCode === RIL.MMI_KS_SC_CALL_FORWARDING &&
        message.additionalInformation) {
      this._cfRulesToMobileCfInfo(message.additionalInformation);
    }

    let result = {
      serviceCode: message.mmiServiceCode,
      additionalInformation: message.additionalInformation
    };

    if (success) {
      result.statusMessage = message.statusMessage;
      let mmiResult = new DOMMMIResult(result);
      Services.DOMRequest.fireSuccess(request, mmiResult);
    } else {
      let mmiError = new requestWindow.DOMMMIError(result.serviceCode,
                                                   message.errorMsg,
                                                   null,
                                                   result.additionalInformation);
      Services.DOMRequest.fireDetailedError(request, mmiError);
    }
  },

  _deliverEvent: function _deliverEvent(listenerType, name, args) {
    let thisListeners = this[listenerType];
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
  },

  /**
   * Helper for guarding us against invalid program values for call barring.
   */
  _isValidCallBarringProgram: function _isValidCallBarringProgram(program) {
    switch (program) {
      case Ci.nsIDOMMozMobileConnection.CALL_BARRING_PROGRAM_ALL_OUTGOING:
      case Ci.nsIDOMMozMobileConnection.CALL_BARRING_PROGRAM_OUTGOING_INTERNATIONAL:
      case Ci.nsIDOMMozMobileConnection.CALL_BARRING_PROGRAM_OUTGOING_INTERNATIONAL_EXCEPT_HOME:
      case Ci.nsIDOMMozMobileConnection.CALL_BARRING_PROGRAM_ALL_INCOMING:
      case Ci.nsIDOMMozMobileConnection.CALL_BARRING_PROGRAM_INCOMING_ROAMING:
        return true;
      default:
        return false;
    }
  },

  /**
   * Helper for guarding us against invalid option for call barring.
   */
  _isValidCallBarringOption:
      function _isValidCallBarringOption(option, usedForSetting) {
    if (!option ||
        option.serviceClass == null ||
        !this._isValidCallBarringProgram(option.program)) {
      return false;
    }

    // For setting callbarring option, |enabled| and |password| are required.
    if (usedForSetting && (option.enabled == null || option.password == null)) {
      return false;
    }

    return true;
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([RILContentHelper,
                                                     DOMMMIError,
                                                     IccCardLockError]);

