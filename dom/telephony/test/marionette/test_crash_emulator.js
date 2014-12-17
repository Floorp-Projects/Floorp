/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

startTest(function() {
  let outCall;

  gDial("5555551111")
    .then(call => outCall = call)
    .then(() => gRemoteAnswer(outCall))
    .then(() => {
      return new Promise(function(resolve, reject) {
        callStartTime = Date.now();
        waitFor(resolve,function() {
          callDuration = Date.now() - callStartTime;
          log("Waiting while call is active, call duration (ms): " + callDuration);
          return(callDuration >= 2000);
        });
      });
    })
    .then(() => gHangUp(outCall))
    .catch(error => ok(false, "Promise reject: " + error))
    .then(finish);
});
