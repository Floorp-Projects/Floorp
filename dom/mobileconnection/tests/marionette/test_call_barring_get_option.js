/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const TEST_DATA = [
  // Test passing invalid program.
  {
    options: {
      program: 5, /* Invalid program */
      serviceClass: 0
    },
    expectedError: "InvalidParameter"
  }, {
    options: {
      program: null,
      serviceClass: 0
    },
    expectedError: "InvalidParameter"
  }, {
    options: {
      /* Undefined program */
      serviceClass: 0
    },
    expectedError: "InvalidParameter"
  },
  // Test passing invalid serviceClass.
  {
    options: {
      program: MozMobileConnection.CALL_BARRING_PROGRAM_ALL_OUTGOING,
      serviceClass: null
    },
    expectedError: "InvalidParameter"
  }, {
    options: {
      program: MozMobileConnection.CALL_BARRING_PROGRAM_ALL_OUTGOING,
      /* Undefined serviceClass */
    },
    expectedError: "InvalidParameter"
  },
  // TODO: Bug 1027546 - [B2G][Emulator] Support call barring
  // Currently emulator doesn't support call barring, so we expect to get a
  // 'RequestNotSupported' error here.
  {
    options: {
      program: MozMobileConnection.CALL_BARRING_PROGRAM_ALL_OUTGOING,
      serviceClass: 0
    },
    expectedError: "RequestNotSupported"
  }
];

function testGetCallBarringOption(aOptions, aExpectedError) {
  log("Test getting call barring to " + JSON.stringify(aOptions));

  return getCallBarringOption(aOptions)
    .then(function resolve(aResult) {
      ok(false, "should not success");
    }, function reject(aError) {
      is(aError.name, aExpectedError, "failed to getCallBarringOption");
    });
}

// Start tests
startTestCommon(function() {
  let promise = Promise.resolve();
  for (let i = 0; i < TEST_DATA.length; i++) {
    let data = TEST_DATA[i];
    promise = promise.then(() => testGetCallBarringOption(data.options,
                                                          data.expectedError));
  }
  return promise;
});
