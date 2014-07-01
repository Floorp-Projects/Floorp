/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

function muxModem(id) {
  let deferred = Promise.defer();

  emulator.runWithCallback("mux modem " + id, function() {
    deferred.resolve();
  });

  return deferred.promise;
}

function testNewCallWhenOtherConnectionInUse(firstServiceId, secondServiceId) {
  log("= testNewCallWhenOtherConnectionInUse =");
  log("1st call on " + firstServiceId + ", 2nd call on " + secondServiceId);

  let outCall;

  return Promise.resolve()
    .then(() => muxModem(firstServiceId))
    .then(() => {
      return telephony.dial("0912345000", firstServiceId);
    })
    .then(call => {
      outCall = call;
      is(outCall.serviceId, firstServiceId);
    })
    .then(() => gRemoteAnswer(outCall))
    .then(() => {
      return telephony.dial("0912345001", secondServiceId);
    })
    .then(() => {
      log("The promise should not be resolved");
      ok(false);
    }, cause => {
      is(cause, "OtherConnectionInUse");
    })
    .then(() => gRemoteHangUp(outCall));
}

startDSDSTest(function() {
  testNewCallWhenOtherConnectionInUse(0, 1)
    .then(() => testNewCallWhenOtherConnectionInUse(1, 0))
    .then(() => muxModem(0))
    .then(null, () => {
      ok(false, "promise rejects during test.");
    })
    .then(finish);
});
