/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that the highlighter works when the debugger is paused.

function debuggerIsPaused(dbg) {
  const {
    selectors: { getIsPaused, getCurrentThread },
    getState,
  } = dbg;
  return !!getIsPaused(getState(), getCurrentThread(getState()));
}

function waitForPaused(dbg) {
  return new Promise(resolve => {
    if (debuggerIsPaused(dbg)) {
      resolve();
      return;
    }

    const unsubscribe = dbg.store.subscribe(() => {
      if (debuggerIsPaused(dbg)) {
        unsubscribe();
        resolve();
      }
    });
  });
}

async function createDebuggerContext(toolbox) {
  const panel = await toolbox.getPanelWhenReady("jsdebugger");
  const win = panel.panelWin;
  const { store, client, selectors, actions } = panel.getVarsForTests();

  return {
    actions: actions,
    selectors: selectors,
    getState: store.getState,
    store: store,
    client: client,
    toolbox: toolbox,
    win: win,
    panel: panel,
  };
}

const IFRAME_SRC = "<style>" +
    "body {" +
      "margin:0;" +
      "height:100%;" +
      "background-color:red" +
    "}" +
  "</style><body>hello from iframe</body>";

const DOCUMENT_SRC = "<style>" +
    "iframe {" +
      "height:200px;" +
      "border: 11px solid black;" +
      "padding: 13px;" +
    "}" +
    "body,iframe {" +
      "margin:0" +
    "}" +
  "</style>" +
  "<body>" +
   "<script>setInterval('debugger', 100)</script>" +
   "<iframe src='data:text/html;charset=utf-8," + IFRAME_SRC + "'></iframe>" +
  "</body>";

const TEST_URI = "data:text/html;charset=utf-8," + DOCUMENT_SRC;

add_task(async function() {
  const { inspector, toolbox, testActor } = await openInspectorForURL(TEST_URI);

  const target = await TargetFactory.forTab(gBrowser.selectedTab);
  await gDevTools.showToolbox(target, "jsdebugger");
  const dbg = await createDebuggerContext(toolbox);

  await waitForPaused(dbg);

  await gDevTools.showToolbox(target, "inspector");

  // Needed to get this test to pass consistently :(
  await waitForTime(1000);

  info("Waiting for box mode to show.");
  const body = await getNodeFront("body", inspector);
  await inspector.highlighter.showBoxModel(body);

  info("Waiting for element picker to become active.");
  await startPicker(toolbox);

  info("Moving mouse over iframe padding.");
  await moveMouseOver("iframe", 1, 1);

  info("Performing checks");
  await testActor.isNodeCorrectlyHighlighted("iframe", is);

  function moveMouseOver(selector, x, y) {
    info("Waiting for element " + selector + " to be highlighted");
    testActor.synthesizeMouse({selector, x, y, options: {type: "mousemove"}});
    return inspector.inspector.nodePicker.once("picker-node-hovered");
  }
});
