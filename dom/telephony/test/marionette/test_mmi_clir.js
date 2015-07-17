/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const TEST_DATA = [
  // [mmi, expectedError]
  // Currently emulator doesn't support REQUEST_GET_CLIR, so we expect to get a
  // 'RequestNotSupported' error here.
  ["*#31#", "RequestNotSupported"],
  // Currently emulator doesn't support REQUEST_SET_CLIR, so we expect to get a
  // 'RequestNotSupported' error here.
  ["*31#", "RequestNotSupported"],
  ["#31#", "RequestNotSupported"],
  // Unsupported CLIR MMI code.
  ["**31#", "emMmiErrorNotSupported"],
  ["##31#", "emMmiErrorNotSupported"],
];

function testCLIR(aMmi, aExpectedError) {
  log("Test " + aMmi + " ...");

  return gSendMMI(aMmi).then(aResult => {
    // Since emulator doesn't support clip, so we expect the result is always
    // failed.
    ok(!aResult.success, "Check success");
    is(aResult.serviceCode, "scClir", "Check serviceCode");
    is(aResult.statusMessage, aExpectedError, "Check statusMessage");
  });
}

// Start test
startTest(function() {
  let promise = Promise.resolve();

  TEST_DATA.forEach(function(aData) {
    promise = promise.then(() => testCLIR(aData[0], aData[1]));
  });

  return promise
    .catch(aError => ok(false, "Promise reject: " + aError))
    .then(finish);
});
