/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  const { ToolSidebar } = require("devtools/client/framework/sidebar");

  const tab1URL = "data:text/html;charset=utf8,<title>1</title><p>1</p>";

  const collectedEvents = [];

  const toolDefinition = {
    id: "testTool1072208",
    visibilityswitch: "devtools.testTool1072208.enabled",
    url: CHROME_URL_ROOT + "browser_toolbox_sidebar_events.xul",
    label: "Test tool",
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
      ok(true, "Tool open");

      panel.once("sidebar-created", function() {
        collectedEvents.push("sidebar-created");
      });

      panel.once("sidebar-destroyed", function() {
        collectedEvents.push("sidebar-destroyed");
      });

      const tabbox = panel.panelDoc.getElementById("sidebar");
      panel.sidebar = new ToolSidebar(tabbox, panel, "testbug1072208", true);

      panel.sidebar.once("show", function() {
        collectedEvents.push("show");
      });

      panel.sidebar.once("hide", function() {
        collectedEvents.push("hide");
      });

      panel.sidebar.once("tab1-selected", () => finishUp(panel));
      panel.sidebar.addTab("tab1", tab1URL, {selected: true});
      panel.sidebar.show();
    }).catch(console.error);
  });

  function finishUp(panel) {
    panel.sidebar.hide();
    panel.sidebar.destroy();

    const events = collectedEvents.join(":");
    is(events, "sidebar-created:show:hide:sidebar-destroyed",
      "Found the right amount of collected events.");

    panel.toolbox.destroy().then(function() {
      gDevTools.unregisterTool(toolDefinition.id);
      gBrowser.removeCurrentTab();

      executeSoon(function() {
        finish();
      });
    });
  }
}

