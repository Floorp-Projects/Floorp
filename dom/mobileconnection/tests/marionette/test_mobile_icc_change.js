/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

// The emulator's hard coded iccid value.
const ICCID = "89014103211118510720";

function setRadioEnabledAndWaitIccChange(aEnabled) {
  let promises = [];
  promises.push(waitForManagerEvent("iccchange"));
  promises.push(setRadioEnabled(aEnabled));

  return Promise.all(promises);
}

// Start tests
startTestCommon(function() {
  is(mobileConnection.iccId, ICCID, "test initial iccId");

  return setRadioEnabledAndWaitIccChange(false)
    .then(() => {
      is(mobileConnection.iccId, null, "mobileConnection.iccId");
    })

    // Restore radio state.
    .then(() => setRadioEnabledAndWaitIccChange(true))
    .then(() => {
      is(mobileConnection.iccId, ICCID, "mobileConnection.iccId");

      // ICC object should be ready in mozIccManager.
      let icc = getMozIccByIccId(mobileConnection.iccId);
      ok(icc instanceof MozIcc, "icc should be an instance of MozIcc");
      is(icc.iccInfo.iccid, mobileConnection.iccId, "icc.iccInfo.iccid");
    });
});
