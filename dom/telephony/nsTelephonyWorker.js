/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Telephony.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Turner <bent.mozilla@gmail.com> (Original Author)
 *   Philipp von Weitershausen <philipp@weitershausen.de>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const DEBUG = true; // set to false to suppress debug messages

const TELEPHONYWORKER_CONTRACTID = "@mozilla.org/telephony/worker;1";
const TELEPHONYWORKER_CID        = Components.ID("{2d831c8d-6017-435b-a80c-e5d422810cea}");

const DOM_CALL_READYSTATE_DIALING        = "dialing";
const DOM_CALL_READYSTATE_RINGING        = "ringing";
const DOM_CALL_READYSTATE_BUSY           = "busy";
const DOM_CALL_READYSTATE_CONNECTING     = "connecting";
const DOM_CALL_READYSTATE_CONNECTED      = "connected";
const DOM_CALL_READYSTATE_DISCONNECTING  = "disconnecting";
const DOM_CALL_READYSTATE_DISCONNECTED   = "disconnected";
const DOM_CALL_READYSTATE_INCOMING       = "incoming";
const DOM_CALL_READYSTATE_HOLDING        = "holding";
const DOM_CALL_READYSTATE_HELD           = "held";

const kSmsReceivedObserverTopic          = "sms-received";
const DOM_SMS_DELIVERY_RECEIVED          = "received";

XPCOMUtils.defineLazyServiceGetter(this, "gSmsService",
                                   "@mozilla.org/sms/smsservice;1",
                                   "nsISmsService");

/**
 * Fake nsIAudioManager implementation so that we can run the telephony
 * code in a non-Gonk build.
 */
let FakeAudioManager = {
  microphoneMuted: false,
  masterVolume: 1.0,
  masterMuted: false,
  phoneState: Ci.nsIAudioManager.PHONE_STATE_CURRENT,
  _forceForUse: {},
  setForceForUse: function setForceForUse(usage, force) {
    this._forceForUse[usage] = force;
  },
  getForceForUse: function setForceForUse(usage) {
    return this._forceForUse[usage] || Ci.nsIAudioManager.FORCE_NONE;
  }
};

XPCOMUtils.defineLazyGetter(this, "gAudioManager", function getAudioManager() {
  try {
    return Cc["@mozilla.org/telephony/audiomanager;1"]
             .getService(Ci.nsIAudioManager);
  } catch (ex) {
    //TODO on the phone this should not fall back as silently.
    debug("Using fake audio manager.");
    return FakeAudioManager;
  }
});


function nsTelephonyWorker() {
  this.worker = new ChromeWorker("resource://gre/modules/ril_worker.js");
  this.worker.onerror = this.onerror.bind(this);
  this.worker.onmessage = this.onmessage.bind(this);

  this._callbacks = [];
  this.currentState = {
    signalStrength: null,
    operator:       null,
    radioState:     null,
    cardState:      null,
    currentCalls:   {}
  };
}
nsTelephonyWorker.prototype = {

  classID:   TELEPHONYWORKER_CID,
  classInfo: XPCOMUtils.generateCI({classID: TELEPHONYWORKER_CID,
                                    contractID: TELEPHONYWORKER_CONTRACTID,
                                    classDescription: "TelephonyWorker",
                                    interfaces: [Ci.nsIRadioWorker,
                                                 Ci.nsITelephone]}),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIRadioWorker,
                                         Ci.nsITelephone]),

  onerror: function onerror(event) {
    // It is very important to call preventDefault on the event here.
    // If an exception is thrown on the worker, it bubbles out to the
    // component that created it. If that component doesn't have an
    // onerror handler, the worker will try to call the error reporter
    // on the context it was created on. However, That doesn't work
    // for component contexts and can result in crashes. This onerror
    // handler has to make sure that it calls preventDefault on the
    // incoming event.
    event.preventDefault();

    debug("Got an error: " + event.filename + ":" +
          event.lineno + ": " + event.message + "\n");
  },

  /**
   * Process the incoming message from the RIL worker:
   * (1) Update the current state. This way any component that hasn't
   *     been listening for callbacks can easily catch up by looking at
   *     this.currentState.
   * (2) Update state in related systems such as the audio.
   * (3) Multiplex the message to telephone callbacks.
   */
  onmessage: function onmessage(event) {
    let message = event.data;
    debug("Received message: " + JSON.stringify(message));
    let value;
    switch (message.type) {
      case "signalstrengthchange":
        this.currentState.signalStrength = message.signalStrength;
        break;
      case "operatorchange":
        this.currentState.operator = message.operator;
        break;
      case "radiostatechange":
        this.currentState.radioState = message.radioState;
        break;
      case "cardstatechange":
        this.currentState.cardState = message.cardState;
        break;
      case "callstatechange":
        this.handleCallState(message);
        break;
      case "sms-received":
        this.handleSmsReceived(message);
        break;
      default:
        // Got some message from the RIL worker that we don't know about.
        return;
    }
    let methodname = "on" + message.type;
    this._callbacks.forEach(function (callback) {
      let method = callback[methodname];
      if (typeof method != "function") {
        return;
      }
      method.call(callback, message);
    });
  },

  /**
   * Handle call state changes by updating our current state and
   * the audio system.
   */
  handleCallState: function handleCallState(message) {
    let currentCalls = this.currentState.currentCalls;
    let oldState = currentCalls[message.callIndex];

    // Update current state.
    if (message.callState == DOM_CALL_READYSTATE_DISCONNECTED) {
      delete currentCalls[message.callIndex];
    } else {
      currentCalls[message.callIndex] = message;
    }

    // Update the audio system.
    //TODO this does not handle multiple concurrent calls yet.
    switch (message.callState) {
      case DOM_CALL_READYSTATE_DIALING:
        this.worker.postMessage({type: "setMute", mute: false});
        gAudioManager.phoneState = Ci.nsIAudioManager.PHONE_STATE_IN_CALL;
        gAudioManager.setForceForUse(Ci.nsIAudioManager.USE_COMMUNICATION,
                                     Ci.nsIAudioManager.FORCE_NONE);
        break;
      case DOM_CALL_READYSTATE_INCOMING:
        gAudioManager.phoneState = Ci.nsIAudioManager.PHONE_STATE_RINGTONE;
        break;
      case DOM_CALL_READYSTATE_CONNECTED:
        if (!oldState ||
            oldState.callState == DOM_CALL_READYSTATE_INCOMING ||
            oldState.callState == DOM_CALL_READYSTATE_CONNECTING) {
          // It's an incoming call, so tweak the audio now. If it was an
          // outgoing call, it would have been tweaked at dialing.
          this.worker.postMessage({type: "setMute", mute: false});
          gAudioManager.phoneState = Ci.nsIAudioManager.PHONE_STATE_IN_CALL;
          gAudioManager.setForceForUse(Ci.nsIAudioManager.USE_COMMUNICATION,
                                       Ci.nsIAudioManager.FORCE_NONE);
        }
        break;
      case DOM_CALL_READYSTATE_DISCONNECTED:
        this.worker.postMessage({type: "setMute", mute: true});
        gAudioManager.phoneState = Ci.nsIAudioManager.PHONE_STATE_NORMAL;
        break;
    }
  },

  handleSmsReceived: function handleSmsReceived(message) {
    //TODO: put the sms into a database, assign it a proper id, yada yada
    let sms = gSmsService.createSmsMessage(-1,
                                           DOM_SMS_DELIVERY_RECEIVED,
                                           message.sender || null,
                                           message.receiver || null,
                                           message.body || null,
                                           message.timestamp);
    Services.obs.notifyObservers(sms, kSmsReceivedObserverTopic, null);
  },

  // nsIRadioWorker

  worker: null,

  // nsITelephone

  currentState: null,

  dial: function dial(number) {
    debug("Dialing " + number);
    this.worker.postMessage({type: "dial", number: number});
  },

  hangUp: function hangUp(callIndex) {
    debug("Hanging up call no. " + callIndex);
    this.worker.postMessage({type: "hangUp", callIndex: callIndex});
  },
  
  startTone: function startTone(dtmfChar) {
    debug("Sending Tone for " + dtmfChar);
    this.worker.postMessage({type: "startTone", dtmfChar: dtmfChar});
  },

  stopTone: function stopTone() {
    debug("Stopping Tone");
    this.worker.postMessage({type: "stopTone"});
  },

  answerCall: function answerCall() {
    this.worker.postMessage({type: "answerCall"});
  },

  rejectCall: function rejectCall() {
    this.worker.postMessage({type: "rejectCall"});
  },

  get microphoneMuted() {
    return gAudioManager.microphoneMuted;
  },
  set microphoneMuted(value) {
    if (value == this.microphoneMuted) {
      return;
    }
    gAudioManager.phoneState = value ?
      Ci.nsIAudioManager.PHONE_STATE_IN_COMMUNICATION :
      Ci.nsIAudioManager.PHONE_STATE_IN_CALL;  //XXX why is this needed?
    gAudioManager.microphoneMuted = value;
  },

  get speakerEnabled() {
    return (gAudioManager.getForceForUse(Ci.nsIAudioManager.USE_COMMUNICATION)
            == Ci.nsIAudioManager.FORCE_SPEAKER);
  },
  set speakerEnabled(value) {
    if (value == this.speakerEnabled) {
      return;
    }
    gAudioManager.phoneState = Ci.nsIAudioManager.PHONE_STATE_IN_CALL; // XXX why is this needed?
    let force = value ? Ci.nsIAudioManager.FORCE_SPEAKER :
                        Ci.nsIAudioManager.FORCE_NONE;
    gAudioManager.setForceUse(Ci.nsIAudioManager.USE_COMMUNICATION, force);
  },

  getNumberOfMessagesForText: function getNumberOfMessagesForText(text) {
    //TODO: this assumes 7bit encoding, which is incorrect. Need to look
    // for characters not supported by 7bit alphabets and then calculate
    // length in UCS2 encoding.
    return Math.ceil(text.length / 160);
  },

  sendSMS: function sendSMS(number, message) {
    this.worker.postMessage({type: "sendSMS",
                             number: number,
                             body: message});
  },

  _callbacks: null,

  registerCallback: function registerCallback(callback) {
    this._callbacks.push(callback);
  },

  unregisterCallback: function unregisterCallback(callback) {
    let index = this._callbacks.indexOf(callback);
    if (index == -1) {
      throw "Callback not registered!";
    }
    this._callbacks.splice(index, 1);
  },

};

const NSGetFactory = XPCOMUtils.generateNSGetFactory([nsTelephonyWorker]);

let debug;
if (DEBUG) {
  debug = function (s) {
    dump("-*- TelephonyWorker component: " + s + "\n");
  };
} else {
  debug = function (s) {};
}
