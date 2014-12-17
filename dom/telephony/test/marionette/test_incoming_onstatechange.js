/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_HEAD_JS = 'head.js';
MARIONETTE_TIMEOUT = 60000;

const inNumber = "5555552222";
const inInfo = gInCallStrPool(inNumber);
let inCall;

startTest(function() {
  gRemoteDial(inNumber)
    .then(call => inCall = call)
    .then(() => gCheckAll(null, [inCall], "", [], [inInfo.incoming]))

    // Answer incoming call
    .then(() => {
      let p1 = gWaitForStateChangeEvent(inCall, "connecting")
        .then(() => gWaitForStateChangeEvent(inCall, "connected"));
      let p2 = gAnswer(inCall);
      return Promise.all([p1, p2]);
    })
    .then(() => gCheckAll(inCall, [inCall], "", [], [inInfo.active]))

    // Hold the call.
    .then(() => {
      let p1 = gWaitForStateChangeEvent(inCall, "holding")
        .then(() => gWaitForStateChangeEvent(inCall, "held"));
      let p2 = gHold(inCall);
      return Promise.all([p1, p2]);
    })
    .then(() => gCheckAll(null, [inCall], "", [], [inInfo.held]))

    // Resume the call.
    .then(() => {
      let p1 = gWaitForStateChangeEvent(inCall, "resuming")
        .then(() => gWaitForStateChangeEvent(inCall, "connected"));
      let p2 = gResume(inCall);
      return Promise.all([p1, p2]);
    })
    .then(() => gCheckAll(inCall, [inCall], "", [], [inInfo.active]))

    // Hang-up call
    .then(() => {
      let p1 = gWaitForStateChangeEvent(inCall, "disconnecting")
        .then(() => gWaitForStateChangeEvent(inCall, "disconnected"));
      let p2 = gHangUp(inCall);
      return Promise.all([p1, p2]);
    })
    .then(() => gCheckAll(null, [], "", [], []))

    .catch(error => ok(false, "Promise reject: " + error))
    .then(finish);
});
