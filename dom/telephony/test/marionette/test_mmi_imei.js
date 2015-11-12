/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

function testGettingIMEI() {
  log("Test *#06# ...");

  return gSendMMI("*#06#").then(aResult => {
    ok(aResult.success, "success");
    is(aResult.serviceCode, "scImei", "Service code IMEI");
    // IMEI is hardcoded as "000000000000000".
    // See it here {B2G_HOME}/external/qemu/telephony/android_modem.c
    // (The aResult of +CGSN).
    is(aResult.statusMessage, "000000000000000", "Emulator IMEI");
    is(aResult.additionalInformation, undefined, "No additional information");
  });
}

// Start test
startTestWithPermissions(['mobileconnection'], function() {
  Promise.resolve()
    .then(() => gSetRadioEnabledAll(false))
    .then(() => testGettingIMEI())
    .then(() => gSetRadioEnabledAll(true))
    .then(() => testGettingIMEI())
    .catch(error => ok(false, "Promise reject: " + error))
    .then(finish);
});
