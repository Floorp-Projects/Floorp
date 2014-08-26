/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

function testAutoHoldCall() {
  let outCall1;
  let outCall2;

  return gDial("0900000001")
    .then(call => { outCall1 = call; })
    .then(() => gRemoteAnswer(outCall1))
    .then(() => {
      is(outCall1.state, "connected");
    })
    .then(() => gDial("0900000002"))
    .then(call => { outCall2 = call; })
    .then(() => {
      is(outCall1.state, "held");
    })
    .then(() => gRemoteHangUpCalls([outCall1, outCall2]));
}

function testAutoHoldConferenceCall() {
  let subCall1;
  let subCall2;
  let outCall;

  return gSetupConference(["0900000001", "0900000002"])
    .then(calls => {
      [subCall1, subCall2] = calls;
      is(conference.state, "connected");
    })
    .then(() => gDial("0900000003"))
    .then(call => { outCall = call; })
    .then(() => {
      is(subCall1.state, "held");
      is(subCall2.state, "held");
      is(conference.state, "held");
    })
    .then(() => gRemoteHangUpCalls([subCall1, subCall2, outCall]));
}

startTest(function() {
  testAutoHoldCall()
    .then(() => testAutoHoldConferenceCall())
    .then(null, () => {
      ok(false, 'promise rejects during test.');
    })
    .then(finish);
});
