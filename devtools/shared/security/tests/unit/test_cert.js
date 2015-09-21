/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const certService = Cc["@mozilla.org/security/local-cert-service;1"]
                    .getService(Ci.nsILocalCertService);

const gNickname = "devtools";

function run_test() {
  // Need profile dir to store the key / cert
  do_get_profile();
  // Ensure PSM is initialized
  Cc["@mozilla.org/psm;1"].getService(Ci.nsISupports);
  run_next_test();
}

function getOrCreateCert(nickname) {
  let deferred = promise.defer();
  certService.getOrCreateCert(nickname, {
    handleCert: function(c, rv) {
      if (rv) {
        deferred.reject(rv);
        return;
      }
      deferred.resolve(c);
    }
  });
  return deferred.promise;
}

function removeCert(nickname) {
  let deferred = promise.defer();
  certService.removeCert(nickname, {
    handleResult: function(rv) {
      if (rv) {
        deferred.reject(rv);
        return;
      }
      deferred.resolve();
    }
  });
  return deferred.promise;
}

add_task(function*() {
  // No master password, so no prompt required here
  ok(!certService.loginPromptRequired);

  let certA = yield getOrCreateCert(gNickname);
  equal(certA.nickname, gNickname);

  // Getting again should give the same cert
  let certB = yield getOrCreateCert(gNickname);
  equal(certB.nickname, gNickname);

  // Should be matching instances
  ok(certA.equals(certB));

  // Check a few expected attributes
  ok(certA.isSelfSigned);
  equal(certA.certType, Ci.nsIX509Cert.USER_CERT);

  // New nickname should give a different cert
  let diffNameCert = yield getOrCreateCert("cool-stuff");
  ok(!diffNameCert.equals(certA));

  // Remove the cert, and get a new one again
  yield removeCert(gNickname);
  let newCert = yield getOrCreateCert(gNickname);
  ok(!newCert.equals(certA));

  // Drop all cert references and GC
  let serial = newCert.serialNumber;
  certA = certB = diffNameCert = newCert = null;
  Cu.forceGC();
  Cu.forceCC();

  // Should still get the same cert back
  let certAfterGC = yield getOrCreateCert(gNickname);
  equal(certAfterGC.serialNumber, serial);
});
