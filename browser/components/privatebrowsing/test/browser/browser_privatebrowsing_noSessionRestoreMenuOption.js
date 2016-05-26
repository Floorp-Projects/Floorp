"use strict";

/**
 * Tests that if we open a tab within a private browsing window, and then
 * close that private browsing window, that subsequent private browsing
 * windows do not allow the command for restoring the last session.
 */
add_task(function* test_no_session_restore_menu_option() {
  let win = yield BrowserTestUtils.openNewBrowserWindow({ private: true });
  ok(true, "The first private window got loaded");
  win.gBrowser.addTab("about:mozilla");
  yield BrowserTestUtils.closeWindow(win);

  win = yield BrowserTestUtils.openNewBrowserWindow({ private: true });
  let srCommand = win.document.getElementById("Browser:RestoreLastSession");
  ok(srCommand, "The Session Restore command should exist");
  is(PrivateBrowsingUtils.isWindowPrivate(win), true,
     "PrivateBrowsingUtils should report the correct per-window private browsing status");
  is(srCommand.hasAttribute("disabled"), true,
     "The Session Restore command should be disabled in private browsing mode");

  yield BrowserTestUtils.closeWindow(win);
});
