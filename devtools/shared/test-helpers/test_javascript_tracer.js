/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test for the thread helpers utility module.
 *
 * This uses a xpcshell test in order to avoid recording the noise
 * of all Firefox components when using a mochitest.
 */

const { traceAllJSCalls } = ChromeUtils.importESModule(
  "resource://devtools/shared/test-helpers/thread-helpers.sys.mjs"
);
// ESLint thinks this is a browser test, but it's actually an xpcshell
// test and so `setTimeout` isn't available out of the box.
// eslint-disable-next-line mozilla/no-redeclare-with-import-autofix
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs"
);

add_task(async function sanityCheck() {
  let ranTheOtherEventLoop = false;
  setTimeout(function otherEventLoop() {
    ranTheOtherEventLoop = true;
  }, 0);
  const jsTracer = traceAllJSCalls();
  function foo() {}
  for (let i = 0; i < 10; i++) {
    foo();
  }
  jsTracer.stop();
  ok(
    !ranTheOtherEventLoop,
    "When we don't pause frame execution, the other event do not execute"
  );
});

add_task(async function withPrefix() {
  const jsTracer = traceAllJSCalls({ prefix: "my-prefix" });
  function foo() {}
  for (let i = 0; i < 10; i++) {
    foo();
  }
  jsTracer.stop();
  ok(true, "Were able to run with a prefix argument");
});

add_task(async function pause() {
  const start = Cu.now();
  let ranTheOtherEventLoop = false;
  setTimeout(function otherEventLoop() {
    ranTheOtherEventLoop = true;
  }, 0);
  const jsTracer = traceAllJSCalls({ pause: 100 });
  function foo() {}
  for (let i = 0; i < 10; i++) {
    foo();
  }
  jsTracer.stop();
  const duration = Cu.now() - start;
  ok(
    duration > 10 * 100,
    "The execution of the for loop was slow down by at least the pause duration in each loop"
  );
  ok(
    ranTheOtherEventLoop,
    "When we pause frame execution, the other event can execute"
  );
});
