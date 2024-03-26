/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { JSTracer } = ChromeUtils.importESModule(
  "resource://devtools/server/tracer/tracer.sys.mjs",
  { global: "shared" }
);

add_task(async function () {
  // Because this test uses evalInSandbox, we need to tweak the following prefs
  Services.prefs.setBoolPref(
    "security.allow_parent_unrestricted_js_loads",
    true
  );
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("security.allow_parent_unrestricted_js_loads");
  });
});

add_task(async function testTracingContentGlobal() {
  const toggles = [];
  const frames = [];
  const listener = {
    onTracingToggled(state) {
      toggles.push(state);
    },
    onTracingFrame(frameInfo) {
      frames.push(frameInfo);
    },
  };

  info("Register a tracing listener");
  JSTracer.addTracingListener(listener);

  const sandbox = Cu.Sandbox("https://example.com");
  Cu.evalInSandbox("function bar() {}; function foo() {bar()};", sandbox);

  info("Start tracing");
  JSTracer.startTracing({ global: sandbox, prefix: "testContentPrefix" });
  Assert.equal(toggles.length, 1);
  Assert.equal(toggles[0], true);

  info("Call some code");
  sandbox.foo();

  Assert.equal(frames.length, 2);
  const lastFrame = frames.pop();
  const beforeLastFrame = frames.pop();
  Assert.equal(beforeLastFrame.depth, 0);
  Assert.equal(beforeLastFrame.formatedDisplayName, "λ foo");
  Assert.equal(beforeLastFrame.prefix, "testContentPrefix: ");
  Assert.ok(beforeLastFrame.frame);
  Assert.equal(lastFrame.depth, 1);
  Assert.equal(lastFrame.formatedDisplayName, "λ bar");
  Assert.equal(lastFrame.prefix, "testContentPrefix: ");
  Assert.ok(lastFrame.frame);

  info("Stop tracing");
  JSTracer.stopTracing();
  Assert.equal(toggles.length, 2);
  Assert.equal(toggles[1], false);

  info("Recall code after stop, no more traces are logged");
  sandbox.foo();
  Assert.equal(frames.length, 0);

  info("Start tracing again, and recall code");
  JSTracer.startTracing({ global: sandbox, prefix: "testContentPrefix" });
  sandbox.foo();
  info("New traces are logged");
  Assert.equal(frames.length, 2);

  info("Unregister the listener and recall code");
  JSTracer.removeTracingListener(listener);
  sandbox.foo();
  info("No more traces are logged");
  Assert.equal(frames.length, 2);

  info("Stop tracing");
  JSTracer.stopTracing();
});

add_task(async function testTracingJSMGlobal() {
  // We have to register the listener code in a sandbox, i.e. in a distinct global
  // so that we aren't creating traces when tracer calls it. (and cause infinite loops)
  const systemPrincipal = Services.scriptSecurityManager.getSystemPrincipal();
  const listenerSandbox = Cu.Sandbox(systemPrincipal);
  Cu.evalInSandbox(
    "new " +
      function () {
        globalThis.toggles = [];
        globalThis.frames = [];
        globalThis.listener = {
          onTracingToggled(state) {
            globalThis.toggles.push(state);
          },
          onTracingFrame(frameInfo) {
            globalThis.frames.push(frameInfo);
          },
        };
      },
    listenerSandbox
  );

  info("Register a tracing listener");
  JSTracer.addTracingListener(listenerSandbox.listener);

  info("Start tracing");
  JSTracer.startTracing({ global: null, prefix: "testPrefix" });
  Assert.equal(listenerSandbox.toggles.length, 1);
  Assert.equal(listenerSandbox.toggles[0], true);

  info("Call some code");
  function bar() {}
  function foo() {
    bar();
  }
  foo();

  // Note that the tracer will record the two Assert.equal and the info calls.
  // So only assert the last two frames.
  const lastFrame = listenerSandbox.frames.at(-1);
  const beforeLastFrame = listenerSandbox.frames.at(-2);
  Assert.equal(beforeLastFrame.depth, 7);
  Assert.equal(beforeLastFrame.formatedDisplayName, "λ foo");
  Assert.equal(beforeLastFrame.prefix, "testPrefix: ");
  Assert.ok(beforeLastFrame.frame);
  Assert.equal(lastFrame.depth, 8);
  Assert.equal(lastFrame.formatedDisplayName, "λ bar");
  Assert.equal(lastFrame.prefix, "testPrefix: ");
  Assert.ok(lastFrame.frame);

  info("Stop tracing");
  JSTracer.stopTracing();
  Assert.equal(listenerSandbox.toggles.length, 2);
  Assert.equal(listenerSandbox.toggles[1], false);

  JSTracer.removeTracingListener(listenerSandbox.listener);
});

add_task(async function testTracingValues() {
  // Test the `traceValues` flag
  const sandbox = Cu.Sandbox("https://example.com");
  Cu.evalInSandbox(
    `function foo() { bar(-0, 1, ["array"], { attribute: 3 }, "4", BigInt(5), Symbol("6"), Infinity, undefined, null, false, NaN, function foo() {}, function () {}, class MyClass {}); }; function bar(a, b, c) {}`,
    sandbox
  );

  // Pass an override method to catch all strings tentatively logged to stdout
  const logs = [];
  function loggingMethod(str) {
    logs.push(str);
  }

  info("Start tracing");
  JSTracer.startTracing({ global: sandbox, traceValues: true, loggingMethod });

  info("Call some code");
  sandbox.foo();

  Assert.equal(logs.length, 3);
  Assert.equal(logs[0], "Start tracing JavaScript\n");
  Assert.stringContains(logs[1], "λ foo()");
  Assert.stringContains(
    logs[2],
    `λ bar(-0, 1, Array(1), [object Object], "4", BigInt(5), Symbol(6), Infinity, undefined, null, false, NaN, function foo(), function anonymous(), class MyClass)`
  );

  info("Stop tracing");
  JSTracer.stopTracing();
});

add_task(async function testTracingFunctionReturn() {
  // Test the `traceFunctionReturn` flag
  const sandbox = Cu.Sandbox("https://example.com");
  Cu.evalInSandbox(
    `function foo() { bar(); return 0 } function bar() { return "string" }; foo();`,
    sandbox
  );

  // Pass an override method to catch all strings tentatively logged to stdout
  const logs = [];
  function loggingMethod(str) {
    logs.push(str);
  }

  info("Start tracing");
  JSTracer.startTracing({
    global: sandbox,
    traceFunctionReturn: true,
    loggingMethod,
  });

  info("Call some code");
  sandbox.foo();

  Assert.equal(logs.length, 5);
  Assert.equal(logs[0], "Start tracing JavaScript\n");
  Assert.stringContains(logs[1], "λ foo");
  Assert.stringContains(logs[2], "λ bar");
  Assert.stringContains(logs[3], `λ bar return`);
  Assert.stringContains(logs[4], "λ foo return");

  info("Stop tracing");
  JSTracer.stopTracing();
});

add_task(async function testTracingFunctionReturnAndValues() {
  // Test the `traceFunctionReturn` and `traceValues` flag
  const sandbox = Cu.Sandbox("https://example.com");
  Cu.evalInSandbox(
    `function foo() { bar(); second(); } function bar() { return "string" }; function second() { return null; }; foo();`,
    sandbox
  );

  // Pass an override method to catch all strings tentatively logged to stdout
  const logs = [];
  function loggingMethod(str) {
    logs.push(str);
  }

  info("Start tracing");
  JSTracer.startTracing({
    global: sandbox,
    traceFunctionReturn: true,
    traceValues: true,
    loggingMethod,
  });

  info("Call some code");
  sandbox.foo();

  Assert.equal(logs.length, 7);
  Assert.equal(logs[0], "Start tracing JavaScript\n");
  Assert.stringContains(logs[1], "λ foo()");
  Assert.stringContains(logs[2], "λ bar()");
  Assert.stringContains(logs[3], `λ bar return "string"`);
  Assert.stringContains(logs[4], "λ second()");
  Assert.stringContains(logs[5], `λ second return null`);
  Assert.stringContains(logs[6], "λ foo return undefined");

  info("Stop tracing");
  JSTracer.stopTracing();
});

add_task(async function testTracingStep() {
  // Test the `traceStep` flag
  const sandbox = Cu.Sandbox("https://example.com");
  const source = `
function foo() {
  bar();            /* line 3 */
  second();         /* line 4 */
  dump("foo\\n");
}
function bar() {
  let res;          /* line 8 */
  if (1 === 1) {    /* line 9 */
    res = "string"; /* line 10 */
  } else {
    res = "nope"
  }
  return res;       /* line 14 */
};
function second() {
  let x = 0;        /* line 17 */
  for (let i = 0; i < 2; i++) { /* line 18 */
    x++;            /* line 19 */
  }
  return null;      /* line 21 */
};
foo();`;
  Cu.evalInSandbox(source, sandbox, null, "file.js", 1);

  // Pass an override method to catch all strings tentatively logged to stdout
  const logs = [];
  function loggingMethod(str) {
    logs.push(str);
  }

  info("Start tracing");
  JSTracer.startTracing({
    global: sandbox,
    traceSteps: true,
    loggingMethod,
  });

  info("Call some code");
  sandbox.foo();

  Assert.equal(logs.length, 19);
  Assert.equal(logs[0], "Start tracing JavaScript\n");
  Assert.stringContains(logs[1], "λ foo");
  Assert.stringContains(logs[1], "file.js:3:3");

  Assert.stringContains(logs[2], "λ bar");
  Assert.stringContains(logs[2], "file.js:7:16");

  // Each "step" only prints the location and nothing more
  Assert.stringContains(logs[3], "file.js:9:7");

  Assert.stringContains(logs[4], "file.js:10:5");

  Assert.stringContains(logs[5], "file.js:14:3");

  Assert.stringContains(logs[6], "file.js:4:3");

  Assert.stringContains(logs[7], "λ second");
  Assert.stringContains(logs[7], "file.js:16:19");

  Assert.stringContains(logs[8], "file.js:17:11");

  // For loop
  Assert.stringContains(logs[9], "file.js:18:16");

  Assert.stringContains(logs[10], "file.js:18:19");

  Assert.stringContains(logs[11], "file.js:19:5");

  Assert.stringContains(logs[12], "file.js:18:26");

  Assert.stringContains(logs[13], "file.js:18:19");

  Assert.stringContains(logs[14], "file.js:19:5");

  Assert.stringContains(logs[15], "file.js:18:26");

  Assert.stringContains(logs[16], "file.js:18:19");
  // End of for loop

  Assert.stringContains(logs[17], "file.js:21:3");

  Assert.stringContains(logs[18], "file.js:5:3");
  info("Stop tracing");
  JSTracer.stopTracing();
});

add_task(async function testTracingPauseOnStep() {
  // Test the `pauseOnStep` flag
  const sandbox = Cu.Sandbox("https://example.com");
  sandbox.dump = dump;
  const source = `var counter = 0; function incrementCounter() { let x = 0; dump("++\\n"); counter++; };`;
  Cu.evalInSandbox(source, sandbox);

  // Pass an override method to catch all strings tentatively logged to stdout
  const logs = [];
  let loggingMethodResolve;
  function loggingMethod(str) {
    logs.push(str);
    if (loggingMethodResolve) {
      loggingMethodResolve();
    }
  }

  info("Start tracing without pause");
  JSTracer.startTracing({
    global: sandbox,
    loggingMethod,
  });

  info("Call some code");
  sandbox.incrementCounter();

  Assert.equal(logs.length, 2);
  Assert.equal(logs[0], "Start tracing JavaScript\n");
  Assert.stringContains(logs[1], "λ incrementCounter");

  info(
    "When pauseOnStep isn't used, the traced code runs synchronously to completion"
  );
  Assert.equal(sandbox.counter, 1);

  info("Stop tracing");
  JSTracer.stopTracing();

  logs.length = 0;
  sandbox.counter = 0;

  info("Start tracing with 0ms pause");
  JSTracer.startTracing({
    global: sandbox,
    pauseOnStep: 0,
    loggingMethod,
  });

  let onTraces = Promise.withResolvers();
  let onResumed = Promise.withResolvers();
  // This is used when receiving new traces in `loggingMethod()`
  loggingMethodResolve = onTraces.resolve;

  info(
    "Run the to-be-traced code in a distinct event loop as it would be paused synchronously and would prevent further test script execution"
  );
  Services.tm.dispatchToMainThread(() => {
    sandbox.incrementCounter();
    onResumed.resolve();
  });

  info("Wait for tracer to call the listener");
  await onTraces.promise;

  Assert.equal(logs.length, 2);
  Assert.equal(logs[0], "Start tracing JavaScript\n");
  Assert.stringContains(logs[1], "λ incrementCounter");

  info(
    "When pauseInStep is used, the tracer listener is called, but the traced function is paused and doesn't run synchronously to completion"
  );
  Assert.equal(
    sandbox.counter,
    0,
    "The increment method was called but its execution flow was blocked and couldn't increment"
  );

  info("Wait for traced code to be resumed");
  await onResumed.promise;
  info(
    "If we release the event loop, we can see the traced function completion"
  );
  Assert.equal(sandbox.counter, 1);

  info("Stop tracing");
  JSTracer.stopTracing();

  logs.length = 0;
  sandbox.counter = 0;

  info("Start tracing with 250ms pause");
  JSTracer.startTracing({
    global: sandbox,
    pauseOnStep: 250,
    loggingMethod,
  });

  onTraces = Promise.withResolvers();
  onResumed = Promise.withResolvers();
  // This is used when receiving new traces in `loggingMethod()`
  loggingMethodResolve = onTraces.resolve;

  info(
    "Run the to-be-traced code in a distinct event loop as it would be paused synchronously and would prevent further test script execution"
  );
  const startTimestamp = Cu.now();
  Services.tm.dispatchToMainThread(() => {
    sandbox.incrementCounter();
    onResumed.resolve();
  });

  info("Wait for tracer to call the listener");
  await onTraces.promise;

  Assert.equal(logs.length, 2);
  Assert.equal(logs[0], "Start tracing JavaScript\n");
  Assert.stringContains(logs[1], "λ incrementCounter");

  info(
    "When pauseInStep is used, the tracer lsitener is called, but the traced function is paused and doesn't run synchronously to completion"
  );
  Assert.equal(sandbox.counter, 0);

  info("Wait for traced code to be resumed");
  await onResumed.promise;
  info(
    "If we release the event loop, we can see the traced function completion"
  );
  Assert.equal(sandbox.counter, 1);
  info("The thread should have paused at least the pauseOnStep's duration");
  Assert.greater(Cu.now() - startTimestamp, 250);

  info("Stop tracing");
  JSTracer.stopTracing();
});

add_task(async function testTracingFilterSourceUrl() {
  // Test the `filterFrameSourceUrl` flag
  const sandbox = Cu.Sandbox("https://example.com");

  // Use a unique global (sandbox), but with two distinct scripts (first.js and second.js)
  const source1 = `function foo() { bar(); }`;
  Cu.evalInSandbox(source1, sandbox, null, "first.js", 1);

  // Only code running in that second source should be traced.
  const source2 = `function bar() { }`;
  Cu.evalInSandbox(source2, sandbox, null, "second.js", 1);

  // Pass an override method to catch all strings tentatively logged to stdout
  const logs = [];
  function loggingMethod(str) {
    logs.push(str);
  }

  info("Start tracing");
  JSTracer.startTracing({
    global: sandbox,
    filterFrameSourceUrl: "second",
    loggingMethod,
  });

  info("Call some code");
  sandbox.foo();

  Assert.equal(logs.length, 2);
  Assert.equal(logs[0], "Start tracing JavaScript\n");
  Assert.stringContains(logs[1], "λ bar");
  Assert.stringContains(logs[1], "second.js:1:18");

  info("Stop tracing");
  JSTracer.stopTracing();
});

add_task(async function testTracingAllGlobals() {
  // Test the `traceAllGlobals` flag

  // Create two distinct globals in order to verify that both are traced
  const sandbox1 = Cu.Sandbox("https://example.com");
  const sandbox2 = Cu.Sandbox("https://example.com");

  const source1 = `function foo() { bar(); }`;
  Cu.evalInSandbox(source1, sandbox1, null, "sandbox1.js", 1);

  const source2 = `function bar() { }`;
  Cu.evalInSandbox(source2, sandbox2, null, "sandbox2.js", 1);
  // Expose `bar` from sandbox2 as global in sandbox1, so that `foo` from sandbox1 can call it.
  sandbox1.bar = sandbox2.bar;

  // Pass an override method to catch all strings tentatively logged to stdout
  //
  // But in this test, we have to evaluate it in a special sandbox which will be ignored by the tracer.
  // Otherwise, the tracer would do an infinite loop on this loggingMethod.
  const ignoredGlobal = new Cu.Sandbox(null, { invisibleToDebugger: true });
  const loggingMethodString = `
    var logs = [];
    function loggingMethod(str) {
      logs.push(str);
    };
  `;
  Cu.evalInSandbox(
    loggingMethodString,
    ignoredGlobal,
    null,
    "loggin-method.js",
    1
  );
  const { loggingMethod, logs } = ignoredGlobal;

  info("Start tracing on all globals");
  JSTracer.startTracing({
    traceAllGlobals: true,
    loggingMethod,
  });

  // Call some code while being careful to not call anything else which may be traced
  sandbox1.foo();

  JSTracer.stopTracing();

  Assert.equal(logs.length, 4);
  Assert.equal(logs[0], "Start tracing JavaScript\n");
  Assert.stringContains(logs[1], "λ foo");
  Assert.stringContains(logs[1], "sandbox1.js:1:18");
  Assert.stringContains(logs[2], "λ bar");
  Assert.stringContains(logs[2], "sandbox2.js:1:18");
  Assert.equal(logs[3], "Stop tracing JavaScript\n");
});
