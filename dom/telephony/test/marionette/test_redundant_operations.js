/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const inNumber = "5555552222";
const inInfo = gInCallStrPool(inNumber);
let inCall;

function error(event, action) {
  ok(false, "Received '" + event + "' event when " + action);
}

function checkUnexpected(msg, call, event1, event2, actionCallback) {
  let error1 = error.bind(this, event1, msg);
  let error2 = error.bind(this, event2, msg);

  call.addEventListener(event1, error1);
  call.addEventListener(event2, error2);
  actionCallback();

  return gDelay(2000).then(() => {
    call.removeEventListener(event1, error1);
    call.removeEventListener(event2, error2);
  });
}

startTest(function() {
  gRemoteDial(inNumber)
    .then(call => inCall = call)
    .then(() => gAnswer(inCall))
    .then(() => checkUnexpected("answered an active call", inCall,
                                "connecting", "connected",
                                () => inCall.answer()))

    .then(() => gHold(inCall))
    .then(() => checkUnexpected("held a held call", inCall,
                                "holding", "held",
                                () => inCall.hold()))
    .then(() => checkUnexpected("answered a held call", inCall,
                                "connecting", "connected",
                                () => inCall.answer()))

    .then(() => gResume(inCall))
    .then(() => checkUnexpected("resumed non-held call", inCall,
                                "resuming", "connected",
                                () => inCall.resume()))

    .then(() => gHangUp(inCall))
    .then(() => checkUnexpected("answered a disconnected call", inCall,
                                "connecting", "connected",
                                () => inCall.answer()))
    .then(() => checkUnexpected("held a disconnected call", inCall,
                                "holding", "held",
                                () => inCall.hold()))
    .then(() => checkUnexpected("resumed a disconnected call", inCall,
                                "resuming", "connected",
                                () => inCall.resume()))
    .then(() => checkUnexpected("hang-up a disconnected call", inCall,
                                "disconnecting", "disconnected",
                                () => inCall.hangUp()))

    .catch(error => ok(false, "Promise reject: " + error))
    .then(finish);
});
