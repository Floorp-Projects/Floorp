/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const outNumber = "5555551111";
const outInfo = gOutCallStrPool(outNumber);
var outCall;

startTest(function() {
  gDial(outNumber)
    .then(call => outCall = call)
    .then(() => gCheckAll(outCall, [outCall], "", [], [outInfo.ringing]))
    .then(() => {
      let p1 = gWaitForEvent(outCall, "error")
        .then(event => {
          is(event.call, outCall);
          is(event.call.error.name, "BusyError");
          is(event.call.disconnectedReason, "Busy");
        });
      let p2 = emulator.runCmd("telephony busy " + outNumber);
      return Promise.all([p1, p2]);
    })
    .then(() => gCheckAll(null, [], "", [], []))
    .catch(error => ok(false, "Promise reject: " + error))
    .then(finish);
});
