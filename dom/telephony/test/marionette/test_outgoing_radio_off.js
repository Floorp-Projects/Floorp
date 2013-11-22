/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

let connection;
let outgoing;

function receivedPending(received, pending, nextAction) {
  let index = pending.indexOf(received);
  if (index != -1) {
    pending.splice(index, 1);
  }
  if (pending.length === 0) {
    nextAction();
  }
}

function setRadioEnabled(enabled, callback) {
  let request  = connection.setRadioEnabled(enabled);
  let desiredRadioState = enabled ? 'enabled' : 'disabled';

  let pending = ['onradiostatechange', 'onsuccess'];
  let done = callback;

  connection.onradiostatechange = function() {
    let state = connection.radioState;
    log("Received 'radiostatechange' event, radioState: " + state);

    if (state == desiredRadioState) {
      receivedPending('onradiostatechange', pending, done);
    }
  };

  request.onsuccess = function onsuccess() {
    receivedPending('onsuccess', pending, done);
  };

  request.onerror = function onerror() {
    ok(false, "setRadioEnabled should be ok");
  };
}

function dial(number) {
  // Verify initial state before dial.
  ok(telephony);
  is(telephony.active, null);
  ok(telephony.calls);
  is(telephony.calls.length, 0);

  log("Make an outgoing call.");
  outgoing = telephony.dial(number);

  ok(outgoing);
  is(outgoing.number, number);
  is(outgoing.state, "dialing");

  is(telephony.active, outgoing);
  is(telephony.calls.length, 1);
  is(telephony.calls[0], outgoing);

  outgoing.onerror = function onerror(event) {
    log("Received 'error' event.");
    is(event.call, outgoing);
    ok(event.call.error);
    is(event.call.error.name, "RadioNotAvailable");

    emulator.run("gsm list", function(result) {
      log("Initial call list: " + result);
      is(result[0], "OK");

      setRadioEnabled(true, cleanUp);
    });
  };
}

function cleanUp() {
  finish();
}

startTestWithPermissions(['mobileconnection'], function() {
  connection = navigator.mozMobileConnections[0];
  ok(connection instanceof MozMobileConnection,
     "connection is instanceof " + connection.constructor);

  setRadioEnabled(false, function() {
    dial("0912345678");
  });
});
