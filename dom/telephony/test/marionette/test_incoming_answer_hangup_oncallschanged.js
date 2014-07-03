/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

let number = "5555552368";
let incoming;

function simulateIncoming() {
  log("Simulating an incoming call.");

  telephony.oncallschanged = function oncallschanged(event) {
    log("Received 'callschanged' event.");

    if (!event.call) {
      log("Notifying calls array is loaded. No call information accompanies.");
      return;
    }

    telephony.oncallschanged = null;

    incoming = event.call;
    ok(incoming);
    is(incoming.id.number, number);
    is(incoming.state, "incoming");

    is(telephony.calls.length, 1);
    is(telephony.calls[0], incoming);

    emulator.runCmdWithCallback("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "inbound from " + number + " : incoming");
      answer();
    });
  };

  emulator.runCmdWithCallback("gsm call " + number);
}

function answer() {
  log("Answering the incoming call.");

  let gotConnecting = false;
  incoming.onconnecting = function onconnecting(event) {
    log("Received 'connecting' call event.");
    is(incoming, event.call);
    is(incoming.state, "connecting");

    // Incoming call is not 'active' until its state becomes 'connected'.
    isnot(incoming, telephony.active);
    gotConnecting = true;
  };

  incoming.onconnected = function onconnected(event) {
    log("Received 'connected' call event.");
    is(incoming, event.call);
    is(incoming.state, "connected");
    ok(gotConnecting);

    is(incoming, telephony.active);

    emulator.runCmdWithCallback("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "inbound from " + number + " : active");
      hangUp();
    });
  };
  incoming.answer();
}

function hangUp() {
  log("Hanging up the incoming call.");

  // Should received 'diconnecting', 'callschanged', 'disconnected' events in
  // order.
  let gotDisconnecting = false;
  let gotCallschanged = false;

  incoming.ondisconnecting = function ondisconnecting(event) {
    log("Received 'disconnecting' call event.");
    is(incoming, event.call);
    is(incoming.state, "disconnecting");
    gotDisconnecting = true;
  };

  telephony.oncallschanged = function oncallschanged(event) {
    log("Received 'callschanged' event.");

    if (!event.call) {
      log("Notifying calls array is loaded. No call information accompanies.");
      return;
    }

    is(incoming, event.call);
    is(incoming.state, "disconnected");
    is(telephony.active, null);
    is(telephony.calls.length, 0);
    gotCallschanged = true;
  };

  incoming.ondisconnected = function ondisconnected(event) {
    log("Received 'disconnected' call event.");
    is(incoming, event.call);
    is(incoming.state, "disconnected");
    ok(gotDisconnecting);
    ok(gotCallschanged);

    is(telephony.active, null);
    is(telephony.calls.length, 0);

    emulator.runCmdWithCallback("gsm list", function(result) {
      log("Call list is now: " + result);
      cleanUp();
    });
  };

  incoming.hangUp();
}

function cleanUp() {
  telephony.oncallschanged = null;
  finish();
}

startTest(function() {
  simulateIncoming();
});
