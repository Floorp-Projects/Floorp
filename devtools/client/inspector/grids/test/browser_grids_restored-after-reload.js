/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the grid highlighter is re-displayed after reloading a page and the grid
// item is highlighted.

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

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let { gridInspector, inspector } = yield openLayoutView();
  let { document: doc } = gridInspector;
  let { highlighters, store } = inspector;

  yield selectNode("#grid", inspector);
  let gridList = doc.getElementById("grid-list");
  let checkbox = gridList.children[0].querySelector("input");

  info("Toggling ON the CSS grid highlighter from the layout panel.");
  let onHighlighterShown = highlighters.once("grid-highlighter-shown");
  let onCheckboxChange = waitUntilState(store, state =>
    state.grids.length == 1 &&
    state.grids[0].highlighted);
  checkbox.click();
  yield onHighlighterShown;
  yield onCheckboxChange;

  info("Checking the CSS grid highlighter is created.");
  ok(highlighters.highlighters[HIGHLIGHTER_TYPE],
    "CSS grid highlighter is created in the highlighters overlay.");
  ok(highlighters.gridHighlighterShown, "CSS grid highlighter is shown.");

  info("Reload the page, expect the highlighter to be displayed once again and " +
    "grid is checked");
  let onStateRestored = highlighters.once("grid-state-restored");
  let onGridListRestored = waitUntilState(store, state =>
    state.grids.length == 1 &&
    state.grids[0].highlighted);
  yield refreshTab(gBrowser.selectedTab);
  let { restored } = yield onStateRestored;
  yield onGridListRestored;

  info("Check that the grid highlighter can be displayed after reloading the page");
  ok(restored, "The highlighter state was restored");
  ok(highlighters.gridHighlighterShown, "CSS grid highlighter is shown.");

  info("Navigate to another URL, and check that the highlighter is hidden and " +
    "grid is unchecked");
  let otherUri = "data:text/html;charset=utf-8," + encodeURIComponent(OTHER_URI);
  onStateRestored = highlighters.once("grid-state-restored");
  onGridListRestored = waitUntilState(store, state =>
    state.grids.length == 1 &&
    !state.grids[0].highlighted);
  yield navigateTo(inspector, otherUri);
  ({ restored } = yield onStateRestored);
  yield onGridListRestored;

  info("Check that the grid highlighter is hidden after navigating to a different page");
  ok(!restored, "The highlighter state was not restored");
  ok(!highlighters.gridHighlighterShown, "CSS grid highlighter is hidden.");
});
