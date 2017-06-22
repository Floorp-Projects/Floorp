/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the telemetry count for the number of CSS Grid Elements on a page navigation
// is correct when the toolbox is opened.

const TEST_URI1 = `
  <div></div>
`;

const TEST_URI2 = `
  <style type='text/css'>
    #grid {
      display: grid;
    }
  </style>
  <div id="grid">
    <div id="cell1">cell1</div>
    <div id="cell2">cell2</div>
  </div>
`;

const CSS_GRID_COUNT_HISTOGRAM_ID = "DEVTOOLS_NUMBER_OF_CSS_GRIDS_IN_A_PAGE";

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI1));

  let { inspector } = yield openLayoutView();
  let { store } = inspector;

  info("Navigate to TEST_URI2");

  let onGridListUpdate = waitUntilState(store, state => state.grids.length == 1);
  yield navigateTo(inspector,
    "data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI2));
  yield onGridListUpdate;

  let histogram = Services.telemetry.getHistogramById(CSS_GRID_COUNT_HISTOGRAM_ID);
  let snapshot = histogram.snapshot();

  is(snapshot.counts[1], 1, "Got a count of 1 for 1 CSS Grid element seen.");
  is(snapshot.sum, 1, "Got the correct sum.");
});
