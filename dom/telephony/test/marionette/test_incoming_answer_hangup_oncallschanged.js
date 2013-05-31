/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

SpecialPowers.addPermission("telephony", true, document);

let telephony = window.navigator.mozTelephony;
let number = "5555552368";
let incoming;

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
        simulateIncoming();
      } else {
        log("Call exists from a previous test, failing out.");
        cleanUp();
      };
    });
  } else {
    simulateIncoming();
  }
}

function simulateIncoming() {
  log("Simulating an incoming call.");

  telephony.oncallschanged = function oncallschanged(event) {
    log("Received 'callschanged' event.");

    if (!event.call) {
      log("Notifying calls array is loaded. No call information accompanies.");
      return;
    }

    let expected_states = ["incoming", "disconnected"];
    ok(expected_states.indexOf(event.call.state) != -1,
      "Unexpected call state: " + event.call.state);

    if (event.call.state == "incoming") {
      log("Received 'callschanged' event for an incoming call.");
      incoming = event.call;
      ok(incoming);
      is(incoming.number, number);

      is(telephony.calls.length, 1);
      is(telephony.calls[0], incoming);

      runEmulatorCmd("gsm list", function(result) {
        log("Call list is now: " + result);
        is(result[0], "inbound from " + number + " : incoming");
        is(result[1], "OK");
        answer();
      });
    }

    if (event.call.state == "disconnected") {
      log("Received 'callschanged' event for a disconnected call.");
      is(event.call, incoming);
      is(incoming.state, "disconnected");
      is(telephony.active, null);
      is(telephony.calls.length, 0);
      cleanUp();
    }
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

    // Incoming call is not 'active' until its state becomes 'connected'.
    isnot(incoming, telephony.active);
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
    });
  };
  incoming.hangUp();
}

function cleanUp() {
  telephony.oncallschanged = null;
  SpecialPowers.removePermission("telephony", document);
  finish();
}

getExistingCalls();
