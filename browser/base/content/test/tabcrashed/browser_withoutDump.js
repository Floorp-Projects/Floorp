"use strict";

const PAGE = "data:text/html,<html><body>A%20regular,%20everyday,%20normal%20page.";

/**
 * Monkey patches TabCrashHandler.getDumpID to return null in order to test
 * about:tabcrashed when a dump is not available.
 * Set pref to ensure these tests are run with notification animations enabled
 */
add_task(async function setup() {
  // run these tests with animations enabled
  await SpecialPowers.pushPrefEnv({
    "set": [["toolkit.cosmeticAnimations.enabled", true]]
  });
  let originalGetDumpID = TabCrashHandler.getDumpID;
  TabCrashHandler.getDumpID = function(browser) { return null; };
  registerCleanupFunction(() => {
    TabCrashHandler.getDumpID = originalGetDumpID;
  });
});

/**
 * Tests tab crash page when a dump is not available.
 */
add_task(async function test_without_dump() {
  return BrowserTestUtils.withNewTab({
    gBrowser,
    url: PAGE,
  }, async function(browser) {
    let tab = gBrowser.getTabForBrowser(browser);
    await BrowserTestUtils.crashBrowser(browser);

    let tabRemovedPromise = BrowserTestUtils.tabRemoved(tab);

    await ContentTask.spawn(browser, null, async function() {
      let doc = content.document;
      Assert.ok(!doc.documentElement.classList.contains("crashDumpAvailable"),
         "doesn't have crash dump");

      let options = doc.getElementById("options");
      Assert.ok(options, "has crash report options");
      Assert.ok(options.hidden, "crash report options are hidden");

      doc.getElementById("closeTab").click();
    });

    await tabRemovedPromise;
  });
});
