/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

function handleConferenceAddError(callToAdd) {
  log('Handle conference add error.');

  let deferred = Promise.defer();

  conference.onerror = function(evt) {
    log('Receiving a conference error event.');
    conference.onerror = null;

    is(evt.name, 'addError', 'conference addError');

    deferred.resolve();
  };

  return conference.add(callToAdd)
    .then(() => ok(false, "|conference.add| should be rejected"),
          () => log("|conference.add| is rejected as expected"))
    .then(() => deferred.promise);
}

function testConferenceAddError() {
  log('= testConferenceAddError =');

  let outCall, inCall, inCall2, inCall3, inCall4, inCall5;
  let outNumber = "5555550101";
  let inNumber  = "5555550201";
  let inNumber2 = "5555550202";
  let inNumber3 = "5555550203";
  let inNumber4 = "5555550204";
  let inNumber5 = "5555550205";
  let outInfo = gOutCallStrPool(outNumber);
  let inInfo = gInCallStrPool(inNumber);
  let inInfo2 = gInCallStrPool(inNumber2);
  let inInfo3 = gInCallStrPool(inNumber3);
  let inInfo4 = gInCallStrPool(inNumber4);
  let inInfo5 = gInCallStrPool(inNumber5);

  return Promise.resolve()
    .then(() => gSetupConference([outNumber, inNumber, inNumber2, inNumber3,
                                 inNumber4]))
    .then(calls => {
      [outCall, inCall, inCall2, inCall3, inCall4] = calls;
    })
    .then(() => gRemoteDial(inNumber5))
    .then(call => {inCall5 = call;})
    .then(() => gAnswer(inCall5, function() {
      gCheckState(inCall5, [inCall5], 'held',
                  [outCall, inCall, inCall2, inCall3, inCall4]);
    }))
    .then(() => gCheckAll(inCall5, [inCall5], 'held',
                          [outCall, inCall, inCall2, inCall3, inCall4],
                          [outInfo.held, inInfo.held, inInfo2.held,
                           inInfo3.held, inInfo4.held, inInfo5.active]))
    // Maximum number of conference participants is 5.
    .then(() => handleConferenceAddError(inCall5))
    .then(() => gRemoteHangUpCalls([outCall, inCall, inCall2, inCall3, inCall4,
                                    inCall5]));
}

// Start the test
startTest(function() {
  testConferenceAddError()
    .catch(error => ok(false, "Promise reject: " + error))
    .then(finish);
});
