/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const TEST_DATA = [
  // TODO: Bug 896394 - B2G RIL: Add marionette test cases for CDMA roaming
  //                    preference.
  // Currently emulator run as GSM mode by default. So we expect to get a
  // 'RequestNotSupported' error here.
  {
    mode: "home",
    expectedErrorMessage: "RequestNotSupported"
  }, {
    mode: "affiliated",
    expectedErrorMessage: "RequestNotSupported"
  }, {
    mode: "any",
    expectedErrorMessage: "RequestNotSupported"
  },
  // Test passing an invalid mode. We expect to get an exception here.
  {
    mode: "InvalidMode",
    expectGotException: true
  }, {
    mode: null,
    expectGotException: true
  }
];

function testSetRoamingPreference(aMode, aExpectedErrorMsg, aExpectGotException) {
  log("Test setting roaming preference mode to " + aMode);

  try {
    return setRoamingPreference(aMode)
      .then(function resolve() {
        ok(!aExpectedErrorMsg, "setRoamingPreference success");
      }, function reject(aError) {
        is(aError.name, aExpectedErrorMsg, "failed to setRoamingPreference");
      });
  } catch (e) {
    ok(aExpectGotException, "caught an exception: " + e);
  }
}

// Start tests
startTestCommon(function() {
  let promise = Promise.resolve();
  for (let i = 0; i < TEST_DATA.length; i++) {
    let data = TEST_DATA[i];
    promise = promise.then(() => testSetRoamingPreference(data.mode,
                                                          data.expectedErrorMessage,
                                                          data.expectGotException));
  }
  return promise;
});
