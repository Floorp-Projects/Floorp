/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the grid display badge toggles on the grid highlighter.

const TEST_URI = `
  <style type="text/css">
    #grid {
      display: grid;
    }
  </style>
  <div id="grid"></div>
`;

const HIGHLIGHTER_TYPE = "CssGridHighlighter";

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector } = await openLayoutView();
  const { highlighters, store } = inspector;

  info("Check the grid display badge is shown and not active.");
  await selectNode("#grid", inspector);
  const gridContainer = await getContainerForSelector("#grid", inspector);
  const gridDisplayBadge = gridContainer.elt.querySelector(".markup-badge[data-display]");
  ok(!gridDisplayBadge.classList.contains("active"), "grid display badge is not active.");

  info("Check the initial state of the grid highlighter.");
  ok(!highlighters.highlighters[HIGHLIGHTER_TYPE],
    "No CSS grid highlighter exists in the highlighters overlay.");
  ok(!highlighters.gridHighlighterShown, "No CSS grid highlighter is shown.");

  info("Toggling ON the CSS grid highlighter from the grid display badge.");
  const onHighlighterShown = highlighters.once("grid-highlighter-shown");
  let onCheckboxChange = waitUntilState(store, state =>
    state.grids.length === 1 &&
    state.grids[0].highlighted);
  gridDisplayBadge.click();
  await onHighlighterShown;
  await onCheckboxChange;

  info("Check the CSS grid highlighter is created and grid display badge state.");
  ok(highlighters.highlighters[HIGHLIGHTER_TYPE],
    "CSS grid highlighter is created in the highlighters overlay.");
  ok(highlighters.gridHighlighterShown, "CSS grid highlighter is shown.");
  ok(gridDisplayBadge.classList.contains("active"), "grid display badge is active.");

  info("Toggling OFF the CSS grid highlighter from the grid display badge.");
  const onHighlighterHidden = highlighters.once("grid-highlighter-hidden");
  onCheckboxChange = waitUntilState(store, state =>
    state.grids.length == 1 &&
    !state.grids[0].highlighted);
  gridDisplayBadge.click();
  await onHighlighterHidden;
  await onCheckboxChange;

  ok(!gridDisplayBadge.classList.contains("active"), "grid display badge is not active.");
});
