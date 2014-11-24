/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

let RIL = {};
Cu.import("resource://gre/modules/ril_consts.js", RIL);

const GONK_SMSSERVICE_CONTRACTID = "@mozilla.org/sms/gonksmsservice;1";
const GONK_SMSSERVICE_CID = Components.ID("{f9b9b5e2-73b4-11e4-83ff-a33e27428c86}");

const NS_XPCOM_SHUTDOWN_OBSERVER_ID      = "xpcom-shutdown";
const NS_PREFBRANCH_PREFCHANGE_TOPIC_ID  = "nsPref:changed";

const kPrefDefaultServiceId = "dom.sms.defaultServiceId";
const kPrefRilDebuggingEnabled = "ril.debugging.enabled";
const kPrefRilNumRadioInterfaces = "ril.numRadioInterfaces";

XPCOMUtils.defineLazyGetter(this, "gRadioInterfaces", function() {
  let ril = Cc["@mozilla.org/ril;1"].getService(Ci.nsIRadioInterfaceLayer);

  let interfaces = [];
  for (let i = 0; i < ril.numRadioInterfaces; i++) {
    interfaces.push(ril.getRadioInterface(i));
  }
  return interfaces;
});

let DEBUG = RIL.DEBUG_RIL;
function debug(s) {
  dump("SmsService: " + s);
}

function SmsService() {
  this._silentNumbers = [];
  this.smsDefaultServiceId = this._getDefaultServiceId();

  Services.prefs.addObserver(kPrefRilDebuggingEnabled, this, false);
  Services.prefs.addObserver(kPrefDefaultServiceId, this, false);
  Services.obs.addObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
}
SmsService.prototype = {
  classID: GONK_SMSSERVICE_CID,

  classInfo: XPCOMUtils.generateCI({classID: GONK_SMSSERVICE_CID,
                                    contractID: GONK_SMSSERVICE_CONTRACTID,
                                    classDescription: "SmsService",
                                    interfaces: [Ci.nsISmsService,
                                                 Ci.nsIGonkSmsService],
                                    flags: Ci.nsIClassInfo.SINGLETON}),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsISmsService,
                                         Ci.nsIGonkSmsService,
                                         Ci.nsIObserver]),

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

  /**
   * nsISmsService interface
   */
  smsDefaultServiceId: 0,

  getSegmentInfoForText: function(aText, aRequest) {
    gRadioInterfaces[0].getSegmentInfoForText(aText, aRequest);
  },

  send: function(aServiceId, aNumber, aMessage, aSilent, aRequest) {
    if (aServiceId > (gRadioInterfaces.length - 1)) {
      throw Cr.NS_ERROR_INVALID_ARG;
    }

    gRadioInterfaces[aServiceId].sendSMS(aNumber, aMessage, aSilent, aRequest);
  },

  // An array of slient numbers.
  _silentNumbers: null,
  isSilentNumber: function(aNumber) {
    return this._silentNumbers.indexOf(aNumber) >= 0;
  },

  addSilentNumber: function(aNumber) {
    if (this.isSilentNumber(aNumber)) {
      throw Cr.NS_ERROR_UNEXPECTED;
    }

    this._silentNumbers.push(aNumber);
  },

  removeSilentNumber: function(aNumber) {
   let index = this._silentNumbers.indexOf(aNumber);
   if (index < 0) {
     throw Cr.NS_ERROR_INVALID_ARG;
   }

   this._silentNumbers.splice(index, 1);
  },

  getSmscAddress: function(aServiceId, aRequest) {
    if (aServiceId > (gRadioInterfaces.length - 1)) {
      throw Cr.NS_ERROR_INVALID_ARG;
    }

    gRadioInterfaces[aServiceId].getSmscAddress(aRequest);
  },

  /**
   * TODO: nsIGonkSmsService interface
   */

  /**
   * nsIObserver interface.
   */
  observe: function(aSubject, aTopic, aData) {
    switch (aTopic) {
      case NS_PREFBRANCH_PREFCHANGE_TOPIC_ID:
        if (aData === kPrefRilDebuggingEnabled) {
          this._updateDebugFlag();
        }
        else if (aData === kPrefDefaultServiceId) {
          this.smsDefaultServiceId = this._getDefaultServiceId();
        }
        break;
      case NS_XPCOM_SHUTDOWN_OBSERVER_ID:
        Services.prefs.removeObserver(kPrefRilDebuggingEnabled, this);
        Services.prefs.removeObserver(kPrefDefaultServiceId, this);
        Services.obs.removeObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
        break;
    }
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([SmsService]);
