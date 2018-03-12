/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that opening the toolbox doesn't throw when the previously selected
// tool is not supported.

const testToolDefinition = {
  id: "test-tool",
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
        panelDoc: iframeWindow.document
      };
    }
};

add_task(function* () {
  gDevTools.registerTool(testToolDefinition);
  let tab = yield addTab("about:blank");
  let target = TargetFactory.forTab(tab);

  let toolbox = yield gDevTools.showToolbox(target, testToolDefinition.id);
  is(toolbox.currentToolId, "test-tool", "test-tool was selected");
  yield gDevTools.closeToolbox(target);

  // Make the previously selected tool unavailable.
  testToolDefinition.isTargetSupported = () => false;

  target = TargetFactory.forTab(tab);
  toolbox = yield gDevTools.showToolbox(target);
  is(toolbox.currentToolId, "webconsole", "web console was selected");

  yield gDevTools.closeToolbox(target);
  gDevTools.unregisterTool(testToolDefinition.id);
  tab = toolbox = target = null;
  gBrowser.removeCurrentTab();
});
