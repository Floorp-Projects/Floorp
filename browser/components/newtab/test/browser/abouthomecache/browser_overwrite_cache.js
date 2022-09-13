/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that if a pre-existing about:home cache exists, that it can
 * be overwritten with new information.
 */
add_task(async function test_overwrite_cache() {
  await withFullyLoadedAboutHome(async browser => {
    await simulateRestart(browser);
    const TEST_ID = "test_overwrite_cache_h1";

    // We need the CSP meta tag in about: pages, otherwise we hit assertions in
    // debug builds.
    await injectIntoCache(
      `
      <html>
        <head>
        <meta http-equiv="Content-Security-Policy" content="default-src 'none'; object-src 'none'; script-src resource: chrome:; connect-src https:; img-src https: data: blob:; style-src 'unsafe-inline';">
        </head>
        <body>
          <h1 id="${TEST_ID}">Something new</h1>
          <div id="root"></div>
        </body>
        <script src="about:home?jscache"></script>
      </html>`,
      "window.__FROM_STARTUP_CACHE__ = true;"
    );
    await simulateRestart(browser, { withAutoShutdownWrite: false });

    await SpecialPowers.spawn(browser, [TEST_ID], async testID => {
      let target = content.document.getElementById(testID);
      Assert.ok(target, "Found the target element");
    });
  });
});
