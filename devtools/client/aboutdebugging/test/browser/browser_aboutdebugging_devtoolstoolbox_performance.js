/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This test can take a long time to run on debug builds.
requestLongerTimeout(2);

/* import-globals-from helper-collapsibilities.js */
Services.scriptloader.loadSubScript(
  CHROME_URL_ROOT + "helper-collapsibilities.js",
  this
);

/**
 * Check that graphs used by the old performance panel are correctly displayed.
 */
add_task(async function() {
  info("Force all debug target panes to be expanded");
  prepareCollapsibilitiesTest();

  info("Force old performance panel");
  await pushPref("devtools.performance.new-panel-enabled", false);

  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);
  const { devtoolsTab, devtoolsWindow } = await openAboutDevtoolsToolbox(
    document,
    tab,
    window
  );

  info("Select performance panel");
  const toolbox = getToolbox(devtoolsWindow);
  await toolbox.selectTool("performance");

  // Retrieve shared helpers for the old performance panel.
  const {
    startRecording,
    stopRecording,
  } = require("devtools/client/performance/test/helpers/actions");
  const performancePanel = toolbox.getCurrentPanel();
  await startRecording(performancePanel);

  const {
    idleWait,
  } = require("devtools/client/performance/test/helpers/wait-utils");
  await idleWait(100);

  info("Stop recording");
  await stopRecording(performancePanel);

  info("Select the call tree");
  const { EVENTS, DetailsView, JsCallTreeView } = performancePanel.panelWin;
  const rendered = once(JsCallTreeView, EVENTS.UI_JS_CALL_TREE_RENDERED);
  await DetailsView.selectView("js-calltree");

  info("Wait for the call tree to be rendered");
  await rendered;

  await closeAboutDevtoolsToolbox(document, devtoolsTab, window);
  await removeTab(tab);
});
