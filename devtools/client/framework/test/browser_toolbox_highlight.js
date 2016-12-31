/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var {Toolbox} = require("devtools/client/framework/toolbox");

var toolbox = null;

function test() {
  Task.spawn(function* () {
    const URL = "data:text/plain;charset=UTF-8,Nothing to see here, move along";

    const TOOL_ID_1 = "jsdebugger";
    const TOOL_ID_2 = "webconsole";
    yield addTab(URL);

    const target = TargetFactory.forTab(gBrowser.selectedTab);
    toolbox = yield gDevTools.showToolbox(target, TOOL_ID_1, Toolbox.HostType.BOTTOM)

    // select tool 2
    yield toolbox.selectTool(TOOL_ID_2)
    // and highlight the first one
    yield highlightTab(TOOL_ID_1);
    // to see if it has the proper class.
    yield checkHighlighted(TOOL_ID_1);
    // Now switch back to first tool
    yield toolbox.selectTool(TOOL_ID_1);
    // to check again. But there is no easy way to test if
    // it is showing orange or not.
    yield checkNoHighlightWhenSelected(TOOL_ID_1);
    // Switch to tool 2 again
    yield toolbox.selectTool(TOOL_ID_2);
    // and check again.
    yield checkHighlighted(TOOL_ID_1);
    // Now unhighlight the tool
    yield unhighlightTab(TOOL_ID_1);
    // to see the classes gone.
    yield checkNoHighlight(TOOL_ID_1);

    // Now close the toolbox and exit.
    executeSoon(() => {
      toolbox.destroy().then(() => {
        toolbox = null;
        gBrowser.removeCurrentTab();
        finish();
      });
    });
  })
  .catch(error => {
    ok(false, "There was an error running the test.");
  });
}

function highlightTab(toolId) {
  info(`Highlighting tool ${toolId}'s tab.`);
  return toolbox.highlightTool(toolId);
}

function unhighlightTab(toolId) {
  info(`Unhighlighting tool ${toolId}'s tab.`);
  return toolbox.unhighlightTool(toolId);
}

function checkHighlighted(toolId) {
  let tab = toolbox.doc.getElementById("toolbox-tab-" + toolId);
  ok(tab.classList.contains("highlighted"),
     `The highlighted class is present in ${toolId}.`);
  ok(!tab.classList.contains("selected"),
     `The tab is not selected in ${toolId}`);
}

function checkNoHighlightWhenSelected(toolId) {
  let tab = toolbox.doc.getElementById("toolbox-tab-" + toolId);
  ok(tab.classList.contains("highlighted"),
     `The highlighted class is present in ${toolId}`);
  ok(tab.classList.contains("selected"),
     `And the tab is selected, so the orange glow will not be present. in ${toolId}`);
}

function checkNoHighlight(toolId) {
  let tab = toolbox.doc.getElementById("toolbox-tab-" + toolId);
  ok(!tab.classList.contains("highlighted"),
     `The highlighted class is not present in ${toolId}`);
}
