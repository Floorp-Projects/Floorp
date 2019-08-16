"use strict";

const PAGE =
  "data:text/html,<html><body>A%20regular,%20everyday,%20normal%20page.";

add_task(async function setup() {
  prepareNoDump();
});

/**
 * Tests tab crash page when a dump is not available.
 */
add_task(async function test_without_dump() {
  return BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: PAGE,
    },
    async function(browser) {
      let tab = gBrowser.getTabForBrowser(browser);
      await BrowserTestUtils.crashFrame(browser);

      let tabClosingPromise = BrowserTestUtils.waitForTabClosing(tab);

      await ContentTask.spawn(browser, null, async function() {
        let doc = content.document;
        Assert.ok(
          !doc.documentElement.classList.contains("crashDumpAvailable"),
          "doesn't have crash dump"
        );

        let options = doc.getElementById("options");
        Assert.ok(options, "has crash report options");
        Assert.ok(options.hidden, "crash report options are hidden");

        doc.getElementById("closeTab").click();
      });

      await tabClosingPromise;
    }
  );
});
