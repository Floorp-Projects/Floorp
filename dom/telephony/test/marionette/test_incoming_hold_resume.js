/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

SpecialPowers.addPermission("telephony", true, document);

let telephony = window.navigator.mozTelephony;
let number = "5555551234";
let connectedCalls;
let incomingCall;

function getExistingCalls() {
  emulator.run("gsm list", function(result) {
    log("Initial call list: " + result);
    if (result[0] == "OK") {
      verifyInitialState(false);
    } else {
      cancelExistingCalls(result);
    }
  });
}

function cancelExistingCalls(callList) {
  if (callList.length && callList[0] != "OK") {
    // Existing calls remain; get rid of the next one in the list
    nextCall = callList.shift().split(/\s+/)[2].trim();
    log("Cancelling existing call '" + nextCall +"'");
    emulator.run("gsm cancel " + nextCall, function(result) {
      if (result[0] == "OK") {
        cancelExistingCalls(callList);
      } else {
        log("Failed to cancel existing call");
        cleanUp();
      }
    });
  } else {
    // No more calls in the list; give time for emulator to catch up
    waitFor(verifyInitialState, function() {
      return (telephony.calls.length === 0);
    });
  }
}

function verifyInitialState(confirmNoCalls = true) {
  log("Verifying initial state.");
  ok(telephony);
  is(telephony.active, null);
  ok(telephony.calls);
  is(telephony.calls.length, 0);
  if (confirmNoCalls) {
    emulator.run("gsm list", function(result) {
    log("Initial call list: " + result);
      is(result[0], "OK");
      if (result[0] == "OK") {
        simulateIncoming();
      } else {
        log("Call exists from a previous test, failing out.");
        cleanUp();
      }
    });
  } else {
    simulateIncoming();
  }
}

function simulateIncoming() {
  log("Simulating an incoming call.");

  telephony.onincoming = function onincoming(event) {
    log("Received 'incoming' call event.");
    incomingCall = event.call;
    ok(incomingCall);
    is(incomingCall.number, number);
    is(incomingCall.state, "incoming");

    is(telephony.calls.length, 1);
    is(telephony.calls[0], incomingCall);

    emulator.run("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "inbound from " + number + " : incoming");
      is(result[1], "OK");
      answer();
    });
  };
  emulator.run("gsm call " + number);
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

    emulator.run("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "inbound from " + number + " : active");
      is(result[1], "OK");
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

    emulator.run("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "inbound from " + number + " : held");
      is(result[1], "OK");
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

    emulator.run("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "inbound from " + number + " : active");
      is(result[1], "OK");
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

    emulator.run("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "OK");
      cleanUp();
    });
  };
  incomingCall.hangUp();
}

function cleanUp() {
  telephony.onincoming = null;
  SpecialPowers.removePermission("telephony", document);
  finish();
}

// Start the test
startTest(function() {
  getExistingCalls();
});
