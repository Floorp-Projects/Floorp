/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the grid highlighter is hidden when the highlighted grid container is
// removed from the page.

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
  let {inspector, view, testActor} = yield openRuleView();
  let highlighters = view.highlighters;

  yield selectNode("#grid", inspector);
  let container = getRuleViewProperty(view, "#grid", "display").valueSpan;
  let gridToggle = container.querySelector(".ruleview-grid");

  info("Toggling ON the CSS grid highlighter from the rule-view.");
  let onHighlighterShown = highlighters.once("grid-highlighter-shown");
  gridToggle.click();
  yield onHighlighterShown;
  ok(highlighters.gridHighlighterShown, "CSS grid highlighter is shown.");

  let onHighlighterHidden = highlighters.once("grid-highlighter-hidden");
  info("Remove the #grid container in the content page");
  testActor.eval(`
    content.document.querySelector("#grid").remove();
  `);
  yield onHighlighterHidden;
  ok(!highlighters.gridHighlighterShown, "CSS grid highlighter is hidden.");
});
