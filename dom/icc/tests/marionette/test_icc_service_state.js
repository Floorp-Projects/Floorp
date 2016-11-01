/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

function testUnsupportedService() {
  try {
    icc.getServiceState("unsupported-service");
    ok(false, "should get exception");
  } catch (aException) {
    ok(true, "got exception: " + aException);
  }
}

// Start tests
startTestCommon(function() {
  let icc = getMozIcc();

  // Check fdn service state
  return icc.getServiceState("fdn")
    .then((aResult) => {
      is(aResult, true, "check fdn service state");
    })

    // Test unsupported service
    .then(() => testUnsupportedService());
});
