/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

let number;
let emergency;
let outgoing;

let testCase = 0;
let expectedResults = [
  ["112", true],
  ["911", true],
  ["0912345678", false],
  ["777", false],
];

function createGoldenCallListResult0(number, state) {
  //  "outbound to  xxxxxxxxxx : ringing"
  let padPattern = "          ";
  let pad = padPattern.substring(0, padPattern.length - number.length);
  return "outbound to  " + number + pad + " : " + state;
}

function dial() {
  log("Make an outgoing call.");

  telephony.dial(number).then(call => {
    outgoing = call;
    ok(outgoing);
    is(outgoing.number, number);
    is(outgoing.state, "dialing");

    is(outgoing, telephony.active);
    is(telephony.calls.length, 1);
    is(telephony.calls[0], outgoing);

    outgoing.onalerting = function onalerting(event) {
      log("Received 'onalerting' call event.");
      is(outgoing, event.call);
      is(outgoing.state, "alerting");
      is(outgoing.emergency, emergency);

      emulator.run("gsm list", function(result) {
        log("Call list is now: " + result);
        is(result[0], createGoldenCallListResult0(number, "ringing"));
        is(result[1], "OK");
        answer();
      });
    };
  });
}

function answer() {
  log("Answering the call.");

  // We get no "connecting" event when the remote party answers the call.

  outgoing.onconnected = function onconnected(event) {
    log("Received 'connected' call event.");
    is(outgoing, event.call);
    is(outgoing.state, "connected");
    is(outgoing.emergency, emergency);

    is(outgoing, telephony.active);

    emulator.run("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], createGoldenCallListResult0(number, "active"));
      is(result[1], "OK");
      hangUp();
    });
  };
  emulator.run("gsm accept " + number);
}

function hangUp() {
  log("Hanging up the call.");

  // We get no "disconnecting" event when the remote party terminates the call.

  outgoing.ondisconnected = function ondisconnected(event) {
    log("Received 'disconnected' call event.");
    is(outgoing, event.call);
    is(outgoing.state, "disconnected");
    is(outgoing.emergency, emergency);

    is(telephony.active, null);
    is(telephony.calls.length, 0);

    emulator.run("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "OK");
      verifyNextEmergencyLabel();
    });
  };
  emulator.run("gsm cancel " + number);
}

function cleanUp() {
  finish();
}

function verifyNextEmergencyLabel() {
  if (testCase >= expectedResults.length) {
    cleanUp();
  } else {
    log("Running test case: " + testCase + "/" + expectedResults.length);
    number = expectedResults[testCase][0];
    emergency = expectedResults[testCase][1];
    testCase++;

    // No more calls in the list; give time for emulator to catch up
    waitFor(dial, function() {
      return (telephony.calls.length === 0);
    });
  }
}

startTest(function() {
  verifyNextEmergencyLabel();
});
