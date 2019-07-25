"use strict";

const PAGE =
  "data:text/html,<html><body>A%20regular,%20everyday,%20normal%20page.";

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
    async function(browser) {
      // Simulate buildID mismatch.
      TabCrashHandler.testBuildIDMismatch = true;

      await BrowserTestUtils.crashBrowser(browser, false);
      let doc = browser.contentDocument;

      // Since about:restartRequired will run in the parent process, we can safely
      // manipulate its DOM nodes directly
      let title = doc.getElementById("title");
      let description = doc.getElementById("errorLongContent");
      let restartButton = doc.getElementById("restart");

      ok(title, "Title element exists.");
      ok(description, "Description element exists.");
      ok(restartButton, "Restart button exists.");

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
