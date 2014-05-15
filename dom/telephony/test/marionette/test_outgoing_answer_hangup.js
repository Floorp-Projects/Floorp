/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

let number = "5555552368";
let outgoing;
let calls;

function dial() {
  log("Make an outgoing call.");

  telephony.dial(number).then(call => {
    outgoing = call;
    ok(outgoing);
    is(outgoing.id.number, number);
    is(outgoing.state, "dialing");

    is(outgoing, telephony.active);
    //ok(telephony.calls === calls); // bug 717414
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
        answer();
      });
    };
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

    emulator.run("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "outbound to  " + number + " : active");
      is(result[1], "OK");
      hangUp();
    });
  };
  emulator.run("gsm accept " + number);
}

function hangUp() {
  log("Hanging up the outgoing call.");

  // We get no "disconnecting" event when the remote party terminates the call.

  outgoing.ondisconnected = function ondisconnected(event) {
    log("Received 'disconnected' call event.");
    is(outgoing, event.call);
    is(outgoing.state, "disconnected");

    is(telephony.active, null);
    is(telephony.calls.length, 0);

    emulator.run("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "OK");
      cleanUp();
    });
  };
  emulator.run("gsm cancel " + number);
}

function cleanUp() {
  finish();
}

startTest(function() {
  dial();
});
