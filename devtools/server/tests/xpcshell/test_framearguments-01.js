/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check a frame actor's arguments property.
 */

add_task(
  threadFrontTest(async ({ threadFront, debuggee }) => {
    const packet = await executeOnNextTickAndWaitForPause(
      () => evalCode(debuggee),
      threadFront
    );

    const args = packet.frame.arguments;
    Assert.equal(args.length, 6);
    Assert.equal(args[0], 42);
    Assert.equal(args[1], true);
    Assert.equal(args[2], "nasu");
    Assert.equal(args[3].type, "null");
    Assert.equal(args[4].type, "undefined");
    Assert.equal(args[5].type, "object");
    Assert.equal(args[5].class, "Object");
    Assert.ok(!!args[5].actor);

    await threadFront.resume();
  })
);

function evalCode(debuggee) {
  debuggee.eval(
    "(" +
      function () {
        // These arguments are tested.
        // eslint-disable-next-line no-unused-vars
        function stopMe(number, bool, string, null_, undef, object) {
          debugger;
        }
        stopMe(42, true, "nasu", null, undefined, { foo: "bar" });
      } +
      ")()"
  );
}
