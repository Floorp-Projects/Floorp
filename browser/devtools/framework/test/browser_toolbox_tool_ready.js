/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  addTab("about:blank").then(function(tab) {
    let target = TargetFactory.forTab(tab);
    target.makeRemote().then(performChecks.bind(null, target));
  }).then(null, console.error);

  function performChecks(target) {
    let toolIds = gDevTools.getToolDefinitionArray()
                    .filter(def => def.isTargetSupported(target))
                    .map(def => def.id);

    let open = function(index) {
      let toolId = toolIds[index];

      info("About to open " + index + "/" + toolId);
      gDevTools.showToolbox(target, toolId).then(function(toolbox) {
        ok(toolbox, "toolbox exists for " + toolId);
        is(toolbox.currentToolId, toolId, "currentToolId should be " + toolId);

        let panel = toolbox.getCurrentPanel();
        ok(panel.isReady, toolId + " panel should be ready");

        let nextIndex = index + 1;
        if (nextIndex >= toolIds.length) {
          toolbox.destroy().then(function() {
            gBrowser.removeCurrentTab();
            finish();
          });
        }
        else {
          open(nextIndex);
        }
      }, console.error);
    };

    open(0);
  }
}
