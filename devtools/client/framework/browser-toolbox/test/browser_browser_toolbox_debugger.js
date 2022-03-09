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

/* eslint-disable mozilla/no-arbitrary-setTimeout */

const { fetch } = require("devtools/shared/DevToolsUtils");

const debuggerHeadURL =
  CHROME_URL_ROOT + "../../../debugger/test/mochitest/shared-head.js";

add_task(async function runTest() {
  let { content: debuggerHead } = await fetch(debuggerHeadURL);

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

  info("### First test breakpoint in the parent process script");
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

  await ToolboxTask.spawn(`"${testUrl}"`, async _testUrl => {
    /* global gToolbox, createDebuggerContext, waitForSources, waitForPaused,
          addBreakpoint, assertPausedAtSourceAndLine, stepIn, findSource,
          removeBreakpoint, resume, selectSource, assertNotPaused, assertBreakpoint */
    const { Services } = ChromeUtils.import(
      "resource://gre/modules/Services.jsm"
    );

    Services.prefs.clearUserPref("devtools.debugger.tabs");
    Services.prefs.clearUserPref("devtools.debugger.pending-selected-location");

    info("Waiting for debugger load");
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

    const source = findSource(dbg, fileName);
    assertPausedAtSourceAndLine(dbg, source.id, 2);
    is(
      dbg.selectors.getBreakpointCount(),
      1,
      "There is exactly one breakpoint"
    );

    await stepIn(dbg);

    assertPausedAtSourceAndLine(dbg, source.id, 3);
    is(
      dbg.selectors.getBreakpointCount(),
      1,
      "We still have only one breakpoint after step-in"
    );

    // Remove the breakpoint before resuming in order to prevent hitting the breakpoint
    // again during test closing.
    await removeBreakpoint(dbg, source.id, 2);

    await resume(dbg);

    // Let a change for the interval to re-execute
    await new Promise(r => setTimeout(r, 1000));

    is(dbg.selectors.getBreakpointCount(), 0, "There is no more breakpoints");

    assertNotPaused(dbg);
  });

  clearInterval(interval);

  info("### Now test breakpoint in a privileged content process script");
  const testUrl2 = `http://mozilla.org/content-process-test-${id}.js`;
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [testUrl2], testUrl => {
    // Use a sandbox in order to have a URL to set a breakpoint
    const s = Cu.Sandbox("http://mozilla.org");
    Cu.evalInSandbox(
      "(" +
        function() {
          this.foo = function foo() {
            return 1;
          };
        } +
        ").call(this)",
      s,
      "1.8",
      testUrl,
      0
    );
    content.interval = content.setInterval(s.foo, 1000);
  });
  await ToolboxTask.spawn(`"${testUrl2}"`, async _testUrl => {
    const dbg = createDebuggerContext(gToolbox);

    const fileName = _testUrl.match(/content-process-test.*\.js/)[0];
    await waitForSources(dbg, _testUrl);

    await selectSource(dbg, fileName);

    const onPaused = waitForPaused(dbg);
    await addBreakpoint(dbg, fileName, 2);
    await onPaused;

    const source = findSource(dbg, fileName);
    assertPausedAtSourceAndLine(dbg, source.id, 2);
    await assertBreakpoint(dbg, 2);
    is(dbg.selectors.getBreakpointCount(), 1, "We have exactly one breakpoint");

    await stepIn(dbg);

    assertPausedAtSourceAndLine(dbg, source.id, 3);
    is(
      dbg.selectors.getBreakpointCount(),
      1,
      "We still have only one breakpoint after step-in"
    );

    // Remove the breakpoint before resuming in order to prevent hitting the breakpoint
    // again during test closing.
    await removeBreakpoint(dbg, source.id, 2);

    await resume(dbg);

    // Let a change for the interval to re-execute
    await new Promise(r => setTimeout(r, 1000));

    is(dbg.selectors.getBreakpointCount(), 0, "There is no more breakpoints");

    assertNotPaused(dbg);
  });
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.clearInterval(content.interval);
  });

  await ToolboxTask.destroy();
});
