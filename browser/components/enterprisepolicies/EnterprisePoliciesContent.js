/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

function EnterprisePoliciesManagerContent() {
}

EnterprisePoliciesManagerContent.prototype = {
  classID:        Components.ID("{dc6358f8-d167-4566-bf5b-4350b5e6a7a2}"),
  QueryInterface: ChromeUtils.generateQI([Ci.nsIEnterprisePolicies]),
  _xpcom_factory: XPCOMUtils.generateSingletonFactory(EnterprisePoliciesManagerContent),

  get status() {
    return Services.cpmm.sharedData.get("EnterprisePolicies:Status") ||
           Ci.nsIEnterprisePolicies.INACTIVE;
  },

  isAllowed(feature) {
    let disallowedFeatures = Services.cpmm.sharedData.get("EnterprisePolicies:DisallowedFeatures");
    return !(disallowedFeatures && disallowedFeatures.has(feature));
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([EnterprisePoliciesManagerContent]);
