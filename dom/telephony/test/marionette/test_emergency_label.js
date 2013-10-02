/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

SpecialPowers.addPermission("telephony", true, document);

let telephony = window.navigator.mozTelephony;
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

function getExistingCalls() {
  emulator.run("gsm list", function(result) {
    log("Initial call list: " + result);
    if (result[0] == "OK") {
      verifyInitialState(false);
    } else {
      cancelExistingCalls(result);
    }
  });
}

function cancelExistingCalls(callList) {
  if (callList.length && callList[0] != "OK") {
    // Existing calls remain; get rid of the next one in the list
    nextCall = callList.shift().split(/\s+/)[2].trim();
    log("Cancelling existing call '" + nextCall +"'");
    emulator.run("gsm cancel " + nextCall, function(result) {
      if (result[0] == "OK") {
        cancelExistingCalls(callList);
      } else {
        log("Failed to cancel existing call");
        cleanUp();
      }
    });
  } else {
    // No more calls in the list; give time for emulator to catch up
    waitFor(verifyInitialState, function() {
      return (telephony.calls.length === 0);
    });
  }
}

function verifyInitialState(confirmNoCalls = true) {
  log("Verifying initial state.");
  ok(telephony);
  is(telephony.active, null);
  ok(telephony.calls);
  is(telephony.calls.length, 0);
  if (confirmNoCalls) {
    emulator.run("gsm list", function(result) {
    log("Initial call list: " + result);
      is(result[0], "OK");
      if (result[0] == "OK") {
        dial();
      } else {
        log("Call exists from a previous test, failing out.");
        cleanUp();
      }
    });
  } else {
    dial();
  }
}

function createGoldenCallListResult0(number, state) {
  //  "outbound to  xxxxxxxxxx : ringing"
  let padPattern = "          ";
  let pad = padPattern.substring(0, padPattern.length - number.length);
  return "outbound to  " + number + pad + " : " + state;
}

function dial() {
  log("Make an outgoing call.");

  outgoing = telephony.dial(number);
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
  SpecialPowers.removePermission("telephony", document);
  finish();
}

function verifyNextEmergencyLabel() {
  if (testCase >= expectedResults.length) {
    cleanUp();
  } else {
    log("Running test case: " + testCase + "/" + expectedResults.length);
    number = expectedResults[testCase][0];
    emergency = expectedResults[testCase][1];
    getExistingCalls();
    testCase++;
  }
}

startTest(function() {
  verifyNextEmergencyLabel();
});
