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
  log("Test initial iccId");
  is(mobileConnection.iccId, ICCID);

  return setRadioEnabledAndWaitIccChange(false)
    .then(() => {
      is(mobileConnection.iccId, null);
    })

    // Restore radio state.
    .then(() => setRadioEnabledAndWaitIccChange(true))
    .then(() => {
      is(mobileConnection.iccId, ICCID);
    });
});
