/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

SpecialPowers.addPermission("telephony", true, document);

let telephony = window.navigator.mozTelephony;
let number = "5555552368";
let outgoing;

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
      is(telephony.calls.length, 1);
      is(telephony.calls[0], outgoing);

      checkCallList();
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

function checkCallList() {
  runEmulatorCmd("gsm list", function(result) {
    log("Call list is now: " + result)
    if ((result[0] == "outbound to  " + number + " : unknown") && (result[1] == "OK")) {
      answer();
    } else {
      window.setTimeout(checkCallList, 100);
    }
  });
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
      log("Call list (after 'connected' event) is now: " + result);
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
  telephony.oncallschanged = null;
  SpecialPowers.removePermission("telephony", document);
  finish();
}

getExistingCalls();
