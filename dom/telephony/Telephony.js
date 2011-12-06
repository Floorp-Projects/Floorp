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
      if (handerList.indexOf(handler) == -1) {
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

const DOM_RADIOSTATE_UNAVAILABLE = "unavailable";
const DOM_RADIOSTATE_OFF         = "off";
const DOM_RADIOSTATE_READY       = "ready";
   
const DOM_CARDSTATE_UNAVAILABLE    = "unavailable";
const DOM_CARDSTATE_ABSENT         = "absent";
const DOM_CARDSTATE_PIN_REQUIRED   = "pin_required";
const DOM_CARDSTATE_PUK_REQUIRED   = "puk_required";
const DOM_CARDSTATE_NETWORK_LOCKED = "network_locked";
const DOM_CARDSTATE_NOT_READY      = "not_ready";
const DOM_CARDSTATE_READY          = "ready";

/**
 * Callback object that Telephony registers with nsITelephone.
 * Telephony can't use itself because that might overload event handler
 * attributes ('onfoobar').
 */
function TelephonyRadioCallback(telephony) {
  this.telephony = telephony;
}
TelephonyRadioCallback.prototype = {

  QueryInterface: XPCOMUtils.generateQI([Ci.nsITelephoneCallback]),

  // nsITelephoneCallback

  onsignalstrengthchange: function onsignalstrengthchange(signalStrength) {
    this.telephony.signalStrength = signalStrength;
    this.telephony._dispatchEventByType("signalstrengthchange");
  },

  onoperatorchange: function onoperatorchange(operator) {
    this.telephony.operator = operator;
    this.telephony._dispatchEventByType("operatorchange");
  },

  onradiostatechange: function onradiostatechange(radioState) {
    this.telephony.radioState = radioState;
    this.telephony._dispatchEventByType("radiostatechange");
  },

  oncardstatechange: function oncardstatechange(cardState) {
    this.telephony.cardState = cardState;
    this.telephony._dispatchEventByType("cardstatechange");
  },

  oncallstatechange: function oncallstatechange(callState) {
    this.telephony._processCallState(callState);
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
    this.radioInterface = Cc["@mozilla.org/telephony/radio-interface;1"]
                            .createInstance(Ci.nsITelephone);
    this.radioCallback = new TelephonyRadioCallback(this);
    window.addEventListener("unload", function onunload(event) {
      this.radioInterface.unregisterCallback(this.radioCallback);
      this.radioCallback = null;
      this.window = null;
    }.bind(this));
    this.radioInterface.registerCallback(this.radioCallback);
    this.liveCalls = [];

    let initialState = this.radioInterface.initialState;
    this.operator        = initialState.operator;
    this.radioState      = initialState.radioState;
    this.cardState       = initialState.cardState;
    this.signalStrength  = initialState.signalStrength;
    this._processCallState(initialState.callState);
  },

  _dispatchEventByType: function _dispatchEventByType(type) {
    let event = this.window.document.createEvent("Event");
    event.initEvent(type, false, false);
    //event.isTrusted = true;
    this.dispatchEvent(event);
  },

  _processCallState: function _processCallState(callState) {
    //TODO
  },

  // mozIDOMTelephony

  liveCalls: null,

  dial: function dial(number) {
    this.radioInterface.dial(number);
    return new TelephonyCall(number, DOM_CALL_READYSTATE_DIALING);
  },

  // Additional stuff that's useful.

  signalStrength: null,
  operator: null,
  radioState: DOM_RADIOSTATE_UNAVAILABLE,
  cardState: DOM_CARDSTATE_UNAVAILABLE,

};
defineEventListenerSlot(Telephony.prototype, "radiostatechange");
defineEventListenerSlot(Telephony.prototype, "cardstatechange");
defineEventListenerSlot(Telephony.prototype, "signalstrengthchange");
defineEventListenerSlot(Telephony.prototype, "operatorchange");
defineEventListenerSlot(Telephony.prototype, "incoming");


const DOM_CALL_READYSTATE_DIALING   = "dialing";
const DOM_CALL_READYSTATE_DOM_CALLING   = "calling";
const DOM_CALL_READYSTATE_INCOMING  = "incoming";
const DOM_CALL_READYSTATE_CONNECTED = "connected";
const DOM_CALL_READYSTATE_CLOSED    = "closed";
const DOM_CALL_READYSTATE_BUSY      = "busy";

function TelephonyCall(number, initialState) {
  this.number = number;
  this.readyState = initialState;
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

  number: null,
  readyState: null,

  answer: function answer() {
    //TODO
  },

  disconnect: function disconnect() {
    //TODO
  },

};
defineEventListenerSlot(TelephonyCall.prototype, "connect");
defineEventListenerSlot(TelephonyCall.prototype, "disconnect");
defineEventListenerSlot(TelephonyCall.prototype, "busy");


const NSGetFactory = XPCOMUtils.generateNSGetFactory([Telephony]);
