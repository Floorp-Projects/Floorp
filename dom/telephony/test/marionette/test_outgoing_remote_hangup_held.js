/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

SpecialPowers.addPermission("telephony", true, document);

let telephony = window.navigator.mozTelephony;
let outNumber = "5555551111";
let outgoingCall;

function verifyInitialState() {
  log("Verifying initial state.");
  ok(telephony);
  is(telephony.active, null);
  ok(telephony.calls);
  is(telephony.calls.length, 0);

  runEmulatorCmd("gsm list", function(result) {
    log("Initial call list: " + result);
    is(result[0], "OK");
    if (result[0] == "OK") {
      dial();
    } else {
      log("Call exists from a previous test, failing out.");
      cleanUp();
    }
  });
}

function dial() {
  log("Make an outgoing call.");

  outgoingCall = telephony.dial(outNumber);
  ok(outgoingCall);
  is(outgoingCall.number, outNumber);
  is(outgoingCall.state, "dialing");

  is(outgoingCall, telephony.active);
  is(telephony.calls.length, 1);
  is(telephony.calls[0], outgoingCall);

  outgoingCall.onalerting = function onalerting(event) {
    log("Received 'alerting' call event.");

    is(outgoingCall, event.call);
    is(outgoingCall.state, "alerting");

    runEmulatorCmd("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "outbound to  " + outNumber + " : ringing");
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

    runEmulatorCmd("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "outbound to  " + outNumber + " : active");
      is(result[1], "OK");
      hold();
    });
  };
  runEmulatorCmd("gsm accept " + outNumber);
};

function hold() {
  log("Holding the outgoing call.");

  outgoingCall.onholding = function onholding(event) {
    log("Received 'holding' call event.");
    is(outgoingCall, event.call);
    is(outgoingCall.state, "holding");

    is(outgoingCall, telephony.active);
  };

  outgoingCall.onheld = function onheld(event) {
    log("Received 'held' call event.");
    is(outgoingCall, event.call);
    is(outgoingCall.state, "held");

    is(telephony.active, null);
    is(telephony.calls.length, 1);
    is(telephony.calls[0], outgoingCall);

    runEmulatorCmd("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "outbound to  " + outNumber + " : held");
      is(result[1], "OK");
      hangUp();
    });
  };
  outgoingCall.hold();
}

function hangUp() {
  log("Hanging up the outgoing call (remotely).");

  // We get no 'disconnecting' event when remote party hangs-up the call
  
  outgoingCall.ondisconnected = function ondisconnected(event) {
    log("Received 'disconnected' call event.");
    is(outgoingCall, event.call);
    is(outgoingCall.state, "disconnected");

    is(telephony.active, null);
    is(telephony.calls.length, 0);

    runEmulatorCmd("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "OK");
      cleanUp();
    });
  };
  runEmulatorCmd("gsm cancel " + outNumber);
}

function cleanUp() {
  SpecialPowers.removePermission("telephony", document);
  finish();
}

// Start the test
verifyInitialState();
