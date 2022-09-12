/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-unused-vars, no-undef */

"use strict";

const { BrowserToolboxLauncher } = ChromeUtils.importESModule(
  "resource://devtools/client/framework/browser-toolbox/Launcher.sys.mjs"
);
const { DevToolsClient } = require("devtools/client/devtools-client");

/**
 * Open up a browser toolbox and return a ToolboxTask object for interacting
 * with it. ToolboxTask has the following methods:
 *
 * importFunctions(object)
 *
 *   The object contains functions from this process which should be defined in
 *   the global evaluation scope of the toolbox. The toolbox cannot load testing
 *   files directly.
 *
 * destroy()
 *
 *   Destroy the browser toolbox and make sure it exits cleanly.
 *
 * @param {Object}:
 *        - {Boolean} enableBrowserToolboxFission: pass true to enable the OBT.
 *        - {Boolean} enableContentMessages: pass true to log content messages
 *          in the Console.
 *        - {Function} existingProcessClose: if truth-y, connect to an existing
 *          browser toolbox process rather than launching a new one and
 *          connecting to it.  The given function is expected to return an
 *          object containing an `exitCode`, like `{exitCode}`, and will be
 *          awaited in the returned `destroy()` function.  `exitCode` is
 *          asserted to be 0 (success).
 */
async function initBrowserToolboxTask({
  enableBrowserToolboxFission,
  enableContentMessages,
  existingProcessClose,
} = {}) {
  if (AppConstants.ASAN) {
    ok(
      false,
      "ToolboxTask cannot be used on ASAN builds. This test should be skipped (Bug 1591064)."
    );
  }

  await pushPref("devtools.chrome.enabled", true);
  await pushPref("devtools.debugger.remote-enabled", true);
  await pushPref("devtools.browsertoolbox.enable-test-server", true);
  await pushPref("devtools.debugger.prompt-connection", false);

  if (enableBrowserToolboxFission) {
    await pushPref("devtools.browsertoolbox.fission", true);
  }

  // This rejection seems to affect all tests using the browser toolbox.
  ChromeUtils.import(
    "resource://testing-common/PromiseTestUtils.jsm"
  ).PromiseTestUtils.allowMatchingRejectionsGlobally(/File closed/);

  let process;
  let dbgProcess;
  if (!existingProcessClose) {
    [process, dbgProcess] = await new Promise(resolve => {
      BrowserToolboxLauncher.init({
        onRun: (_process, _dbgProcess) => resolve([_process, _dbgProcess]),
        overwritePreferences: true,
      });
    });
    ok(true, "Browser toolbox started");
    is(
      BrowserToolboxLauncher.getBrowserToolboxSessionState(),
      true,
      "Has session state"
    );
  } else {
    ok(true, "Connecting to existing browser toolbox");
    ok(
      !enableBrowserToolboxFission,
      "Not trying to control preferences in existing browser toolbox"
    );
  }

  // The port of the DevToolsServer installed in the toolbox process is fixed.
  // See browser-toolbox/window.js
  let transport;
  while (true) {
    try {
      transport = await DevToolsClient.socketConnect({
        host: "localhost",
        port: 6001,
        webSocket: false,
      });
      break;
    } catch (e) {
      await waitForTime(100);
    }
  }
  ok(true, "Got transport");

  const client = new DevToolsClient(transport);
  await client.connect();

  const descriptorFront = await client.mainRoot.getMainProcess();
  const target = await descriptorFront.getTarget();
  const consoleFront = await target.getFront("console");

  ok(true, "Connected");

  if (enableContentMessages) {
    const preferenceFront = await client.mainRoot.getFront("preference");
    await preferenceFront.setBoolPref(
      "devtools.browserconsole.contentMessages",
      true
    );
  }

  await importFunctions({
    info: msg => dump(msg + "\n"),
    is: (a, b, description) => {
      let msg =
        "'" + JSON.stringify(a) + "' is equal to '" + JSON.stringify(b) + "'";
      if (description) {
        msg += " - " + description;
      }
      if (a !== b) {
        msg = "FAILURE: " + msg;
        dump(msg + "\n");
        throw new Error(msg);
      } else {
        msg = "SUCCESS: " + msg;
        dump(msg + "\n");
      }
    },
    ok: (a, description) => {
      let msg = "'" + JSON.stringify(a) + "' is true";
      if (description) {
        msg += " - " + description;
      }
      if (!a) {
        msg = "FAILURE: " + msg;
        dump(msg + "\n");
        throw new Error(msg);
      } else {
        msg = "SUCCESS: " + msg;
        dump(msg + "\n");
      }
    },
  });

  async function evaluateExpression(expression, options = {}) {
    const onEvaluationResult = consoleFront.once("evaluationResult");
    await consoleFront.evaluateJSAsync({ text: expression, ...options });
    return onEvaluationResult;
  }

  /**
   * Invoke the given function and argument(s) within the global evaluation scope
   * of the toolbox. The evaluation scope predefines the name "gToolbox" for the
   * toolbox itself.
   *
   * @param {value|Array<value>} arg
   *        If an Array is passed, we will consider it as the list of arguments
   *        to pass to `fn`. Otherwise we will consider it as the unique argument
   *        to pass to it.
   * @param {Function} fn
   *        Function to call in the global scope within the browser toolbox process.
   *        This function will be stringified and passed to the process via RDP.
   * @return {Promise<Value>}
   *        Return the primitive value returned by `fn`.
   */
  async function spawn(arg, fn) {
    // Use JSON.stringify to ensure that we can pass strings
    // as well as any JSON-able object.
    const argString = JSON.stringify(Array.isArray(arg) ? arg : [arg]);
    const rv = await evaluateExpression(`(${fn}).apply(null,${argString})`, {
      // Use the following argument in order to ensure waiting for the completion
      // of the promise returned by `fn` (in case this is an async method).
      mapped: { await: true },
    });
    if (rv.exceptionMessage) {
      throw new Error(`ToolboxTask.spawn failure: ${rv.exceptionMessage}`);
    } else if (rv.topLevelAwaitRejected) {
      throw new Error(`ToolboxTask.spawn await rejected`);
    }
    return rv.result;
  }

  async function importFunctions(functions) {
    for (const [key, fn] of Object.entries(functions)) {
      await evaluateExpression(`this.${key} = ${fn}`);
    }
  }

  async function importScript(script) {
    const response = await evaluateExpression(script);
    if (response.hasException) {
      ok(
        false,
        "ToolboxTask.spawn exception while importing script: " +
          response.exceptionMessage
      );
    }
  }

  let destroyed = false;
  async function destroy() {
    // No need to do anything if `destroy` was already called.
    if (destroyed) {
      return;
    }

    const closePromise = existingProcessClose
      ? existingProcessClose()
      : dbgProcess.wait();
    evaluateExpression("gToolbox.destroy()").catch(e => {
      // Ignore connection close as the toolbox destroy may destroy
      // everything quickly enough so that evaluate request is still pending
      if (!e.message.includes("Connection closed")) {
        throw e;
      }
    });

    const { exitCode } = await closePromise;
    ok(true, "Browser toolbox process closed");

    is(exitCode, 0, "The remote debugger process died cleanly");

    if (!existingProcessClose) {
      is(
        BrowserToolboxLauncher.getBrowserToolboxSessionState(),
        false,
        "No session state after closing"
      );
    }

    await client.close();
    destroyed = true;
  }

  // When tests involving using this task fail, the spawned Browser Toolbox is not
  // destroyed and might impact the next tests (e.g. pausing the content process before
  // the debugger from the content toolbox does). So make sure to cleanup everything.
  registerCleanupFunction(destroy);

  return {
    importFunctions,
    importScript,
    spawn,
    destroy,
  };
}
