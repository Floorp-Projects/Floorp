/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const TEST_DATA = [
  // [mmi, expectedError]
  // Currently emulator doesn't support REQUEST_QUERY_CALL_WAITING, so we expect
  // to get a 'RequestNotSupported' error here.
  ["*#43*10#", "RequestNotSupported"],
  // Currently emulator doesn't support REQUEST_SET_CALL_WAITING, so we expect
  // to get a 'RequestNotSupported' error here.
  ["*43*10#", "RequestNotSupported"],
  ["#43*10#", "RequestNotSupported"],
  // Unsupported Call Waiting MMI code.
  ["**43*10#", "emMmiErrorNotSupported"],
  ["##43*10#", "emMmiErrorNotSupported"],
];

function testCallWaiting(aMmi, aExpectedError) {
  log("Test " + aMmi + " ...");

  return gSendMMI(aMmi).then(aResult => {
    // Since emulator doesn't support call waiting, so we expect the result is
    // always failed.
    ok(!aResult.success, "Check success");
    is(aResult.serviceCode, "scCallWaiting", "Check serviceCode");
    is(aResult.statusMessage, aExpectedError, "Check statusMessage");
  });
}

// Start test
startTest(function() {
  let promise = Promise.resolve();

  TEST_DATA.forEach(function(aData) {
    promise = promise.then(() => testCallWaiting(aData[0], aData[1]));
  });

  return promise
    .catch(aError => ok(false, "Promise reject: " + aError))
    .then(finish);
});
