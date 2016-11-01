/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

function setCardLockAndCheck(aIcc, aLockType, aPin, aEnabled, aErrorName,
                             aRetryCount) {
  let options = {
    lockType: aLockType,
    enabled: aEnabled,
    pin: aPin
  };

  return aIcc.setCardLock(options)
    .then((aResult) => {
      if (aErrorName) {
        ok(false, "setting pin should not success");
        return;
      }

      // Check lock state.
      return aIcc.getCardLock(aLockType)
        .then((aResult) => {
          is(aResult.enabled, aEnabled, "result.enabled");
        });
    }, (aError) => {
      if (!aErrorName) {
        ok(false, "setting pin should not fail");
        return;
      }

      // Check the request error.
      is(aError.name, aErrorName, "error.name");
      is(aError.retryCount, aRetryCount, "error.retryCount");
    });
}

// Start tests
startTestCommon(function() {
  let icc = getMozIcc();
  let lockType = "pin";
  let retryCount;

  return icc.getCardLockRetryCount(lockType)
    // Get current PIN-lock retry count.
    .then((aResult) => {
      retryCount = aResult.retryCount;
      ok(true, lockType + " retryCount is " + retryCount);
    })
    // Test fail to enable PIN-lock by passing wrong pin.
    // The retry count should be decreased by 1.
    .then(() => setCardLockAndCheck(icc, lockType, "1111", true,
                                    "IncorrectPassword", retryCount -1))
    // Test enabling PIN-lock.
    .then(() => setCardLockAndCheck(icc, lockType, DEFAULT_PIN, true))
    // Restore pin state.
    .then(() => setCardLockAndCheck(icc, lockType, DEFAULT_PIN, false));
});
