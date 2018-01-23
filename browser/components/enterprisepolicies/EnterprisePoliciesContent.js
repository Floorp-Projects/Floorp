/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const PREF_LOGLEVEL           = "browser.policies.loglevel";

XPCOMUtils.defineLazyGetter(this, "log", () => {
  let { ConsoleAPI } = Cu.import("resource://gre/modules/Console.jsm", {});
  return new ConsoleAPI({
    prefix: "Enterprise Policies Child",
    // tip: set maxLogLevel to "debug" and use log.debug() to create detailed
    // messages during development. See LOG_LEVELS in Console.jsm for details.
    maxLogLevel: "error",
    maxLogLevelPref: PREF_LOGLEVEL,
  });
});


// ==== Start XPCOM Boilerplate ==== \\

// Factory object
const EnterprisePoliciesFactory = {
  _instance: null,
  createInstance: function BGSF_createInstance(outer, iid) {
    if (outer != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return this._instance == null ?
      this._instance = new EnterprisePoliciesManagerContent() : this._instance;
  }
};

// ==== End XPCOM Boilerplate ==== //


function EnterprisePoliciesManagerContent() {
  let policies = Services.cpmm.initialProcessData.policies;
  if (policies) {
    this._status = policies.status;
    // make a copy of the array so that we can keep adding to it
    // in a way that is not confusing.
    this._disallowedFeatures = policies.disallowedFeatures.slice();
  }

  Services.cpmm.addMessageListener("EnterprisePolicies:DisallowFeature", this);
  Services.cpmm.addMessageListener("EnterprisePolicies:Restart", this);
}

EnterprisePoliciesManagerContent.prototype = {
  // for XPCOM
  classID:          Components.ID("{dc6358f8-d167-4566-bf5b-4350b5e6a7a2}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIMessageListener,
                                         Ci.nsIEnterprisePolicies]),

  // redefine the default factory for XPCOMUtils
  _xpcom_factory: EnterprisePoliciesFactory,

  _status: Ci.nsIEnterprisePolicies.INACTIVE,

  _disallowedFeatures: [],

  receiveMessage({name, data}) {
    switch (name) {
      case "EnterprisePolicies:DisallowFeature":
        this._disallowedFeatures.push(data.feature);
        break;

      case "EnterprisePolicies:Restart":
        this._disallowedFeatures = [];
        break;
    }
  },

  get status() {
    return this._status;
  },

  isAllowed(feature) {
    return !this._disallowedFeatures.includes(feature);
  }
};

var components = [EnterprisePoliciesManagerContent];
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
