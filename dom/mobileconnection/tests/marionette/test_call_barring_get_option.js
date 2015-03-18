/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

function testGetCallBarringOption(aExpectedError, aOptions) {
  log("Test getCallBarringOption with " + JSON.stringify(aOptions));

  return getCallBarringOption(aOptions)
    .then(function resolve(aResult) {
      ok(false, "should be rejected");
    }, function reject(aError) {
      is(aError.name, aExpectedError, "failed to getCallBarringOption");
    });
}

// Start tests
startTestCommon(function() {
  return Promise.resolve()

    // Test program
    .then(() => testGetCallBarringOption("InvalidParameter", {
      program: 5, /* Invalid program */
      serviceClass: 0
    }))

    .then(() => testGetCallBarringOption("InvalidParameter", {
      program: null, /* Invalid program */
      serviceClass: 0
    }))

    .then(() => testGetCallBarringOption("InvalidParameter", {
      /* Undefined program */
      serviceClass: 0
    }))

    // Test serviceClass
    .then(() => testGetCallBarringOption("InvalidParameter", {
      program: MozMobileConnection.CALL_BARRING_PROGRAM_ALL_OUTGOING,
      serviceClass: null /* Invalid serviceClass */
    }))

    .then(() => testGetCallBarringOption("InvalidParameter", {
      program: MozMobileConnection.CALL_BARRING_PROGRAM_ALL_OUTGOING,
      /* Undefined serviceClass */
    }));
});
