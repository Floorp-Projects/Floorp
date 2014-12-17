/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const number = "0912345678";

startTestWithPermissions(['mobileconnection'], function() {
  let connection = navigator.mozMobileConnections[0];
  ok(connection instanceof MozMobileConnection,
     "connection is instanceof " + connection.constructor);

  let outCall;

  gSetRadioEnabled(connection, false)
    .then(() => gDial(number))
    .then(null, cause => { is(cause, "RadioNotAvailable"); })
    .then(() => gDialEmergency(number))
    .then(null, cause => { is(cause, "RadioNotAvailable"); })
    .then(() => gSetRadioEnabled(connection, true))
    .then(() => gDialEmergency(number))
    .then(null, cause => { is(cause, "BadNumberError"); })
    .then(finish);
});
