/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */

const TEST_FILE = "dummy_page.html";

// Test for bug 1321020.
add_task(function* () {
  let dir = getChromeDir(getResolvedURI(gTestPath));
  dir.append(TEST_FILE);
  const uriString = Services.io.newFileURI(dir).spec;
  const openedUriString = uriString + "?opened";

  // Open first file:// page.
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, uriString);
  registerCleanupFunction(function* () {
    yield BrowserTestUtils.removeTab(tab);
  });

  // Open new file:// tab from JavaScript in first file:// page.
  let promiseTabOpened = BrowserTestUtils.waitForNewTab(gBrowser, openedUriString);
  yield ContentTask.spawn(tab.linkedBrowser, openedUriString, uri => {
    content.open(uri, "_blank");
  });

  let openedTab = yield promiseTabOpened;
  registerCleanupFunction(function* () {
    yield BrowserTestUtils.removeTab(openedTab);
  });

  let openedBrowser = openedTab.linkedBrowser;
  yield BrowserTestUtils.browserLoaded(openedBrowser);

  // Ensure that new file:// tab can be navigated to web content.
  openedBrowser.loadURI("http://example.org/");
  let href = yield BrowserTestUtils.browserLoaded(openedBrowser);
  is(href, "http://example.org/",
     "Check that new file:// page has navigated successfully to web content");
});
