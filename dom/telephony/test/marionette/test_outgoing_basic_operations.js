/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 90000;
MARIONETTE_HEAD_JS = 'head.js';

const outNumber = "5555551111";
const outInfo = gOutCallStrPool(outNumber);
let outCall;

function outgoing() {
  return gDial(outNumber)
    .then(call => outCall = call)
    .then(() => gCheckAll(outCall, [outCall], "", [], [outInfo.ringing]))
    .then(() => is(outCall.disconnectedReason, null));
}

function answer() {
  return gRemoteAnswer(outCall)
    .then(() => gCheckAll(outCall, [outCall], "", [], [outInfo.active]))
    .then(() => is(outCall.disconnectedReason, null));
}

function hangUp() {
  return gHangUp(outCall)
    .then(() => gCheckAll(null, [], "", [], []))
    .then(() => is(outCall.disconnectedReason, "NormalCallClearing"));
}

function remoteHangUp() {
  return gRemoteHangUp(outCall)
    .then(() => gCheckAll(null, [], "", [], []))
    .then(() => is(outCall.disconnectedReason, "NormalCallClearing"));
}

function hold() {
  return gHold(outCall)
    .then(() => gCheckAll(null, [outCall], "", [], [outInfo.held]))
    .then(() => is(outCall.disconnectedReason, null));
}

function resume() {
  return gResume(outCall)
    .then(() => gCheckAll(outCall, [outCall], "", [], [outInfo.active]))
    .then(() => is(outCall.disconnectedReason, null));
}

// Test cases.

function testOutgoingReject() {
  log("= testOutgoingReject =");
  return outgoing()
    .then(() => remoteHangUp());
}

function testOutgoingCancel() {
  log("= testOutgoingCancel =");
  return outgoing()
    .then(() => hangUp());
}

function testOutgoingAnswerHangUp() {
  log("= testOutgoingAnswerHangUp =");
  return outgoing()
    .then(() => answer())
    .then(() => hangUp());
}

function testOutgoingAnswerRemoteHangUp() {
  log("= testOutgoingAnswerRemoteHangUp =");
  return outgoing()
    .then(() => answer())
    .then(() => remoteHangUp());
}

function testOutgoingAnswerHoldHangUp() {
  log("= testOutgoingAnswerHoldHangUp =");
  return outgoing()
    .then(() => answer())
    .then(() => hold())
    .then(() => hangUp());
}

function testOutgoingAnswerHoldRemoteHangUp() {
  log("= testOutgoingAnswerHoldRemoteHangUp =");
  return outgoing()
    .then(() => answer())
    .then(() => hold())
    .then(() => remoteHangUp());
}

function testOutgoingAnswerHoldResumeHangUp() {
  log("= testOutgoingAnswerHoldResumeHangUp =");
  return outgoing()
    .then(() => answer())
    .then(() => hold())
    .then(() => resume())
    .then(() => hangUp());
}

function testOutgoingAnswerHoldResumeRemoteHangUp() {
  log("= testOutgoingAnswerHoldResumeRemoteHangUp =");
  return outgoing()
    .then(() => answer())
    .then(() => hold())
    .then(() => resume())
    .then(() => remoteHangUp());
}

startTest(function() {
  Promise.resolve()
    .then(() => testOutgoingReject())
    .then(() => testOutgoingCancel())
    .then(() => testOutgoingAnswerHangUp())
    .then(() => testOutgoingAnswerRemoteHangUp())
    .then(() => testOutgoingAnswerHoldHangUp())
    .then(() => testOutgoingAnswerHoldRemoteHangUp())
    .then(() => testOutgoingAnswerHoldResumeHangUp())
    .then(() => testOutgoingAnswerHoldResumeRemoteHangUp())

    .catch(error => ok(false, "Promise reject: " + error))
    .then(finish);
});
