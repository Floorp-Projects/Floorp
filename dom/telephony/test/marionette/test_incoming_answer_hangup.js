/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 10000;

const WHITELIST_PREF = "dom.telephony.app.phone.url";
SpecialPowers.setCharPref(WHITELIST_PREF, window.location.href);

let telephony = window.navigator.mozTelephony;
let number = "5555552368";
let incoming;
let calls;

function verifyInitialState() {
  log("Verifying initial state.");
  ok(telephony);
  is(telephony.active, null);
  ok(telephony.calls);
  is(telephony.calls.length, 0);
  calls = telephony.calls;

  runEmulatorCmd("gsm list", function(result) {
    log("Initial call list: " + result);
    is(result[0], "OK");
    simulateIncoming();
  });
}

function simulateIncoming() {
  log("Simulating an incoming call.");

  telephony.onincoming = function onincoming(event) {
    log("Received 'incoming' call event.");
    incoming = event.call;
    ok(incoming);
    is(incoming.number, number);
    is(incoming.state, "incoming");

    //ok(telephony.calls === calls); // bug 717414
    is(telephony.calls.length, 1);
    is(telephony.calls[0], incoming);

    runEmulatorCmd("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "inbound from " + number + " : incoming");
      is(result[1], "OK");
      answer();
    });
  };
  runEmulatorCmd("gsm call " + number);
}

function answer() {
  log("Answering the incoming call.");

  let gotConnecting = false;
  incoming.onconnecting = function onconnecting(event) { 
    log("Received 'connecting' call event.");
    is(incoming, event.call);
    is(incoming.state, "connecting");
    gotConnecting = true;
  };

  incoming.onconnected = function onconnected(event) {
    log("Received 'connected' call event.");
    is(incoming, event.call);
    is(incoming.state, "connected");
    ok(gotConnecting);

    is(incoming, telephony.active);

    runEmulatorCmd("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "inbound from " + number + " : active");
      is(result[1], "OK");
      hangUp();
    });
  };
  incoming.answer();
};

function hangUp() {
  log("Hanging up the incoming call.");

  let gotDisconnecting = false;
  incoming.ondisconnecting = function ondisconnecting(event) {
    log("Received 'disconnecting' call event.");
    is(incoming, event.call);
    is(incoming.state, "disconnecting");
    gotDisconnecting = true;
  };

  incoming.ondisconnected = function ondisconnected(event) {
    log("Received 'disconnected' call event.");
    is(incoming, event.call);
    is(incoming.state, "disconnected");
    ok(gotDisconnecting);

    is(telephony.active, null);
    is(telephony.calls.length, 0);

    runEmulatorCmd("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "OK");
      cleanUp();
    });
  };
  incoming.hangUp();
}

function cleanUp() {
  SpecialPowers.clearUserPref(WHITELIST_PREF);
  finish();
}

verifyInitialState();
