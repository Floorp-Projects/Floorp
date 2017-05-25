"use strict";

/**
 * WHOA THERE: We should never be adding new things to EXPECTED_REFLOWS. This
 * is a whitelist that should slowly go away as we improve the performance of
 * the front-end. Instead of adding more reflows to the whitelist, you should
 * be modifying your code to avoid the reflow.
 *
 * See https://developer.mozilla.org/en-US/Firefox/Performance_best_practices_for_Firefox_fe_engineers
 * for tips on how to do that.
 */
const EXPECTED_REFLOWS = [
  /**
   * Nothing here! Please don't add anything new!
   */
];

/*
 * This test ensures that there are no unexpected
 * uninterruptible reflows when closing a tab that will
 * cause the existing tabs to grow bigger.
 */
add_task(async function() {
  await ensureNoPreloadedBrowser();

  // At the time of writing, there are no reflows on tab closing with
  // tab growth. Mochitest will fail if we have no assertions, so we
  // add one here to make sure nobody adds any new ones.
  Assert.equal(EXPECTED_REFLOWS.length, 0,
    "We shouldn't have added any new expected reflows.");

  // Compute the number of tabs we can put into the strip without
  // overflowing. If we remove one of the tabs, we know that the
  // remaining tabs will grow to fill the remaining space in the
  // tabstrip.
  const TAB_COUNT_FOR_GROWTH = computeMaxTabCount();
  await createTabs(TAB_COUNT_FOR_GROWTH);

  // Because the tab strip is a scrollable frame, we can't use the
  // default dirtying function from withReflowObserver and reliably
  // get reflows for the strip. Instead, we provide a node that's
  // already in the scrollable frame to dirty - in this case, the
  // original tab.
  let origTab = gBrowser.selectedTab;
  let lastTab = gBrowser.tabs[gBrowser.tabs.length - 1];
  await BrowserTestUtils.switchTab(gBrowser, lastTab);

  await withReflowObserver(async function() {
    let switchDone = BrowserTestUtils.waitForEvent(window, "TabSwitchDone");
    let tab = gBrowser.tabs[gBrowser.tabs.length - 1];
    gBrowser.removeTab(tab, { animate: true });
    await BrowserTestUtils.waitForEvent(tab, "transitionend",
      false, e => e.propertyName === "max-width");
    await switchDone;
  }, EXPECTED_REFLOWS, window, origTab);

  await removeAllButFirstTab();
});
