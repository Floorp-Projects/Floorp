/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test global exit button

const TEST_URL = "data:text/html;charset=utf-8,";

addRDMTask(TEST_URL, function* ({ ui, manager }) {
  let { toolWindow } = ui;
  let { store } = toolWindow;

  // Wait until the viewport has been added
  yield waitUntilState(store, state => state.viewports.length == 1);

  let exitButton = toolWindow.document.getElementById("global-exit-button");

  yield waitForFrameLoad(ui, TEST_URL);

  ok(manager.isActiveForTab(ui.tab),
    "Responsive Design Mode active for the tab");

  exitButton.click();

  yield once(manager, "off");

  ok(!manager.isActiveForTab(ui.tab),
    "Responsive Design Mode is not active for the tab");
});
