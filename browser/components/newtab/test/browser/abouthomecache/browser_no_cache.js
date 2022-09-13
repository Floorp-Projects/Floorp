/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

/**
 * Test that if there's no cache written, that we load the dynamic
 * about:home document on startup.
 */
add_task(async function test_no_cache() {
  await withFullyLoadedAboutHome(async browser => {
    await clearCache();
    // We're testing the no-cache case, so we bypass the automatic writing
    // of the cache on shutdown, and we also don't need to wait for the
    // cache to be available.
    await simulateRestart(browser, {
      withAutoShutdownWrite: false,
      ensureCacheWinsRace: false,
    });
    await ensureDynamicAboutHome(
      browser,
      AboutHomeStartupCache.CACHE_RESULT_SCALARS.DOES_NOT_EXIST
    );
  });
});
