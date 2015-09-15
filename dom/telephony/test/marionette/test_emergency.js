/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

var outCall;

function testEmergencyNumber() {
  return gDialEmergency("911")
    .then(call => outCall = call)
    .then(() => gRemoteAnswer(outCall))
    .then(() => gHangUp(outCall));
}

function testNormalNumber() {
  return gDialEmergency("0912345678")
    .catch(cause => {
      is(cause, "BadNumberError");
      return gCheckAll(null, [], "", [], []);
    });
}

function testBadNumber() {
  return gDialEmergency("not a valid emergency number")
    .catch(cause => {
      is(cause, "BadNumberError");
      return gCheckAll(null, [], "", [], []);
    });
}

startTest(function() {
  Promise.resolve()
    .then(() => testEmergencyNumber())
    .then(() => testNormalNumber())
    .then(() => testBadNumber())
    .catch(error => ok(false, "Promise reject: " + error))
    .then(finish);
});
