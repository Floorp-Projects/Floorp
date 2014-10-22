/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

function testConferenceHangUpForeground() {
  log('= testConferenceHangUpForeground =');

  let outCall;
  let inCall;
  let outNumber = "5555550101";
  let inNumber  = "5555550201";

  return Promise.resolve()
    .then(() => gSetupConference([outNumber, inNumber]))
    .then(calls => {
      [outCall, inCall] = calls;
    })
    .then(() => gHangUpConference())
    .then(() => gCheckAll(null, [], '', [], []));
}

function testConferenceHangUpBackground() {
  log('= testConferenceHangUpBackground =');

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
    .then(() => gHangUpConference())
    .then(() => gCheckAll(null, [], '', [], []));
}

// Start the test
startTest(function() {
  testConferenceHangUpForeground()
    .then(() => testConferenceHangUpBackground())
    .then(null, error => {
      ok(false, 'promise rejects during test.');
    })
    .then(finish);
});
