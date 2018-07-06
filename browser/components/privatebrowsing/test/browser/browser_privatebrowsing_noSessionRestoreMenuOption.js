"use strict";

/**
 * Tests that if we open a tab within a private browsing window, and then
 * close that private browsing window, that subsequent private browsing
 * windows do not allow the command for restoring the last session.
 */
add_task(async function test_no_session_restore_menu_option() {
  let win = await BrowserTestUtils.openNewBrowserWindow({ private: true });
  ok(true, "The first private window got loaded");
  BrowserTestUtils.addTab(win.gBrowser, "about:mozilla");
  await BrowserTestUtils.closeWindow(win);

  win = await BrowserTestUtils.openNewBrowserWindow({ private: true });
  let srCommand = win.document.getElementById("Browser:RestoreLastSession");
  ok(srCommand, "The Session Restore command should exist");
  is(PrivateBrowsingUtils.isWindowPrivate(win), true,
     "PrivateBrowsingUtils should report the correct per-window private browsing status");
  is(srCommand.hasAttribute("disabled"), true,
     "The Session Restore command should be disabled in private browsing mode");

  await BrowserTestUtils.closeWindow(win);
});
