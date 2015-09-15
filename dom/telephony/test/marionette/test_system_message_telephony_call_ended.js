/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 90000;
MARIONETTE_HEAD_JS = 'head.js';

const inNumber = "5555552222";
const inInfo = gInCallStrPool(inNumber);
var inCall;

/**
 * telephony-call-ended message contains the following fields:
 *  - serviceId
 *  - number
 *  - emergency
 *  - duration
 *  - direction
 *  - hangUpLocal
 */

function testIncomingReject() {
  log("= testIncomingReject =");

  let p1 = gWaitForSystemMessage("telephony-call-ended")
    .then(message => {
      is(message.number, inNumber);
      is(message.duration, 0);
      is(message.direction, "incoming");
      is(message.hangUpLocal, true);
    });

  let p2 = gRemoteDial(inNumber)
    .then(call => inCall = call)
    .then(() => gHangUp(inCall));

  return Promise.all([p1, p2]);
}

function testIncomingCancel() {
  log("= testIncomingCancel =");

  let p1 = gWaitForSystemMessage("telephony-call-ended")
    .then(message => {
      is(message.number, inNumber);
      is(message.duration, 0);
      is(message.direction, "incoming");
      is(message.hangUpLocal, false);
    });

  let p2 = gRemoteDial(inNumber)
    .then(call => inCall = call)
    .then(() => gRemoteHangUp(inCall));

  return Promise.all([p1, p2]);
}

function testIncomingAnswerHangUp() {
  log("= testIncomingAnswerHangUp =");

  p1 = gWaitForSystemMessage("telephony-call-ended")
    .then(message => {
      is(message.number, inNumber);
      ok(message.duration > 0);
      is(message.direction, "incoming");
      is(message.hangUpLocal, true);
    });

  p2 = gRemoteDial(inNumber)
    .then(call => inCall = call)
    .then(() => gAnswer(inCall))
    .then(() => gHangUp(inCall));

  return Promise.all([p1, p2]);
}

function testIncomingAnswerRemoteHangUp() {
  log("= testIncomingAnswerRemoteHangUp =");

  p1 = gWaitForSystemMessage("telephony-call-ended")
    .then(message => {
      is(message.number, inNumber);
      ok(message.duration > 0);
      is(message.direction, "incoming");
      is(message.hangUpLocal, false);
    });

  p2 = gRemoteDial(inNumber)
    .then(call => inCall = call)
    .then(() => gAnswer(inCall))
    .then(() => gRemoteHangUp(inCall));

  return Promise.all([p1, p2]);
}

startTest(function() {
  Promise.resolve()
    .then(() => testIncomingReject())
    .then(() => testIncomingCancel())
    .then(() => testIncomingAnswerHangUp())
    .then(() => testIncomingAnswerRemoteHangUp())

    .catch(error => ok(false, "Promise reject: " + error))
    .then(finish);
});
