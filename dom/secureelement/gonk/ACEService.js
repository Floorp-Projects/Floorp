/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Copyright © 2015, Deutsche Telekom, Inc. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "SEUtils",
                                  "resource://gre/modules/SEUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "SE", function() {
  let obj = {};
  Cu.import("resource://gre/modules/se_consts.js", obj);
  return obj;
});

var DEBUG = SE.DEBUG_ACE;
function debug(msg) {
  if (DEBUG) {
    dump("ACEservice: " + msg + "\n");
  }
}

/**
 * Implements decision making algorithm as described in GPD specification,
 * mostly in 3.1, 3.2 and 4.2.3.
 *
 * TODO: Bug 1137533: Implement GPAccessRulesManager APDU filters
 */
function GPAccessDecision(rules, certHash, aid) {
  this.rules = rules;
  this.certHash = certHash;
  this.aid = aid;
}

GPAccessDecision.prototype = {
  isAccessAllowed: function isAccessAllowed() {
    // GPD SE Access Control v1.1, 3.4.1, Table 3-2: (Conflict resolution)
    // If a specific rule allows, all other non-specific access is denied.
    // Conflicting specific rules will resolve to the first Allowed == "true"
    // match. Given no specific rule, the global "All" rules will determine
    // access. "Some", skips further processing if access Allowed == "true".
    //
    // Access must be decided before the SE connector openChannel, and the
    // exchangeAPDU call.
    //
    // NOTE: This implementation may change with the introduction of APDU
    //       filters.
    let decision = this.rules.some(this._decideAppAccess.bind(this));
    return decision;
  },

  _decideAppAccess: function _decideAppAccess(rule) {
    let appMatched, appletMatched;

    // GPD SE AC 4.2.3: Algorithm for Applying Rules
    // Specific rule overrides global rule.
    //
    // DeviceAppID is the application hash, and the AID is SE Applet ID:
    //
    // GPD SE AC 4.2.3 A:
    //   SearchRuleFor(DeviceAppID, AID)
    // GPD SE AC 4.2.3 B: If no rule fits A:
    //   SearchRuleFor(<AllDeviceApplications>, AID)
    // GPD SE AC 4.2.3 C: If no rule fits A or B:
    //   SearchRuleFor(DeviceAppID, <AllSEApplications>)
    // GPD SE AC 4.2.3 D: If no rule fits A, B, or C:
    //   SearchRuleFor(<AllDeviceApplications>, <AllSEApplications>)

    // Device App
    appMatched = Array.isArray(rule.application) ?
      // GPD SE AC 4.2.3 A and 4.2.3 C (DeviceAppID rule)
      this._appCertHashMatches(rule.application) :
      // GPD SE AC 4.2.3 B and 4.2.3 D (All Device Applications)
      rule.application === Ci.nsIAccessRulesManager.ALLOW_ALL;

    if (!appMatched) {
      return false; // bail out early.
    }

    // SE Applet
    appletMatched = Array.isArray(rule.applet) ?
      // GPD SE AC 4.2.3 A and 4.2.3 B (AID rule)
      SEUtils.arraysEqual(rule.applet, this.aid) :
      // GPD SE AC 4.2.3 C and 4.2.3 D (All AID)
      rule.applet === Ci.nsIAccessRulesManager.ALL_APPLET;

    return appletMatched;
  },

  _appCertHashMatches: function _appCertHashMatches(hashArray) {
    if (!Array.isArray(hashArray)) {
      return false;
    }

    return !!(hashArray.find((hash) => {
      return SEUtils.arraysEqual(hash, this.certHash);
    }));
  }
};

function ACEService() {
  this._rulesManagers = new Map();

  this._rulesManagers.set(
    SE.TYPE_UICC,
    Cc["@mozilla.org/secureelement/access-control/rules-manager;1"]
      .createInstance(Ci.nsIAccessRulesManager));
}

ACEService.prototype = {
  _rulesManagers: null,

  isAccessAllowed: function isAccessAllowed(localId, seType, aid) {
    if(!Services.prefs.getBoolPref("devtools.debugger.forbid-certified-apps")) {
      debug("Certified apps debug enabled, allowing access");
      return Promise.resolve(true);
    }

    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
  },

  _getDevCertHashForApp: function getDevCertHashForApp(manifestURL) {
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
  },

  classID: Components.ID("{882a7463-2ca7-4d61-a89a-10eb6fd70478}"),
  contractID: "@mozilla.org/secureelement/access-control/ace;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAccessControlEnforcer])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([ACEService]);

