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
    .then(() => gAnswer(inCall))
    .then(() => gCheckAll(inCall, [inCall], "", [], [inInfo.active]))
    .then(() => gHold(inCall))
    .then(() => gCheckAll(null, [inCall], "", [], [inInfo.held]))

    // With one call on hold, make outgoing call
    .then(() => gDial(outNumber))
    .then(call => outCall = call)
    .then(() => gCheckAll(outCall, [inCall, outCall], "", [],
                          [inInfo.held, outInfo.ringing]))
    .then(() => gRemoteAnswer(outCall))
    .then(() => gCheckAll(outCall, [inCall, outCall], "", [],
                          [inInfo.held, outInfo.active]))

    // Hang-up
    .then(() => gHangUp(inCall))
    .then(() => gCheckAll(outCall, [outCall], "", [], [outInfo.active]))
    .then(() => gHangUp(outCall))
    .then(() => gCheckAll(null, [], "", [], []))

    .catch(error => ok(false, "Promise reject: " + error))
    .then(finish);
});
