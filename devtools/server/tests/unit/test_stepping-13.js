/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/*
 * Check that is possible to step into both the inner and outer function
 * calls.
 */

add_task(
  threadFrontTest(async ({ threadFront, targetFront, debuggee }) => {
    dumpn("Evaluating test code and waiting for first debugger statement");

    const consoleFront = await targetFront.getFront("console");
    consoleFront.evaluateJSAsync(
      `(function () {
        const a = () => { return 2 };
        debugger;
        a(a())
      })()`
    );

    await waitForEvent(threadFront, "paused");
    const step1 = await stepOver(threadFront);
    Assert.equal(step1.frame.where.line, 4, "step to line 4");

    const step2 = await stepIn(threadFront);
    Assert.equal(step2.frame.where.line, 2, "step in to line 2");

    const step3 = await stepOut(threadFront);
    Assert.equal(step3.frame.where.line, 4, "step back to line 4");
    Assert.equal(step3.frame.where.column, 9, "step out to column 9");

    const step4 = await stepIn(threadFront);
    Assert.equal(step4.frame.where.line, 2, "step in to line 2");

    await threadFront.resume();
  })
);
