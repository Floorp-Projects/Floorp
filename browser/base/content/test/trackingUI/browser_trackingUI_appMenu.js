/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.import(
  "resource://testing-common/CustomizableUITestUtils.jsm",
  this
);

add_task(async function setup() {
  // Hide protections cards so as not to trigger more async messaging
  // when landing on the page.
  SpecialPowers.pushPrefEnv({
    set: [
      ["browser.contentblocking.report.monitor.enabled", false],
      ["browser.contentblocking.report.lockwise.enabled", false],
      ["browser.contentblocking.report.proxy.enabled", false],
    ],
  });
});

// Test that the "Privacy Protections" button in the app menu loads about:protections
add_task(async function testProtectionsButton() {
  let cuiTestUtils = new CustomizableUITestUtils(window);

  await BrowserTestUtils.withNewTab(gBrowser, async function(browser) {
    await cuiTestUtils.openMainMenu();

    let loaded = TestUtils.waitForCondition(
      () => gBrowser.currentURI.spec == "about:protections",
      "Should open about:protections"
    );
    document.getElementById("appMenu-protection-report-button").click();
    await loaded;

    // When the graph is built it means any messaging has finished,
    // we can close the tab.
    await ContentTask.spawn(browser, {}, async function() {
      await ContentTaskUtils.waitForCondition(() => {
        let bars = content.document.querySelectorAll(".graph-bar");
        return bars.length;
      }, "The graph has been built");
    });
  });
});
