/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(5);

async function performChecks(tab) {
  let toolbox;
  const toolIds = await getSupportedToolIds(tab);
  for (const toolId of toolIds) {
    info("About to open " + toolId);
    toolbox = await gDevTools.showToolboxForTab(tab, { toolId });
    ok(toolbox, "toolbox exists for " + toolId);
    is(toolbox.currentToolId, toolId, "currentToolId should be " + toolId);

    const panel = toolbox.getCurrentPanel();
    ok(panel, toolId + " panel has been registered in the toolbox");
  }

  await toolbox.destroy();
}

function test() {
  (async function () {
    toggleAllTools(true);
    const tab = await addTab("about:blank");
    await performChecks(tab);
    gBrowser.removeCurrentTab();
    toggleAllTools(false);
    finish();
  })();
}
