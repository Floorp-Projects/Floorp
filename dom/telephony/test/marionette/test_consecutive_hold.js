/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 90000;
MARIONETTE_HEAD_JS = 'head.js';

const inNumber = "5555552222";
const inInfo = gInCallStrPool(inNumber);
let inCall;

startTest(function() {
  Promise.resolve()

    // Incoming
    .then(() => gRemoteDial(inNumber))
    .then(call => inCall = call)
    .then(() => gCheckAll(null, [inCall], "", [], [inInfo.incoming]))
    .then(() => is(inCall.disconnectedReason, null))

    // Answer
    .then(() => gAnswer(inCall))
    .then(() => gCheckAll(inCall, [inCall], "", [], [inInfo.active]))
    .then(() => is(inCall.disconnectedReason, null))

    // Disable the Hold function of the emulator, then hold the active call,
    // where the hold request will fail and the call will remain active.
    .then(() => emulator.runCmd("gsm disable hold"))
    .then(() => gHold(inCall))
    .then(() => ok(false, "This hold request should be rejected."),
          () => log("This hold request is rejected as expected."))
    .then(() => gCheckAll(inCall, [inCall], "", [], [inInfo.active]))
    .then(() => is(inCall.disconnectedReason, null))

    // Enable the Hold function of the emulator, then hold the active call,
    // where the hold request should succeed and the call should become held.
    .then(() => emulator.runCmd("gsm enable hold"))
    .then(() => gHold(inCall))
    .then(() => log("This hold request is resolved as expected."),
          () => ok(false, "This hold request should be resolved."))
    .then(() => gCheckAll(null, [inCall], "", [], [inInfo.held]))
    .then(() => is(inCall.disconnectedReason, null))

    // Hang up the call
    .then(() => gHangUp(inCall))
    .then(() => gCheckAll(null, [], "", [], []))
    .then(() => is(inCall.disconnectedReason, "NormalCallClearing"))

    // Clean Up
    .catch(error => ok(false, "Promise reject: " + error))
    .then(() => emulator.runCmd("gsm enable hold"))
    .then(finish);
});
