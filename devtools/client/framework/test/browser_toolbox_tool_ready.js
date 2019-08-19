/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(5);

function performChecks(target) {
  return (async function() {
    const toolIds = gDevTools
      .getToolDefinitionArray()
      .filter(def => def.isTargetSupported(target))
      .map(def => def.id);

    let toolbox;
    for (let index = 0; index < toolIds.length; index++) {
      const toolId = toolIds[index];

      info("About to open " + index + "/" + toolId);
      toolbox = await gDevTools.showToolbox(target, toolId);
      ok(toolbox, "toolbox exists for " + toolId);
      is(toolbox.currentToolId, toolId, "currentToolId should be " + toolId);

      const panel = toolbox.getCurrentPanel();
      ok(panel.isReady, toolId + " panel should be ready");
    }

    await toolbox.destroy();
  })();
}

function test() {
  (async function() {
    toggleAllTools(true);
    const tab = await addTab("about:blank");
    const target = await TargetFactory.forTab(tab);
    await performChecks(target);
    gBrowser.removeCurrentTab();
    toggleAllTools(false);
    finish();
  })();
}
