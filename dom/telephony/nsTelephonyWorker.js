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

const DEBUG = true; // set to false to suppress debug messages

const TELEPHONYWORKER_CONTRACTID = "@mozilla.org/telephony/worker;1";
const TELEPHONYWORKER_CID        = Components.ID("{2d831c8d-6017-435b-a80c-e5d422810cea}");


function nsTelephonyWorker() {
  this.worker = new ChromeWorker("resource://gre/modules/ril_worker.js");
  this.worker.onerror = this.onerror.bind(this);
  this.worker.onmessage = this.onmessage.bind(this);

  this._callbacks = [];
  this.initialState = {};
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

  onmessage: function onmessage(event) {
    let message = event.data;
    debug("Received message: " + JSON.stringify(message));
    let value;
    switch (message.type) {
      case "signalstrengthchange":
        this.initialState.signalStrength = message.signalStrength;
        value = message.signalStrength;
        break;
      case "operatorchange":
        this.initialState.operator = message.operator;
        value = message.operator;
        break;
      case "onradiostatechange":
        this.initialState.radioState = message.radioState;
        value = message.radioState;
        break;
      case "cardstatechange":
        this.initialState.cardState = message.cardState;
        value = message.cardState;
        break;
      case "callstatechange":
        this.initialState.callState = message.callState;
        value = message.callState;
        break;
      default:
        // Got some message from the RIL worker that we don't know about.
    }
    this._callbacks.forEach(function (callback) {
      let method = callback[methodname];
      if (typeof method != "function") {
        return;
      }
      method.call(callback, value);
    });
  },

  // nsIRadioWorker

  worker: null,

  // nsITelephone

  initialState: null,

  dial: function dial(number) {
    debug("Dialing " + number);
    this.worker.postMessage({type: "dial", number: number});
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
