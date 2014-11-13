/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

function testConferenceHoldAndResume() {
  log('= testConferenceHoldAndResume =');

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
    .then(() => gHoldConference([outCall, inCall], function() {
      gCheckState(null, [], 'held', [outCall, inCall]);
    }))
    .then(() => gCheckAll(null, [], 'held', [outCall, inCall],
                          [outInfo.held, inInfo.held]))
    .then(() => gResumeConference([outCall, inCall], function() {
      gCheckState(conference, [], 'connected', [outCall, inCall]);
    }))
    .then(() => gCheckAll(conference, [], 'connected', [outCall, inCall],
                          [outInfo.active, inInfo.active]))
    .then(() => gRemoteHangUpCalls([outCall, inCall]));
}

// Start the test
startTest(function() {
  testConferenceHoldAndResume()
    .catch(error => ok(false, "Promise reject: " + error))
    .then(finish);
});
