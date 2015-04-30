/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

function sendUnlockPuk2Mmi(aPuk2, aNewPin2, aNewPin2Again) {
  let MMI_CODE = "**052*" + aPuk2 + "*" + aNewPin2 + "*" + aNewPin2Again + "#";
  log("Test " + MMI_CODE);

  return gSendMMI(MMI_CODE);
}

function testUnlockPuk2MmiError(aPuk2, aNewPin2, aNewPin2Again, aErrorName) {
  return sendUnlockPuk2Mmi(aPuk2, aNewPin2, aNewPin2Again)
    .then((aResult) => {
      ok(!aResult.success, "Check success");
      is(aResult.serviceCode, "scPuk2", "Check service code");
      is(aResult.statusMessage, aErrorName, "Check statusMessage");
      is(aResult.additionalInformation, null, "Check additional information");
    });
}

// Start test
startTest(function() {
  return Promise.resolve()
    // Test passing no puk2.
    .then(() => testUnlockPuk2MmiError("", "1111", "2222", "emMmiError"))
    // Test passing no newPin2.
    .then(() => testUnlockPuk2MmiError("11111111", "", "", "emMmiError"))
    // Test passing mismatched newPin2.
    .then(() => testUnlockPuk2MmiError("11111111", "1111", "2222",
                                       "emMmiErrorMismatchPin"))
    // Test passing invalid puk2 (> 8 digit).
    .then(() => testUnlockPuk2MmiError("123456789", "0000", "0000",
                                       "emMmiErrorInvalidPin"))
    // Test passing valid puk2 and newPin2. But currently emulator doesn't
    // support RIL_REQUEST_ENTER_SIM_PUK2, so we expect to get a
    // 'RequestNotSupported' error here.
    .then(() => testUnlockPuk2MmiError("11111111", "0000", "0000",
                                       "RequestNotSupported"))
    .catch(error => ok(false, "Promise reject: " + error))
    .then(finish);
});
