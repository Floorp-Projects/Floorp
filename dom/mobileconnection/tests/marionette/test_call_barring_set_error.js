/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

function testSetCallBarringOption(aExpectedError, aOptions) {
  log("Test setCallBarringOption with " + JSON.stringify(aOptions));

  return setCallBarringOption(aOptions)
    .then(function resolve() {
      ok(false, "should be rejected");
    }, function reject(aError) {
      is(aError.name, aExpectedError, "failed to changeCallBarringPassword");
    });
}

// Start tests
startTestCommon(function() {
  return Promise.resolve()

    // Test program
    .then(() => testSetCallBarringOption("InvalidParameter", {
      "program": 5, /* Invalid program */
      "enabled": true,
      "password": "0000",
      "serviceClass": 0
    }))

    .then(() => testSetCallBarringOption("InvalidParameter", {
      "program": null,
      "enabled": true,
      "password": "0000",
      "serviceClass": 0
    }))

    .then(() => testSetCallBarringOption("InvalidParameter", {
      /* Undefined program */
      "enabled": true,
      "password": "0000",
      "serviceClass": 0
    }))

    // Test enabled
    .then(() => testSetCallBarringOption("InvalidParameter", {
      "program": MozMobileConnection.CALL_BARRING_PROGRAM_ALL_OUTGOING,
      "enabled": null, /* Invalid enabled */
      "password": "0000",
      "serviceClass": 0
    }))

    .then(() => testSetCallBarringOption("InvalidParameter", {
      "program": MozMobileConnection.CALL_BARRING_PROGRAM_ALL_OUTGOING,
      /* Undefined enabled */
      "password": "0000",
      "serviceClass": 0
    }))

    // Test password
    .then(() => testSetCallBarringOption("InvalidParameter", {
      "program": MozMobileConnection.CALL_BARRING_PROGRAM_ALL_OUTGOING,
      "enabled": true,
      "password": null, /* Invalid password */
      "serviceClass": 0
    }))

    .then(() => testSetCallBarringOption("InvalidParameter", {
      "program": MozMobileConnection.CALL_BARRING_PROGRAM_ALL_OUTGOING,
      "enabled": true,
      /* Undefined password */
      "serviceClass": 0
    }))

    .then(() => testSetCallBarringOption("IncorrectPassword", {
      "program": MozMobileConnection.CALL_BARRING_PROGRAM_ALL_OUTGOING,
      "enabled": true,
      "password": "1111", /* Incorrect password */
      "serviceClass": 0
    }))

    // Test serviceClass
    .then(() => testSetCallBarringOption("InvalidParameter", {
      "program": MozMobileConnection.CALL_BARRING_PROGRAM_ALL_OUTGOING,
      "enabled": true,
      "password": "0000",
      "serviceClass": null /* Invalid serviceClass */
    }))

    .then(() => testSetCallBarringOption("InvalidParameter", {
      "program": MozMobileConnection.CALL_BARRING_PROGRAM_ALL_OUTGOING,
      "enabled": true,
      "password": "0000",
      /* Undefined serviceClass */
    }))
});
