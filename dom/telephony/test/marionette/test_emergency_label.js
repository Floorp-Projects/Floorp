/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

function testEmergencyLabel(number, emergency) {
  log("= testEmergencyLabel = " + number + " should be " +
      (emergency ? "emergency" : "normal") + " call");

  let outCall;

  return gDial(number)
    .then(call => { outCall = call; })
    .then(() => {
      is(outCall.emergency, emergency, "emergency result should be correct");
    })
    .then(() => gRemoteAnswer(outCall))
    .then(() => {
      is(outCall.emergency, emergency, "emergency result should be correct");
    })
    .then(() => gRemoteHangUp(outCall));
}

startTest(function() {
  testEmergencyLabel("112", true)
    .then(() => testEmergencyLabel("911", true))
    .then(() => testEmergencyLabel("0912345678", false))
    .then(() => testEmergencyLabel("777", false))
    .then(null, () => {
      ok(false, 'promise rejects during test.');
    })
    .then(finish);
});
