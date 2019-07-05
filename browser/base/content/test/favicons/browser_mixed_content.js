add_task(async () => {
  const testPath =
    "https://example.com/browser/browser/base/content/test/favicons/file_insecure_favicon.html";
  const expectedIcon =
    "http://example.com/browser/browser/base/content/test/favicons/file_favicon.png";

  let tab = BrowserTestUtils.addTab(gBrowser, testPath);
  gBrowser.selectedTab = tab;
  let browser = tab.linkedBrowser;

  let faviconPromise = waitForLinkAvailable(browser);
  await BrowserTestUtils.browserLoaded(browser);
  let iconURI = await faviconPromise;
  is(iconURI, expectedIcon, "Got correct icon.");

  ok(
    gIdentityHandler._isMixedPassiveContentLoaded,
    "Should have seen mixed content."
  );

  BrowserTestUtils.removeTab(tab);
});
