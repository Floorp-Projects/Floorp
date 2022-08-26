/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/chrome-script */

"use strict";

// This is only usable from the parent process, even for doing simple stuff like
// serializing a cert.
var gCertMaker = Cc["@mozilla.org/security/x509certdb;1"].getService(
  Ci.nsIX509CertDB
);

var gCertOverrides = Cc["@mozilla.org/security/certoverride;1"].getService(
  Ci.nsICertOverrideService
);

addMessageListener("add-turns-certs", certs => {
  var port = 5349;
  certs.forEach(certDescription => {
    var cert = gCertMaker.constructX509FromBase64(certDescription.cert);
    gCertOverrides.rememberValidityOverride(
      certDescription.hostname,
      port,
      {},
      cert,
      false
    );
  });
  sendAsyncMessage("certs-added");
});
