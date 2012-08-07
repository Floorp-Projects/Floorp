/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 10000;

SpecialPowers.addPermission("telephony", true, document);

let telephony = window.navigator.mozTelephony;
let number = "not a valid phone number";
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
  log("Make an outgoing call to an invalid number.");

  outgoing = telephony.dial(number);
  ok(outgoing);
  is(outgoing.number, number);
  is(outgoing.state, "dialing");

  //is(outgoing, telephony.active); // bug 757587
  //ok(telephony.calls === calls); // bug 757587
  //is(calls.length, 1); // bug 757587
  //is(calls[0], outgoing); // bug 757587

  outgoing.onerror = function onerror(event) {
    log("Received 'error' event.");
    is(event.call, outgoing);
    ok(call.error);
    is(call.error.name, "BadNumberError");

    runEmulatorCmd("gsm list", function(result) {
      log("Initial call list: " + result);
      is(result[0], "OK");
      cleanUp();
    });
  };
}

function cleanUp() {
  SpecialPowers.removePermission("telephony", document);
  finish();
}

verifyInitialState();
