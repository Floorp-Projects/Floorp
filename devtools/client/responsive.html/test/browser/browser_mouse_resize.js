/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URL = "data:text/html;charset=utf-8,";

addRDMTask(TEST_URL, function* ({ ui, manager }) {
  ok(ui, "An instance of the RDM should be attached to the tab.");
  yield setViewportSize(ui, manager, 300, 300);

  // Do horizontal + vertical resize
  yield testViewportResize(ui, ".viewport-resize-handle",
    [10, 10], [320, 310], [10, 10]);

  // Do horizontal resize
  yield testViewportResize(ui, ".viewport-horizontal-resize-handle",
    [-10, 10], [300, 310], [-10, 0]);

  // Do vertical resize
  yield testViewportResize(ui, ".viewport-vertical-resize-handle",
    [-10, -10], [300, 300], [0, -10], ui);
});
