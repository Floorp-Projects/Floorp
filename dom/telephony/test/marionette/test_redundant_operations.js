/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

let inNumber = "5555551111";
let incomingCall;

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

    emulator.runCmdWithCallback("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "inbound from " + inNumber + " : incoming");
      answerIncoming();
    });
  };
  emulator.runCmdWithCallback("gsm call " + inNumber);
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

    emulator.runCmdWithCallback("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "inbound from " + inNumber + " : active");
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
  };

  incomingCall.onconnected = function onconnectedErr(event) {
    log("Received 'connected' call event, but should not have.");
    ok(false, "Received 'connected' event when answered already active call");
  };

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

    emulator.runCmdWithCallback("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "inbound from " + inNumber + " : held");
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
  };

  incomingCall.onheld = function onheldErr(event) {
    log("Received 'held' call event, but should not have.");
    ok(false, "Received 'held' event when held an already held call");
  };

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
  };

  incomingCall.onconnected = function onconnectedErr(event) {
    log("Received 'connected' call event, but should not have.");
    ok(false, "Received 'connected' event when answered a held call");
  };

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

    emulator.runCmdWithCallback("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "inbound from " + inNumber + " : active");
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
  };

  incomingCall.onconnected = function onconnectedErr(event) {
    log("Received 'connected' call event, but should not have.");
    ok(false, "Received 'connected' event when resumed non-held call");
  };

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

    emulator.runCmdWithCallback("gsm list", function(result) {
      log("Call list is now: " + result);
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
  };

  incomingCall.onconnected = function onconnectedErr(event) {
    log("Received 'connected' call event, but should not have.");
    ok(false, "Received 'connected' event when answered disconnected call");
  };

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
  };

  incomingCall.onheld = function onheldErr(event) {
    log("Received 'held' call event, but should not have.");
    ok(false, "Received 'held' event when held a disconnected call");
  };

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
  };

  incomingCall.onconnected = function onconnectedErr(event) {
    log("Received 'connected' call event, but should not have.");
    ok(false, "Received 'connected' event when resumed disconnected call");
  };

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
  };

  incomingCall.ondisconnected = function ondisconnectedErr(event) {
    log("Received 'disconnected' call event, but should not have.");
    ok(false, "Received 'disconnected' event when hung-up non-active call");
  };

  incomingCall.hangUp();

  is(telephony.active, null);
  is(telephony.calls.length, 0);
  cleanUp();
}

function cleanUp() {
  telephony.onincoming = null;
  finish();
}

startTest(function() {
  simulateIncoming();
});
