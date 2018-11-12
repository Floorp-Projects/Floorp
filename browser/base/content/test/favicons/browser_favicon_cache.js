add_task(async () => {
  const testPath = "http://example.com/browser/browser/base/content/test/favicons/cookie_favicon.html";
  const testUrl = Services.io.newURI(testPath);

  let tab = BrowserTestUtils.addTab(gBrowser, testPath);
  gBrowser.selectedTab = tab;
  let browser = tab.linkedBrowser;

  let faviconPromise = waitForLinkAvailable(browser);
  await BrowserTestUtils.browserLoaded(browser);
  await faviconPromise;
  let cookies = Services.cookies.getCookiesFromHost("example.com", browser.contentPrincipal.originAttributes);
  let seenCookie = false;
  for (let cookie of cookies) {
    if (cookie.name == "faviconCookie") {
      seenCookie = true;
      is(cookie.value, 1, "Should have seen the right initial cookie.");
    }
  }
  ok(seenCookie, "Should have seen the cookie.");

  faviconPromise = waitForLinkAvailable(browser);
  BrowserTestUtils.loadURI(browser, testPath);
  await BrowserTestUtils.browserLoaded(browser);
  await faviconPromise;
  cookies = Services.cookies.getCookiesFromHost("example.com", browser.contentPrincipal.originAttributes);
  seenCookie = false;
  for (let cookie of cookies) {
    if (cookie.name == "faviconCookie") {
      seenCookie = true;
      is(cookie.value, 1, "Should have seen the cached cookie.");
    }
  }
  ok(seenCookie, "Should have seen the cookie.");

  BrowserTestUtils.removeTab(tab);
});
