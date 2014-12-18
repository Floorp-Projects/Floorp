/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

// Make an outgoing call then power off radio.
startTestWithPermissions(['mobileconnection'], function() {
  let connection = navigator.mozMobileConnections[0];
  ok(connection instanceof MozMobileConnection,
     "connection is instanceof " + connection.constructor);

  let outCall;

  gDial("0912345000")
    .then(call => outCall = call)
    .then(() => gRemoteAnswer(outCall))
    .then(() => {
      let p1 = gWaitForNamedStateEvent(outCall, "disconnected");
      let p2 = gSetRadioEnabled(connection, false);
      return Promise.all([p1, p2]);
    })
    .then(() => gSetRadioEnabled(connection, true))
    .catch(error => ok(false, "Promise reject: " + error))
    .then(finish);
});
