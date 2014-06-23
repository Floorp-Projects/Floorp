/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

let number = "not a valid emergency number";

function dial() {
  log("Make an outgoing call to an invalid number.");

  telephony.dialEmergency(number).then(null, cause => {
    log("Received promise 'reject'");

    is(telephony.active, null);
    is(telephony.calls.length, 0);
    is(cause, "BadNumberError");

    emulator.runWithCallback("gsm list", function(result) {
      log("Initial call list: " + result);
      cleanUp();
    });
  });
}

function cleanUp() {
  finish();
}

startTest(function() {
  dial();
});
