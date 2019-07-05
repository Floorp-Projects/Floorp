/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test_blank() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function(browser) {
      BrowserTestUtils.loadURI(browser, "http://example.com");
      await BrowserTestUtils.browserLoaded(browser);
      ok(!gBrowser.canGoBack, "about:blank wasn't added to session history");
    }
  );
});

add_task(async function test_newtab() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function(browser) {
      let tab = gBrowser.getTabForBrowser(browser);

      // Can't load it directly because that'll use a preloaded tab if present.
      BrowserTestUtils.loadURI(browser, "about:newtab");
      // We will need to wait for about:newtab to be loaded so that it goes into
      // the session history.
      await BrowserTestUtils.browserStopped(browser, "about:newtab");

      let { mustChangeProcess } = E10SUtils.shouldLoadURIInBrowser(
        browser,
        "http://example.com"
      );

      BrowserTestUtils.loadURI(browser, "http://example.com");

      let stopped = BrowserTestUtils.browserStopped(browser);

      if (mustChangeProcess) {
        // If we did a process flip, we will need to ensure that the restoration has
        // completed before we check gBrowser.canGoBack.
        await BrowserTestUtils.waitForEvent(tab, "SSTabRestored");
      }

      await stopped;

      is(
        gBrowser.canGoBack,
        true,
        "about:newtab was added to the session history when AS was enabled."
      );
    }
  );
});
