// This test simulates opening the newtab page and moving it to a new window.
// Links in the page should still work.
add_task(async function test_newtab_to_window() {
  await setTestTopSites();

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:newtab",
    false
  );

  let swappedPromise = BrowserTestUtils.waitForEvent(
    tab.linkedBrowser,
    "SwapDocShells"
  );
  let newWindow = gBrowser.replaceTabWithWindow(tab);
  await swappedPromise;

  is(
    newWindow.gBrowser.selectedBrowser.currentURI.spec,
    "about:newtab",
    "about:newtab moved to window"
  );

  let tabPromise = BrowserTestUtils.waitForNewTab(
    newWindow.gBrowser,
    "https://example.com/",
    true
  );

  await BrowserTestUtils.synthesizeMouse(
    `.top-sites a`,
    2,
    2,
    { accelKey: true },
    newWindow.gBrowser.selectedBrowser
  );

  await tabPromise;

  is(newWindow.gBrowser.tabs.length, 2, "second page is opened");

  BrowserTestUtils.removeTab(newWindow.gBrowser.selectedTab);
  await BrowserTestUtils.closeWindow(newWindow);
});
