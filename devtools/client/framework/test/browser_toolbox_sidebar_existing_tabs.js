/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the sidebar widget auto-registers existing tabs.

const {ToolSidebar} = require("devtools/client/framework/sidebar");

const testToolDefinition = {
  id: "testTool",
  url: CHROME_URL_ROOT + "browser_toolbox_sidebar_existing_tabs.xul",
  label: "Test Tool",
  isTargetSupported: () => true,
  build: (iframeWindow, toolbox) => {
    return promise.resolve({
      target: toolbox.target,
      toolbox: toolbox,
      isReady: true,
      destroy: () => {},
      panelDoc: iframeWindow.document,
    });
  }
};

add_task(function* () {
  let tab = yield addTab("about:blank");

  let target = TargetFactory.forTab(tab);

  gDevTools.registerTool(testToolDefinition);
  let toolbox = yield gDevTools.showToolbox(target, testToolDefinition.id);

  let toolPanel = toolbox.getPanel(testToolDefinition.id);
  let tabbox = toolPanel.panelDoc.getElementById("sidebar");

  info("Creating the sidebar widget");
  let sidebar = new ToolSidebar(tabbox, toolPanel, "bug1101569");

  info("Checking that existing tabs have been registered");
  ok(sidebar.getTab("tab1"), "Existing tab 1 was found");
  ok(sidebar.getTab("tab2"), "Existing tab 2 was found");
  ok(sidebar.getTabPanel("tabpanel1"), "Existing tabpanel 1 was found");
  ok(sidebar.getTabPanel("tabpanel2"), "Existing tabpanel 2 was found");

  info("Checking that the sidebar API works with existing tabs");

  sidebar.select("tab2");
  is(tabbox.selectedTab, tabbox.querySelector("#tab2"),
    "Existing tabs can be selected");

  sidebar.select("tab1");
  is(tabbox.selectedTab, tabbox.querySelector("#tab1"),
    "Existing tabs can be selected");

  is(sidebar.getCurrentTabID(), "tab1", "getCurrentTabID returns the expected id");

  info("Removing a tab");
  sidebar.removeTab("tab2", "tabpanel2");
  ok(!sidebar.getTab("tab2"), "Tab 2 was removed correctly");
  ok(!sidebar.getTabPanel("tabpanel2"), "Tabpanel 2 was removed correctly");

  sidebar.destroy();
  yield toolbox.destroy();
  gDevTools.unregisterTool(testToolDefinition.id);
  gBrowser.removeCurrentTab();
});
