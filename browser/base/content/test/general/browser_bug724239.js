/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test_blank() {
  await BrowserTestUtils.withNewTab({ gBrowser, url: "about:blank" },
                                    async function(browser) {
    BrowserTestUtils.loadURI(browser, "http://example.com");
    await BrowserTestUtils.browserLoaded(browser);
    ok(!gBrowser.canGoBack, "about:blank wasn't added to session history");
  });
});

add_task(async function test_newtab() {
  await SpecialPowers.pushPrefEnv({set: [["browser.newtabpage.activity-stream.enabled", true]]});
  await BrowserTestUtils.withNewTab({ gBrowser, url: "about:blank" },
                                    async function(browser) {
    // Can't load it directly because that'll use a preloaded tab if present.
    BrowserTestUtils.loadURI(browser, "about:newtab");
    await BrowserTestUtils.browserLoaded(browser);

    BrowserTestUtils.loadURI(browser, "http://example.com");
    await BrowserTestUtils.browserLoaded(browser);
    is(gBrowser.canGoBack, true, "about:newtab was added to the session history when AS was enabled.");
  });
  await SpecialPowers.pushPrefEnv({set: [["browser.newtabpage.activity-stream.enabled", false]]});
  await BrowserTestUtils.withNewTab({ gBrowser, url: "about:blank" },
                                    async function(browser) {
    // Can't load it directly because that'll use a preloaded tab if present.
    BrowserTestUtils.loadURI(browser, "about:newtab");
    await BrowserTestUtils.browserLoaded(browser);

    BrowserTestUtils.loadURI(browser, "http://example.com");
    await BrowserTestUtils.browserLoaded(browser);
    is(gBrowser.canGoBack, false, "about:newtab was not added to the session history when AS was disabled.");
  });
});
