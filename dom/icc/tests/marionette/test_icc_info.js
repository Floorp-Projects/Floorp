/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;
MARIONETTE_HEAD_JS = "head.js";

/* Basic test */
function basicTest(aIcc) {
  let iccInfo = aIcc.iccInfo;

  // The emulator's hard coded iccid value.
  // See it here {B2G_HOME}/external/qemu/telephony/sim_card.c#L299.
  is(iccInfo.iccid, 89014103211118510720);

  if (iccInfo instanceof MozGsmIccInfo) {
    log("Test Gsm IccInfo");
    is(iccInfo.iccType, "sim");
    is(iccInfo.spn, "Android");
    // The emulator's hard coded mcc and mnc codes.
    // See it here {B2G_HOME}/external/qemu/telephony/android_modem.c#L2465.
    is(iccInfo.mcc, 310);
    is(iccInfo.mnc, 410);
    // Phone number is hardcoded in MSISDN
    // See {B2G_HOME}/external/qemu/telephony/sim_card.c, in asimcard_io().
    is(iccInfo.msisdn, "15555215554");
  } else {
    log("Test Cdma IccInfo");
    is(iccInfo.iccType, "ruim");
    // MDN is hardcoded as "8587777777".
    // See it here {B2G_HOME}/hardware/ril/reference-ril/reference-ril.c,
    // in requestCdmaSubscription().
    is(iccInfo.mdn, "8587777777");
    // PRL version is hardcoded as 1.
    // See it here {B2G_HOME}/hardware/ril/reference-ril/reference-ril.c,
    // in requestCdmaSubscription().
    is(iccInfo.prlVersion, 1);
  }
}

// Start tests
startTestCommon(function() {
  let icc = getMozIcc();

  return Promise.resolve()
    // Basic test
    .then(() => basicTest(icc))
    // Test iccInfo when card becomes undetected
    .then(() => {
      let promises = [];
      promises.push(setRadioEnabled(false));
      promises.push(waitForTargetEvent(icc, "iccinfochange", function() {
        // Expect iccInfo changes to null
        return icc.iccInfo === null;
      }));
      return Promise.all(promises);
    })
    // Restore radio status and expect to get iccdetected event.
    .then(() => {
      let promises = [];
      promises.push(setRadioEnabled(true));
      promises.push(waitForTargetEvent(iccManager, "iccdetected"));
      return Promise.all(promises);
    });
});
