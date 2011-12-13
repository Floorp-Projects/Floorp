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
 *  Philipp von Weitershausen <philipp@weitershausen.de>
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

const TELEPHONY_CID = Components.ID("{37e248d2-02ff-469b-bb31-eef5a4a4bee3}");
const TELEPHONY_CONTRACTID = "@mozilla.org/telephony;1";

const TELEPHONY_CALL_CID = Components.ID("{6b9b3daf-e5ea-460b-89a5-641ee20dd577}");
const TELEPHONY_CALL_CONTRACTID = "@mozilla.org/telephony-call;1";


const DOM_RADIOSTATE_UNAVAILABLE   = "unavailable";
const DOM_RADIOSTATE_OFF           = "off";
const DOM_RADIOSTATE_READY         = "ready";

const DOM_CARDSTATE_UNAVAILABLE    = "unavailable";
const DOM_CARDSTATE_ABSENT         = "absent";
const DOM_CARDSTATE_PIN_REQUIRED   = "pin_required";
const DOM_CARDSTATE_PUK_REQUIRED   = "puk_required";
const DOM_CARDSTATE_NETWORK_LOCKED = "network_locked";
const DOM_CARDSTATE_NOT_READY      = "not_ready";
const DOM_CARDSTATE_READY          = "ready";

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

const CALLINDEX_TEMPORARY_DIALING = -1;

/**
 * Define an event listener slot on an object, e.g.
 * 
 *   obj.onerror = function () {...}
 * 
 * will register the function as an event handler for the "error" event
 * if the "error" slot was defined on 'obj' or its prototype.
 */
function defineEventListenerSlot(object, event_type) {
  let property_name = "on" + event_type;
  let hidden_name = "_on" + event_type;
  let bound_name = "_bound_on" + event_type;
  object.__defineGetter__(property_name, function getter() {
    return this[hidden_name];
  });
  object.__defineSetter__(property_name, function setter(handler) {
    let old_handler = this[bound_name];
    if (old_handler) {
      this.removeEventListener(event_type, old_handler);
    }
    // Bind the handler to the object so that its 'this' is correct.
    let bound_handler = handler.bind(this);
    this.addEventListener(event_type, bound_handler);
    this[hidden_name] = handler;
    this[bound_name] = bound_handler;
  });
}


/**
 * Base object for event targets.
 */
function EventTarget() {}
EventTarget.prototype = {

  addEventListener: function addEventListener(type, handler) {
    //TODO verify that handler is an nsIDOMEventListener (or function)
    if (!this._listeners) {
      this._listeners = {};
    }
    if (!this._listeners[type]) {
      this._listeners[type] = [];
    }
    if (this._listeners[type].indexOf(handler) != -1) {
      // The handler is already registered. Ignore.
      return;
    }
    this._listeners[type].push(handler);
  },

  removeEventListener: function removeEventListener(type, handler) {
     let list, index;
     if (this._listeners &&
         (list = this._listeners[type]) &&
         (index = list.indexOf(handler) != -1)) {
       list.splice(index, 1);
       return;
     }
  },

  dispatchEvent: function dispatchEvent(event) {
    //TODO this does not deal with bubbling, defaultPrevented, canceling, etc.
    //TODO disallow re-dispatch of the same event if it's already being
    // dispatched (recursion).
    if (!this._listeners) {
      return;
    }
    let handlerList = this._listeners[event.type];
    if (!handlerList) {
      return;
    }
    event.target = this;

    // We need to worry about event handler mutations during the event firing.
    // The correct behaviour is to *not* call any listeners that are added
    // during the firing and to *not* call any listeners that are removed
    // during the firing. To address this, we make a copy of the listener list
    // before dispatching and then double-check that each handler is still
    // registered before firing it.
    let handlers = handlerList.slice();
    handlers.forEach(function (handler) {
      if (handlerList.indexOf(handler) == -1) {
        return;
      }
      switch (typeof handler) {
        case "function":
          handler(event);
          break;
        case "object":
          handler.handleEvent(event);
          break;
      }
    });
  }
};

/**
 * Callback object that Telephony registers with nsITelephone.
 * Telephony can't use itself because that might overload event handler
 * attributes ('onfoobar').
 */
function TelephoneCallback(telephony) {
  this.telephony = telephony;
}
TelephoneCallback.prototype = {

  QueryInterface: XPCOMUtils.generateQI([Ci.nsITelephoneCallback]),

  // nsITelephoneCallback

  onsignalstrengthchange: function onsignalstrengthchange(event) {
    this.telephony.signalStrength = event.signalStrength;
    this.telephony._dispatchEventByType("signalstrengthchange");
  },

  onoperatorchange: function onoperatorchange(event) {
    this.telephony.operator = event.operator;
    this.telephony._dispatchEventByType("operatorchange");
  },

  onradiostatechange: function onradiostatechange(event) {
    this.telephony.radioState = event.radioState;
    this.telephony._dispatchEventByType("radiostatechange");
  },

  oncardstatechange: function oncardstatechange(event) {
    this.telephony.cardState = event.cardState;
    this.telephony._dispatchEventByType("cardstatechange");
  },

  oncallstatechange: function oncallstatechange(event) {
    this.telephony._processCallState(event);
  },

};

/**
 * The navigator.mozTelephony object.
 */
function Telephony() {}
Telephony.prototype = {

  __proto__: EventTarget.prototype,

  classID: TELEPHONY_CID,
  classInfo: XPCOMUtils.generateCI({classID: TELEPHONY_CID,
                                    contractID: TELEPHONY_CONTRACTID,
                                    interfaces: [Ci.mozIDOMTelephony,
                                                 Ci.nsIDOMEventTarget],
                                    flags: Ci.nsIClassInfo.DOM_OBJECT,
                                    classDescription: "Telephony"}),
  QueryInterface: XPCOMUtils.generateQI([Ci.mozIDOMTelephony,
                                         Ci.nsIDOMEventTarget,
                                         Ci.nsIDOMGlobalPropertyInitializer]),

  // nsIDOMGlobalPropertyInitializer

  init: function init(window) {
    this.window = window;
    this.telephone = Cc["@mozilla.org/telephony/radio-interface;1"]
                       .createInstance(Ci.nsITelephone);
    this.telephoneCallback = new TelephoneCallback(this);
    //TODO switch to method suggested by bz in bug 707507
    window.addEventListener("unload", function onunload(event) {
      this.telephone.unregisterCallback(this.telephoneCallback);
      this.telephoneCallback = null;
      this.window = null;
    }.bind(this));
    this.telephone.registerCallback(this.telephoneCallback);
    this.callsByIndex = {};
    this.liveCalls = [];

    // Populate existing state.
    let currentState = this.telephone.currentState;
    let states = currentState.currentCalls;
    for (let i = 0; i < states.length; i++) {
      let state = states[i];
      let call = new TelephonyCall(this.telephone, state.callIndex);
      call.readyState = state.callState;
      call.number = state.number;
      this.liveCalls.push(call);
      this.callsByIndex[state.callIndex] = call;
    }

    this.operator        = currentState.operator;
    this.radioState      = currentState.radioState;
    this.cardState       = currentState.cardState;
    this.signalStrength  = currentState.signalStrength;
  },

  _dispatchEventByType: function _dispatchEventByType(type) {
    let event = this.window.document.createEvent("Event");
    event.initEvent(type, false, false);
    //event.isTrusted = true;
    this.dispatchEvent(event);
  },

  _dispatchCallEvent: function _dispatchCallEvent(call, type, target) {
    let event = this.window.document.createEvent("Event");
    event.initEvent(type, false, false);
    event.call = call; //XXX this is probably not going to work
    //event.isTrusted = true;
    target = target || call;
    target.dispatchEvent(event);    
  },

  _processCallState: function _processCallState(state) {
    // If the call is dialing, chances are good that we just kicked that off
    // so there's a call object without a callIndex. Let's fix that.
    if (state.callState == DOM_CALL_READYSTATE_DIALING) {
      let call = this.callsByIndex[CALLINDEX_TEMPORARY_DIALING];
      if (call) {
        call.callIndex = state.callIndex;
        delete this.callsByIndex[CALLINDEX_TEMPORARY_DIALING];
        this.callsByIndex[call.callIndex] = call;
        // Nothing else to do, since the initial call state will already be
        // DOM_CALL_READYSTATE_DIALING, so there's no event to dispatch.
        return;
      }
    }

    // If there is an existing call object, update state and dispatch event
    // on it.
    let call = this.callsByIndex[state.callIndex];
    if (call) {
      if (call.readyState == state.callState) {
        // No change in ready state, don't dispatch an event.
        return;
      }
      if (state.readyState == DOM_CALL_READYSTATE_DISCONNECTED) {
        let index = this.liveCalls.indexOf(call);
        if (index != -1) {
          this.liveCalls.splice(index, 1);
        }
        delete this.callsByIndex[call.callIndex];
      }
      call.readyState = state.callState;
      this._dispatchCallEvent(call, "readystatechange");
      this._dispatchCallEvent(call, state.callState);
      return;
    }

    // There's no call object yet, so let's create a new one, except when
    // the state notified means that the call is over.
    if (state.readyState == DOM_CALL_READYSTATE_DISCONNECTED) {
      return;
    }
    call = new TelephonyCall(this.telephone, state.callIndex);
    call.number = state.number;
    call.readyState = state.callState;
    this.callsByIndex[state.callIndex] = call;
    this.liveCalls.push(call);

    let target;
    if (call.readyState == DOM_CALL_READYSTATE_INCOMING) {
      target = this;
    } else {
      target = call;
      this._dispatchCallEvent(call, "readystatechange");
    }
    this._dispatchCallEvent(call, state.callState, target);
  },

  callsByIndex: null,

  // mozIDOMTelephony

  liveCalls: null,

  dial: function dial(number) {
    this.telephone.dial(number);

    // We don't know ahead of time what callIndex the call is going to have
    // so let's assign a temp value for now and sort it out on the first
    // 'callstatechange' event.
    //TODO ensure there isn't already an outgoing call
    let callIndex = CALLINDEX_TEMPORARY_DIALING;
    let call = new TelephonyCall(this.telephone, callIndex);
    call.readyState = DOM_CALL_READYSTATE_DIALING;
    call.number = number;
    this.callsByIndex[callIndex] = call;
    this.liveCalls.push(call);
    return call;
  },

  get muted() {
    return this.telephone.microphoneMuted;
  },
  set muted(value) {
    this.telephone.microphoneMuted = value;
  },

  get speakerOn() {
    return this.telephone.speakerEnabled;
  },
  set speakerOn(value) {
    this.telephone.speakerEnabled = value;
  },

  // Additional stuff that's useful.

  signalStrength: null,
  operator: null,
  radioState: DOM_RADIOSTATE_UNAVAILABLE,
  cardState: DOM_CARDSTATE_UNAVAILABLE,

};
defineEventListenerSlot(Telephony.prototype, DOM_CALL_READYSTATE_INCOMING);
//XXX philikon's additions
defineEventListenerSlot(Telephony.prototype, "radiostatechange");
defineEventListenerSlot(Telephony.prototype, "cardstatechange");
defineEventListenerSlot(Telephony.prototype, "signalstrengthchange");
defineEventListenerSlot(Telephony.prototype, "operatorchange");


function TelephonyCall(telephone, callIndex) {
  this.telephone = telephone;
  this.callIndex = callIndex;
}
TelephonyCall.prototype = {

  __proto__: EventTarget.prototype,

  classID: TELEPHONY_CALL_CID,
  classInfo: XPCOMUtils.generateCI({classID: TELEPHONY_CALL_CID,
                                    contractID: TELEPHONY_CALL_CONTRACTID,
                                    interfaces: [Ci.mozIDOMTelephonyCall,
                                                 Ci.nsIDOMEventTarget],
                                    flags: Ci.nsIClassInfo.DOM_OBJECT,
                                    classDescription: "TelephonyCall"}),
  QueryInterface: XPCOMUtils.generateQI([Ci.mozIDOMTelephonyCall,
                                         Ci.nsIDOMEventTarget]),


  callIndex: null,

  // mozIDOMTelephonyCall

  number: null,
  readyState: null,

  answer: function answer() {
    if (this.readyState != DOM_CALL_READYSTATE_INCOMING) {
      throw "Can only answer an incoming call!";
    }
    this.telephone.answerCall();
  },

  disconnect: function disconnect() {
    if (this.readyState == DOM_CALL_READYSTATE_INCOMING) {
      this.telephone.rejectCall();
    } else {
      this.telephone.hangUp(this.callIndex);
    }
  },

};
defineEventListenerSlot(TelephonyCall.prototype, "readystatechange");
defineEventListenerSlot(TelephonyCall.prototype, DOM_CALL_READYSTATE_RINGING);
defineEventListenerSlot(TelephonyCall.prototype, DOM_CALL_READYSTATE_BUSY);
defineEventListenerSlot(TelephonyCall.prototype, DOM_CALL_READYSTATE_CONNECTING);
defineEventListenerSlot(TelephonyCall.prototype, DOM_CALL_READYSTATE_CONNECTED);
defineEventListenerSlot(TelephonyCall.prototype, DOM_CALL_READYSTATE_DISCONNECTING);
defineEventListenerSlot(TelephonyCall.prototype, DOM_CALL_READYSTATE_DISCONNECTED);


const NSGetFactory = XPCOMUtils.generateNSGetFactory([Telephony]);
