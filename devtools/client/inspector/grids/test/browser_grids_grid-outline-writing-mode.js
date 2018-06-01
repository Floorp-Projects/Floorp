/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the grid outline adjusts to match the container's writing mode.

const TEST_URI = `
  <style type='text/css'>
    .grid {
      display: grid;
      width: 400px;
      height: 300px;
    }
    .rtl {
      direction: rtl;
    }
    .v-rl {
      writing-mode: vertical-rl;
    }
    .v-lr {
      writing-mode: vertical-lr;
    }
    .s-rl {
      writing-mode: sideways-rl;
    }
    .s-lr {
      writing-mode: sideways-lr;
    }
  </style>
  <div class="grid">
    <div id="cella">Cell A</div>
    <div id="cellb">Cell B</div>
    <div id="cellc">Cell C</div>
  </div>
  <div class="grid rtl">
    <div id="cella">Cell A</div>
    <div id="cellb">Cell B</div>
    <div id="cellc">Cell C</div>
  </div>
  <div class="grid v-rl">
    <div id="cella">Cell A</div>
    <div id="cellb">Cell B</div>
    <div id="cellc">Cell C</div>
  </div>
  <div class="grid v-lr">
    <div id="cella">Cell A</div>
    <div id="cellb">Cell B</div>
    <div id="cellc">Cell C</div>
  </div>
  <div class="grid s-rl">
    <div id="cella">Cell A</div>
    <div id="cellb">Cell B</div>
    <div id="cellc">Cell C</div>
  </div>
  <div class="grid s-lr">
    <div id="cella">Cell A</div>
    <div id="cellb">Cell B</div>
    <div id="cellc">Cell C</div>
  </div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));

  const { inspector, gridInspector } = await openLayoutView();
  const { document: doc } = gridInspector;
  const { highlighters, store } = inspector;

  info("Checking the initial state of the Grid Inspector.");
  ok(!doc.getElementById("grid-outline-container"),
    "There should be no grid outline shown.");

  let elements;

  elements = await enableGrid(doc, highlighters, store, 0);
  is(elements[0].style.transform,
    "matrix(1, 0, 0, 1, 0, 0)",
    "Transform matches for horizontal-tb and ltr.");
  await disableGrid(doc, highlighters, store, 0);

  elements = await enableGrid(doc, highlighters, store, 1);
  is(elements[0].style.transform,
    "matrix(-1, 0, 0, 1, 200, 0)",
    "Transform matches for horizontal-tb and rtl");
  await disableGrid(doc, highlighters, store, 1);

  elements = await enableGrid(doc, highlighters, store, 2);
  is(elements[0].style.transform,
    "matrix(6.12323e-17, 1, -1, 6.12323e-17, 200, 0)",
    "Transform matches for vertical-rl and ltr");
  await disableGrid(doc, highlighters, store, 2);

  elements = await enableGrid(doc, highlighters, store, 3);
  is(elements[0].style.transform,
    "matrix(-6.12323e-17, 1, 1, 6.12323e-17, 0, 0)",
    "Transform matches for vertical-lr and ltr");
  await disableGrid(doc, highlighters, store, 3);

  elements = await enableGrid(doc, highlighters, store, 4);
  is(elements[0].style.transform,
    "matrix(6.12323e-17, 1, -1, 6.12323e-17, 200, 0)",
    "Transform matches for sideways-rl and ltr");
  await disableGrid(doc, highlighters, store, 4);

  elements = await enableGrid(doc, highlighters, store, 5);
  is(elements[0].style.transform,
    "matrix(6.12323e-17, -1, 1, 6.12323e-17, -9.18485e-15, 150)",
    "Transform matches for sideways-lr and ltr");
  await disableGrid(doc, highlighters, store, 5);
});

async function enableGrid(doc, highlighters, store, index) {
  info(`Enabling the CSS grid highlighter for grid ${index}.`);
  const onHighlighterShown = highlighters.once("grid-highlighter-shown");
  const onCheckboxChange = waitUntilState(store, state =>
    state.grids.length == 6 &&
    state.grids[index].highlighted);
  const onGridOutlineRendered = waitForDOM(doc, "#grid-cell-group");
  const gridList = doc.getElementById("grid-list");
  gridList.children[index].querySelector("input").click();
  await onHighlighterShown;
  await onCheckboxChange;
  return onGridOutlineRendered;
}

async function disableGrid(doc, highlighters, store, index) {
  info(`Disabling the CSS grid highlighter for grid ${index}.`);
  const onHighlighterShown = highlighters.once("grid-highlighter-hidden");
  const onCheckboxChange = waitUntilState(store, state =>
    state.grids.length == 6 &&
    !state.grids[index].highlighted);
  const onGridOutlineRemoved = waitForDOM(doc, "#grid-cell-group", 0);
  const gridList = doc.getElementById("grid-list");
  gridList.children[index].querySelector("input").click();
  await onHighlighterShown;
  await onCheckboxChange;
  return onGridOutlineRemoved;
}
