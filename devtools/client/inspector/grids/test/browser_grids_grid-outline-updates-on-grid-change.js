/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the grid outline does reflect the grid in the page even after the grid has
// changed.

const TEST_URI = `
  <style>
  .container {
    display: grid;
    grid-template-columns: repeat(2, 20vw);
    grid-auto-rows: 20px;
  }
  </style>
  <div class="container">
    <div>item 1</div>
    <div>item 2</div>
  </div>
`;

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));

  let { inspector, gridInspector, testActor } = yield openLayoutView();
  let { document: doc } = gridInspector;
  let { highlighters, store } = inspector;

  info("Clicking on the first checkbox to highlight the grid");
  let checkbox = doc.querySelector("#grid-list input");

  let onHighlighterShown = highlighters.once("grid-highlighter-shown");
  let onCheckboxChange = waitUntilState(store, state =>
    state.grids.length == 1 && state.grids[0].highlighted);
  let onGridOutlineRendered = waitForDOM(doc, ".grid-outline-cell", 2);

  checkbox.click();

  yield onHighlighterShown;
  yield onCheckboxChange;
  let elements = yield onGridOutlineRendered;

  info("Checking the grid outline is shown.");
  is(elements.length, 2, "Grid outline is shown.");

  info("Changing the grid in the page");
  let onReflow = new Promise(resolve => {
    let listener = {
      callback: () => {
        inspector.reflowTracker.untrackReflows(listener, listener.callback);
        resolve();
      }
    };
    inspector.reflowTracker.trackReflows(listener, listener.callback);
  });
  let onGridOutlineChanged = waitForDOM(doc, ".grid-outline-cell", 4);

  testActor.eval(`
    const div = content.document.createElement("div");
    div.textContent = "item 3";
    content.document.querySelector(".container").appendChild(div);
  `);

  yield onReflow;
  elements = yield onGridOutlineChanged;

  info("Checking the grid outline is correct.");
  is(elements.length, 4, "Grid outline was changed.");
});
