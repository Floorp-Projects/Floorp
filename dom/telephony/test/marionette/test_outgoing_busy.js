/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 10000;

SpecialPowers.addPermission("telephony", true, document);

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

  //is(outgoing, telephony.active); // bug 757587
  //ok(telephony.calls === calls); // bug 757587
  //is(calls.length, 1); // bug 757587
  //is(calls[0], outgoing); // bug 757587

outgoing.onstatechange = function onstatechange(event) {
  log("outgoing call state: " + outgoing.state);
};

  runEmulatorCmd("gsm list", function(result) {
    log("Call list is now: " + result);
    is(result[0], "outbound to  " + number + " : unknown");
    is(result[1], "OK");
    busy();
  });
}

function busy() {
  log("The receiver is busy.");

  outgoing.onbusy = function onbusy(event) {
    log("Received 'busy' call event.");
    is(outgoing, event.call);
    is(outgoing.state, "busy");

    //is(outgoing, telephony.active);  // bug 757587

    runEmulatorCmd("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "OK");
      cleanUp();
    });
  };
  runEmulatorCmd("gsm busy " + number);
};

function cleanUp() {
  SpecialPowers.removePermission("telephony", document);
  finish();
}

verifyInitialState();
