/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 10000;

const WHITELIST_PREF = "dom.telephony.app.phone.url";
SpecialPowers.setCharPref(WHITELIST_PREF, window.location.href);

let telephony = window.navigator.mozTelephony;
let number = "5555552368";
let outgoing;
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
    dial();
  });
}

function dial() {
  log("Make an outgoing call.");

  outgoing = telephony.dial(number);
  ok(outgoing);
  is(outgoing.number, number);
  is(outgoing.state, "dialing");

  is(outgoing, telephony.active);
  //ok(telephony.calls === calls); // bug 717414
  is(telephony.calls.length, 1);
  is(telephony.calls[0], outgoing);

  runEmulatorCmd("gsm list", function(result) {
    log("Call list is now: " + result);
    is(result[0], "outbound to  " + number + " : unknown");
    is(result[1], "OK");
    reject();
  });
}

function reject() {
  log("Reject the outgoing call on the other end.");

  // We get no "disconnecting" event when the remote party rejects the call.

  outgoing.ondisconnected = function ondisconnected(event) {
    log("Received 'disconnected' call event.");
    is(outgoing, event.call);
    is(outgoing.state, "disconnected");

    is(telephony.active, null);
    is(telephony.calls.length, 0);

    runEmulatorCmd("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "OK");
      cleanUp();
    });
  };
  runEmulatorCmd("gsm list", cmdCallback);
};

function cmdCallback(result) {
  let state = "outbound to  " + number + " : unknown";
  log("Call list is now: " + result);

  // The outgoing call cannot be canceled when call state is unknown.
  // Wait until the call connection is established.
  if (result[0] == state) {
    runEmulatorCmd("gsm list", cmdCallback);
  } else {
    runEmulatorCmd("gsm cancel " + number);
  }
}

function cleanUp() {
  SpecialPowers.clearUserPref(WHITELIST_PREF);
  finish();
}

verifyInitialState();
