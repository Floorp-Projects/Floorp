/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { Ci, Cc } = require("chrome");
var defer = require("devtools/shared/defer");
var DevToolsUtils = require("devtools/shared/DevToolsUtils");
DevToolsUtils.defineLazyGetter(this, "localCertService", () => {
  // Ensure PSM is initialized to support TLS sockets
  Cc["@mozilla.org/psm;1"].getService(Ci.nsISupports);
  return Cc["@mozilla.org/security/local-cert-service;1"]
         .getService(Ci.nsILocalCertService);
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
    const deferred = defer();
    localCertService.getOrCreateCert(localCertName, {
      handleCert: function(cert, rv) {
        if (rv) {
          deferred.reject(rv);
          return;
        }
        deferred.resolve(cert);
      }
    });
    return deferred.promise;
  },

  /**
   * Remove the DevTools self-signed X.509 cert for this device.
   *
   * @return promise
   */
  remove() {
    const deferred = defer();
    localCertService.removeCert(localCertName, {
      handleCert: function(rv) {
        if (rv) {
          deferred.reject(rv);
          return;
        }
        deferred.resolve();
      }
    });
    return deferred.promise;
  }

};
