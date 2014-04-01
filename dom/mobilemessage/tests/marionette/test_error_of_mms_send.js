/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const kPrefRilRadioDisabled  = "ril.radio.disabled";

function testSendFailed(aCause, aServiceId) {
  log("testSendFailed, aCause: " + aCause + ", aServiceId: " + aServiceId);
  let sendParameters;

  if (aServiceId) {
    sendParameters = { serviceId: aServiceId };
  }

  let mmsParameters = { subject: "Test",
                        receivers: ["+0987654321"],
                        attachments: [] };

  return sendMmsWithFailure(mmsParameters, sendParameters)
    .then((result) => {
      is(result.error.name, aCause, "Checking failure cause.");
    });
}

startTestCommon(function testCaseMain() {
  return Promise.resolve()
    .then(() => {
      SpecialPowers.setBoolPref(kPrefRilRadioDisabled, true);
    })
    .then(() => testSendFailed("RadioDisabledError"))
    .then(() => {
      SpecialPowers.setBoolPref(kPrefRilRadioDisabled, false);
    })
    .then(() => runIfMultiSIM(
                  () => testSendFailed("NonActiveSimCardError", 1)));
});
