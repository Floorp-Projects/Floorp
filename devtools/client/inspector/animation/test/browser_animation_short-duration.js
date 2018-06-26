/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test tooltips and iteration path of summary graph with short duration animation.

add_task(async function() {
  await addTab(URL_ROOT + "doc_short_duration.html");
  const { panel } = await openAnimationInspector();

  const animationItemEl =
    findAnimationItemElementsByTargetSelector(panel, ".short");
  const summaryGraphEl = animationItemEl.querySelector(".animation-summary-graph");

  info("Check tooltip");
  assertTooltip(summaryGraphEl);

  info("Check iteration path");
  assertIterationPath(summaryGraphEl);
});

function assertTooltip(summaryGraphEl) {
  const tooltip = summaryGraphEl.getAttribute("title");
  const expected = "Duration: 0s";
  ok(tooltip.includes(expected), `Tooltip should include '${ expected }'`);
}

function assertIterationPath(summaryGraphEl) {
  const pathEl =
    summaryGraphEl.querySelector(
      ".animation-computed-timing-path .animation-iteration-path");
  const expected = [
    { x: 0, y: 0 },
    { x: 0.999, y: 99.9 },
    { x: 1, y: 0 },
  ];
  assertPathSegments(pathEl, true, expected);
}
