/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

function muxModem(id) {
  return new Promise((resolve, reject) => {
    emulator.runCmdWithCallback("mux modem " + id, resolve);
  });
}

function testDtmfNoActiveCall() {
  log("= testDtmfNoActiveCall =");
  return new Promise((resolve, reject) => {
    gSendTone('1', 5, 0).then(() => {
      log("Unexpected success. We cannot send a DTMF without an active call");
      reject();
    }, resolve);
  });
}

function testDtmfDsds() {
  log("= testDtmfDsds =");

  let outCall;
  let number = "0912345000";
  let serviceId = 0;
  let otherServiceId = 1;

  return Promise.resolve()
    .then(() => muxModem(serviceId))
    .then(() => gDial(number, serviceId))
    .then(call => {
      outCall = call;
      is(outCall.serviceId, serviceId);
    })
    .then(() => gRemoteAnswer(outCall))
    // Send tone with correct serviceId.
    .then(() => gSendTone('1', 5, serviceId))
    .then(() => emulator.runCmd("modem dtmf"))
    .then(tone => {
      is(tone, '1,OK', 'Sent tone is 1');
    })
    // Send tone without serviceId.
    .then(() => gSendTone('2', 5))
    .then(() => emulator.runCmd("modem dtmf"))
    .then(tone => {
      is(tone, '2,OK', 'Sent tone is 2');
    })
    // Send tone with incorrect serviceId.
    .then(gSendTone('1', 5, otherServiceId).catch((e) => {
        log('Expected Error ' + e);
        gRemoteHangUp(outCall);
      })
    )
    .catch((e) => {
      log('Unexpected Error ' + e);
      ok(false);
    });
}

startDSDSTest(function() {
  testDtmfNoActiveCall()
    .then(testDtmfDsds)
    .then(emulator.runCmd("modem dtmf reset"))
    .catch(error => ok(false, "Promise reject: " + error))
    .then(finish);
});
