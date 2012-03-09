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
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Turner <bent.mozilla@gmail.com> (Original Author)
 *   Philipp von Weitershausen <philipp@weitershausen.de>
 *   Sinker Li <thinker@codemud.net>
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

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

var RIL = {};
Cu.import("resource://gre/modules/ril_consts.js", RIL);

const DEBUG = false; // set to true to see debug messages

const RADIOINTERFACELAYER_CID =
  Components.ID("{2d831c8d-6017-435b-a80c-e5d422810cea}");
const DATACALLINFO_CID =
  Components.ID("{ef474cd9-94f7-4c05-a31b-29b9de8a10d2}");

const nsIAudioManager = Ci.nsIAudioManager;
const nsIRadioInterfaceLayer = Ci.nsIRadioInterfaceLayer;

const kSmsReceivedObserverTopic          = "sms-received";
const DOM_SMS_DELIVERY_RECEIVED          = "received";
const DOM_SMS_DELIVERY_SENT              = "sent";

XPCOMUtils.defineLazyServiceGetter(this, "gSmsService",
                                   "@mozilla.org/sms/smsservice;1",
                                   "nsISmsService");

XPCOMUtils.defineLazyServiceGetter(this, "gSmsRequestManager",
                                   "@mozilla.org/sms/smsrequestmanager;1",
                                   "nsISmsRequestManager");

XPCOMUtils.defineLazyServiceGetter(this, "gSmsDatabaseService",
                                   "@mozilla.org/sms/rilsmsdatabaseservice;1",
                                   "nsISmsDatabaseService");

function convertRILCallState(state) {
  switch (state) {
    case RIL.CALL_STATE_ACTIVE:
      return nsIRadioInterfaceLayer.CALL_STATE_CONNECTED;
    case RIL.CALL_STATE_HOLDING:
      return nsIRadioInterfaceLayer.CALL_STATE_HELD;
    case RIL.CALL_STATE_DIALING:
      return nsIRadioInterfaceLayer.CALL_STATE_DIALING;
    case RIL.CALL_STATE_ALERTING:
      return nsIRadioInterfaceLayer.CALL_STATE_RINGING;
    case RIL.CALL_STATE_INCOMING:
      return nsIRadioInterfaceLayer.CALL_STATE_INCOMING;
    case RIL.CALL_STATE_WAITING:
      return nsIRadioInterfaceLayer.CALL_STATE_HELD; // XXX This may not be right...
    default:
      throw new Error("Unknown rilCallState: " + state);
  }
}

/**
 * Fake nsIAudioManager implementation so that we can run the telephony
 * code in a non-Gonk build.
 */
let FakeAudioManager = {
  microphoneMuted: false,
  masterVolume: 1.0,
  masterMuted: false,
  phoneState: nsIAudioManager.PHONE_STATE_CURRENT,
  _forceForUse: {},
  setForceForUse: function setForceForUse(usage, force) {
    this._forceForUse[usage] = force;
  },
  getForceForUse: function setForceForUse(usage) {
    return this._forceForUse[usage] || nsIAudioManager.FORCE_NONE;
  }
};

XPCOMUtils.defineLazyGetter(this, "gAudioManager", function getAudioManager() {
  try {
    return Cc["@mozilla.org/telephony/audiomanager;1"]
             .getService(nsIAudioManager);
  } catch (ex) {
    //TODO on the phone this should not fall back as silently.
    debug("Using fake audio manager.");
    return FakeAudioManager;
  }
});


function DataCallInfo(state, cid, apn) {
  this.callState = state;
  this.cid = cid;
  this.apn = apn;
}
DataCallInfo.protoptype = {
  classID:      DATACALLINFO_CID,
  classInfo:    XPCOMUtils.generateCI({classID: DATACALLINFO_CID,
                                       classDescription: "DataCallInfo",
                                       interfaces: [Ci.nsIDataCallInfo]}),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDataCallInfo]),
};


function RadioInterfaceLayer() {
  this.worker = new ChromeWorker("resource://gre/modules/ril_worker.js");
  this.worker.onerror = this.onerror.bind(this);
  this.worker.onmessage = this.onmessage.bind(this);
  debug("Starting Worker\n");
  this.currentState = {
    signalStrength: null,
    operator:       null,
    radioState:     null,
    cardState:      null
  };
}
RadioInterfaceLayer.prototype = {

  classID:   RADIOINTERFACELAYER_CID,
  classInfo: XPCOMUtils.generateCI({classID: RADIOINTERFACELAYER_CID,
                                    classDescription: "RadioInterfaceLayer",
                                    interfaces: [Ci.nsIWorkerHolder,
                                                 Ci.nsIRadioInterfaceLayer]}),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWorkerHolder,
                                         Ci.nsIRadioInterfaceLayer]),

  onerror: function onerror(event) {
    debug("Got an error: " + event.filename + ":" +
          event.lineno + ": " + event.message + "\n");
    event.preventDefault();
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
    switch (message.type) {
      case "callStateChange":
        // This one will handle its own notifications.
        this.handleCallStateChange(message.call);
        break;
      case "callDisconnected":
        // This one will handle its own notifications.
        this.handleCallDisconnected(message.call);
        break;
      case "enumerateCalls":
        // This one will handle its own notifications.
        this.handleEnumerateCalls(message.calls);
        break;
      case "registrationstatechange":
        this.currentState.registrationState = message.registrationState;
        break;
      case "gprsregistrationstatechange":
        this.currentState.gprsRegistrationState = message.gprsRegistrationState;
        break;
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
      case "sms-received":
        this.handleSmsReceived(message);
        return;
      case "sms-sent":
        this.handleSmsSent(message);
        return;
      case "datacallstatechange":
        this.handleDataCallState(message.datacall);
        break;
      case "datacalllist":
        this.handleDataCallList(message);
        break;
      case "nitzTime":
        // TODO bug 714349
        // Send information to time manager to decide what to do with it
        // Message contains networkTimeInSeconds, networkTimeZoneInMinutes,
        // dstFlag,localTimeStampInMS
        // indicating the time, daylight savings flag, and timezone
        // sent from the network and a timestamp of when the message was received
        // so an offset can be added if/when the time is actually set.
        if (DEBUG) {
          debug("nitzTime networkTime=" + message.networkTimeInSeconds
               + " timezone=" + message.networkTimeZoneInMinutes
               + " dst=" + message.dstFlag
               + " timestamp=" + message.localTimeStampInMS);
        }
        break;
      default:
        throw new Error("Don't know about this message type: " + message.type);
    }
  },

  /**
   * Track the active call and update the audio system as its state changes.
   *
   * XXX Needs some more work to support hold/resume.
   */
  _activeCall: null,
  updateCallAudioState: function updateCallAudioState() {
    if (!this._activeCall) {
      // Disable audio.
      gAudioManager.phoneState = nsIAudioManager.PHONE_STATE_NORMAL;
      debug("No active call, put audio system into PHONE_STATE_NORMAL.");
      return;
    }
    switch (this._activeCall.state) {
      case nsIRadioInterfaceLayer.CALL_STATE_INCOMING:
        gAudioManager.phoneState = nsIAudioManager.PHONE_STATE_RINGTONE;
        debug("Incoming call, put audio system into PHONE_STATE_RINGTONE.");
        break;
      case nsIRadioInterfaceLayer.CALL_STATE_DIALING: // Fall through...
      case nsIRadioInterfaceLayer.CALL_STATE_CONNECTED:
        gAudioManager.phoneState = nsIAudioManager.PHONE_STATE_IN_CALL;
        gAudioManager.setForceForUse(nsIAudioManager.USE_COMMUNICATION,
                                     nsIAudioManager.FORCE_NONE);
        debug("Active call, put audio system into PHONE_STATE_IN_CALL.");
        break;
    }
  },

  /**
   * Handle call state changes by updating our current state and the audio
   * system.
   */
  handleCallStateChange: function handleCallStateChange(call) {
    debug("handleCallStateChange: " + JSON.stringify(call));
    call.state = convertRILCallState(call.state);
    if (call.state == nsIRadioInterfaceLayer.CALL_STATE_DIALING ||
        call.state == nsIRadioInterfaceLayer.CALL_STATE_RINGING ||
        call.state == nsIRadioInterfaceLayer.CALL_STATE_CONNECTED) {
      // This is now the active call.
      this._activeCall = call;
    }
    this.updateCallAudioState();
    this._deliverCallback("callStateChanged",
                          [call.callIndex, call.state, call.number]);
  },

  /**
   * Handle call disconnects by updating our current state and the audio system.
   */
  handleCallDisconnected: function handleCallDisconnected(call) {
    debug("handleCallDisconnected: " + JSON.stringify(call));
    if (this._activeCall && this._activeCall.callIndex == call.callIndex) {
      this._activeCall = null;
    }
    this.updateCallAudioState();
    this._deliverCallback("callStateChanged",
                          [call.callIndex,
                           nsIRadioInterfaceLayer.CALL_STATE_DISCONNECTED,
                           call.number]);
  },

  /**
   * Handle calls delivered in response to a 'enumerateCalls' request.
   */
  handleEnumerateCalls: function handleEnumerateCalls(calls) {
    debug("handleEnumerateCalls: " + JSON.stringify(calls));
    let callback = this._enumerationCallbacks.shift();
    let activeCallIndex = this._activeCall ? this._activeCall.callIndex : -1;
    for (let i in calls) {
      let call = calls[i];
      let state = convertRILCallState(call.state);
      let keepGoing;
      try {
        keepGoing =
          callback.enumerateCallState(call.callIndex, state, call.number,
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

  handleSmsReceived: function handleSmsReceived(message) {
    debug("handleSmsReceived: " + JSON.stringify(message));
    let id = gSmsDatabaseService.saveReceivedMessage(message.sender || null,
                                                     message.body || null,
                                                     message.timestamp);
    let sms = gSmsService.createSmsMessage(id,
                                           DOM_SMS_DELIVERY_RECEIVED,
                                           message.sender || null,
                                           message.receiver || null,
                                           message.body || null,
                                           message.timestamp);
    Services.obs.notifyObservers(sms, kSmsReceivedObserverTopic, null);
  },

  handleSmsSent: function handleSmsSent(message) {
    debug("handleSmsSent: " + JSON.stringify(message));
    let timestamp = Date.now();
    let id = gSmsDatabaseService.saveSentMessage(message.number, message.body, timestamp);
    let sms = gSmsService.createSmsMessage(id,
                                           DOM_SMS_DELIVERY_SENT,
                                           null,
                                           message.number,
                                           message.body,
                                           timestamp);
    //TODO handle errors (bug 727319)
    gSmsRequestManager.notifySmsSent(message.requestId, sms);
  },

  /**
   * Handle data call state changes.
   */
  handleDataCallState: function handleDataCallState(datacall) {
    this._deliverDataCallCallback("dataCallStateChanged",
                                  [datacall.cid, datacall.ifname, datacall.state]);
  },

  /**
   * Handle data call list.
   */
  handleDataCallList: function handleDataCallList(message) {
    let datacalls = [];
    for each (let datacall in message.datacalls) {
      datacalls.push(new DataCallInfo(datacall.state,
                                      datacall.cid,
                                      datacall.apn));
    }
    this._deliverDataCallCallback("receiveDataCallList",
                                  [datacalls, datacalls.length]);
  },

  // nsIRadioWorker

  worker: null,

  // nsIRadioInterfaceLayer

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

  answerCall: function answerCall(callIndex) {
    this.worker.postMessage({type: "answerCall", callIndex: callIndex});
  },

  rejectCall: function rejectCall(callIndex) {
    this.worker.postMessage({type: "rejectCall", callIndex: callIndex});
  },

  get microphoneMuted() {
    return gAudioManager.microphoneMuted;
  },
  set microphoneMuted(value) {
    if (value == this.microphoneMuted) {
      return;
    }
    gAudioManager.phoneState = value ?
      nsIAudioManager.PHONE_STATE_IN_COMMUNICATION :
      nsIAudioManager.PHONE_STATE_IN_CALL;  //XXX why is this needed?
    gAudioManager.microphoneMuted = value;
  },

  get speakerEnabled() {
    return (gAudioManager.getForceForUse(nsIAudioManager.USE_COMMUNICATION) ==
            nsIAudioManager.FORCE_SPEAKER);
  },
  set speakerEnabled(value) {
    if (value == this.speakerEnabled) {
      return;
    }
    gAudioManager.phoneState = nsIAudioManager.PHONE_STATE_IN_CALL; // XXX why is this needed?
    let force = value ? nsIAudioManager.FORCE_SPEAKER :
                        nsIAudioManager.FORCE_NONE;
    gAudioManager.setForceForUse(nsIAudioManager.USE_COMMUNICATION, force);
  },

  getNumberOfMessagesForText: function getNumberOfMessagesForText(text) {
    //TODO: this assumes 7bit encoding, which is incorrect. Need to look
    // for characters not supported by 7bit alphabets and then calculate
    // length in UCS2 encoding.
    return Math.ceil(text.length / 160);
  },

  sendSMS: function sendSMS(number, message, requestId, processId) {
    this.worker.postMessage({type: "sendSMS",
                             number: number,
                             body: message,
                             requestId: requestId,
                             processId: processId});
  },

  _callbacks: null,
  _enumerationCallbacks: null,

  registerCallback: function registerCallback(callback) {
    if (this._callbacks) {
      if (this._callbacks.indexOf(callback) != -1) {
        throw new Error("Already registered this callback!");
      }
    } else {
      this._callbacks = [];
    }
    this._callbacks.push(callback);
    debug("Registered callback: " + callback);
  },

  unregisterCallback: function unregisterCallback(callback) {
    if (!this._callbacks) {
      return;
    }
    let index = this._callbacks.indexOf(callback);
    if (index != -1) {
      this._callbacks.splice(index, 1);
      debug("Unregistered callback: " + callback);
    }
  },

  enumerateCalls: function enumerateCalls(callback) {
    debug("Requesting enumeration of calls for callback: " + callback);
    this.worker.postMessage({type: "enumerateCalls"});
    if (!this._enumerationCallbacks) {
      this._enumerationCallbacks = [];
    }
    this._enumerationCallbacks.push(callback);
  },

  _deliverCallback: function _deliverCallback(name, args) {
    // We need to worry about callback registration state mutations during the
    // callback firing. The behaviour we want is to *not* call any callbacks
    // that are added during the firing and to *not* call any callbacks that are
    // removed during the firing. To address this, we make a copy of the
    // callback list before dispatching and then double-check that each callback
    // is still registered before calling it.
    if (!this._callbacks) {
      return;
    }
    let callbacks = this._callbacks.slice();
    for each (let callback in callbacks) {
      if (this._callbacks.indexOf(callback) == -1) {
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

  registerDataCallCallback: function registerDataCallCallback(callback) {
    if (this._datacall_callbacks) {
      if (this._datacall_callbacks.indexOf(callback) != -1) {
        throw new Error("Already registered this callback!");
      }
    } else {
      this._datacall_callbacks = [];
    }
    this._datacall_callbacks.push(callback);
    debug("Registering callback: " + callback);
  },

  unregisterDataCallCallback: function unregisterDataCallCallback(callback) {
    if (!this._datacall_callbacks) {
      return;
    }
    let index = this._datacall_callbacks.indexOf(callback);
    if (index != -1) {
      this._datacall_callbacks.splice(index, 1);
      debug("Unregistering callback: " + callback);
    }
  },

  _deliverDataCallCallback: function _deliverDataCallCallback(name, args) {
    // We need to worry about callback registration state mutations during the
    // callback firing. The behaviour we want is to *not* call any callbacks
    // that are added during the firing and to *not* call any callbacks that are
    // removed during the firing. To address this, we make a copy of the
    // callback list before dispatching and then double-check that each callback
    // is still registered before calling it.
    if (!this._datacall_callbacks) {
      return;
    }
    let callbacks = this._datacall_callbacks.slice();
    for each (let callback in callbacks) {
      if (this._datacall_callbacks.indexOf(callback) == -1) {
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

  setupDataCall: function setupDataCall(radioTech, apn, user, passwd, chappap, pdptype) {
    this.worker.postMessage({type: "setupDataCall",
                             radioTech: radioTech,
                             apn: apn,
                             user: user,
                             passwd: passwd,
                             chappap: chappap,
                             pdptype: pdptype});
  },

  deactivateDataCall: function deactivateDataCall(cid, reason) {
    this.worker.postMessage({type: "deactivateDataCall",
                             cid: cid,
                             reason: reason});
  },

  getDataCallList: function getDataCallList() {
    this.worker.postMessage({type: "getDataCallList"});
  },

};

const NSGetFactory = XPCOMUtils.generateNSGetFactory([RadioInterfaceLayer]);

let debug;
if (DEBUG) {
  debug = function (s) {
    dump("-*- RadioInterfaceLayer: " + s + "\n");
  };
} else {
  debug = function (s) {};
}
