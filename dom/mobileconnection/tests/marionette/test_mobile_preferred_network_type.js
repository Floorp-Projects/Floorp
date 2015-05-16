/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const TEST_DATA = [
  {
    preferredNetworkType: "gsm"
  }, {
    preferredNetworkType: "wcdma"
  }, {
    preferredNetworkType: "wcdma/gsm-auto"
  }, {
    preferredNetworkType: "cdma/evdo"
  }, {
    preferredNetworkType: "evdo"
  }, {
    preferredNetworkType: "cdma"
  }, {
    preferredNetworkType: "wcdma/gsm/cdma/evdo"
  },
  {
    preferredNetworkType: "wcdma/gsm" // Restore to default
  },
  // Currently emulator doesn't support lte network. So we expect to get a
  // 'ModeNotSupported' error here.
  {
    preferredNetworkType: "lte/cdma/evdo",
    expectedErrorMessage: "ModeNotSupported"
  }, {
    preferredNetworkType: "lte/wcdma/gsm",
    expectedErrorMessage: "ModeNotSupported"
  }, {
    preferredNetworkType: "lte/wcdma/gsm/cdma/evdo",
    expectedErrorMessage: "ModeNotSupported"
  }, {
    preferredNetworkType: "lte",
    expectedErrorMessage: "ModeNotSupported"
  }, {
    preferredNetworkType: "lte/wcdma",
    expectedErrorMessage: "ModeNotSupported"
  },
  // Test passing an invalid mode. We expect to get an exception here.
  {
    preferredNetworkType: "InvalidTypes",
    expectGotException: true
  }, {
    preferredNetworkType: null,
    expectGotException: true
  }
];

function verifyPreferredNetworkType(aExpectedType) {
  return getPreferredNetworkType()
    .then(function resolve(aType) {
      is(aType, aExpectedType, "getPreferredNetworkType success");
    }, function reject() {
      ok(false, "failed to getPreferredNetworkType");
    });
}

function setAndVerifyPreferredNetworkType(aType, aExpectedErrorMsg, aExpectGotException) {
  log("Test setting preferred network types to " + aType);

  try {
    return setPreferredNetworkType(aType)
      .then(function resolve() {
        ok(!aExpectedErrorMsg, "setPreferredNetworkType success");
        return verifyPreferredNetworkType(aType);
      }, function reject(aError) {
        is(aError.name, aExpectedErrorMsg, "failed to setPreferredNetworkType");
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
    promise = promise.then(() => setAndVerifyPreferredNetworkType(data.preferredNetworkType,
                                                                  data.expectedErrorMessage,
                                                                  data.expectGotException));
  }
  return promise;
});
