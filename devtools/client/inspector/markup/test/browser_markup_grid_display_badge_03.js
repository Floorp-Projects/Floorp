/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that toggling a flex highlighter does not change a grid highlighter badge.
// Bug 1592604

const TEST_URI = `
  <style type='text/css'>
    .grid {
      display: grid;
    }
    .flex {
      display: flex;
    }
  </style>
  <div class="grid">
  </div>
  <div class="flex">
  </div>
`;

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector } = await openLayoutView();

  const gridBadge = await enableHighlighterByBadge("grid", ".grid", inspector);
  const flexBadge = await enableHighlighterByBadge("flex", ".flex", inspector);

  info("Check that both display badges are active");
  ok(flexBadge.classList.contains("active"), `flex display badge is active.`);
  ok(gridBadge.classList.contains("active"), `grid display badge is active.`);
});

/**
 * Enable the flex or grid highlighter by clicking on the corresponding badge
 * next to a node in the markup view. Returns the badge element.
 *
 * @param  {String} type
 *         Either "flex" or "grid"
 * @param  {String} selector
 *         Selector matching the flex or grid container element.
 * @param  {Inspector} inspector
 *         Instance of Inspector panel
 * @return {Element} The DOM element of the display badge that shows next to the element
 *         mathched by the selector in the markup view.
 */
async function enableHighlighterByBadge(type, selector, inspector) {
  const { waitForHighlighterTypeShown } = getHighlighterTestHelpers(inspector);

  info(`Check the ${type} display badge is shown and not active.`);
  const container = await getContainerForSelector(selector, inspector);
  const badge = container.elt.querySelector(
    ".inspector-badge.interactive[data-display]"
  );
  ok(!badge.classList.contains("active"), `${type} badge is not active.`);
  ok(badge.classList.contains("interactive"), `${type} badge is interactive.`);

  info(`Toggling ON the ${type} highlighter from the ${type} display badge.`);
  let onHighlighterShown;
  switch (type) {
    case "grid":
      onHighlighterShown = waitForHighlighterTypeShown(
        inspector.highlighters.TYPES.GRID
      );
      break;
    case "flex":
      onHighlighterShown = waitForHighlighterTypeShown(
        inspector.highlighters.TYPES.FLEXBOX
      );
      break;
  }

  badge.click();
  await onHighlighterShown;

  ok(badge.classList.contains("active"), `${type} badge is active.`);
  ok(badge.classList.contains("interactive"), `${type} badge is interactive.`);

  return badge;
}
