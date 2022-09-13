/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the about:home startup cache is cleared if the app
 * locale changes.
 */
add_task(async function test_locale_change() {
  await withFullyLoadedAboutHome(async browser => {
    await simulateRestart(browser);
    await ensureCachedAboutHome(browser);

    Services.obs.notifyObservers(null, "intl:app-locales-changed");

    // We're testing that switching locales blows away the cache, so we
    // bypass the automatic writing of the cache on shutdown, and we
    // also don't need to wait for the cache to be available.
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
