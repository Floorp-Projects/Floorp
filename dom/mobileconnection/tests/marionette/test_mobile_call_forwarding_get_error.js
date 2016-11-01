/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const TEST_DATA = [
  // Currently emulator doesn't support CALL_FORWARD_REASON_ALL_CALL_FORWARDING
  // and CALL_FORWARD_REASON_ALL_CONDITIONAL_CALL_FORWARDING, so
  // we expect to get a 'GenericFailure' error here.
  {
    reason: MozMobileConnection.CALL_FORWARD_REASON_ALL_CALL_FORWARDING,
    errorMsg: "GenericFailure"
  }, {
    reason: MozMobileConnection.CALL_FORWARD_REASON_ALL_CONDITIONAL_CALL_FORWARDING,
    errorMsg: "GenericFailure"
  },
  // Test passing invalid reason
  {
    reason: 10 /* Invalid reason */,
    errorMsg: "InvalidParameter"
  }
];

function testGetCallForwardingOptionError(aReason, aErrorMsg) {
  log("Test getting call forwarding for " + aReason);

  return getCallForwardingOption(aReason)
    .then(function resolve() {
      ok(false, "getCallForwardingOption success");
    }, function reject(aError) {
      is(aError.name, aErrorMsg, "failed to getCallForwardingOption");
    });
}

// Start tests
startTestCommon(function() {
  let promise = Promise.resolve();
  for (let i = 0; i < TEST_DATA.length; i++) {
    let data = TEST_DATA[i];
    promise = promise.then(() => testGetCallForwardingOptionError(data.reason,
                                                                  data.errorMsg));
  }
  return promise;
});
