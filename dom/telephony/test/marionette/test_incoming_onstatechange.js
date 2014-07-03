/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_HEAD_JS = 'head.js';
MARIONETTE_TIMEOUT = 60000;

let incomingCall;
let inNumber = "5555551111";

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

  gotConnecting = false;
  incomingCall.onstatechange = function statechangeconnect(event) {
    log("Received 'onstatechange' call event.");
    is(incomingCall, event.call);
    if(!gotConnecting){
      is(incomingCall.state, "connecting");
      gotConnecting = true;
    } else {
      is(incomingCall.state, "connected");
      is(telephony.active, incomingCall);
      is(telephony.calls.length, 1);
      is(telephony.calls[0], incomingCall);

      emulator.runCmdWithCallback("gsm list", function(result) {
        log("Call list is now: " + result);
        is(result[0], "inbound from " + inNumber + " : active");
        hold();
      });
    }
  };
  incomingCall.answer();
}

function hold() {
  log("Putting the call on hold.");

  let gotHolding = false;
  incomingCall.onstatechange = function onstatechangehold(event) {
    log("Received 'onstatechange' call event.");
    is(incomingCall, event.call);
    if(!gotHolding){
      is(incomingCall.state, "holding");
      gotHolding = true;
    } else {
      is(incomingCall.state, "held");
      is(telephony.active, null);
      is(telephony.calls.length, 1);
      is(telephony.calls[0], incomingCall);

      emulator.runCmdWithCallback("gsm list", function(result) {
        log("Call list is now: " + result);
        is(result[0], "inbound from " + inNumber + " : held");
        resume();
      });
    }
  };
  incomingCall.hold();
}

function resume() {
  log("Resuming the held call.");

  let gotResuming = false;
  incomingCall.onstatechange = function onstatechangeresume(event) {
    log("Received 'onstatechange' call event.");
    is(incomingCall, event.call);
    if(!gotResuming){
      is(incomingCall.state, "resuming");
      gotResuming = true;
    } else {
      is(incomingCall.state, "connected");
      is(telephony.active, incomingCall);
      is(telephony.calls.length, 1);
      is(telephony.calls[0], incomingCall);

      emulator.runCmdWithCallback("gsm list", function(result) {
        log("Call list is now: " + result);
        is(result[0], "inbound from " + inNumber + " : active");
        hangUp();
      });
    }
  };
  incomingCall.resume();
}

function hangUp() {
  log("Hanging up the incoming call (local hang-up).");

  let gotDisconnecting = false;
  incomingCall.onstatechange = function onstatechangedisconnect(event) {
    log("Received 'onstatechange' call event.");
    is(incomingCall, event.call);
    if(!gotDisconnecting){
      is(incomingCall.state, "disconnecting");
      gotDisconnecting = true;
    } else {
      is(incomingCall.state, "disconnected");
      is(telephony.active, null);
      is(telephony.calls.length, 0);

      emulator.runCmdWithCallback("gsm list", function(result) {
        log("Call list is now: " + result);
        cleanUp();
      });
    }
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
