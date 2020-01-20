/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable max-nested-callbacks */

"use strict";

/**
 * Test that the preview in a Promise's grip is correct when the Promise is
 * rejected.
 */

add_task(
  threadFrontTest(async ({ threadFront, debuggee }) => {
    const packet = await executeOnNextTickAndWaitForPause(
      () => evalCode(debuggee),
      threadFront
    );

    const environment = await packet.frame.getEnvironment();
    const grip = environment.bindings.variables.p;
    ok(grip.value.preview);
    equal(grip.value.class, "Promise");
    equal(grip.value.promiseState.state, "rejected");
    equal(
      grip.value.promiseState.reason.actorID,
      packet.frame.arguments[0].actorID,
      "The promise's rejected state reason should be the same value passed " +
        "to the then function"
    );
  })
);

function evalCode(debuggee) {
  /* eslint-disable */
  Cu.evalInSandbox(
    "doTest();\n" +
    function doTest() {
      var resolved = Promise.reject(new Error("uh oh"));
      resolved.catch(() => {
        var p = resolved;
        debugger;
      });
    },
    debuggee
  );
  /* eslint-enable */
}
