/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-unused-vars, no-undef */

"use strict";

const { BrowserToolboxLauncher } = ChromeUtils.import(
  "resource://devtools/client/framework/browser-toolbox/Launcher.jsm"
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
 * spawn(arg, function)
 *
 *   Invoke the given function and argument within the global evaluation scope
 *   of the toolbox. The evaluation scope predefines the name "gToolbox" for the
 *   toolbox itself.
 *
 * destroy()
 *
 *   Destroy the browser toolbox and make sure it exits cleanly.
 *
 * @param {Object}:
 *        - {Boolean} enableBrowserToolboxFission: pass true to enable the OBT.
 *        - {Boolean} enableContentMessages: pass true to log content messages
 *          in the Console.
 */
async function initBrowserToolboxTask({
  enableBrowserToolboxFission,
  enableContentMessages,
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

  const process = await new Promise(onRun => {
    BrowserToolboxLauncher.init(null, onRun, /* overwritePreferences */ true);
  });
  ok(true, "Browser toolbox started\n");
  is(
    BrowserToolboxLauncher.getBrowserToolboxSessionState(),
    true,
    "Has session state"
  );

  // The port of the DevToolsServer installed in the toolbox process is fixed.
  // See browser-toolbox-window.js
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

  const preferenceFront = await client.mainRoot.getFront("preference");

  if (enableContentMessages) {
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

  async function spawn(arg, fn) {
    const rv = await evaluateExpression(`(${fn})(${arg})`, {
      mapped: { await: true },
    });
    if (rv.exception) {
      throw new Error(`ToolboxTask.spawn failure: ${rv.exception.message}`);
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

  async function destroy() {
    const closePromise = process._dbgProcess.wait();
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

    is(
      BrowserToolboxLauncher.getBrowserToolboxSessionState(),
      false,
      "No session state after closing"
    );

    await client.close();
  }

  return {
    importFunctions,
    importScript,
    spawn,
    destroy,
  };
}
