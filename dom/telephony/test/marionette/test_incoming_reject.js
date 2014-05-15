/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

let number = "5555552368";
let incoming;
let calls;

function simulateIncoming() {
  log("Simulating an incoming call.");

  telephony.onincoming = function onincoming(event) {
    log("Received 'incoming' call event.");
    incoming = event.call;
    ok(incoming);
    is(incoming.id.number, number);
    is(incoming.state, "incoming");

    //ok(telephony.calls === calls); // bug 717414
    is(telephony.calls.length, 1);
    is(telephony.calls[0], incoming);

    emulator.run("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "inbound from " + number + " : incoming");
      is(result[1], "OK");
      reject();
    });
  };
  emulator.run("gsm call " + number);
}

function reject() {
  log("Reject the incoming call.");

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

    emulator.run("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "OK");
      cleanUp();
    });
  };
  incoming.hangUp();
}

function cleanUp() {
  telephony.onincoming = null;
  finish();
}

startTest(function() {
  simulateIncoming();
});
