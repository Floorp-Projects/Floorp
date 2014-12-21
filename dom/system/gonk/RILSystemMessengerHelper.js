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
  QueryInterface: XPCOMUtils.generateQI([Ci.nsITelephonyMessenger,
                                         Ci.nsISmsMessenger,
                                         Ci.nsICellbroadcastMessenger,
                                         Ci.nsIMobileConnectionMessenger,
                                         Ci.nsIIccMessenger]),

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
  },

  notifyUssdReceived: function(aServiceId, aMessage, aSessionEnded) {
    this.messenger.notifyUssdReceived(aServiceId, aMessage, aSessionEnded);
  },

  /**
   * nsISmsMessenger API
   */
  notifySms: function(aNotificationType, aId, aThreadId, aIccId, aDelivery,
                      aDeliveryStatus, aSender, aReceiver, aBody, aMessageClass,
                      aTimestamp, aSentTimestamp, aDeliveryTimestamp, aRead) {
    this.messenger.notifySms(aNotificationType, aId, aThreadId, aIccId, aDelivery,
                             aDeliveryStatus, aSender, aReceiver, aBody, aMessageClass,
                             aTimestamp, aSentTimestamp, aDeliveryTimestamp, aRead);
  },

  /**
   * nsICellbroadcastMessenger API
   */
  notifyCbMessageReceived: function(aServiceId, aGsmGeographicalScope, aMessageCode,
                                    aMessageId, aLanguage, aBody, aMessageClass,
                                    aTimestamp, aCdmaServiceCategory, aHasEtwsInfo,
                                    aEtwsWarningType, aEtwsEmergencyUserAlert, aEtwsPopup) {
    this.messenger.notifyCbMessageReceived(aServiceId, aGsmGeographicalScope, aMessageCode,
                                           aMessageId, aLanguage, aBody, aMessageClass,
                                           aTimestamp, aCdmaServiceCategory, aHasEtwsInfo,
                                           aEtwsWarningType, aEtwsEmergencyUserAlert, aEtwsPopup);
  },

  /**
   * nsIMobileConnectionMessenger API
   */
  notifyCdmaInfoRecDisplay: function(aServiceId, aDisplay) {
    this.messenger.notifyCdmaInfoRecDisplay(aServiceId, aDisplay);
  },

  notifyCdmaInfoRecCalledPartyNumber: function(aServiceId, aType, aPlan,
                                               aNumber, aPi, aSi) {
    this.messenger.notifyCdmaInfoRecCalledPartyNumber(aServiceId, aType, aPlan,
                                                      aNumber, aPi, aSi);
  },

  notifyCdmaInfoRecCallingPartyNumber: function(aServiceId, aType, aPlan,
                                                aNumber, aPi, aSi) {
    this.messenger.notifyCdmaInfoRecCallingPartyNumber(aServiceId, aType, aPlan,
                                                       aNumber, aPi, aSi);
  },

  notifyCdmaInfoRecConnectedPartyNumber: function(aServiceId, aType, aPlan,
                                                  aNumber, aPi, aSi) {
    this.messenger.notifyCdmaInfoRecConnectedPartyNumber(aServiceId, aType, aPlan,
                                                         aNumber, aPi, aSi);
  },

  notifyCdmaInfoRecSignal: function(aServiceId, aType, aAlertPitch, aSignal) {
    this.messenger.notifyCdmaInfoRecSignal(aServiceId, aType, aAlertPitch, aSignal);
  },

  notifyCdmaInfoRecRedirectingNumber: function(aServiceId, aType, aPlan,
                                               aNumber, aPi, aSi, aReason) {
    this.messenger.notifyCdmaInfoRecRedirectingNumber(aServiceId, aType, aPlan,
                                                      aNumber, aPi, aSi, aReason);
  },

  notifyCdmaInfoRecLineControl: function(aServiceId, aPolarityIncluded,
                                         aToggle, aReverse, aPowerDenial) {
    this.messenger.notifyCdmaInfoRecLineControl(aServiceId, aPolarityIncluded,
                                                aToggle, aReverse, aPowerDenial);
  },

  notifyCdmaInfoRecClir: function(aServiceId, aCause) {
    this.messenger.notifyCdmaInfoRecClir(aServiceId, aCause);
  },

  notifyCdmaInfoRecAudioControl: function(aServiceId, aUpLink, aDownLink) {
    this.messenger.notifyCdmaInfoRecAudioControl(aServiceId, aUpLink, aDownLink);
  },

  /**
   * nsIIccMessenger API
   */
  notifyStkProactiveCommand: function(aIccId, aCommand) {
    this.messenger.notifyStkProactiveCommand(aIccId, aCommand);
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([RILSystemMessengerHelper]);
