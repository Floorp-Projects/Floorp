/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

SpecialPowers.addPermission("telephony", true, document);

let telephony = window.navigator.mozTelephony;
let inNumber = "5555551111";
let incomingCall;

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
        simulateIncoming();
      } else {
        log("Call exists from a previous test, failing out.");
        cleanUp();
      };
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
    is(incomingCall.number, inNumber);
    is(incomingCall.state, "incoming");

    is(telephony.calls.length, 1);
    is(telephony.calls[0], incomingCall);

    runEmulatorCmd("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "inbound from " + inNumber + " : incoming");
      is(result[1], "OK");
      answerIncoming();
    });
  };
  runEmulatorCmd("gsm call " + inNumber);
}

function answerIncoming() {
  log("Answering the incoming call.");

  let gotConnecting = false;
  incomingCall.onconnecting = function onconnectingIn(event) { 
    log("Received 'connecting' call event for incoming call.");
    is(incomingCall, event.call);
    is(incomingCall.state, "connecting");
    gotConnecting = true;
  };

  incomingCall.onconnected = function onconnectedIn(event) {
    log("Received 'connected' call event for incoming call.");
    is(incomingCall, event.call);
    is(incomingCall.state, "connected");
    ok(gotConnecting);

    is(incomingCall, telephony.active);

    runEmulatorCmd("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "inbound from " + inNumber + " : active");
      is(result[1], "OK");
      answerAlreadyConnected();
    });
  };
  incomingCall.answer();
}

function answerAlreadyConnected() {
  log("Attempting to answer already connected call, should be ignored.");

  incomingCall.onconnecting = function onconnectingErr(event) {
    log("Received 'connecting' call event, but should not have.");
    ok(false, "Received 'connecting' event when answered already active call");
  }

  incomingCall.onconnected = function onconnectedErr(event) {
    log("Received 'connected' call event, but should not have.");
    ok(false, "Received 'connected' event when answered already active call");
  }

  incomingCall.answer();

  is(incomingCall.state, "connected");
  is(telephony.calls.length, 1);
  is(telephony.calls[0], incomingCall);
  is(incomingCall, telephony.active);
  hold();
}

function hold() {
  log("Putting the call on hold.");

  let gotHolding = false;
  incomingCall.onholding = function onholding(event) {
    log("Received 'holding' call event.");
    is(incomingCall, event.call);
    is(incomingCall.state, "holding");
    gotHolding = true;
  };

  incomingCall.onheld = function onheld(event) {
    log("Received 'held' call event.");
    is(incomingCall, event.call);
    is(incomingCall.state, "held");
    ok(gotHolding);

    is(telephony.active, null);
    is(telephony.calls.length, 1);
    is(telephony.calls[0], incomingCall);

    runEmulatorCmd("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "inbound from " + inNumber + " : held");
      is(result[1], "OK");
      holdAlreadyHeld();
    });
  };
  incomingCall.hold();
}

function holdAlreadyHeld() {
  log("Attempting to hold an already held call, should be ignored.");

  incomingCall.onholding = function onholding(event) {
    log("Received 'holding' call event, but should not have.");
    ok(false, "Received 'holding' event when held an already held call");
  }

  incomingCall.onheld = function onheldErr(event) {
    log("Received 'held' call event, but should not have.");
    ok(false, "Received 'held' event when held an already held call");
  }

  incomingCall.hold();

  is(incomingCall.state, "held");
  is(telephony.active, null);
  is(telephony.calls.length, 1);
  is(telephony.calls[0], incomingCall);

  answerHeld();
}

function answerHeld() {
  log("Attempting to answer a held call, should be ignored.");

  incomingCall.onconnecting = function onconnectingErr(event) {
    log("Received 'connecting' call event, but should not have.");
    ok(false, "Received 'connecting' event when answered a held call");
  }

  incomingCall.onconnected = function onconnectedErr(event) {
    log("Received 'connected' call event, but should not have.");
    ok(false, "Received 'connected' event when answered a held call");
  }

  incomingCall.answer();

  is(incomingCall.state, "held");
  is(telephony.active, null);
  is(telephony.calls.length, 1);
  is(telephony.calls[0], incomingCall);

  resume();
}

function resume() {
  log("Resuming the held call.");

  let gotResuming = false;
  incomingCall.onresuming = function onresuming(event) {
    log("Received 'resuming' call event.");
    is(incomingCall, event.call);
    is(incomingCall.state, "resuming");
    gotResuming = true;
  };

  incomingCall.onconnected = function onconnected(event) {
    log("Received 'connected' call event.");
    is(incomingCall, event.call);
    is(incomingCall.state, "connected");
    ok(gotResuming);

    is(incomingCall, telephony.active);
    is(telephony.calls.length, 1);
    is(telephony.calls[0], incomingCall);

    runEmulatorCmd("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "inbound from " + inNumber + " : active");
      is(result[1], "OK");
      resumeNonHeld();
    });
  };
  incomingCall.resume();
}

function resumeNonHeld() {
  log("Attempting to resume non-held call, should be ignored.");

  incomingCall.onresuming = function onresumingErr(event) {
    log("Received 'resuming' call event, but should not have.");
    ok(false, "Received 'resuming' event when resumed non-held call");
  }

  incomingCall.onconnected = function onconnectedErr(event) {
    log("Received 'connected' call event, but should not have.");
    ok(false, "Received 'connected' event when resumed non-held call");
  }

  incomingCall.resume();

  is(incomingCall.state, "connected");
  is(telephony.calls.length, 1);
  is(telephony.calls[0], incomingCall);
  is(incomingCall, telephony.active);
  hangUp();
}

function hangUp() {
  log("Hanging up the call (local hang-up).");

  let gotDisconnecting = false;
  incomingCall.ondisconnecting = function ondisconnecting(event) {
    log("Received 'disconnecting' call event.");
    is(incomingCall, event.call);
    is(incomingCall.state, "disconnecting");
    gotDisconnecting = true;
  };

  incomingCall.ondisconnected = function ondisconnectedOut(event) {
    log("Received 'disconnected' call event.");
    is(incomingCall, event.call);
    is(incomingCall.state, "disconnected");
    ok(gotDisconnecting);

    is(telephony.active, null);
    is(telephony.calls.length, 0);

    runEmulatorCmd("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "OK");
      answerDisconnected();
    });
  };
  incomingCall.hangUp();
}

function answerDisconnected() {
  log("Attempting to answer disconnected call, should be ignored.");

  incomingCall.onconnecting = function onconnectingErr(event) {
    log("Received 'connecting' call event, but should not have.");
    ok(false, "Received 'connecting' event when answered disconnected call");
  }

  incomingCall.onconnected = function onconnectedErr(event) {
    log("Received 'connected' call event, but should not have.");
    ok(false, "Received 'connected' event when answered disconnected call");
  }

  incomingCall.answer();

  is(telephony.active, null);
  is(telephony.calls.length, 0);
  holdDisconnected();
}

function holdDisconnected() {
  log("Attempting to hold disconnected call, should be ignored.");

  incomingCall.onholding = function onholdingErr(event) {
    log("Received 'holding' call event, but should not have.");
    ok(false, "Received 'holding' event when held a disconnected call");
  }

  incomingCall.onheld = function onheldErr(event) {
    log("Received 'held' call event, but should not have.");
    ok(false, "Received 'held' event when held a disconnected call");
  }

  incomingCall.hold();

  is(telephony.active, null);
  is(telephony.calls.length, 0);
  resumeDisconnected();
}

function resumeDisconnected() {
  log("Attempting to resume disconnected call, should be ignored.");

  incomingCall.onresuming = function onresumingErr(event) {
    log("Received 'resuming' call event, but should not have.");
    ok(false, "Received 'resuming' event when resumed disconnected call");
  }

  incomingCall.onconnected = function onconnectedErr(event) {
    log("Received 'connected' call event, but should not have.");
    ok(false, "Received 'connected' event when resumed disconnected call");
  }

  incomingCall.resume();

  is(telephony.active, null);
  is(telephony.calls.length, 0);
  hangUpNonConnected();
}

function hangUpNonConnected() {
  log("Attempting to hang-up disconnected call, should be ignored.");

  incomingCall.ondisconnecting = function ondisconnectingErr(event) {
    log("Received 'disconnecting' call event, but should not have.");
    ok(false, "Received 'disconnecting' event when hung-up non-active call");
  }

  incomingCall.ondisconnected = function ondisconnectedErr(event) {
    log("Received 'disconnected' call event, but should not have.");
    ok(false, "Received 'disconnected' event when hung-up non-active call");
  }

  incomingCall.hangUp();

  is(telephony.active, null);
  is(telephony.calls.length, 0);
  cleanUp();
}

function cleanUp() {
  telephony.onincoming = null;
  SpecialPowers.removePermission("telephony", document);
  finish();
}

// Start the test
getExistingCalls();
