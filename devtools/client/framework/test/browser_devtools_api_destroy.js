/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests devtools API

function test() {
  addTab("about:blank").then(runTests);
}

function runTests(aTab) {
  const toolDefinition = {
    id: "testTool",
    visibilityswitch: "devtools.testTool.enabled",
    isTargetSupported: () => true,
    url: "about:blank",
    label: "someLabel",
    build: function(iframeWindow, toolbox) {
      const deferred = defer();
      executeSoon(() => {
        deferred.resolve({
          target: toolbox.target,
          toolbox: toolbox,
          isReady: true,
          destroy: function() {},
        });
      });
      return deferred.promise;
    },
  };

  gDevTools.registerTool(toolDefinition);

  const collectedEvents = [];

  const target = TargetFactory.forTab(aTab);
  gDevTools.showToolbox(target, toolDefinition.id).then(function(toolbox) {
    const panel = toolbox.getPanel(toolDefinition.id);
    ok(panel, "Tool open");

    gDevTools.once("toolbox-destroy", (toolbox, iframe) => {
      collectedEvents.push("toolbox-destroy");
    });

    gDevTools.once(toolDefinition.id + "-destroy", (toolbox, iframe) => {
      collectedEvents.push("gDevTools-" + toolDefinition.id + "-destroy");
    });

    toolbox.once("destroy", () => {
      collectedEvents.push("destroy");
    });

    toolbox.once(toolDefinition.id + "-destroy", () => {
      collectedEvents.push("toolbox-" + toolDefinition.id + "-destroy");
    });

    toolbox.destroy().then(function() {
      is(collectedEvents.join(":"),
        "toolbox-destroy:destroy:gDevTools-testTool-destroy:toolbox-testTool-destroy",
        "Found the right amount of collected events.");

      gDevTools.unregisterTool(toolDefinition.id);
      gBrowser.removeCurrentTab();

      executeSoon(function() {
        finish();
      });
    });
  });
}
