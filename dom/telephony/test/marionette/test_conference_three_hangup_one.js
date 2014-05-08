/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

function testConferenceThreeAndHangupOne() {
  log('= testConferenceThreeAndHangupOne =');

  let outCall;
  let inCall;
  let inCall2;
  let outNumber = "5555550101";
  let inNumber  = "5555550201";
  let inNumber2 = "5555550202";
  let inInfo = gInCallStrPool(inNumber);
  let inInfo2 = gInCallStrPool(inNumber2);

  return Promise.resolve()
    .then(() => gSetupConference([outNumber, inNumber, inNumber2]))
    .then(calls => {
      [outCall, inCall, inCall2] = calls;
    })
    .then(() => gHangUpCallInConference(outCall, [], [inCall, inCall2]))
    .then(() => gCheckAll(conference, [], 'connected', [inCall, inCall2],
                          [inInfo.active, inInfo2.active]))
    .then(() => gRemoteHangUpCalls([inCall, inCall2]));
}

// Start the test
startTest(function() {
  testConferenceThreeAndHangupOne()
    .then(null, error => {
      ok(false, 'promise rejects during test.');
    })
    .then(finish);
});
