/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// eslint-disable-next-line @microsoft/sdl/no-insecure-url
const PAGE_1 = "http://example.com";
// eslint-disable-next-line @microsoft/sdl/no-insecure-url
const PAGE_2 = "http://example.org";
// eslint-disable-next-line @microsoft/sdl/no-insecure-url
const PAGE_3 = "http://example.net";

/**
 * Checks that a particular about:tabcrashed page has the attribute set to
 * use the "multiple about:tabcrashed" UI.
 *
 * @param browser (<xul:browser>)
 *   The browser to check.
 * @param expected (Boolean)
 *   True if we expect the "multiple" state to be set.
 * @returns Promise
 * @resolves undefined
 *   When the check has completed.
 */
async function assertShowingMultipleUI(browser, expected) {
  let showingMultiple = await SpecialPowers.spawn(browser, [], async () => {
    return (
      content.document.getElementById("main").getAttribute("multiple") == "true"
    );
  });
  Assert.equal(showingMultiple, expected, "Got the expected 'multiple' state.");
}

/**
 * Takes a Telemetry histogram snapshot and returns the sum of all counts.
 *
 * @param snapshot (Object)
 *        The Telemetry histogram snapshot to examine.
 * @return (int)
 *         The sum of all counts in the snapshot.
 */
function snapshotCount(snapshot) {
  return Object.values(snapshot.values).reduce((a, b) => a + b, 0);
}

/**
 * Switches to a tab, crashes it, and waits for about:tabcrashed
 * to load.
 *
 * @param tab (<xul:tab>)
 *   The tab to switch to and crash.
 * @returns Promise
 * @resolves undefined
 *   When about:tabcrashed is loaded.
 */
async function switchToAndCrashTab(tab) {
  let browser = tab.linkedBrowser;

  await BrowserTestUtils.switchTab(gBrowser, tab);
  let tabcrashed = BrowserTestUtils.waitForEvent(
    browser,
    "AboutTabCrashedReady",
    false,
    null,
    true
  );
  await BrowserTestUtils.crashFrame(browser);
  await tabcrashed;
}

/**
 * Tests that the appropriate pieces of UI in the about:tabcrashed pages
 * are updated to reflect how many other about:tabcrashed pages there
 * are.
 */
add_task(async function test_multiple_tabcrashed_pages() {
  let histogram = Services.telemetry.getHistogramById(
    "FX_CONTENT_CRASH_NOT_SUBMITTED"
  );
  histogram.clear();

  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE_1);
  let browser1 = tab1.linkedBrowser;

  let tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE_2);
  let browser2 = tab2.linkedBrowser;

  let tab3 = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE_3);
  let browser3 = tab3.linkedBrowser;

  await switchToAndCrashTab(tab1);
  Assert.ok(tab1.hasAttribute("crashed"), "tab1 has crashed");
  Assert.ok(!tab2.hasAttribute("crashed"), "tab2 has not crashed");
  Assert.ok(!tab3.hasAttribute("crashed"), "tab3 has not crashed");

  // Should not be showing UI for multiple tabs in tab1.
  await assertShowingMultipleUI(browser1, false);

  await switchToAndCrashTab(tab2);
  Assert.ok(tab1.hasAttribute("crashed"), "tab1 is still crashed");
  Assert.ok(tab2.hasAttribute("crashed"), "tab2 has crashed");
  Assert.ok(!tab3.hasAttribute("crashed"), "tab3 has not crashed");

  // tab1 and tab2 should now be showing UI for multiple tab crashes.
  await assertShowingMultipleUI(browser1, true);
  await assertShowingMultipleUI(browser2, true);

  await switchToAndCrashTab(tab3);
  Assert.ok(tab1.hasAttribute("crashed"), "tab1 is still crashed");
  Assert.ok(tab2.hasAttribute("crashed"), "tab2 is still crashed");
  Assert.ok(tab3.hasAttribute("crashed"), "tab3 has crashed");

  // tab1 and tab2 should now be showing UI for multiple tab crashes.
  await assertShowingMultipleUI(browser1, true);
  await assertShowingMultipleUI(browser2, true);
  await assertShowingMultipleUI(browser3, true);

  BrowserTestUtils.removeTab(tab1);
  await assertShowingMultipleUI(browser2, true);
  await assertShowingMultipleUI(browser3, true);

  BrowserTestUtils.removeTab(tab2);
  await assertShowingMultipleUI(browser3, false);

  BrowserTestUtils.removeTab(tab3);

  // We only record the FX_CONTENT_CRASH_NOT_SUBMITTED probe if there
  // was a single about:tabcrashed page at unload time, so we expect
  // only a single entry for the probe for when we removed the last
  // crashed tab.
  await BrowserTestUtils.waitForCondition(() => {
    return snapshotCount(histogram.snapshot()) == 1;
  }, `Collected value should become 1.`);

  histogram.clear();
});
