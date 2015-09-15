/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const inNumber = "5555552222";
const inInfo = gInCallStrPool(inNumber);
var inCall;

function error(aEvent, aAction) {
  ok(false, "Received '" + aEvent + "' event when " + aAction);
}

function checkUnexpected(aMsg, aCall, aEvent, aActionCallback) {
  let handler = error.bind(this, aEvent, aMsg);
  aCall.addEventListener(aEvent, handler);

  return aActionCallback().then(
    () => ok(false, msg + "should be rejected."),
    () => gDelay(2000).then(() => aCall.removeEventListener(aEvent, handler)));
}

startTest(function() {
  gRemoteDial(inNumber)
    .then(call => inCall = call)
    .then(() => gAnswer(inCall))
    .then(() => checkUnexpected("answered an active call", inCall,
                                "connected", () => inCall.answer()))
    .then(() => gHold(inCall))
    .then(() => checkUnexpected("held a held call", inCall,
                                "held", () => inCall.hold()))
    .then(() => checkUnexpected("answered a held call", inCall,
                                "connected", () => inCall.answer()))
    .then(() => gResume(inCall))
    .then(() => checkUnexpected("resumed non-held call", inCall,
                                "connected", () => inCall.resume()))
    .then(() => gHangUp(inCall))
    .then(() => checkUnexpected("answered a disconnected call", inCall,
                                "connected", () => inCall.answer()))
    .then(() => checkUnexpected("held a disconnected call", inCall,
                                "held", () => inCall.hold()))
    .then(() => checkUnexpected("resumed a disconnected call", inCall,
                                "connected", () => inCall.resume()))
    .then(() => checkUnexpected("hang-up a disconnected call", inCall,
                                "disconnected", () => inCall.hangUp()))
    .catch(error => ok(false, "Promise reject: " + error))
    .then(finish);
});
