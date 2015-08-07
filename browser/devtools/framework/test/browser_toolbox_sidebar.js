/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  const Cu = Components.utils;
  let {ToolSidebar} = require("devtools/framework/sidebar");

  const toolURL = "data:text/xml;charset=utf8,<?xml version='1.0'?>" +
                  "<?xml-stylesheet href='chrome://browser/skin/devtools/common.css' type='text/css'?>" +
                  "<window xmlns='http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul'>" +
                  "<hbox flex='1'><description flex='1'>foo</description><splitter class='devtools-side-splitter'/>" +
                  "<tabbox flex='1' id='sidebar' class='devtools-sidebar-tabs'><tabs/><tabpanels flex='1'/></tabbox>" +
                  "</hbox>" +
                  "</window>";

  const tab1URL = "data:text/html;charset=utf8,<title>1</title><p>1</p>";
  const tab2URL = "data:text/html;charset=utf8,<title>2</title><p>2</p>";
  const tab3URL = "data:text/html;charset=utf8,<title>3</title><p>3</p>";

  let panelDoc;
  let tab1Selected = false;
  let registeredTabs = {};
  let readyTabs = {};

  let toolDefinition = {
    id: "fakeTool4242",
    visibilityswitch: "devtools.fakeTool4242.enabled",
    url: toolURL,
    label: "FAKE TOOL!!!",
    isTargetSupported: () => true,
    build: function(iframeWindow, toolbox) {
      let deferred = promise.defer();
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
    let target = TargetFactory.forTab(aTab);
    gDevTools.showToolbox(target, toolDefinition.id).then(function(toolbox) {
      let panel = toolbox.getPanel(toolDefinition.id);
      ok(true, "Tool open");

      let tabbox = panel.panelDoc.getElementById("sidebar");
      panel.sidebar = new ToolSidebar(tabbox, panel, "testbug865688", true);

      panel.sidebar.on("new-tab-registered", function(event, id) {
        registeredTabs[id] = true;
      });

      panel.sidebar.once("tab1-ready", function(event) {
        info(event);
        readyTabs.tab1 = true;
        allTabsReady(panel);
      });

      panel.sidebar.once("tab2-ready", function(event) {
        info(event);
        readyTabs.tab2 = true;
        allTabsReady(panel);
      });

      panel.sidebar.once("tab3-ready", function(event) {
        info(event);
        readyTabs.tab3 = true;
        allTabsReady(panel);
      });

      panel.sidebar.once("tab1-selected", function(event) {
        info(event);
        tab1Selected = true;
        allTabsReady(panel);
      });

      panel.sidebar.addTab("tab1", tab1URL, true);
      panel.sidebar.addTab("tab2", tab2URL);
      panel.sidebar.addTab("tab3", tab3URL);

      panel.sidebar.show();
    }).then(null, console.error);
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

    let tabs = panel.sidebar._tabbox.querySelectorAll("tab");
    let panels = panel.sidebar._tabbox.querySelectorAll("tabpanel");
    let label = 1;
    for (let tab of tabs) {
      is(tab.getAttribute("label"), label++, "Tab has the right title");
    }

    is(label, 4, "Found the right amount of tabs.");
    is(panel.sidebar._tabbox.selectedPanel, panels[0], "First tab is selected");
    is(panel.sidebar.getCurrentTabID(), "tab1", "getCurrentTabID() is correct");

    panel.sidebar.once("tab1-unselected", function() {
      ok(true, "received 'unselected' event");
      panel.sidebar.once("tab2-selected", function() {
        ok(true, "received 'selected' event");
        panel.sidebar.hide();
        is(panel.sidebar._tabbox.getAttribute("hidden"), "true", "Sidebar hidden");
        is(panel.sidebar.getWindowForTab("tab1").location.href, tab1URL, "Window is accessible");
        testRemoval(panel);
      });
    });

    panel.sidebar.select("tab2");
  }

  function testRemoval(panel) {
    panel.sidebar.once("tab-unregistered", function(event, id) {
      info(event);
      registeredTabs[id] = false;

      is(id, "tab3", "The right tab must be removed");

      let tabs = panel.sidebar._tabbox.querySelectorAll("tab");
      let panels = panel.sidebar._tabbox.querySelectorAll("tabpanel");

      is(tabs.length, 2, "There is the right number of tabs");
      is(panels.length, 2, "There is the right number of panels");

      testWidth(panel);
    });

    panel.sidebar.removeTab("tab3");
  }

  function testWidth(panel) {
    let tabbox = panel.panelDoc.getElementById("sidebar");
    tabbox.width = 420;
    panel.sidebar.destroy().then(function() {
      tabbox.width = 0;
      panel.sidebar = new ToolSidebar(tabbox, panel, "testbug865688", true);
      panel.sidebar.show();
      is(panel.panelDoc.getElementById("sidebar").width, 420, "Width restored")

      finishUp(panel);
    });
  }

  function finishUp(panel) {
    panel.sidebar.destroy();
    gDevTools.unregisterTool(toolDefinition.id);

    gBrowser.removeCurrentTab();

    executeSoon(function() {
      finish();
    });
  }
}
