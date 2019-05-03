/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
});

/**
 * Make sure that stepping in the last statement of the last frame doesn't
 * cause an unexpected pause, when another JS frame is pushed on the stack
 * (bug 785689).
 */

add_task(threadClientTest(async ({ threadClient, debuggee, client }) => {
  dumpn("Evaluating test code and waiting for first debugger statement");
  await executeOnNextTickAndWaitForPause(() => evaluateTestCode(debuggee), client);

  const step1 = await stepIn(client, threadClient);
  equal(step1.type, "paused");
  equal(step1.frame.where.line, 3);
  equal(step1.why.type, "resumeLimit");
  equal(debuggee.a, undefined);
  equal(debuggee.b, undefined);

  const step2 = await stepIn(client, threadClient);
  equal(step2.type, "paused");
  equal(step2.frame.where.line, 4);
  equal(step2.why.type, "resumeLimit");
  equal(debuggee.a, 1);
  equal(debuggee.b, undefined);

  const step3 = await stepIn(client, threadClient);
  equal(step3.type, "paused");
  equal(step3.frame.where.line, 4);
  equal(step3.why.type, "resumeLimit");
  equal(debuggee.a, 1);
  equal(debuggee.b, 2);

  await new Promise(async resolve => {
    await threadClient.stepIn();
    threadClient.addOneTimeListener("paused", (event, packet) => {
      equal(packet.type, "paused");
      // Before fixing bug 785689, the type was resumeLimit.
      equal(packet.why.type, "debuggerStatement");
      resolve();
    });
    debuggee.eval("debugger;");
  });
}));

function evaluateTestCode(debuggee) {
  /* eslint-disable */
  Cu.evalInSandbox(
    `                                   // 1
    debugger;                           // 2
    var a = 1;                          // 3
    var b = 2;`,                        // 4
    debuggee,
    "1.8",
    "test_stepping-05-test-code.js",
    1
  );
  /* eslint-disable */
}
