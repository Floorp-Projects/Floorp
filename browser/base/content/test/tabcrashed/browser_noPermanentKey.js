"use strict";

const PAGE = "data:text/html,<html><body>A%20regular,%20everyday,%20normal%20page.";

add_task(async function setup() {
  await setupLocalCrashReportServer();
});

/**
 * Tests tab crash page when a browser that somehow doesn't have a permanentKey
 * crashes.
 */
add_task(async function test_without_dump() {
  return BrowserTestUtils.withNewTab({
    gBrowser,
    url: PAGE,
  }, async function(browser) {
    delete browser.permanentKey;

    await BrowserTestUtils.crashBrowser(browser);
    let crashReport = promiseCrashReport();

    await ContentTask.spawn(browser, null, async function() {
      let doc = content.document;
      Assert.ok(doc.documentElement.classList.contains("crashDumpAvailable"),
        "Should be offering to submit a crash report.");
      // With the permanentKey gone, restoring this tab is no longer
      // possible. We'll just close it instead.
      let closeTab = doc.getElementById("closeTab");
      closeTab.click();
    });

    await crashReport;
  });
});
