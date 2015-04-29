/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

var RIL = {};
Cu.import("resource://gre/modules/ril_consts.js", RIL);

const GONK_ICCSERVICE_CONTRACTID = "@mozilla.org/icc/gonkiccservice;1";
const GONK_ICCSERVICE_CID = Components.ID("{df854256-9554-11e4-a16c-c39e8d106c26}");

const NS_XPCOM_SHUTDOWN_OBSERVER_ID      = "xpcom-shutdown";
const NS_PREFBRANCH_PREFCHANGE_TOPIC_ID  = "nsPref:changed";

const kPrefRilDebuggingEnabled = "ril.debugging.enabled";
const kPrefRilNumRadioInterfaces = "ril.numRadioInterfaces";

XPCOMUtils.defineLazyServiceGetter(this, "gRadioInterfaceLayer",
                                   "@mozilla.org/ril;1",
                                   "nsIRadioInterfaceLayer");

XPCOMUtils.defineLazyServiceGetter(this, "gMobileConnectionService",
                                   "@mozilla.org/mobileconnection/mobileconnectionservice;1",
                                   "nsIGonkMobileConnectionService");

XPCOMUtils.defineLazyServiceGetter(this, "gIccMessenger",
                                   "@mozilla.org/ril/system-messenger-helper;1",
                                   "nsIIccMessenger");

XPCOMUtils.defineLazyServiceGetter(this, "gStkCmdFactory",
                                   "@mozilla.org/icc/stkcmdfactory;1",
                                   "nsIStkCmdFactory");

let DEBUG = RIL.DEBUG_RIL;
function debug(s) {
  dump("IccService: " + s);
}

function IccInfo() {}
IccInfo.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIIccInfo]),

  // nsIIccInfo

  iccType: null,
  iccid: null,
  mcc: null,
  mnc: null,
  spn: null,
  isDisplayNetworkNameRequired: false,
  isDisplaySpnRequired: false
};

function GsmIccInfo() {}
GsmIccInfo.prototype = {
  __proto__: IccInfo.prototype,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIGsmIccInfo,
                                         Ci.nsIIccInfo]),

  // nsIGsmIccInfo

  msisdn: null
};

function CdmaIccInfo() {}
CdmaIccInfo.prototype = {
  __proto__: IccInfo.prototype,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsICdmaIccInfo,
                                         Ci.nsIIccInfo]),

  // nsICdmaIccInfo

  mdn: null,
  prlVersion: 0
};

function IccService() {
  this._iccs = [];

  let numClients = gRadioInterfaceLayer.numRadioInterfaces;
  for (let i = 0; i < numClients; i++) {
    this._iccs.push(new Icc(i));
  }

  this._updateDebugFlag();

  Services.prefs.addObserver(kPrefRilDebuggingEnabled, this, false);
  Services.obs.addObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
}
IccService.prototype = {
  classID: GONK_ICCSERVICE_CID,

  classInfo: XPCOMUtils.generateCI({classID: GONK_ICCSERVICE_CID,
                                    contractID: GONK_ICCSERVICE_CONTRACTID,
                                    classDescription: "IccService",
                                    interfaces: [Ci.nsIIccService,
                                                 Ci.nsIGonkIccService],
                                    flags: Ci.nsIClassInfo.SINGLETON}),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIIccService,
                                         Ci.nsIGonkIccService,
                                         Ci.nsIObserver]),

  // An array of Icc instances.
  _iccs: null,

  _updateDebugFlag: function() {
    try {
      DEBUG = DEBUG ||
              Services.prefs.getBoolPref(kPrefRilDebuggingEnabled);
    } catch (e) {}
  },

  /**
   * nsIIccService interface.
   */
  getIccByServiceId: function(aServiceId) {
    let icc = this._iccs[aServiceId];
    if (!icc) {
      throw Cr.NS_ERROR_UNEXPECTED;
    }

    return icc;
  },

  /**
   * nsIGonkIccService interface.
   */
  notifyStkCommand: function(aServiceId, aStkcommand) {
    if (DEBUG) {
      debug("notifyStkCommand for service Id: " + aServiceId);
    }

    let icc = this.getIccByServiceId(aServiceId);

    gIccMessenger.notifyStkProactiveCommand(icc.iccInfo.iccid, aStkcommand);

    icc._deliverListenerEvent("notifyStkCommand", [aStkcommand]);
  },

  notifyStkSessionEnd: function(aServiceId) {
    if (DEBUG) {
      debug("notifyStkSessionEnd for service Id: " + aServiceId);
    }

    this.getIccByServiceId(aServiceId)
      ._deliverListenerEvent("notifyStkSessionEnd");
  },

  notifyCardStateChanged: function(aServiceId, aCardState) {
    if (DEBUG) {
      debug("notifyCardStateChanged for service Id: " + aServiceId +
            ", CardState: " + aCardState);
    }

    this.getIccByServiceId(aServiceId)._updateCardState(aCardState);
  },

  notifyIccInfoChanged: function(aServiceId, aIccInfo) {
    if (DEBUG) {
      debug("notifyIccInfoChanged for service Id: " + aServiceId +
            ", IccInfo: " + JSON.stringify(aIccInfo));
    }

    this.getIccByServiceId(aServiceId)._updateIccInfo(aIccInfo);
  },

  notifyImsiChanged: function(aServiceId, aImsi) {
    if (DEBUG) {
      debug("notifyImsiChanged for service Id: " + aServiceId +
            ", Imsi: " + aImsi);
    }

    let icc = this.getIccByServiceId(aServiceId);
    icc.imsi = aImsi || null;
  },

  /**
   * nsIObserver interface.
   */
  observe: function(aSubject, aTopic, aData) {
    switch (aTopic) {
      case NS_PREFBRANCH_PREFCHANGE_TOPIC_ID:
        if (aData === kPrefRilDebuggingEnabled) {
          this._updateDebugFlag();
        }
        break;
      case NS_XPCOM_SHUTDOWN_OBSERVER_ID:
        Services.prefs.removeObserver(kPrefRilDebuggingEnabled, this);
        Services.obs.removeObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
        break;
    }
  }
};

function Icc(aClientId) {
  this._clientId = aClientId;
  this._radioInterface = gRadioInterfaceLayer.getRadioInterface(aClientId);
  this._listeners = [];
}
Icc.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIIcc]),

  _clientId: 0,
  _radioInterface: null,
  _listeners: null,

  _updateCardState: function(aCardState) {
    if (this.cardState != aCardState) {
      this.cardState = aCardState;
    }

    this._deliverListenerEvent("notifyCardStateChanged");
  },

  // An utility function to copy objects.
  _updateInfo: function(aSrcInfo, aDestInfo) {
    for (let key in aSrcInfo) {
      aDestInfo[key] = aSrcInfo[key];
    }
  },

  /**
   * A utility function to compare objects. The srcInfo may contain
   * "rilMessageType", should ignore it.
   */
  _isInfoChanged: function(srcInfo, destInfo) {
    if (!destInfo) {
      return true;
    }

    for (let key in srcInfo) {
      if (key === "rilMessageType") {
        continue;
      }
      if (srcInfo[key] !== destInfo[key]) {
        return true;
      }
    }

    return false;
  },

  /**
   * We need to consider below cases when update iccInfo:
   * 1. Should clear iccInfo to null if there is no card detected.
   * 2. Need to create corresponding object based on iccType.
   */
  _updateIccInfo: function(aIccInfo) {
    let oldSpn = this.iccInfo ? this.iccInfo.spn : null;

    // Card is not detected, clear iccInfo to null.
    if (!aIccInfo || !aIccInfo.iccid) {
      if (this.iccInfo) {
        if (DEBUG) {
          debug("Card is not detected, clear iccInfo to null.");
        }
        this.imsi = null;
        this.iccInfo = null;
        this._deliverListenerEvent("notifyIccInfoChanged");
      }
      return;
    }

    if (!this._isInfoChanged(aIccInfo, this.iccInfo)) {
      return;
    }

    // If iccInfo is null, new corresponding object based on iccType.
    if (!this.iccInfo ||
        this.iccInfo.iccType != aIccInfo.iccType) {
      if (aIccInfo.iccType === "ruim" || aIccInfo.iccType === "csim") {
        this.iccInfo = new CdmaIccInfo();
      } else if (aIccInfo.iccType === "sim" || aIccInfo.iccType === "usim") {
        this.iccInfo = new GsmIccInfo();
      } else {
        this.iccInfo = new IccInfo();
      }
    }

    this._updateInfo(aIccInfo, this.iccInfo);

    this._deliverListenerEvent("notifyIccInfoChanged");

    // Update lastKnownSimMcc.
    if (aIccInfo.mcc) {
      try {
        Services.prefs.setCharPref("ril.lastKnownSimMcc",
                                   aIccInfo.mcc.toString());
      } catch (e) {}
    }

    // Update lastKnownHomeNetwork.
    if (aIccInfo.mcc && aIccInfo.mnc) {
      let lastKnownHomeNetwork = aIccInfo.mcc + "-" + aIccInfo.mnc;
      // Append spn information if available.
      if (aIccInfo.spn) {
        lastKnownHomeNetwork += "-" + aIccInfo.spn;
      }

      gMobileConnectionService.notifyLastHomeNetworkChanged(this._clientId,
                                                            lastKnownHomeNetwork);
    }

    // If spn becomes available, we should check roaming again.
    if (!oldSpn && aIccInfo.spn) {
      gMobileConnectionService.notifySpnAvailable(this._clientId);
    }
  },

  _deliverListenerEvent: function(aName, aArgs) {
    let listeners = this._listeners.slice();
    for (let listener of listeners) {
      if (this._listeners.indexOf(listener) === -1) {
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
          debug("listener for " + aName + " threw an exception: " + e);
        }
      }
    }
  },

  _modifyCardLock: function(aOperation, aOptions, aCallback) {
    this._radioInterface.sendWorkerMessage(aOperation,
                                           aOptions,
                                           (aResponse) => {
      if (aResponse.errorMsg) {
        let retryCount =
          (aResponse.retryCount !== undefined) ? aResponse.retryCount : -1;
        aCallback.notifyCardLockError(aResponse.errorMsg, retryCount);
        return;
      }

      aCallback.notifySuccess();
    });
  },

  /**
   * Helper to match the MVNO pattern with IMSI.
   *
   * Note: Characters 'x' and 'X' in MVNO are skipped and not compared.
   *       E.g., if the aMvnoData passed is '310260x10xxxxxx', then the
   *       function returns true only if imsi has the same first 6 digits, 8th
   *       and 9th digit.
   *
   * @param aMvnoData
   *        MVNO pattern.
   * @param aImsi
   *        IMSI of this ICC.
   *
   * @return true if matched.
   */
  _isImsiMatches: function(aMvnoData, aImsi) {
    // This should not be an error, but a mismatch.
    if (aMvnoData.length > aImsi.length) {
      return false;
    }

    for (let i = 0; i < aMvnoData.length; i++) {
      let c = aMvnoData[i];
      if ((c !== 'x') && (c !== 'X') && (c !== aImsi[i])) {
        return false;
      }
    }
    return true;
  },

  /**
   * nsIIcc interface.
   */
  iccInfo: null,
  cardState: Ci.nsIIcc.CARD_STATE_UNKNOWN,
  imsi: null,

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

  getCardLockEnabled: function(aLockType, aCallback) {
    this._radioInterface.sendWorkerMessage("iccGetCardLockEnabled",
                                           { lockType: aLockType },
                                           (aResponse) => {
      if (aResponse.errorMsg) {
        aCallback.notifyError(aResponse.errorMsg);
        return;
      }

      aCallback.notifySuccessWithBoolean(aResponse.enabled);
    });
  },

  unlockCardLock: function(aLockType, aPassword, aNewPin, aCallback) {
    this._modifyCardLock("iccUnlockCardLock",
                         { lockType: aLockType,
                           password: aPassword,
                           newPin: aNewPin },
                         aCallback);
  },

  setCardLockEnabled: function(aLockType, aPassword, aEnabled, aCallback) {
    this._modifyCardLock("iccSetCardLockEnabled",
                         { lockType: aLockType,
                           password: aPassword,
                           enabled: aEnabled },
                         aCallback);
  },

  changeCardLockPassword: function(aLockType, aPassword, aNewPassword, aCallback) {
    this._modifyCardLock("iccChangeCardLockPassword",
                         { lockType: aLockType,
                           password: aPassword,
                           newPassword: aNewPassword, },
                         aCallback);
  },

  getCardLockRetryCount: function(aLockType, aCallback) {
    this._radioInterface.sendWorkerMessage("iccGetCardLockRetryCount",
                                           { lockType: aLockType },
                                           (aResponse) => {
      if (aResponse.errorMsg) {
        aCallback.notifyError(aResponse.errorMsg);
        return;
      }

      aCallback.notifyGetCardLockRetryCount(aResponse.retryCount);
    });
  },

  matchMvno: function(aMvnoType, aMvnoData, aCallback) {
    if (!aMvnoData) {
      aCallback.notifyError(RIL.GECKO_ERROR_INVALID_PARAMETER);
      return;
    }

    switch (aMvnoType) {
      case Ci.nsIIcc.CARD_MVNO_TYPE_IMSI:
        let imsi = this.imsi;
        if (!imsi) {
          aCallback.notifyError(RIL.GECKO_ERROR_GENERIC_FAILURE);
          break;
        }
        aCallback.notifySuccessWithBoolean(
          this._isImsiMatches(aMvnoData, imsi));
        break;
      case Ci.nsIIcc.CARD_MVNO_TYPE_SPN:
        let spn = this.iccInfo && this.iccInfo.spn;
        if (!spn) {
          aCallback.notifyError(RIL.GECKO_ERROR_GENERIC_FAILURE);
          break;
        }
        aCallback.notifySuccessWithBoolean(spn == aMvnoData);
        break;
      case Ci.nsIIcc.CARD_MVNO_TYPE_GID:
        this._radioInterface.sendWorkerMessage("getGID1",
                                               null,
                                               (aResponse) => {
          let gid = aResponse.gid1;
          let mvnoDataLength = aMvnoData.length;

          if (!gid) {
            aCallback.notifyError(RIL.GECKO_ERROR_GENERIC_FAILURE);
          } else if (mvnoDataLength > gid.length) {
            aCallback.notifySuccessWithBoolean(false);
          } else {
            let result =
              gid.substring(0, mvnoDataLength).toLowerCase() ==
              aMvnoData.toLowerCase();
            aCallback.notifySuccessWithBoolean(result);
          }
        });
        break;
      default:
        aCallback.notifyError(RIL.GECKO_ERROR_MODE_NOT_SUPPORTED);
        break;
    }
  },

  getServiceStateEnabled: function(aService, aCallback) {
    this._radioInterface.sendWorkerMessage("getIccServiceState",
                                           { service: aService },
                                           (aResponse) => {
      if (aResponse.errorMsg) {
        aCallback.notifyError(aResponse.errorMsg);
        return;
      }

      aCallback.notifySuccessWithBoolean(aResponse.result);
    });
  },

  iccOpenChannel: function(aAid, aCallback) {
    this._radioInterface.sendWorkerMessage("iccOpenChannel",
                                           { aid: aAid },
                                           (aResponse) => {
      if (aResponse.errorMsg) {
        aCallback.notifyError(aResponse.errorMsg);
        return;
      }

      aCallback.notifyOpenChannelSuccess(aResponse.channel);
    });
  },

  iccExchangeAPDU: function(aChannel, aCla, aIns, aP1, aP2, aP3, aData, aCallback) {
    if (!aData) {
      if (DEBUG) debug('data is not set , aP3 : ' + aP3);
    }

    let apdu = {
      cla: aCla,
      command: aIns,
      p1: aP1,
      p2: aP2,
      p3: aP3,
      data: aData
    };

    this._radioInterface.sendWorkerMessage("iccExchangeAPDU",
                                           { channel: aChannel, apdu: apdu },
                                           (aResponse) => {
      if (aResponse.errorMsg) {
        aCallback.notifyError(aResponse.errorMsg);
        return;
      }

      aCallback.notifyExchangeAPDUResponse(aResponse.sw1,
                                           aResponse.sw2,
                                           aResponse.simResponse);
    });
  },

  iccCloseChannel: function(aChannel, aCallback) {
    this._radioInterface.sendWorkerMessage("iccCloseChannel",
                                           { channel: aChannel },
                                           (aResponse) => {
      if (aResponse.errorMsg) {
        aCallback.notifyError(aResponse.errorMsg);
        return;
      }

      aCallback.notifyCloseChannelSuccess();
    });
  },

  sendStkResponse: function(aCommand, aResponse) {
    let response = gStkCmdFactory.createResponseMessage(aResponse);
    response.command = gStkCmdFactory.createCommandMessage(aCommand);
    this._radioInterface.sendWorkerMessage("sendStkTerminalResponse", response);
  },

  sendStkMenuSelection: function(aItemIdentifier, aHelpRequested) {
    this._radioInterface
      .sendWorkerMessage("sendStkMenuSelection", {
        itemIdentifier: aItemIdentifier,
        helpRequested: aHelpRequested
      });
  },

  sendStkTimerExpiration: function(aTimerId, aTimerValue) {
    this._radioInterface
      .sendWorkerMessage("sendStkTimerExpiration",{
        timer: {
          timerId: aTimerId,
          timerValue: aTimerValue
        }
      });
  },

  sendStkEventDownload: function(aEvent) {
    this._radioInterface
      .sendWorkerMessage("sendStkEventDownload",
                         { event: gStkCmdFactory.createEventMessage(aEvent) });
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([IccService]);
