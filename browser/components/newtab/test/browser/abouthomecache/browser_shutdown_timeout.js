/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that if there's a substantial delay in getting the cache
 * streams from the privileged about content process for any reason
 * during shutdown, that we timeout and let the AsyncShutdown proceed,
 * rather than letting it block until AsyncShutdown causes a shutdown
 * hang crash.
 */
add_task(async function test_shutdown_timeout() {
  await withFullyLoadedAboutHome(async browser => {
    // First, make sure the cache is populated so that later on, after
    // the timeout, simulateRestart doesn't complain about not finding
    // a pre-existing cache. This complaining only happens if this test
    // is run in isolation.
    await clearCache();
    await simulateRestart(browser);

    // Next, manually shutdown the AboutHomeStartupCacheChild so that
    // it doesn't respond to requests to the cache streams.
    await SpecialPowers.spawn(browser, [], async () => {
      let { AboutHomeStartupCacheChild } = ChromeUtils.importESModule(
        "resource:///modules/AboutNewTabService.sys.mjs"
      );
      AboutHomeStartupCacheChild.uninit();
    });

    // Then, manually dirty the cache state so that we attempt to write
    // on shutdown.
    AboutHomeStartupCache.onPreloadedNewTabMessage();

    await simulateRestart(browser, { expectTimeout: true });

    Assert.ok(
      true,
      "We reached here, which means shutdown didn't block forever."
    );

    // Clear the cache so that we're not in a half-persisted state.
    await clearCache();
  });
});
