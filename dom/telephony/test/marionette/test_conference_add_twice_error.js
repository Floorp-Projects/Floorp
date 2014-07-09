/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

function testConferenceTwoCallsTwice() {
  log('= testConferenceTwoCallsTwice =');

  let outCall;
  let inCall;
  let outNumber = "5555550101";
  let inNumber  = "5555550201";
  let outInfo = gOutCallStrPool(outNumber);
  let inInfo = gInCallStrPool(inNumber);

  let sendConferenceTwice = true;

  return Promise.resolve()
    .then(gCheckInitialState)
    .then(() => gDial(outNumber))
    .then(call => { outCall = call; })
    .then(() => gCheckAll(outCall, [outCall], '', [], [outInfo.ringing]))
    .then(() => gRemoteAnswer(outCall))
    .then(() => gCheckAll(outCall, [outCall], '', [], [outInfo.active]))
    .then(() => gRemoteDial(inNumber))
    .then(call => { inCall = call; })
    .then(() => gCheckAll(outCall, [outCall, inCall], '', [],
                          [outInfo.active, inInfo.incoming]))
    .then(() => gAnswer(inCall))
    .then(() => gCheckAll(inCall, [outCall, inCall], '', [],
                          [outInfo.held, inInfo.active]))
    .then(() => gAddCallsToConference([outCall, inCall], function() {
      gCheckState(conference, [], 'connected', [outCall, inCall]);
    }, sendConferenceTwice))
    .then(() => gCheckAll(conference, [], 'connected', [outCall, inCall],
                          [outInfo.active, inInfo.active]))
    .then(() => gRemoteHangUpCalls([outCall, inCall]));
}

// Start the test
startTest(function() {
  testConferenceTwoCallsTwice()
    .then(null, error => {
      ok(false, 'promise rejects during test.');
    })
    .then(finish);
});
