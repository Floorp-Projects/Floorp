/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the list of grids show the subgrids in the correct nested list and toggling
// the CSS grid highlighter for a subgrid.

const TEST_URI = URL_ROOT + "doc_subgrid.html";

add_task(async function() {
  await addTab(TEST_URI);
  const { gridInspector, inspector } = await openLayoutView();
  const { document: doc } = gridInspector;
  const { highlighters, store } = inspector;

  await selectNode("#grid", inspector);
  const gridListEl = doc.getElementById("grid-list");
  const containerSubgridListEl = gridListEl.children[1];
  const mainSubgridListEl = containerSubgridListEl.querySelector("ul");

  info("Checking the initial state of the Grid Inspector.");
  is(getGridItemElements(gridListEl).length, 1, "One grid container is listed.");
  is(getGridItemElements(containerSubgridListEl).length, 2,
    "Got the correct number of subgrids in div.container");
  is(getGridItemElements(mainSubgridListEl).length, 2,
    "Got the correct number of subgrids in main.subgrid");
  ok(!highlighters.gridHighlighters.size, "No CSS grid highlighter is shown.");

  info("Toggling ON the CSS grid highlighter from the layout panel.");
  const onHighlighterShown = highlighters.once("grid-highlighter-shown");
  let onCheckboxChange = waitUntilState(store, state => state.grids[1].highlighted);
  const checkbox = containerSubgridListEl.children[0].querySelector("input");
  checkbox.click();
  await onHighlighterShown;
  await onCheckboxChange;

  info("Checking the CSS grid highlighter is created.");
  is(highlighters.gridHighlighters.size, 1, "CSS grid highlighter is shown.");

  info("Toggling OFF the CSS grid highlighter from the layout panel.");
  const onHighlighterHidden = highlighters.once("grid-highlighter-hidden");
  onCheckboxChange = waitUntilState(store, state => !state.grids[1].highlighted);
  checkbox.click();
  await onHighlighterHidden;
  await onCheckboxChange;

  info("Checking the CSS grid highlighter is not shown.");
  ok(!highlighters.gridHighlighters.size, "No CSS grid highlighter is shown.");
});

/**
 * Returns the grid item elements <li> from the grid list element <ul>.
 *
 * @param  {Element} gridListEl
 *         The grid list element <ul>.
 * @return {Array<Element>} containing the grid item elements <li>.
 */
function getGridItemElements(gridListEl) {
  return [...gridListEl.children].filter(node => node.nodeName === "li");
}
