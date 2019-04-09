/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// There are shutdown issues for which multiple rejections are left uncaught.
// See bug 1018184 for resolving these issues.
const { PromiseTestUtils } = ChromeUtils.import("resource://testing-common/PromiseTestUtils.jsm");
PromiseTestUtils.whitelistRejectionsGlobally(/Component not initialized/);

/**
 * Tests if on clicking the stack frame, UI switches to the Debugger panel.
 */
add_task(async function() {
  // Set a higher panel height in order to get full CodeMirror content
  await pushPref("devtools.toolbox.footer.height", 400);

  // Async stacks aren't on by default in all builds
  await pushPref("javascript.options.asyncstack", true);

  const { tab, monitor, toolbox } = await initNetMonitor(POST_DATA_URL);
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  // Execute requests.
  await performRequests(monitor, tab, 2);

  info("Clicking stack-trace tab and waiting for stack-trace panel to open");
  const waitForTab = waitForDOM(document, "#stack-trace-tab");
  // Click on the first request
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelector(".request-list-item"));
  await waitForTab;
  const waitForPanel = waitForDOM(document, "#stack-trace-panel .frame-link", 5);
  // Open the stack-trace tab for that request
  document.getElementById("stack-trace-tab").click();
  await waitForPanel;

  const frameLinkNode = document.querySelector(".frame-link");
  await checkClickOnNode(toolbox, frameLinkNode);

  await teardown(monitor);
});

/**
 * Helper function for testOpenInDebugger.
 */
async function checkClickOnNode(toolbox, frameLinkNode) {
  info("checking click on node location");

  const url = frameLinkNode.getAttribute("data-url");
  ok(url, `source url found ("${url}")`);

  const line = frameLinkNode.getAttribute("data-line");
  ok(line, `source line found ("${line}")`);

  // create the promise
  const onJsDebuggerSelected = toolbox.once("jsdebugger-selected");
  // Fire the click event
  frameLinkNode.querySelector(".frame-link-source").click();
  // wait for the promise to resolve
  await onJsDebuggerSelected;

  const dbg = await toolbox.getPanelWhenReady("jsdebugger");
  await waitUntil(() => dbg._selectors.getSelectedSource(dbg._getState()));
  is(
    dbg._selectors.getSelectedSource(dbg._getState()).url,
    url,
    "expected source url"
  );
}
