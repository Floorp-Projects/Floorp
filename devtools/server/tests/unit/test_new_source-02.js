/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that sourceURL has the correct effect when using gThreadFront.eval.
 */

var gDebuggee;
var gTargetFront;
var gThreadFront;

add_task(
  threadFrontTest(
    async ({ threadFront, debuggee, targetFront }) => {
      gThreadFront = threadFront;
      gTargetFront = targetFront;
      gDebuggee = debuggee;
      test_simple_new_source();
    },
    { waitForFinish: true }
  )
);

function test_simple_new_source() {
  gThreadFront.once("paused", function() {
    gThreadFront.once("newSource", async function(packet2) {
      // The "stopMe" eval source is emitted first.
      Assert.ok(!!packet2.source);
      Assert.ok(packet2.source.introductionType, "eval");

      gThreadFront.once("newSource", function(packet) {
        dump(JSON.stringify(packet, null, 2));
        Assert.ok(!!packet.source);
        Assert.ok(!!packet.source.url.match(/example\.com/));

        threadFrontTestFinished();
      });

      const consoleFront = await gTargetFront.getFront("console");
      consoleFront.evaluateJSAsync(
        "function f() { }\n//# sourceURL=http://example.com/code.js"
      );
    });
  });

  /* eslint-disable */
  gDebuggee.eval("(" + function () {
    function stopMe(arg1) { debugger; }
    stopMe({obj: true});
  } + ")()");
  /* eslint-enable */
}
