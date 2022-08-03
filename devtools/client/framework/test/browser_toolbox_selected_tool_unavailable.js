/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that opening the toolbox doesn't throw when the previously selected
// tool is not supported.

const testToolDefinition = {
  id: "testTool",
  isToolSupported: () => true,
  visibilityswitch: "devtools.test-tool.enabled",
  url: "about:blank",
  label: "someLabel",
  build: (iframeWindow, toolbox) => {
    return {
      target: toolbox.target,
      toolbox,
      isReady: true,
      destroy: () => {},
      panelDoc: iframeWindow.document,
    };
  },
};

add_task(async function() {
  gDevTools.registerTool(testToolDefinition);
  let tab = await addTab("about:blank");

  let toolbox = await gDevTools.showToolboxForTab(tab, {
    toolId: testToolDefinition.id,
  });
  is(toolbox.currentToolId, "testTool", "test-tool was selected");
  await toolbox.destroy();

  // Make the previously selected tool unavailable.
  testToolDefinition.isToolSupported = () => false;

  toolbox = await gDevTools.showToolboxForTab(tab);
  is(toolbox.currentToolId, "webconsole", "web console was selected");

  await toolbox.destroy();
  gDevTools.unregisterTool(testToolDefinition.id);
  tab = toolbox = null;
  gBrowser.removeCurrentTab();
});
