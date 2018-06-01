/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { DevToolsShim } =
    ChromeUtils.import("chrome://devtools-startup/content/DevToolsShim.jsm", {});
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm", {});

// Test the DevToolsShim

/**
 * Create a mocked version of DevTools that records all calls made to methods expected
 * to be called by DevToolsShim.
 */
function createMockDevTools() {
  const methods = [
    "on",
    "off",
    "emit",
    "saveDevToolsSession",
    "restoreDevToolsSession",
  ];

  const mock = {
    callLog: {}
  };

  for (const method of methods) {
    // Create a stub for method, that only pushes its arguments in the inner callLog
    mock[method] = function(...args) {
      mock.callLog[method].push(args);
    };
    mock.callLog[method] = [];
  }

  return mock;
}

/**
 * Check if a given method was called an expected number of times, and finally check the
 * arguments provided to the last call, if appropriate.
 */
function checkCalls(mock, method, length, lastArgs) {
  ok(mock.callLog[method].length === length,
      "Devtools.on was called the expected number of times");

  // If we don't want to check the last call or if the method was never called, bail out.
  if (!lastArgs || length === 0) {
    return;
  }

  for (let i = 0; i < lastArgs.length; i++) {
    const expectedArg = lastArgs[i];
    ok(mock.callLog[method][length - 1][i] === expectedArg,
        `Devtools.${method} was called with the expected argument (index ${i})`);
  }
}

function test_register_unregister() {
  ok(!DevToolsShim.isInitialized(), "DevTools are not initialized");

  DevToolsShim.register(createMockDevTools());
  ok(DevToolsShim.isInitialized(), "DevTools are initialized");

  DevToolsShim.unregister();
  ok(!DevToolsShim.isInitialized(), "DevTools are not initialized");
}

function test_on_is_forwarded_to_devtools() {
  ok(!DevToolsShim.isInitialized(), "DevTools are not initialized");

  function cb1() {}
  function cb2() {}
  const mock = createMockDevTools();

  DevToolsShim.on("test_event", cb1);
  DevToolsShim.register(mock);
  checkCalls(mock, "on", 1, ["test_event", cb1]);

  DevToolsShim.on("other_event", cb2);
  checkCalls(mock, "on", 2, ["other_event", cb2]);
}

function test_off_called_before_registering_devtools() {
  ok(!DevToolsShim.isInitialized(), "DevTools are not initialized");

  function cb1() {}
  const mock = createMockDevTools();

  DevToolsShim.on("test_event", cb1);
  DevToolsShim.off("test_event", cb1);

  DevToolsShim.register(mock);
  checkCalls(mock, "on", 0);
}

function test_off_called_before_with_bad_callback() {
  ok(!DevToolsShim.isInitialized(), "DevTools are not initialized");

  function cb1() {}
  function cb2() {}
  const mock = createMockDevTools();

  DevToolsShim.on("test_event", cb1);
  DevToolsShim.off("test_event", cb2);

  DevToolsShim.register(mock);
  // on should still be called
  checkCalls(mock, "on", 1, ["test_event", cb1]);
  // Calls to off should not be held and forwarded.
  checkCalls(mock, "off", 0);
}

function test_events() {
  ok(!DevToolsShim.isInitialized(), "DevTools are not initialized");

  const mock = createMockDevTools();
  // Check emit was not called.
  checkCalls(mock, "emit", 0);

  // Check emit is called once with the devtools-registered event.
  DevToolsShim.register(mock);
  checkCalls(mock, "emit", 1, ["devtools-registered"]);

  // Check emit is called once with the devtools-unregistered event.
  DevToolsShim.unregister();
  checkCalls(mock, "emit", 2, ["devtools-unregistered"]);
}

function test_restore_session_apis() {
  // Backup method and preferences that will be updated for the test.
  const initDevToolsBackup = DevToolsShim.initDevTools;
  const devtoolsEnabledValue = Services.prefs.getBoolPref("devtools.enabled");

  // Create fake session objects to restore.
  const sessionWithoutDevTools = {};
  const sessionWithDevTools = {
    scratchpads: [{}],
    browserConsole: true,
  };

  function checkRestoreSessionNotApplied(policyDisabled, enabled) {
    Services.prefs.setBoolPref("devtools.enabled", enabled);
    Services.prefs.setBoolPref("devtools.policy.disabled", policyDisabled);
    ok(!DevToolsShim.isInitialized(), "DevTools are not initialized");
    ok(!DevToolsShim.isEnabled(), "DevTools are not enabled");

    // Check that save & restore DevToolsSession don't initialize the tools and don't
    // crash.
    DevToolsShim.saveDevToolsSession({});
    DevToolsShim.restoreDevToolsSession(sessionWithDevTools);
    ok(!DevToolsShim.isInitialized(), "DevTools are still not initialized");
  }

  // Tools are disabled by policy and not enabled
  checkRestoreSessionNotApplied(true, false);
  // Tools are not disabled by policy, but not enabled
  checkRestoreSessionNotApplied(false, false);
  // Tools are disabled by policy and "considered" as enabled (see Bug 1440675)
  checkRestoreSessionNotApplied(true, true);

  Services.prefs.setBoolPref("devtools.enabled", true);
  Services.prefs.setBoolPref("devtools.policy.disabled", false);
  ok(DevToolsShim.isEnabled(), "DevTools are enabled");
  ok(!DevToolsShim.isInitialized(), "DevTools are not initialized");

  // Check that DevTools are not initialized when calling restoreDevToolsSession without
  // DevTools related data.
  DevToolsShim.restoreDevToolsSession(sessionWithoutDevTools);
  ok(!DevToolsShim.isInitialized(), "DevTools are still not initialized");

  const mock = createMockDevTools();
  DevToolsShim.initDevTools = () => {
    // Next call to restoreDevToolsSession is expected to initialize DevTools, which we
    // simulate here by registering our mock.
    DevToolsShim.register(mock);
  };

  DevToolsShim.restoreDevToolsSession(sessionWithDevTools);
  checkCalls(mock, "restoreDevToolsSession", 1, [sessionWithDevTools]);

  ok(DevToolsShim.isInitialized(), "DevTools are initialized");

  DevToolsShim.saveDevToolsSession({});
  checkCalls(mock, "saveDevToolsSession", 1, []);

  // Restore initial backups.
  DevToolsShim.initDevTools = initDevToolsBackup;
  Services.prefs.setBoolPref("devtools.enabled", devtoolsEnabledValue);
}

function run_test() {
  test_register_unregister();
  DevToolsShim.unregister();

  test_on_is_forwarded_to_devtools();
  DevToolsShim.unregister();

  test_off_called_before_registering_devtools();
  DevToolsShim.unregister();

  test_off_called_before_with_bad_callback();
  DevToolsShim.unregister();

  test_restore_session_apis();
  DevToolsShim.unregister();

  test_events();
}
