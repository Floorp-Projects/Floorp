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

  yield selectNode("#grid", inspector);
  let outline = doc.getElementById("grid-outline-container");
  let gridList = doc.querySelector("#grid-list");
  let checkbox = gridList.children[0].querySelector("input");

  info("Checking the initial state of the Grid Inspector.");
  ok(!outline, "There should be no grid outline shown.");

  info("Toggling ON the CSS grid highlighter from the layout panel.");
  let onHighlighterShown = highlighters.once("grid-highlighter-shown");
  let onCheckboxChange = waitUntilState(store, state =>
    state.grids.length == 1 &&
    state.grids[0].highlighted);
  checkbox.click();
  yield onHighlighterShown;
  yield onCheckboxChange;

  outline = doc.getElementById("grid-outline");
  info("Checking the grid outline is shown.");
  is(outline.childNodes.length, 1, "One grid container outline is shown.");

  info("Toggling OFF the CSS grid highlighter from the layout panel.");
  let onHighlighterHidden = highlighters.once("grid-highlighter-hidden");
  onCheckboxChange = waitUntilState(store, state =>
    state.grids.length == 1 &&
    !state.grids[0].highlighted);
  checkbox.click();
  yield onHighlighterHidden;
  yield onCheckboxChange;

  outline = doc.getElementById("grid-outline-container");
  info("Checking the grid outline is hidden.");
  ok(!outline, "There should be no outline shown.");
});
