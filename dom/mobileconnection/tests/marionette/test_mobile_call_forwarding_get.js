/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const TEST_DATA = [
  // Test get calling forwarding.
  // TODO: Bug 861725 - B2G Emulator: support call forwarding
  // Currently emulator doesn't support REQUEST_QUERY_CALL_FORWARD_STATUS, so
  // we expect to get a 'RequestNotSupported' error here.
  {
    reason: MozMobileConnection.CALL_FORWARD_REASON_UNCONDITIONAL,
    expectedErrorMsg: "RequestNotSupported"
  }, {
    reason: MozMobileConnection.CALL_FORWARD_REASON_MOBILE_BUSY,
    expectedErrorMsg: "RequestNotSupported"
  }, {
    reason: MozMobileConnection.CALL_FORWARD_REASON_NO_REPLY,
    expectedErrorMsg: "RequestNotSupported"
  }, {
    reason: MozMobileConnection.CALL_FORWARD_REASON_NOT_REACHABLE,
    expectedErrorMsg: "RequestNotSupported"
  }, {
    reason: MozMobileConnection.CALL_FORWARD_REASON_ALL_CALL_FORWARDING,
    expectedErrorMsg: "RequestNotSupported"
  }, {
    reason: MozMobileConnection.CALL_FORWARD_REASON_ALL_CONDITIONAL_CALL_FORWARDING,
    expectedErrorMsg: "RequestNotSupported"
  },
  // Test passing invalid reason
  {
    reason: 10 /* Invalid reason */,
    expectedErrorMsg: "InvalidParameter"
  }
];

function testGetCallForwardingOption(aReason, aExpectedErrorMsg) {
  log("Test getting call forwarding for " + aReason);

  return getCallForwardingOption(aReason)
    .then(function resolve() {
      ok(!aExpectedErrorMsg, "getCallForwardingOption success");
    }, function reject(aError) {
      is(aError.name, aExpectedErrorMsg, "failed to getCallForwardingOption");
    });
}

// Start tests
startTestCommon(function() {
  let promise = Promise.resolve();
  for (let i = 0; i < TEST_DATA.length; i++) {
    let data = TEST_DATA[i];
    promise = promise.then(() => testGetCallForwardingOption(data.reason,
                                                             data.expectedErrorMsg));
  }
  return promise;
});
