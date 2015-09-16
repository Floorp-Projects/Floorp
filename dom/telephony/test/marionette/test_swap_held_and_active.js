/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const outNumber = "5555551111";
const outInfo = gOutCallStrPool(outNumber);
var outCall;

const inNumber = "5555552222";
const inInfo = gInCallStrPool(inNumber);
var inCall;

startTest(function() {
  gDial(outNumber)
    .then(call => outCall = call)
    .then(() => gRemoteAnswer(outCall))
    .then(() => gHold(outCall))

    .then(() => gRemoteDial(inNumber))
    .then(call => inCall = call)
    .then(() => gAnswer(inCall))
    .then(() => gCheckAll(inCall, [outCall, inCall], "", [],
                          [outInfo.held, inInfo.active]))

    // Swap calls by resuming the held (outgoing) call. Will force active
    // (incoming) call to hold.
    .then(() => {
      let p1 = gWaitForNamedStateEvent(inCall, "held");
      let p2 = gResume(outCall);
      return Promise.all([p1, p2]);
    })
    .then(() => gCheckAll(outCall, [outCall, inCall], "", [],
                          [outInfo.active, inInfo.held]))

    .then(() => gHangUp(outCall))
    .then(() => gHangUp(inCall))

    .catch(error => ok(false, "Promise reject: " + error))
    .then(finish);
});
