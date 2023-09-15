add_task(async () => {
  const testPath =
    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    "http://example.com/browser/browser/base/content/test/favicons/blank.html";
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  const expectedIcon = "http://example.com/favicon.ico";

  let tab = BrowserTestUtils.addTab(gBrowser, testPath);
  gBrowser.selectedTab = tab;
  let browser = tab.linkedBrowser;

  let faviconPromise = waitForLinkAvailable(browser);
  await BrowserTestUtils.browserLoaded(browser);
  let iconURI = await faviconPromise;
  is(iconURI, expectedIcon, "Got correct initial icon.");

  faviconPromise = waitForLinkAvailable(browser);
  BrowserTestUtils.startLoadingURIString(browser, testPath);
  await BrowserTestUtils.browserLoaded(browser);
  iconURI = await faviconPromise;
  is(iconURI, expectedIcon, "Got correct icon on second load.");

  BrowserTestUtils.removeTab(tab);
});
