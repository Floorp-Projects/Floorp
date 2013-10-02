/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

SpecialPowers.addPermission("telephony", true, document);

let telephony = window.navigator.mozTelephony;
let number = "5555557777";
let connectedCalls;
let outgoingCall;

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
        dial();
      } else {
        log("Call exists from a previous test, failing out.");
        cleanUp();
      }
    });
  } else {
    dial();
  }
}

function dial() {
  log("Make an outgoing call.");

  outgoingCall = telephony.dial(number);
  ok(outgoingCall);
  is(outgoingCall.number, number);
  is(outgoingCall.state, "dialing");

  is(outgoingCall, telephony.active);
  is(telephony.calls.length, 1);
  is(telephony.calls[0], outgoingCall);

  outgoingCall.onalerting = function onalerting(event) {
    log("Received 'onalerting' call event.");
    is(outgoingCall, event.call);
    is(outgoingCall.state, "alerting");

    emulator.run("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "outbound to  " + number + " : ringing");
      is(result[1], "OK");
      answer();
    });
  };
}

function answer() {
  log("Answering the outgoing call.");

  // We get no "connecting" event when the remote party answers the call.

  outgoingCall.onconnected = function onconnected(event) {
    log("Received 'connected' call event.");
    is(outgoingCall, event.call);
    is(outgoingCall.state, "connected");

    is(outgoingCall, telephony.active);

    emulator.run("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "outbound to  " + number + " : active");
      is(result[1], "OK");
      hold();
    });
  };
  emulator.run("gsm accept " + number);
}

function hold() {
  log("Putting the call on hold.");

  let gotHolding = false;
  outgoingCall.onholding = function onholding(event) {
    log("Received 'holding' call event");
    is(outgoingCall, event.call);
    is(outgoingCall.state, "holding");
    gotHolding = true;
  };

  outgoingCall.onheld = function onheld(event) {
    log("Received 'held' call event");
    is(outgoingCall, event.call);
    is(outgoingCall.state, "held");
    ok(gotHolding);

    is(telephony.active, null);
    is(telephony.calls.length, 1);
    is(telephony.calls[0], outgoingCall);

    emulator.run("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "outbound to  " + number + " : held");
      is(result[1], "OK");
      // Bug 781604: emulator assertion if outgoing call kept on hold
      // Wait on hold for a couple of seconds
      //log("Pausing 2 seconds while on hold");
      //setTimeout(resume, 2000);
      resume();
    });
  };
  outgoingCall.hold();
}

function resume() {
  log("Resuming the held call.");

  let gotResuming = false;
  outgoingCall.onresuming = function onresuming(event) {
    log("Received 'resuming' call event");
    is(outgoingCall, event.call);
    is(outgoingCall.state, "resuming");
    gotResuming = true;
  };

  outgoingCall.onconnected = function onconnected(event) {
    log("Received 'connected' call event");
    is(outgoingCall, event.call);
    is(outgoingCall.state, "connected");
    ok(gotResuming);

    is(outgoingCall, telephony.active);
    is(telephony.calls.length, 1);
    is(telephony.calls[0], outgoingCall);

    emulator.run("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "outbound to  " + number + " : active");
      is(result[1], "OK");
      hangUp();
    });
  };
  outgoingCall.resume();
}

function hangUp() {
  log("Hanging up the outgoing call.");

  let gotDisconnecting = false;
  outgoingCall.ondisconnecting = function ondisconnecting(event) {
    log("Received 'disconnecting' call event.");
    is(outgoingCall, event.call);
    is(outgoingCall.state, "disconnecting");
    gotDisconnecting = true;
  };

  outgoingCall.ondisconnected = function ondisconnected(event) {
    log("Received 'disconnected' call event.");
    is(outgoingCall, event.call);
    is(outgoingCall.state, "disconnected");
    ok(gotDisconnecting);

    is(telephony.active, null);
    is(telephony.calls.length, 0);

    emulator.run("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "OK");
      cleanUp();
    });
  };
  outgoingCall.hangUp();
}

function cleanUp() {
  SpecialPowers.removePermission("telephony", document);
  finish();
}

// Start the test
startTest(function() {
  getExistingCalls();
});
