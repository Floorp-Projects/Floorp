/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

function testConferenceTwoAndRemoveOne() {
  log('= testConferenceTwoAndRemoveOne =');

  let outCall;
  let inCall;
  let outNumber = "5555550101";
  let inNumber  = "5555550201";
  let outInfo = gOutCallStrPool(outNumber);
  let inInfo = gInCallStrPool(inNumber);

  return Promise.resolve()
    .then(() => gSetupConference([outNumber, inNumber]))
    .then(calls => {
      [outCall, inCall] = calls;
    })
    .then(() => gRemoveCallInConference(outCall, [inCall], [], function() {
      gCheckState(outCall, [outCall, inCall], '', []);
    }))
    .then(() => gCheckAll(outCall, [outCall, inCall], '', [],
                          [outInfo.active, inInfo.held]))
    .then(() => gRemoteHangUpCalls([outCall, inCall]));
}

// Start the test
startTest(function() {
  testConferenceTwoAndRemoveOne()
    .then(null, error => {
      ok(false, 'promise rejects during test.');
    })
    .then(finish);
});
