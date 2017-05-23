/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests DevToolsShim API works as expected when DevTools are available.

const TOOL_ID = "test-tool";

const { DevToolsShim } =
    Components.utils.import("chrome://devtools-shim/content/DevToolsShim.jsm", {});

add_task(function* () {
  yield addTab("about:blank");

  yield testThemeRegistrationWithShim();
  yield testToolRegistrationWithShim();

  gBrowser.removeCurrentTab();
});

// Test that theme registration works with the DevToolsShim.
function* testThemeRegistrationWithShim() {
  let themeId = yield new Promise(resolve => {
    DevToolsShim.on("theme-registered", function onThemeRegistered(e, id) {
      resolve(id);
    });

    DevToolsShim.registerTheme({
      id: "test-theme",
      label: "Test theme",
      stylesheets: [CHROME_URL_ROOT + "doc_theme.css"],
      classList: ["theme-test"],
    });
  });

  is(themeId, "test-theme", "theme-registered event handler sent theme id");

  ok(gDevTools.getThemeDefinitionMap().has(themeId), "theme added to map");
  DevToolsShim.unregisterTheme("test-theme");
  ok(!gDevTools.getThemeDefinitionMap().has(themeId), "theme removed");
}

// Test that tool registration works with the DevToolsShim.
function* testToolRegistrationWithShim() {
  let toolDefinition = {
    id: TOOL_ID,
    isTargetSupported: () => true,
    visibilityswitch: "devtools.test-tool.enabled",
    url: "about:blank",
    label: "someLabel",
    build: function (iframeWindow, toolbox) {
      let panel = createTestPanel(iframeWindow, toolbox);
      return panel.open();
    },
  };

  // Check that tool registration works when using the DevToolsShim.
  ok(!gDevTools.getToolDefinitionMap().has(TOOL_ID), "The tool is not registered");
  DevToolsShim.registerTool(toolDefinition);
  ok(gDevTools.getToolDefinitionMap().has(TOOL_ID), "The tool is registered");

  let events = {};

  // Check that events can be listened to on the shim in the same way as on gDevTools
  DevToolsShim.on(TOOL_ID + "-init", function onTool1Init(event, toolbox, iframe) {
    DevToolsShim.off(TOOL_ID + "-init", onTool1Init);
    ok(toolbox, "toolbox argument available");
    ok(iframe, "iframe argument available");
    events.init = true;
  });

  DevToolsShim.on(TOOL_ID + "-ready", function onToolReady(event, toolbox, iframe) {
    DevToolsShim.off(TOOL_ID + "-ready", onToolReady);
    ok(toolbox, "toolbox argument available");
    ok(iframe, "iframe argument available");
    events.ready = true;
  });

  let target = TargetFactory.forTab(gBrowser.selectedTab);
  let toolbox = yield gDevTools.showToolbox(target, TOOL_ID);

  // init & ready events should have been fired when opening the toolbox.
  ok(events.init, "init event fired");
  ok(events.ready, "ready event fired");

  ok(gDevTools.getToolDefinitionMap().has(TOOL_ID), "The tool is still registered");
  DevToolsShim.unregisterTool(TOOL_ID);
  ok(!gDevTools.getToolDefinitionMap().has(TOOL_ID), "The tool is no longer registered");

  // Wait for toolbox select event after unregistering the currently selected tool.
  yield toolbox.once("select");

  yield toolbox.destroy();
}
