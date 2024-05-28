/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// eslint-disable-next-line @microsoft/sdl/no-insecure-url
const TEST_URL = "http://example.com";

/**
 * Test that if the StatusPanel is shown for a link, and then
 * hidden, that it can be shown again for that same link.
 * (Bug 1445455).
 */
add_task(async function test_show_statuspanel_twice() {
  let win = await BrowserTestUtils.openNewBrowserWindow();
  win.XULBrowserWindow.overLink = TEST_URL;
  win.StatusPanel.update();
  await promiseStatusPanelShown(win, TEST_URL);

  win.XULBrowserWindow.overLink = "";
  win.StatusPanel.update();
  await promiseStatusPanelHidden(win);

  win.XULBrowserWindow.overLink = TEST_URL;
  win.StatusPanel.update();
  await promiseStatusPanelShown(win, TEST_URL);

  await BrowserTestUtils.closeWindow(win);
});
