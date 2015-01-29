/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

function testAutoHoldCall() {
  log('= testAutoHoldCall =');

  let outCall1;
  let outCall2;
  const callNumber1 = "0900000001";
  const callNumber2 = "0900000002";

  return Promise.resolve()
    .then(() => gDial(callNumber1))
    .then(call => { outCall1 = call; })
    .then(() => gRemoteAnswer(outCall1))
    .then(() => {
      is(outCall1.state, "connected");
    })
    .then(() => gDial(callNumber2))
    .then(call => { outCall2 = call; })
    .then(() => {
      is(outCall1.state, "held");
    })
    .then(() => gRemoteHangUpCalls([outCall1, outCall2]));
}


function testAutoHoldCallFailed() {
  log('= testAutoHoldCallFailed =');

  let outCall1;
  let outCall2;
  const callNumber1 = "0900000011";
  const callNumber2 = "0900000012";

  return Promise.resolve()
    .then(() => emulator.runCmd("gsm disable hold"))
    .then(() => gDial(callNumber1))
    .then(call => { outCall1 = call; })
    .then(() => gRemoteAnswer(outCall1))
    .then(() => {
      is(outCall1.state, "connected");
    })
    .then(() => gDial(callNumber2))
    .then(call => {
      ok(false, "The second |dial()| should be rejected.");
      outCall2 = call;
      return gRemoteHangUpCalls([outCall2]);
    }, () => log("The second |dial()| is rejected as expected."))
    .then(() => gRemoteHangUpCalls([outCall1]))
    .then(() => emulator.runCmd("gsm enable hold"));
}

function testAutoHoldConferenceCall() {
  log('= testAutoHoldConferenceCall =');

  let subCall1;
  let subCall2;
  let outCall;
  const subNumber1 = "0900000021";
  const subNumber2 = "0900000022";
  const outNumber = "0900000023";

  return Promise.resolve()
    .then(() => gSetupConference([subNumber1, subNumber2]))
    .then(calls => {
      [subCall1, subCall2] = calls;
      is(conference.state, "connected");
    })
    .then(() => gDial(outNumber))
    .then(call => { outCall = call; })
    .then(() => {
      is(subCall1.state, "held");
      is(subCall2.state, "held");
      is(conference.state, "held");
    })
    .then(() => gRemoteHangUpCalls([subCall1, subCall2, outCall]));
}

startTest(function() {
  Promise.resolve()
    .then(() => testAutoHoldCall())
    .then(() => testAutoHoldCallFailed())
    .then(() => testAutoHoldConferenceCall())
    .catch(error => {
      ok(false, "Promise reject: " + error);
      emulator.runCmd("gsm enable hold");
    })
    .then(finish);
});
