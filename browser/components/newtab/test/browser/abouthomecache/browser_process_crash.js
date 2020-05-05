/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that if the "privileged about content process" crashes, that it
 * drops its internal reference to the "privileged about content process"
 * process manager, and that a subsequent restart of that process type
 * results in a new cached document load. Also tests that crashing of
 * any other content process type doesn't clear the process manager
 * reference.
 */
add_task(async function test_bump_version() {
  await BrowserTestUtils.withNewTab("about:home", async browser => {
    await simulateRestart(browser);

    await BrowserTestUtils.crashFrame(browser);
    Assert.ok(
      !AboutHomeStartupCache._procManager,
      "Should have dropped the reference to the crashed process"
    );
  });

  await BrowserTestUtils.withNewTab("about:home", async browser => {
    await ensureCachedAboutHome(browser);
  });

  await BrowserTestUtils.withNewTab("http://example.com", async browser => {
    await BrowserTestUtils.crashFrame(browser);
    Assert.ok(
      AboutHomeStartupCache._procManager,
      "Should still have the reference to the privileged about process"
    );
  });
});
