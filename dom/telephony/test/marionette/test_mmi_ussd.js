/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

function testUSSD() {
  log("Test *#1234# ...");

  return gSendMMI("*#1234#").then(aResult => {
    ok(aResult.success, "Check success");
    is(aResult.serviceCode, "scUssd", "Check serviceCode");
    is(aResult.statusMessage, "", "Check statusMessage");
    is(aResult.additionalInformation, undefined, "No additional information");
  });
}

// Start test
startTest(function() {
  return testUSSD()
    .catch(error => ok(false, "Promise reject: " + error))
    .then(finish);
});
