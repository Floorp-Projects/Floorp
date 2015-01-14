/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

function handleConferenceRemoveError(callToRemove) {
  log('Handle conference remove error.');

  let deferred = Promise.defer();

  conference.onerror = function(evt) {
    log('Receiving a conference error event.');
    conference.onerror = null;

    is(evt.name, 'removeError', 'conference removeError');

    deferred.resolve();
  };

  return conference.remove(callToRemove)
    .then(() => ok(false, "|conference.remove()| should be rejected"),
          () => log("|conference.remove()| is rejected as expected"))
    .then(() => deferred.promise);
}

function testConferenceRemoveError() {
  log('= testConferenceRemoveError =');

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
    .then(() => gSetupConference([outNumber, inNumber]))
    .then(calls => {
      [outCall, inCall] = calls;
    })
    .then(() => gRemoteDial(inNumber2))
    .then(call => {inCall2 = call;})
    .then(() => gCheckAll(conference, [inCall2], 'connected', [outCall, inCall],
                          [outInfo.active, inInfo.active, inInfo2.waiting]))
    .then(() => gAnswer(inCall2, function() {
      gCheckState(inCall2, [inCall2], 'held', [outCall, inCall]);
    }))
    .then(() => gCheckAll(inCall2, [inCall2], 'held', [outCall, inCall],
                          [outInfo.held, inInfo.held, inInfo2.active]))
    .then(() => gResumeConference([outCall, inCall], function() {
      gCheckState(conference, [inCall2], 'connected', [outCall, inCall]);
    }))
    // Not allowed to remove a call when there are one connected and one held
    // calls.
    .then(() => handleConferenceRemoveError(outCall))
    .then(() => gRemoteHangUpCalls([outCall, inCall, inCall2]));
}

// Start the test
startTest(function() {
  testConferenceRemoveError()
    .catch(error => ok(false, "Promise reject: " + error))
    .then(finish);
});
