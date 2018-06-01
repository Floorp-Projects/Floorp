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

const OTHER_URI = `
  <style type='text/css'>
    #grid {
      display: grid;
    }
  </style>
  <div id="grid">
    <div id="cell1">cell1</div>
    <div id="cell2">cell2</div>
    <div id="cell3">cell3</div>
  </div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));

  info("Check that the grid highlighter can be displayed");
  const {inspector, view} = await openRuleView();
  const {highlighters} = view;

  await selectNode("#grid", inspector);
  const container = getRuleViewProperty(view, "#grid", "display").valueSpan;
  const gridToggle = container.querySelector(".ruleview-grid");

  info("Toggling ON the CSS grid highlighter from the rule-view.");
  const onHighlighterShown = highlighters.once("grid-highlighter-shown");
  gridToggle.click();
  await onHighlighterShown;

  ok(highlighters.gridHighlighterShown, "CSS grid highlighter is shown.");

  info("Reload the page, expect the highlighter to be displayed once again");
  let onStateRestored = highlighters.once("grid-state-restored");
  await refreshTab();
  let { restored } = await onStateRestored;
  ok(restored, "The highlighter state was restored");

  info("Check that the grid highlighter can be displayed after reloading the page");
  ok(highlighters.gridHighlighterShown, "CSS grid highlighter is shown.");

  info("Navigate to another URL, and check that the highlighter is hidden");
  const otherUri = "data:text/html;charset=utf-8," + encodeURIComponent(OTHER_URI);
  onStateRestored = highlighters.once("grid-state-restored");
  await navigateTo(inspector, otherUri);
  ({ restored } = await onStateRestored);
  ok(!restored, "The highlighter state was not restored");
  ok(!highlighters.gridHighlighterShown, "CSS grid highlighter is hidden.");
});
