/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var win;

registerCleanupFunction(function* () {
  if (win) {
    yield BrowserTestUtils.closeWindow(win);
  }
});

add_task(function* test_panel_should_be_shown() {
  registerCleanupFunction(function* () {
    cleanupPanel();
  });

  yield window.LoopUI.openPanel();

  Assert.notEqual(document.getElementById("loop-notification-panel"), null, "Panel exists");
});

add_task(function* test_init_copy_panel_private() {
  let rejected = false;
  win = yield BrowserTestUtils.openNewBrowserWindow({ private: true });

  try {
    yield win.LoopUI.openPanel();
  }
  catch (ex) {
    rejected = true;
  }

  Assert.ok(rejected, "openPanel promise should have been rejected");
  Assert.equal(win.document.getElementById("loop-notification-panel").childNodes.length, 0, "Panel doesn't exist for private browsing");
});
