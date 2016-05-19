/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var {Toolbox} = require("devtools/client/framework/toolbox");

var toolbox = null;

function test() {
  const URL = "data:text/plain;charset=UTF-8,Nothing to see here, move along";

  const TOOL_ID_1 = "jsdebugger";
  const TOOL_ID_2 = "webconsole";

  addTab(URL).then(() => {
    let target = TargetFactory.forTab(gBrowser.selectedTab);
    gDevTools.showToolbox(target, TOOL_ID_1, Toolbox.HostType.BOTTOM)
             .then(aToolbox => {
               toolbox = aToolbox;
                // select tool 2
               toolbox.selectTool(TOOL_ID_2)
                       // and highlight the first one
                       .then(highlightTab.bind(null, TOOL_ID_1))
                       // to see if it has the proper class.
                       .then(checkHighlighted.bind(null, TOOL_ID_1))
                       // Now switch back to first tool
                       .then(() => toolbox.selectTool(TOOL_ID_1))
                       // to check again. But there is no easy way to test if
                       // it is showing orange or not.
                       .then(checkNoHighlightWhenSelected.bind(null, TOOL_ID_1))
                       // Switch to tool 2 again
                       .then(() => toolbox.selectTool(TOOL_ID_2))
                       // and check again.
                       .then(checkHighlighted.bind(null, TOOL_ID_1))
                       // Now unhighlight the tool
                       .then(unhighlightTab.bind(null, TOOL_ID_1))
                       // to see the classes gone.
                       .then(checkNoHighlight.bind(null, TOOL_ID_1))
                       // Now close the toolbox and exit.
                       .then(() => executeSoon(() => {
                         toolbox.destroy()
                                 .then(() => {
                                   toolbox = null;
                                   gBrowser.removeCurrentTab();
                                   finish();
                                 });
                       }));
             });
  });
}

function highlightTab(toolId) {
  info("Highlighting tool " + toolId + "'s tab.");
  toolbox.highlightTool(toolId);
}

function unhighlightTab(toolId) {
  info("Unhighlighting tool " + toolId + "'s tab.");
  toolbox.unhighlightTool(toolId);
}

function checkHighlighted(toolId) {
  let tab = toolbox.doc.getElementById("toolbox-tab-" + toolId);
  ok(tab.hasAttribute("highlighted"), "The highlighted attribute is present");
  ok(!tab.hasAttribute("selected") || tab.getAttribute("selected") != "true",
     "The tab is not selected");
}

function checkNoHighlightWhenSelected(toolId) {
  let tab = toolbox.doc.getElementById("toolbox-tab-" + toolId);
  ok(tab.hasAttribute("highlighted"), "The highlighted attribute is present");
  ok(tab.hasAttribute("selected") && tab.getAttribute("selected") == "true",
     "and the tab is selected, so the orange glow will not be present.");
}

function checkNoHighlight(toolId) {
  let tab = toolbox.doc.getElementById("toolbox-tab-" + toolId);
  ok(!tab.hasAttribute("highlighted"),
     "The highlighted attribute is not present");
}
