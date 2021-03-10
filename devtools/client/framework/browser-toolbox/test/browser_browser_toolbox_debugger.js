/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This test asserts that the new debugger works from the browser toolbox process

// There are shutdown issues for which multiple rejections are left uncaught.
// See bug 1018184 for resolving these issues.
const { PromiseTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromiseTestUtils.jsm"
);
PromiseTestUtils.allowMatchingRejectionsGlobally(/File closed/);

// On debug test runner, it takes about 50s to run the test.
requestLongerTimeout(4);

const { fetch } = require("devtools/shared/DevToolsUtils");

const debuggerHeadURL =
  CHROME_URL_ROOT + "../../../debugger/test/mochitest/head.js";
const helpersURL =
  CHROME_URL_ROOT + "../../../debugger/test/mochitest/helpers.js";
const helpersContextURL =
  CHROME_URL_ROOT + "../../../debugger/test/mochitest/helpers/context.js";

add_task(async function runTest() {
  const s = Cu.Sandbox("http://mozilla.org");

  // Use a unique id for the fake script name in order to be able to run
  // this test more than once. That's because the Sandbox is not immediately
  // destroyed and so the debugger would display only one file but not necessarily
  // connected to the latest sandbox.
  const id = new Date().getTime();

  // Pass a fake URL to evalInSandbox. If we just pass a filename,
  // Debugger is going to fail and only display root folder (`/`) listing.
  // But it won't try to fetch this url and use sandbox content as expected.
  const testUrl = `http://mozilla.org/browser-toolbox-test-${id}.js`;
  Cu.evalInSandbox(
    "(" +
      function() {
        this.plop = function plop() {
          return 1;
        };
      } +
      ").call(this)",
    s,
    "1.8",
    testUrl,
    0
  );

  // Execute the function every second in order to trigger the breakpoint
  const interval = setInterval(s.plop, 1000);

  let { content: debuggerHead } = await fetch(debuggerHeadURL);

  // Also include the debugger helpers which are separated from debugger's head to be
  // reused in other modules.
  const { content: debuggerHelpers } = await fetch(helpersURL);
  const { content: debuggerContextHelpers } = await fetch(helpersContextURL);
  debuggerHead = debuggerHead + debuggerContextHelpers + debuggerHelpers;

  // We remove its import of shared-head, which isn't available in browser toolbox process
  // And isn't needed thanks to testHead's symbols
  debuggerHead = debuggerHead.replace(
    /Services.scriptloader.loadSubScript[^\)]*\);/g,
    ""
  );

  const ToolboxTask = await initBrowserToolboxTask({
    enableBrowserToolboxFission: true,
  });
  await ToolboxTask.importFunctions({
    // head.js uses this method
    registerCleanupFunction: () => {},
    waitForDispatch,
    waitUntil,
  });
  await ToolboxTask.importScript(debuggerHead);

  await ToolboxTask.spawn(`"${testUrl}"`, async _testUrl => {
    /* global createDebuggerContext, waitForSources,
          waitForPaused, addBreakpoint, assertPausedLocation, stepIn,
          findSource, removeBreakpoint, resume, selectSource */
    const { Services } = ChromeUtils.import(
      "resource://gre/modules/Services.jsm"
    );

    Services.prefs.clearUserPref("devtools.debugger.tabs");
    Services.prefs.clearUserPref("devtools.debugger.pending-selected-location");

    info("Waiting for debugger load");
    /* global gToolbox */
    await gToolbox.selectTool("jsdebugger");
    const dbg = createDebuggerContext(gToolbox);
    const window = dbg.win;
    const document = window.document;

    await waitForSources(dbg, _testUrl);

    info("Loaded, selecting the test script to debug");
    // First expand the main thread
    const mainThread = [...document.querySelectorAll(".tree-node")].find(
      node => {
        return node.querySelector(".label").textContent.trim() == "Main Thread";
      }
    );
    mainThread.querySelector(".arrow").click();

    // Then expand the domain
    const domain = [...document.querySelectorAll(".tree-node")].find(node => {
      return node.querySelector(".label").textContent.trim() == "mozilla.org";
    });
    const arrow = domain.querySelector(".arrow");
    arrow.click();

    const fileName = _testUrl.match(/browser-toolbox-test.*\.js/)[0];

    // And finally the expected source
    let script = [...document.querySelectorAll(".tree-node")].find(node => {
      return node.textContent.includes(fileName);
    });
    script = script.querySelector(".node");
    script.click();

    const onPaused = waitForPaused(dbg);
    await selectSource(dbg, fileName);
    await addBreakpoint(dbg, fileName, 2);

    await onPaused;

    assertPausedLocation(dbg, fileName, 2);

    await stepIn(dbg);

    assertPausedLocation(dbg, fileName, 3);

    // Remove the breakpoint before resuming in order to prevent hitting the breakpoint
    // again during test closing.
    const source = findSource(dbg, fileName);
    await removeBreakpoint(dbg, source.id, 2);

    await resume(dbg);
  });

  clearInterval(interval);

  await ToolboxTask.destroy();
});
