/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the sidebar widget correctly displays the "all tabs..." button
// when the tabs overflow.

const {ToolSidebar} = require("devtools/client/framework/sidebar");

const testToolDefinition = {
  id: "testTool",
  url: CHROME_URL_ROOT + "browser_toolbox_sidebar_tool.xul",
  label: "Test Tool",
  isTargetSupported: () => true,
  build: (iframeWindow, toolbox) => {
    return {
      target: toolbox.target,
      toolbox: toolbox,
      isReady: true,
      destroy: () => {},
      panelDoc: iframeWindow.document,
    };
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
  let sidebar = new ToolSidebar(tabbox, toolPanel, "bug1101569", {
    showAllTabsMenu: true
  });

  let allTabsMenu = toolPanel.panelDoc.querySelector(".devtools-sidebar-alltabs");
  ok(allTabsMenu, "The all-tabs menu is available");
  is(allTabsMenu.getAttribute("hidden"), "true", "The menu is hidden for now");

  info("Adding 10 tabs to the sidebar widget");
  for (let nb = 0; nb < 10; nb++) {
    let url = `data:text/html;charset=utf8,<title>tab ${nb}</title><p>Test tab ${nb}</p>`;
    sidebar.addTab("tab" + nb, url, nb === 0);
  }

  info("Fake an overflow event so that the all-tabs menu is visible");
  sidebar._onTabBoxOverflow();
  ok(!allTabsMenu.hasAttribute("hidden"), "The all-tabs menu is now shown");

  info("Select each tab, one by one");
  for (let nb = 0; nb < 10; nb++) {
    let id = "tab" + nb;

    info("Found tab item nb " + nb);
    let item = allTabsMenu.querySelector("#sidebar-alltabs-item-" + id);

    info("Click on the tab");
    EventUtils.sendMouseEvent({type: "click"}, item, toolPanel.panelDoc.defaultView);

    is(tabbox.selectedTab.id, "sidebar-tab-" + id,
      "The selected tab is now nb " + nb);
  }

  info("Fake an underflow event so that the all-tabs menu gets hidden");
  sidebar._onTabBoxUnderflow();
  is(allTabsMenu.getAttribute("hidden"), "true", "The all-tabs menu is hidden");

  yield sidebar.destroy();
  gDevTools.unregisterTool(testToolDefinition.id);
  gBrowser.removeCurrentTab();
});
