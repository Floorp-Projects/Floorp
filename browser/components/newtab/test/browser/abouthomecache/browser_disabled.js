/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This file tests scenarios where the cache is disabled due to user
 * configuration.
 */

registerCleanupFunction(async () => {
  // When the test completes, make sure we cleanup with a populated cache,
  // since this is the default starting state for these tests.
  await withFullyLoadedAboutHome(async browser => {
    await simulateRestart(browser);
  });
});

/**
 * Tests the case where the cache is disabled via the pref.
 */
add_task(async function test_cache_disabled() {
  await withFullyLoadedAboutHome(async browser => {
    await SpecialPowers.pushPrefEnv({
      set: [["browser.startup.homepage.abouthome_cache.enabled", false]],
    });

    await simulateRestart(browser);

    await ensureDynamicAboutHome(
      browser,
      AboutHomeStartupCache.CACHE_RESULT_SCALARS.DISABLED
    );

    await SpecialPowers.popPrefEnv();
  });
});

/**
 * Tests the case where the cache is disabled because the home page is
 * not set at about:home.
 */
add_task(async function test_cache_custom_homepage() {
  await withFullyLoadedAboutHome(async browser => {
    await HomePage.set("https://example.com");
    await simulateRestart(browser);

    await ensureDynamicAboutHome(
      browser,
      AboutHomeStartupCache.CACHE_RESULT_SCALARS.NOT_LOADING_ABOUTHOME
    );

    HomePage.reset();
  });
});

/**
 * Tests the case where the cache is disabled because the session is
 * configured to automatically be restored.
 */
add_task(async function test_cache_restore_session() {
  await withFullyLoadedAboutHome(async browser => {
    await SpecialPowers.pushPrefEnv({
      set: [["browser.startup.page", 3]],
    });

    await simulateRestart(browser);

    await ensureDynamicAboutHome(
      browser,
      AboutHomeStartupCache.CACHE_RESULT_SCALARS.NOT_LOADING_ABOUTHOME
    );

    await SpecialPowers.popPrefEnv();
  });
});

/**
 * Tests the case where the cache is disabled because about:newtab
 * preloading is disabled.
 */
add_task(async function test_cache_no_preloading() {
  await withFullyLoadedAboutHome(async browser => {
    await SpecialPowers.pushPrefEnv({
      set: [["browser.newtab.preload", false]],
    });

    await simulateRestart(browser);

    await ensureDynamicAboutHome(
      browser,
      AboutHomeStartupCache.CACHE_RESULT_SCALARS.PRELOADING_DISABLED
    );

    await SpecialPowers.popPrefEnv();
  });
});
