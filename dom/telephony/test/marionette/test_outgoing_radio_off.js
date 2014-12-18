/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

let connection;
const normalNumber = "0912345678";
const emergencyNumber = "112";
let outCall;

function testDial_NormalNumber() {
  return gSetRadioEnabled(connection, false)
    .then(() => gDial(normalNumber))
    .catch(cause => {
      is(cause, "RadioNotAvailable");
      return gCheckAll(null, [], "", [], []);
    });
}

function testDial_EmergencyNumber() {
  return gSetRadioEnabled(connection, false)
    .then(() => gDial(emergencyNumber))
    .then(call => { outCall = call; })
    .then(() => gRemoteAnswer(outCall))
    .then(() => gDelay(1000))  // See Bug 1018051 for the purpose of the delay.
    .then(() => gRemoteHangUp(outCall));
}

function testDialEmergency_NormalNumber() {
  return gSetRadioEnabled(connection, false)
    .then(() => gDialEmergency(normalNumber))
    .catch(cause => {
      is(cause, "RadioNotAvailable");
      return gCheckAll(null, [], "", [], []);
    });
}

function testDialEmergency_EmergencyNumber() {
  return gSetRadioEnabled(connection, false)
    .then(() => gDialEmergency(emergencyNumber))
    .then(call => { outCall = call; })
    .then(() => gRemoteAnswer(outCall))
    .then(() => gDelay(1000))  // See Bug 1018051 for the purpose of the delay.
    .then(() => gRemoteHangUp(outCall));
}

startTestWithPermissions(['mobileconnection'], function() {
  connection = navigator.mozMobileConnections[0];
  ok(connection instanceof MozMobileConnection,
     "connection is instanceof " + connection.constructor);

  Promise.resolve()
    .then(() => testDial_NormalNumber())
    .then(() => testDial_EmergencyNumber())
    .then(() => testDialEmergency_NormalNumber())
    .then(() => testDialEmergency_EmergencyNumber())
    .then(() => gSetRadioEnabled(connection, true))
    .catch(error => ok(false, "Promise reject: " + error))
    .then(finish);
});
