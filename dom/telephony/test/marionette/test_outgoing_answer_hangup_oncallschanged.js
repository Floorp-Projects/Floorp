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

  telephony.oncallschanged = function oncallschanged(event) {
    log("Received 'callschanged' call event.");

    let expected_states = ["dialing", "disconnected"];
    ok(expected_states.indexOf(event.call.state) != -1,
      "Unexpected call state: " + event.call.state);

    if (event.call.state == "dialing") {
      outgoing = event.call;
      ok(outgoing);
      is(outgoing.number, number);

      is(outgoing, telephony.active);
      //ok(telephony.calls === calls); // bug 717414
      is(telephony.calls.length, 1);
      is(telephony.calls[0], outgoing);

      runEmulatorCmd("gsm list", function(result) {
        log("Call list is now: " + result);
        is(result[0], "outbound to  " + number + " : unknown");
        is(result[1], "OK");
        answer();
      });
    }

    if (event.call.state == "disconnected") {
      is(outgoing.state, "disconnected");
      is(telephony.active, null);
      is(telephony.calls.length, 0);
      cleanUp();
    }
  };

  telephony.dial(number);
}

function answer() {
  log("Answering the outgoing call.");

  // We get no "connecting" event when the remote party answers the call.

  outgoing.onconnected = function onconnected(event) {
    log("Received 'connected' call event.");
    is(outgoing, event.call);
    is(outgoing.state, "connected");

    is(outgoing, telephony.active);

    runEmulatorCmd("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "outbound to  " + number + " : active");
      is(result[1], "OK");
      hangUp();
    });
  };
  runEmulatorCmd("gsm accept " + number);
};

function hangUp() {
  log("Hanging up the outgoing call.");

  runEmulatorCmd("gsm cancel " + number);
}

function cleanUp() {
  SpecialPowers.clearUserPref(WHITELIST_PREF);
  finish();
}

verifyInitialState();
