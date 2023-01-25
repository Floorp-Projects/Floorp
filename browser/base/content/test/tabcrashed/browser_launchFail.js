/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that if the content process fails to launch in the
 * foreground tab, that we show about:tabcrashed, but do not
 * attempt to wait for a crash dump for it (which will never come).
 */
add_task(async function test_launchfail_foreground() {
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  await BrowserTestUtils.withNewTab("http://example.com", async browser => {
    let tabcrashed = BrowserTestUtils.waitForEvent(
      browser,
      "AboutTabCrashedReady",
      false,
      null,
      true
    );
    await BrowserTestUtils.simulateProcessLaunchFail(browser);
    Assert.equal(
      0,
      TabCrashHandler.queuedCrashedBrowsers,
      "No crashed browsers should be queued."
    );
    await tabcrashed;
  });
});

/**
 * Tests that if the content process fails to launch in a background
 * tab, that upon choosing that tab, we show about:tabcrashed, but do
 * not attempt to wait for a crash dump for it (which will never come).
 */
add_task(async function test_launchfail_background() {
  let originalTab = gBrowser.selectedTab;
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  await BrowserTestUtils.withNewTab("http://example.com", async browser => {
    let tab = gBrowser.getTabForBrowser(browser);
    await BrowserTestUtils.switchTab(gBrowser, originalTab);

    let tabcrashed = BrowserTestUtils.waitForEvent(
      browser,
      "AboutTabCrashedReady",
      false,
      null,
      true
    );
    await BrowserTestUtils.simulateProcessLaunchFail(browser);
    Assert.equal(
      0,
      TabCrashHandler.queuedCrashedBrowsers,
      "No crashed browsers should be queued."
    );
    await BrowserTestUtils.switchTab(gBrowser, tab);
    await tabcrashed;
  });
});
