/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

let connection;

function setRadioEnabled(enabled) {
  let desiredRadioState = enabled ? 'enabled' : 'disabled';
  log("Set radio: " + desiredRadioState);

  let promises = [];

  let promise = gWaitForEvent(connection, "radiostatechange", event => {
    let state = connection.radioState;
    log("current radioState: " + state);
    return state == desiredRadioState;
  });
  promises.push(promise);

  promises.push(connection.setRadioEnabled(enabled));

  return Promise.all(promises);
}

startTestWithPermissions(['mobileconnection'], function() {
  connection = navigator.mozMobileConnections[0];
  ok(connection instanceof MozMobileConnection,
     "connection is instanceof " + connection.constructor);

  setRadioEnabled(false)
    .then(() => gDial("0912345678"))
    .catch(cause => {
      is(telephony.active, null);
      is(telephony.calls.length, 0);
      is(cause, "RadioNotAvailable");
    })
    .then(() => setRadioEnabled(true))
    .catch(error => ok(false, "Promise reject: " + error))
    .then(finish);
});
