/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { TabStateFlusher } = ChromeUtils.importESModule(
  "resource:///modules/sessionstore/TabStateFlusher.sys.mjs"
);

add_task(async function test_blank() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function (browser) {
      // eslint-disable-next-line @microsoft/sdl/no-insecure-url
      BrowserTestUtils.startLoadingURIString(browser, "http://example.com");
      await BrowserTestUtils.browserLoaded(browser);
      ok(!gBrowser.canGoBack, "about:blank wasn't added to session history");
    }
  );
});

add_task(async function test_newtab() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function (browser) {
      // Can't load it directly because that'll use a preloaded tab if present.
      let stopped = BrowserTestUtils.browserStopped(browser, "about:newtab");
      BrowserTestUtils.startLoadingURIString(browser, "about:newtab");
      await stopped;

      // eslint-disable-next-line @microsoft/sdl/no-insecure-url
      stopped = BrowserTestUtils.browserStopped(browser, "http://example.com/");
      // eslint-disable-next-line @microsoft/sdl/no-insecure-url
      BrowserTestUtils.startLoadingURIString(browser, "http://example.com/");
      await stopped;

      // This makes sure the parent process has the most up-to-date notion
      // of the tab's session history.
      await TabStateFlusher.flush(browser);

      let tab = gBrowser.getTabForBrowser(browser);
      let tabState = JSON.parse(SessionStore.getTabState(tab));
      Assert.equal(
        tabState.entries.length,
        2,
        "We should have 2 entries in the session history."
      );

      Assert.equal(
        tabState.entries[0].url,
        "about:newtab",
        "about:newtab should be the first entry."
      );

      Assert.ok(gBrowser.canGoBack, "Should be able to browse back.");
    }
  );
});
