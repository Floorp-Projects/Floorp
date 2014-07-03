/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

let number = "5555552368";
let outgoing;

function dial() {
  log("Make an outgoing call.");

  telephony.dial(number).then(call => {
    outgoing = call;
    ok(outgoing);
    is(outgoing.id.number, number);
    is(outgoing.state, "dialing");

    is(outgoing, telephony.active);
    is(telephony.calls.length, 1);
    is(telephony.calls[0], outgoing);

    outgoing.onalerting = function onalerting(event) {
      log("Received 'onalerting' call event.");
      is(outgoing, event.call);
      is(outgoing.state, "alerting");

      emulator.runCmdWithCallback("gsm list", function(result) {
        log("Call list is now: " + result);
        is(result[0], "outbound to  " + number + " : ringing");
        busy();
      });
    };
  });
}

function busy() {
  log("The receiver is busy.");

  outgoing.onerror = function onerror(event) {
    log("Received 'error' call event.");
    is(outgoing, event.call);
    is(event.call.error.name, "BusyError");

    emulator.runCmdWithCallback("gsm list", function(result) {
      log("Call list is now: " + result);
      cleanUp();
    });
  };

  emulator.runCmdWithCallback("gsm busy " + number);
}

function cleanUp() {
  finish();
}

startTest(function() {
  dial();
});
