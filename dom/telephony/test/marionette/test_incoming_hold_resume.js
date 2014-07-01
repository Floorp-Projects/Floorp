/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

let number = "5555551234";
let connectedCalls;
let incomingCall;

function simulateIncoming() {
  log("Simulating an incoming call.");

  telephony.onincoming = function onincoming(event) {
    log("Received 'incoming' call event.");
    incomingCall = event.call;
    ok(incomingCall);
    is(incomingCall.id.number, number);
    is(incomingCall.state, "incoming");

    is(telephony.calls.length, 1);
    is(telephony.calls[0], incomingCall);

    emulator.runWithCallback("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "inbound from " + number + " : incoming");
      answer();
    });
  };
  emulator.runWithCallback("gsm call " + number);
}

function answer() {
  log("Answering the incoming call.");

  let gotConnecting = false;
  incomingCall.onconnecting = function onconnecting(event) {
    log("Received 'connecting' call event.");
    is(incomingCall, event.call);
    is(incomingCall.state, "connecting");
    gotConnecting = true;
  };

  incomingCall.onconnected = function onconnected(event) {
    log("Received 'connected' call event.");
    is(incomingCall, event.call);
    is(incomingCall.state, "connected");
    ok(gotConnecting);

    is(incomingCall, telephony.active);

    emulator.runWithCallback("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "inbound from " + number + " : active");
      hold();
    });
  };
  incomingCall.answer();
}

function hold() {
  log("Putting the call on hold.");

  let gotHolding = false;
  incomingCall.onholding = function onholding(event) {
    log("Received 'holding' call event");
    is(incomingCall, event.call);
    is(incomingCall.state, "holding");
    gotHolding = true;
  };

  incomingCall.onheld = function onheld(event) {
    log("Received 'held' call event");
    is(incomingCall, event.call);
    is(incomingCall.state, "held");
    ok(gotHolding);

    is(telephony.active, null);
    is(telephony.calls.length, 1);
    is(telephony.calls[0], incomingCall);

    emulator.runWithCallback("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "inbound from " + number + " : held");
      // Wait on hold for a couple of seconds
      log("Pausing 2 seconds while on hold");
      setTimeout(resume, 2000);
    });
  };
  incomingCall.hold();
}

function resume() {
  log("Resuming the held call.");

  let gotResuming = false;
  incomingCall.onresuming = function onresuming(event) {
    log("Received 'resuming' call event");
    is(incomingCall, event.call);
    is(incomingCall.state, "resuming");
    gotResuming = true;
  };

  incomingCall.onconnected = function onconnected(event) {
    log("Received 'connected' call event");
    is(incomingCall, event.call);
    is(incomingCall.state, "connected");
    ok(gotResuming);

    is(incomingCall, telephony.active);
    is(telephony.calls.length, 1);
    is(telephony.calls[0], incomingCall);

    emulator.runWithCallback("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "inbound from " + number + " : active");
      hangUp();
    });
  };
  incomingCall.resume();
}

function hangUp() {
  log("Hanging up the incoming call.");

  let gotDisconnecting = false;
  incomingCall.ondisconnecting = function ondisconnecting(event) {
    log("Received 'disconnecting' call event.");
    is(incomingCall, event.call);
    is(incomingCall.state, "disconnecting");
    gotDisconnecting = true;
  };

  incomingCall.ondisconnected = function ondisconnected(event) {
    log("Received 'disconnected' call event.");
    is(incomingCall, event.call);
    is(incomingCall.state, "disconnected");
    ok(gotDisconnecting);

    is(telephony.active, null);
    is(telephony.calls.length, 0);

    emulator.runWithCallback("gsm list", function(result) {
      log("Call list is now: " + result);
      cleanUp();
    });
  };
  incomingCall.hangUp();
}

function cleanUp() {
  telephony.onincoming = null;
  finish();
}

startTest(function() {
  simulateIncoming();
});
