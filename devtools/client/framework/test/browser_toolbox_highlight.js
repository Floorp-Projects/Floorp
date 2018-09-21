/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var {Toolbox} = require("devtools/client/framework/toolbox");

var toolbox = null;

function test() {
  (async function() {
    const URL = "data:text/plain;charset=UTF-8,Nothing to see here, move along";

    const TOOL_ID_1 = "jsdebugger";
    const TOOL_ID_2 = "webconsole";
    await addTab(URL);

    const target = TargetFactory.forTab(gBrowser.selectedTab);
    toolbox = await gDevTools.showToolbox(target, TOOL_ID_1, Toolbox.HostType.BOTTOM);

    // select tool 2
    await toolbox.selectTool(TOOL_ID_2);
    // and highlight the first one
    await highlightTab(TOOL_ID_1);
    // to see if it has the proper class.
    await checkHighlighted(TOOL_ID_1);
    // Now switch back to first tool
    await toolbox.selectTool(TOOL_ID_1);
    // to check again. But there is no easy way to test if
    // it is showing orange or not.
    await checkNoHighlightWhenSelected(TOOL_ID_1);
    // Switch to tool 2 again
    await toolbox.selectTool(TOOL_ID_2);
    // and check again.
    await checkHighlighted(TOOL_ID_1);
    // Highlight another tool
    await highlightTab(TOOL_ID_2);
    // Check that both tools are highlighted.
    await checkHighlighted(TOOL_ID_1);
    // Check second tool being both highlighted and selected.
    await checkNoHighlightWhenSelected(TOOL_ID_2);
    // Select tool 1
    await toolbox.selectTool(TOOL_ID_1);
    // Check second tool is still highlighted
    await checkHighlighted(TOOL_ID_2);
    // Unhighlight the second tool
    await unhighlightTab(TOOL_ID_2);
    // to see the classes gone.
    await checkNoHighlight(TOOL_ID_2);
    // Now unhighlight the tool
    await unhighlightTab(TOOL_ID_1);
    // to see the classes gone.
    await checkNoHighlight(TOOL_ID_1);

    // Now close the toolbox and exit.
    executeSoon(() => {
      toolbox.destroy().then(() => {
        toolbox = null;
        gBrowser.removeCurrentTab();
        finish();
      });
    });
  })()
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
  const tab = toolbox.doc.getElementById("toolbox-tab-" + toolId);
  ok(tab.classList.contains("highlighted"),
     `The highlighted class is present in ${toolId}.`);
  ok(!tab.classList.contains("selected"),
     `The tab is not selected in ${toolId}`);
}

function checkNoHighlightWhenSelected(toolId) {
  const tab = toolbox.doc.getElementById("toolbox-tab-" + toolId);
  ok(tab.classList.contains("highlighted"),
     `The highlighted class is present in ${toolId}`);
  ok(tab.classList.contains("selected"),
     `And the tab is selected, so the orange glow will not be present. in ${toolId}`);
}

function checkNoHighlight(toolId) {
  const tab = toolbox.doc.getElementById("toolbox-tab-" + toolId);
  ok(!tab.classList.contains("highlighted"),
     `The highlighted class is not present in ${toolId}`);
}
