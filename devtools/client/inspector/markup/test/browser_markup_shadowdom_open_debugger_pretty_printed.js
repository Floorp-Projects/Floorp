/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that clicking on the "custom" badge opens the debugger to the pretty-printed
// custom element definition.

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/debugger/test/mochitest/shared-head.js",
  this
);

const TEST_URL =
  URL_ROOT + "doc_markup_shadowdom_open_debugger_pretty_printed.html";

add_task(async function () {
  info("Open inspector.");
  await clearDebuggerPreferences();
  const { inspector, toolbox } = await openInspectorForURL(TEST_URL);

  await selectNode("test-component", inspector);
  const testFront = await getNodeFront("test-component", inspector);

  const testContainer = inspector.markup.getContainer(testFront);
  const customBadge = testContainer.elt.querySelector(
    ".inspector-badge.interactive[data-custom]"
  );

  info("Click custom badge.");
  customBadge.click();

  await toolbox.getPanelWhenReady("jsdebugger");
  const dbg = createDebuggerContext(toolbox);

  await waitForSelectedSource(dbg, "shadowdom_open_debugger.min.js");
  await waitForSelectedLocation(dbg, 1);

  info("Pretty-print source.");
  clickElement(dbg, "prettyPrintButton");
  await waitForSelectedSource(dbg, "shadowdom_open_debugger.min.js:formatted");
  info("Switch back to the original source.");
  await selectSource(dbg, "shadowdom_open_debugger.min.js");

  info("Return to inspector.");
  await toolbox.selectTool("inspector");

  info("Click custom badge again.");
  customBadge.click();

  await waitForSelectedSource(dbg, "shadowdom_open_debugger.min.js:formatted");
  await waitForSelectedLocation(dbg, 5);
});
