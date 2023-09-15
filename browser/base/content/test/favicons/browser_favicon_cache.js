add_task(async () => {
  const testPath =
    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    "http://example.com/browser/browser/base/content/test/favicons/cookie_favicon.html";
  const resetPath =
    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    "http://example.com/browser/browser/base/content/test/favicons/cookie_favicon.sjs?reset";

  let tab = BrowserTestUtils.addTab(gBrowser, testPath);
  gBrowser.selectedTab = tab;
  let browser = tab.linkedBrowser;

  let faviconPromise = waitForLinkAvailable(browser);
  await BrowserTestUtils.browserLoaded(browser);
  await faviconPromise;
  let cookies = Services.cookies.getCookiesFromHost(
    "example.com",
    browser.contentPrincipal.originAttributes
  );
  let seenCookie = false;
  for (let cookie of cookies) {
    if (cookie.name == "faviconCookie") {
      seenCookie = true;
      is(cookie.value, "1", "Should have seen the right initial cookie.");
    }
  }
  ok(seenCookie, "Should have seen the cookie.");

  faviconPromise = waitForLinkAvailable(browser);
  BrowserTestUtils.startLoadingURIString(browser, testPath);
  await BrowserTestUtils.browserLoaded(browser);
  await faviconPromise;
  cookies = Services.cookies.getCookiesFromHost(
    "example.com",
    browser.contentPrincipal.originAttributes
  );
  seenCookie = false;
  for (let cookie of cookies) {
    if (cookie.name == "faviconCookie") {
      seenCookie = true;
      is(cookie.value, "1", "Should have seen the cached cookie.");
    }
  }
  ok(seenCookie, "Should have seen the cookie.");

  // Reset the cookie so if this test is run again it will still pass.
  await fetch(resetPath);

  BrowserTestUtils.removeTab(tab);
});
