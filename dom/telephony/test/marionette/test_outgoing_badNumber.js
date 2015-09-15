/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const number = "****5555552368****";
var outCall;

function testDialOutInvalidNumber() {
  log("Make an outCall call to an invalid number.");

  // Note: The number is valid from the view of phone and the call could be
  // dialed out successfully. However, it will later receive the BadNumberError
  // from network side.
  return telephony.dial(number).then(call => {
    outCall = call;
    ok(outCall);
    is(outCall.id.number, "");  // Emulator returns empty number for this call.
    is(outCall.state, "dialing");

    is(outCall, telephony.active);
    is(telephony.calls.length, 1);
    is(telephony.calls[0], outCall);

    return gWaitForEvent(outCall, "error").then(event => {
      is(event.call, outCall);
      ok(event.call.error);
      is(event.call.error.name, "BadNumberError");
      is(event.call.disconnectedReason, "BadNumber");
    })
    .then(() => gCheckAll(null, [], "", [], []));
  });
}

startTest(function() {
  testDialOutInvalidNumber()
    .catch(error => ok(false, "Promise reject: " + error))
    .then(finish);
});
