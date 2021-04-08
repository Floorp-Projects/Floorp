/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test can run for a long time on debug platforms.
requestLongerTimeout(5);

/* import-globals-from helper-collapsibilities.js */
Services.scriptloader.loadSubScript(
  CHROME_URL_ROOT + "helper-collapsibilities.js",
  this
);

const TOOLS = [
  "inspector",
  "webconsole",
  "jsdebugger",
  "styleeditor",
  "memory",
  "netmonitor",
  "storage",
  "accessibility",
];

// If the new performance panel is enabled, it's not available in about:debugging toolboxes.
if (
  Services.prefs.getBoolPref(
    "devtools.performance.new-panel-enabled",
    false
  ) === false
) {
  TOOLS.push("performance");
}

/**
 * Test whether about:devtools-toolbox display correctly after reloading.
 */
add_task(async function() {
  info("Force all debug target panes to be expanded");
  prepareCollapsibilitiesTest();

  for (const toolId of TOOLS) {
    await testReloadAboutDevToolsToolbox(toolId);
  }
});

async function testReloadAboutDevToolsToolbox(toolId) {
  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);
  // We set the options panel to be the default one because slower panels might lead to
  // race conditions which create leaks in debug mode.
  await pushPref("devtools.toolbox.selectedTool", "options");
  const {
    devtoolsBrowser,
    devtoolsTab,
    devtoolsWindow,
  } = await openAboutDevtoolsToolbox(document, tab, window);

  info(`Select tool: ${toolId}`);
  const toolbox = getToolbox(devtoolsWindow);
  await toolbox.selectTool(toolId);

  info("Wait for requests to settle before reloading");
  await toolbox.target.client.waitForRequestsToSettle();

  info("Reload about:devtools-toolbox page");
  devtoolsBrowser.reload();
  await gDevTools.once("toolbox-ready");
  ok(true, "Toolbox is re-created again");

  // Check that about:devtools-toolbox is still selected tab. See Bug 1570692.
  is(
    devtoolsBrowser,
    gBrowser.selectedBrowser,
    "about:devtools-toolbox is still selected"
  );

  info("Check whether about:devtools-toolbox page displays correctly");
  ok(
    devtoolsBrowser.contentDocument.querySelector(".debug-target-info"),
    "about:devtools-toolbox page displays correctly"
  );

  await closeAboutDevtoolsToolbox(document, devtoolsTab, window);
  await removeTab(tab);
}
