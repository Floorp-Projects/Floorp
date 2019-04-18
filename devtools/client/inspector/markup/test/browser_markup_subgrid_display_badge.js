/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URL = URL_ROOT + "doc_markup_subgrid.html";

add_task(async function() {
  info("Enable subgrid in order to see the subgrid display type.");
  await pushPref("layout.css.grid-template-subgrid-value.enabled", true);
  const { inspector } = await openInspectorForURL(TEST_URL);
  const { highlighters, store } = inspector;

  info("Check the subgrid display badge is shown and not active.");
  await selectNode("main", inspector);
  const eltContainer = await getContainerForSelector("main", inspector);
  const subgridDisplayBadge = eltContainer.elt.querySelector(
    ".inspector-badge.interactive[data-display]");
  ok(!subgridDisplayBadge.classList.contains("active"),
    "subgrid display badge is not active.");
  ok(subgridDisplayBadge.classList.contains("interactive"),
    "subgrid display badge is interactive.");

  info("Check the initial state of the grid highlighter.");
  ok(!highlighters.gridHighlighters.size,
    "No CSS grid highlighter exists in the highlighters overlay.");

  info("Toggling ON the CSS grid highlighter from the subgrid display badge.");
  const onHighlighterShown = highlighters.once("grid-highlighter-shown");
  let onCheckboxChange = waitUntilState(store, state =>
    state.grids.length === 2 &&
    state.grids[1].highlighted);
  subgridDisplayBadge.click();
  await onHighlighterShown;
  await onCheckboxChange;

  info("Check that the CSS grid highlighter is created and the display badge state.");
  is(highlighters.gridHighlighters.size, 1,
    "CSS grid highlighter is created in the highlighters overlay.");
  ok(subgridDisplayBadge.classList.contains("active"),
    "subgrid display badge is active.");
  ok(subgridDisplayBadge.classList.contains("interactive"),
   "subgrid display badge is interactive.");

  info("Toggling OFF the CSS grid highlighter from the subgrid display badge.");
  const onHighlighterHidden = highlighters.once("grid-highlighter-hidden");
  onCheckboxChange = waitUntilState(store, state =>
     state.grids.length == 2 &&
     !state.grids[1].highlighted);
  subgridDisplayBadge.click();
  await onHighlighterHidden;
  await onCheckboxChange;

  ok(!subgridDisplayBadge.classList.contains("active"),
    "subgrid display badge is not active.");
  ok(subgridDisplayBadge.classList.contains("interactive"),
    "subgrid display badge is interactive.");
});
