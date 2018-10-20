/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that opening the toolbox doesn't throw when the previously selected
// tool is not supported.

const testToolDefinition = {
  id: "testTool",
  isTargetSupported: () => true,
  visibilityswitch: "devtools.test-tool.enabled",
  url: "about:blank",
  label: "someLabel",
  build: (iframeWindow, toolbox) => {
    return {
        target: toolbox.target,
        toolbox: toolbox,
        isReady: true,
        destroy: () => {},
        panelDoc: iframeWindow.document,
    };
  },
};

add_task(async function() {
  gDevTools.registerTool(testToolDefinition);
  let tab = await addTab("about:blank");
  let target = await TargetFactory.forTab(tab);

  let toolbox = await gDevTools.showToolbox(target, testToolDefinition.id);
  is(toolbox.currentToolId, "testTool", "test-tool was selected");
  await gDevTools.closeToolbox(target);

  // Make the previously selected tool unavailable.
  testToolDefinition.isTargetSupported = () => false;

  target = await TargetFactory.forTab(tab);
  toolbox = await gDevTools.showToolbox(target);
  is(toolbox.currentToolId, "webconsole", "web console was selected");

  await gDevTools.closeToolbox(target);
  gDevTools.unregisterTool(testToolDefinition.id);
  tab = toolbox = target = null;
  gBrowser.removeCurrentTab();
});
