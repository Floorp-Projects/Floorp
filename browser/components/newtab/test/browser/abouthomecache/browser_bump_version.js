/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that if the "version" metadata on the cache entry doesn't match
 * the expectation that we ignore the cache and load the dynamic about:home
 * document.
 */
add_task(async function test_bump_version() {
  await withFullyLoadedAboutHome(async browser => {
    // First, ensure that a pre-existing cache exists.
    await simulateRestart(browser);

    let cacheEntry = await AboutHomeStartupCache.ensureCacheEntry();
    Assert.equal(
      cacheEntry.getMetaDataElement("version"),
      Services.appinfo.appBuildID,
      "Cache entry should be versioned on the build ID"
    );
    cacheEntry.setMetaDataElement("version", "somethingnew");
    // We don't need to shutdown write or ensure the cache wins the race,
    // since we expect the cache to be blown away because the version number
    // has been bumped.
    await simulateRestart(browser, {
      withAutoShutdownWrite: false,
      ensureCacheWinsRace: false,
    });
    await ensureDynamicAboutHome(
      browser,
      AboutHomeStartupCache.CACHE_RESULT_SCALARS.INVALIDATED
    );
  });
});
