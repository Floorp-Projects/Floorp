/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the z order of grids.

const TEST_URI = URL_ROOT + "doc_subgrid.html";

add_task(async () => {
  await addTab(TEST_URI);
  const { gridInspector, inspector } = await openLayoutView();
  const { document: doc } = gridInspector;
  const { highlighters, store } = inspector;

  await selectNode("#grid", inspector);
  await waitUntilState(store, state => state.grids.length === 5);

  const parentEl = doc.getElementById("grid-list");
  // Input for .container
  const parentInput = parentEl.children[0].querySelector("input");
  const subgridEl = parentEl.children[1];
  // Input for <main>
  const subgridInput = subgridEl.children[1].querySelector("input");
  const grandSubgridEl = subgridEl.children[2];
  // Input for .aside1
  const grandSubgridInput = grandSubgridEl.children[0].querySelector("input");

  info(
    "Toggling ON the CSS grid highlighters for .container, <main> and .aside1"
  );
  const grandSubgridFront = await toggle(grandSubgridInput, highlighters);
  const subgridFront = await toggle(subgridInput, highlighters);
  let parentFront = await toggle(parentInput, highlighters);
  await waitUntilState(
    store,
    state => state.grids.filter(g => g.highlighted).length === 3
  );

  info("Check z-index of grid highlighting");
  is(getZIndex(store, parentFront), 0, "z-index of parent grid is 0");
  is(getZIndex(store, subgridFront), 1, "z-index of subgrid is 1");
  is(getZIndex(store, grandSubgridFront), 2, "z-index of subgrid is 2");

  info("Toggling OFF the CSS grid highlighters for .container");
  await toggle(parentInput, highlighters);
  await waitUntilState(
    store,
    state => state.grids.filter(g => g.highlighted).length === 2
  );

  info("Check z-index keeps even if the parent grid is hidden");
  is(getZIndex(store, subgridFront), 1, "z-index of subgrid is 1");
  is(getZIndex(store, grandSubgridFront), 2, "z-index of subgrid is 2");

  info("Toggling ON again the CSS grid highlighters for .container");
  parentFront = await toggle(parentInput, highlighters);
  await waitUntilState(
    store,
    state => state.grids.filter(g => g.highlighted).length === 3
  );

  info("Check z-index of all of grids highlighting keeps");
  is(getZIndex(store, parentFront), 0, "z-index of parent grid is 0");
  is(getZIndex(store, subgridFront), 1, "z-index of subgrid is 1");
  is(getZIndex(store, grandSubgridFront), 2, "z-index of subgrid is 2");
});

function getZIndex(store, nodeFront) {
  const grids = store.getState().grids;
  const gridData = grids.find(g => g.nodeFront === nodeFront);
  return gridData.zIndex;
}

async function toggle(input, highlighters) {
  const eventName = input.checked
    ? "grid-highlighter-hidden"
    : "grid-highlighter-shown";
  const onHighlighterEvent = highlighters.once(eventName);
  input.click();
  const nodeFront = await onHighlighterEvent;
  return nodeFront;
}
