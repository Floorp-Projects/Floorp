/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

/**
 * Test that if there's no cache written, that we load the dynamic
 * about:home document on startup.
 */
add_task(async function test_no_cache() {
  await BrowserTestUtils.withNewTab("about:home", async browser => {
    await clearCache();
    await simulateRestart(browser, false /* withAutoShutdownWrite */);
    await ensureDynamicAboutHome(
      browser,
      AboutHomeStartupCache.CACHE_RESULT_SCALARS.DOES_NOT_EXIST
    );
  });
});
