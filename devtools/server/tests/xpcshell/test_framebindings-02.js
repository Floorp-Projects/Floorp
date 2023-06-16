/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check a frame actor's parent bindings.
 */

add_task(
  threadFrontTest(async ({ threadFront, debuggee }) => {
    const packet = await executeOnNextTickAndWaitForPause(
      () => evalCode(debuggee),
      threadFront
    );

    const environment = await packet.frame.getEnvironment();
    let parentEnv = environment.parent;
    const bindings = parentEnv.bindings;
    const args = bindings.arguments;
    const vars = bindings.variables;
    Assert.notEqual(parentEnv, undefined);
    Assert.equal(args.length, 0);
    Assert.equal(vars.stopMe.value.type, "object");
    Assert.equal(vars.stopMe.value.class, "Function");
    Assert.ok(!!vars.stopMe.value.actor);

    // Skip the global lexical scope.
    parentEnv = parentEnv.parent.parent;
    Assert.notEqual(parentEnv, undefined);
    const objClient = threadFront.pauseGrip(parentEnv.object);
    const response = await objClient.getPrototypeAndProperties();
    Assert.equal(response.ownProperties.Object.value.getGrip().type, "object");
    Assert.equal(
      response.ownProperties.Object.value.getGrip().class,
      "Function"
    );
    Assert.ok(!!response.ownProperties.Object.value.actorID);

    await threadFront.resume();
  })
);

function evalCode(debuggee) {
  /* eslint-disable */
  debuggee.eval(
    "(" +
      function () {
        function stopMe(number, bool, string, null_, undef, object) {
          var a = 1;
          var b = true;
          var c = { a: "a" };
          eval("");
          debugger;
        }
        stopMe(42, true, "nasu", null, undefined, { foo: "bar" });
      } +
      ")()"
  );
}
