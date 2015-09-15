/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_HEAD_JS = 'head.js';
MARIONETTE_TIMEOUT = 60000;

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

    // Hold the call.
    .then(() => gHold(inCall))
    .then(() => gCheckAll(null, [inCall], "", [], [inInfo.held]))

    // Resume the call.
    .then(() => gResume(inCall))
    .then(() => gCheckAll(inCall, [inCall], "", [], [inInfo.active]))

    // Hang-up call
    .then(() => gHangUp(inCall))
    .then(() => gCheckAll(null, [], "", [], []))

    .catch(error => ok(false, "Promise reject: " + error))
    .then(finish);
});
