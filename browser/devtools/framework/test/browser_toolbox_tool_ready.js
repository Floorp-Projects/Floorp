/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

///////////////////
//
// Whitelisting this test.
// As part of bug 1077403, the leaking uncaught rejection should be fixed. 
//
thisTestLeaksUncaughtRejectionsAndShouldBeFixed("Error: Shader Editor is still waiting for a WebGL context to be created.");

function performChecks(target) {
  return Task.spawn(function() {
    let toolIds = gDevTools.getToolDefinitionArray()
                           .filter(def => def.isTargetSupported(target))
                           .map(def => def.id);

    let toolbox;
    for (let index = 0; index < toolIds.length; index++) {
      let toolId = toolIds[index];

      info("About to open " + index + "/" + toolId);
      toolbox = yield gDevTools.showToolbox(target, toolId);
      ok(toolbox, "toolbox exists for " + toolId);
      is(toolbox.currentToolId, toolId, "currentToolId should be " + toolId);

      let panel = toolbox.getCurrentPanel();
      ok(panel.isReady, toolId + " panel should be ready");
    }

    yield toolbox.destroy();
  });
}

function test() {
  Task.spawn(function() {
    toggleAllTools(true);
    let tab = yield addTab("about:blank");
    let target = TargetFactory.forTab(tab);
    yield target.makeRemote();
    yield performChecks(target);
    gBrowser.removeCurrentTab();
    toggleAllTools(false);
    finish();
  }, console.error);
}
