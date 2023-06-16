/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/debugger/test/mochitest/shared-head.js",
  this
);

// Test that the highlighter works when the debugger is paused.

function debuggerIsPaused(dbg) {
  return !!dbg.selectors.getIsPaused(dbg.selectors.getCurrentThread());
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

const IFRAME_SRC =
  "<style>" +
  "body {" +
  "margin:0;" +
  "height:100%;" +
  "background-color:red" +
  "}" +
  "</style><body>hello from iframe</body>";

const DOCUMENT_SRC =
  "<style>" +
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
  "<iframe src='data:text/html;charset=utf-8," +
  IFRAME_SRC +
  "'></iframe>" +
  "</body>";

const TEST_URI = "data:text/html;charset=utf-8," + DOCUMENT_SRC;

add_task(async function () {
  const { inspector, toolbox, highlighterTestFront, tab } =
    await openInspectorForURL(TEST_URI);

  await gDevTools.showToolboxForTab(tab, { toolId: "jsdebugger" });
  const dbg = await createDebuggerContext(toolbox);

  await waitForPaused(dbg);

  await gDevTools.showToolboxForTab(tab, { toolId: "inspector" });

  // Needed to get this test to pass consistently :(
  await waitForTime(1000);

  info("Waiting for box mode to show.");
  const body = await getNodeFront("body", inspector);
  await inspector.highlighters.showHighlighterTypeForNode(
    inspector.highlighters.TYPES.BOXMODEL,
    body
  );

  info("Waiting for element picker to become active.");
  await startPicker(toolbox);

  info("Moving mouse over iframe padding.");
  await hoverElement(inspector, "iframe", 1, 1);

  info("Performing checks");
  await isNodeCorrectlyHighlighted(highlighterTestFront, "iframe");
});
