/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the grid highlighter is re-displayed after reloading a page.

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

  info("Check that the grid highlighter can be displayed");
  let {inspector, view} = yield openRuleView();
  let {highlighters} = view;

  yield selectNode("#grid", inspector);
  let container = getRuleViewProperty(view, "#grid", "display").valueSpan;
  let gridToggle = container.querySelector(".ruleview-grid");

  info("Toggling ON the CSS grid highlighter from the rule-view.");
  let onHighlighterShown = highlighters.once("grid-highlighter-shown");
  gridToggle.click();
  yield onHighlighterShown;

  ok(highlighters.gridHighlighterShown, "CSS grid highlighter is shown.");

  info("Reload the page, expect the highlighter to be displayed once again");
  onHighlighterShown = highlighters.once("grid-highlighter-shown");
  yield refreshTab(gBrowser.selectedTab);
  yield onHighlighterShown;

  info("Check that the grid highlighter can be displayed after reloading the page");
  ok(highlighters.gridHighlighterShown, "CSS grid highlighter is shown.");
});
