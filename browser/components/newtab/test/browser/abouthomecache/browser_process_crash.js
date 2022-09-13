/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that if the "privileged about content process" crashes, that it
 * drops its internal reference to the "privileged about content process"
 * process manager, and that a subsequent restart of that process type
 * results in a dynamic document load. Also tests that crashing of
 * any other content process type doesn't clear the process manager
 * reference.
 */
add_task(async function test_process_crash() {
  await withFullyLoadedAboutHome(async browser => {
    await simulateRestart(browser);
    let origProcManager = AboutHomeStartupCache._procManager;

    await BrowserTestUtils.crashFrame(browser);
    Assert.notEqual(
      origProcManager,
      AboutHomeStartupCache._procManager,
      "Should have dropped the reference to the crashed process"
    );
  });

  await withFullyLoadedAboutHome(async browser => {
    // The cache should still be considered "valid and used", since it was
    // used successfully before the crash.
    await ensureDynamicAboutHome(
      browser,
      AboutHomeStartupCache.CACHE_RESULT_SCALARS.VALID_AND_USED
    );

    // Now simulate a restart to attach the AboutHomeStartupCache to
    // the new privileged about content process.
    await simulateRestart(browser);
  });

  let latestProcManager = AboutHomeStartupCache._procManager;

  await BrowserTestUtils.withNewTab("http://example.com", async browser => {
    await BrowserTestUtils.crashFrame(browser);
    Assert.equal(
      latestProcManager,
      AboutHomeStartupCache._procManager,
      "Should still have the reference to the privileged about process"
    );
  });
});

/**
 * Tests that if the "privileged about content process" crashes while
 * a cache request is still underway, that the cache request resolves with
 * null input streams.
 */
add_task(async function test_process_crash_while_requesting_streams() {
  await withFullyLoadedAboutHome(async browser => {
    await simulateRestart(browser);
    let cacheStreamsPromise = AboutHomeStartupCache.requestCache();
    await BrowserTestUtils.crashFrame(browser);
    let cacheStreams = await cacheStreamsPromise;

    if (!cacheStreams.pageInputStream && !cacheStreams.scriptInputStream) {
      Assert.ok(true, "Page and script input streams are null.");
    } else {
      // It's possible (but probably rare) the parent was able to receive the
      // streams before the crash occurred. In that case, we'll make sure that
      // we can still read the streams.
      info("Received the streams. Checking that they're readable.");
      Assert.ok(
        cacheStreams.pageInputStream.available(),
        "Bytes available for page stream"
      );
      Assert.ok(
        cacheStreams.scriptInputStream.available(),
        "Bytes available for script stream"
      );
    }
  });
});
