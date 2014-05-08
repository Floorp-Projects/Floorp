/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

function testConferenceTwoAndHangupOne() {
  log('= testConferenceTwoAndHangupOne =');

  let outCall;
  let inCall;
  let outNumber = "5555550101";
  let inNumber  = "5555550201";
  let inInfo = gInCallStrPool(inNumber);

  return Promise.resolve()
    .then(() => gSetupConference([outNumber, inNumber]))
    .then(calls => {
      [outCall, inCall] = calls;
    })
    .then(() => gHangUpCallInConference(outCall, [inCall], [], function() {
      gCheckState(inCall, [inCall], '', []);
    }))
    .then(() => gCheckAll(inCall, [inCall], '', [], [inInfo.active]))
    .then(() => gRemoteHangUpCalls([inCall]));
}

// Start the test
startTest(function() {
  testConferenceTwoAndHangupOne()
    .then(null, error => {
      ok(false, 'promise rejects during test.');
    })
    .then(finish);
});
