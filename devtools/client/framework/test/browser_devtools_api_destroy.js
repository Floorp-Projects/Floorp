/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests devtools API

function test() {
  addTab("about:blank").then(runTests);
}

async function runTests(aTab) {
  const toolDefinition = {
    id: "testTool",
    visibilityswitch: "devtools.testTool.enabled",
    isToolSupported: () => true,
    url: "about:blank",
    label: "someLabel",
    build(iframeWindow, toolbox) {
      return new Promise(resolve => {
        executeSoon(() => {
          resolve({
            target: toolbox.target,
            toolbox,
            isReady: true,
            destroy() {},
          });
        });
      });
    },
  };

  gDevTools.registerTool(toolDefinition);

  const collectedEvents = [];

  gDevTools
    .showToolboxForTab(aTab, { toolId: toolDefinition.id })
    .then(function(toolbox) {
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
        is(
          collectedEvents.join(":"),
          "toolbox-destroy:destroy:gDevTools-testTool-destroy:toolbox-testTool-destroy",
          "Found the right amount of collected events."
        );

        gDevTools.unregisterTool(toolDefinition.id);
        gBrowser.removeCurrentTab();

        executeSoon(function() {
          finish();
        });
      });
    });
}
