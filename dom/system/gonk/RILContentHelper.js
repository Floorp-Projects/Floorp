/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

var RIL = {};
Cu.import("resource://gre/modules/ril_consts.js", RIL);

const DEBUG = false; // set to true to see debug messages

const RILCONTENTHELPER_CID =
  Components.ID("{472816e1-1fd6-4405-996c-806f9ea68174}");
const MOBILECONNECTIONINFO_CID =
  Components.ID("{a35cfd39-2d93-4489-ac7d-396475dacb27}");

const RIL_IPC_MSG_NAMES = [
  "RIL:CardStateChanged",
  "RIL:VoiceInfoChanged",
  "RIL:DataInfoChanged",
  "RIL:EnumerateCalls",
  "RIL:CallStateChanged",
];

const kVoiceChangedTopic     = "mobile-connection-voice-changed";
const kDataChangedTopic      = "mobile-connection-data-changed";
const kCardStateChangedTopic = "mobile-connection-cardstate-changed";

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIFrameMessageManager");

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
  emergencyCallsOnly: false,
  roaming: false,
  operator: null,
  type: null,
  signalStrength: null,
  relSignalStrength: null
};


function RILContentHelper() {
  this.voiceConnectionInfo = new MobileConnectionInfo();
  this.dataConnectionInfo = new MobileConnectionInfo();

  for each (let msgname in RIL_IPC_MSG_NAMES) {
    cpmm.addMessageListener(msgname, this);
  }
  Services.obs.addObserver(this, "xpcom-shutdown", false);

  // Request initial state.
  let radioState = cpmm.QueryInterface(Ci.nsISyncMessageSender)
                       .sendSyncMessage("RIL:GetRadioState")[0];
  if (!radioState) {
    debug("Received null radioState from chrome process.");
    return;
  }
  this.cardState = radioState.cardState;
  for (let key in radioState.voice) {
    this.voiceConnectionInfo[key] = radioState.voice[key];
  }
  for (let key in radioState.data) {
    this.dataConnectionInfo[key] = radioState.data[key];
  }
}
RILContentHelper.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIMobileConnectionProvider,
                                         Ci.nsIRILContentHelper,
                                         Ci.nsIObserver]),
  classID:   RILCONTENTHELPER_CID,
  classInfo: XPCOMUtils.generateCI({classID: RILCONTENTHELPER_CID,
                                    classDescription: "RILContentHelper",
                                    interfaces: [Ci.nsIMobileConnectionProvider,
                                                 Ci.nsIRILContentHelper]}),

  // nsIRILContentHelper

  cardState:           RIL.GECKO_CARDSTATE_UNAVAILABLE,
  voiceConnectionInfo: null,
  dataConnectionInfo:  null,

  getNetworks: function getNetworks(window) {
    //TODO bug 744344
    throw Components.Exception("Not implemented", Cr.NS_ERROR_NOT_IMPLEMENTED);
  },

  _telephonyCallbacks: null,
  _enumerationTelephonyCallbacks: null,

  registerTelephonyCallback: function registerTelephonyCallback(callback) {
    if (this._telephonyCallbacks) {
      if (this._telephonyCallbacks.indexOf(callback) != -1) {
        throw new Error("Already registered this callback!");
      }
    } else {
      this._telephonyCallbacks = [];
    }
    this._telephonyCallbacks.push(callback);
    debug("Registered telephony callback: " + callback);
  },

  unregisterTelephonyCallback: function unregisterTelephonyCallback(callback) {
    if (!this._telephonyCallbacks) {
      return;
    }
    let index = this._telephonyCallbacks.indexOf(callback);
    if (index != -1) {
      this._telephonyCallbacks.splice(index, 1);
      debug("Unregistered telephony callback: " + callback);
    }
  },

  enumerateCalls: function enumerateCalls(callback) {
    debug("Requesting enumeration of calls for callback: " + callback);
    cpmm.sendAsyncMessage("RIL:EnumerateCalls");
    if (!this._enumerationTelephonyCallbacks) {
      this._enumerationTelephonyCallbacks = [];
    }
    this._enumerationTelephonyCallbacks.push(callback);
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
      for each (let msgname in RIL_IPC_MSG_NAMES) {
        cpmm.removeMessageListener(msgname, this);
      }
      Services.obs.removeObserver(this, "xpcom-shutdown");
      cpmm = null;
    }
  },

  // nsIFrameMessageListener

  receiveMessage: function receiveMessage(msg) {
    debug("Received message '" + msg.name + "': " + JSON.stringify(msg.json));
    switch (msg.name) {
      case "RIL:CardStateChanged":
        if (this.cardState != msg.json.cardState) {
          this.cardState = msg.json.cardState;
          Services.obs.notifyObservers(null, kCardStateChangedTopic, null);
        }
        break;
      case "RIL:VoiceInfoChanged":
        for (let key in msg.json) {
          this.voiceConnectionInfo[key] = msg.json[key];
        }
        Services.obs.notifyObservers(null, kVoiceChangedTopic, null);
        break;
      case "RIL:DataInfoChanged":
        for (let key in msg.json) {
          this.dataConnectionInfo[key] = msg.json[key];
        }
        Services.obs.notifyObservers(null, kDataChangedTopic, null);
        break;
      case "RIL:EnumerateCalls":
        this.handleEnumerateCalls(msg.json);
        break;
      case "RIL:CallStateChanged":
        this._deliverTelephonyCallback("callStateChanged",
                                       [msg.json.callIndex, msg.json.state,
                                        msg.json.number]);
    }
  },

  handleEnumerateCalls: function handleEnumerateCalls(message) {
    debug("handleEnumerateCalls: " + JSON.stringify(message));
    let callback = this._enumerationTelephonyCallbacks.shift();
    let calls = message.calls;
    let activeCallIndex = message.activeCallIndex;
    for (let i in calls) {
      let call = calls[i];
      let keepGoing;
      try {
        keepGoing =
          callback.enumerateCallState(call.callIndex, call.state, call.number,
                                      call.callIndex == activeCallIndex);
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

  _deliverTelephonyCallback: function _deliverTelephonyCallback(name, args) {
    if (!this._telephonyCallbacks) {
      return;
    }

    let callbacks = this._telephonyCallbacks.slice();
    for each (let callback in callbacks) {
      if (this._telephonyCallbacks.indexOf(callback) == -1) {
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

};

const NSGetFactory = XPCOMUtils.generateNSGetFactory([RILContentHelper]);

let debug;
if (DEBUG) {
  debug = function (s) {
    dump("-*- RILContentHelper: " + s + "\n");
  };
} else {
  debug = function (s) {};
}
