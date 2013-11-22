/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

let connection;
let number = "112";
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

function dial() {
  log("Make an outgoing call.");

  outgoing = telephony.dial(number);
  ok(outgoing);
  is(outgoing.number, number);
  is(outgoing.state, "dialing");

  is(outgoing, telephony.active);
  is(telephony.calls.length, 1);
  is(telephony.calls[0], outgoing);

  outgoing.onalerting = function onalerting(event) {
    log("Received 'onalerting' call event.");
    is(outgoing, event.call);
    is(outgoing.state, "alerting");

    emulator.run("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "outbound to  " + number + "        : ringing");
      is(result[1], "OK");
      answer();
    });
  };
}

function answer() {
  log("Answering the outgoing call.");

  // We get no "connecting" event when the remote party answers the call.

  outgoing.onconnected = function onconnected(event) {
    log("Received 'connected' call event.");
    is(outgoing, event.call);
    is(outgoing.state, "connected");

    is(outgoing, telephony.active);

    emulator.run("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "outbound to  " + number + "        : active");
      is(result[1], "OK");
      hangUp();
    });
  };
  emulator.run("gsm accept " + number);
}

function hangUp() {
  log("Hanging up the outgoing call.");

  // We get no "disconnecting" event when the remote party terminates the call.

  outgoing.ondisconnected = function ondisconnected(event) {
    log("Received 'disconnected' call event.");
    is(outgoing, event.call);
    is(outgoing.state, "disconnected");

    is(telephony.active, null);
    is(telephony.calls.length, 0);

    emulator.run("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "OK");
      cleanUp();
    });
  };
  emulator.run("gsm cancel " + number);
}

function cleanUp() {
  finish();
}

startTestWithPermissions(['mobileconnection'], function() {
  connection = navigator.mozMobileConnections[0];
  ok(connection instanceof MozMobileConnection,
     "connection is instanceof " + connection.constructor);

  setRadioEnabled(false, function() {
    dial();
  });
});
