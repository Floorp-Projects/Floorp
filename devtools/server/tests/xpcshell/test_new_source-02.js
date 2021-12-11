/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that sourceURL has the correct effect when using threadFront.eval.
 */

add_task(
  threadFrontTest(async ({ commands, threadFront, debuggee }) => {
    await executeOnNextTickAndWaitForPause(
      () => evalCode(debuggee),
      threadFront
    );

    const packet1 = await waitForEvent(threadFront, "newSource");

    Assert.ok(!!packet1.source);
    Assert.ok(packet1.source.introductionType, "eval");

    commands.scriptCommand.execute(
      "function f() { }\n//# sourceURL=http://example.com/code.js"
    );

    const packet2 = await waitForEvent(threadFront, "newSource");
    dump(JSON.stringify(packet2, null, 2));
    Assert.ok(!!packet2.source);
    Assert.ok(!!packet2.source.url.match(/example\.com/));
  })
);

function evalCode(debuggee) {
  /* eslint-disable */
  debuggee.eval(
    "(" +
      function() {
        function stopMe(arg1) {
          debugger;
        }
        stopMe({ obj: true });
      } +
      ")()"
  );
  /* eslint-enable */
}
