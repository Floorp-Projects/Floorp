/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

let inNumber = "5555551111";
let outNumber = "5555552222";
let incomingCall;
let outgoingCall;

function simulateIncoming() {
  log("Simulating an incoming call.");

  telephony.onincoming = function onincoming(event) {
    log("Received 'incoming' call event.");
    incomingCall = event.call;
    ok(incomingCall);
    is(incomingCall.id.number, inNumber);
    is(incomingCall.state, "incoming");

    is(telephony.calls.length, 1);
    is(telephony.calls[0], incomingCall);

    // Wait for emulator to catch up before continuing
    waitFor(verifyCallList,function() {
      return(rcvdEmulatorCallback);
    });
  };

  let rcvdEmulatorCallback = false;
  emulator.run("gsm call " + inNumber, function(result) {
    is(result[0], "OK", "emulator callback");
    rcvdEmulatorCallback = true;
  });
}

function verifyCallList(){
  emulator.run("gsm list", function(result) {
    log("Call list is now: " + result);
    is(result[0], "inbound from " + inNumber + " : incoming");
    is(result[1], "OK");
    answerIncoming();
  });
}

function answerIncoming() {
  log("Answering the incoming call.");

  let gotConnecting = false;
  incomingCall.onconnecting = function onconnectingIn(event) {
    log("Received 'connecting' call event for original (incoming) call.");
    is(incomingCall, event.call);
    is(incomingCall.state, "connecting");
    gotConnecting = true;
  };

  incomingCall.onconnected = function onconnectedIn(event) {
    log("Received 'connected' call event for original (incoming) call.");
    is(incomingCall, event.call);
    is(incomingCall.state, "connected");
    ok(gotConnecting);

    is(incomingCall, telephony.active);

    emulator.run("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "inbound from " + inNumber + " : active");
      is(result[1], "OK");
      holdCall();
    });
  };
  incomingCall.answer();
}

// Put the original (incoming) call on hold
function holdCall(){
  log("Putting the original (incoming) call on hold.");

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
      is(result[0], "inbound from " + inNumber + " : held");
      is(result[1], "OK");
      dial();
    });
  };
  incomingCall.hold();
}

// With one call on hold, make outgoing call
function dial() {
  log("Making an outgoing call (while have one call already held).");

  telephony.dial(outNumber).then(call => {
    outgoingCall = call;
    ok(outgoingCall);
    is(outgoingCall.id.number, outNumber);
    is(outgoingCall.state, "dialing");

    is(outgoingCall, telephony.active);
    is(telephony.calls.length, 2);
    is(telephony.calls[0], incomingCall);
    is(telephony.calls[1], outgoingCall);

    outgoingCall.onalerting = function onalerting(event) {
      log("Received 'onalerting' call event.");
      is(outgoingCall, event.call);
      is(outgoingCall.state, "alerting");

      emulator.run("gsm list", function(result) {
        log("Call list is now: " + result);
        is(result[0], "inbound from " + inNumber + " : held");
        is(result[1], "outbound to  " + outNumber + " : ringing");
        is(result[2], "OK");
        answerOutgoing();
      });
    };
  });
}

// Have the outgoing call answered
function answerOutgoing() {
  log("Answering the outgoing/2nd call");

  // We get no "connecting" event when the remote party answers the call.
  outgoingCall.onconnected = function onconnectedOut(event) {
    log("Received 'connected' call event for outgoing/2nd call.");
    is(outgoingCall, event.call);
    is(outgoingCall.state, "connected");

    is(outgoingCall, telephony.active);

    // Wait for emulator to catch up before continuing
    waitFor(checkCallList,function() {
      return(rcvdEmulatorCallback);
    });
  };

  let rcvdEmulatorCallback = false;
  emulator.run("gsm accept " + outNumber, function(result) {
    is(result[0], "OK", "emulator callback");
    rcvdEmulatorCallback = true;
  });
}

function checkCallList(){
  emulator.run("gsm list", function(result) {
    log("Call list is now: " + result);
    is(result[0], "inbound from " + inNumber + " : held");
    is(result[1], "outbound to  " + outNumber + " : active");
    is(result[2], "OK");
    hangUpIncoming();
  });
}

// Hang-up the original incoming call, which is now held
function hangUpIncoming() {
  log("Hanging up the original incoming (now held) call.");

  let gotDisconnecting = false;
  incomingCall.ondisconnecting = function ondisconnectingIn(event) {
    log("Received 'disconnecting' call event for original (incoming) call.");
    is(incomingCall, event.call);
    is(incomingCall.state, "disconnecting");
    gotDisconnecting = true;
  };

  incomingCall.ondisconnected = function ondisconnectedIn(event) {
    log("Received 'disconnected' call event for original (incoming) call.");
    is(incomingCall, event.call);
    is(incomingCall.state, "disconnected");
    ok(gotDisconnecting);

    // Now back to one call
    is(telephony.active, outgoingCall);
    is(telephony.calls.length, 1);
    is(telephony.calls[0], outgoingCall);

    emulator.run("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "outbound to  " + outNumber + " : active");
      is(result[1], "OK");
      hangUpOutgoing();
    });
  };
  incomingCall.hangUp();
}

// Hang-up the remaining (outgoing) call
function hangUpOutgoing() {
  log("Hanging up the remaining (outgoing) call.");

  let gotDisconnecting = false;
  outgoingCall.ondisconnecting = function ondisconnectingOut(event) {
    log("Received 'disconnecting' call event for remaining (outgoing) call.");
    is(outgoingCall, event.call);
    is(outgoingCall.state, "disconnecting");
    gotDisconnecting = true;
  };

  outgoingCall.ondisconnected = function ondisconnectedOut(event) {
    log("Received 'disconnected' call event for remaining (outgoing) call.");
    is(outgoingCall, event.call);
    is(outgoingCall.state, "disconnected");
    ok(gotDisconnecting);

    // Now no calls
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
  telephony.onincoming = null;
  finish();
}

startTest(function() {
  simulateIncoming();
});
