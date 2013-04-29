/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

SpecialPowers.addPermission("telephony", true, document);

let telephony = window.navigator.mozTelephony;
let outNumber = "5555551111";
let inNumber = "5555552222";
let outgoingCall;
let incomingCall;
let gotOriginalConnected = false;
let gotHeld = false;
let gotConnected = false;

function getExistingCalls() {
  runEmulatorCmd("gsm list", function(result) {
    log("Initial call list: " + result);
    if (result[0] == "OK") {
      verifyInitialState(false);
    } else {
      cancelExistingCalls(result);
    };
  });
}

function cancelExistingCalls(callList) {
  if (callList.length && callList[0] != "OK") {
    // Existing calls remain; get rid of the next one in the list
    nextCall = callList.shift().split(' ')[2].trim();
    log("Cancelling existing call '" + nextCall +"'");
    runEmulatorCmd("gsm cancel " + nextCall, function(result) {
      if (result[0] == "OK") {
        cancelExistingCalls(callList);
      } else {
        log("Failed to cancel existing call");
        cleanUp();
      };
    });
  } else {
    // No more calls in the list; give time for emulator to catch up
    waitFor(verifyInitialState, function() {
      return (telephony.calls.length == 0);
    });
  };
}

function verifyInitialState(confirmNoCalls = true) {
  log("Verifying initial state.");
  ok(telephony);
  is(telephony.active, null);
  ok(telephony.calls);
  is(telephony.calls.length, 0);
  if (confirmNoCalls) {
    runEmulatorCmd("gsm list", function(result) {
    log("Initial call list: " + result);
      is(result[0], "OK");
      if (result[0] == "OK") {
        dial();
      } else {
        log("Call exists from a previous test, failing out.");
        cleanUp();
      };
    });
  } else {
    dial();
  }
}

function dial() {
  log("Make an outgoing call.");
  outgoingCall = telephony.dial(outNumber);
  ok(outgoingCall);
  is(outgoingCall.number, outNumber);
  is(outgoingCall.state, "dialing");

  outgoingCall.onalerting = function onalerting(event) {
    log("Received 'alerting' call event.");

    is(outgoingCall, event.call);
    is(outgoingCall.state, "alerting");
    is(outgoingCall, telephony.active);
    is(telephony.calls.length, 1);
    is(telephony.calls[0], outgoingCall);

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
  outgoingCall.onconnected = function onconnectedOut(event) {
    log("Received 'connected' call event for the original outgoing call.");

    is(outgoingCall, event.call);
    is(outgoingCall.state, "connected");
    is(outgoingCall, telephony.active);

    runEmulatorCmd("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "outbound to  " + outNumber + " : active");
      is(result[1], "OK");

      if(!gotOriginalConnected){
        gotOriginalConnected = true;
        holdCall();
      } else {
        // Received connected event for original call multiple times (fail)
        ok(false,
           "Received 'connected' event for original call multiple times.");
      }
    });
  };
  runEmulatorCmd("gsm accept " + outNumber);
}

function holdCall() {
  log("Putting the original (outgoing) call on hold.");

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

    runEmulatorCmd("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "outbound to  " + outNumber + " : held");
      is(result[1], "OK");
      simulateIncoming();
    });
  };
  outgoingCall.hold();
}

// With one call on hold, simulate an incoming call
function simulateIncoming() {
  log("Simulating an incoming call (with one call already held).");

  telephony.onincoming = function onincoming(event) {
    log("Received 'incoming' call event.");
    incomingCall = event.call;
    ok(incomingCall);
    is(incomingCall.number, inNumber);
    is(incomingCall.state, "incoming");

    // Should be two calls now
    is(telephony.calls.length, 2);
    is(telephony.calls[0], outgoingCall);
    is(telephony.calls[1], incomingCall);

    runEmulatorCmd("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "outbound to  " + outNumber + " : held");
      is(result[1], "inbound from " + inNumber + " : incoming");
      is(result[2], "OK");
      answerIncoming();
    });
  };
  runEmulatorCmd("gsm call " + inNumber);
}

// Answer incoming call; original outgoing call should be held
function answerIncoming() {
  log("Answering the incoming call.");

  let gotConnecting = false;
  incomingCall.onconnecting = function onconnectingIn(event) { 
    log("Received 'connecting' call event for incoming/2nd call.");
    is(incomingCall, event.call);
    is(incomingCall.state, "connecting");
    gotConnecting = true;
  };

  incomingCall.onconnected = function onconnectedIn(event) {
    log("Received 'connected' call event for incoming/2nd call.");
    is(incomingCall, event.call);
    ok(gotConnecting);

    is(incomingCall, telephony.active);

    // Original outbound call now held, incoming call active
    is(outgoingCall.state, "held");
    is(incomingCall.state, "connected");

    runEmulatorCmd("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "outbound to  " + outNumber + " : held");
      is(result[1], "inbound from " + inNumber + " : active");
      is(result[2], "OK");
      swapCalls();
    });
  };
  incomingCall.answer();
}

// Now swap the held and active calls
function swapCalls() {
  log("Swapping the held and active calls.");

  // Swap calls by resuming the held (outgoing) call. Will force active
  // (incoming) call to hold.

  incomingCall.onheld = function onheldIn(event) {
    log("Received 'held' call event for incoming call.");
    gotHeld = true;

    if (gotHeld && gotConnected) { verifySwap(); }
  };

  let gotResuming = false;
  outgoingCall.onresuming = function onresuming(event) {
    log("Received 'resuming' call event for outbound call.");
    is(outgoingCall, event.call);
    is(outgoingCall.state, "resuming");
    gotResuming = true;
  };

  outgoingCall.onconnected = function onconnected(event) {
    log("Received 'connected' call event for outbound call.");
    is(outgoingCall, event.call);
    ok(gotResuming);

    is(outgoingCall, telephony.active);
    is(telephony.calls.length, 2);
    is(telephony.calls[0], outgoingCall);
    is(telephony.calls[1], incomingCall);
    gotConnected = true;

    if (gotHeld && gotConnected) { verifySwap(); }
  };
  outgoingCall.resume();     
}

function verifySwap() {
  // Call status reflects swap
  is(outgoingCall.state, "connected");
  is(incomingCall.state, "held");

  runEmulatorCmd("gsm list", function(result) {
    log("Call list is now: " + result);
    is(result[0], "outbound to  " + outNumber + " : active");
    is(result[1], "inbound from " + inNumber + " : held");

    // Begin hang-up
    hangUpOutgoing();
  });
}

// Hang-up original outgoing (now active) call
function hangUpOutgoing() {
  log("Hanging up the original outgoing (now active) call.");

  let gotDisconnecting = false;
  outgoingCall.ondisconnecting = function ondisconnectingOut(event) {
    log("Received 'disconnecting' call event for original outgoing call.");
    is(outgoingCall, event.call);
    is(outgoingCall.state, "disconnecting");
    gotDisconnecting = true;
  };

  outgoingCall.ondisconnected = function ondisconnectedOut(event) {
    log("Received 'disconnected' call event for original outgoing call.");
    is(outgoingCall, event.call);
    is(outgoingCall.state, "disconnected");
    ok(gotDisconnecting);

    // Back to one call now
    is(telephony.calls.length, 1);
    is(incomingCall.state, "held");

    runEmulatorCmd("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "inbound from " + inNumber + " : held");
      is(result[1], "OK");
      hangUpIncoming();
    });
  };
  outgoingCall.hangUp();
}

// Hang-up remaining (now held) call
function hangUpIncoming() {
  log("Hanging up the remaining (now held) call.");

  let gotDisconnecting = false;
  incomingCall.ondisconnecting = function ondisconnectingIn(event) {
    log("Received 'disconnecting' call event for remaining (held) call.");
    is(incomingCall, event.call);
    is(incomingCall.state, "disconnecting");
    gotDisconnecting = true;
  };

  incomingCall.ondisconnected = function ondisconnectedIn(event) {
    log("Received 'disconnected' call event for remaining (incoming) call.");
    is(incomingCall, event.call);
    is(incomingCall.state, "disconnected");
    ok(gotDisconnecting);

    // Zero calls left
    is(telephony.active, null);
    is(telephony.calls.length, 0);

    runEmulatorCmd("gsm list", function(result) {
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
getExistingCalls();
