/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

SpecialPowers.addPermission("telephony", true, document);

let telephony = window.navigator.mozTelephony;
let outNumber = "5555551111";
let outgoingCall;

function getExistingCalls() {
  runEmulatorCmd("gsm list", function(result) {
    log("Initial call list: " + result);
    if (result[0] == "OK") {
      dial();
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
    waitFor(dial, function() {
      return (telephony.calls.length == 0);
    });
  };
}

function dial() {
  log("Make an outgoing call.");
  outgoingCall = telephony.dial(outNumber);

  outgoingCall.onalerting = function onalerting(event) {
    log("Received 'alerting' call event.");
    answer();
  };  
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
  runEmulatorCmd("gsm accept " + outNumber);
}

function cleanUp(){
  outgoingCall.hangUp();
  ok("passed");
  finish();
}

getExistingCalls();