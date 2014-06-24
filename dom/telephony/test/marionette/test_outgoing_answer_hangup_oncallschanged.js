/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

let number = "5555552368";
let outgoing;

function dial() {
  log("Make an outgoing call.");

  telephony.oncallschanged = function oncallschanged(event) {
    log("Received 'callschanged' call event.");

    if (!event.call) {
      log("Notifying calls array is loaded. No call information accompanies.");
      return;
    }

    let expected_states = ["dialing", "disconnected"];
    ok(expected_states.indexOf(event.call.state) != -1,
      "Unexpected call state: " + event.call.state);

    if (event.call.state == "dialing") {
      outgoing = event.call;
      ok(outgoing);
      is(outgoing.id.number, number);

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
  emulator.runWithCallback("gsm list", function(result) {
    log("Call list is now: " + result);
    if (((result[0] == "outbound to  " + number + " : unknown") ||
         (result[0] == "outbound to  " + number + " : dialing"))) {
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

    emulator.runWithCallback("gsm list", function(result) {
      log("Call list (after 'connected' event) is now: " + result);
      is(result[0], "outbound to  " + number + " : active");
      hangUp();
    });
  };
  emulator.runWithCallback("gsm accept " + number);
}

function hangUp() {
  log("Hanging up the outgoing call.");

  emulator.runWithCallback("gsm cancel " + number);
}

function cleanUp() {
  telephony.oncallschanged = null;
  finish();
}

startTest(function() {
  dial();
});
