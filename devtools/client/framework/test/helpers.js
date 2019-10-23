/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-unused-vars, no-undef */

"use strict";

const { BrowserToolboxProcess } = ChromeUtils.import(
  "resource://devtools/client/framework/ToolboxProcess.jsm"
);
const { DebuggerClient } = require("devtools/shared/client/debugger-client");

// Open up a browser toolbox and return a ToolboxTask object for interacting
// with it. ToolboxTask has the following methods:
//
// importFunctions(object)
//
//   The object contains functions from this process which should be defined in
//   the global evaluation scope of the toolbox. The toolbox cannot load testing
//   files directly.
//
// spawn(arg, function)
//
//   Invoke the given function and argument within the global evaluation scope
//   of the toolbox. The evaluation scope predefines the name "gToolbox" for the
//   toolbox itself.
//
// destroy()
//
//   Destroy the browser toolbox and make sure it exits cleanly.
async function initBrowserToolboxTask() {
  await pushPref("devtools.chrome.enabled", true);
  await pushPref("devtools.debugger.remote-enabled", true);
  await pushPref("devtools.browser-toolbox.allow-unsafe-script", true);

  // This rejection seems to affect all tests using the browser toolbox.
  ChromeUtils.import(
    "resource://testing-common/PromiseTestUtils.jsm"
  ).PromiseTestUtils.whitelistRejectionsGlobally(/File closed/);

  const process = await new Promise(onRun => {
    BrowserToolboxProcess.init(null, onRun, /* overwritePreferences */ true);
  });
  ok(true, "Browser toolbox started\n");
  is(
    BrowserToolboxProcess.getBrowserToolboxSessionState(),
    true,
    "Has session state"
  );

  // The port of the DebuggerServer installed in the toolbox process is fixed.
  // See toolbox-process-window.js
  let transport;
  while (true) {
    try {
      transport = await DebuggerClient.socketConnect({
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

  const client = new DebuggerClient(transport);
  await client.connect();

  ok(true, "Connected");

  const target = await client.mainRoot.getMainProcess();
  const console = await target.getFront("console");

  async function spawn(arg, fn) {
    const rv = await console.evaluateJSAsync(`(${fn})(${arg})`, {
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
      await console.evaluateJSAsync(`this.${key} = ${fn}`);
    }
  }

  async function destroy() {
    const closePromise = process._dbgProcess.wait();
    console.evaluateJSAsync("gToolbox.destroy()");

    const { exitCode } = await closePromise;
    ok(true, "Browser toolbox process closed");

    is(exitCode, 0, "The remote debugger process died cleanly");

    is(
      BrowserToolboxProcess.getBrowserToolboxSessionState(),
      false,
      "No session state after closing"
    );

    await client.close();
  }

  return {
    importFunctions,
    spawn,
    destroy,
  };
}
