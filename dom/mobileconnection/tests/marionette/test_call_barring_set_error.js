/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const TEST_DATA = [
  // Test passing invalid program.
  {
    options: {
      "program": 5, /* Invalid program */
      "enabled": true,
      "password": "0000",
      "serviceClass": 0
    },
    expectedError: "InvalidParameter"
  }, {
    options: {
      "program": null,
      "enabled": true,
      "password": "0000",
      "serviceClass": 0
    },
    expectedError: "InvalidParameter"
  }, {
    options: {
      /* Undefined program */
      "enabled": true,
      "password": "0000",
      "serviceClass": 0
    },
    expectedError: "InvalidParameter"
  },
  // Test passing invalid enabled.
  {
    options: {
      "program": MozMobileConnection.CALL_BARRING_PROGRAM_ALL_OUTGOING,
      "enabled": null,
      "password": "0000",
      "serviceClass": 0
    },
    expectedError: "InvalidParameter"
  }, {
    options: {
      "program": MozMobileConnection.CALL_BARRING_PROGRAM_ALL_OUTGOING,
      /* Undefined enabled */
      "password": "0000",
      "serviceClass": 0
    },
    expectedError: "InvalidParameter"
  },
  // Test passing invalid password.
  {
    options: {
      "program": MozMobileConnection.CALL_BARRING_PROGRAM_ALL_OUTGOING,
      "enabled": true,
      "password": null,
      "serviceClass": 0
    },
    expectedError: "InvalidParameter"
  }, {
    options: {
      "program": MozMobileConnection.CALL_BARRING_PROGRAM_ALL_OUTGOING,
      "enabled": true,
      /* Undefined password */
      "serviceClass": 0
    },
    expectedError: "InvalidParameter"
  },
  // Test passing invalid serviceClass.
  {
    options: {
      "program": MozMobileConnection.CALL_BARRING_PROGRAM_ALL_OUTGOING,
      "enabled": true,
      "password": "0000",
      "serviceClass": null
    },
    expectedError: "InvalidParameter"
  }, {
    options: {
      "program": MozMobileConnection.CALL_BARRING_PROGRAM_ALL_OUTGOING,
      "enabled": true,
      "password": "0000",
      /* Undefined serviceClass */
    },
    expectedError: "InvalidParameter"
  },
  // TODO: Bug 1027546 - [B2G][Emulator] Support call barring
  // Currently emulator doesn't support call barring, so we expect to get a
  // 'RequestNotSupported' error here.
  {
    options: {
      "program": MozMobileConnection.CALL_BARRING_PROGRAM_ALL_OUTGOING,
      "enabled": true,
      "password": "0000",
      "serviceClass": 0
    },
    expectedError: "RequestNotSupported"
  }
];

function testSetCallBarringOption(aOptions, aExpectedError) {
  log("Test setting call barring to " + JSON.stringify(aOptions));

  return setCallBarringOption(aOptions)
    .then(function resolve() {
      ok(false, "changeCallBarringPassword success");
    }, function reject(aError) {
      is(aError.name, aExpectedError, "failed to changeCallBarringPassword");
    });
}

// Start tests
startTestCommon(function() {
  let promise = Promise.resolve();
  for (let i = 0; i < TEST_DATA.length; i++) {
    let data = TEST_DATA[i];
    promise = promise.then(() => testSetCallBarringOption(data.options,
                                                          data.expectedError));
  }
  return promise;
});
