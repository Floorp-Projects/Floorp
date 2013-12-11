/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

SpecialPowers.addPermission("mobileconnection", true, document);

let connection = navigator.mozMobileConnections[0];
ok(connection instanceof MozMobileConnection,
   "connection is instanceof " + connection.constructor);


function setPreferredNetworkType(type, callback) {
  log("setPreferredNetworkType: " + type);

  let request = connection.setPreferredNetworkType(type);
  ok(request instanceof DOMRequest,
     "request instanceof " + request.constructor);

  request.onsuccess = function onsuccess() {
    ok(true, "request success");
    callback();
  }
  request.onerror = function onerror() {
    ok(false, request.error);
    callback();
  }
}

function getPreferredNetworkType(callback) {
  log("getPreferredNetworkType");

  let request = connection.getPreferredNetworkType();
  ok(request instanceof DOMRequest,
     "request instanceof " + request.constructor);

  request.onsuccess = function onsuccess() {
    ok(true, "request success");
    log("getPreferredNetworkType: " + request.result);
    callback(request.result);
  }
  request.onerror = function onerror() {
    ok(false, request.error);
    callback();
  }
}

function failToSetPreferredNetworkType(type, expectedError, callback) {
  log("failToSetPreferredNetworkType: " + type + ", expected error: "
    + expectedError);

  let request = connection.setPreferredNetworkType(type);
  ok(request instanceof DOMRequest,
     "request instanceof " + request.constructor);

  request.onsuccess = function onsuccess() {
    ok(false, "request should not succeed");
    callback();
  }
  request.onerror = function onerror() {
    ok(true, "request error");
    is(request.error.name, expectedError);
    callback();
  }
}

function setAndVerifyNetworkType(type) {
  setPreferredNetworkType(type, function() {
    getPreferredNetworkType(function(result) {
      is(result, type);
      testPreferredNetworkTypes();
    });
  });
}

function testPreferredNetworkTypes() {
  let networkType = supportedTypes.shift();
  if (!networkType) {
    runNextTest();
    return;
  }
  setAndVerifyNetworkType(networkType);
}

function failToSetAndVerifyNetworkType(type, expectedError, previousType) {
  failToSetPreferredNetworkType(type, expectedError, function() {
    getPreferredNetworkType(function(result) {
      // should return the previous selected type.
      is(result, previousType);
      testInvalidNetworkTypes();
    });
  });
}

function testInvalidNetworkTypes() {
  let networkType = invalidTypes.shift();
  if (!networkType) {
    runNextTest();
    return;
  }
  failToSetAndVerifyNetworkType(networkType, "InvalidParameter",
                                "wcdma/gsm");
}

let supportedTypes = [
  'gsm',
  'wcdma',
  'wcdma/gsm-auto',
  'cdma/evdo',
  'evdo',
  'cdma',
  'wcdma/gsm/cdma/evdo',
  'wcdma/gsm' // restore to default
];

let invalidTypes = [
  ' ',
  'AnInvalidType'
];

let tests = [
  testPreferredNetworkTypes,
  testInvalidNetworkTypes
];

function runNextTest() {
  let test = tests.shift();
  if (!test) {
    cleanUp();
    return;
  }

  test();
}

function cleanUp() {
  SpecialPowers.removePermission("mobileconnection", document);
  finish();
}

runNextTest();
