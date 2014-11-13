/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

function muxModem(id) {
  let deferred = Promise.defer();

  emulator.runCmdWithCallback("mux modem " + id, function() {
    deferred.resolve();
  });

  return deferred.promise;
}

function testOutgoingCallForServiceId(number, serviceId) {
  let outCall;
  let outInfo = gOutCallStrPool(number);

  return Promise.resolve()
    .then(() => gDial(number, serviceId))
    .then(call => {
      outCall = call;
      is(outCall.serviceId, serviceId);
    })
    .then(() => gCheckAll(outCall, [outCall], '', [], [outInfo.ringing]))
    .then(() => gRemoteAnswer(outCall))
    .then(() => gCheckAll(outCall, [outCall], '', [], [outInfo.active]))
    .then(() => gRemoteHangUp(outCall))
    .then(() => gCheckAll(null, [], '', [], []));
}

function testIncomingCallForServiceId(number, serviceId) {
  let inCall;
  let inInfo = gInCallStrPool(number);

  return Promise.resolve()
    .then(() => gRemoteDial(number))
    .then(call => {
      inCall = call;
      is(inCall.serviceId, serviceId);
    })
    .then(() => gCheckAll(null, [inCall], '', [], [inInfo.incoming]))
    .then(() => gAnswer(inCall))
    .then(() => gCheckAll(inCall, [inCall], '', [], [inInfo.active]))
    .then(() => gRemoteHangUp(inCall))
    .then(() => gCheckAll(null, [], '', [], []));
}

function testOutgoingCall() {
  log("= testOutgoingCall =");

  return Promise.resolve()
    .then(() => muxModem(0))
    .then(() => testOutgoingCallForServiceId("0912345000", 0))
    .then(() => muxModem(1))
    .then(() => testOutgoingCallForServiceId("0912345001", 1))
    .then(() => muxModem(0));
}

function testIncomingCall() {
  log("= testIncomingCall =");

  return Promise.resolve()
    .then(() => muxModem(0))
    .then(() => testIncomingCallForServiceId("0912345000", 0))
    .then(() => muxModem(1))
    .then(() => testIncomingCallForServiceId("0912345001", 1))
    .then(() => muxModem(0));
}

startDSDSTest(function() {
  testOutgoingCall()
    .then(testIncomingCall)
    .catch(error => ok(false, "Promise reject: " + error))
    .then(finish);
});
