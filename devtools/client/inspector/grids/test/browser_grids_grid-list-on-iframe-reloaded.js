/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the list of grids does refresh when an iframe containing a grid is removed
// and re-created.
// See bug 1378306 where this happened with jsfiddle.

const TEST_URI = URL_ROOT + "doc_iframe_reloaded.html";

add_task(async function() {
  await addTab(TEST_URI);

  const { inspector, gridInspector, testActor } = await openLayoutView();
  const { document: doc } = gridInspector;
  const { store, highlighters } = inspector;
  const gridList = doc.querySelector("#grid-list");

  info("Clicking on the first checkbox to highlight the grid");
  await enableTheFirstGrid(doc, inspector);

  is(gridList.childNodes.length, 1, "There's one grid in the list");
  const checkbox = gridList.querySelector("input");
  ok(checkbox.checked, "The checkbox is checked");
  ok(highlighters.gridHighlighterShown, "There's a highlighter shown");

  info("Reload the iframe in content and expect the grid list to update");
  const oldGrid = store.getState().grids[0];
  const onNewListUnchecked = waitUntilState(store, state =>
    state.grids.length == 1 &&
    state.grids[0].actorID !== oldGrid.actorID &&
    !state.grids[0].highlighted);
  const onHighlighterHidden = highlighters.once("grid-highlighter-hidden");
  testActor.eval("window.wrappedJSObject.reloadIFrame();");
  await onNewListUnchecked;
  await onHighlighterHidden;

  is(gridList.childNodes.length, 1, "There's still one grid in the list");

  info("Highlight the first grid again to make sure this still works");
  await enableTheFirstGrid(doc, inspector);

  is(gridList.childNodes.length, 1, "There's again one grid in the list");
  ok(highlighters.gridHighlighterShown, "There's a highlighter shown");
});

async function enableTheFirstGrid(doc, { highlighters, store }) {
  const checkbox = doc.querySelector("#grid-list input");

  const onHighlighterShown = highlighters.once("grid-highlighter-shown");
  const onCheckboxChange = waitUntilState(store, state =>
    state.grids.length == 1 && state.grids[0].highlighted);

  checkbox.click();

  await onHighlighterShown;
  await onCheckboxChange;
}
