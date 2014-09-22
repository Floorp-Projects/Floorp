/* -*- Mode: js; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/systemlibs.js");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

var RIL = {};
Cu.import("resource://gre/modules/ril_consts.js", RIL);

const GONK_MOBILECONNECTIONSERVICE_CONTRACTID =
  "@mozilla.org/mobileconnection/gonkmobileconnectionservice;1";

const GONK_MOBILECONNECTIONSERVICE_CID =
  Components.ID("{0c9c1a96-2c72-4c55-9e27-0ca73eb16f63}");
const MOBILECONNECTIONINFO_CID =
  Components.ID("{8162b3c0-664b-45f6-96cd-f07b4e193b0e}");
const MOBILENETWORKINFO_CID =
  Components.ID("{a6c8416c-09b4-46d1-bf29-6520d677d085}");
const MOBILECELLINFO_CID =
  Components.ID("{0635d9ab-997e-4cdf-84e7-c1883752dff3}");

const NS_XPCOM_SHUTDOWN_OBSERVER_ID      = "xpcom-shutdown";
const NS_PREFBRANCH_PREFCHANGE_TOPIC_ID  = "nsPref:changed";
const NS_NETWORK_ACTIVE_CHANGED_TOPIC_ID = "network-active-changed";

const kPrefRilDebuggingEnabled = "ril.debugging.enabled";

XPCOMUtils.defineLazyServiceGetter(this, "gSystemMessenger",
                                   "@mozilla.org/system-message-internal;1",
                                   "nsISystemMessagesInternal");

XPCOMUtils.defineLazyServiceGetter(this, "gNetworkManager",
                                   "@mozilla.org/network/manager;1",
                                   "nsINetworkManager");

XPCOMUtils.defineLazyServiceGetter(this, "gRadioInterfaceLayer",
                                   "@mozilla.org/ril;1",
                                   "nsIRadioInterfaceLayer");

let DEBUG = RIL.DEBUG_RIL;
function debug(s) {
  dump("MobileConnectionService: " + s + "\n");
}

function MobileNetworkInfo() {
  this.shortName = null;
  this.longName = null;
  this.mcc = null;
  this.mnc = null;
  this.stat = null;
}
MobileNetworkInfo.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIMobileNetworkInfo]),
  classID:        MOBILENETWORKINFO_CID,
  classInfo:      XPCOMUtils.generateCI({
    classID:          MOBILENETWORKINFO_CID,
    classDescription: "MobileNetworkInfo",
    interfaces:       [Ci.nsIMobileNetworkInfo]
  })
};

function MobileCellInfo() {
  this.gsmLocationAreaCode = -1;
  this.gsmCellId = -1;
  this.cdmaBaseStationId = -1;
  this.cdmaBaseStationLatitude = -2147483648;
  this.cdmaBaseStationLongitude = -2147483648;
  this.cdmaSystemId = -1;
  this.cdmaNetworkId = -1;
}
MobileCellInfo.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIMobileCellInfo]),
  classID:        MOBILECELLINFO_CID,
  classInfo:      XPCOMUtils.generateCI({
    classID:          MOBILECELLINFO_CID,
    classDescription: "MobileCellInfo",
    interfaces:       [Ci.nsIMobileCellInfo]
  })
};

function MobileConnectionInfo() {}
MobileConnectionInfo.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIMobileConnectionInfo]),
  classID: MOBILECONNECTIONINFO_CID,
  classInfo: XPCOMUtils.generateCI({
    classID: MOBILECONNECTIONINFO_CID,
    classDescription: "MobileConnectionInfo",
    interfaces: [Ci.nsIMobileConnectionInfo]
  }),

  state: null,
  connected: false,
  emergencyCallsOnly: false,
  roaming: false,
  network: null,
  cell: null,
  type: null,
  signalStrength: null,
  relSignalStrength: null
};

function CallForwardingOptions(aOptions) {
  this.active = aOptions.active;
  this.action = aOptions.action;
  this.reason = aOptions.reason;
  this.number = aOptions.number;
  this.timeSeconds = aOptions.timeSeconds;
  this.serviceClass = aOptions.serviceClass;
}
CallForwardingOptions.prototype = {
  __exposedProps__ : {active: 'r',
                      action: 'r',
                      reason: 'r',
                      number: 'r',
                      timeSeconds: 'r',
                      serviceClass: 'r'},
};

function MMIResult(aOptions) {
  this.serviceCode = aOptions.serviceCode;
  this.statusMessage = aOptions.statusMessage;
  this.additionalInformation = aOptions.additionalInformation;
}
MMIResult.prototype = {
  __exposedProps__ : {serviceCode: 'r',
                      statusMessage: 'r',
                      additionalInformation: 'r'},
};

function MobileConnectionProvider(aClientId, aRadioInterface) {
  this._clientId = aClientId;
  this._radioInterface = aRadioInterface;
  this._operatorInfo = new MobileNetworkInfo();
  // An array of nsIMobileConnectionListener instances.
  this._listeners = [];

  this.supportedNetworkTypes = this._getSupportedNetworkTypes();
  this.voice = new MobileConnectionInfo();
  this.data = new MobileConnectionInfo();
}
MobileConnectionProvider.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIMobileConnection]),

  _clientId: null,
  _radioInterface: null,
  _operatorInfo: null,
  _listeners: null,

  /**
   * The networks that are currently trying to be selected (or "automatic").
   * This helps ensure that only one network per client is selected at a time.
   */
  _selectingNetwork: null,

  voice: null,
  data: null,
  iccId: null,
  networkSelectionMode: null,
  radioState: null,
  lastKnownNetwork: null,
  lastKnownHomeNetwork: null,
  supportedNetworkTypes: null,

  /**
   * A utility function to dump debug message.
   */
  _debug: function(aMessage) {
    dump("MobileConnectionProvider[" + this._clientId + "]: " + aMessage + "\n");
  },

  /**
   * A utility function to get supportedNetworkTypes from system property.
   */
  _getSupportedNetworkTypes: function() {
    let key = "ro.moz.ril." + this._clientId + ".network_types";
    let supportedNetworkTypes = libcutils.property_get(key, "").split(",");

    // If mozRIL system property is not available, fallback to AOSP system
    // property for support network types.
    if (supportedNetworkTypes.length === 1 && supportedNetworkTypes[0] === "") {
      key = "ro.telephony.default_network";
      let indexString = libcutils.property_get(key, "");
      let index = parseInt(indexString, 10);
      if (DEBUG) this._debug("Fallback to " + key + ": " + index)

      let networkTypes = RIL.RIL_PREFERRED_NETWORK_TYPE_TO_GECKO[index];
      supportedNetworkTypes = networkTypes ?
        networkTypes.replace("-auto", "", "g").split("/") :
        RIL.GECKO_SUPPORTED_NETWORK_TYPES_DEFAULT.split(",");
    }

    for (let type of supportedNetworkTypes) {
      // If the value in system property is not valid, use the default one which
      // is defined in ril_consts.js.
      if (RIL.GECKO_SUPPORTED_NETWORK_TYPES.indexOf(type) < 0) {
        if (DEBUG) {
          this._debug("Unknown network type: " + type);
        }
        supportedNetworkTypes =
          RIL.GECKO_SUPPORTED_NETWORK_TYPES_DEFAULT.split(",");
        break;
      }
    }
    if (DEBUG) {
      this._debug("Supported Network Types: " + supportedNetworkTypes);
    }
    return supportedNetworkTypes;
  },

  /**
   * Helper for guarding us against invalid reason values for call forwarding.
   */
  _isValidCallForwardingReason: function(aReason) {
    switch (aReason) {
      case Ci.nsIMobileConnection.CALL_FORWARD_REASON_UNCONDITIONAL:
      case Ci.nsIMobileConnection.CALL_FORWARD_REASON_MOBILE_BUSY:
      case Ci.nsIMobileConnection.CALL_FORWARD_REASON_NO_REPLY:
      case Ci.nsIMobileConnection.CALL_FORWARD_REASON_NOT_REACHABLE:
      case Ci.nsIMobileConnection.CALL_FORWARD_REASON_ALL_CALL_FORWARDING:
      case Ci.nsIMobileConnection.CALL_FORWARD_REASON_ALL_CONDITIONAL_CALL_FORWARDING:
        return true;
      default:
        return false;
    }
  },

  /**
   * Helper for guarding us against invalid action values for call forwarding.
   */
  _isValidCallForwardingAction: function(aAction) {
    switch (aAction) {
      case Ci.nsIMobileConnection.CALL_FORWARD_ACTION_DISABLE:
      case Ci.nsIMobileConnection.CALL_FORWARD_ACTION_ENABLE:
      case Ci.nsIMobileConnection.CALL_FORWARD_ACTION_REGISTRATION:
      case Ci.nsIMobileConnection.CALL_FORWARD_ACTION_ERASURE:
        return true;
      default:
        return false;
    }
  },

  /**
   * Helper for guarding us against invalid program values for call barring.
   */
  _isValidCallBarringProgram: function(aProgram) {
    switch (aProgram) {
      case Ci.nsIMobileConnection.CALL_BARRING_PROGRAM_ALL_OUTGOING:
      case Ci.nsIMobileConnection.CALL_BARRING_PROGRAM_OUTGOING_INTERNATIONAL:
      case Ci.nsIMobileConnection.CALL_BARRING_PROGRAM_OUTGOING_INTERNATIONAL_EXCEPT_HOME:
      case Ci.nsIMobileConnection.CALL_BARRING_PROGRAM_ALL_INCOMING:
      case Ci.nsIMobileConnection.CALL_BARRING_PROGRAM_INCOMING_ROAMING:
        return true;
      default:
        return false;
    }
  },

  /**
   * Helper for guarding us against invalid options for call barring.
   */
  _isValidCallBarringOptions: function(aOptions, aUsedForSetting) {
    if (!aOptions || aOptions.serviceClass == null ||
        !this._isValidCallBarringProgram(aOptions.program)) {
      return false;
    }

    // For setting callbarring options, |enabled| and |password| are required.
    if (aUsedForSetting &&
        (aOptions.enabled == null || aOptions.password == null)) {
      return false;
    }

    return true;
  },

  /**
   * Helper for guarding us against invalid mode for clir.
   */
  _isValidClirMode: function(aMode) {
    switch (aMode) {
      case Ci.nsIMobileConnection.CLIR_DEFAULT:
      case Ci.nsIMobileConnection.CLIR_INVOCATION:
      case Ci.nsIMobileConnection.CLIR_SUPPRESSION:
        return true;
      default:
        return false;
    }
  },

  /**
   * Fix the roaming. RIL can report roaming in some case it is not
   * really the case. See bug 787967
   */
  _checkRoamingBetweenOperators: function(aNetworkInfo) {
    // TODO: Bug 864489 - B2G RIL: use ipdl as IPC in MozIccManager
    // Should get iccInfo from GonkIccProvider.
    let iccInfo = this._radioInterface.rilContext.iccInfo;
    let operator = aNetworkInfo.network;
    let state = aNetworkInfo.state;

    if (!iccInfo || !operator ||
        state !== RIL.GECKO_MOBILE_CONNECTION_STATE_REGISTERED) {
      return false;
    }

    let spn = iccInfo.spn && iccInfo.spn.toLowerCase();
    let longName = operator.longName && operator.longName.toLowerCase();
    let shortName = operator.shortName && operator.shortName.toLowerCase();

    let equalsLongName = longName && (spn == longName);
    let equalsShortName = shortName && (spn == shortName);
    let equalsMcc = iccInfo.mcc == operator.mcc;

    let newRoaming = aNetworkInfo.roaming &&
                     !(equalsMcc && (equalsLongName || equalsShortName));
    if (newRoaming === aNetworkInfo.roaming) {
      return false;
    }

    aNetworkInfo.roaming = newRoaming;
    return true;
  },

  _updateConnectionInfo: function(aDestInfo, aSrcInfo) {
    let isUpdated = false;
    for (let key in aSrcInfo) {
      if (key === "network" || key === "cell") {
        // nsIMobileNetworkInfo and nsIMobileCellInfo are handled explicitly below.
        continue;
      }

      if (aDestInfo[key] !== aSrcInfo[key]) {
        isUpdated = true;
        aDestInfo[key] = aSrcInfo[key];
      }
    }

    // Make sure we also reset the operator and signal strength information
    // if we drop off the network.
    if (aDestInfo.state !== RIL.GECKO_MOBILE_CONNECTION_STATE_REGISTERED) {
      aDestInfo.cell = null;
      aDestInfo.network = null;
      aDestInfo.signalStrength = null;
      aDestInfo.relSignalStrength = null;
    } else {
      aDestInfo.network = this._operatorInfo;

      if (aSrcInfo.cell == null) {
        if (aDestInfo.cell != null) {
          isUpdated = true;
          aDestInfo.cell = null;
        }
      } else {
        if (aDestInfo.cell == null) {
          aDestInfo.cell = new MobileCellInfo();
        }
        isUpdated = this._updateInfo(aDestInfo.cell, aSrcInfo.cell) || isUpdated;
      }
    }

    // Check roaming state
    isUpdated = this._checkRoamingBetweenOperators(aDestInfo) || isUpdated;
    return isUpdated;
  },

  _updateInfo: function(aDestInfo, aSrcInfo) {
    let isUpdated = false;
    for (let key in aSrcInfo) {
      if (aDestInfo[key] !== aSrcInfo[key]) {
        isUpdated = true;
        aDestInfo[key] = aSrcInfo[key];
      }
    }
    return isUpdated;
  },

  _rulesToCallForwardingOptions: function(aRules) {
    for (let i = 0; i < aRules.length; i++) {
      let info = new CallForwardingOptions(aRules[i]);
      aRules[i] = info;
    }
  },

  _dispatchNotifyError: function(aCallback, aErrorMsg) {
    Services.tm.currentThread.dispatch(() => aCallback.notifyError(aErrorMsg),
                                       Ci.nsIThread.DISPATCH_NORMAL);
  },

  registerListener: function(aListener) {
    if (this._listeners.indexOf(aListener) >= 0) {
      throw Cr.NS_ERROR_UNEXPECTED;
    }

    this._listeners.push(aListener);
  },

  unregisterListener: function(aListener) {
    let index = this._listeners.indexOf(aListener);
    if (index >= 0) {
      this._listeners.splice(index, 1);
    }
  },

  deliverListenerEvent: function(aName, aArgs) {
    let listeners = this._listeners.slice();
    for (let listener of listeners) {
      if (listeners.indexOf(listener) === -1) {
        continue;
      }
      let handler = listener[aName];
      if (typeof handler != "function") {
        throw new Error("No handler for " + aName);
      }
      try {
        handler.apply(listener, aArgs);
      } catch (e) {
        if (DEBUG) {
          this._debug("listener for " + aName + " threw an exception: " + e);
        }
      }
    }
  },

  updateVoiceInfo: function(aNewInfo, aBatch = false) {
    let isUpdated = this._updateConnectionInfo(this.voice, aNewInfo);
    if (isUpdated && !aBatch) {
      this.deliverListenerEvent("notifyVoiceChanged");
    }
  },

  updateDataInfo: function(aNewInfo, aBatch = false) {
    // For the data connection, the `connected` flag indicates whether
    // there's an active data call. We get correct `connected` state here.
    let active = gNetworkManager.active;
    aNewInfo.connected = false;
    if (active &&
        active.type === Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE &&
        active.serviceId === this._clientId) {
      aNewInfo.connected = true;
    }

    let isUpdated = this._updateConnectionInfo(this.data, aNewInfo);
    if (isUpdated && !aBatch) {
      this.deliverListenerEvent("notifyDataChanged");
    }
  },

  updateOperatorInfo: function(aNewInfo, aBatch = false) {
    let isUpdated = this._updateInfo(this._operatorInfo, aNewInfo);

    // Update lastKnownNetwork
    if (this._operatorInfo.mcc && this._operatorInfo.mnc) {
      let network = this._operatorInfo.mcc + "-" + this._operatorInfo.mnc;
      if (this.lastKnownNetwork !== network) {
        if (DEBUG) {
          this._debug("lastKnownNetwork now is " + network);
        }

        this.lastKnownNetwork = network;
        this.deliverListenerEvent("notifyLastKnownNetworkChanged");
      }
    }

    // If the voice is unregistered, no need to send notification.
    if (this.voice.state !== RIL.GECKO_MOBILE_CONNECTION_STATE_REGISTERED &&
        isUpdated && !aBatch) {
      this.deliverListenerEvent("notifyVoiceChanged");
    }

    // If the data is unregistered, no need to send notification.
    if (this.data.state !== RIL.GECKO_MOBILE_CONNECTION_STATE_REGISTERED &&
        isUpdated && !aBatch) {
      this.deliverListenerEvent("notifyDataChanged");
    }
  },

  updateSignalInfo: function(aNewInfo, aBatch = false) {
    // If the voice is not registered, no need to update signal information.
    if (this.voice.state === RIL.GECKO_MOBILE_CONNECTION_STATE_REGISTERED) {
      if (this._updateInfo(this.voice, aNewInfo.voice) && !aBatch) {
        this.deliverListenerEvent("notifyVoiceChanged");
      }
    }

    // If the data is not registered, no need to update signal information.
    if (this.data.state === RIL.GECKO_MOBILE_CONNECTION_STATE_REGISTERED) {
      if (this._updateInfo(this.data, aNewInfo.data) && !aBatch) {
        this.deliverListenerEvent("notifyDataChanged");
      }
    }
  },

  updateIccId: function(aIccId) {
    if (this.iccId === aIccId) {
      return;
    }

    this.iccId = aIccId;
    this.deliverListenerEvent("notifyIccChanged");
  },

  updateRadioState: function(aRadioState) {
    if (this.radioState === aRadioState) {
      return;
    }

    this.radioState = aRadioState;
    this.deliverListenerEvent("notifyRadioStateChanged");
  },

  getSupportedNetworkTypes: function(aTypes) {
    aTypes.value = this.supportedNetworkTypes.slice();
    return aTypes.value.length;
  },

  getNetworks: function(aCallback) {
    this._radioInterface.sendWorkerMessage("getAvailableNetworks", null,
                                           (function(aResponse) {
      if (aResponse.errorMsg) {
        aCallback.notifyError(aResponse.errorMsg);
        return false;
      }

      let networks = aResponse.networks;
      for (let i = 0; i < networks.length; i++) {
        let info = new MobileNetworkInfo();
        this._updateInfo(info, networks[i]);
        networks[i] = info;
      }

      aCallback.notifyGetNetworksSuccess(networks.length, networks);
      return false;
    }).bind(this));
  },

  selectNetwork: function(aNetwork, aCallback) {
    if (!aNetwork ||
        isNaN(parseInt(aNetwork.mcc, 10)) ||
        isNaN(parseInt(aNetwork.mnc, 10))) {
      this._dispatchNotifyError(aCallback, RIL.GECKO_ERROR_INVALID_PARAMETER);
      return;
    }

    if (this._selectingNetwork) {
      this._dispatchNotifyError(aCallback, "AlreadySelectingANetwork");
      return;
    }

    let options = {mcc: aNetwork.mcc, mnc: aNetwork.mnc};
    this._selectingNetwork = options;
    this._radioInterface.sendWorkerMessage("selectNetwork", options,
                                           (function(aResponse) {
      this._selectingNetwork = null;
      if (aResponse.errorMsg) {
        aCallback.notifyError(aResponse.errorMsg);
        return false;
      }

      aCallback.notifySuccess();
      return false;
    }).bind(this));
  },

  selectNetworkAutomatically: function(aCallback) {
    if (this._selectingNetwork) {
      this._dispatchNotifyError(aCallback, "AlreadySelectingANetwork");
      return;
    }

    this._selectingNetwork = "automatic";
    this._radioInterface.sendWorkerMessage("selectNetworkAuto", null,
                                           (function(aResponse) {
      this._selectingNetwork = null;
      if (aResponse.errorMsg) {
        aCallback.notifyError(aResponse.errorMsg);
        return false;
      }

      aCallback.notifySuccess();
      return false;
    }).bind(this));
  },

  setPreferredNetworkType: function(aType, aCallback) {
    if (this.radioState !== RIL.GECKO_RADIOSTATE_ENABLED) {
      this._dispatchNotifyError(aCallback, RIL.GECKO_ERROR_RADIO_NOT_AVAILABLE);
      return;
    }

    this._radioInterface.sendWorkerMessage("setPreferredNetworkType",
                                           {type: aType},
                                           (function(aResponse) {
      if (aResponse.errorMsg) {
        aCallback.notifyError(aResponse.errorMsg);
        return false;
      }

      aCallback.notifySuccess();
      return false;
    }).bind(this));
  },

  getPreferredNetworkType: function(aCallback) {
    if (this.radioState !== RIL.GECKO_RADIOSTATE_ENABLED) {
      this._dispatchNotifyError(aCallback, RIL.GECKO_ERROR_RADIO_NOT_AVAILABLE);
      return;
    }

    this._radioInterface.sendWorkerMessage("getPreferredNetworkType", null,
                                           (function(aResponse) {
      if (aResponse.errorMsg) {
        aCallback.notifyError(aResponse.errorMsg);
        return false;
      }

      aCallback.notifySuccessWithString(aResponse.type);
      return false;
    }).bind(this));
  },

  setRoamingPreference: function(aMode, aCallback) {
    this._radioInterface.sendWorkerMessage("setRoamingPreference",
                                           {mode: aMode},
                                           (function(aResponse) {
      if (aResponse.errorMsg) {
        aCallback.notifyError(aResponse.errorMsg);
        return false;
      }

      aCallback.notifySuccess();
      return false;
    }).bind(this));
  },

  getRoamingPreference: function(aCallback) {
    this._radioInterface.sendWorkerMessage("queryRoamingPreference", null,
                                           (function(aResponse) {
      if (aResponse.errorMsg) {
        aCallback.notifyError(aResponse.errorMsg);
        return false;
      }

      aCallback.notifySuccessWithString(aResponse.mode);
      return false;
    }).bind(this));
  },

  setVoicePrivacyMode: function(aEnabled, aCallback) {
    this._radioInterface.sendWorkerMessage("setVoicePrivacyMode",
                                           {enabled: aEnabled},
                                           (function(aResponse) {
      if (aResponse.errorMsg) {
        aCallback.notifyError(aResponse.errorMsg);
        return false;
      }

      aCallback.notifySuccess();
      return false;
    }).bind(this));
  },

  getVoicePrivacyMode: function(aCallback) {
    this._radioInterface.sendWorkerMessage("queryVoicePrivacyMode", null,
                                           (function(aResponse) {
      if (aResponse.errorMsg) {
        aCallback.notifyError(aResponse.errorMsg);
        return false;
      }

      aCallback.notifySuccessWithBoolean(aResponse.enabled);
      return false;
    }).bind(this));
  },

  sendMMI: function(aMmi, aCallback) {
    this._radioInterface.sendWorkerMessage("sendMMI", {mmi: aMmi},
                                           (function(aResponse) {
      aResponse.serviceCode = aResponse.mmiServiceCode || "";
      // We expect to have an IMEI at this point if the request was supposed
      // to query for the IMEI, so getting a successful reply from the RIL
      // without containing an actual IMEI number is considered an error.
      if (aResponse.serviceCode === RIL.MMI_KS_SC_IMEI &&
          !aResponse.statusMessage) {
        aResponse.errorMsg = aResponse.errorMsg ||
                             RIL.GECKO_ERROR_GENERIC_FAILURE;
      }

      if (aResponse.errorMsg) {
        if (aResponse.additionalInformation) {
          aCallback.notifyError(aResponse.errorMsg, "",
                                aResponse.serviceCode,
                                aResponse.additionalInformation);
        } else {
          aCallback.notifyError(aResponse.errorMsg, "",
                                aResponse.serviceCode);
        }
        return false;
      }

      if (aResponse.isSetCallForward) {
        this.deliverListenerEvent("notifyCFStateChanged",
                                  [!aResponse.errorMsg, aResponse.action,
                                   aResponse.reason, aResponse.number,
                                   aResponse.timeSeconds, aResponse.serviceClass]);
      }

      // MMI query call forwarding options request returns a set of rules that
      // will be exposed in the form of an array of MozCallForwardingOptions
      // instances.
      if (aResponse.serviceCode === RIL.MMI_KS_SC_CALL_FORWARDING &&
          aResponse.additionalInformation) {
        this._rulesToCallForwardingOptions(aResponse.additionalInformation);
      }

      let mmiResult = new MMIResult(aResponse);
      aCallback.notifySendCancelMmiSuccess(mmiResult);
      return false;
    }).bind(this));
  },

  cancelMMI: function(aCallback) {
    this._radioInterface.sendWorkerMessage("cancelUSSD", null,
                                           (function(aResponse) {
      if (aResponse.errorMsg) {
        aCallback.notifyError(aResponse.errorMsg);
        return false;
      }

      aCallback.notifySuccess();
      return false;
    }).bind(this));
  },

  setCallForwarding: function(aOptions, aCallback) {
    if (!aOptions ||
        !this._isValidCallForwardingReason(aOptions.reason) ||
        !this._isValidCallForwardingAction(aOptions.action)){
      this._dispatchNotifyError(aCallback, RIL.GECKO_ERROR_INVALID_PARAMETER);
      return;
    }

    let options = {
      active: aOptions.active,
      action: aOptions.action,
      reason: aOptions.reason,
      number: aOptions.number,
      timeSeconds: aOptions.timeSeconds,
      serviceClass: RIL.ICC_SERVICE_CLASS_VOICE
    };

    this._radioInterface.sendWorkerMessage("setCallForward", options,
                                           (function(aResponse) {
      if (aResponse.errorMsg) {
        aCallback.notifyError(aResponse.errorMsg);
        return false;
      }

      this.deliverListenerEvent("notifyCFStateChanged",
                                [!aResponse.errorMsg, aResponse.action,
                                 aResponse.reason, aResponse.number,
                                 aResponse.timeSeconds, aResponse.serviceClass]);

      aCallback.notifySuccess();
      return false;
    }).bind(this));
  },

  getCallForwarding: function(aReason, aCallback) {
    if (!this._isValidCallForwardingReason(aReason)){
      this._dispatchNotifyError(aCallback, RIL.GECKO_ERROR_INVALID_PARAMETER);
      return;
    }

    this._radioInterface.sendWorkerMessage("queryCallForwardStatus",
                                           {reason: aReason},
                                           (function(aResponse) {
      if (aResponse.errorMsg) {
        aCallback.notifyError(aResponse.errorMsg);
        return false;
      }

      let infos = aResponse.rules;
      this._rulesToCallForwardingOptions(infos);
      aCallback.notifyGetCallForwardingSuccess(infos);
      return false;
    }).bind(this));
  },

  setCallBarring: function(aOptions, aCallback) {
    if (!this._isValidCallBarringOptions(aOptions, true)) {
      this._dispatchNotifyError(aCallback, RIL.GECKO_ERROR_INVALID_PARAMETER);
      return;
    }

    let options = {
      program: aOptions.program,
      enabled: aOptions.enabled,
      password: aOptions.password,
      serviceClass: aOptions.serviceClass
    };

    this._radioInterface.sendWorkerMessage("setCallBarring", options,
                                           (function(aResponse) {
      if (aResponse.errorMsg) {
        aCallback.notifyError(aResponse.errorMsg);
        return false;
      }

      aCallback.notifySuccess();
      return false;
    }).bind(this));
  },

  getCallBarring: function(aOptions, aCallback) {
    if (!this._isValidCallBarringOptions(aOptions)) {
      this._dispatchNotifyError(aCallback, RIL.GECKO_ERROR_INVALID_PARAMETER);
      return;
    }

    let options = {
      program: aOptions.program,
      password: aOptions.password,
      serviceClass: aOptions.serviceClass
    };

    this._radioInterface.sendWorkerMessage("queryCallBarringStatus", options,
                                           (function(aResponse) {
      if (aResponse.errorMsg) {
        aCallback.notifyError(aResponse.errorMsg);
        return false;
      }

      aCallback.notifyGetCallBarringSuccess(aResponse.program,
                                            aResponse.enabled,
                                            aResponse.serviceClass);
      return false;
    }).bind(this));
  },

  changeCallBarringPassword: function(aOptions, aCallback) {
    // Checking valid PIN for supplementary services. See TS.22.004 clause 5.2.
    if (aOptions.pin == null || !aOptions.pin.match(/^\d{4}$/) ||
        aOptions.newPin == null || !aOptions.newPin.match(/^\d{4}$/)) {
      this._dispatchNotifyError(aCallback, "InvalidPassword");
      return;
    }

    let options = {
      pin: aOptions.pin,
      newPin: aOptions.newPin
    };

    this._radioInterface.sendWorkerMessage("changeCallBarringPassword", options,
                                           (function(aResponse) {
      if (aResponse.errorMsg) {
        aCallback.notifyError(aResponse.errorMsg);
        return false;
      }

      aCallback.notifySuccess();
      return false;
    }).bind(this));
  },

  setCallWaiting: function(aEnabled, aCallback) {
    this._radioInterface.sendWorkerMessage("setCallWaiting",
                                           {enabled: aEnabled},
                                           (function(aResponse) {
      if (aResponse.errorMsg) {
        aCallback.notifyError(aResponse.errorMsg);
        return false;
      }

      aCallback.notifySuccess();
      return false;
    }).bind(this));
  },

  getCallWaiting: function(aCallback) {
    this._radioInterface.sendWorkerMessage("queryCallWaiting", null,
                                           (function(aResponse) {
      if (aResponse.errorMsg) {
        aCallback.notifyError(aResponse.errorMsg);
        return false;
      }

      aCallback.notifySuccessWithBoolean(aResponse.enabled);
      return false;
    }).bind(this));
  },

  setCallingLineIdRestriction: function(aMode, aCallback) {
    if (!this._isValidClirMode(aMode)) {
      this._dispatchNotifyError(aCallback, RIL.GECKO_ERROR_INVALID_PARAMETER);
      return;
    }

    if (this.radioState !== RIL.GECKO_RADIOSTATE_ENABLED) {
      this._dispatchNotifyError(aCallback, RIL.GECKO_ERROR_RADIO_NOT_AVAILABLE);
      return;
    }

    this._radioInterface.sendWorkerMessage("setCLIR", {clirMode: aMode},
                                           (function(aResponse) {
      if (aResponse.errorMsg) {
        aCallback.notifyError(aResponse.errorMsg);
        return false;
      }

      this.deliverListenerEvent("notifyClirModeChanged", [aResponse.mode]);
      aCallback.notifySuccess();
      return false;
    }).bind(this));
  },

  getCallingLineIdRestriction: function(aCallback) {
    if (this.radioState !== RIL.GECKO_RADIOSTATE_ENABLED) {
      this._dispatchNotifyError(aCallback, RIL.GECKO_ERROR_RADIO_NOT_AVAILABLE);
      return;
    }

    this._radioInterface.sendWorkerMessage("getCLIR", null,
                                           (function(aResponse) {
      if (aResponse.errorMsg) {
        aCallback.notifyError(aResponse.errorMsg);
        return false;
      }

      aCallback.notifyGetClirStatusSuccess(aResponse.n, aResponse.m);
      return false;
    }).bind(this));
  },

  exitEmergencyCbMode: function(aCallback) {
    this._radioInterface.sendWorkerMessage("exitEmergencyCbMode", null,
                                           (function(aResponse) {
      if (aResponse.errorMsg) {
        aCallback.notifyError(aResponse.errorMsg);
        return false;
      }

      aCallback.notifySuccess();
      return false;
    }).bind(this));
  },

  setRadioEnabled: function(aEnabled, aCallback) {
    this._radioInterface.sendWorkerMessage("setRadioEnabled",
                                           {enabled: aEnabled},
                                           (function(aResponse) {
      if (aResponse.errorMsg) {
        aCallback.notifyError(aResponse.errorMsg);
        return true;
      }

      aCallback.notifySuccess();
      return true;
    }).bind(this));
  },
};

function MobileConnectionService() {
  this._providers = [];

  let numClients = gRadioInterfaceLayer.numRadioInterfaces;
  for (let i = 0; i < numClients; i++) {
    let radioInterface = gRadioInterfaceLayer.getRadioInterface(i);
    let provider = new MobileConnectionProvider(i, radioInterface);
    this._providers.push(provider);
  }

  Services.prefs.addObserver(kPrefRilDebuggingEnabled, this, false);
  Services.obs.addObserver(this, NS_NETWORK_ACTIVE_CHANGED_TOPIC_ID, false);
  Services.obs.addObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);

  debug("init complete");
}
MobileConnectionService.prototype = {
  classID: GONK_MOBILECONNECTIONSERVICE_CID,
  classInfo: XPCOMUtils.generateCI({classID: GONK_MOBILECONNECTIONSERVICE_CID,
                                    contractID: GONK_MOBILECONNECTIONSERVICE_CONTRACTID,
                                    classDescription: "MobileConnectionService",
                                    interfaces: [Ci.nsIGonkMobileConnectionService,
                                                 Ci.nsIMobileConnectionService],
                                    flags: Ci.nsIClassInfo.SINGLETON}),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIGonkMobileConnectionService,
                                         Ci.nsIMobileConnectionService,
                                         Ci.nsIObserver]),

  // An array of MobileConnectionProvider instances.
  _providers: null,

  _shutdown: function() {
    Services.prefs.removeObserver(kPrefRilDebuggingEnabled, this);
    Services.obs.removeObserver(this, NS_NETWORK_ACTIVE_CHANGED_TOPIC_ID);
    Services.obs.removeObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
  },

  _updateDebugFlag: function() {
    try {
      DEBUG = RIL.DEBUG_RIL ||
              Services.prefs.getBoolPref(kPrefRilDebuggingEnabled);
    } catch (e) {}
  },

  /**
   * nsIMobileConnectionService interface.
   */
  get numItems() {
    return this._providers.length;
  },

  getItemByServiceId: function(aServiceId) {
    let provider = this._providers[aServiceId];
    if (!provider) {
      throw Cr.NS_ERROR_UNEXPECTED;
    }

    return provider;
  },

  /**
   * nsIGonkMobileConnectionService interface.
   */
  notifyVoiceInfoChanged: function(aClientId, aVoiceInfo) {
    if (DEBUG) {
      debug("notifyVoiceInfoChanged for " + aClientId + ": " +
            JSON.stringify(aVoiceInfo));
    }

    this.getItemByServiceId(aClientId).updateVoiceInfo(aVoiceInfo);
  },

  notifyDataInfoChanged: function(aClientId, aDataInfo) {
    if (DEBUG) {
      debug("notifyDataInfoChanged for " + aClientId + ": " +
            JSON.stringify(aDataInfo));
    }

    this.getItemByServiceId(aClientId).updateDataInfo(aDataInfo);
  },

  notifyUssdReceived: function(aClientId, aMessage, aSessionEnded) {
    if (DEBUG) {
      debug("notifyUssdReceived for " + aClientId + ": " +
            aMessage + " (sessionEnded : " + aSessionEnded + ")");
    }

    this.getItemByServiceId(aClientId)
        .deliverListenerEvent("notifyUssdReceived", [aMessage, aSessionEnded]);

    let info = {
      message: aMessage,
      sessionEnded: aSessionEnded,
      serviceId: aClientId
    };

    gSystemMessenger.broadcastMessage("ussd-received", info);
  },

  notifyDataError: function(aClientId, aMessage) {
    if (DEBUG) {
      debug("notifyDataError for " + aClientId + ": " + aMessage);
    }

    this.getItemByServiceId(aClientId)
        .deliverListenerEvent("notifyDataError", [aMessage]);
  },

  notifyEmergencyCallbackModeChanged: function(aClientId, aActive, aTimeoutMs) {
    if (DEBUG) {
      debug("notifyEmergencyCbModeChanged for " + aClientId + ": " +
            JSON.stringify({active: aActive, timeoutMs: aTimeoutMs}));
    }

    this.getItemByServiceId(aClientId)
        .deliverListenerEvent("notifyEmergencyCbModeChanged",
                              [aActive, aTimeoutMs]);
  },

  notifyOtaStatusChanged: function(aClientId, aStatus) {
    if (DEBUG) {
      debug("notifyOtaStatusChanged for " + aClientId + ": " + aStatus);
    }

    this.getItemByServiceId(aClientId)
        .deliverListenerEvent("notifyOtaStatusChanged", [aStatus]);
  },

  notifyIccChanged: function(aClientId, aIccId) {
    if (DEBUG) {
      debug("notifyIccChanged for " + aClientId + ": " + aIccId);
    }

    this.getItemByServiceId(aClientId).updateIccId(aIccId);
  },

  notifyRadioStateChanged: function(aClientId, aRadioState) {
    if (DEBUG) {
      debug("notifyRadioStateChanged for " + aClientId + ": " + aRadioState);
    }

    this.getItemByServiceId(aClientId).updateRadioState(aRadioState);
  },

  notifyNetworkInfoChanged: function(aClientId, aNetworkInfo) {
    if (DEBUG) {
      debug("notifyNetworkInfoChanged for " + aClientId + ": " +
            JSON.stringify(aNetworkInfo));
    }

    let provider = this.getItemByServiceId(aClientId);

    let isVoiceUpdated = false;
    let isDataUpdated = false;
    let operatorMessage = aNetworkInfo[RIL.NETWORK_INFO_OPERATOR];
    let voiceMessage = aNetworkInfo[RIL.NETWORK_INFO_VOICE_REGISTRATION_STATE];
    let dataMessage = aNetworkInfo[RIL.NETWORK_INFO_DATA_REGISTRATION_STATE];
    let signalMessage = aNetworkInfo[RIL.NETWORK_INFO_SIGNAL];
    let selectionMessage = aNetworkInfo[RIL.NETWORK_INFO_NETWORK_SELECTION_MODE];

    // Batch the *InfoChanged messages together
    if (operatorMessage) {
      provider.updateOperatorInfo(operatorMessage, true);
    }

    if (voiceMessage) {
      provider.updateVoiceInfo(voiceMessage, true);
    }

    if (dataMessage) {
      provider.updateDataInfo(dataMessage, true);
    }

    if (signalMessage) {
      provider.updateSignalInfo(signalMessage, true);
    }

    if (selectionMessage) {
      this.notifyNetworkSelectModeChanged(aClientId, selectionMessage.mode);
    }

    if (voiceMessage || operatorMessage || signalMessage) {
      provider.deliverListenerEvent("notifyVoiceChanged");
    }

    if (dataMessage || operatorMessage || signalMessage) {
      provider.deliverListenerEvent("notifyDataChanged");
    }
  },

  notifySignalStrengthChanged: function(aClientId, aSignal) {
    if (DEBUG) {
      debug("notifySignalStrengthChanged for " + aClientId + ": " +
            JSON.stringify(aSignal));
    }

    this.getItemByServiceId(aClientId).updateSignalInfo(aSignal);
  },

  notifyOperatorChanged: function(aClientId, aOperator) {
    if (DEBUG) {
      debug("notifyOperatorChanged for " + aClientId + ": " +
            JSON.stringify(aOperator));
    }

    this.getItemByServiceId(aClientId).updateOperatorInfo(aOperator);
  },

  notifyNetworkSelectModeChanged: function(aClientId, aMode) {
    if (DEBUG) {
      debug("notifyNetworkSelectModeChanged for " + aClientId + ": " + aMode);
    }

    let provider = this.getItemByServiceId(aClientId);
    if (provider.networkSelectionMode === aMode) {
      return;
    }

    provider.networkSelectionMode = aMode;
    provider.deliverListenerEvent("notifyNetworkSelectionModeChanged");
  },

  notifySpnAvailable: function(aClientId) {
    if (DEBUG) {
      debug("notifySpnAvailable for " + aClientId);
    }

    let provider = this.getItemByServiceId(aClientId);

    // Update voice roaming state
    provider.updateVoiceInfo({});

    // Update data roaming state
    provider.updateDataInfo({});
  },

  notifyLastHomeNetworkChanged: function(aClientId, aNetwork) {
    if (DEBUG) {
      debug("notifyLastHomeNetworkChanged for " + aClientId + ": " + aNetwork);
    }

    let provider = this.getItemByServiceId(aClientId);
    if (provider.lastKnownHomeNetwork === aNetwork) {
      return;
    }

    provider.lastKnownHomeNetwork = aNetwork;
    provider.deliverListenerEvent("notifyLastKnownHomeNetworkChanged");
  },

  /**
   * nsIObserver interface.
   */
  observe: function(aSubject, aTopic, aData) {
    switch (aTopic) {
      case NS_NETWORK_ACTIVE_CHANGED_TOPIC_ID:
        for (let i = 0; i < this.numItems; i++) {
          let provider = this._providers[i];
          // Update connected flag only.
          provider.updateDataInfo({});
        }
        break;
      case NS_PREFBRANCH_PREFCHANGE_TOPIC_ID:
        if (aData === kPrefRilDebuggingEnabled) {
          this._updateDebugFlag();
        }
        break;
      case NS_XPCOM_SHUTDOWN_OBSERVER_ID:
        this._shutdown();
        break;
    }
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([MobileConnectionService]);
