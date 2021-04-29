/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the list of grids does refresh when an iframe containing a grid is removed
// and re-created.
// See bug 1378306 where this happened with jsfiddle.

const TEST_URI = URL_ROOT + "doc_iframe_reloaded.html";

add_task(async function() {
  await addTab(TEST_URI);
  const { gridInspector, inspector } = await openLayoutView();
  const { document: doc } = gridInspector;
  const { highlighters, store } = inspector;
  const gridList = doc.getElementById("grid-list");
  const checkbox = gridList.children[0].querySelector("input");

  info("Clicking on the first checkbox to highlight the grid");
  const onHighlighterShown = highlighters.once("grid-highlighter-shown");
  const onCheckboxChange = waitUntilState(
    store,
    state => state.grids.length == 1 && state.grids[0].highlighted
  );
  checkbox.click();
  await onHighlighterShown;
  await onCheckboxChange;

  ok(checkbox.checked, "The checkbox is checked");
  is(gridList.childNodes.length, 1, "There's one grid in the list");
  is(highlighters.gridHighlighters.size, 1, "There's a highlighter shown");
  is(
    highlighters.state.grids.size,
    1,
    "There's a saved grid state to be restored."
  );

  info("Reload the iframe in content and expect the grid list to update");
  const oldGrid = store.getState().grids[0];
  const onNewListUnchecked = waitUntilState(
    store,
    state =>
      state.grids.length == 1 &&
      state.grids[0].actorID !== oldGrid.actorID &&
      !state.grids[0].highlighted
  );
  const onHighlighterHidden = highlighters.once("grid-highlighter-hidden");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], () =>
    content.wrappedJSObject.reloadIFrame()
  );
  await onNewListUnchecked;
  await onHighlighterHidden;

  is(gridList.childNodes.length, 1, "There's still one grid in the list");
  ok(!highlighters.state.grids.size, "No grids to be restored on page reload.");
});
