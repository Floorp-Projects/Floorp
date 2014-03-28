/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Ci = Components.interfaces;
const Cu = Components.utils;

this.EXPORTED_SYMBOLS = [
  "TrustedRootCertificate"
];

const APP_TRUSTED_ROOTS= ["AppMarketplaceProdPublicRoot",
                          "AppMarketplaceProdReviewersRoot",
                          "AppMarketplaceDevPublicRoot",
                          "AppMarketplaceDevReviewersRoot",
                          "AppXPCShellRoot"];

this.TrustedRootCertificate = {
  _index: Ci.nsIX509CertDB.AppMarketplaceProdPublicRoot,
  get index() {
    return this._index;
  },
  set index(aIndex) {
    // aIndex should be one of the
    // Ci.nsIX509CertDB AppTrustedRoot defined values
    let found = APP_TRUSTED_ROOTS.some((trustRoot) => {
      return Ci.nsIX509CertDB[trustRoot] === aIndex;
    });
    if (found) {
      this._index = aIndex;
    }
  }
};

