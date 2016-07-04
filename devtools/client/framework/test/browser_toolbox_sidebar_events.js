/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  const Cu = Components.utils;
  const { ToolSidebar } = require("devtools/client/framework/sidebar");

  const toolURL = "data:text/xml;charset=utf8,<?xml version='1.0'?>" +
                  "<window xmlns='http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul'>" +
                  "<hbox flex='1'><description flex='1'>foo</description><splitter class='devtools-side-splitter'/>" +
                  "<tabbox flex='1' id='sidebar' class='devtools-sidebar-tabs'><tabs/><tabpanels flex='1'/></tabbox>" +
                  "</hbox>" +
                  "</window>";

  const tab1URL = "data:text/html;charset=utf8,<title>1</title><p>1</p>";

  let collectedEvents = [];

  let toolDefinition = {
    id: "testTool1072208",
    visibilityswitch: "devtools.testTool1072208.enabled",
    url: toolURL,
    label: "Test tool",
    isTargetSupported: () => true,
    build: function (iframeWindow, toolbox) {
      let deferred = defer();
      executeSoon(() => {
        deferred.resolve({
          target: toolbox.target,
          toolbox: toolbox,
          isReady: true,
          destroy: function () {},
          panelDoc: iframeWindow.document,
        });
      });
      return deferred.promise;
    },
  };

  gDevTools.registerTool(toolDefinition);

  addTab("about:blank").then(function (aTab) {
    let target = TargetFactory.forTab(aTab);
    gDevTools.showToolbox(target, toolDefinition.id).then(function (toolbox) {
      let panel = toolbox.getPanel(toolDefinition.id);
      ok(true, "Tool open");

      panel.once("sidebar-created", function (event, id) {
        collectedEvents.push(event);
      });

      panel.once("sidebar-destroyed", function (event, id) {
        collectedEvents.push(event);
      });

      let tabbox = panel.panelDoc.getElementById("sidebar");
      panel.sidebar = new ToolSidebar(tabbox, panel, "testbug1072208", true);

      panel.sidebar.once("show", function (event, id) {
        collectedEvents.push(event);
      });

      panel.sidebar.once("hide", function (event, id) {
        collectedEvents.push(event);
      });

      panel.sidebar.once("tab1-selected", () => finishUp(panel));
      panel.sidebar.addTab("tab1", tab1URL, {selected: true});
      panel.sidebar.show();
    }).then(null, console.error);
  });

  function finishUp(panel) {
    panel.sidebar.hide();
    panel.sidebar.destroy();

    let events = collectedEvents.join(":");
    is(events, "sidebar-created:show:hide:sidebar-destroyed",
      "Found the right amount of collected events.");

    gDevTools.unregisterTool(toolDefinition.id);
    gBrowser.removeCurrentTab();

    executeSoon(function () {
      finish();
    });
  }
}

