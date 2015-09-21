/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that the sidebar widget auto-registers existing tabs.

const {ToolSidebar} = require("devtools/client/framework/sidebar");

const testToolURL = "data:text/xml;charset=utf8,<?xml version='1.0'?>" +
                "<?xml-stylesheet href='chrome://browser/skin/devtools/common.css' type='text/css'?>" +
                "<window xmlns='http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul'>" +
                "<hbox flex='1'><description flex='1'>test tool</description>" +
                "<splitter class='devtools-side-splitter'/>" +
                "<tabbox flex='1' id='sidebar' class='devtools-sidebar-tabs'>" +
                "<tabs><tab id='tab1' label='tab 1'></tab><tab id='tab2' label='tab 2'></tab></tabs>" +
                "<tabpanels flex='1'><tabpanel id='tabpanel1'>tab 1</tabpanel><tabpanel id='tabpanel2'>tab 2</tabpanel></tabpanels>" +
                "</tabbox></hbox></window>";

const testToolDefinition = {
  id: "testTool",
  url: testToolURL,
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

add_task(function*() {
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
  gDevTools.unregisterTool(testToolDefinition.id);
  gBrowser.removeCurrentTab();
});
