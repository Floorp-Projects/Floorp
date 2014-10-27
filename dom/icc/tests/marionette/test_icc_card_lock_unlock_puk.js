/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

function passingWrongPinAndWait(aIcc) {
  return aIcc.getCardLockRetryCount("pin").then((aResult) => {
    let promises = [];
    let retryCount = aResult.retryCount;

    ok(true, "pin retryCount is " + retryCount);

    promises.push(waitForTargetEvent(aIcc, "cardstatechange", function() {
      return aIcc.cardState === "pukRequired";
    }));

    for (let i = 0; i < retryCount; i++) {
      promises.push(aIcc.unlockCardLock({ lockType: "pin", pin: "1111" })
        .then(() => {
          ok(false, "unlocking pin should not success");
        }, function reject(aRetryCount, aError) {
          // check the request error.
          is(aError.name, "IncorrectPassword", "error.name");
          is(aError.retryCount, aRetryCount, "error.retryCount");
        }.bind(null, retryCount - i - 1)));
    }

    return Promise.all(promises);
  });
}

function testUnlockPuk(aIcc, aPuk, aNewPin, aErrorName, aRetryCount) {
  log("testUnlockPuk with puk=" + aPuk + " and newPin=" + aNewPin);

  return aIcc.unlockCardLock({ lockType: "puk", puk: aPuk, newPin: aNewPin })
    .then((aResult) => {
      if (aErrorName) {
        ok(false, "unlocking puk should not success");
      }
    }, (aError) => {
      if (!aErrorName) {
        ok(false, "unlocking puk should not fail");
        return;
      }

      // Check the request error.
      is(aError.name, aErrorName, "error.name");
      is(aError.retryCount, aRetryCount, "error.retryCount");
    });
}

function testUnlockPukAndWait(aIcc, aPuk, aNewPin, aCardState) {
  log("testUnlockPuk with puk=" + aPuk + "/newPin=" + aNewPin +
      ", and wait card state changes to '" + aCardState + "'");

  let promises = [];

  promises.push(waitForTargetEvent(aIcc, "cardstatechange", function() {
    return aIcc.cardState === aCardState;
  }));
  promises.push(testUnlockPuk(aIcc, aPuk, aNewPin));

  return Promise.all(promises);
}

// Start tests
startTestCommon(function() {
  let icc = getMozIcc();
  let retryCount;

  // Enable pin lock.
  return setPinLockEnabled(icc, true)
    // Reset card state to "pinRequired" by restarting radio
    .then(() => restartRadioAndWait("pinRequired"))
    .then(() => { icc = getMozIcc() })
    // Switch cardState to "pukRequired" by entering wrong pin code.
    .then(() => passingWrongPinAndWait(icc))

    // Get current PUK-lock retry count.
    .then(() => icc.getCardLockRetryCount("puk"))
    .then((aResult) => {
      retryCount = aResult.retryCount;
      ok(true, "puk retryCount is " + retryCount);
    })

    // Test unlock PUK code fail.
    // The retry count should be decreased by 1.
    .then(() => testUnlockPuk(icc, "11111111", DEFAULT_PIN, "IncorrectPassword",
                              retryCount - 1))

    // Test unlock PUK code success.
    .then(() => testUnlockPukAndWait(icc, DEFAULT_PUK, DEFAULT_PIN, "ready"))

    // Restore pin state.
    .then(() => setPinLockEnabled(icc, false));
});
