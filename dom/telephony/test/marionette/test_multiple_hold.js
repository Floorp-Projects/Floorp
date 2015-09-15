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
  gRemoteDial(inNumber)
    .then(call => inCall = call)
    .then(() => gCheckAll(null, [inCall], "", [], [inInfo.incoming]))

    // Answer incoming call
    .then(() => gAnswer(inCall))
    .then(() => gCheckAll(inCall, [inCall], "", [], [inInfo.active]))

    // Hold the call
    .then(() => gHold(inCall))
    .then(() => gCheckAll(null, [inCall], "", [], [inInfo.held]))

    // Dial out.
    .then(() => gDial(outNumber))
    .then(call => outCall = call)
    .then(() => gCheckAll(outCall, [inCall, outCall], "", [],
                          [inInfo.held, outInfo.ringing]))

    // Remote answer the call
    .then(() => gRemoteAnswer(outCall))
    .then(() => gCheckAll(outCall, [inCall, outCall], "", [],
                          [inInfo.held, outInfo.active]))

    // With one held call and one active, hold the active one; expect the first
    // (held) call to automatically become active, and the 2nd call to be held
    .then(() => {
      let p1 = gWaitForNamedStateEvent(inCall, "connected");
      let p2 = gHold(outCall);
      return Promise.all([p1, p2]);
    })
    .then(() => gCheckAll(inCall, [inCall, outCall], "", [],
                          [inInfo.active, outInfo.held]))

    // Hangup the active call will automatically resume the held call.
    .then(() => {
      let p1 = gWaitForNamedStateEvent(outCall, "connected");
      let p2 = gHangUp(inCall);
      return Promise.all([p1, p2]);
    })
    .then(() => gCheckAll(outCall, [outCall], "", [], [outInfo.active]))
    .then(() => gHangUp(outCall))
    .then(() => gCheckAll(null, [], "", [], []))

    .catch(error => ok(false, "Promise reject: " + error))
    .then(finish);
});
