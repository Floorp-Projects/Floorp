/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

startTestWithPermissions(['mobileconnection'], function() {
  let connection = navigator.mozMobileConnections[0];
  ok(connection instanceof MozMobileConnection,
     "connection is instanceof " + connection.constructor);

  let outCall;

  gSetRadioEnabled(connection, false)
    .then(() => gDial("112"))
    .then(call => { outCall = call; })
    .then(() => gRemoteAnswer(outCall))
    .then(() => gDelay(1000))  // See Bug 1018051 for the purpose of the delay.
    .then(() => gRemoteHangUp(outCall))
    .catch(error => ok(false, "Promise reject: " + error))
    .then(finish);
});
