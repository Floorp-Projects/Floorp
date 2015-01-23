/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Promise.jsm");

XPCOMUtils.defineLazyGetter(this, "RIL", function () {
  let obj = {};
  Cu.import("resource://gre/modules/ril_consts.js", obj);
  return obj;
});

const GONK_TELEPHONYSERVICE_CONTRACTID =
  "@mozilla.org/telephony/gonktelephonyservice;1";

const GONK_TELEPHONYSERVICE_CID =
  Components.ID("{67d26434-d063-4d28-9f48-5b3189788155}");
const MOBILECALLFORWARDINGOPTIONS_CID =
  Components.ID("{79b5988b-9436-48d8-a652-88fa033f146c}");
const TELEPHONYCALLINFO_CID =
  Components.ID("{d9e8b358-a02c-4cf3-9fc7-816c2e8d46e4}");

const NS_XPCOM_SHUTDOWN_OBSERVER_ID = "xpcom-shutdown";

const NS_PREFBRANCH_PREFCHANGE_TOPIC_ID = "nsPref:changed";

const kPrefRilNumRadioInterfaces = "ril.numRadioInterfaces";
const kPrefRilDebuggingEnabled = "ril.debugging.enabled";
const kPrefDefaultServiceId = "dom.telephony.defaultServiceId";

const nsITelephonyAudioService = Ci.nsITelephonyAudioService;
const nsITelephonyService = Ci.nsITelephonyService;

const CALL_WAKELOCK_TIMEOUT = 5000;

// Index of the CDMA second call which isn't held in RIL but only in TelephoyService.
const CDMA_SECOND_CALL_INDEX = 2;

const DIAL_ERROR_INVALID_STATE_ERROR = "InvalidStateError";
const DIAL_ERROR_OTHER_CONNECTION_IN_USE = "OtherConnectionInUse";
const DIAL_ERROR_BAD_NUMBER = RIL.GECKO_CALL_ERROR_BAD_NUMBER;


const TONES_GAP_DURATION = 70;

let DEBUG;
function debug(s) {
  dump("TelephonyService: " + s + "\n");
}

XPCOMUtils.defineLazyServiceGetter(this, "gRadioInterfaceLayer",
                                   "@mozilla.org/ril;1",
                                   "nsIRadioInterfaceLayer");

XPCOMUtils.defineLazyServiceGetter(this, "gPowerManagerService",
                                   "@mozilla.org/power/powermanagerservice;1",
                                   "nsIPowerManagerService");

XPCOMUtils.defineLazyServiceGetter(this, "gTelephonyMessenger",
                                   "@mozilla.org/ril/system-messenger-helper;1",
                                   "nsITelephonyMessenger");

XPCOMUtils.defineLazyServiceGetter(this, "gAudioService",
                                   "@mozilla.org/telephony/audio-service;1",
                                   "nsITelephonyAudioService");

XPCOMUtils.defineLazyServiceGetter(this, "gGonkMobileConnectionService",
                                   "@mozilla.org/mobileconnection/mobileconnectionservice;1",
                                   "nsIGonkMobileConnectionService");

XPCOMUtils.defineLazyGetter(this, "gPhoneNumberUtils", function() {
  let ns = {};
  Cu.import("resource://gre/modules/PhoneNumberUtils.jsm", ns);
  return ns.PhoneNumberUtils;
});

XPCOMUtils.defineLazyGetter(this, "gDialNumberUtils", function() {
  let ns = {};
  Cu.import("resource://gre/modules/DialNumberUtils.jsm", ns);
  return ns.DialNumberUtils;
});

function MobileCallForwardingOptions(aOptions) {
  for (let key in aOptions) {
    this[key] = aOptions[key];
  }
}
MobileCallForwardingOptions.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIMobileCallForwardingOptions]),
  classID: MOBILECALLFORWARDINGOPTIONS_CID,
  classInfo: XPCOMUtils.generateCI({
    classID:          MOBILECALLFORWARDINGOPTIONS_CID,
    classDescription: "MobileCallForwardingOptions",
    interfaces:       [Ci.nsIMobileCallForwardingOptions]
  }),

  // nsIMobileForwardingOptions

  active: false,
  action: Ci.nsIMobileConnection.CALL_FORWARD_ACTION_UNKNOWN,
  reason: Ci.nsIMobileConnection.CALL_FORWARD_REASON_UNKNOWN,
  number: null,
  timeSeconds: -1,
  serviceClass: Ci.nsIMobileConnection.ICC_SERVICE_CLASS_NONE
};

function TelephonyCallInfo(aCall) {
  this.clientId = aCall.clientId;
  this.callIndex = aCall.callIndex;
  this.callState = aCall.state;
  this.number = aCall.number;
  this.numberPresentation = aCall.numberPresentation;
  this.name = aCall.name;
  this.namePresentation = aCall.namePresentation;
  this.isOutgoing = aCall.isOutgoing;
  this.isEmergency = aCall.isEmergency;
  this.isConference = aCall.isConference;
  this.isSwitchable = aCall.isSwitchable;
  this.isMergeable = aCall.isMergeable;
}
TelephonyCallInfo.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsITelephonyCallInfo]),
  classID: TELEPHONYCALLINFO_CID,
  classInfo: XPCOMUtils.generateCI({
    classID:          TELEPHONYCALLINFO_CID,
    classDescription: "TelephonyCallInfo",
    interfaces:       [Ci.nsITelephonyCallInfo]
  }),

  // nsITelephonyCallInfo

  clientId: 0,
  callIndex: 0,
  callState: nsITelephonyService.CALL_STATE_UNKNOWN,
  number: "",
  numberPresentation: nsITelephonyService.CALL_PRESENTATION_ALLOWED,
  name: "",
  namePresentation: nsITelephonyService.CALL_PRESENTATION_ALLOWED,
  isOutgoing: true,
  isEmergency: false,
  isConference: false,
  isSwitchable: true,
  isMergeable: true
};

function TelephonyService() {
  this._numClients = gRadioInterfaceLayer.numRadioInterfaces;
  this._listeners = [];

  this._isDialing = false;
  this._cachedDialRequest = null;
  this._currentCalls = {};
  this._audioStates = {};

  this._cdmaCallWaitingNumber = null;

  this._updateDebugFlag();
  this.defaultServiceId = this._getDefaultServiceId();

  Services.prefs.addObserver(kPrefRilDebuggingEnabled, this, false);
  Services.prefs.addObserver(kPrefDefaultServiceId, this, false);

  Services.obs.addObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);

  for (let i = 0; i < this._numClients; ++i) {
    this._enumerateCallsForClient(i);
    this._audioStates[i] = RIL.AUDIO_STATE_NO_CALL;
  }
}
TelephonyService.prototype = {
  classID: GONK_TELEPHONYSERVICE_CID,
  classInfo: XPCOMUtils.generateCI({classID: GONK_TELEPHONYSERVICE_CID,
                                    contractID: GONK_TELEPHONYSERVICE_CONTRACTID,
                                    classDescription: "TelephonyService",
                                    interfaces: [Ci.nsITelephonyService,
                                                 Ci.nsIGonkTelephonyService],
                                    flags: Ci.nsIClassInfo.SINGLETON}),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsITelephonyService,
                                         Ci.nsIGonkTelephonyService,
                                         Ci.nsIObserver]),

  // The following attributes/functions are used for acquiring/releasing the
  // CPU wake lock when the RIL handles the incoming call. Note that we need
  // a timer to bound the lock's life cycle to avoid exhausting the battery.
  _callRingWakeLock: null,
  _callRingWakeLockTimer: null,

  _acquireCallRingWakeLock: function() {
    if (!this._callRingWakeLock) {
      if (DEBUG) debug("Acquiring a CPU wake lock for handling incoming call.");
      this._callRingWakeLock = gPowerManagerService.newWakeLock("cpu");
    }
    if (!this._callRingWakeLockTimer) {
      if (DEBUG) debug("Creating a timer for releasing the CPU wake lock.");
      this._callRingWakeLockTimer =
        Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    }
    if (DEBUG) debug("Setting the timer for releasing the CPU wake lock.");
    this._callRingWakeLockTimer
        .initWithCallback(this._releaseCallRingWakeLock.bind(this),
                          CALL_WAKELOCK_TIMEOUT, Ci.nsITimer.TYPE_ONE_SHOT);
  },

  _releaseCallRingWakeLock: function() {
    if (DEBUG) debug("Releasing the CPU wake lock for handling incoming call.");
    if (this._callRingWakeLockTimer) {
      this._callRingWakeLockTimer.cancel();
    }
    if (this._callRingWakeLock) {
      this._callRingWakeLock.unlock();
      this._callRingWakeLock = null;
    }
  },

  _getClient: function(aClientId) {
    return gRadioInterfaceLayer.getRadioInterface(aClientId);
  },

  _sendToRilWorker: function(aClientId, aType, aMessage, aCallback) {
    this._getClient(aClientId).sendWorkerMessage(aType, aMessage, aCallback);
  },

  // An array of nsITelephonyListener instances.
  _listeners: null,
  _notifyAllListeners: function(aMethodName, aArgs) {
    let listeners = this._listeners.slice();
    for (let listener of listeners) {
      if (this._listeners.indexOf(listener) == -1) {
        // Listener has been unregistered in previous run.
        continue;
      }

      let handler = listener[aMethodName];
      try {
        handler.apply(listener, aArgs);
      } catch (e) {
        debug("listener for " + aMethodName + " threw an exception: " + e);
      }
    }
  },

  _updateAudioState: function(aAudioState) {
    switch (aAudioState) {
      case RIL.AUDIO_STATE_NO_CALL:
        gAudioService.setPhoneState(nsITelephonyAudioService.PHONE_STATE_NORMAL);
        break;
      case RIL.AUDIO_STATE_INCOMING:
        gAudioService.setPhoneState(nsITelephonyAudioService.PHONE_STATE_RINGTONE);
        break;
      case RIL.AUDIO_STATE_IN_CALL:
        gAudioService.setPhoneState(nsITelephonyAudioService.PHONE_STATE_IN_CALL);
        break;
    }
  },

  _convertRILCallState: function(aState) {
    switch (aState) {
      case RIL.CALL_STATE_UNKNOWN:
        return nsITelephonyService.CALL_STATE_UNKNOWN;
      case RIL.CALL_STATE_ACTIVE:
        return nsITelephonyService.CALL_STATE_CONNECTED;
      case RIL.CALL_STATE_HOLDING:
        return nsITelephonyService.CALL_STATE_HELD;
      case RIL.CALL_STATE_DIALING:
        return nsITelephonyService.CALL_STATE_DIALING;
      case RIL.CALL_STATE_ALERTING:
        return nsITelephonyService.CALL_STATE_ALERTING;
      case RIL.CALL_STATE_INCOMING:
      case RIL.CALL_STATE_WAITING:
        return nsITelephonyService.CALL_STATE_INCOMING;
      default:
        throw new Error("Unknown rilCallState: " + aState);
    }
  },

  _convertRILSuppSvcNotification: function(aNotification) {
    switch (aNotification) {
      case RIL.GECKO_SUPP_SVC_NOTIFICATION_REMOTE_HELD:
        return nsITelephonyService.NOTIFICATION_REMOTE_HELD;
      case RIL.GECKO_SUPP_SVC_NOTIFICATION_REMOTE_RESUMED:
        return nsITelephonyService.NOTIFICATION_REMOTE_RESUMED;
      default:
        if (DEBUG) {
          debug("Unknown rilSuppSvcNotification: " + aNotification);
        }
        return;
    }
  },

  _rulesToCallForwardingOptions: function(aRules) {
    return aRules.map(rule => new MobileCallForwardingOptions(rule));
  },

  _updateDebugFlag: function() {
    try {
      DEBUG = RIL.DEBUG_RIL ||
              Services.prefs.getBoolPref(kPrefRilDebuggingEnabled);
    } catch (e) {}
  },

  _getDefaultServiceId: function() {
    let id = Services.prefs.getIntPref(kPrefDefaultServiceId);
    let numRil = Services.prefs.getIntPref(kPrefRilNumRadioInterfaces);

    if (id >= numRil || id < 0) {
      id = 0;
    }

    return id;
  },

  _currentCalls: null,
  _enumerateCallsForClient: function(aClientId) {
    if (DEBUG) debug("Enumeration of calls for client " + aClientId);

    this._sendToRilWorker(aClientId, "enumerateCalls", null, response => {
      if (!this._currentCalls[aClientId]) {
        this._currentCalls[aClientId] = {};
      }
      for (let call of response.calls) {
        call.clientId = aClientId;
        call.state = this._convertRILCallState(call.state);
        call.isSwitchable = true;
        call.isMergeable = true;

        this._currentCalls[aClientId][call.callIndex] = call;
      }
    });
  },

  /**
   * Checks whether to temporarily suppress caller id for the call.
   *
   * @param aMmi
   *        MMI full object.
   */
  _isTemporaryCLIR: function(aMmi) {
    return (aMmi && aMmi.serviceCode === RIL.MMI_SC_CLIR) && aMmi.dialNumber;
  },

  /**
   * Map MMI procedure to CLIR MODE.
   *
   * @param aProcedure
   *        MMI procedure
   */
  _getTemporaryCLIRMode: function(aProcedure) {
    // In temporary mode, MMI_PROCEDURE_ACTIVATION means allowing CLI
    // presentation, i.e. CLIR_SUPPRESSION. See TS 22.030, Annex B.
    switch (aProcedure) {
      case RIL.MMI_PROCEDURE_ACTIVATION:
        return RIL.CLIR_SUPPRESSION;
      case RIL.MMI_PROCEDURE_DEACTIVATION:
        return RIL.CLIR_INVOCATION;
      default:
        return RIL.CLIR_DEFAULT;
    }
  },

  /**
   * nsITelephonyService interface.
   */

  defaultServiceId: 0,

  registerListener: function(aListener) {
    if (this._listeners.indexOf(aListener) >= 0) {
      throw Cr.NS_ERROR_UNEXPECTED;
    }

    this._listeners.push(aListener);
  },

  unregisterListener: function(aListener) {
    let index = this._listeners.indexOf(aListener);
    if (index < 0) {
      throw Cr.NS_ERROR_UNEXPECTED;
    }

    this._listeners.splice(index, 1);
  },

  enumerateCalls: function(aListener) {
    if (DEBUG) debug("Requesting enumeration of calls for callback");

    for (let cid = 0; cid < this._numClients; ++cid) {
      let calls = this._currentCalls[cid];
      if (!calls) {
        continue;
      }
      for (let i = 0, indexes = Object.keys(calls); i < indexes.length; ++i) {
        let call = calls[indexes[i]];
        let callInfo = new TelephonyCallInfo(call);
        aListener.enumerateCallState(callInfo);
      }
    }
    aListener.enumerateCallStateComplete();
  },

  _hasCalls: function(aClientId) {
    return Object.keys(this._currentCalls[aClientId]).length !== 0;
  },

  _hasCallsOnOtherClient: function(aClientId) {
    for (let cid = 0; cid < this._numClients; ++cid) {
      if (cid !== aClientId && this._hasCalls(cid)) {
        return true;
      }
    }
    return false;
  },

  // All calls in the conference is regarded as one conference call.
  _numCallsOnLine: function(aClientId) {
    let numCalls = 0;
    let hasConference = false;

    for (let cid in this._currentCalls[aClientId]) {
      let call = this._currentCalls[aClientId][cid];
      if (call.isConference) {
        hasConference = true;
      } else {
        numCalls++;
      }
    }

    return hasConference ? numCalls + 1 : numCalls;
  },

  /**
   * Get arbitrary one of active call.
   */
  _getOneActiveCall: function(aClientId) {
    for (let index in this._currentCalls[aClientId]) {
      let call = this._currentCalls[aClientId][index];
      if (call.state === nsITelephonyService.CALL_STATE_CONNECTED) {
        return call;
      }
    }
    return null;
  },

  _addCdmaChildCall: function(aClientId, aNumber, aParentId) {
    let childCall = {
      callIndex: CDMA_SECOND_CALL_INDEX,
      state: RIL.CALL_STATE_DIALING,
      number: aNumber,
      isOutgoing: true,
      isEmergency: false,
      isConference: false,
      isSwitchable: false,
      isMergeable: true,
      parentId: aParentId
    };

    // Manual update call state according to the request response.
    this.notifyCallStateChanged(aClientId, childCall);

    childCall.state = RIL.CALL_STATE_ACTIVE;
    this.notifyCallStateChanged(aClientId, childCall);

    let parentCall = this._currentCalls[aClientId][childCall.parentId];
    parentCall.childId = CDMA_SECOND_CALL_INDEX;
    parentCall.state = RIL.CALL_STATE_HOLDING;
    parentCall.isSwitchable = false;
    parentCall.isMergeable = true;
    this.notifyCallStateChanged(aClientId, parentCall);
  },

  dial: function(aClientId, aNumber, aIsDialEmergency, aCallback) {
    if (DEBUG) debug("Dialing " + (aIsDialEmergency ? "emergency " : "") + aNumber);

    // We don't try to be too clever here, as the phone is probably in the
    // locked state. Let's just check if it's a number without normalizing
    if (!aIsDialEmergency) {
      aNumber = gPhoneNumberUtils.normalize(aNumber);
    }

    // Validate the number.
    // Note: isPlainPhoneNumber also accepts USSD and SS numbers
    if (!gPhoneNumberUtils.isPlainPhoneNumber(aNumber)) {
      if (DEBUG) debug("Error: Number '" + aNumber + "' is not viable. Drop.");
      aCallback.notifyError(DIAL_ERROR_BAD_NUMBER);
      return;
    }

    if (this._hasCalls(aClientId)) {
      // 3GPP TS 22.030 6.5.5
      // Handling of supplementary services within a call.

      let mmiCallback = response => {
        aCallback.notifyDialMMI(RIL.MMI_KS_SC_CALL);
        if (!response.success) {
          aCallback.notifyDialMMIError(RIL.MMI_ERROR_KS_ERROR);
        } else {
          aCallback.notifyDialMMISuccess(RIL.MMI_SM_KS_CALL_CONTROL);
        }
      };

      if (aNumber === "0") {
        this._sendToRilWorker(aClientId, "hangUpBackground", null, mmiCallback);
      } else if (aNumber === "1") {
        this._sendToRilWorker(aClientId, "hangUpForeground", null, mmiCallback);
      } else if (aNumber[0] === "1" && aNumber.length === 2) {
        this._sendToRilWorker(aClientId, "hangUpCall",
                              { callIndex: parseInt(aNumber[1]) }, mmiCallback);
      } else if (aNumber === "2") {
        this._sendToRilWorker(aClientId, "switchActiveCall", null, mmiCallback);
      } else if (aNumber[0] === "2" && aNumber.length === 2) {
        this._sendToRilWorker(aClientId, "separateCall",
                              { callIndex: parseInt(aNumber[1]) }, mmiCallback);
      } else if (aNumber === "3") {
        this._sendToRilWorker(aClientId, "conferenceCall", null, mmiCallback);
      } else {
        // Entering "Directory Number"
        this._dialCall(aClientId,
                       { number: aNumber,
                         isDialEmergency: aIsDialEmergency }, aCallback);
      }
    } else {
      let mmi = gDialNumberUtils.parseMMI(aNumber);
      if (!mmi) {
        this._dialCall(aClientId,
                       { number: aNumber,
                         isDialEmergency: aIsDialEmergency }, aCallback);
      } else if (this._isTemporaryCLIR(mmi)) {
        this._dialCall(aClientId,
                       { number: mmi.dialNumber,
                         clirMode: this._getTemporaryCLIRMode(mmi.procedure),
                         isDialEmergency: aIsDialEmergency }, aCallback);
      } else {
        // Reject MMI code from dialEmergency api.
        if (aIsDialEmergency) {
          aCallback.notifyError(DIAL_ERROR_BAD_NUMBER);
          return;
        }

        this._dialMMI(aClientId, mmi, aCallback);
      }
    }
  },

  /**
   * @param aOptions.number
   * @param aOptions.clirMode (optional)
   * @param aOptions.isDialEmergency
   */
  _dialCall: function(aClientId, aOptions, aCallback) {
    if (this._isDialing) {
      if (DEBUG) debug("Error: Already has a dialing call.");
      aCallback.notifyError(DIAL_ERROR_INVALID_STATE_ERROR);
      return;
    }

    // We can only have at most two calls on the same line (client).
    if (this._numCallsOnLine(aClientId) >= 2) {
      if (DEBUG) debug("Error: Already has more than 2 calls on line.");
      aCallback.notifyError(DIAL_ERROR_INVALID_STATE_ERROR);
      return;
    }

    // For DSDS, if there is aleady a call on SIM 'aClientId', we cannot place
    // any new call on other SIM.
    if (this._hasCallsOnOtherClient(aClientId)) {
      if (DEBUG) debug("Error: Already has a call on other sim.");
      aCallback.notifyError(DIAL_ERROR_OTHER_CONNECTION_IN_USE);
      return;
    }

    aOptions.isEmergency = gDialNumberUtils.isEmergency(aOptions.number);
    if (aOptions.isEmergency) {
      // Automatically select a proper clientId for emergency call.
      aClientId = gRadioInterfaceLayer.getClientIdForEmergencyCall() ;
      if (aClientId === -1) {
        if (DEBUG) debug("Error: No client is avaialble for emergency call.");
        aCallback.notifyError(DIAL_ERROR_INVALID_STATE_ERROR);
        return;
      }
    }

    // Before we dial, we have to hold the active call first.
    let activeCall = this._getOneActiveCall(aClientId);
    if (!activeCall) {
      this._sendDialCallRequest(aClientId, aOptions, aCallback);
    } else {
      if (DEBUG) debug("There is an active call. Hold it first before dial.");

      this._cachedDialRequest = {
        clientId: aClientId,
        options: aOptions,
        callback: aCallback
      };

      if (activeCall.isConference) {
        this.holdConference(aClientId,
                            { notifySuccess: function () {},
                              notifyError: function (errorMsg) {} });
      } else {
        this.holdCall(aClientId, activeCall.callIndex,
                      { notifySuccess: function () {},
                        notifyError: function (errorMsg) {} });
      }
    }
  },

  _sendDialCallRequest: function(aClientId, aOptions, aCallback) {
    this._isDialing = true;

    this._sendToRilWorker(aClientId, "dial", aOptions, response => {
      this._isDialing = false;

      if (!response.success) {
        aCallback.notifyError(response.errorMsg);
        return;
      }

      let currentCdmaCallIndex = !response.isCdma ? null :
        Object.keys(this._currentCalls[aClientId])[0];

      if (currentCdmaCallIndex == null) {
        aCallback.notifyDialCallSuccess(aClientId, response.callIndex,
                                        response.number);
      } else {
        // RIL doesn't hold the 2nd call. We create one by ourselves.
        aCallback.notifyDialCallSuccess(aClientId, CDMA_SECOND_CALL_INDEX,
                                        response.number);
        this._addCdmaChildCall(aClientId, response.number, currentCdmaCallIndex);
      }
    });
  },

  /**
   * @param aClientId
   *        Client id.
   * @param aMmi
   *        Parsed MMI structure.
   * @param aCallback
   *        A nsITelephonyDialCallback object.
   * @param aStartNewSession
   *        True to start a new session for ussd request.
   */
  _dialMMI: function(aClientId, aMmi, aCallback) {
    let mmiServiceCode = aMmi ?
      this._serviceCodeToKeyString(aMmi.serviceCode) : RIL.MMI_KS_SC_USSD;

    aCallback.notifyDialMMI(mmiServiceCode);

    this._sendToRilWorker(aClientId, "sendMMI",
                          { mmi: aMmi }, response => {
      if (DEBUG) debug("MMI response: " + JSON.stringify(response));

      if (!response.success) {
        if (response.additionalInformation != null) {
          aCallback.notifyDialMMIErrorWithInfo(response.errorMsg,
                                               response.additionalInformation);
        } else {
          aCallback.notifyDialMMIError(response.errorMsg);
        }
        return;
      }

      // We expect to have an IMEI at this point if the request was supposed
      // to query for the IMEI, so getting a successful reply from the RIL
      // without containing an actual IMEI number is considered an error.
      if (mmiServiceCode === RIL.MMI_KS_SC_IMEI &&
          !response.statusMessage) {
        aCallback.notifyDialMMIError(RIL.GECKO_ERROR_GENERIC_FAILURE);
        return;
      }

      // MMI query call forwarding options request returns a set of rules that
      // will be exposed in the form of an array of MozCallForwardingOptions
      // instances.
      if (mmiServiceCode === RIL.MMI_KS_SC_CALL_FORWARDING) {
        if (response.isSetCallForward) {
          gGonkMobileConnectionService.notifyCFStateChanged(aClientId,
                                                            response.action,
                                                            response.reason,
                                                            response.number,
                                                            response.timeSeconds,
                                                            response.serviceClass);
        }

        if (response.additionalInformation != null) {
          let callForwardingOptions =
            this._rulesToCallForwardingOptions(response.additionalInformation);
          aCallback.notifyDialMMISuccessWithCallForwardingOptions(
            response.statusMessage, callForwardingOptions.length, callForwardingOptions);
          return;
        }
      }

      // No additional information
      if (response.additionalInformation === undefined) {
        aCallback.notifyDialMMISuccess(response.statusMessage);
        return;
      }

      // Additional information is an integer.
      if (!isNaN(parseInt(response.additionalInformation, 10))) {
        aCallback.notifyDialMMISuccessWithInteger(
          response.statusMessage, response.additionalInformation);
        return;
      }

      // Additional information is an array of strings.
      let array = response.additionalInformation;
      if (Array.isArray(array) && array.length > 0 && typeof array[0] === "string") {
        aCallback.notifyDialMMISuccessWithStrings(response.statusMessage,
                                                  array.length, array);
        return;
      }

      aCallback.notifyDialMMISuccess(response.statusMessage);
    });
  },

  _serviceCodeToKeyString: function(aServiceCode) {
    switch (aServiceCode) {
      case RIL.MMI_SC_CFU:
      case RIL.MMI_SC_CF_BUSY:
      case RIL.MMI_SC_CF_NO_REPLY:
      case RIL.MMI_SC_CF_NOT_REACHABLE:
      case RIL.MMI_SC_CF_ALL:
      case RIL.MMI_SC_CF_ALL_CONDITIONAL:
        return RIL.MMI_KS_SC_CALL_FORWARDING;
      case RIL.MMI_SC_PIN:
        return RIL.MMI_KS_SC_PIN;
      case RIL.MMI_SC_PIN2:
        return RIL.MMI_KS_SC_PIN2;
      case RIL.MMI_SC_PUK:
        return RIL.MMI_KS_SC_PUK;
      case RIL.MMI_SC_PUK2:
        return RIL.MMI_KS_SC_PUK2;
      case RIL.MMI_SC_IMEI:
        return RIL.MMI_KS_SC_IMEI;
      case RIL.MMI_SC_CLIP:
        return RIL.MMI_KS_SC_CLIP;
      case RIL.MMI_SC_CLIR:
        return RIL.MMI_KS_SC_CLIR;
      case RIL.MMI_SC_BAOC:
      case RIL.MMI_SC_BAOIC:
      case RIL.MMI_SC_BAOICxH:
      case RIL.MMI_SC_BAIC:
      case RIL.MMI_SC_BAICr:
      case RIL.MMI_SC_BA_ALL:
      case RIL.MMI_SC_BA_MO:
      case RIL.MMI_SC_BA_MT:
        return RIL.MMI_KS_SC_CALL_BARRING;
      case RIL.MMI_SC_CALL_WAITING:
        return RIL.MMI_KS_SC_CALL_WAITING;
      case RIL.MMI_SC_CHANGE_PASSWORD:
        return RIL.MMI_KS_SC_CHANGE_PASSWORD;
      default:
        return RIL.MMI_KS_SC_USSD;
    }
  },

  /**
   * The default callback handler for call operations.
   *
   * @param aCallback
   *        An callback object including notifySuccess() and notifyError(aMsg)
   * @param aResponse
   *        The response from ril_worker.
   */
  _defaultCallbackHandler: function(aCallback, aResponse) {
    if (!aResponse.success) {
      aCallback.notifyError(aResponse.errorMsg);
    } else {
      aCallback.notifySuccess();
    }
  },

  sendTones: function(aClientId, aDtmfChars, aPauseDuration, aToneDuration,
                      aCallback) {
    let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    let tones = aDtmfChars;
    let playTone = (tone) => {
      this._sendToRilWorker(aClientId, "startTone", { dtmfChar: tone }, response => {
        if (!response.success) {
          aCallback.notifyError(response.errorMsg);
          return;
        }

        timer.initWithCallback(() => {
          this.stopTone();
          timer.initWithCallback(() => {
            if (tones.length === 1) {
              aCallback.notifySuccess();
            } else {
              tones = tones.substr(1);
              playTone(tones[0]);
            }
          }, TONES_GAP_DURATION, Ci.nsITimer.TYPE_ONE_SHOT);
        }, aToneDuration, Ci.nsITimer.TYPE_ONE_SHOT);
      });
    };

    timer.initWithCallback(() => {
      playTone(tones[0]);
    }, aPauseDuration, Ci.nsITimer.TYPE_ONE_SHOT);
  },

  startTone: function(aClientId, aDtmfChar) {
    this._sendToRilWorker(aClientId, "startTone", { dtmfChar: aDtmfChar });
  },

  stopTone: function(aClientId) {
    this._sendToRilWorker(aClientId, "stopTone");
  },

  answerCall: function(aClientId, aCallIndex, aCallback) {
    this._sendToRilWorker(aClientId, "answerCall", { callIndex: aCallIndex },
                          this._defaultCallbackHandler.bind(this, aCallback));
  },

  rejectCall: function(aClientId, aCallIndex, aCallback) {
    this._sendToRilWorker(aClientId, "rejectCall", { callIndex: aCallIndex },
                          this._defaultCallbackHandler.bind(this, aCallback));
  },

  hangUpCall: function(aClientId, aCallIndex, aCallback) {
    let parentId = this._currentCalls[aClientId][aCallIndex].parentId;
    if (parentId) {
      // Should release both, child and parent, together. Since RIL holds only
      // the parent call, we send 'parentId' to RIL.
      this.hangUpCall(aClientId, parentId, aCallback);
    } else {
      this._sendToRilWorker(aClientId, "hangUpCall", { callIndex: aCallIndex },
                            this._defaultCallbackHandler.bind(this, aCallback));
    }
  },

  holdCall: function(aClientId, aCallIndex, aCallback) {
    let call = this._currentCalls[aClientId][aCallIndex];
    if (!call || !call.isSwitchable) {
      aCallback.notifyError(RIL.GECKO_ERROR_GENERIC_FAILURE);
      return;
    }

    this._sendToRilWorker(aClientId, "holdCall", { callIndex: aCallIndex },
                          this._defaultCallbackHandler.bind(this, aCallback));
  },

  resumeCall: function(aClientId, aCallIndex, aCallback) {
    let call = this._currentCalls[aClientId][aCallIndex];
    if (!call || !call.isSwitchable) {
      aCallback.notifyError(RIL.GECKO_ERROR_GENERIC_FAILURE);
      return;
    }

    this._sendToRilWorker(aClientId, "resumeCall", { callIndex: aCallIndex },
                          this._defaultCallbackHandler.bind(this, aCallback));
  },

  conferenceCall: function(aClientId, aCallback) {
    let indexes = Object.keys(this._currentCalls[aClientId]);
    if (indexes.length < 2) {
      aCallback.notifyError(RIL.GECKO_ERROR_GENERIC_FAILURE);
      return;
    }

    for (let i = 0; i < indexes.length; ++i) {
      let call = this._currentCalls[aClientId][indexes[i]];
      if (!call.isMergeable) {
        aCallback.notifyError(RIL.GECKO_ERROR_GENERIC_FAILURE);
        return;
      }
    }

    this._sendToRilWorker(aClientId, "conferenceCall", null, response => {
      if (!response.success) {
        aCallback.notifyError(RIL.GECKO_ERROR_GENERIC_FAILURE);
        this._notifyAllListeners("notifyConferenceError", [response.errorName,
                                                           response.errorMsg]);
        return;
      }

      if (response.isCdma) {
        let indexes = Object.keys(this._currentCalls[aClientId]);
        if (indexes.length < 2) {
          aCallback.notifyError(RIL.GECKO_ERROR_GENERIC_FAILURE);
          return;
        }

        for (let i = 0; i < indexes.length; ++i) {
          let call = this._currentCalls[aClientId][indexes[i]];
          call.state = RIL.CALL_STATE_ACTIVE;
          call.isConference = true;
          this.notifyCallStateChanged(aClientId, call);
        }
        this.notifyConferenceCallStateChanged(RIL.CALL_STATE_ACTIVE);
      }

      aCallback.notifySuccess();
    });
  },

  separateCall: function(aClientId, aCallIndex, aCallback) {
    let call = this._currentCalls[aClientId][aCallIndex];
    if (!call || !call.isConference) {
      aCallback.notifyError(RIL.GECKO_ERROR_GENERIC_FAILURE);
      return;
    }

    let parentId = call.parentId;
    if (parentId) {
      this.separateCall(aClientId, parentId);
      return;
    }

    this._sendToRilWorker(aClientId, "separateCall", { callIndex: aCallIndex },
                          response => {
      if (!response.success) {
        aCallback.notifyError(RIL.GECKO_ERROR_GENERIC_FAILURE);
        this._notifyAllListeners("notifyConferenceError", [response.errorName,
                                                           response.errorMsg]);
        return;
      }

      if (response.isCdma) {
        // See 3gpp2, S.R0006-522-A v1.0. Table 4, XID 6S.
        let call = this._currentCalls[aClientId][aCallIndex];
        if (!call || !call.isConference || !call.childId) {
          aCallback.notifyError(RIL.GECKO_ERROR_GENERIC_FAILURE);
          return;
        }

        let childCall = this._currentCalls[aClientId][call.childId];
        this.notifyCallDisconnected(aClientId, childCall);
      }

      aCallback.notifySuccess();
    });
  },

  hangUpConference: function(aClientId, aCallback) {
    this._sendToRilWorker(aClientId, "hangUpConference", null,
                          this._defaultCallbackHandler.bind(this, aCallback));
  },

  holdConference: function(aClientId, aCallback) {
    this._sendToRilWorker(aClientId, "holdConference", null,
                          this._defaultCallbackHandler.bind(this, aCallback));
  },

  resumeConference: function(aClientId, aCallback) {
    this._sendToRilWorker(aClientId, "resumeConference", null,
                          this._defaultCallbackHandler.bind(this, aCallback));
  },

  sendUSSD: function(aClientId, aUssd, aCallback) {
    this._sendToRilWorker(aClientId, "sendUSSD",
                          { ussd: aUssd, checkSession: true },
                          response => {
      if (!response.success) {
        aCallback.notifyError(response.errorMsg);
      } else {
        aCallback.notifySuccess();
      }
    });
  },

  cancelUSSD: function(aClientId, aCallback) {
    this._sendToRilWorker(aClientId, "cancelUSSD", {}, response => {
      if (!response.success) {
        aCallback.notifyError(response.errorMsg);
      } else {
        aCallback.notifySuccess();
      }
    });
  },

  get microphoneMuted() {
    return gAudioService.microphoneMuted;
  },

  set microphoneMuted(aMuted) {
    gAudioService.microphoneMuted = aMuted;
  },

  get speakerEnabled() {
    return gAudioService.speakerEnabled;
  },

  set speakerEnabled(aEnabled) {
    gAudioService.speakerEnabled = aEnabled;
  },

  /**
   * nsIGonkTelephonyService interface.
   */

  notifyAudioStateChanged: function(aClientId, aState) {
    this._audioStates[aClientId] = aState;

    let audioState = aState;
    for (let i = 0; i < this._numClients; ++i) {
      audioState = Math.max(audioState, this._audioStates[i]);
    }

    this._updateAudioState(audioState);
  },

  /**
   * Handle call disconnects by updating our current state and the audio system.
   */
  notifyCallDisconnected: function(aClientId, aCall) {
    if (DEBUG) debug("handleCallDisconnected: " + JSON.stringify(aCall));

    aCall.clientId = aClientId;
    aCall.state = nsITelephonyService.CALL_STATE_DISCONNECTED;
    aCall.isEmergency = gDialNumberUtils.isEmergency(aCall.number);
    let duration = ("started" in aCall && typeof aCall.started == "number") ?
      new Date().getTime() - aCall.started : 0;

    gTelephonyMessenger.notifyCallEnded(aClientId,
                                        aCall.number,
                                        this._cdmaCallWaitingNumber,
                                        aCall.isEmergency,
                                        duration,
                                        aCall.isOutgoing,
                                        aCall.hangUpLocal);

    // Clear cache of this._cdmaCallWaitingNumber after call disconnected.
    this._cdmaCallWaitingNumber = null;

    let manualConfStateChange = false;
    let childId = this._currentCalls[aClientId][aCall.callIndex].childId;
    if (childId) {
      // Child cannot live without parent.
      let childCall = this._currentCalls[aClientId][childId];
      this.notifyCallDisconnected(aClientId, childCall);
    } else {
      let parentId = this._currentCalls[aClientId][aCall.callIndex].parentId;
      if (parentId) {
        let parentCall = this._currentCalls[aClientId][parentId];
        // The child is going to be released.
        delete parentCall.childId;
        if (parentCall.isConference) {
          // As the child is going to be gone, the parent should be moved out
          // of conference accordingly.
          manualConfStateChange = true;
          parentCall.isConference = false;
          parentCall.isSwitchable = true;
          parentCall.isMergeable = true;
          aCall.isConference = false;
          this.notifyCallStateChanged(aClientId, parentCall, true);
        }
      }
    }

    if (!aCall.failCause ||
        aCall.failCause === RIL.GECKO_CALL_ERROR_NORMAL_CALL_CLEARING) {
      let callInfo = new TelephonyCallInfo(aCall);
      this._notifyAllListeners("callStateChanged", [callInfo]);
    } else {
      this._notifyAllListeners("notifyError",
                               [aClientId, aCall.callIndex, aCall.failCause]);
    }
    delete this._currentCalls[aClientId][aCall.callIndex];

    if (manualConfStateChange) {
      this.notifyConferenceCallStateChanged(RIL.CALL_STATE_UNKNOWN);
    }
  },

  /**
   * Handle an incoming call.
   *
   * Not much is known about this call at this point, but it's enough
   * to start bringing up the Phone app already.
   */
  notifyCallRing: function() {
    // We need to acquire a CPU wake lock to avoid the system falling into
    // the sleep mode when the RIL handles the incoming call.
    this._acquireCallRingWakeLock();

    gTelephonyMessenger.notifyNewCall();
  },

  /**
   * Handle call state changes by updating our current state and the audio
   * system.
   */
  notifyCallStateChanged: function(aClientId, aCall, aSkipStateConversion) {
    if (DEBUG) debug("handleCallStateChange: " + JSON.stringify(aCall));

    if (!aSkipStateConversion) {
      aCall.state = this._convertRILCallState(aCall.state);
    }

    if (aCall.state == nsITelephonyService.CALL_STATE_DIALING) {
      gTelephonyMessenger.notifyNewCall();
    }

    aCall.clientId = aClientId;

    function pick(arg, defaultValue) {
      return typeof arg !== 'undefined' ? arg : defaultValue;
    }

    let call = this._currentCalls[aClientId][aCall.callIndex];
    if (call) {
      call.state = aCall.state;
      call.number = aCall.number;
      call.isConference = aCall.isConference;
      call.isEmergency = gDialNumberUtils.isEmergency(aCall.number);
      call.isSwitchable = pick(aCall.isSwitchable, call.isSwitchable);
      call.isMergeable = pick(aCall.isMergeable, call.isMergeable);
    } else {
      call = aCall;
      call.isEmergency = pick(aCall.isEmergency, gDialNumberUtils.isEmergency(aCall.number));
      call.isSwitchable = pick(aCall.isSwitchable, true);
      call.isMergeable = pick(aCall.isMergeable, true);
      call.name = pick(aCall.name, "");
      call.numberPresentaation = pick(aCall.numberPresentation, nsITelephonyService.CALL_PRESENTATION_ALLOWED);
      call.namePresentaation = pick(aCall.namePresentation, nsITelephonyService.CALL_PRESENTATION_ALLOWED);

      this._currentCalls[aClientId][aCall.callIndex] = call;
    }

    // Handle cached dial request.
    if (this._cachedDialRequest && !this._getOneActiveCall(aClientId)) {
      if (DEBUG) debug("All calls held. Perform the cached dial request.");

      let request = this._cachedDialRequest;
      this._sendDialCallRequest(request.clientId, request.options, request.callback);
      this._cachedDialRequest = null;
    }

    let callInfo = new TelephonyCallInfo(call);
    this._notifyAllListeners("callStateChanged", [callInfo]);
  },

  notifyCdmaCallWaiting: function(aClientId, aCall) {
    // We need to acquire a CPU wake lock to avoid the system falling into
    // the sleep mode when the RIL handles the incoming call.
    this._acquireCallRingWakeLock();

    let call = this._currentCalls[aClientId][CDMA_SECOND_CALL_INDEX];
    if (call) {
      // TODO: Bug 977503 - B2G RIL: [CDMA] update callNumber when a waiting
      // call comes after a 3way call.
      this.notifyCallDisconnected(aClientId, call);
    }

    this._cdmaCallWaitingNumber = aCall.number;

    this._notifyAllListeners("notifyCdmaCallWaiting", [aClientId,
                                                       aCall.number,
                                                       aCall.numberPresentation,
                                                       aCall.name,
                                                       aCall.namePresentation]);
  },

  notifySupplementaryService: function(aClientId, aCallIndex, aNotification) {
    let notification = this._convertRILSuppSvcNotification(aNotification);
    this._notifyAllListeners("supplementaryServiceNotification",
                             [aClientId, aCallIndex, notification]);
  },

  notifyConferenceCallStateChanged: function(aState) {
    if (DEBUG) debug("handleConferenceCallStateChanged: " + aState);
    aState = this._convertRILCallState(aState);
    this._notifyAllListeners("conferenceCallStateChanged", [aState]);
  },

  notifyUssdReceived: function(aClientId, aMessage, aSessionEnded) {
    if (DEBUG) {
      debug("notifyUssdReceived for " + aClientId + ": " +
            aMessage + " (sessionEnded : " + aSessionEnded + ")");
    }

    gTelephonyMessenger.notifyUssdReceived(aClientId, aMessage, aSessionEnded);
  },

  /**
   * nsIObserver interface.
   */

  observe: function(aSubject, aTopic, aData) {
    switch (aTopic) {
      case NS_PREFBRANCH_PREFCHANGE_TOPIC_ID:
        if (aData === kPrefRilDebuggingEnabled) {
          this._updateDebugFlag();
        } else if (aData === kPrefDefaultServiceId) {
          this.defaultServiceId = this._getDefaultServiceId();
        }
        break;

      case NS_XPCOM_SHUTDOWN_OBSERVER_ID:
        // Release the CPU wake lock for handling the incoming call.
        this._releaseCallRingWakeLock();

        Services.obs.removeObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
        break;
    }
  }
};

/**
 * This implements nsISystemMessagesWrapper.wrapMessage(), which provides a
 * plugable way to wrap a "ussd-received" type system message.
 *
 * Please see SystemMessageManager.js to know how it customizes the wrapper.
 */
function USSDReceivedWrapper() {
  if (DEBUG) debug("USSDReceivedWrapper()");
}
USSDReceivedWrapper.prototype = {
  // nsISystemMessagesWrapper implementation.
  wrapMessage: function(aMessage, aWindow) {
    if (DEBUG) debug("wrapMessage: " + JSON.stringify(aMessage));

    let session = aMessage.sessionEnded ? null :
      new aWindow.USSDSession(aMessage.serviceId);

    let event = new aWindow.USSDReceivedEvent("ussdreceived", {
      serviceId: aMessage.serviceId,
      message: aMessage.message,
      session: session
    });

    return event;
  },

  classDescription: "USSDReceivedWrapper",
  classID: Components.ID("{d03684ed-ede4-4210-8206-f4f32772d9f5}"),
  contractID: "@mozilla.org/dom/system-messages/wrapper/ussd-received;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISystemMessagesWrapper])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([TelephonyService,
                                                    USSDReceivedWrapper]);
