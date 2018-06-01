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

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI1));

  startTelemetry();

  const { inspector } = await openLayoutView();
  const { store } = inspector;

  info("Navigate to TEST_URI2");

  const onGridListUpdate = waitUntilState(store, state => state.grids.length == 1);
  await navigateTo(inspector,
    "data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI2));
  await onGridListUpdate;

  checkResults();
});

function checkResults() {
  // Check for:
  //   - 1 CSS Grid Element
  checkTelemetry("DEVTOOLS_NUMBER_OF_CSS_GRIDS_IN_A_PAGE", "",
    [0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0], "array");
}
