/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_CONTEXT = "chrome";

Cu.import("resource://gre/modules/Promise.jsm");

const MMS_SERVICE_CONTRACTID = "@mozilla.org/mms/mmsservice;1";
const SMS_SERVICE_CONTRACTID = "@mozilla.org/sms/smsservice;1";

const PREF_RIL_NUM_RADIO_INTERFACES = "ril.numRadioInterfaces";
const PREF_MMS_DEFAULT_SERVICE_ID = "dom.mms.defaultServiceId";
const PREF_SMS_DEFAULT_SERVICE_ID = "dom.sms.defaultServiceId";

function setPrefAndVerify(prefKey, setVal, service, attrName, expectedVal, deferred) {
  log("  Set '" + prefKey + "' to " + setVal);
  Services.prefs.setIntPref(prefKey, setVal);
  let prefVal = Services.prefs.getIntPref(prefKey);
  is(prefVal, setVal, "'" + prefKey + "' set to " + setVal);

  window.setTimeout(function () {
    let defaultVal = service[attrName];
    is(defaultVal, expectedVal, attrName);

    deferred.resolve(service);
  }, 0);
}

function getNumRadioInterfaces() {
  let deferred = Promise.defer();

  window.setTimeout(function () {
    let numRil = Services.prefs.getIntPref(PREF_RIL_NUM_RADIO_INTERFACES);
    log("numRil = " + numRil);

    deferred.resolve(numRil);
  }, 0);

  return deferred.promise;
}

function getService(contractId, ifaceName) {
  let deferred = Promise.defer();

  window.setTimeout(function () {
    log("Getting service for " + ifaceName);
    let service = Cc[contractId].getService(Ci[ifaceName]);
    ok(service, "service.constructor is " + service.constructor);

    deferred.resolve(service);
  }, 0);

  return deferred.promise;
}

function checkInitialEquality(attrName, prefKey, service) {
  let deferred = Promise.defer();

  log("  Checking initial value for '" + prefKey + "'");
  let origPrefVal = Services.prefs.getIntPref(prefKey);
  ok(isFinite(origPrefVal), "default '" + prefKey + "' value");

  window.setTimeout(function () {
    let defaultVal = service[attrName];
    is(defaultVal, origPrefVal, attrName);

    deferred.resolve(service);
  }, 0);

  return deferred.promise;
}

function checkSetToNegtiveValue(attrName, prefKey, service) {
  let deferred = Promise.defer();

  // Set to -1 and verify defaultVal == 0.
  setPrefAndVerify(prefKey, -1, service, attrName, 0, deferred);

  return deferred.promise;
}

function checkSetToOverflowedValue(attrName, prefKey, numRil, service) {
  let deferred = Promise.defer();

  // Set to larger-equal than numRil and verify defaultVal == 0.
  setPrefAndVerify(prefKey, numRil, service, attrName, 0, deferred);

  return deferred.promise;
}

function checkValueChange(attrName, prefKey, numRil, service) {
  let deferred = Promise.defer();

  if (numRil > 1) {
    // Set to (numRil - 1) and verify defaultVal equals.
    setPrefAndVerify(prefKey, numRil - 1, service, attrName, numRil - 1, deferred);
  } else {
    window.setTimeout(function () {
      deferred.resolve(service);
    }, 0);
  }

  return deferred.promise;
}

function verify(contractId, ifaceName, attrName, prefKey, numRil) {
  let deferred = Promise.defer();

  getService(contractId, ifaceName)
    .then(checkInitialEquality.bind(null, attrName, prefKey))
    .then(checkSetToNegtiveValue.bind(null, attrName, prefKey))
    .then(checkSetToOverflowedValue.bind(null, attrName, prefKey, numRil))
    .then(checkValueChange.bind(null, attrName, prefKey, numRil))
    .then(function () {
      // Reset.
      Services.prefs.clearUserPref(prefKey);

      deferred.resolve(numRil);
    });

  return deferred.promise;
}

getNumRadioInterfaces()
  .then(verify.bind(null, MMS_SERVICE_CONTRACTID, "nsIMmsService",
                    "mmsDefaultServiceId", PREF_MMS_DEFAULT_SERVICE_ID))
  .then(verify.bind(null, SMS_SERVICE_CONTRACTID, "nsISmsService",
                    "smsDefaultServiceId", PREF_SMS_DEFAULT_SERVICE_ID))
  .then(finish);
