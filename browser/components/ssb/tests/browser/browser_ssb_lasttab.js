/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Opening from the last browser tab should not close the window.
add_task(async () => {
  let win = await BrowserTestUtils.openNewBrowserWindow();

  let windowClosed = false;
  BrowserTestUtils.windowClosed(win).then(() => {
    windowClosed = true;
  });

  Assert.equal(win.gBrowser.tabs.length, 1, "Should be only one tab.");
  let tab = win.gBrowser.selectedTab;

  let loaded = BrowserTestUtils.browserLoaded(
    win.gBrowser.selectedBrowser,
    false,
    gHttpsTestRoot + "test_page.html"
  );
  BrowserTestUtils.loadURI(
    win.gBrowser.selectedBrowser,
    gHttpsTestRoot + "test_page.html"
  );
  await loaded;

  let ssb = await openSSBFromBrowserWindow(win);
  Assert.equal(win.gBrowser.tabs.length, 1, "Should still be only one tab.");
  Assert.notEqual(tab, win.gBrowser.selectedTab, "Should be a new tab.");

  Assert.ok(!windowClosed, "Should not have seen the window close.");
  await BrowserTestUtils.closeWindow(ssb);
  await BrowserTestUtils.closeWindow(win);
});
