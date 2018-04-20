/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(5);

function performChecks(target) {
  return (async function() {
    let toolIds = gDevTools.getToolDefinitionArray()
                           .filter(def => def.isTargetSupported(target))
                           .map(def => def.id);

    let toolbox;
    for (let index = 0; index < toolIds.length; index++) {
      let toolId = toolIds[index];

      info("About to open " + index + "/" + toolId);
      toolbox = await gDevTools.showToolbox(target, toolId);
      ok(toolbox, "toolbox exists for " + toolId);
      is(toolbox.currentToolId, toolId, "currentToolId should be " + toolId);

      let panel = toolbox.getCurrentPanel();
      ok(panel.isReady, toolId + " panel should be ready");
    }

    await toolbox.destroy();
  })();
}

function test() {
  (async function() {
    toggleAllTools(true);
    let tab = await addTab("about:blank");
    let target = TargetFactory.forTab(tab);
    await target.makeRemote();
    await performChecks(target);
    gBrowser.removeCurrentTab();
    toggleAllTools(false);
    finish();
  })();
}
