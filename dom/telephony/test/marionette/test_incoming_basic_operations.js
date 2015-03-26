/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 90000;
MARIONETTE_HEAD_JS = 'head.js';

const inNumber = "5555552222";
const inInfo = gInCallStrPool(inNumber);
let inCall;

function incoming() {
  return gRemoteDial(inNumber)
    .then(call => inCall = call)
    .then(() => gCheckAll(null, [inCall], "", [], [inInfo.incoming]))
    .then(() => is(inCall.disconnectedReason, null));
}

function answer() {
  return gAnswer(inCall)
    .then(() => gCheckAll(inCall, [inCall], "", [], [inInfo.active]))
    .then(() => is(inCall.disconnectedReason, null));
}

function hold() {
  return gHold(inCall)
    .then(() => gCheckAll(null, [inCall], "", [], [inInfo.held]))
    .then(() => is(inCall.disconnectedReason, null));
}

function resume() {
  return gResume(inCall)
    .then(() => gCheckAll(inCall, [inCall], "", [], [inInfo.active]))
    .then(() => is(inCall.disconnectedReason, null));
}

function hangUp() {
  return gHangUp(inCall)
    .then(() => gCheckAll(null, [], "", [], []))
    .then(() => is(inCall.disconnectedReason, "NormalCallClearing"));
}

function remoteHangUp() {
  return gRemoteHangUp(inCall)
    .then(() => gCheckAll(null, [], "", [], []))
    .then(() => is(inCall.disconnectedReason, "NormalCallClearing"));
}

// Test cases.

function testIncomingReject() {
  log("= testIncomingReject =");
  return incoming()
    .then(() => hangUp());
}

function testIncomingCancel() {
  log("= testIncomingCancel =");
  return incoming()
    .then(() => remoteHangUp());
}

function testIncomingAnswerHangUp() {
  log("= testIncomingAnswerHangUp =");
  return incoming()
    .then(() => answer())
    .then(() => hangUp());
}

function testIncomingAnswerRemoteHangUp() {
  log("= testIncomingAnswerRemoteHangUp =");
  return incoming()
    .then(() => answer())
    .then(() => remoteHangUp());
}

function testIncomingAnswerHoldHangUp() {
  log("= testIncomingAnswerHoldHangUp =");
  return incoming()
    .then(() => answer())
    .then(() => hold())
    .then(() => hangUp());
}

function testIncomingAnswerHoldRemoteHangUp() {
  log("= testIncomingAnswerHoldRemoteHangUp =");
  return incoming()
    .then(() => answer())
    .then(() => hold())
    .then(() => remoteHangUp());
}

function testIncomingAnswerHoldResumeHangUp() {
  log("= testIncomingAnswerHoldResumeHangUp =");
  return incoming()
    .then(() => answer())
    .then(() => hold())
    .then(() => resume())
    .then(() => hangUp());
}

function testIncomingAnswerHoldResumeRemoteHangUp() {
  log("= testIncomingAnswerHoldResumeRemoteHangUp =");
  return incoming()
    .then(() => answer())
    .then(() => hold())
    .then(() => resume())
    .then(() => remoteHangUp());
}

startTest(function() {
  Promise.resolve()
    .then(() => testIncomingReject())
    .then(() => testIncomingCancel())
    .then(() => testIncomingAnswerHangUp())
    .then(() => testIncomingAnswerRemoteHangUp())
    .then(() => testIncomingAnswerHoldHangUp())
    .then(() => testIncomingAnswerHoldRemoteHangUp())
    .then(() => testIncomingAnswerHoldResumeHangUp())
    .then(() => testIncomingAnswerHoldResumeRemoteHangUp())

    .catch(error => ok(false, "Promise reject: " + error))
    .then(finish);
});
