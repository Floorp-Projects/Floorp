/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const TEST_NUMBER = "0912345678";
const TEST_TIME_SECONDS = 5;
const TEST_DATA = [
  // Currently emulator doesn't support CALL_FORWARD_REASON_ALL_CALL_FORWARDING
  // and CALL_FORWARD_REASON_ALL_CONDITIONAL_CALL_FORWARDING, so
  // we expect to get a 'GenericFailure' error here.
  {
    options: {
      action: MozMobileConnection.CALL_FORWARD_ACTION_DISABLE,
      reason: MozMobileConnection.CALL_FORWARD_REASON_ALL_CALL_FORWARDING,
    },
    errorMsg: "GenericFailure"
  }, {
    options: {
      action: MozMobileConnection.CALL_FORWARD_ACTION_ENABLE,
      reason: MozMobileConnection.CALL_FORWARD_REASON_ALL_CONDITIONAL_CALL_FORWARDING,
    },
    errorMsg: "GenericFailure"
  },
  // Test passing invalid action. We expect to get a 'InvalidParameter' error.
  {
    options: {
      // Set operation doesn't allow "query" action.
      action: MozMobileConnection.CALL_FORWARD_ACTION_QUERY_STATUS,
      reason: MozMobileConnection.CALL_FORWARD_REASON_MOBILE_BUSY,
    },
    errorMsg: "InvalidParameter"
  }, {
    options: {
      action: 10 /* Invalid action */,
      reason: MozMobileConnection.CALL_FORWARD_REASON_MOBILE_BUSY,
    },
    errorMsg: "InvalidParameter"
  },
  // Test passing invalid reason. We expect to get a 'InvalidParameter' error.
  {
    options: {
      action: MozMobileConnection.CALL_FORWARD_ACTION_DISABLE,
      reason: 10 /*Invalid reason*/,
    },
    errorMsg: "InvalidParameter"
  }
];

function testSetCallForwardingOption(aOptions, aErrorMsg) {
  log("Test setting call forwarding to " + JSON.stringify(aOptions));

  aOptions.number = TEST_NUMBER;
  aOptions.timeSeconds = TEST_TIME_SECONDS;

  return setCallForwardingOption(aOptions)
    .then(function resolve() {
      ok(false, "setCallForwardingOption success");
    }, function reject(aError) {
      is(aError.name, aErrorMsg, "failed to setCallForwardingOption");
    });
}

// Start tests
startTestCommon(function() {
  let promise = Promise.resolve();
  for (let i = 0; i < TEST_DATA.length; i++) {
    let data = TEST_DATA[i];
    promise = promise.then(() => testSetCallForwardingOption(data.options,
                                                             data.errorMsg));
  }
  return promise;
});
