/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const LOCK_TYPE = "pin";

function testChangePin(aIcc, aPin, aNewPin, aErrorName, aRetryCount) {
  log("testChangePin for pin=" + aPin + " and newPin=" + aNewPin);
  return aIcc.setCardLock({ lockType: LOCK_TYPE, pin: aPin, newPin: aNewPin })
    .then((aResult) => {
      if (aErrorName) {
        ok(false, "changing pin should not success");
      }
    }, (aError) => {
      if (!aErrorName) {
        ok(false, "changing pin should not fail");
        return;
      }

      // check the request error.
      is(aError.name, aErrorName, "error.name");
      is(aError.retryCount, aRetryCount, "error.retryCount");
    });
}

// Start tests
startTestCommon(function() {
  let icc = getMozIcc();
  let retryCount;

  return icc.getCardLockRetryCount(LOCK_TYPE)
    // Get current PIN-lock retry count.
    .then((aResult) => {
      retryCount = aResult.retryCount;
      ok(true, LOCK_TYPE + " retryCount is " + retryCount);
    })
    // Test PIN code changes fail.
    // The retry count should be decreased by 1.
    .then(() => testChangePin(icc, "1111", DEFAULT_PIN, "IncorrectPassword",
                              retryCount - 1))
    // Test PIN code changes success. This will reset the retry count.
    .then(() => testChangePin(icc, DEFAULT_PIN, DEFAULT_PIN));
});
