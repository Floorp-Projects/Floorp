/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

function testConferenceThreeAndRemoveOne() {
  log('= testConferenceThreeAndRemoveOne =');

  let outCall;
  let inCall;
  let inCall2;
  let outNumber = "5555550101";
  let inNumber  = "5555550201";
  let inNumber2 = "5555550202";
  let outInfo = gOutCallStrPool(outNumber);
  let inInfo = gInCallStrPool(inNumber);
  let inInfo2 = gInCallStrPool(inNumber2);

  return Promise.resolve()
    .then(() => gSetupConference([outNumber, inNumber, inNumber2]))
    .then(calls => {
      [outCall, inCall, inCall2] = calls;
    })
    .then(() => gRemoveCallInConference(outCall, [], [inCall, inCall2],
                                        function() {
        gCheckState(outCall, [outCall], 'held', [inCall, inCall2]);
    }))
    .then(() => gCheckAll(outCall, [outCall], 'held', [inCall, inCall2],
                          [outInfo.active, inInfo.held, inInfo2.held]))
    .then(() => gRemoteHangUpCalls([outCall, inCall, inCall2]));
}

// Start the test
startTest(function() {
  testConferenceThreeAndRemoveOne()
    .catch(error => ok(false, "Promise reject: " + error))
    .then(finish);
});
