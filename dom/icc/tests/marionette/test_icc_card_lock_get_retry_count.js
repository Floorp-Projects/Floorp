/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

function testGetCardLockRetryCount(aIcc, aLockType, aRetryCount) {
  log("testGetCardLockRetryCount for " + aLockType);

  try {
    return aIcc.getCardLockRetryCount(aLockType)
      .then((aResult) => {
        if (!aRetryCount) {
          ok(false, "getCardLockRetryCount(" + aLockType + ") should not success");
          return;
        }

        // Check the request result.
        is(aResult.retryCount, aRetryCount, "result.retryCount");
      }, (aError) => {
        if (aRetryCount) {
          ok(false, "getCardLockRetryCount(" + aLockType + ") should not fail" +
                    aError.name);
        }
      });
  } catch (e) {
    ok(!aRetryCount, "caught an exception: " + e);
    return Promise.resolve();
  }
}

// Start tests
startTestCommon(function() {
  let icc = getMozIcc();

  // Read PIN-lock retry count.
  // The default PIN-lock retry count hard coded in emulator is 3.
  return testGetCardLockRetryCount(icc, "pin", 3)
    // Read PUK-lock retry count.
    // The default PUK-lock retry count hard coded in emulator is 6.
    .then(() => testGetCardLockRetryCount(icc, "puk", 6))
    // Read lock retry count for an invalid entries.
    .then(() => testGetCardLockRetryCount(icc, "invalid-lock-type"));
});
