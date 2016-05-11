/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test global touch simulation button

const TEST_URL = "data:text/html;charset=utf-8,";

addRDMTask(TEST_URL, function* ({ ui }) {
  let { store, document } = ui.toolWindow;
  let touchButton = document.querySelector("#global-touch-simulation-button");

  // Wait until the viewport has been added
  yield waitUntilState(store, state => state.viewports.length == 1);
  yield waitForFrameLoad(ui, TEST_URL);

  ok(!touchButton.classList.contains("active"),
    "Touch simulation is not active by default.");

  touchButton.click();

  ok(touchButton.classList.contains("active"),
    "Touch simulation is started on click.");
});
