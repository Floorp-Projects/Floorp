"use strict";

const PAGE =
  "data:text/html,<html><body>A%20regular,%20everyday,%20normal%20page.";

async function assertIsAtRestartRequiredPage(browser) {
  let doc = browser.contentDocument;

  // Since about:restartRequired will run in the parent process, we can safely
  // manipulate its DOM nodes directly
  let title = doc.getElementById("title");
  let description = doc.getElementById("errorLongContent");
  let restartButton = doc.getElementById("restart");

  Assert.ok(title, "Title element exists.");
  Assert.ok(description, "Description element exists.");
  Assert.ok(restartButton, "Restart button exists.");
}

/**
 * This function returns a Promise that resolves once the following
 * actions have taken place:
 *
 * 1) A new tab is opened up at PAGE
 * 2) The tab is crashed
 * 3) The about:restartrequired page is displayed
 *
 * @returns Promise
 */
function crashTabTestHelper() {
  return BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: PAGE,
    },
    async function (browser) {
      // Simulate buildID mismatch.
      TabCrashHandler.testBuildIDMismatch = true;

      let restartRequiredLoaded = BrowserTestUtils.waitForContentEvent(
        browser,
        "AboutRestartRequiredLoad",
        false,
        null,
        true
      );
      await BrowserTestUtils.crashFrame(browser, false);
      await restartRequiredLoaded;
      await assertIsAtRestartRequiredPage(browser);

      // Reset
      TabCrashHandler.testBuildIDMismatch = false;
    }
  );
}

/**
 * Tests that the about:restartrequired page appears when buildID mismatches
 * between parent and child processes are encountered.
 */
add_task(async function test_default() {
  await crashTabTestHelper();
});

/**
 * Tests that if the content process fails to launch in the
 * foreground tab, that we show the restart required page, but do not
 * attempt to wait for a crash dump for it (which will never come).
 */
add_task(async function test_restart_required_foreground() {
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  await BrowserTestUtils.withNewTab("http://example.com", async browser => {
    let loaded = BrowserTestUtils.browserLoaded(browser, false, null, true);
    await BrowserTestUtils.simulateProcessLaunchFail(
      browser,
      true /* restart required */
    );
    Assert.equal(
      0,
      TabCrashHandler.queuedCrashedBrowsers,
      "No crashed browsers should be queued."
    );
    await loaded;
    await assertIsAtRestartRequiredPage(browser);
  });
});

/**
 * Tests that if the content process fails to launch in a background
 * tab because a restart is required, that upon choosing that tab, we
 * show the restart required error page, but do not attempt to wait for
 * a crash dump for it (which will never come).
 */
add_task(async function test_launchfail_background() {
  let originalTab = gBrowser.selectedTab;
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  await BrowserTestUtils.withNewTab("http://example.com", async browser => {
    let tab = gBrowser.getTabForBrowser(browser);
    await BrowserTestUtils.switchTab(gBrowser, originalTab);
    await BrowserTestUtils.simulateProcessLaunchFail(
      browser,
      true /* restart required */
    );
    Assert.equal(
      0,
      TabCrashHandler.queuedCrashedBrowsers,
      "No crashed browsers should be queued."
    );
    let loaded = BrowserTestUtils.waitForContentEvent(
      browser,
      "AboutRestartRequiredLoad",
      false,
      null,
      true
    );
    await BrowserTestUtils.switchTab(gBrowser, tab);
    await loaded;

    await assertIsAtRestartRequiredPage(browser);
  });
});
