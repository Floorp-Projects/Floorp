/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

let outNumber = "5555551111";
let outgoingCall;

function dial() {
  log("Make an outgoing call.");
  telephony.dial(outNumber).then(call => {
    outgoingCall = call;
    outgoingCall.onalerting = function onalerting(event) {
      log("Received 'alerting' call event.");
      answer();
    };
  });
}

function answer() {
  log("Answering the outgoing call.");

  outgoingCall.onconnected = function onconnectedOut(event) {
    log("Received 'connected' call event for the original outgoing call.");
    // just some code to keep call active for awhile
    callStartTime = Date.now();
    waitFor(cleanUp,function() {
      callDuration = Date.now() - callStartTime;
      log("Waiting while call is active, call duration (ms): " + callDuration);
      return(callDuration >= 2000);
    });
  };
  emulator.runWithCallback("gsm accept " + outNumber);
}

function cleanUp(){
  outgoingCall.hangUp();
  ok("passed");
  finish();
}

startTest(function() {
  dial();
});
