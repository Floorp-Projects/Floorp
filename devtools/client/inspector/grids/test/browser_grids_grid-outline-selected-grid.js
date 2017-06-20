/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the grid outline is shown when a grid container is selected.

const TEST_URI = `
  <style type='text/css'>
    #grid {
      display: grid;
    }
  </style>
  <div id="grid">
    <div id="cella">Cell A</div>
    <div id="cellb">Cell B</div>
    <div id="cellc">Cell C</div>
  </div>
`;

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));

  let { inspector, gridInspector } = yield openLayoutView();
  let { document: doc } = gridInspector;
  let { highlighters, store } = inspector;

  let gridList = doc.getElementById("grid-list");
  let checkbox = gridList.children[0].querySelector("input");

  info("Checking the initial state of the Grid Inspector.");
  ok(!doc.getElementById("grid-outline-container"),
    "There should be no grid outline shown.");

  info("Toggling ON the CSS grid highlighter from the layout panel.");
  let onHighlighterShown = highlighters.once("grid-highlighter-shown");
  let onCheckboxChange = waitUntilState(store, state =>
    state.grids.length == 1 &&
    state.grids[0].highlighted);
  let onGridOutlineRendered = waitForDOM(doc, "#grid-cell-group rect", 4);
  checkbox.click();
  yield onHighlighterShown;
  yield onCheckboxChange;
  let elements = yield onGridOutlineRendered;

  info("Checking the grid outline is shown.");
  is(elements.length, 4, "Grid outline is shown.");
});
