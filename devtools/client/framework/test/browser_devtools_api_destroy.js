/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests devtools API

function test() {
  addTab("about:blank").then(runTests);
}

function runTests(aTab) {
  let toolDefinition = {
    id: "testTool",
    visibilityswitch: "devtools.testTool.enabled",
    isTargetSupported: () => true,
    url: "about:blank",
    label: "someLabel",
    build: function (iframeWindow, toolbox) {
      let deferred = promise.defer();
      executeSoon(() => {
        deferred.resolve({
          target: toolbox.target,
          toolbox: toolbox,
          isReady: true,
          destroy: function () {},
        });
      });
      return deferred.promise;
    },
  };

  gDevTools.registerTool(toolDefinition);

  let collectedEvents = [];

  let target = TargetFactory.forTab(aTab);
  gDevTools.showToolbox(target, toolDefinition.id).then(function (toolbox) {
    let panel = toolbox.getPanel(toolDefinition.id);
    ok(panel, "Tool open");

    gDevTools.once("toolbox-destroy", (event, toolbox, iframe) => {
      collectedEvents.push(event);
    });

    gDevTools.once(toolDefinition.id + "-destroy", (event, toolbox, iframe) => {
      collectedEvents.push("gDevTools-" + event);
    });

    toolbox.once("destroy", (event) => {
      collectedEvents.push(event);
    });

    toolbox.once(toolDefinition.id + "-destroy", (event) => {
      collectedEvents.push("toolbox-" + event);
    });

    toolbox.destroy().then(function () {
      is(collectedEvents.join(":"),
        "toolbox-destroy:destroy:gDevTools-testTool-destroy:toolbox-testTool-destroy",
        "Found the right amount of collected events.");

      gDevTools.unregisterTool(toolDefinition.id);
      gBrowser.removeCurrentTab();

      executeSoon(function () {
        finish();
      });
    });
  });
}
