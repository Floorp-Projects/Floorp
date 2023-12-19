/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { addTracingListener, removeTracingListener, startTracing, stopTracing } =
  ChromeUtils.import("resource://devtools/server/tracer/tracer.jsm");

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
  addTracingListener(listener);

  const sandbox = Cu.Sandbox("https://example.com");
  Cu.evalInSandbox("function bar() {}; function foo() {bar()};", sandbox);

  info("Start tracing");
  startTracing({ global: sandbox, prefix: "testContentPrefix" });
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
  stopTracing();
  Assert.equal(toggles.length, 2);
  Assert.equal(toggles[1], false);

  info("Recall code after stop, no more traces are logged");
  sandbox.foo();
  Assert.equal(frames.length, 0);

  info("Start tracing again, and recall code");
  startTracing({ global: sandbox, prefix: "testContentPrefix" });
  sandbox.foo();
  info("New traces are logged");
  Assert.equal(frames.length, 2);

  info("Unregister the listener and recall code");
  removeTracingListener(listener);
  sandbox.foo();
  info("No more traces are logged");
  Assert.equal(frames.length, 2);

  info("Stop tracing");
  stopTracing();
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
  addTracingListener(listenerSandbox.listener);

  info("Start tracing");
  startTracing({ global: null, prefix: "testPrefix" });
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
  Assert.equal(beforeLastFrame.depth, 0);
  Assert.equal(beforeLastFrame.formatedDisplayName, "λ foo");
  Assert.equal(beforeLastFrame.prefix, "testPrefix: ");
  Assert.ok(beforeLastFrame.frame);
  Assert.equal(lastFrame.depth, 1);
  Assert.equal(lastFrame.formatedDisplayName, "λ bar");
  Assert.equal(lastFrame.prefix, "testPrefix: ");
  Assert.ok(lastFrame.frame);

  info("Stop tracing");
  stopTracing();
  Assert.equal(listenerSandbox.toggles.length, 2);
  Assert.equal(listenerSandbox.toggles[1], false);

  removeTracingListener(listenerSandbox.listener);
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
  startTracing({ global: sandbox, traceValues: true, loggingMethod });

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
  stopTracing();
});
