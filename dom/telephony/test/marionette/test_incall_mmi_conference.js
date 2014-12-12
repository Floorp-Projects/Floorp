/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const call1Number = "0900000001";
const call2Number = "0900000002";
const call3Number = "0900000003";
const call1Info = gOutCallStrPool(call1Number);
const call2Info = gOutCallStrPool(call2Number);
const call3Info = gOutCallStrPool(call3Number);

function setupTwoCalls() {
  let call1;
  let call2;

  return gDial(call1Number)
    .then(call => call1 = call)
    .then(() => gRemoteAnswer(call1))
    .then(() => gDial(call2Number))
    .then(call => call2 = call)
    .then(() => gRemoteAnswer(call2))
    .then(() => gCheckAll(call2, [call1, call2], "", [],
                          [call1Info.held, call2Info.active]))
    .then(() => [call1, call2]);
}

function testInCallMMI() {
  log('= testInCallMMI =');

  return setupTwoCalls()
    .then(calls => [call1, call2] = calls)
    // Conference two calls.
    .then(() => log("- Conference two calls."))
    .then(() => gSendMMI("3"))
    .then(() => gWaitForNamedStateEvent(conference, "connected"))
    .then(() => gCheckAll(conference, [], "connected", [call1, call2],
                          [call1Info.active, call2Info.active]))

    // Make third call.
    .then(() => log("- Make third call."))
    .then(() => gDial(call3Number))
    .then(call => call3 = call)
    .then(() => gRemoteAnswer(call3))
    .then(() => gCheckAll(call3, [call3], "held", [call1, call2],
                          [call1Info.held, call2Info.held, call3Info.active]))

    // Enlarge the conference.
    .then(() => log("- Enlarge the conference."))
    .then(() => gSendMMI("3"))
    .then(() => gWaitForNamedStateEvent(conference, "connected"))
    .then(() => gCheckAll(conference, [], "connected", [call1, call2, call3],
                          [call1Info.active, call2Info.active, call3Info.active]))

    // Separate call 2.
    .then(() => log("- Separate call 2."))
    .then(() => gSendMMI("22"))
    .then(() => gWaitForNamedStateEvent(conference, "held"))
    .then(() => gCheckAll(call2, [call2], "held", [call1, call3],
                          [call1Info.held, call2Info.active, call3Info.held]))

    // Switch active.
    .then(() => log("- Switch active."))
    .then(() => gSendMMI("2"))
    .then(() => gWaitForNamedStateEvent(conference, "connected"))
    .then(() => gCheckAll(conference, [call2], "connected", [call1, call3],
                          [call1Info.active, call2Info.held, call3Info.active]))

    // Release call 2.
    .then(() => log("- Release call 2."))
    .then(() => gSendMMI("12"))
    .then(() => gWaitForNamedStateEvent(call2, "disconnected"))
    .then(() => gCheckAll(conference, [], "connected", [call1, call3],
                          [call1Info.active, call3Info.active]))

    // Release call 1.
    .then(() => log("- Release call 1."))
    .then(() => gSendMMI("11"))
    .then(() => gWaitForStateChangeEvent(conference, ""))
    .then(() => gCheckAll(call3, [call3], "", [], [call3Info.active]))

    .then(() => gRemoteHangUpCalls([call3]));
}

startTest(function() {
  testInCallMMI()
    .catch(error => ok(false, "Promise reject: " + error))
    .then(finish);
});
