/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

SpecialPowers.addPermission("telephony", true, document);

let telephony = window.navigator.mozTelephony;
let outgoingCall;
let outNumber = "5555551111";

function getExistingCalls() {
  runEmulatorCmd("gsm list", function(result) {
    log("Initial call list: " + result);
    if (result[0] == "OK") {
      verifyInitialState(false);
    } else {
      cancelExistingCalls(result);
    };
  });
}

function cancelExistingCalls(callList) {
  if (callList.length && callList[0] != "OK") {
    // Existing calls remain; get rid of the next one in the list
    nextCall = callList.shift().split(' ')[2].trim();
    log("Cancelling existing call '" + nextCall +"'");
    runEmulatorCmd("gsm cancel " + nextCall, function(result) {
      if (result[0] == "OK") {
        cancelExistingCalls(callList);
      } else {
        log("Failed to cancel existing call");
        cleanUp();
      };
    });
  } else {
    // No more calls in the list; give time for emulator to catch up
    waitFor(verifyInitialState, function() {
      return (telephony.calls.length == 0);
    });
  };
}

function verifyInitialState(confirmNoCalls = true) {
  log("Verifying initial state.");
  ok(telephony);
  is(telephony.active, null);
  ok(telephony.calls);
  is(telephony.calls.length, 0);
  if (confirmNoCalls) {
    runEmulatorCmd("gsm list", function(result) {
    log("Initial call list: " + result);
      is(result[0], "OK");
      if (result[0] == "OK") {
        dial();
      } else {
        log("Call exists from a previous test, failing out.");
        cleanUp();
      };
    });
  } else {
    dial();
  }
}

function dial() {
  log("Make an outgoing call.");

  outgoingCall = telephony.dial(outNumber);
  ok(outgoingCall);
  is(outgoingCall.number, outNumber);
  is(outgoingCall.state, "dialing");

  is(outgoingCall, telephony.active);
  is(telephony.calls.length, 1);
  is(telephony.calls[0], outgoingCall);

  outgoingCall.onstatechange = function statechangering(event) {
    log("Received 'onstatechange' call event.");

    is(outgoingCall, event.call);
    let expectedStates = ["dialing", "alerting"];
    ok(expectedStates.indexOf(event.call.state) != -1);

    if (event.call.state == "alerting") {
      runEmulatorCmd("gsm list", function(result) {
        log("Call list is now: " + result);
        is(result[0], "outbound to  " + outNumber + " : ringing");
        is(result[1], "OK");
        answer();
      });
    }
  };
}

function answer() {
  log("Answering the outgoing call.");

  // We get no "connecting" event when the remote party answers the call.

  outgoingCall.onstatechange = function onstatechangeanswer(event) {
    log("Received 'onstatechange' call event.");
    is(outgoingCall, event.call);
    is(outgoingCall.state, "connected");
    is(outgoingCall, telephony.active);

    runEmulatorCmd("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "outbound to  " + outNumber + " : active");
      is(result[1], "OK");
      hold();
    });
  };
  runEmulatorCmd("gsm accept " + outNumber);
};

function hold() {
  log("Putting the call on hold.");

  let gotHolding = false;
  outgoingCall.onstatechange = function onstatechangehold(event) {
    log("Received 'onstatechange' call event.");
    is(outgoingCall, event.call);
    if(!gotHolding){
      is(outgoingCall.state, "holding");
      gotHolding = true;
    } else {
      is(outgoingCall.state, "held");
      is(telephony.active, null);
      is(telephony.calls.length, 1);
      is(telephony.calls[0], outgoingCall);

      runEmulatorCmd("gsm list", function(result) {
        log("Call list is now: " + result);
        is(result[0], "outbound to  " + outNumber + " : held");
        is(result[1], "OK");
        resume();
      });
    };
  };
  outgoingCall.hold();
}

function resume() {
  log("Resuming the held call.");

  let gotResuming = false;
  outgoingCall.onstatechange = function onstatechangeresume(event) {
    log("Received 'onstatechange' call event.");
    is(outgoingCall, event.call);
    if(!gotResuming){
      is(outgoingCall.state, "resuming");
      gotResuming = true;
    } else {
      is(outgoingCall.state, "connected");
      is(telephony.active, outgoingCall);
      is(telephony.calls.length, 1);
      is(telephony.calls[0], outgoingCall);

      runEmulatorCmd("gsm list", function(result) {
        log("Call list is now: " + result);
        is(result[0], "outbound to  " + outNumber + " : active");
        is(result[1], "OK");
        hangUp();
      });
    };
  };
  outgoingCall.resume();
}

function hangUp() {
  log("Hanging up the outgoing call (local hang-up).");

  let gotDisconnecting = false;
  outgoingCall.onstatechange = function onstatechangedisconnect(event) {
    log("Received 'onstatechange' call event.");
    is(outgoingCall, event.call);
    if(!gotDisconnecting){
      is(outgoingCall.state, "disconnecting");
      gotDisconnecting = true;
    } else {
      is(outgoingCall.state, "disconnected");
      is(telephony.active, null);
      is(telephony.calls.length, 0);

      runEmulatorCmd("gsm list", function(result) {
        log("Call list is now: " + result);
        is(result[0], "OK");
        cleanUp();
      });
    };
  };
  outgoingCall.hangUp();
}

function cleanUp() {
  SpecialPowers.removePermission("telephony", document);
  finish();
}

// Start the test
getExistingCalls();
