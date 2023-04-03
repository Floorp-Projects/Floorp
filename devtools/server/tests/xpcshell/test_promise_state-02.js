/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable max-nested-callbacks */

"use strict";

/**
 * Test that the preview in a Promise's grip is correct when the Promise is
 * fulfilled.
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
    equal(grip.preview.ownProperties["<state>"].value, "fulfilled");
    equal(
      grip.preview.ownProperties["<value>"].value.actorID,
      packet.frame.arguments[0].actorID,
      "The promise's fulfilled state value in the preview should be the same " +
        "value passed to the then function"
    );

    const objClient = threadFront.pauseGrip(grip);
    const { promiseState } = await objClient.getPromiseState();
    equal(promiseState.state, "fulfilled");
    equal(
      promiseState.value.getGrip().actorID,
      packet.frame.arguments[0].actorID,
      "The promise's fulfilled state value in getPromiseState() should be " +
        "the same value passed to the then function"
    );
  })
);

function evalCode(debuggee) {
  /* eslint-disable mozilla/var-only-at-top-level, no-unused-vars */
  // prettier-ignore
  Cu.evalInSandbox(
    "doTest();\n" +
    function doTest() {
      var resolved = Promise.resolve({});
      resolved.then(() => {
        var p = resolved;
        debugger;
      });
    },
    debuggee
  );
  /* eslint-enable mozilla/var-only-at-top-level, no-unused-vars */
}
