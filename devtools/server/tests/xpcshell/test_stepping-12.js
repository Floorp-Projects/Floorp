/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that step out stops at the parent and the parent's parent.
 * This checks for the failure found in bug 1530549.
 */

const sourceUrl = "test_stepping-10-test-code.js";

add_task(
  threadFrontTest(async args => {
    dumpn("Evaluating test code and waiting for first debugger statement");

    await testGenerator(args);
    await testAwait(args);
    await testInterleaving(args);
    await testMultipleSteps(args);
  })
);

async function testAwait({ threadFront, debuggee }) {
  function evaluateTestCode() {
    Cu.evalInSandbox(
      `
        (async function() {
          debugger;
          r = await Promise.resolve('yay');
          a = 4;
        })();
      `,
      debuggee,
      "1.8",
      sourceUrl,
      1
    );
  }

  await executeOnNextTickAndWaitForPause(evaluateTestCode, threadFront);

  dumpn("Step Over and land on line 5");
  const step1 = await stepOver(threadFront);
  equal(step1.frame.where.line, 4);
  equal(step1.frame.where.column, 10);

  const step2 = await stepOver(threadFront);
  equal(step2.frame.where.line, 5);
  equal(step2.frame.where.column, 10);
  equal(debuggee.r, "yay");
  await resume(threadFront);
}

async function testInterleaving({ threadFront, debuggee }) {
  function evaluateTestCode() {
    Cu.evalInSandbox(
      `
        (async function simpleRace() {
          debugger;
          this.result = await new Promise((r) => {
            Promise.resolve().then(() => { debugger });
            Promise.resolve().then(r('yay'))
          })
          var a = 2;
          debugger;
        })()
      `,
      debuggee,
      "1.8",
      sourceUrl,
      1
    );
  }

  await executeOnNextTickAndWaitForPause(evaluateTestCode, threadFront);

  dumpn("Step Over and land on line 5");
  const step1 = await stepOver(threadFront);
  equal(step1.frame.where.line, 4);

  const step2 = await stepOver(threadFront);
  equal(step2.frame.where.line, 5);
  equal(step2.frame.where.column, 43);

  const step3 = await resumeAndWaitForPause(threadFront);
  equal(step3.frame.where.line, 9);
  equal(debuggee.result, "yay");

  await resume(threadFront);
}

async function testMultipleSteps({ threadFront, debuggee }) {
  function evaluateTestCode() {
    Cu.evalInSandbox(
      `
        (async function simpleRace() {
          debugger;
          await Promise.resolve();
          var a = 2;
          await Promise.resolve();
          var b = 2;
          await Promise.resolve();
          debugger;
        })()
      `,
      debuggee,
      "1.8",
      sourceUrl,
      1
    );
  }

  await executeOnNextTickAndWaitForPause(evaluateTestCode, threadFront);

  const step1 = await stepOver(threadFront);
  equal(step1.frame.where.line, 4);

  const step2 = await stepOver(threadFront);
  equal(step2.frame.where.line, 5);

  const step3 = await stepOver(threadFront);
  equal(step3.frame.where.line, 6);
  resume(threadFront);
}

async function testGenerator({ threadFront, debuggee }) {
  function evaluateTestCode() {
    Cu.evalInSandbox(
      `
        (async function() {
          function* makeSteps() {
            debugger;
            yield 1;
            yield 2;
            return 3;
          }
          const s = makeSteps();
          s.next();
          s.next();
          s.next();
        })()
      `,
      debuggee,
      "1.8",
      sourceUrl,
      1
    );
  }

  await executeOnNextTickAndWaitForPause(evaluateTestCode, threadFront);

  const step1 = await stepOver(threadFront);
  equal(step1.frame.where.line, 5);

  const step2 = await stepOver(threadFront);
  equal(step2.frame.where.line, 6);

  const step3 = await stepOver(threadFront);
  equal(step3.frame.where.line, 7);
  await resume(threadFront);
}
