/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const call1Number = "0900000001";
const call2Number = "0900000002";
const call1Info = gOutCallStrPool(call1Number);
const call2Info = gOutCallStrPool(call2Number);

// call1 is held, call2 is active
function setupTwoCalls() {
  let call1;
  let call2;

  return gDial(call1Number)
    .then(call => call1 = call)
    .then(() => gRemoteAnswer(call1))
    .then(() => gDial(call2Number))
    .then(call => call2 = call)
    .then(() => gRemoteAnswer(call2))
    .then(() => gCheckAll(call2, [call1, call2], "", [],
                          [call1Info.held, call2Info.active]))
    .then(() => [call1, call2]);
}

function testInCallMMI_0() {
  log('= testInCallMMI_0 =');

  return setupTwoCalls()
    .then(calls => [call1, call2] = calls)
    // Hangup held call.
    .then(() => gSendMMI("0"))
    .then(() => gWaitForNamedStateEvent(call1, "disconnected"))
    .then(() => gCheckAll(call2, [call2], "", [], [call2Info.active]))
    .then(() => gRemoteHangUpCalls([call2]));
}

function testInCallMMI_1() {
  log('= testInCallMMI_1 =');

  return setupTwoCalls()
    .then(calls => [call1, call2] = calls)
    // Hangup current call, resume held call.
    .then(() => gSendMMI("1"))
    .then(() => {
      let p1 = gWaitForNamedStateEvent(call1, "connected");
      let p2 = gWaitForNamedStateEvent(call2, "disconnected");
      return Promise.all([p1, p2]);
    })
    .then(() => gCheckAll(call1, [call1], "", [], [call1Info.active]))
    .then(() => gRemoteHangUpCalls([call1]));
}

function testInCallMMI_2() {
  log('= testInCallMMI_2 =');

  return setupTwoCalls()
    .then(calls => [call1, call2] = calls)
    // Hold current call, resume held call.
    .then(() => gSendMMI("2"))
    .then(() => {
      let p1 = gWaitForNamedStateEvent(call1, "connected");
      let p2 = gWaitForNamedStateEvent(call2, "held");
      return Promise.all([p1, p2]);
    })
    .then(() => gCheckAll(call1, [call1, call2], "", [],
                          [call1Info.active, call2Info.held]))
    // Hold current call, resume held call.
    .then(() => gSendMMI("2"))
    .then(() => {
      let p1 = gWaitForNamedStateEvent(call1, "held");
      let p2 = gWaitForNamedStateEvent(call2, "connected");
      return Promise.all([p1, p2]);
    })
    .then(() => gCheckAll(call2, [call1, call2], "", [],
                          [call1Info.held, call2Info.active]))
    .then(() => gRemoteHangUpCalls([call1, call2]));
}

startTest(function() {
  testInCallMMI_0()
    .then(() => testInCallMMI_1())
    .then(() => testInCallMMI_2())
    .catch(error => ok(false, "Promise reject: " + error))
    .then(finish);
});
