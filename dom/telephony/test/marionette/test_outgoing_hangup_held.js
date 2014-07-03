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

      emulator.runCmdWithCallback("gsm list", function(result) {
        log("Call list is now: " + result);
        is(result[0], "outbound to  " + number + " : ringing");
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

    emulator.runCmdWithCallback("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "outbound to  " + number + " : active");
      hold();
    });
  };
  emulator.runCmdWithCallback("gsm accept " + number);
}

function hold() {
  log("Holding the outgoing call.");

  outgoing.onholding = function onholding(event) {
    log("Received 'holding' call event.");
    is(outgoing, event.call);
    is(outgoing.state, "holding");

    is(outgoing, telephony.active);
  };

  outgoing.onheld = function onheld(event) {
    log("Received 'held' call event.");
    is(outgoing, event.call);
    is(outgoing.state, "held");

    is(telephony.active, null);
    is(telephony.calls.length, 1);

    emulator.runCmdWithCallback("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "outbound to  " + number + " : held");
      hangUp();
    });
  };
  outgoing.hold();
}

function hangUp() {
  log("Hanging up the outgoing call.");

  let gotDisconnecting = false;
  outgoing.ondisconnecting = function ondisconnecting(event) {
    log("Received disconnecting call event.");
    is(outgoing, event.call);
    is(outgoing.state, "disconnecting");
    gotDisconnecting = true;
  };

  outgoing.ondisconnected = function ondisconnected(event) {
    log("Received 'disconnected' call event.");
    is(outgoing, event.call);
    is(outgoing.state, "disconnected");
    ok(gotDisconnecting);

    is(telephony.active, null);
    is(telephony.calls.length, 0);

    emulator.runCmdWithCallback("gsm list", function(result) {
      log("Call list is now: " + result);
      cleanUp();
    });
  };

  outgoing.hangUp();
}

function cleanUp() {
  finish();
}

startTest(function() {
  dial();
});
