/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

SpecialPowers.addPermission("telephony", true, document);

let telephony = window.navigator.mozTelephony;
let incomingCall;
let inNumber = "5555551111";

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
      simulateIncoming();
    } else {
      log("Call exists from a previous test, failing out.");
      cleanUp();
    }
  });
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

      runEmulatorCmd("gsm list", function(result) {
        log("Call list is now: " + result);
        is(result[0], "inbound from " + inNumber + " : active");
        is(result[1], "OK");
        hold();
      });
    };
  };
  incomingCall.answer();
};

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

      runEmulatorCmd("gsm list", function(result) {
        log("Call list is now: " + result);
        is(result[0], "inbound from " + inNumber + " : held");
        is(result[1], "OK");
        resume();
      });
    };
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

      runEmulatorCmd("gsm list", function(result) {
        log("Call list is now: " + result);
        is(result[0], "inbound from " + inNumber + " : active");
        is(result[1], "OK");
        hangUp();
      });
    };
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

      runEmulatorCmd("gsm list", function(result) {
        log("Call list is now: " + result);
        is(result[0], "OK");
        cleanUp();
      });
    };
  };
  incomingCall.hangUp();
}

function cleanUp() {
  telephony.onincoming = null;
  SpecialPowers.removePermission("telephony", document);
  finish();
}

// Start the test
verifyInitialState();
