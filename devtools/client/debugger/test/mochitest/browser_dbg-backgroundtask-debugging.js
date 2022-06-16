/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Tests that `--backgroundtask` debugging works.
 *
 * This test is subtle.  We launch a `--backgroundtask` with `--jsdebugger` and
 * `--wait-for-jsdebugger` within the test.  The background task infrastructure
 * launches a browser toolbox, and the test connects to that browser toolbox
 * instance.  The test drives the instance, verifying that the automatically
 * placed breakpoint paused execution.  It then closes the browser toolbox,
 * which resumes the execution and the task exits.
 *
 * In the future, it would be nice to change the task's running environment, for
 * example by redefining a failing exit code to exit code 0.  Attempts to do
 * this have so far not been robust in automation.
 */

"use strict";

requestLongerTimeout(4);

/* import-globals-from ../../../framework/browser-toolbox/test/helpers-browser-toolbox.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/framework/browser-toolbox/test/helpers-browser-toolbox.js",
  this
);

const { BackgroundTasksTestUtils } = ChromeUtils.import(
  "resource://testing-common/BackgroundTasksTestUtils.jsm"
);
BackgroundTasksTestUtils.init(this);
const do_backgroundtask = BackgroundTasksTestUtils.do_backgroundtask.bind(
  BackgroundTasksTestUtils
);

add_task(async function test_backgroundtask_debugger() {
  // In this test, the background task infrastructure launches the browser
  // toolbox.  The browser toolbox profile prefs are taken from the default
  // profile (which is the standard place for background tasks to look for
  // task-specific configuration).  The current test profile will be
  // considered the default profile by the background task apparatus, so this
  // is how we configure the browser toolbox profile prefs.
  //
  // These prefs are not set for the background task under test directly: the
  // relevant prefs are set in the background task defaults.
  await pushPref("devtools.chrome.enabled", true);
  await pushPref("devtools.debugger.remote-enabled", true);
  await pushPref("devtools.browsertoolbox.enable-test-server", true);
  await pushPref("devtools.debugger.prompt-connection", false);

  // Before we start the background task, the preference file must be flushed to disk.
  Services.prefs.savePrefFile(null);

  // This invokes the test-only background task `BackgroundTask_jsdebugger.jsm`.
  const p = do_backgroundtask("jsdebugger", {
    extraArgs: [`--jsdebugger`, "--wait-for-jsdebugger"],
    extraEnv: {
      // Force the current test profile to be considered the default profile.
      MOZ_BACKGROUNDTASKS_DEFAULT_PROFILE_PATH: Services.dirsvc.get(
        "ProfD",
        Ci.nsIFile
      ).path,
    },
  });

  ok(true, "Launched background task");

  const existingProcessClose = async () => {
    const exitCode = await p;
    return { exitCode };
  };
  const ToolboxTask = await initBrowserToolboxTask({ existingProcessClose });

  await ToolboxTask.importFunctions({
    checkEvaluateInTopFrame,
    evaluateInTopFrame,
    createDebuggerContext,
    expandAllScopes,
    findElement,
    findElementWithSelector,
    getSelector,
    getThreadContext,
    getVisibleSelectedFrameLine,
    isPaused,
    resume,
    stepOver,
    toggleObjectInspectorNode,
    toggleScopeNode,
    waitForElement,
    waitForLoadedScopes,
    waitForPaused,
    waitForResumed,
    waitForSelectedSource,
    waitForState,
    waitUntil,
    log: (msg, data) =>
      console.log(`${msg} ${!data ? "" : JSON.stringify(data)}`),
    info: (msg, data) =>
      console.info(`${msg} ${!data ? "" : JSON.stringify(data)}`),
  });

  // ToolboxTask.spawn passes input arguments by stringify-ing them via string
  // concatenation.  But functions do not survive this process, so we manually
  // recreate (in the toolbox process) the single function the `expandAllScopes`
  // invocation in this test needs.
  await ToolboxTask.spawn(JSON.stringify(selectors), async _selectors => {
    this.selectors = _selectors;
    this.selectors.scopeNode = i =>
      `.scopes-list .tree-node:nth-child(${i}) .object-label`;
  });

  // The debugger should automatically be selected.
  await ToolboxTask.spawn(null, async () => {
    await waitUntil(() => gToolbox.currentToolId == "jsdebugger");
  });
  ok(true, "Debugger selected");

  // The debugger should automatically pause.
  await ToolboxTask.spawn(null, async () => {
    try {
      /* global gToolbox */
      // Wait for the debugger to finish loading.
      await gToolbox.getPanelWhenReady("jsdebugger");

      const dbg = createDebuggerContext(gToolbox);

      // Scopes are supposed to be automatically expanded, but with
      // `setBreakpointOnLoad` that doesn't seem to be happening.  Explicitly
      // expand scopes so that they are expanded for `waitForPaused`.
      await expandAllScopes(dbg);

      await waitForPaused(dbg);

      if (!gToolbox.isHighlighted("jsdebugger")) {
        throw new Error("Debugger not highlighted");
      }
    } catch (e) {
      console.log("Caught exception in spawn", e);
      throw e;
    }
  });
  ok(true, "Paused in backgroundtask script");

  // If we `resume`, then the task completes and the Browser Toolbox exits,
  // which isn't handled cleanly by `spawn`, resulting in a test time out.  This
  // closes the toolbox "from the inside", which continues execution.  The test
  // then waits for the background task to terminate with exit code 0.
  await ToolboxTask.destroy();
});
