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

      emulator.run("gsm list", function(result) {
        log("Call list is now: " + result);
        is(result[0], "outbound to  " + number + " : ringing");
        is(result[1], "OK");
        reject();
      });
    };
  });
}

function reject() {
  log("Reject the outgoing call on the other end.");
  // We get no "disconnecting" event when the remote party rejects the call.

  outgoing.ondisconnected = function ondisconnected(event) {
    log("Received 'disconnected' call event.");
    is(outgoing, event.call);
    is(outgoing.state, "disconnected");

    is(telephony.active, null);
    is(telephony.calls.length, 0);

    // Wait for emulator to catch up before continuing
    waitFor(verifyCallList,function() {
      return(rcvdEmulatorCallback);
    });
  };

  let rcvdEmulatorCallback = false;
  emulator.run("gsm cancel " + number, function(result) {
    is(result[0], "OK", "emulator callback");
    rcvdEmulatorCallback = true;
  });
}

function verifyCallList(){
  emulator.run("gsm list", function(result) {
    log("Call list is now: " + result);
    is(result[0], "OK");
    cleanUp();
  });
}

function cleanUp() {
  finish();
}

startTest(function() {
  dial();
});
