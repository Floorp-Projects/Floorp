/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the about:home cache gets written on shutdown, and read
 * from in the subsequent startup.
 */
add_task(async function test_basic_behaviour() {
  await withFullyLoadedAboutHome(async browser => {
    // First, clear the cache to test the base case.
    await clearCache();
    await simulateRestart(browser);
    await ensureCachedAboutHome(browser);

    // Next, test that a subsequent restart also shows the cached
    // about:home.
    await simulateRestart(browser);
    await ensureCachedAboutHome(browser);
  });
});
