/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  const {ToolSidebar} = require("devtools/client/framework/sidebar");

  const tab1URL = "data:text/html;charset=utf8,<title>1</title><p>1</p>";
  const tab2URL = "data:text/html;charset=utf8,<title>2</title><p>2</p>";
  const tab3URL = "data:text/html;charset=utf8,<title>3</title><p>3</p>";

  let tab1Selected = false;
  const registeredTabs = {};
  const readyTabs = {};

  const toolDefinition = {
    id: "fakeTool4242",
    visibilityswitch: "devtools.fakeTool4242.enabled",
    url: CHROME_URL_ROOT + "browser_toolbox_sidebar_toolURL.xul",
    label: "FAKE TOOL!!!",
    isTargetSupported: () => true,
    build: function(iframeWindow, toolbox) {
      const deferred = defer();
      executeSoon(() => {
        deferred.resolve({
          target: toolbox.target,
          toolbox: toolbox,
          isReady: true,
          destroy: function() {},
          panelDoc: iframeWindow.document,
        });
      });
      return deferred.promise;
    },
  };

  gDevTools.registerTool(toolDefinition);

  addTab("about:blank").then(function(aTab) {
    const target = TargetFactory.forTab(aTab);
    gDevTools.showToolbox(target, toolDefinition.id).then(function(toolbox) {
      const panel = toolbox.getPanel(toolDefinition.id);
      panel.toolbox = toolbox;
      ok(true, "Tool open");

      const tabbox = panel.panelDoc.getElementById("sidebar");
      panel.sidebar = new ToolSidebar(tabbox, panel, "testbug865688", true);

      panel.sidebar.on("new-tab-registered", function(id) {
        registeredTabs[id] = true;
      });

      panel.sidebar.once("tab1-ready", function() {
        info("tab1-ready");
        readyTabs.tab1 = true;
        allTabsReady(panel);
      });

      panel.sidebar.once("tab2-ready", function() {
        info("tab2-ready");
        readyTabs.tab2 = true;
        allTabsReady(panel);
      });

      panel.sidebar.once("tab3-ready", function() {
        info("tab3-ready");
        readyTabs.tab3 = true;
        allTabsReady(panel);
      });

      panel.sidebar.once("tab1-selected", function() {
        info("tab1-selected");
        tab1Selected = true;
        allTabsReady(panel);
      });

      panel.sidebar.addTab("tab1", tab1URL, {selected: true});
      panel.sidebar.addTab("tab2", tab2URL);
      panel.sidebar.addTab("tab3", tab3URL);

      panel.sidebar.show();
    }).catch(console.error);
  });

  function allTabsReady(panel) {
    if (!tab1Selected || !readyTabs.tab1 || !readyTabs.tab2 || !readyTabs.tab3) {
      return;
    }

    ok(registeredTabs.tab1, "tab1 registered");
    ok(registeredTabs.tab2, "tab2 registered");
    ok(registeredTabs.tab3, "tab3 registered");
    ok(readyTabs.tab1, "tab1 ready");
    ok(readyTabs.tab2, "tab2 ready");
    ok(readyTabs.tab3, "tab3 ready");

    const tabs = panel.sidebar._tabbox.querySelectorAll("tab");
    const panels = panel.sidebar._tabbox.querySelectorAll("tabpanel");
    let label = 1;
    for (const tab of tabs) {
      is(tab.getAttribute("label"), label++, "Tab has the right title");
    }

    is(label, 4, "Found the right amount of tabs.");
    is(panel.sidebar._tabbox.selectedPanel, panels[0], "First tab is selected");
    is(panel.sidebar.getCurrentTabID(), "tab1", "getCurrentTabID() is correct");

    panel.sidebar.once("tab1-unselected", function() {
      ok(true, "received 'unselected' event");
      panel.sidebar.once("tab2-selected", function() {
        ok(true, "received 'selected' event");
        tabs[1].focus();
        is(panel.sidebar._panelDoc.activeElement, tabs[1],
          "Focus is set to second tab");
        panel.sidebar.hide();
        isnot(panel.sidebar._panelDoc.activeElement, tabs[1],
          "Focus is reset for sidebar");
        is(panel.sidebar._tabbox.getAttribute("hidden"), "true", "Sidebar hidden");
        is(panel.sidebar.getWindowForTab("tab1").location.href, tab1URL, "Window is accessible");
        testRemoval(panel);
      });
    });

    panel.sidebar.select("tab2");
  }

  function testRemoval(panel) {
    panel.sidebar.once("tab-unregistered", function(id) {
      info("tab-unregistered");
      registeredTabs[id] = false;

      is(id, "tab3", "The right tab must be removed");

      const tabs = panel.sidebar._tabbox.querySelectorAll("tab");
      const panels = panel.sidebar._tabbox.querySelectorAll("tabpanel");

      is(tabs.length, 2, "There is the right number of tabs");
      is(panels.length, 2, "There is the right number of panels");

      testWidth(panel);
    });

    panel.sidebar.removeTab("tab3");
  }

  function testWidth(panel) {
    const tabbox = panel.panelDoc.getElementById("sidebar");
    tabbox.width = 420;
    panel.sidebar.destroy().then(function() {
      tabbox.width = 0;
      panel.sidebar = new ToolSidebar(tabbox, panel, "testbug865688", true);
      panel.sidebar.show();
      is(panel.panelDoc.getElementById("sidebar").width, 420, "Width restored");

      finishUp(panel);
    });
  }

  function finishUp(panel) {
    panel.sidebar.destroy();
    panel.toolbox.destroy().then(function() {
      gDevTools.unregisterTool(toolDefinition.id);

      gBrowser.removeCurrentTab();

      executeSoon(function() {
        finish();
      });
    });
  }
}
