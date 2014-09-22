/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

function testGettingIMEI() {
  log("Test *#06# ...");

  let MMI_CODE = "*#06#";
  return sendMMI(MMI_CODE)
    .then(function resolve(aResult) {
      ok(true, MMI_CODE + " success");
      is(aResult.serviceCode, "scImei", "Service code IMEI");
      // IMEI is hardcoded as "000000000000000".
      // See it here {B2G_HOME}/external/qemu/telephony/android_modem.c
      // (The result of +CGSN).
      is(aResult.statusMessage, "000000000000000", "Emulator IMEI");
      is(aResult.additionalInformation, undefined, "No additional information");
    }, function reject() {
      ok(false, MMI_CODE + " should not fail");
    });
}

// Start test
startTest(function() {
  Promise.resolve()
    .then(() => testGettingIMEI())
    .then(finish);
});
