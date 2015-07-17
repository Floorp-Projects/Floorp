/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

function testUSSD() {
  log("Test *#1234# ...");

  return gSendMMI("*#1234#").then(aResult => {
    // Since emulator doesn't support sending USSD, so we expect the result is
    // always failed.
    ok(!aResult.success, "Check success");
    is(aResult.serviceCode, "scUssd", "Check serviceCode");
    is(aResult.statusMessage, "RequestNotSupported", "Check statusMessage");
    is(aResult.additionalInformation, undefined, "No additional information");
  });
}

// Start test
startTest(function() {
  return testUSSD()
    .catch(error => ok(false, "Promise reject: " + error))
    .then(finish);
});
