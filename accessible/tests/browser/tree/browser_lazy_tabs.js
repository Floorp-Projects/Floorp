/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that lazy background tabs aren't unintentionally loaded when building
// the a11y tree (bug 1700708).
addAccessibleTask(``, async function (browser, accDoc) {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.sessionstore.restore_on_demand", true]],
  });

  info("Opening a new window");
  let win = await BrowserTestUtils.openNewBrowserWindow();
  // Window is opened with a blank tab.
  info("Loading second tab");
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser: win.gBrowser,
    url: "data:text/html,2",
  });
  info("Loading third tab");
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser: win.gBrowser,
    url: "data:text/html,3",
  });
  info("Closing the window");
  await BrowserTestUtils.closeWindow(win);

  is(SessionStore.getClosedWindowCount(), 1, "Should have a window to restore");
  info("Restoring the window");
  win = SessionStore.undoCloseWindow(0);
  await BrowserTestUtils.waitForEvent(win, "SSWindowStateReady");
  await BrowserTestUtils.waitForEvent(
    win.gBrowser.tabContainer,
    "SSTabRestored"
  );
  is(win.gBrowser.tabs.length, 3, "3 tabs restored");
  ok(win.gBrowser.tabs[2].selected, "Third tab selected");
  ok(getAccessible(win.gBrowser.tabs[1]), "Second tab has accessible");
  ok(!win.gBrowser.browsers[1].isConnected, "Second tab is lazy");
  info("Closing the restored window");
  await BrowserTestUtils.closeWindow(win);
});
