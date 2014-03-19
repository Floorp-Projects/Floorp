/* -*- Mode: js; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Promise.jsm");

var RIL = {};
Cu.import("resource://gre/modules/ril_consts.js", RIL);

const GONK_TELEPHONYPROVIDER_CONTRACTID =
  "@mozilla.org/telephony/gonktelephonyprovider;1";
const GONK_TELEPHONYPROVIDER_CID =
  Components.ID("{67d26434-d063-4d28-9f48-5b3189788155}");

const NS_XPCOM_SHUTDOWN_OBSERVER_ID   = "xpcom-shutdown";

const NS_PREFBRANCH_PREFCHANGE_TOPIC_ID = "nsPref:changed";

const kPrefRilNumRadioInterfaces = "ril.numRadioInterfaces";
const kPrefRilDebuggingEnabled = "ril.debugging.enabled";
const kPrefDefaultServiceId = "dom.telephony.defaultServiceId";

const nsIAudioManager = Ci.nsIAudioManager;
const nsITelephonyProvider = Ci.nsITelephonyProvider;

const CALL_WAKELOCK_TIMEOUT = 5000;

// Index of the CDMA second call which isn't held in RIL but only in TelephoyProvider.
const CDMA_SECOND_CALL_INDEX = 2;

let DEBUG;
function debug(s) {
  dump("TelephonyProvider: " + s + "\n");
}

XPCOMUtils.defineLazyGetter(this, "gAudioManager", function getAudioManager() {
  try {
    return Cc["@mozilla.org/telephony/audiomanager;1"]
             .getService(nsIAudioManager);
  } catch (ex) {
    //TODO on the phone this should not fall back as silently.

    // Fake nsIAudioManager implementation so that we can run the telephony
    // code in a non-Gonk build.
    if (DEBUG) debug("Using fake audio manager.");
    return {
      microphoneMuted: false,
      masterVolume: 1.0,
      masterMuted: false,
      phoneState: nsIAudioManager.PHONE_STATE_CURRENT,
      _forceForUse: {},

      setForceForUse: function(usage, force) {
        this._forceForUse[usage] = force;
      },

      getForceForUse: function(usage) {
        return this._forceForUse[usage] || nsIAudioManager.FORCE_NONE;
      }
    };
  }
});

XPCOMUtils.defineLazyServiceGetter(this, "gRadioInterfaceLayer",
                                   "@mozilla.org/ril;1",
                                   "nsIRadioInterfaceLayer");

XPCOMUtils.defineLazyServiceGetter(this, "gPowerManagerService",
                                   "@mozilla.org/power/powermanagerservice;1",
                                   "nsIPowerManagerService");

XPCOMUtils.defineLazyServiceGetter(this, "gSystemMessenger",
                                   "@mozilla.org/system-message-internal;1",
                                   "nsISystemMessagesInternal");

XPCOMUtils.defineLazyGetter(this, "gPhoneNumberUtils", function() {
  let ns = {};
  Cu.import("resource://gre/modules/PhoneNumberUtils.jsm", ns);
  return ns.PhoneNumberUtils;
});

function SingleCall(options){
  this.clientId = options.clientId;
  this.callIndex = options.callIndex;
  this.state = options.state;
  this.number = options.number;
  this.isOutgoing = options.isOutgoing;
  this.isEmergency = options.isEmergency;
  this.isConference = options.isConference;
}
SingleCall.prototype = {
  clientId: null,
  callIndex: null,
  state: null,
  number: null,
  isOutgoing: false,
  isEmergency: false,
  isConference: false
};

function ConferenceCall(state){
  this.state = state;
}
ConferenceCall.prototype = {
  state: null
};

function TelephonyProvider() {
  this._numClients = gRadioInterfaceLayer.numRadioInterfaces;
  this._listeners = [];
  this._currentCalls = {};
  this._updateDebugFlag();
  this.defaultServiceId = this._getDefaultServiceId();

  Services.prefs.addObserver(kPrefRilDebuggingEnabled, this, false);
  Services.prefs.addObserver(kPrefDefaultServiceId, this, false);

  Services.obs.addObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);

  for (let i = 0; i < this._numClients; ++i) {
    this._enumerateCallsForClient(i);
  }
}
TelephonyProvider.prototype = {
  classID: GONK_TELEPHONYPROVIDER_CID,
  classInfo: XPCOMUtils.generateCI({classID: GONK_TELEPHONYPROVIDER_CID,
                                    contractID: GONK_TELEPHONYPROVIDER_CONTRACTID,
                                    classDescription: "TelephonyProvider",
                                    interfaces: [Ci.nsITelephonyProvider,
                                                 Ci.nsIGonkTelephonyProvider],
                                    flags: Ci.nsIClassInfo.SINGLETON}),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsITelephonyProvider,
                                         Ci.nsIGonkTelephonyProvider,
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

  _matchActiveSingleCall: function(aCall) {
    return this._activeCall &&
           this._activeCall instanceof SingleCall &&
           this._activeCall.clientId === aCall.clientId &&
           this._activeCall.callIndex === aCall.callIndex;
  },

  /**
   * Track the active call and update the audio system as its state changes.
   */
  _activeCall: null,
  _updateCallAudioState: function(aCall, aConferenceState) {
    if (aConferenceState === nsITelephonyProvider.CALL_STATE_CONNECTED) {
      this._activeCall = new ConferenceCall(aConferenceState);
      gAudioManager.phoneState = nsIAudioManager.PHONE_STATE_IN_CALL;
      if (this.speakerEnabled) {
        gAudioManager.setForceForUse(nsIAudioManager.USE_COMMUNICATION,
                                     nsIAudioManager.FORCE_SPEAKER);
      }
      if (DEBUG) {
        debug("Active call, put audio system into PHONE_STATE_IN_CALL: " +
              gAudioManager.phoneState);
      }
      return;
    }

    if (aConferenceState === nsITelephonyProvider.CALL_STATE_UNKNOWN ||
        aConferenceState === nsITelephonyProvider.CALL_STATE_HELD) {
      if (this._activeCall instanceof ConferenceCall) {
        this._activeCall = null;
        gAudioManager.phoneState = nsIAudioManager.PHONE_STATE_NORMAL;
        if (DEBUG) {
          debug("No active call, put audio system into PHONE_STATE_NORMAL: " +
                gAudioManager.phoneState);
        }
      }
      return;
    }

    if (!aCall) {
      return;
    }

    if (aCall.isConference) {
      if (this._matchActiveSingleCall(aCall)) {
        this._activeCall = null;
      }
      return;
    }

    switch (aCall.state) {
      case nsITelephonyProvider.CALL_STATE_DIALING: // Fall through...
      case nsITelephonyProvider.CALL_STATE_ALERTING:
      case nsITelephonyProvider.CALL_STATE_CONNECTED:
        aCall.isActive = true;
        this._activeCall = new SingleCall(aCall);
        gAudioManager.phoneState = nsIAudioManager.PHONE_STATE_IN_CALL;
        if (this.speakerEnabled) {
          gAudioManager.setForceForUse(nsIAudioManager.USE_COMMUNICATION,
                                       nsIAudioManager.FORCE_SPEAKER);
        }
        if (DEBUG) {
          debug("Active call, put audio system into PHONE_STATE_IN_CALL: " +
                gAudioManager.phoneState);
        }
        break;

      case nsITelephonyProvider.CALL_STATE_INCOMING:
        aCall.isActive = false;
        if (!this._activeCall) {
          // We can change the phone state into RINGTONE only when there's
          // no active call.
          gAudioManager.phoneState = nsIAudioManager.PHONE_STATE_RINGTONE;
          if (DEBUG) {
            debug("Incoming call, put audio system into PHONE_STATE_RINGTONE: " +
                  gAudioManager.phoneState);
          }
        }
        break;

      case nsITelephonyProvider.CALL_STATE_HELD: // Fall through...
      case nsITelephonyProvider.CALL_STATE_DISCONNECTED:
        aCall.isActive = false;
        if (this._matchActiveSingleCall(aCall)) {
          // Previously active call is not active now.
          this._activeCall = null;
        }

        if (!this._activeCall) {
          // No active call. Disable the audio.
          gAudioManager.phoneState = nsIAudioManager.PHONE_STATE_NORMAL;
          if (DEBUG) {
            debug("No active call, put audio system into PHONE_STATE_NORMAL: " +
                  gAudioManager.phoneState);
          }
        }
        break;
    }
  },

  _convertRILCallState: function(aState) {
    switch (aState) {
      case RIL.CALL_STATE_UNKNOWN:
        return nsITelephonyProvider.CALL_STATE_UNKNOWN;
      case RIL.CALL_STATE_ACTIVE:
        return nsITelephonyProvider.CALL_STATE_CONNECTED;
      case RIL.CALL_STATE_HOLDING:
        return nsITelephonyProvider.CALL_STATE_HELD;
      case RIL.CALL_STATE_DIALING:
        return nsITelephonyProvider.CALL_STATE_DIALING;
      case RIL.CALL_STATE_ALERTING:
        return nsITelephonyProvider.CALL_STATE_ALERTING;
      case RIL.CALL_STATE_INCOMING:
      case RIL.CALL_STATE_WAITING:
        return nsITelephonyProvider.CALL_STATE_INCOMING;
      default:
        throw new Error("Unknown rilCallState: " + aState);
    }
  },

  _convertRILSuppSvcNotification: function(aNotification) {
    switch (aNotification) {
      case RIL.GECKO_SUPP_SVC_NOTIFICATION_REMOTE_HELD:
        return nsITelephonyProvider.NOTIFICATION_REMOTE_HELD;
      case RIL.GECKO_SUPP_SVC_NOTIFICATION_REMOTE_RESUMED:
        return nsITelephonyProvider.NOTIFICATION_REMOTE_RESUMED;
      default:
        if (DEBUG) {
          debug("Unknown rilSuppSvcNotification: " + aNotification);
        }
        return;
    }
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

    this._getClient(aClientId).sendWorkerMessage("enumerateCalls", null,
                                                 (function(response) {
      if (!this._currentCalls[aClientId]) {
        this._currentCalls[aClientId] = {};
      }
      for (let call of response.calls) {
        call.clientId = aClientId;
        call.state = this._convertRILCallState(call.state);
        call.isActive = this._matchActiveSingleCall(call);
        call.isSwitchable = true;
        call.isMergeable = true;

        this._currentCalls[aClientId][call.callIndex] = call;
      }

      return false;
    }).bind(this));
  },

  /**
   * nsITelephonyProvider interface.
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
        aListener.enumerateCallState(call.clientId, call.callIndex,
                                     call.state, call.number,
                                     call.isActive, call.isOutgoing,
                                     call.isEmergency, call.isConference,
                                     call.isSwitchable, call.isMergeable);
      }
    }
    aListener.enumerateCallStateComplete();
  },

  isDialing: false,
  dial: function(aClientId, aNumber, aIsEmergency, aTelephonyCallback) {
    if (DEBUG) debug("Dialing " + (aIsEmergency ? "emergency " : "") + aNumber);

    if (this.isDialing) {
      if (DEBUG) debug("Already has a dialing call. Drop.");
      aTelephonyCallback.notifyDialError("InvalidStateError");
      return;
    }

    // we don't try to be too clever here, as the phone is probably in the
    // locked state. Let's just check if it's a number without normalizing
    if (!aIsEmergency) {
      aNumber = gPhoneNumberUtils.normalize(aNumber);
    }

    // Validate the number.
    if (!gPhoneNumberUtils.isPlainPhoneNumber(aNumber)) {
      // Note: isPlainPhoneNumber also accepts USSD and SS numbers
      if (DEBUG) debug("Number '" + aNumber + "' is not viable. Drop.");
      let errorMsg = RIL.RIL_CALL_FAILCAUSE_TO_GECKO_CALL_ERROR[RIL.CALL_FAIL_UNOBTAINABLE_NUMBER];
      aTelephonyCallback.notifyDialError(errorMsg);
      return;
    }

    function onCdmaDialSuccess() {
      let indexes = Object.keys(this._currentCalls[aClientId]);
      if (indexes.length != 1 ) {
        aTelephonyCallback.notifyDialSuccess();
        return;
      }

      // RIL doesn't hold the 2nd call. We create one by ourselves.
      let childCall = {
        callIndex: CDMA_SECOND_CALL_INDEX,
        state: RIL.CALL_STATE_DIALING,
        number: aNumber,
        isOutgoing: true,
        isEmergency: false,
        isConference: false,
        isSwitchable: false,
        isMergeable: true,
        parentId: indexes[0]
      };
      aTelephonyCallback.notifyDialSuccess();

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
    };

    this.isDialing = true;
    this._getClient(aClientId).sendWorkerMessage("dial", {
      number: aNumber,
      isDialEmergency: aIsEmergency
    }, (function(response) {
      this.isDialing = false;
      if (!response.success) {
        aTelephonyCallback.notifyDialError(response.errorMsg);
        return false;
      }

      if (response.isCdma) {
        onCdmaDialSuccess.call(this);
      } else {
        aTelephonyCallback.notifyDialSuccess();
      }
      return false;
    }).bind(this));
  },

  hangUp: function(aClientId, aCallIndex) {
    let parentId = this._currentCalls[aClientId][aCallIndex].parentId;
    if (parentId) {
      // Should release both, child and parent, together. Since RIL holds only
      // the parent call, we send 'parentId' to RIL.
      this.hangUp(aClientId, parentId);
    } else {
      this._getClient(aClientId).sendWorkerMessage("hangUp", { callIndex: aCallIndex });
    }
  },

  startTone: function(aClientId, aDtmfChar) {
    this._getClient(aClientId).sendWorkerMessage("startTone", { dtmfChar: aDtmfChar });
  },

  stopTone: function(aClientId) {
    this._getClient(aClientId).sendWorkerMessage("stopTone");
  },

  answerCall: function(aClientId, aCallIndex) {
    this._getClient(aClientId).sendWorkerMessage("answerCall", { callIndex: aCallIndex });
  },

  rejectCall: function(aClientId, aCallIndex) {
    this._getClient(aClientId).sendWorkerMessage("rejectCall", { callIndex: aCallIndex });
  },

  holdCall: function(aClientId, aCallIndex) {
    let call = this._currentCalls[aClientId][aCallIndex];
    if (!call || !call.isSwitchable) {
      // TODO: Bug 975949 - [B2G] Telephony should throw exceptions when some
      // operations aren't allowed instead of simply ignoring them.
      return;
    }

    this._getClient(aClientId).sendWorkerMessage("holdCall", { callIndex: aCallIndex });
  },

  resumeCall: function(aClientId, aCallIndex) {
    let call = this._currentCalls[aClientId][aCallIndex];
    if (!call || !call.isSwitchable) {
      // TODO: Bug 975949 - [B2G] Telephony should throw exceptions when some
      // operations aren't allowed instead of simply ignoring them.
      return;
    }

    this._getClient(aClientId).sendWorkerMessage("resumeCall", { callIndex: aCallIndex });
  },

  conferenceCall: function(aClientId) {
    let indexes = Object.keys(this._currentCalls[aClientId]);
    if (indexes.length < 2) {
      // TODO: Bug 975949 - [B2G] Telephony should throw exceptions when some
      // operations aren't allowed instead of simply ignoring them.
      return;
    }

    for (let i = 0; i < indexes.length; ++i) {
      let call = this._currentCalls[aClientId][indexes[i]];
      if (!call.isMergeable) {
        return;
      }
    }

    function onCdmaConferenceCallSuccess() {
      let indexes = Object.keys(this._currentCalls[aClientId]);
      if (indexes.length < 2) {
        return;
      }

      for (let i = 0; i < indexes.length; ++i) {
        let call = this._currentCalls[aClientId][indexes[i]];
        call.state = RIL.CALL_STATE_ACTIVE;
        call.isConference = true;
        this.notifyCallStateChanged(aClientId, call);
      }
      this.notifyConferenceCallStateChanged(RIL.CALL_STATE_ACTIVE);
    };

    this._getClient(aClientId).sendWorkerMessage("conferenceCall", null,
                                                 (function(response) {
      if (!response.success) {
        this._notifyAllListeners("notifyConferenceError", [response.errorName,
                                                           response.errorMsg]);
        return false;
      }

      if (response.isCdma) {
        onCdmaConferenceCallSuccess.call(this);
      }
      return false;
    }).bind(this));
  },

  separateCall: function(aClientId, aCallIndex) {
    let call = this._currentCalls[aClientId][aCallIndex];
    if (!call || !call.isConference) {
      // TODO: Bug 975949 - [B2G] Telephony should throw exceptions when some
      // operations aren't allowed instead of simply ignoring them.
      return;
    }

    let parentId = call.parentId;
    if (parentId) {
      this.separateCall(aClientId, parentId);
      return;
    }

    function onCdmaSeparateCallSuccess() {
      // See 3gpp2, S.R0006-522-A v1.0. Table 4, XID 6S.
      let call = this._currentCalls[aClientId][aCallIndex];
      if (!call || !call.isConference) {
        return;
      }

      let childId = call.childId;
      if (!childId) {
        return;
      }

      let childCall = this._currentCalls[aClientId][childId];
      this.notifyCallDisconnected(aClientId, childCall);
    };

    this._getClient(aClientId).sendWorkerMessage("separateCall", {
      callIndex: aCallIndex
    }, (function(response) {
      if (!response.success) {
        this._notifyAllListeners("notifyConferenceError", [response.errorName,
                                                           response.errorMsg]);
        return false;
      }

      if (response.isCdma) {
        onCdmaSeparateCallSuccess.call(this);
      }
      return false;
    }).bind(this));
  },

  holdConference: function(aClientId) {
    this._getClient(aClientId).sendWorkerMessage("holdConference");
  },

  resumeConference: function(aClientId) {
    this._getClient(aClientId).sendWorkerMessage("resumeConference");
  },

  get microphoneMuted() {
    return gAudioManager.microphoneMuted;
  },

  set microphoneMuted(aMuted) {
    if (aMuted == this.microphoneMuted) {
      return;
    }
    gAudioManager.microphoneMuted = aMuted;

    if (!this._activeCall) {
      gAudioManager.phoneState = nsIAudioManager.PHONE_STATE_NORMAL;
    }
  },

  get speakerEnabled() {
    let force = gAudioManager.getForceForUse(nsIAudioManager.USE_COMMUNICATION);
    return (force == nsIAudioManager.FORCE_SPEAKER);
  },

  set speakerEnabled(aEnabled) {
    if (aEnabled == this.speakerEnabled) {
      return;
    }
    let force = aEnabled ? nsIAudioManager.FORCE_SPEAKER :
                           nsIAudioManager.FORCE_NONE;
    gAudioManager.setForceForUse(nsIAudioManager.USE_COMMUNICATION, force);

    if (!this._activeCall) {
      gAudioManager.phoneState = nsIAudioManager.PHONE_STATE_NORMAL;
    }
  },

  /**
   * nsIGonkTelephonyProvider interface.
   */

  /**
   * Handle call disconnects by updating our current state and the audio system.
   */
  notifyCallDisconnected: function(aClientId, aCall) {
    if (DEBUG) debug("handleCallDisconnected: " + JSON.stringify(aCall));

    aCall.state = nsITelephonyProvider.CALL_STATE_DISCONNECTED;
    let duration = ("started" in aCall && typeof aCall.started == "number") ?
      new Date().getTime() - aCall.started : 0;
    let data = {
      number: aCall.number,
      duration: duration,
      direction: aCall.isOutgoing ? "outgoing" : "incoming"
    };
    gSystemMessenger.broadcastMessage("telephony-call-ended", data);

    aCall.clientId = aClientId;
    this._updateCallAudioState(aCall, null);

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
      this._notifyAllListeners("callStateChanged", [aClientId,
                                                    aCall.callIndex,
                                                    aCall.state,
                                                    aCall.number,
                                                    aCall.isActive,
                                                    aCall.isOutgoing,
                                                    aCall.isEmergency,
                                                    aCall.isConference,
                                                    aCall.isSwitchable,
                                                    aCall.isMergeable]);
    } else {
      this.notifyCallError(aClientId, aCall.callIndex, aCall.failCause);
    }
    delete this._currentCalls[aClientId][aCall.callIndex];

    if (manualConfStateChange) {
      this.notifyConferenceCallStateChanged(RIL.CALL_STATE_UNKNOWN);
    }
  },

  /**
   * Handle call error.
   */
  notifyCallError: function(aClientId, aCallIndex, aErrorMsg) {
    this._notifyAllListeners("notifyError", [aClientId, aCallIndex, aErrorMsg]);
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

    gSystemMessenger.broadcastMessage("telephony-new-call", {});
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

    if (aCall.state == nsITelephonyProvider.CALL_STATE_DIALING) {
      gSystemMessenger.broadcastMessage("telephony-new-call", {});
    }

    aCall.clientId = aClientId;
    this._updateCallAudioState(aCall, null);

    let call = this._currentCalls[aClientId][aCall.callIndex];
    if (call) {
      call.state = aCall.state;
      call.isConference = aCall.isConference;
      call.isEmergency = aCall.isEmergency;
      call.isActive = aCall.isActive;
      call.isSwitchable = aCall.isSwitchable != null ?
                          aCall.isSwitchable : call.isSwitchable;
      call.isMergeable = aCall.isMergeable != null ?
                         aCall.isMergeable : call.isMergeable;
    } else {
      call = aCall;
      call.isSwitchable = aCall.isSwitchable != null ?
                          aCall.isSwitchable : true;
      call.isMergeable = aCall.isMergeable != null ?
                         aCall.isMergeable : true;
      this._currentCalls[aClientId][aCall.callIndex] = call;
    }

    this._notifyAllListeners("callStateChanged", [aClientId,
                                                  call.callIndex,
                                                  call.state,
                                                  call.number,
                                                  call.isActive,
                                                  call.isOutgoing,
                                                  call.isEmergency,
                                                  call.isConference,
                                                  call.isSwitchable,
                                                  call.isMergeable]);
  },

  notifyCdmaCallWaiting: function(aClientId, aNumber) {
    // We need to acquire a CPU wake lock to avoid the system falling into
    // the sleep mode when the RIL handles the incoming call.
    this._acquireCallRingWakeLock();

    let call = this._currentCalls[aClientId][CDMA_SECOND_CALL_INDEX];
    if (call) {
      // TODO: Bug 977503 - B2G RIL: [CDMA] update callNumber when a waiting
      // call comes after a 3way call.
      this.notifyCallDisconnected(aClientId, call);
    }
    this._notifyAllListeners("notifyCdmaCallWaiting", [aClientId, aNumber]);
  },

  notifySupplementaryService: function(aClientId, aCallIndex, aNotification) {
    let notification = this._convertRILSuppSvcNotification(aNotification);
    this._notifyAllListeners("supplementaryServiceNotification",
                             [aClientId, aCallIndex, notification]);
  },

  notifyConferenceCallStateChanged: function(aState) {
    if (DEBUG) debug("handleConferenceCallStateChanged: " + aState);
    aState = this._convertRILCallState(aState);
    this._updateCallAudioState(null, aState);

    this._notifyAllListeners("conferenceCallStateChanged", [aState]);
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

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([TelephonyProvider]);
