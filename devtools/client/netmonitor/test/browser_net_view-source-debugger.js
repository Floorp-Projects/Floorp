/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if on clicking the stack frame, UI switches to the Debugger panel.
 */
add_task(async function () {
  // Set a higher panel height in order to get full CodeMirror content
  await pushPref("devtools.toolbox.footer.height", 400);

  let { tab, monitor, toolbox } = await initNetMonitor(POST_DATA_URL);
  info("Starting test... ");

  let { document, store, windowRequire } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  let waitForContentRequests = waitForNetworkEvents(monitor, 0, 2);
  await ContentTask.spawn(tab.linkedBrowser, {},
    () => content.wrappedJSObject.performRequests());
  await waitForContentRequests;

  info("Clicking stack-trace tab and waiting for stack-trace panel to open");
  let wait = waitForDOM(document, "#stack-trace-panel .frame-link", 4);
  // Click on the first request
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelector(".request-list-item"));
  // Open the stack-trace tab for that request
  document.getElementById("stack-trace-tab").click();
  await wait;

  let frameLinkNode = document.querySelector(".frame-link");
  await checkClickOnNode(toolbox, frameLinkNode);

  await teardown(monitor);
});

/**
 * Helper function for testOpenInDebugger.
 */
async function checkClickOnNode(toolbox, frameLinkNode) {
  info("checking click on node location");

  let url = frameLinkNode.getAttribute("data-url");
  ok(url, `source url found ("${url}")`);

  let line = frameLinkNode.getAttribute("data-line");
  ok(line, `source line found ("${line}")`);

  // create the promise
  const onJsDebuggerSelected = toolbox.once("jsdebugger-selected");
  // Fire the click event
  frameLinkNode.querySelector(".frame-link-source").click();
  // wait for the promise to resolve
  await onJsDebuggerSelected;

  let dbg = toolbox.getPanel("jsdebugger");
  is(
    dbg._selectors.getSelectedSource(dbg._getState()).get("url"),
    url,
    "expected source url"
  );
}

