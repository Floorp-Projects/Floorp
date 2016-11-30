/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test toggling the grid highlighter in the rule view and modifying the 'display: grid'
// declaration.

const TEST_URI = `
  <style type='text/css'>
    #grid {
      display: grid;
    }
  </style>
  <div id="grid">
    <div id="cell1">cell1</div>
    <div id="cell2">cell2</div>
  </div>
`;

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  let highlighters = view.highlighters;

  yield selectNode("#grid", inspector);
  let container = getRuleViewProperty(view, "#grid", "display").valueSpan;
  let gridToggle = container.querySelector(".ruleview-grid");

  info("Toggling ON the CSS grid highlighter from the rule-view.");
  let onHighlighterShown = highlighters.once("grid-highlighter-shown");
  gridToggle.click();
  yield onHighlighterShown;

  info("Edit the 'grid' property value to 'block'.");
  let editor = yield focusEditableField(view, container);
  let onHighlighterHidden = highlighters.once("grid-highlighter-hidden");
  let onDone = view.once("ruleview-changed");
  editor.input.value = "block;";
  EventUtils.synthesizeKey("VK_RETURN", {}, view.styleWindow);
  yield onHighlighterHidden;
  yield onDone;

  info("Check the grid highlighter and grid toggle button are hidden.");
  gridToggle = container.querySelector(".ruleview-grid");
  ok(!gridToggle, "Grid highlighter toggle is not visible.");
  ok(!highlighters.gridHighlighterShown, "No CSS grid highlighter is shown.");
});
