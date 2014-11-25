/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

let RSM = {};
Cu.import("resource://gre/modules/RILSystemMessenger.jsm", RSM);

const RILSYSTEMMESSENGERHELPER_CONTRACTID =
  "@mozilla.org/ril/system-messenger-helper;1";
const RILSYSTEMMESSENGERHELPER_CID =
  Components.ID("{19d9a4ea-580d-11e4-8f6c-37ababfaaea9}");

XPCOMUtils.defineLazyServiceGetter(this, "gSystemMessenger",
                                   "@mozilla.org/system-message-internal;1",
                                   "nsISystemMessagesInternal");

let DEBUG = false;
function debug(s) {
  dump("-@- RILSystemMessenger: " + s + "\n");
};

// Read debug setting from pref.
try {
  let debugPref = Services.prefs.getBoolPref("ril.debugging.enabled");
  DEBUG = DEBUG || debugPref;
} catch (e) {}

/**
 * RILSystemMessengerHelper
 */
function RILSystemMessengerHelper() {
  this.messenger = new RSM.RILSystemMessenger();
  this.messenger.broadcastMessage = (aType, aMessage) => {
    if (DEBUG) {
      debug("broadcastMessage: aType: " + aType +
            ", aMessage: "+ JSON.stringify(aMessage));
    }

    gSystemMessenger.broadcastMessage(aType, aMessage);
  };
}
RILSystemMessengerHelper.prototype = {

  classID: RILSYSTEMMESSENGERHELPER_CID,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsITelephonyMessenger]),

  /**
   * RILSystemMessenger instance.
   */
  messenger: null,

  /**
   * nsITelephonyMessenger API
   */
  notifyNewCall: function() {
    this.messenger.notifyNewCall();
  },

  notifyCallEnded: function(aServiceId, aNumber, aCdmaWaitingNumber, aEmergency,
                            aDuration, aOutgoing, aHangUpLocal) {
    this.messenger.notifyCallEnded(aServiceId, aNumber, aCdmaWaitingNumber, aEmergency,
                                   aDuration, aOutgoing, aHangUpLocal);
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([RILSystemMessengerHelper]);