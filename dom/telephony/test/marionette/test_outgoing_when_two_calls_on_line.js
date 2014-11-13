/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

function testReject3rdCall() {
  let outCall1;
  let outCall2;

  return gDial("0912345001")
    .then(call => { outCall1 = call; })
    .then(() => gRemoteAnswer(outCall1))
    .then(() => gHold(outCall1))
    .then(() => gDial("0912345002"))
    .then(call => { outCall2 = call; })
    .then(() => gRemoteAnswer(outCall2))
    .then(() => gDial("0912345003"))
    .then(call => {
      ok(false, "The dial request should be rejected");
    }, cause => {
      log("Reject 3rd call, cuase: " + cause);
      is(cause, "InvalidStateError");
    })
    .then(() => gRemoteHangUpCalls([outCall1, outCall2]));
}

startTest(function() {
  testReject3rdCall()
    .catch(error => ok(false, "Promise reject: " + error))
    .then(finish);
});
