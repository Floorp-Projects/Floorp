/* eslint-disable max-nested-callbacks */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Bug 1552453 - Verify that breakpoints are hit for evaluated
 * scripts that contain a source url pragma.
 */
add_task(
  threadFrontTest(async ({ threadFront, targetFront }) => {
    await threadFront.setBreakpoint(
      { sourceUrl: "http://example.com/code.js", line: 2, column: 1 },
      {}
    );

    info("Create a new script with the displayUrl code.js");
    const consoleFront = await targetFront.getFront("console");
    let onEvaluationResult = consoleFront.once("evaluationResult");
    consoleFront.evaluateJSAsync(
      "function f() {\n return 5; \n}\n//# sourceURL=http://example.com/code.js"
    );
    await onEvaluationResult;

    const sourcePacket = await waitForEvent(threadFront, "newSource");
    equal(sourcePacket.source.url, "http://example.com/code.js");

    info("Evaluate f() and pause at line 2");
    onEvaluationResult = consoleFront.once("evaluationResult");
    consoleFront.evaluateJSAsync("f()");
    const pausedPacket = await waitForPause(threadFront);
    equal(pausedPacket.why.type, "breakpoint");
    equal(pausedPacket.frame.where.line, 2);
    resume(threadFront);
    await onEvaluationResult;
  })
);
