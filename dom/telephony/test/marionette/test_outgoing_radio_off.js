/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

startTestWithPermissions(['mobileconnection'], function() {
  let connection = navigator.mozMobileConnections[0];
  ok(connection instanceof MozMobileConnection,
     "connection is instanceof " + connection.constructor);

  gSetRadioEnabled(connection, false)
    .then(() => gDial("0912345678"))
    .catch(cause => {
      is(telephony.active, null);
      is(telephony.calls.length, 0);
      is(cause, "RadioNotAvailable");
    })
    .then(() => gSetRadioEnabled(connection, true))
    .catch(error => ok(false, "Promise reject: " + error))
    .then(finish);
});
