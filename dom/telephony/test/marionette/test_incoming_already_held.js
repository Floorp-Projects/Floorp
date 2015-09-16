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
    .then(() => gCheckAll(outCall, [outCall], "", [], [outInfo.ringing]))
    .then(() => gRemoteAnswer(outCall))
    .then(() => gCheckAll(outCall, [outCall], "", [], [outInfo.active]))
    .then(() => gHold(outCall))
    .then(() => gCheckAll(null, [outCall], "", [], [outInfo.held]))

    // With one call on hold, simulate an incoming call
    .then(() => gRemoteDial(inNumber))
    .then(call => inCall = call)
    .then(() => gCheckAll(null, [outCall, inCall], "", [],
                          [outInfo.held, inInfo.waiting]))

    // Answer incoming call; original outgoing call should be held
    .then(() => gAnswer(inCall))
    .then(() => gCheckAll(inCall, [outCall, inCall], "", [],
                          [outInfo.held, inInfo.active]))

    // Hang-up original outgoing (now held) call
    .then(() => gHangUp(outCall))
    .then(() => gCheckAll(inCall, [inCall], "", [], [inInfo.active]))

    // Hang-up remaining (incoming) call
    .then(() => gHangUp(inCall))
    .then(() => gCheckAll(null, [], "", [], []))

    .catch(error => ok(false, "Promise reject: " + error))
    .then(finish);
});
