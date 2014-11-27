/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

function testUnlockPin(aIcc, aPin, aErrorName, aRetryCount) {
  log("testUnlockPin with pin=" + aPin);

  return aIcc.unlockCardLock({ lockType: "pin", pin: aPin })
    .then((aResult) => {
      if (aErrorName) {
        ok(false, "unlocking pin should not success");
      }
    }, (aError) => {
      if (!aErrorName) {
        ok(false, "unlocking pin should not fail");
        return;
      }

      // Check the request error.
      is(aError.name, aErrorName, "error.name");
      is(aError.retryCount, aRetryCount, "error.retryCount");
    });
}

function testUnlockPinAndWait(aIcc, aPin, aCardState) {
  log("testUnlockPin with pin=" + aPin + ", and wait cardState changes to '" +
      aCardState + "'");

  let promises = [];

  promises.push(waitForTargetEvent(aIcc, "cardstatechange", function() {
    return aIcc.cardState === aCardState;
  }));
  promises.push(testUnlockPin(aIcc, aPin));

  return Promise.all(promises);
}

// Start tests
startTestCommon(function() {
  let icc = getMozIcc();
  let retryCount;

  // Enable PIN-lock.
  return setPinLockEnabled(icc, true)
    // Reset card state to "pinRequired" by restarting radio
    .then(() => restartRadioAndWait("pinRequired"))
    .then(() => { icc = getMozIcc(); })

    // Get current PIN-lock retry count.
    .then(() => icc.getCardLockRetryCount("pin"))
    .then((aResult) => {
      retryCount = aResult.retryCount;
      ok(true, "pin retryCount is " + retryCount);
    })

    // Test fail to unlock PIN-lock.
    // The retry count should be decreased by 1.
    .then(() => testUnlockPin(icc, "1111", "IncorrectPassword", retryCount - 1))

    // Test success to unlock PIN-lock.
    .then(() => testUnlockPinAndWait(icc, DEFAULT_PIN, "ready"))

    // Restore pin state.
    .then(() => setPinLockEnabled(icc, false));
});
