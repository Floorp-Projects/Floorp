/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

/**
 * Check basic stepping for console evaluations.
 */

add_task(
  threadFrontTest(async ({ threadFront, targetFront, debuggee }) => {
    dumpn("Evaluating test code and waiting for first debugger statement");

    const consoleFront = await targetFront.getFront("console");
    consoleFront.evaluateJSAsync(
      `(function(){
        debugger;
        var a = 1;
        var b = 2;
      })();`
    );

    await waitForEvent(threadFront, "paused");
    const packet = await stepOver(threadFront);
    Assert.equal(packet.frame.where.line, 3, "step to line 3");
    await threadFront.resume();
  })
);
