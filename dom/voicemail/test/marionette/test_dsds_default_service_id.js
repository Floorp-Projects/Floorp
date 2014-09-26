/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_CONTEXT = "chrome";

Cu.import("resource://gre/modules/Promise.jsm");

const VOICEMAIL_SERVICE_CONTRACTID =
  "@mozilla.org/voicemail/gonkvoicemailservice;1";

const PREF_RIL_NUM_RADIO_INTERFACES = "ril.numRadioInterfaces";
const PREF_DEFAULT_SERVICE_ID = "dom.voicemail.defaultServiceId";

function setPrefAndVerify(prefKey, setVal, service, expectedVal, deferred) {
  log("  Set '" + prefKey + "' to " + setVal);
  Services.prefs.setIntPref(prefKey, setVal);
  let prefVal = Services.prefs.getIntPref(prefKey);
  is(prefVal, setVal, "'" + prefKey + "' set to " + setVal);

  window.setTimeout(function() {
    let defaultVal = service.getDefaultItem().serviceId;
    is(defaultVal, expectedVal, "serviceId");

    deferred.resolve(service);
  }, 0);
}

function getNumRadioInterfaces() {
  let deferred = Promise.defer();

  window.setTimeout(function() {
    let numRil = Services.prefs.getIntPref(PREF_RIL_NUM_RADIO_INTERFACES);
    log("numRil = " + numRil);

    deferred.resolve(numRil);
  }, 0);

  return deferred.promise;
}

function getService(contractId, ifaceName) {
  let deferred = Promise.defer();

  window.setTimeout(function() {
    log("Getting service for " + ifaceName);
    let service = Cc[contractId].getService(Ci[ifaceName]);
    ok(service, "service.constructor is " + service.constructor);

    deferred.resolve(service);
  }, 0);

  return deferred.promise;
}

function checkInitialEquality(prefKey, service) {
  let deferred = Promise.defer();

  log("  Checking initial value for '" + prefKey + "'");
  let origPrefVal = Services.prefs.getIntPref(prefKey);
  ok(isFinite(origPrefVal), "default '" + prefKey + "' value");

  window.setTimeout(function() {
    let defaultVal = service.getDefaultItem().serviceId;
    is(defaultVal, origPrefVal, "serviceId");

    deferred.resolve(service);
  }, 0);

  return deferred.promise;
}

function checkSetToNegtiveValue(prefKey, service) {
  let deferred = Promise.defer();

  // Set to -1 and verify defaultVal == 0.
  setPrefAndVerify(prefKey, -1, service, 0, deferred);

  return deferred.promise;
}

function checkSetToOverflowedValue(prefKey, numRil, service) {
  let deferred = Promise.defer();

  // Set to larger-equal than numRil and verify defaultVal == 0.
  setPrefAndVerify(prefKey, numRil, service, 0, deferred);

  return deferred.promise;
}

function checkValueChange(prefKey, numRil, service) {
  let deferred = Promise.defer();

  if (numRil > 1) {
    // Set to (numRil - 1) and verify defaultVal equals.
    setPrefAndVerify(prefKey, numRil - 1, service, numRil - 1, deferred);
  } else {
    window.setTimeout(function() {
      deferred.resolve(service);
    }, 0);
  }

  return deferred.promise;
}

function verify(contractId, ifaceName, prefKey, numRil) {
  let deferred = Promise.defer();

  getService(contractId, ifaceName)
    .then(checkInitialEquality.bind(null, prefKey))
    .then(checkSetToNegtiveValue.bind(null, prefKey))
    .then(checkSetToOverflowedValue.bind(null, prefKey, numRil))
    .then(checkValueChange.bind(null, prefKey, numRil))
    .then(function() {
      // Reset.
      Services.prefs.clearUserPref(prefKey);

      deferred.resolve(numRil);
    });

  return deferred.promise;
}

getNumRadioInterfaces()
  .then(verify.bind(null, VOICEMAIL_SERVICE_CONTRACTID, "nsIVoicemailService",
                    PREF_DEFAULT_SERVICE_ID))
  .then(finish);
