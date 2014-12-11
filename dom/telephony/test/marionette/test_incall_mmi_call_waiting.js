/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const outNumber = "0900000001";
const inNumber = "0900000002";
const outInfo = gOutCallStrPool(outNumber);
const inInfo = gInCallStrPool(inNumber);

function setupTwoCalls() {
  let outCall;
  let inCall;

  return gDial(outNumber)
    .then(call => outCall = call)
    .then(() => gRemoteAnswer(outCall))
    .then(() => gRemoteDial(inNumber))
    .then(call => inCall = call)
    .then(() => gCheckAll(outCall, [outCall, inCall], "", [],
                          [outInfo.active, inInfo.waiting]))
    .then(() => [outCall, inCall]);
}

function testInCallMMI_0() {
  log('= testInCallMMI_0 =');

  return setupTwoCalls()
    .then(calls => [outCall, inCall] = calls)
    // Hangup waiting call.
    .then(() => gSendMMI("0"))
    .then(() => gWaitForNamedStateEvent(inCall, "disconnected"))
    .then(() => gCheckAll(outCall, [outCall], "", [], [outInfo.active]))
    .then(() => gRemoteHangUpCalls([outCall]));
}

function testInCallMMI_1() {
  log('= testInCallMMI_1 =');

  return setupTwoCalls()
    .then(calls => [outCall, inCall] = calls)
    // Hangup current call, accept waiting call.
    .then(() => gSendMMI("1"))
    .then(() => {
      let p1 = gWaitForNamedStateEvent(outCall, "disconnected");
      let p2 = gWaitForNamedStateEvent(inCall, "connected");
      return Promise.all([p1, p2]);
    })
    .then(() => gCheckAll(inCall, [inCall], "", [], [inInfo.active]))
    .then(() => gRemoteHangUpCalls([inCall]));
}

function testInCallMMI_2() {
  log('= testInCallMMI_2 =');

  return setupTwoCalls()
    .then(calls => [outCall, inCall] = calls)
    // Hold current call, accept waiting call.
    .then(() => gSendMMI("2"))
    .then(() => {
      let p1 = gWaitForNamedStateEvent(outCall, "held");
      let p2 = gWaitForNamedStateEvent(inCall, "connected");
      return Promise.all([p1, p2]);
    })
    .then(() => gCheckAll(inCall, [outCall, inCall], "", [],
                          [outInfo.held, inInfo.active]))
    .then(() => gRemoteHangUpCalls([outCall, inCall]));
}

startTest(function() {
  testInCallMMI_0()
    .then(() => testInCallMMI_1())
    .then(() => testInCallMMI_2())
    .catch(error => ok(false, "Promise reject: " + error))
    .then(finish);
});
