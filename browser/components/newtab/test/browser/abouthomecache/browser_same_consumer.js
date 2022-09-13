/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that if a page attempts to load the script stream without
 * having also loaded the page stream, that it will fail and get
 * the default non-cached script.
 */
add_task(async function test_same_consumer() {
  await withFullyLoadedAboutHome(async browser => {
    await simulateRestart(browser);

    // We need the CSP meta tag in about: pages, otherwise we hit assertions in
    // debug builds.
    //
    // We inject a script that sets a __CACHE_CONSUMED__ property to true on
    // the window element. We'll test to ensure that if we try to load the
    // script cache from a different BrowsingContext that this property is
    // not set.
    await injectIntoCache(
      `
      <html>
        <head>
        <meta http-equiv="Content-Security-Policy" content="default-src 'none'; object-src 'none'; script-src resource: chrome:; connect-src https:; img-src https: data: blob:; style-src 'unsafe-inline';">
        </head>
        <body>
          <h1>A fake about:home page</h1>
          <div id="root"></div>
        </body>
      </html>`,
      "window.__CACHE_CONSUMED__ = true;"
    );
    await simulateRestart(browser, { withAutoShutdownWrite: false });

    // Attempting to load the script from the cache should fail, and instead load
    // the markup.
    await BrowserTestUtils.withNewTab("about:home?jscache", async browser2 => {
      await SpecialPowers.spawn(browser2, [], async () => {
        Assert.ok(
          !Cu.waiveXrays(content).__CACHE_CONSUMED__,
          "Should not have found __CACHE_CONSUMED__ property"
        );
        Assert.ok(
          content.document.body.classList.contains("activity-stream"),
          "Should have found activity-stream class on <body> element"
        );
      });
    });
  });
});
