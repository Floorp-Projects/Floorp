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

  BrowserTestUtils.loadURI(
    win.gBrowser.selectedBrowser,
    gHttpsTestRoot + "test_page.html"
  );
  await BrowserTestUtils.browserLoaded(
    win.gBrowser.selectedBrowser,
    true,
    gHttpsTestRoot + "test_page.html"
  );

  let ssb = await openSSBFromBrowserWindow(win);
  Assert.equal(win.gBrowser.tabs.length, 1, "Should still be only one tab.");
  Assert.notEqual(tab, win.gBrowser.selectedTab, "Should be a new tab.");
  Assert.equal(
    getBrowser(ssb).currentURI.spec,
    gHttpsTestRoot + "test_page.html"
  );

  Assert.ok(!windowClosed, "Should not have seen the window close.");
  await BrowserTestUtils.closeWindow(ssb);
  await BrowserTestUtils.closeWindow(win);
});
