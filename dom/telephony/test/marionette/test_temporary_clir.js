/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const clirPrefix = "*31#";
const number = "0912345678";

startTest(function() {
  let outCall;

  telephony.dial(clirPrefix + number).then(call => {
    outCall = call;

    ok(call);
    is(call.id.number, number);
    is(call.state, "dialing");

    let deferred = Promise.defer();

    call.onalerting = function onalerting(event) {
      call.onalerting = null;
      log("Received 'onalerting' call event.");
      is(call.id.number, number);
      deferred.resolve(call);
    };

    return deferred.promise;
  })
  .then(() => gRemoteHangUp(outCall))
  // End
  .then(null, error => {
    ok(false, 'promise rejects during test.');
  })
  .then(finish);
});
