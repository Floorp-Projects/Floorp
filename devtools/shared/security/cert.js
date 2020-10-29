/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { Ci, Cc } = require("chrome");
var DevToolsUtils = require("devtools/shared/DevToolsUtils");
DevToolsUtils.defineLazyGetter(this, "localCertService", () => {
  // Ensure PSM is initialized to support TLS sockets
  Cc["@mozilla.org/psm;1"].getService(Ci.nsISupports);
  return Cc["@mozilla.org/security/local-cert-service;1"].getService(
    Ci.nsILocalCertService
  );
});

const localCertName = "devtools";

exports.local = {
  /**
   * Get or create a new self-signed X.509 cert to represent this device for
   * DevTools purposes over a secure transport, like TLS.
   *
   * The cert is stored permanently in the profile's key store after first use,
   * and is valid for 1 year.  If an expired or otherwise invalid cert is found,
   * it is removed and a new one is made.
   *
   * @return promise
   */
  getOrCreate() {
    return new Promise((resolve, reject) => {
      localCertService.getOrCreateCert(localCertName, {
        handleCert: function(cert, rv) {
          if (rv) {
            reject(rv);
            return;
          }
          resolve(cert);
        },
      });
    });
  },

  /**
   * Remove the DevTools self-signed X.509 cert for this device.
   *
   * @return promise
   */
  remove() {
    return new Promise((resolve, reject) => {
      localCertService.removeCert(localCertName, {
        handleCert: function(rv) {
          if (rv) {
            reject(rv);
            return;
          }
          resolve();
        },
      });
    });
  },
};
