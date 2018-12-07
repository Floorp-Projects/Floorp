/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that changing display:grid to display:block and back on the grid
// container hides the grid highlighter.

const TEST_URI = `
  <style type='text/css'>
    #grid {
      display: grid;
    }
  </style>
  <div id="grid">
    <div class="cell1">cell1</div>
    <div class="cell2">cell2</div>
  </div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, gridInspector } = await openLayoutView();
  const { document: doc } = gridInspector;
  const { highlighters, store } = inspector;

  await selectNode("#grid", inspector);
  const gridList = doc.getElementById("grid-list");
  const checkbox = gridList.children[0].querySelector("input");

  info("Checking the initial state of the Grid Inspector.");
  is(gridList.childNodes.length, 1, "1 grid containers are listed.");
  ok(!highlighters.gridHighlighters.size, "No CSS grid highlighter is shown.");

  info("Toggling ON the CSS grid highlighter for #grid.");
  const onHighlighterShown = highlighters.once("grid-highlighter-shown");
  let onCheckboxChange = waitUntilState(store, state =>
    state.grids.length === 1 &&
    state.grids[0].highlighted && !state.grids[0].disabled);
  checkbox.click();
  await onHighlighterShown;
  await onCheckboxChange;

  is(highlighters.gridHighlighters.size, 1, "One CSS grid highlighter is shown.");

  const ruleView = selectRuleView(inspector);

  info("Check that toggling #grid from display:grid to display:block " +
       "hides its grid highlighter.");
  let rule = getRuleViewRuleEditor(ruleView, 1).rule;
  let displayProp = rule.textProps[0];
  const onDisplayChange = waitUntilState(store, state => state.grids.length === 0);
  await setRuleViewProperty(ruleView, displayProp, "block");
  await onDisplayChange;

  ok(!highlighters.gridHighlighters.size, "No CSS grid highlighter is shown.");

  info("Check that toggling #grid from display:block to display:grid " +
       "doesn't retrigger its grid highlighter.");
  rule = getRuleViewRuleEditor(ruleView, 1).rule;
  displayProp = rule.textProps[0];
  const onBoxModelUpdated = inspector.once("boxmodel-view-updated");
  await setRuleViewProperty(ruleView, displayProp, "grid");
  await onBoxModelUpdated;

  ok(!highlighters.gridHighlighters.size, "No CSS grid highlighter is shown.");

  info("Toggling OFF the CSS grid highlighter for #grid1.");
  onCheckboxChange = waitUntilState(store, state =>
    state.grids.length === 1 &&
    !state.grids[0].highlighted && !state.grids[0].disabled);
  checkbox.click();

  await onCheckboxChange;

  info("Check that the CSS grid highlighter is not shown and the saved grid state.");
  ok(!highlighters.gridHighlighters.size, "No CSS grid highlighter is shown.");
  ok(!highlighters.state.grids.size, "No grids in the saved state");
});
