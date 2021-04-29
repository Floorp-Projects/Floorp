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

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));

  const { inspector, gridInspector } = await openLayoutView();
  const { document: doc } = gridInspector;
  const { highlighters, store } = inspector;

  info("Clicking on the first checkbox to highlight the grid");
  const checkbox = doc.querySelector("#grid-list input");

  const onHighlighterShown = highlighters.once("grid-highlighter-shown");
  const onCheckboxChange = waitUntilState(
    store,
    state => state.grids.length == 1 && state.grids[0].highlighted
  );
  const onGridOutlineRendered = waitForDOM(doc, ".grid-outline-cell", 2);

  checkbox.click();

  await onHighlighterShown;
  await onCheckboxChange;
  let elements = await onGridOutlineRendered;

  info("Checking the grid outline is shown.");
  is(elements.length, 2, "Grid outline is shown.");

  info("Changing the grid in the page");
  const onReflow = inspector.once("reflow-in-selected-target");
  const onGridOutlineChanged = waitForDOM(doc, ".grid-outline-cell", 4);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    const div = content.document.createElement("div");
    div.textContent = "item 3";
    content.document.querySelector(".container").appendChild(div);
  });

  await onReflow;
  elements = await onGridOutlineChanged;

  info("Checking the grid outline is correct.");
  is(elements.length, 4, "Grid outline was changed.");
});
