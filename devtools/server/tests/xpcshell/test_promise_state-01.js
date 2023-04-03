/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable max-nested-callbacks */

"use strict";

/**
 * Test that the preview in a Promise's grip is correct when the Promise is
 * pending.
 */

add_task(
  threadFrontTest(async ({ threadFront, debuggee }) => {
    const packet = await executeOnNextTickAndWaitForPause(
      () => evalCode(debuggee),
      threadFront
    );

    const environment = await packet.frame.getEnvironment();
    const grip = environment.bindings.variables.p.value;

    ok(grip.preview);
    equal(grip.class, "Promise");
    equal(grip.preview.ownProperties["<state>"].value, "pending");

    const objClient = threadFront.pauseGrip(grip);
    const { promiseState } = await objClient.getPromiseState();
    equal(promiseState.state, "pending");
  })
);

function evalCode(debuggee) {
  /* eslint-disable mozilla/var-only-at-top-level, no-unused-vars */
  // prettier-ignore
  Cu.evalInSandbox(
    "doTest();\n" +
    function doTest() {
      var p = new Promise(function () {});
      debugger;
    },
    debuggee
  );
  /* eslint-enable mozilla/var-only-at-top-level, no-unused-vars */
}
